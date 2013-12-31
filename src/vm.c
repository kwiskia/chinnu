/*
 * Copyright (C) 2012, Eric Fritz
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vm.h"
#include "chinnu.h"
#include "bytecode.h"

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
    }
}

Proto *make_proto(Chunk *chunk) {
    Proto *proto = malloc(sizeof(Proto));
    Up **upvars = malloc(sizeof(Up) * chunk->scope->numupvars);

    if (!proto || !upvars) {
        fatal("Out of memory.");
    }

    int i;
    for (i = 0; i < chunk->scope->numupvars; i++) {
        Up *upvar = malloc(sizeof(Up));

        if (!upvar) {
            fatal("Out of memory.");
        }

        upvars[i] = upvar;
    }

    proto->chunk = chunk;
    proto->upvars = upvars;
    return proto;
}

Frame *make_frame(Frame *parent, Proto *proto) {
    int numregs = proto->chunk->scope->numlocals + proto->chunk->numtemps + 1;

    Frame *frame = malloc(sizeof(Frame));
    Object **registers = malloc(sizeof(Object) * numregs);

    if (!frame || !registers) {
        fatal("Out of memory.");
    }

    int i;
    for (i = 0; i < numregs; i++) {
        Object *object = malloc(sizeof(Object));
        Val *value = malloc(sizeof(Val));

        if (!object || !value) {
            fatal("Out of memory.");
        }

        object->type = -1; // uninitialized
        object->value = *value;
        registers[i] = object;
    }

    frame->pc = 0;
    frame->parent = parent;
    frame->proto = proto;
    frame->registers = registers;
    return frame;
}

State *make_state(Frame *root) {
    State *state = malloc(sizeof(State));

    if (!state) {
        fatal("Out of memory.");
    }

    state->current = root;
    return state;
}

Object *execute_function(State *state) {
    Frame *frame = state->current;
    Proto *proto = frame->proto;
    Chunk *chunk = proto->chunk;

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
                fatal("Opcode OP_GETUPVAR not implemented.\n");
                break;

            case OP_SETUPVAR:
                fatal("Opcode OP_SETUPVAR not implemented.\n");
                break;

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
                // TODO - upvars

                frame->registers[a]->type = OBJECT_CLOSURE;
                frame->registers[a]->value.p = make_proto(chunk->children[b]);
            } break;

            case OP_CALL:
            {
                if (frame->registers[b]->type != OBJECT_CLOSURE) {
                    fatal("Tried to call non-closure.");
                }

                // TODO - safety issue (see compile.c for notes)

                Proto *child = frame->registers[b]->value.p;
                Frame *subframe = make_frame(frame, child);

                int i;
                for (i = 0; i < child->chunk->scope->numparams; i++) {
                    copy_object(subframe->registers[i + 1], frame->registers[c + i]);
                }

                state->current = subframe;
                Object *ret = execute_function(state);
                state->current = state->current->parent;

                copy_object(frame->registers[a], ret);
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

                goto exit;
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

exit:
    printf("Returning %d\n", frame->registers[0]->value.i);
    return frame->registers[0];
}

Object *execute(Chunk *chunk) {
    return execute_function(make_state(make_frame(NULL, make_proto(chunk))));
}
