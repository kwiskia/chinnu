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

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "vm.h"
#include "chinnu.h"
#include "bytecode.h"

typedef struct Upval Upval;
typedef struct Closure Closure;
typedef struct Frame Frame;
typedef struct VM VM;
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

struct VM {
    Frame *current;
    UpvalNode *open;

    HeapObject *heap;
    int numobjects;
    int maxobjects;
};

#define IS_INT(reg) ((reg < 256) ? registers[reg].type == OBJECT_INT : chunk->constants[b - 256]->type == CONST_INT)
#define AS_INT(reg) ((reg < 256) ? registers[reg].value.i : chunk->constants[b - 256]->value.i)

#define IS_REAL(reg) ((reg < 256) ? registers[reg].type == OBJECT_REAL : chunk->constants[b - 256]->type == CONST_REAL)
#define AS_REAL(reg) ((reg < 256) ? registers[reg].value.d : chunk->constants[b - 256]->value.d)

#define IS_STR(reg) ((reg < 256)                                                                 \
    ? (registers[reg].type == OBJECT_REFERENCE && registers[reg].value.o->type == OBJECT_STRING) \
    : (chunk->constants[reg - 256]->type == CONST_STRING))

#define TO_STR(reg) ((reg < 256) ? obj_to_str(&registers[reg]) : const_to_str(chunk->constants[reg - 256]))

char *obj_to_str(StackObject *o) {
    switch (o->type) {
        case OBJECT_INT:
        {
            char *str = malloc(15 * sizeof *str);
            sprintf(str, "%d", o->value.i);
            return str;
        }

        case OBJECT_REAL:
        {
            char *str = malloc(15 * sizeof *str);
            sprintf(str, "%.2f", o->value.d);
            return str;
        }

        case OBJECT_BOOL:
            return o->value.i == 1 ? strdup("true") : strdup("false");

        case OBJECT_NULL:
            return strdup("<null>");

        case OBJECT_REFERENCE:
            switch (o->value.o->type) {
                case OBJECT_STRING:
                    return strdup(o->value.o->value.s);

                case OBJECT_CLOSURE:
                    return "<closure>";
            }
    }
}

char *const_to_str(Constant *c) {
    switch (c->type) {
        case CONST_INT:
        {
            char *str = malloc(15 * sizeof *str);
            sprintf(str, "%d", c->value.i);
            return str;
        }

        case CONST_REAL:
        {
            char *str = malloc(15 * sizeof *str);
            sprintf(str, "%.2f", c->value.d);
            return str;
        }

        case CONST_NULL:
            return strdup("<null>");

        case CONST_BOOL:
            return c->value.i == 1 ? strdup("true") : strdup("false");

        case CONST_STRING:
            return strdup(c->value.s);
    }
}

/* forward */
void gc(VM *vm);

HeapObject *make_object(VM *vm) {
    if (vm->numobjects >= vm->maxobjects) {
        gc(vm);
    }

    HeapObject *obj = malloc(sizeof *obj);

    if (!obj) {
        fatal("Out of memory.");
    }

    obj->marked = 0;

    obj->next = vm->heap;
    vm->heap = obj;
    vm->numobjects++;

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
            free(obj->value.s);
            break;
    }

    free(obj);
}

HeapObject *make_string_ref(VM *vm, char *s) {
    HeapObject *obj = make_object(vm);

    obj->type = OBJECT_STRING;
    obj->value.s = s;

    return obj;
}

HeapObject *make_closure_ref(VM *vm, Closure *c) {
    HeapObject *obj = make_object(vm);

    obj->type = OBJECT_CLOSURE;
    obj->value.c = c;

    return obj;
}

