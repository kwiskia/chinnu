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
typedef struct Object Object;

struct Upval {
    int refcount;
    int open;
    union {
        struct {
            int slot;
            Frame *frame;
        } ref;
        Object *o;
    };
};

struct Closure {
    Chunk *chunk;
    Upval **upvals;
};

struct Frame {
    Frame *parent;

    Closure *closure;
    Object **registers;
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

struct Object {
    int type;
    union {
        int i;
        double d;
        char *s;
        Closure *c;
    };
};

typedef enum {
    OBJECT_INT,
    OBJECT_REAL,
    OBJECT_BOOL,
    OBJECT_NULL,
    OBJECT_STRING,
    OBJECT_CLOSURE
} ObjectType;

double cast_to_double(Object *object) {
    if (object->type == OBJECT_INT) {
        return (double) object->i;
    }

    return object->d;
}

void copy_object(Object *a, Object *b) {
    // TODO - clear old string?

    switch (b->type) {
        case OBJECT_INT:
            a->type = OBJECT_INT;
            a->i = b->i;
            break;

        case OBJECT_BOOL:
            a->type = OBJECT_BOOL;
            a->i = b->i;
            break;

        case OBJECT_REAL:
            a->type = OBJECT_REAL;
            a->d = b->d;
            break;

        case OBJECT_NULL:
            a->type = OBJECT_NULL;
            break;

        case OBJECT_STRING:
            a->type = OBJECT_STRING;
            a->s = b->s;
            break;

        case OBJECT_CLOSURE:
            a->type = OBJECT_CLOSURE;
            a->c = b->c;
            break;
    }
}

void print(Object *o) {
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

        case OBJECT_STRING:
            printf("<STRING %s>\n", o->s);
            break;
    }
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
    Object **registers = malloc(numregs * sizeof **registers);

    if (!frame || !registers) {
        fatal("Out of memory.");
    }

    int i;
    for (i = 0; i < numregs; i++) {
        Object *object = malloc(sizeof *object);

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

void execute_function(State *state) {
restart: {
    Frame *frame = state->current;
    Closure *closure = frame->closure;
    Chunk *chunk = closure->chunk;
    Object **registers = frame->registers;

    while (frame->pc < chunk->numinstructions) {
        int instruction = chunk->instructions[frame->pc];

        int o = GET_O(instruction);
        int a = GET_A(instruction);
        int b = GET_B(instruction);
        int c = GET_C(instruction);

        switch (o) {
            case OP_MOVE:
                if (b < 256) {
                    copy_object(registers[a], registers[b]);
                } else {
                    // TODO - clear old string?

                    Constant *constant = chunk->constants[b - 256];

                    switch (constant->type) {
                        case CONST_INT:
                            registers[a]->type = OBJECT_INT;
                            registers[a]->i = constant->i;
                            break;

                        case CONST_BOOL:
                            registers[a]->type = OBJECT_BOOL;
                            registers[a]->i = constant->i;
                            break;

                        case CONST_REAL:
                            registers[a]->type = OBJECT_REAL;
                            registers[a]->d = constant->d;
                            break;

                        case CONST_NULL:
                            registers[a]->type = OBJECT_NULL;
                            break;

                        case CONST_STRING:
                            registers[a]->type = OBJECT_STRING;
                            registers[a]->s = constant->s;
                            break;
                    }
                }
                break;

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

                        case OBJECT_STRING:
                            registers[a]->i = strcmp(registers[b]->s, registers[c]->s) == 0;
                            break;
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

                registers[a]->type = OBJECT_CLOSURE;
                registers[a]->c = child;
            } break;

            case OP_CALL:
            {
                if (registers[b]->type != OBJECT_CLOSURE) {
                    fatal("Tried to call non-closure of type %d (value %d).", registers[b]->type, registers[b]->i);
                }

                // TODO - safety issue (see compile.c for notes)

                Closure *child = registers[b]->c;
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
                if (b < 256) {
                    copy_object(registers[0], registers[b]);
                } else {
                    // TODO - clear old string?

                    Constant *constant = chunk->constants[b - 256];

                    switch (constant->type) {
                        case CONST_INT:
                            registers[0]->type = OBJECT_INT;
                            registers[0]->i = constant->i;
                            break;

                        case CONST_BOOL:
                            registers[0]->type = OBJECT_BOOL;
                            registers[0]->i = constant->i;
                            break;

                        case CONST_REAL:
                            registers[0]->type = OBJECT_REAL;
                            registers[0]->d = constant->d;
                            break;

                        case CONST_NULL:
                            registers[0]->type = OBJECT_NULL;
                            break;

                        case CONST_STRING:
                            registers[0]->type = OBJECT_STRING;
                            registers[0]->s = constant->s;
                            break;
                    }
                }

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

                print(registers[0]);

                if (state->current->parent != NULL) {
                    Frame *p = state->current->parent;
                    copy_object(p->registers[GET_A(p->closure->chunk->instructions[p->pc++])], registers[0]);

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
    execute_function(make_state(make_frame(NULL, make_closure(chunk))));
}
