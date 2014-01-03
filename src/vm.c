/*
 * Copyright (c) 2014, Eric Fritz
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>

#include "vm.h"
#include "chinnu.h"
#include "bytecode.h"

typedef struct Upval Upval;
typedef struct Closure Closure;
typedef struct Frame Frame;
typedef struct State State;
typedef struct HeapObject HeapObject;
typedef struct StackObject StackObject;

struct Upval {
    int refcount;
    int open;
    union {
        struct {
            int slot;
            Frame *frame;
        } ref;
        StackObject *o;
    } data;
};

struct Closure {
    Chunk *chunk;
    Upval **upvals;
};

struct Frame {
    Frame *parent;

    Closure *closure;
    StackObject *registers;
    int pc;
};

typedef struct UpvalNode UpvalNode;

struct UpvalNode {
    Upval *upval;
    UpvalNode *next;
    UpvalNode *prev;
};

typedef enum {
    OBJECT_STRING,
    OBJECT_CLOSURE
} HeapObjectType;

struct HeapObject {
    HeapObject *next;
    unsigned int marked;

    HeapObjectType type;

    union {
        char *s;
        Closure *c;
    } value;
};

typedef enum {
    OBJECT_INT,
    OBJECT_REAL,
    OBJECT_BOOL,
    OBJECT_NULL,
    OBJECT_REFERENCE
} StackObjectType;

struct StackObject {
    StackObjectType type;

    union {
        int i;
        double d;
        HeapObject *o;
    } value;
};

// TODO - rename

struct State {
    Frame *current;
    UpvalNode *head;

    HeapObject *first;
    int numobjects;
    int maxobjects;
};

#define IS_INT(reg) ((reg < 256) ? registers[reg].type == OBJECT_INT : chunk->constants[b - 256]->type == CONST_INT)
#define AS_INT(reg) ((reg < 256) ? registers[reg].value.i : chunk->constants[b - 256]->value.i)

#define IS_REAL(reg) ((reg < 256) ? registers[reg].type == OBJECT_REAL : chunk->constants[b - 256]->type == CONST_REAL)
#define AS_REAL(reg) ((reg < 256) ? registers[reg].value.d : chunk->constants[b - 256]->value.d)

/* forward */
void gc(State *state);

HeapObject *make_object(State *state) {
    if (state->numobjects >= state->maxobjects) {
        gc(state);
    }

    HeapObject *obj = malloc(sizeof *obj);

    if (!obj) {
        fatal("Out of memory.");
    }

    obj->marked = 0;

    obj->next = state->first;
    state->first = obj;
    state->numobjects++;

    return obj;
}

/* forward */
void free_upval(Upval *upval);

void free_obj(HeapObject *obj) {
    switch (obj->type) {
        case OBJECT_CLOSURE:
        {
            int i;
            for (i = 0; i < obj->value.c->chunk->numupvars; i++) {
                Upval *u = obj->value.c->upvals[i];

                if (--u->refcount == 0) {
                    free_upval(u);
                }
            }

            free(obj->value.c);
        } break;

        case OBJECT_STRING:
            // TODO - clear if not interned?
            break;
    }

    free(obj);
}

HeapObject *make_string_ref(State *state, char *s) {
    HeapObject *obj = make_object(state);

    obj->type = OBJECT_STRING;
    obj->value.s = s;

    return obj;
}

HeapObject *make_closure_ref(State *state, Closure *c) {
    HeapObject *obj = make_object(state);

    obj->type = OBJECT_CLOSURE;
    obj->value.c = c;

    return obj;
}

Upval *make_upval(State *state, int slot) {
    Upval *upval = malloc(sizeof *upval);
    UpvalNode *node = malloc(sizeof *node);

    if (!upval || !node) {
        fatal("Out of memory.");
    }

    upval->data.ref.frame = state->current;
    upval->data.ref.slot = slot;
    upval->refcount = 1;
    upval->open = 1;

    node->upval = upval;

    if (state->head) {
        state->head->prev = node;
    }

    node->prev = NULL;
    node->next = state->head;
    state->head = node;

    return upval;
}

