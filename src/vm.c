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
    };
};

struct Closure {
    Chunk *chunk;
    Upval **upvals;
};

struct Frame {
    Frame *parent;

    Closure *closure;
    StackObject **registers;
    int pc;
};

typedef struct UpvalNode UpvalNode;

struct UpvalNode {
    Upval *upval;
    UpvalNode *next;
    UpvalNode *prev;
};

struct State {
    Frame *current;
    UpvalNode *head;
};

typedef enum {
    OBJECT_STRING,
    OBJECT_CLOSURE
} HeapObjectType;

struct HeapObject {
    HeapObjectType type;

    union {
        char *s;
        Closure *c;
    };
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
    };
};

double cast_to_double(StackObject *object) {
    if (object->type == OBJECT_INT) {
        return (double) object->i;
    }

    return object->d;
}

Upval *make_upval(State *state, int slot) {
    Upval *upval = malloc(sizeof *upval);
    UpvalNode *node = malloc(sizeof *node);

    if (!upval || !node) {
        fatal("Out of memory.");
    }

    upval->ref.frame = state->current;
    upval->ref.slot = slot;
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
        free(upval->o);
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
    StackObject **registers = malloc(numregs * sizeof **registers);

    if (!frame || !registers) {
        fatal("Out of memory.");
    }

    int i;
    for (i = 0; i < numregs; i++) {
        StackObject *object = malloc(sizeof *object);

        if (!object) {
            fatal("Out of memory.");
        }

        object->type = -1; // uninitialized
        registers[i] = object;
    }

    frame->pc = 0;
    frame->parent = parent;
    frame->closure = closure;
    frame->registers = registers;
    return frame;
}

State *make_state(Frame *root) {
    State *state = malloc(sizeof *state);

    if (!state) {
        fatal("Out of memory.");
    }

    state->current = root;
    state->head = NULL;
    return state;
}

HeapObject *make_string_ref(char *s) {
    HeapObject *obj = malloc(sizeof *obj);

    if (!obj) {
        fatal("Out of memory.");
    }

    obj->type = OBJECT_STRING;
    obj->s = s;

    return obj;
}

HeapObject *make_closure_ref(Closure *c) {
    HeapObject *obj = malloc(sizeof *obj);

    if (!obj) {
        fatal("Out of memory.");
    }

    obj->type = OBJECT_CLOSURE;
    obj->c = c;

    return obj;
}

void copy_object(StackObject *o1, StackObject *o2) {
    // TODO - share string instances as a heap object,
    // garbage collect at this point (instead of free)

    switch (o2->type) {
        case OBJECT_INT:
            o1->type = OBJECT_INT;
            o1->i = o2->i;
            break;

        case OBJECT_BOOL:
            o1->type = OBJECT_BOOL;
            o1->i = o2->i;
            break;

        case OBJECT_REAL:
            o1->type = OBJECT_REAL;
            o1->d = o2->d;
            break;

        case OBJECT_NULL:
            o1->type = OBJECT_NULL;
            break;

        case OBJECT_REFERENCE:
            o1->type = OBJECT_REFERENCE;
            o1->o = o2->o;
            break;
    }
}

void copy_constant(StackObject *o, Constant *c) {
    // TODO - share string instances as a heap object,
    // garbage collect at this point (instead of free)

    switch (c->type) {
        case CONST_INT:
            o->type = OBJECT_INT;
            o->i = c->i;
            break;

        case CONST_REAL:
            o->type = OBJECT_REAL;
            o->d = c->d;
            break;

        case CONST_BOOL:
            o->type = OBJECT_BOOL;
            o->i = c->i;
            break;

        case CONST_NULL:
            o->type = OBJECT_NULL;
            break;

        case CONST_STRING:
            // TODO - how to deal with strings created by the program?
            // Right now we have no concat operator, so there's no worry,
            // but how do we know when to free a string (user-supplied) or when
            // to leave it alone in the pool (interned)?

            o->type = OBJECT_REFERENCE;
            o->o = make_string_ref(c->s);
            break;
    }
}

void print_heap(HeapObject *o) {
    switch (o->type) {
        case OBJECT_STRING:
            printf("<STRING %s>\n", o->s);
            break;

        case OBJECT_CLOSURE:
            printf("<CLOSURE>\n");
            break;
    }
}

void print(StackObject *o) {
    switch (o->type) {
        case OBJECT_INT:
            printf("<INT %d>\n", o->i);
            break;

        case OBJECT_REAL:
            printf("<REAL %.2f>\n", o->d);
            break;

        case OBJECT_BOOL:
            printf("<%s>\n", o->i == 1 ? "true" : "false");
            break;

        case OBJECT_NULL:
            printf("<NULL>\n");
            break;

        case OBJECT_REFERENCE:
            print_heap(o->o);
    }
}

