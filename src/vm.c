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
    } value;
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
        return (double) object->value.i;
    }

    return object->value.d;
}

void copy_object(Object *a, Object *b) {
    // TODO - clear old string?

    switch (b->type) {
        case OBJECT_INT:
            a->type = OBJECT_INT;
            a->value.i = b->value.i;
            break;

        case OBJECT_BOOL:
            a->type = OBJECT_BOOL;
            a->value.i = b->value.i;
            break;

        case OBJECT_REAL:
            a->type = OBJECT_REAL;
            a->value.d = b->value.d;
            break;

        case OBJECT_NULL:
            a->type = OBJECT_NULL;
            break;

        case OBJECT_STRING:
            a->type = OBJECT_STRING;
            a->value.s = b->value.s;
            break;

        case OBJECT_CLOSURE:
            a->type = OBJECT_CLOSURE;
            a->value.c = b->value.c;
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

    while (frame->pc < chunk->numinstructions) {
        int instruction = chunk->instructions[frame->pc];

        int o = GET_O(instruction);
        int a = GET_A(instruction);
        int b = GET_B(instruction);
        int c = GET_C(instruction);

        switch (o) {
            case OP_MOVE:
                if (b < 256) {
                    copy_object(frame->registers[a], frame->registers[b]);
                } else {
                    // TODO - clear old string?

                    Constant *constant = chunk->constants[b - 256];

                    switch (constant->type) {
                        case CONST_INT:
                            frame->registers[a]->type = OBJECT_INT;
                            frame->registers[a]->value.i = constant->value->i;
                            break;

                        case CONST_BOOL:
                            frame->registers[a]->type = OBJECT_BOOL;
                            frame->registers[a]->value.i = constant->value->i;
                            break;

                        case CONST_REAL:
                            frame->registers[a]->type = OBJECT_REAL;
                            frame->registers[a]->value.d = constant->value->d;
                            break;

                        case CONST_NULL:
                            frame->registers[a]->type = OBJECT_NULL;
                            break;

                        case CONST_STRING:
                            frame->registers[a]->type = OBJECT_STRING;
                            frame->registers[a]->value.s = constant->value->s;
                            break;
                    }
                }
                break;

            case OP_GETUPVAR:
            {
                Upval *upval = closure->upvals[b];

                if (!upval->open) {
                    // upval is closed
                    copy_object(frame->registers[a], upval->o);
                } else {
                    // still on stack
                    copy_object(frame->registers[a], upval->ref.frame->registers[upval->ref.slot]);
                }
            } break;

            case OP_SETUPVAR:
            {
                Upval *upval = closure->upvals[b];

                if (!upval->open) {
                    // upval is closed
                    copy_object(upval->o, frame->registers[a]);
                } else {
                    // still on stack
                    copy_object(upval->ref.frame->registers[upval->ref.slot], frame->registers[a]);
                }
            } break;

            case OP_ADD:
                if (frame->registers[b]->type != OBJECT_INT && frame->registers[b]->type != OBJECT_REAL) {
                    fatal("Tried to add non-numbers.");
                }

                if (frame->registers[c]->type != OBJECT_INT && frame->registers[c]->type != OBJECT_REAL) {
                    fatal("Tried to add non-numbers.");
                }

                // TODO - do with constants as well
                if (frame->registers[b]->type == OBJECT_INT && frame->registers[c]->type == OBJECT_INT) {
                    frame->registers[a]->type = OBJECT_INT;
                    frame->registers[a]->value.i = frame->registers[b]->value.i + frame->registers[c]->value.i;
                } else {
                    frame->registers[a]->type = OBJECT_REAL;
                    frame->registers[a]->value.d = cast_to_double(frame->registers[b]) + cast_to_double(frame->registers[c]);
                }
                break;

            case OP_SUB:
                if (frame->registers[b]->type != OBJECT_INT && frame->registers[b]->type != OBJECT_REAL) {
                    fatal("Tried to sub non-numbers.");
                }

                if (frame->registers[c]->type != OBJECT_INT && frame->registers[c]->type != OBJECT_REAL) {
                    fatal("Tried to sub non-numbers.");
                }

                // TODO - do with constants as well
                if (frame->registers[b]->type == OBJECT_INT && frame->registers[c]->type == OBJECT_INT) {
                    frame->registers[a]->type = OBJECT_INT;
                    frame->registers[a]->value.i = frame->registers[b]->value.i - frame->registers[c]->value.i;
                } else {
                    frame->registers[a]->type = OBJECT_REAL;
                    frame->registers[a]->value.d = cast_to_double(frame->registers[b]) - cast_to_double(frame->registers[c]);
                }
                break;

            case OP_MUL:
                if (frame->registers[b]->type != OBJECT_INT && frame->registers[b]->type != OBJECT_REAL) {
                    fatal("Tried to mul non-numbers.");
                }

                if (frame->registers[c]->type != OBJECT_INT && frame->registers[c]->type != OBJECT_REAL) {
                    fatal("Tried to mul non-numbers.");
                }

                // TODO - do with constants as well
                if (frame->registers[b]->type == OBJECT_INT && frame->registers[c]->type == OBJECT_INT) {
                    frame->registers[a]->type = OBJECT_INT;
                    frame->registers[a]->value.i = frame->registers[b]->value.i * frame->registers[c]->value.i;
                } else {
                    frame->registers[a]->type = OBJECT_REAL;
                    frame->registers[a]->value.d = cast_to_double(frame->registers[b]) * cast_to_double(frame->registers[c]);
                }
                break;

            case OP_DIV:
                if (frame->registers[b]->type != OBJECT_INT && frame->registers[b]->type != OBJECT_REAL) {
                    fatal("Tried to div non-numbers.");
                }

                if (frame->registers[c]->type != OBJECT_INT && frame->registers[c]->type != OBJECT_REAL) {
                    fatal("Tried to div non-numbers.");
                }

                if (cast_to_double(frame->registers[c]) == 0) {
                    fatal("Div by zero.");
                }

                // TODO - do with constants as well
                if (frame->registers[b]->type == OBJECT_INT && frame->registers[c]->type == OBJECT_INT) {
                    frame->registers[a]->type = OBJECT_INT;
                    frame->registers[a]->value.i = frame->registers[b]->value.i / frame->registers[c]->value.i;
                } else {
                    frame->registers[a]->type = OBJECT_REAL;
                    frame->registers[a]->value.d = cast_to_double(frame->registers[b]) / cast_to_double(frame->registers[c]);
                }
                break;

            case OP_NEG:
                // TODO - do for constants as well
                if (frame->registers[a]->type == OBJECT_INT) {
                    frame->registers[a]->value.i = -frame->registers[a]->value.i;
                } else if (frame->registers[a]->type == OBJECT_REAL) {
                    frame->registers[a]->value.d = -frame->registers[a]->value.d;
                } else {
                    fatal("Tried to negate non-numeric type.");
                }
                break;

            case OP_NOT:
                if (frame->registers[a]->type != OBJECT_BOOL) {
                    fatal("Expected boolean type, not %d.", frame->registers[a]->type);
                }

                frame->registers[a]->value.i = frame->registers[a]->value.i == 1 ? 0 : 1;
                break;

            case OP_EQ:
                if (frame->registers[b]->type != frame->registers[c]->type) {
                    frame->registers[a]->value.i = 0;
                } else {
                    switch (frame->registers[b]->type) {
                        case OBJECT_INT:
                        case OBJECT_BOOL:
                            frame->registers[a]->value.i = frame->registers[b]->value.i == frame->registers[c]->value.i;
                            break;

                        case OBJECT_NULL:
                            frame->registers[a]->value.i = 1;
                            break;

                        case OBJECT_REAL:
                            frame->registers[a]->value.d = frame->registers[b]->value.d == frame->registers[c]->value.d;
                            break;

                        case OBJECT_STRING:
                            frame->registers[a]->value.i = strcmp(frame->registers[b]->value.s, frame->registers[c]->value.s) == 0;
                            break;
                    }
                }

                frame->registers[a]->type = OBJECT_BOOL;
                break;

            case OP_LT:
                if (frame->registers[b]->type != OBJECT_INT && frame->registers[b]->type != OBJECT_REAL) {
                    fatal("Tried to compare non-numbers.");
                }

                if (frame->registers[c]->type != OBJECT_INT && frame->registers[c]->type != OBJECT_REAL) {
                    fatal("Tried to compare non-numbers.");
                }

                // TODO - do with constants as well
                if (frame->registers[b]->type == OBJECT_INT && frame->registers[c]->type == OBJECT_INT) {
                    frame->registers[a]->type = OBJECT_BOOL;
                    frame->registers[a]->value.i = frame->registers[b]->value.i < frame->registers[c]->value.i;
                } else {
                    frame->registers[a]->type = OBJECT_BOOL;
                    frame->registers[a]->value.d = cast_to_double(frame->registers[b]) < cast_to_double(frame->registers[c]);
                }
                break;

            case OP_LE:
                if (frame->registers[b]->type != OBJECT_INT && frame->registers[b]->type != OBJECT_REAL) {
                    fatal("Tried to compare non-numbers.");
                }

                if (frame->registers[c]->type != OBJECT_INT && frame->registers[c]->type != OBJECT_REAL) {
                    fatal("Tried to compare non-numbers.");
                }

                // TODO - do with constants as well
                if (frame->registers[b]->type == OBJECT_INT && frame->registers[c]->type == OBJECT_INT) {
                    frame->registers[a]->type = OBJECT_BOOL;
                    frame->registers[a]->value.i = frame->registers[b]->value.i <= frame->registers[c]->value.i;
                } else {
                    frame->registers[a]->type = OBJECT_BOOL;
                    frame->registers[a]->value.d = cast_to_double(frame->registers[b]) <= cast_to_double(frame->registers[c]);
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

                frame->registers[a]->type = OBJECT_CLOSURE;
                frame->registers[a]->value.c = child;
            } break;

            case OP_CALL:
            {
                if (frame->registers[b]->type != OBJECT_CLOSURE) {
                    fatal("Tried to call non-closure of type %d (value %d).", frame->registers[b]->type, frame->registers[b]->value.i);
                }

                // TODO - safety issue (see compile.c for notes)

                Closure *child = frame->registers[b]->value.c;
                Frame *subframe = make_frame(frame, child);

                int i;
                for (i = 0; i < child->chunk->numparams; i++) {
                    copy_object(subframe->registers[i + 1], frame->registers[c + i]);
                }

                state->current = subframe;
                goto restart;
            } break;

            case OP_RETURN:
            {
                if (b < 256) {
                    copy_object(frame->registers[0], frame->registers[b]);
                } else {
                    // TODO - clear old string?

                    Constant *constant = chunk->constants[b - 256];

                    switch (constant->type) {
                        case CONST_INT:
                            frame->registers[0]->type = OBJECT_INT;
                            frame->registers[0]->value.i = constant->value->i;
                            break;

                        case CONST_BOOL:
                            frame->registers[0]->type = OBJECT_BOOL;
                            frame->registers[0]->value.i = constant->value->i;
                            break;

                        case CONST_REAL:
                            frame->registers[0]->type = OBJECT_REAL;
                            frame->registers[0]->value.d = constant->value->d;
                            break;

                        case CONST_NULL:
                            frame->registers[0]->type = OBJECT_NULL;
                            break;

                        case CONST_STRING:
                            frame->registers[0]->type = OBJECT_STRING;
                            frame->registers[0]->value.s = constant->value->s;
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
                            u->o = frame->registers[u->ref.slot];
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
                    copy_object(p->registers[GET_A(p->closure->chunk->instructions[p->pc++])], frame->registers[0]);

                    printf("Returning %d\n", frame->registers[0]->value.i);

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
                if (frame->registers[a]->type != OBJECT_BOOL) {
                    fatal("Expected boolean type, not %d.", frame->registers[a]->type);
                }

                if (frame->registers[a]->value.i == 1) {
                    if (c) {
                        frame->pc -= b;
                    } else {
                        frame->pc += b;
                    }
                }
                break;

            case OP_JUMP_FALSE:
                if (frame->registers[a]->type != OBJECT_BOOL) {
                    fatal("Expected boolean type, not %d.", frame->registers[a]->type);
                }

                if (frame->registers[a]->value.i == 0) {
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