void free_upval(Upval *upval) {
    if (upval->open == 0) {
        free(upval->data.o);
    }

    free(upval);
}

Closure *make_closure(Chunk *chunk) {
    Closure *closure = malloc(sizeof *closure);
    Upval **upvals = malloc(chunk->numupvars * sizeof **upvals);

    if (!closure || !upvals) {
        fatal("Out of memory.");
    }

    int i;
    for (i = 0; i < chunk->numupvars; i++) {
        upvals[i] = NULL;
    }

    closure->chunk = chunk;
    closure->upvals = upvals;
    return closure;
}

Frame *make_frame(Frame *parent, Closure *closure) {
    // move this to code gen, not responsibility of the vm [?]
    int numregs = closure->chunk->numlocals + closure->chunk->numtemps + 1;

    Frame *frame = malloc(sizeof *frame);
    StackObject *registers = malloc(numregs * sizeof *registers);

    if (!frame || !registers) {
        fatal("Out of memory.");
    }

    int i;
    for (i = 0; i < numregs; i++) {
        registers[i].type = -1; // uninitialized
    }

    frame->pc = 0;
    frame->parent = parent;
    frame->closure = closure;
    frame->registers = registers;
    return frame;
}

void free_frame(Frame *frame) {
    free(frame->registers);
    free(frame);
}

State *make_state(Frame *root, int maxobjects) {
    State *state = malloc(sizeof *state);

    if (!state) {
        fatal("Out of memory.");
    }

    state->current = root;
    state->head = NULL;
    state->first = NULL;
    state->numobjects = 0;
    state->maxobjects = maxobjects;

    return state;
}

void mark(HeapObject *obj) {
    if (obj->marked) {
        return;
    }

    obj->marked = 1;

    switch (obj->type) {
        case OBJECT_CLOSURE:
        {
            int i;
            for (i = 0; i < obj->value.c->chunk->numupvars; i++) {
                Upval *u = obj->value.c->upvals[i];

                if (!u->open) {
                    if (u->data.o->type == OBJECT_REFERENCE) {
                        mark(u->data.o->value.o);
                    }
                }
            }
        } break;

        default:
            break;
    }
}

void mark_all(State *state) {
    Frame *frame = state->current;

    while (frame) {
        int numregs = frame->closure->chunk->numlocals + frame->closure->chunk->numtemps + 1;

        int i;
        for (i = 0; i < numregs; i++) {
            if (frame->registers[i].type == OBJECT_REFERENCE) {
                mark(frame->registers[i].value.o);
            }
        }

        frame = frame->parent;
    }
}

void sweep(State *state) {
    HeapObject **obj = &state->first;

    while (*obj) {
        if (!(*obj)->marked) {
            HeapObject *temp = *obj;
            *obj = temp->next;
            free_obj(temp);
            state->numobjects--;
        } else {
            (*obj)->marked = 0;
            obj = &(*obj)->next;
        }
    }
}

void gc(State *state) {
    mark_all(state);
    sweep(state);
}

void copy_object(StackObject *o1, StackObject *o2) {
    // TODO - share string instances as a heap object,
    // garbage collect at this point (instead of free)

    switch (o2->type) {
        case OBJECT_INT:
            o1->type = OBJECT_INT;
            o1->value.i = o2->value.i;
            break;

        case OBJECT_BOOL:
            o1->type = OBJECT_BOOL;
            o1->value.i = o2->value.i;
            break;

        case OBJECT_REAL:
            o1->type = OBJECT_REAL;
            o1->value.d = o2->value.d;
            break;

        case OBJECT_NULL:
            o1->type = OBJECT_NULL;
            break;

        case OBJECT_REFERENCE:
            o1->type = OBJECT_REFERENCE;
            o1->value.o = o2->value.o;
            break;
    }
}