void execute_function(State *state) {
restart: {
    Frame *frame = state->current;
    Closure *closure = frame->closure;
    Chunk *chunk = closure->chunk;
    StackObject **registers = frame->registers;

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
                    copy_object(registers[a], registers[b]);
                } else {
                    copy_constant(registers[a], chunk->constants[b - 256]);
                }
            } break;

            case OP_GETUPVAR:
            {
                Upval *upval = closure->upvals[b];

                if (!upval->open) {
                    // upval is closed
                    copy_object(registers[a], upval->o);
                } else {
                    // still on stack
                    copy_object(registers[a], upval->ref.frame->registers[upval->ref.slot]);
                }
            } break;

            case OP_SETUPVAR:
            {
                Upval *upval = closure->upvals[b];

                if (!upval->open) {
                    // upval is closed
                    copy_object(upval->o, registers[a]);
                } else {
                    // still on stack
                    copy_object(upval->ref.frame->registers[upval->ref.slot], registers[a]);
                }
            } break;

            case OP_ADD:
                if (registers[b]->type != OBJECT_INT && registers[b]->type != OBJECT_REAL) {
                    fatal("Tried to add non-numbers.");
                }

                if (registers[c]->type != OBJECT_INT && registers[c]->type != OBJECT_REAL) {
                    fatal("Tried to add non-numbers.");
                }

                // TODO - do with constants as well
                if (registers[b]->type == OBJECT_INT && registers[c]->type == OBJECT_INT) {
                    registers[a]->type = OBJECT_INT;
                    registers[a]->i = registers[b]->i + registers[c]->i;
                } else {
                    registers[a]->type = OBJECT_REAL;
                    registers[a]->d = cast_to_double(registers[b]) + cast_to_double(registers[c]);
                }
                break;

            case OP_SUB:
                if (registers[b]->type != OBJECT_INT && registers[b]->type != OBJECT_REAL) {
                    fatal("Tried to sub non-numbers.");
                }

                if (registers[c]->type != OBJECT_INT && registers[c]->type != OBJECT_REAL) {
                    fatal("Tried to sub non-numbers.");
                }

                // TODO - do with constants as well
                if (registers[b]->type == OBJECT_INT && registers[c]->type == OBJECT_INT) {
                    registers[a]->type = OBJECT_INT;
                    registers[a]->i = registers[b]->i - registers[c]->i;
                } else {
                    registers[a]->type = OBJECT_REAL;
                    registers[a]->d = cast_to_double(registers[b]) - cast_to_double(registers[c]);
                }
                break;

            case OP_MUL:
                if (registers[b]->type != OBJECT_INT && registers[b]->type != OBJECT_REAL) {
                    fatal("Tried to mul non-numbers.");
                }

                if (registers[c]->type != OBJECT_INT && registers[c]->type != OBJECT_REAL) {
                    fatal("Tried to mul non-numbers.");
                }

                // TODO - do with constants as well
                if (registers[b]->type == OBJECT_INT && registers[c]->type == OBJECT_INT) {
                    registers[a]->type = OBJECT_INT;
                    registers[a]->i = registers[b]->i * registers[c]->i;
                } else {
                    registers[a]->type = OBJECT_REAL;
                    registers[a]->d = cast_to_double(registers[b]) * cast_to_double(registers[c]);
                }
                break;

            case OP_DIV:
                if (registers[b]->type != OBJECT_INT && registers[b]->type != OBJECT_REAL) {
                    fatal("Tried to div non-numbers.");
                }

                if (registers[c]->type != OBJECT_INT && registers[c]->type != OBJECT_REAL) {
                    fatal("Tried to div non-numbers.");
                }

                if (cast_to_double(registers[c]) == 0) {
                    fatal("Div by zero.");
                }

                // TODO - do with constants as well
                if (registers[b]->type == OBJECT_INT && registers[c]->type == OBJECT_INT) {
                    registers[a]->type = OBJECT_INT;
                    registers[a]->i = registers[b]->i / registers[c]->i;
                } else {
                    registers[a]->type = OBJECT_REAL;
                    registers[a]->d = cast_to_double(registers[b]) / cast_to_double(registers[c]);
                }
                break;

            case OP_NEG:
                // TODO - do for constants as well
                if (registers[a]->type == OBJECT_INT) {
                    registers[a]->i = -registers[a]->i;
                } else if (registers[a]->type == OBJECT_REAL) {
                    registers[a]->d = -registers[a]->d;
                } else {
                    fatal("Tried to negate non-numeric type.");
                }
                break;

            case OP_NOT:
                if (registers[a]->type != OBJECT_BOOL) {
                    fatal("Expected boolean type, not %d.", registers[a]->type);
                }

                registers[a]->i = registers[a]->i == 1 ? 0 : 1;
                break;

            case OP_EQ:
                if (registers[b]->type != registers[c]->type) {
                    // TODO - not true for numeric values [!!]
                    registers[a]->i = 0;
                } else {
                    switch (registers[b]->type) {
                        case OBJECT_INT:
                        case OBJECT_BOOL:
                            registers[a]->i = registers[b]->i == registers[c]->i;
                            break;

                        case OBJECT_NULL:
                            registers[a]->i = 1;
                            break;

                        case OBJECT_REAL:
                            registers[a]->d = registers[b]->d == registers[c]->d;
                            break;

                        case OBJECT_REFERENCE:
                            fatal("Comparison of reference types not yet supported.");
                    }
                }

                registers[a]->type = OBJECT_BOOL;
                break;

            case OP_LT:
                if (registers[b]->type != OBJECT_INT && registers[b]->type != OBJECT_REAL) {
                    fatal("Tried to compare non-numbers.");
                }

                if (registers[c]->type != OBJECT_INT && registers[c]->type != OBJECT_REAL) {
                    fatal("Tried to compare non-numbers.");
                }

                // TODO - do with constants as well
                if (registers[b]->type == OBJECT_INT && registers[c]->type == OBJECT_INT) {
                    registers[a]->type = OBJECT_BOOL;
                    registers[a]->i = registers[b]->i < registers[c]->i;
                } else {
                    registers[a]->type = OBJECT_BOOL;
                    registers[a]->d = cast_to_double(registers[b]) < cast_to_double(registers[c]);
                }
                break;

            case OP_LE:
                if (registers[b]->type != OBJECT_INT && registers[b]->type != OBJECT_REAL) {
                    fatal("Tried to compare non-numbers.");
                }

                if (registers[c]->type != OBJECT_INT && registers[c]->type != OBJECT_REAL) {
                    fatal("Tried to compare non-numbers.");
                }

                // TODO - do with constants as well
                if (registers[b]->type == OBJECT_INT && registers[c]->type == OBJECT_INT) {
                    registers[a]->type = OBJECT_BOOL;
                    registers[a]->i = registers[b]->i <= registers[c]->i;
                } else {
                    registers[a]->type = OBJECT_BOOL;
                    registers[a]->d = cast_to_double(registers[b]) <= cast_to_double(registers[c]);
                }
                break;

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

                registers[a]->type = OBJECT_REFERENCE;
                registers[a]->o = make_closure_ref(child);
            } break;

            case OP_CALL:
            {
                if (registers[b]->type != OBJECT_REFERENCE || registers[b]->o->type != OBJECT_CLOSURE) {
                    fatal("Tried to call non-closure.");
                }

                // TODO - safety issue (see compile.c for notes)

                Closure *child = registers[b]->o->c;
                Frame *subframe = make_frame(frame, child);

                int i;
                for (i = 0; i < child->chunk->numparams; i++) {
                    copy_object(subframe->registers[i + 1], registers[c + i]);
                }

                state->current = subframe;
                goto restart;
            } break;

            case OP_RETURN:
            {
                UpvalNode *head;
                for (head = state->head; head != NULL; ) {
                    Upval *u = head->upval;

                    if (u->ref.frame == frame) {
                        if (u->refcount == 0) {
                            free_upval(u);
                        } else {
                            u->open = 0;
                            u->o = registers[u->ref.slot];
                        }

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

                // TODO - free registers

                if (state->current->parent != NULL) {
                    Frame *p = state->current->parent;
                    StackObject *target = p->registers[GET_A(p->closure->chunk->instructions[p->pc++])];

                    if (b < 256) {
                        print(registers[b]);
                        copy_object(target, registers[b]);
                    } else {
                        copy_constant(target, chunk->constants[b - 256]);
                    }

                    state->current = p;
                    goto restart;
                } else {
                    return;
                }
            } break;

            case OP_JUMP:
                if (c) {
                    frame->pc -= b;
                } else {
                    frame->pc += b;
                }
                break;

            case OP_JUMP_TRUE:
                if (registers[a]->type != OBJECT_BOOL) {
                    fatal("Expected boolean type, not %d.", registers[a]->type);
                }

                if (registers[a]->i == 1) {
                    if (c) {
                        frame->pc -= b;
                    } else {
                        frame->pc += b;
                    }
                }
                break;

            case OP_JUMP_FALSE:
                if (registers[a]->type != OBJECT_BOOL) {
                    fatal("Expected boolean type, not %d.", registers[a]->type);
                }

                if (registers[a]->i == 0) {
                    if (c) {
                        frame->pc -= b;
                    } else {
                        frame->pc += b;
                    }
                }
                break;
        }

        frame->pc++;
    }

    fatal("VM left instruction-space.");
}
}

void execute(Chunk *chunk) {
    Closure *closure = make_closure(chunk);
    Frame *frame = make_frame(NULL, closure);
    State *state = make_state(frame);

    execute_function(state);
}