Upval *make_upval(VM *vm, int slot) {
    Upval *upval = malloc(sizeof *upval);
    UpvalNode *node = malloc(sizeof *node);

    if (!upval || !node) {
        fatal("Out of memory.");
    }

    upval->data.ref.frame = vm->current;
    upval->data.ref.slot = slot;
    upval->refcount = 1;
    upval->open = 1;

    node->upval = upval;

    if (vm->open) {
        vm->open->prev = node;
    }

    node->prev = NULL;
    node->next = vm->open;
    vm->open = node;

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

VM *make_vm(Frame *root, int maxobjects) {
    VM *vm = malloc(sizeof *vm);

    if (!vm) {
        fatal("Out of memory.");
    }

    vm->current = root;
    vm->open = NULL;
    vm->heap = NULL;
    vm->numobjects = 0;
    vm->maxobjects = maxobjects;

    return vm;
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

void mark_all(VM *vm) {
    Frame *frame = vm->current;

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

void sweep(VM *vm) {
    HeapObject **obj = &vm->heap;

    while (*obj) {
        if (!(*obj)->marked) {
            HeapObject *temp = *obj;
            *obj = temp->next;
            free_obj(temp);
            vm->numobjects--;
        } else {
            (*obj)->marked = 0;
            obj = &(*obj)->next;
        }
    }
}

void gc(VM *vm) {
    mark_all(vm);
    sweep(vm);
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

void copy_constant(VM *vm, StackObject *o, Constant *c) {
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

            o->value.o = make_string_ref(vm, strdup(c->value.s));
            o->type = OBJECT_REFERENCE; // put this after
            break;
    }
}

void execute_function(VM *vm) {
restart: {
    Frame *frame = vm->current;
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
                    copy_constant(vm, &registers[a], chunk->constants[b - 256]);
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
                // TODO - make string coercion better
                // TODO - make string type with special operators

                if (!IS_STR(b) || IS_STR(c)) {
                    char *arg1 = TO_STR(b);
                    char *arg2 = TO_STR(c);

                    int n = strlen(arg1) + strlen(arg2);
                    char *arg3 = malloc(n + sizeof *arg3);

                    strcpy(arg3, arg1);
                    strcat(arg3, arg2);

                    registers[a].value.o = make_string_ref(vm, arg3);
                    registers[a].type = OBJECT_REFERENCE; // put this after

                    free(arg1);
                    free(arg2);
                } else {
                    if (!(IS_INT(b) || IS_REAL(b)) || !(IS_INT(c) || IS_REAL(c))) {
                        fatal("Cannot add types.");
                    }

                    if (IS_INT(b) && IS_INT(c)) {
                        int arg1 = AS_INT(b);
                        int arg2 = AS_INT(c);

                        registers[a].type = OBJECT_INT;
                        registers[a].value.i = arg1 + arg2;
                    } else {
                        double arg1 = IS_INT(b) ? (double) AS_INT(b) : AS_REAL(b);
                        double arg2 = IS_INT(c) ? (double) AS_INT(c) : AS_REAL(c);

                        registers[a].type = OBJECT_REAL;
                        registers[a].value.d = arg1 + arg2;
                    }
                }
            } break;

            case OP_SUB:
            {
                if (!(IS_INT(b) || IS_REAL(b)) || !(IS_INT(c) || IS_REAL(c))) {
                    fatal("Tried to sub non-numbers.");
                }

                if (IS_INT(b) && IS_INT(c)) {
                    int arg1 = AS_INT(b);
                    int arg2 = AS_INT(c);

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
                    int arg1 = AS_INT(b);
                    int arg2 = AS_INT(c);

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
                    int arg1 = AS_INT(b);
                    int arg2 = AS_INT(c);

                    registers[a].type = OBJECT_INT;
                    registers[a].value.i = arg1 / arg2;
                } else {
                    double arg1 = IS_INT(b) ? (double) AS_INT(b) : AS_REAL(b);
                    double arg2 = IS_INT(c) ? (double) AS_INT(c) : AS_REAL(c);

                    registers[a].type = OBJECT_REAL;
                    registers[a].value.d = arg1 / arg2;
                }
            } break;

            case OP_MOD:
            {
                if (!(IS_INT(b) || IS_REAL(b)) || !(IS_INT(c) || IS_REAL(c))) {
                    fatal("Tried to div non-numbers.");
                }

                if ((IS_INT(c) && AS_INT(c) == 0) || (IS_REAL(c) && AS_REAL(c) == 0)) {
                    fatal("Mod by 0.");
                }

                if (IS_INT(b) && IS_INT(c)) {
                    int arg1 = AS_INT(b);
                    int arg2 = AS_INT(c);

                    registers[a].type = OBJECT_INT;
                    registers[a].value.i = arg1 % arg2;
                } else {
                    double arg1 = IS_INT(b) ? (double) AS_INT(b) : AS_REAL(b);
                    double arg2 = IS_INT(c) ? (double) AS_INT(c) : AS_REAL(c);

                    registers[a].type = OBJECT_REAL;
                    registers[a].value.i = fmod(arg1, arg2);
                }
            } break;

            case OP_POW:
            {
                if (!(IS_INT(b) || IS_REAL(b)) || !(IS_INT(c) || IS_REAL(c))) {
                    fatal("Tried to div non-numbers.");
                }

                if (IS_INT(b) && IS_INT(c)) {
                    int arg1 = AS_INT(b);
                    int arg2 = AS_INT(c);

                    registers[a].type = OBJECT_INT;
                    registers[a].value.i = (int) pow(arg1, arg2);
                } else {
                    double arg1 = IS_INT(b) ? (double) AS_INT(b) : AS_REAL(b);
                    double arg2 = IS_INT(c) ? (double) AS_INT(c) : AS_REAL(c);

                    registers[a].type = OBJECT_REAL;
                    registers[a].value.d = pow(arg1, arg2);
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
                        child->upvals[ac] = make_upval(vm, bc);
                    } else {
                        // share upval
                        child->upvals[ac] = closure->upvals[bc];
                        child->upvals[ac]->refcount++;
                    }
                }

                registers[a].value.o = make_closure_ref(vm, child);
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

                vm->current = subframe;
                goto restart;
            } break;

            case OP_RETURN:
            {
                UpvalNode *head;
                for (head = vm->open; head != NULL; ) {
                    Upval *u = head->upval;

                    if (u->data.ref.frame == frame) {
                        StackObject *o = malloc(sizeof *o);

                        if (!o) {
                            fatal("Out of memory.");
                        }

                        u->open = 0;
                        copy_object(o, &registers[u->data.ref.slot]);
                        u->data.o = o;

                        if (vm->open == head) {
                            vm->open = head->next;
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

                if (vm->current->parent != NULL) {
                    Frame *p = vm->current->parent;
                    StackObject *target = &p->registers[GET_A(p->closure->chunk->instructions[p->pc++])];

                    if (b < 256) {
                    // debug
                        char *d = obj_to_str(&registers[b]);
                        printf("Return value: %s\n", d);
                        free(d);

                        copy_object(target, &registers[b]);
                    } else {
                        copy_constant(vm, target, chunk->constants[b - 256]);
                    }

                    free_frame(frame);

                    vm->current = p;
                    goto restart;
                } else {
                    // debug
                    char *d = obj_to_str(&registers[b]);
                    printf("Return value: %s\n", d);
                    free(d);

                    free_frame(frame);
                    vm->current = NULL;
                    return;
                }
            } break;

            case OP_JUMP:
                frame->pc += c ? -b : b;
                break;

            case OP_JUMP_TRUE:
            {
                if (registers[a].type != OBJECT_BOOL) {
                    fatal("Expected boolean type, not %d.", registers[a].type);
                }

                if (registers[a].value.i == 1) {
                    frame->pc += c ? -b : b;
                }
            } break;

            case OP_JUMP_FALSE:
            {
                if (registers[a].type != OBJECT_BOOL) {
                    fatal("Expected boolean type, not %d.", registers[a].type);
                }

                if (registers[a].value.i == 0) {
                    frame->pc += c ? -b : b;
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
    VM *vm = make_vm(frame, 0);

    execute_function(vm);

    gc(vm);
    free(vm);

    // TODO - free last closure [?]
    // Other closrues should be already taken care of
    // by the last gc sweep, but this one existed outside
    // of the heap.
}