void copy_constant(State *state, StackObject *o, Constant *c) {
    // TODO - share string instances as a heap object,
    // garbage collect at this point (instead of free)

    switch (c->type) {
        case CONST_INT:
            o->type = OBJECT_INT;
            o->value.i = c->value.i;
            break;

        case CONST_REAL:
            o->type = OBJECT_REAL;
            o->value.d = c->value.d;
            break;

        case CONST_BOOL:
            o->type = OBJECT_BOOL;
            o->value.i = c->value.i;
            break;

        case CONST_NULL:
            o->type = OBJECT_NULL;
            break;

        case CONST_STRING:
            // TODO - how to deal with strings created by the program?
            // Right now we have no concat operator, so there's no worry,
            // but how do we know when to free a string (user-supplied) or when
            // to leave it alone in the pool (interned)?

            o->value.o = make_string_ref(state, c->value.s);
            o->type = OBJECT_REFERENCE; // put this after
            break;
    }
}

void print_heap(HeapObject *o) {
    switch (o->type) {
        case OBJECT_STRING:
            printf("<STRING %s>\n", o->value.s);
            break;

        case OBJECT_CLOSURE:
            printf("<CLOSURE>\n");
            break;
    }
}

void print(StackObject *o) {
    switch (o->type) {
        case OBJECT_INT:
            printf("<INT %d>\n", o->value.i);
            break;

        case OBJECT_REAL:
            printf("<REAL %.2f>\n", o->value.d);
            break;

        case OBJECT_BOOL:
            printf("<%s>\n", o->value.i == 1 ? "true" : "false");
            break;

        case OBJECT_NULL:
            printf("<NULL>\n");
            break;

        case OBJECT_REFERENCE:
            print_heap(o->value.o);
            break;
    }
}

void execute_function(State *state) {
restart: {
    Frame *frame = state->current;
    Closure *closure = frame->closure;
    Chunk *chunk = closure->chunk;
    StackObject *registers = frame->registers;

    while (frame->pc < chunk->numinstructions) {
        int instruction = chunk->instructions[frame->pc];

        int o = GET_O(instruction);
        int a = GET_A(instruction);
        int b = GET_B(instruction);
        int c = GET_C(instruction);

        switch (o) {
            case OP_MOVE:
            {
                if (b < 256) {
                    copy_object(&registers[a], &registers[b]);
                } else {
                    copy_constant(state, &registers[a], chunk->constants[b - 256]);
                }
            } break;

            case OP_GETUPVAR:
            {
                Upval *upval = closure->upvals[b];

                if (!upval->open) {
                    // upval is closed
                    copy_object(&registers[a], upval->data.o);
                } else {
                    // still on stack
                    copy_object(&registers[a], &upval->data.ref.frame->registers[upval->data.ref.slot]);
                }
            } break;

            case OP_SETUPVAR:
            {
                Upval *upval = closure->upvals[b];

                if (!upval->open) {
                    // upval is closed
                    copy_object(upval->data.o, &registers[a]);
                } else {
                    // still on stack
                    copy_object(&upval->data.ref.frame->registers[upval->data.ref.slot], &registers[a]);
                }
            } break;

            case OP_ADD:
            {
                if (!(IS_INT(b) || IS_REAL(b)) || !(IS_INT(c) || IS_REAL(c))) {
                    fatal("Tried to add non-numbers.");
                }

                if (IS_INT(b) && IS_INT(c)) {
                    double arg1 = AS_INT(b);
                    double arg2 = AS_INT(c);

                    registers[a].type = OBJECT_INT;
                    registers[a].value.i = arg1 + arg2;
                } else {
                    double arg1 = IS_INT(b) ? (double) AS_INT(b) : AS_REAL(b);
                    double arg2 = IS_INT(c) ? (double) AS_INT(c) : AS_REAL(c);

                    registers[a].type = OBJECT_REAL;
                    registers[a].value.d = arg1 + arg2;
                }
            } break;

            case OP_SUB:
            {
                if (!(IS_INT(b) || IS_REAL(b)) || !(IS_INT(c) || IS_REAL(c))) {
                    fatal("Tried to sub non-numbers.");
                }

                if (IS_INT(b) && IS_INT(c)) {
                    double arg1 = AS_INT(b);
                    double arg2 = AS_INT(c);

                    registers[a].type = OBJECT_INT;
                    registers[a].value.i = arg1 - arg2;
                } else {
                    double arg1 = IS_INT(b) ? (double) AS_INT(b) : AS_REAL(b);
                    double arg2 = IS_INT(c) ? (double) AS_INT(c) : AS_REAL(c);

                    registers[a].type = OBJECT_REAL;
                    registers[a].value.d = arg1 - arg2;
                }
            } break;

            case OP_MUL:
            {
                if (!(IS_INT(b) || IS_REAL(b)) || !(IS_INT(c) || IS_REAL(c))) {
                    fatal("Tried to mul non-numbers.");
                }

                if (IS_INT(b) && IS_INT(c)) {
                    double arg1 = AS_INT(b);
                    double arg2 = AS_INT(c);

                    registers[a].type = OBJECT_INT;
                    registers[a].value.i = arg1 * arg2;
                } else {
                    double arg1 = IS_INT(b) ? (double) AS_INT(b) : AS_REAL(b);
                    double arg2 = IS_INT(c) ? (double) AS_INT(c) : AS_REAL(c);

                    registers[a].type = OBJECT_REAL;
                    registers[a].value.d = arg1 * arg2;
                }
            } break;

            case OP_DIV:
            {
                if (!(IS_INT(b) || IS_REAL(b)) || !(IS_INT(c) || IS_REAL(c))) {
                    fatal("Tried to div non-numbers.");
                }

                if ((IS_INT(c) && AS_INT(c) == 0) || (IS_REAL(c) && AS_REAL(c) == 0)) {
                    fatal("Div by 0.");
                }

                if (IS_INT(b) && IS_INT(c)) {
                    double arg1 = AS_INT(b);
                    double arg2 = AS_INT(c);

                    registers[a].type = OBJECT_INT;
                    registers[a].value.i = arg1 / arg2;
                } else {
                    double arg1 = IS_INT(b) ? (double) AS_INT(b) : AS_REAL(b);
                    double arg2 = IS_INT(c) ? (double) AS_INT(c) : AS_REAL(c);

                    registers[a].type = OBJECT_REAL;
                    registers[a].value.d = arg1 / arg2;
                }
            } break;

            case OP_NEG:
            {
                if (IS_INT(b)) {
                    registers[a].type = OBJECT_INT;
                    registers[a].value.i = -AS_INT(b);
                } else if (IS_REAL(b)) {
                    registers[a].type = OBJECT_INT;
                    registers[a].value.i = -AS_REAL(b);
                } else {
                    fatal("Tried to negate non-numeric type.");
                }
            } break;

            case OP_NOT:
            {
                if (registers[a].type != OBJECT_BOOL) {
                    fatal("Expected boolean type, not %d.", registers[a].type);
                }

                registers[a].value.i = registers[a].value.i == 1 ? 0 : 1;
            } break;

            case OP_EQ:
            {
                if ((IS_INT(b) || IS_REAL(b)) && (IS_INT(c) || IS_REAL(c))) {
                    double arg1 = IS_INT(b) ? (double) AS_INT(b) : AS_REAL(b);
                    double arg2 = IS_INT(c) ? (double) AS_INT(c) : AS_REAL(c);

                    registers[a].type = OBJECT_BOOL;
                    registers[a].value.i = arg1 == arg2;
                } else {
                    fatal("Comparison of reference types not yet supported.");
                }
            } break;

            case OP_LT:
            {
                if (!(IS_INT(b) || IS_REAL(b)) || !(IS_INT(c) || IS_REAL(c))) {
                    fatal("Tried to compare non-numbers.");
                }

                double arg1 = IS_INT(b) ? (double) AS_INT(b) : AS_REAL(b);
                double arg2 = IS_INT(c) ? (double) AS_INT(c) : AS_REAL(c);

                registers[a].type = OBJECT_BOOL;
                registers[a].value.i = arg1 < arg2;
            } break;

            case OP_LE:
            {
                if (!(IS_INT(b) || IS_REAL(b)) || !(IS_INT(c) || IS_REAL(c))) {
                    fatal("Tried to compare non-numbers.");
                }

                double arg1 = IS_INT(b) ? (double) AS_INT(b) : AS_REAL(b);
                double arg2 = IS_INT(c) ? (double) AS_INT(c) : AS_REAL(c);

                registers[a].type = OBJECT_BOOL;
                registers[a].value.i = arg1 <= arg2;
            } break;

            case OP_CLOSURE:
            {
                Closure *child = make_closure(chunk->children[b]);

                int i;
                for (i = 0; i < chunk->children[b]->numupvars; i++) {
                    int inst = chunk->instructions[++frame->pc];

                    int oc = GET_O(inst);
                    int ac = GET_A(inst);
                    int bc = GET_B(inst);
                    int cc = GET_C(inst);

                    if (oc == OP_MOVE) {
                        // first upval for this variable
                        child->upvals[ac] = make_upval(state, bc);
                    } else {
                        // share upval
                        child->upvals[ac] = closure->upvals[bc];
                        child->upvals[ac]->refcount++;
                    }
                }

                registers[a].value.o = make_closure_ref(state, child);
                registers[a].type = OBJECT_REFERENCE; // put this after
            } break;

            case OP_CALL:
            {
                if (registers[b].type != OBJECT_REFERENCE || registers[b].value.o->type != OBJECT_CLOSURE) {
                    fatal("Tried to call non-closure.");
                }

                // TODO - safety issue (see compile.c for notes)

                Closure *child = registers[b].value.o->value.c;
                Frame *subframe = make_frame(frame, child);

                int i;
                for (i = 0; i < child->chunk->numparams; i++) {
                    copy_object(&subframe->registers[i + 1], &registers[c + i]);
                }

                state->current = subframe;
                goto restart;
            } break;

            case OP_RETURN:
            {
                UpvalNode *head;
                for (head = state->head; head != NULL; ) {
                    Upval *u = head->upval;

                    if (u->data.ref.frame == frame) {
                        StackObject *o = malloc(sizeof *o);

                        if (!o) {
                            fatal("Out of memory.");
                        }

                        u->open = 0;
                        copy_object(o, &registers[u->data.ref.slot]);
                        u->data.o = o;

                        if (state->head == head) {
                            state->head = head->next;
                        } else {
                            head->next->prev = head->prev;
                            head->prev->next = head->next;
                        }

                        UpvalNode *temp = head;
                        head = head->next;
                        free(temp);
                    } else {
                        head = head->next;
                    }
                }

                if (state->current->parent != NULL) {
                    Frame *p = state->current->parent;
                    StackObject *target = &p->registers[GET_A(p->closure->chunk->instructions[p->pc++])];

                    if (b < 256) {
                        print(&registers[b]);
                        copy_object(target, &registers[b]);
                    } else {
                        copy_constant(state, target, chunk->constants[b - 256]);
                    }

                    free_frame(frame);

                    state->current = p;
                    goto restart;
                } else {
                    print(&registers[b]);
                    free_frame(frame);
                    state->current = NULL;
                    return;
                }
            } break;

            case OP_JUMP:
            {
                if (c) {
                    frame->pc -= b;
                } else {
                    frame->pc += b;
                }
            } break;

            case OP_JUMP_TRUE:
            {
                if (registers[a].type != OBJECT_BOOL) {
                    fatal("Expected boolean type, not %d.", registers[a].type);
                }

                if (registers[a].value.i == 1) {
                    if (c) {
                        frame->pc -= b;
                    } else {
                        frame->pc += b;
                    }
                }
            } break;

            case OP_JUMP_FALSE:
            {
                if (registers[a].type != OBJECT_BOOL) {
                    fatal("Expected boolean type, not %d.", registers[a].type);
                }

                if (registers[a].value.i == 0) {
                    if (c) {
                        frame->pc -= b;
                    } else {
                        frame->pc += b;
                    }
                }
            } break;
        }

        frame->pc++;
    }

    fatal("VM left instruction-space.");
}
}

void execute(Chunk *chunk) {
    Closure *closure = make_closure(chunk);
    Frame *frame = make_frame(NULL, closure);
    State *state = make_state(frame, 0);

    execute_function(state);

    gc(state);
    free(state);

    // TODO - free last closure [?]
    // Other closrues should be already taken care of
    // by the last gc sweep, but this one existed outside
    // of the heap.
}
