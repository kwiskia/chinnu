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

Object **make_frame(Chunk *chunk) {
    // TODO - handle upvalues
    Object **frame = malloc(sizeof(Object) * (chunk->scope->numlocals + chunk->numtemps + 1));

    if (!frame) {
        fatal("Out of memory.");
    }

    int i;
    for (i = 0; i < chunk->scope->numlocals + chunk->numtemps + 1; i++) {
        Object *object = malloc(sizeof(Object));
        Val *value = malloc(sizeof(Val));

        if (!object || !value) {
            fatal("Out of memory.");
        }

        object->type = -1; // uninitialized
        object->value = *value;
        frame[i] = object;
    }

    return frame;
}

Object *execute_function(Chunk *chunk, Object **frame) {
    int pc = 0;

    while (pc < chunk->numinstructions) {
        int instruction = chunk->instructions[pc];

        int o = GET_O(instruction);
        int a = GET_A(instruction);
        int b = GET_B(instruction);
        int c = GET_C(instruction);

        switch (o) {
            case OP_MOVE:
                if (b < 256) {
                    copy_object(frame[a], frame[b]);
                } else {
                    // TODO - clear old string?

                    Constant *constant = chunk->constants[b - 256];

                    switch (constant->type) {
                        case CONST_INT:
                            frame[a]->type = OBJECT_INT;
                            frame[a]->value.i = constant->value->i;
                            break;

                        case CONST_BOOL:
                            frame[a]->type = OBJECT_BOOL;
                            frame[a]->value.i = constant->value->i;
                            break;

                        case CONST_REAL:
                            frame[a]->type = OBJECT_REAL;
                            frame[a]->value.d = constant->value->d;
                            break;

                        case CONST_NULL:
                            frame[a]->type = OBJECT_NULL;
                            break;

                        case CONST_STRING:
                            frame[a]->type = OBJECT_STRING;
                            frame[a]->value.s = constant->value->s;
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
                if (frame[b]->type != OBJECT_INT && frame[b]->type != OBJECT_REAL) {
                    fatal("Tried to add non-numbers.");
                }

                if (frame[c]->type != OBJECT_INT && frame[c]->type != OBJECT_REAL) {
                    fatal("Tried to add non-numbers.");
                }

                // TODO - do with constants as well
                if (frame[b]->type == OBJECT_INT && frame[c]->type == OBJECT_INT) {
                    frame[a]->type = OBJECT_INT;
                    frame[a]->value.i = frame[b]->value.i + frame[c]->value.i;
                } else {
                    frame[a]->type = OBJECT_REAL;
                    frame[a]->value.d = cast_to_double(frame[b]) + cast_to_double(frame[c]);
                }
                break;

            case OP_SUB:
                if (frame[b]->type != OBJECT_INT && frame[b]->type != OBJECT_REAL) {
                    fatal("Tried to sub non-numbers.");
                }

                if (frame[c]->type != OBJECT_INT && frame[c]->type != OBJECT_REAL) {
                    fatal("Tried to sub non-numbers.");
                }

                // TODO - do with constants as well
                if (frame[b]->type == OBJECT_INT && frame[c]->type == OBJECT_INT) {
                    frame[a]->type = OBJECT_INT;
                    frame[a]->value.i = frame[b]->value.i - frame[c]->value.i;
                } else {
                    frame[a]->type = OBJECT_REAL;
                    frame[a]->value.d = cast_to_double(frame[b]) - cast_to_double(frame[c]);
                }
                break;

            case OP_MUL:
                if (frame[b]->type != OBJECT_INT && frame[b]->type != OBJECT_REAL) {
                    fatal("Tried to mul non-numbers.");
                }

                if (frame[c]->type != OBJECT_INT && frame[c]->type != OBJECT_REAL) {
                    fatal("Tried to mul non-numbers.");
                }

                // TODO - do with constants as well
                if (frame[b]->type == OBJECT_INT && frame[c]->type == OBJECT_INT) {
                    frame[a]->type = OBJECT_INT;
                    frame[a]->value.i = frame[b]->value.i * frame[c]->value.i;
                } else {
                    frame[a]->type = OBJECT_REAL;
                    frame[a]->value.d = cast_to_double(frame[b]) * cast_to_double(frame[c]);
                }
                break;

            case OP_DIV:
                if (frame[b]->type != OBJECT_INT && frame[b]->type != OBJECT_REAL) {
                    fatal("Tried to div non-numbers.");
                }

                if (frame[c]->type != OBJECT_INT && frame[c]->type != OBJECT_REAL) {
                    fatal("Tried to div non-numbers.");
                }

                if (cast_to_double(frame[c]) == 0) {
                    fatal("Div by zero.");
                }

                // TODO - do with constants as well
                if (frame[b]->type == OBJECT_INT && frame[c]->type == OBJECT_INT) {
                    frame[a]->type = OBJECT_INT;
                    frame[a]->value.i = frame[b]->value.i / frame[c]->value.i;
                } else {
                    frame[a]->type = OBJECT_REAL;
                    frame[a]->value.d = cast_to_double(frame[b]) / cast_to_double(frame[c]);
                }
                break;

            case OP_NEG:
                // TODO - do for constants as well
                if (frame[a]->type == OBJECT_INT) {
                    frame[a]->value.i = -frame[a]->value.i;
                } else if (frame[a]->type == OBJECT_REAL) {
                    frame[a]->value.d = -frame[a]->value.d;
                } else {
                    fatal("Tried to negate non-numeric type.");
                }
                break;

            case OP_NOT:
                if (frame[a]->type != OBJECT_BOOL) {
                    fatal("Expected boolean type, not %d.", frame[a]->type);
                }

                frame[a]->value.i = frame[a]->value.i == 1 ? 0 : 1;
                break;

            case OP_EQ:
                if (frame[b]->type != frame[c]->type) {
                    frame[a]->value.i = 0;
                } else {
                    switch (frame[b]->type) {
                        case OBJECT_INT:
                        case OBJECT_BOOL:
                            frame[a]->value.i = frame[b]->value.i == frame[c]->value.i;
                            break;

                        case OBJECT_NULL:
                            frame[a]->value.i = 1;
                            break;

                        case OBJECT_REAL:
                            frame[a]->value.d = frame[b]->value.d == frame[c]->value.d;
                            break;

                        case OBJECT_STRING:
                            frame[a]->value.i = strcmp(frame[b]->value.s, frame[c]->value.s) == 0;
                            break;
                    }
                }

                frame[a]->type = OBJECT_BOOL;
                break;

            case OP_LT:
                if (frame[b]->type != OBJECT_INT && frame[b]->type != OBJECT_REAL) {
                    fatal("Tried to compare non-numbers.");
                }

                if (frame[c]->type != OBJECT_INT && frame[c]->type != OBJECT_REAL) {
                    fatal("Tried to compare non-numbers.");
                }

                // TODO - do with constants as well
                if (frame[b]->type == OBJECT_INT && frame[c]->type == OBJECT_INT) {
                    frame[a]->type = OBJECT_BOOL;
                    frame[a]->value.i = frame[b]->value.i < frame[c]->value.i;
                } else {
                    frame[a]->type = OBJECT_BOOL;
                    frame[a]->value.d = cast_to_double(frame[b]) < cast_to_double(frame[c]);
                }
                break;

            case OP_LE:
                if (frame[b]->type != OBJECT_INT && frame[b]->type != OBJECT_REAL) {
                    fatal("Tried to compare non-numbers.");
                }

                if (frame[c]->type != OBJECT_INT && frame[c]->type != OBJECT_REAL) {
                    fatal("Tried to compare non-numbers.");
                }

                // TODO - do with constants as well
                if (frame[b]->type == OBJECT_INT && frame[c]->type == OBJECT_INT) {
                    frame[a]->type = OBJECT_BOOL;
                    frame[a]->value.i = frame[b]->value.i <= frame[c]->value.i;
                } else {
                    frame[a]->type = OBJECT_BOOL;
                    frame[a]->value.d = cast_to_double(frame[b]) <= cast_to_double(frame[c]);
                }
                break;

            case OP_CLOSURE:
            {
                frame[a]->type = OBJECT_CLOSURE;
                frame[a]->value.c = chunk->children[b];
            } break;

            case OP_CALL:
            {
                if (frame[b]->type != OBJECT_CLOSURE) {
                    fatal("Tried to call non-closure.");
                }

                // TODO - safety issue (see compile.c for notes)

                Chunk *child = frame[b]->value.c;

                Object **subframe = make_frame(child);

                int i;
                for (i = 0; i < child->scope->numparams; i++) {
                    copy_object(subframe[i + 1], frame[c + i]);
                }

                Object *ret = execute_function(child, subframe);

                copy_object(frame[a], ret);
            } break;

            case OP_RETURN:
            {
                if (b < 256) {
                    copy_object(frame[0], frame[b]);
                } else {
                    // TODO - clear old string?

                    Constant *constant = chunk->constants[b - 256];

                    switch (constant->type) {
                        case CONST_INT:
                            frame[a]->type = OBJECT_INT;
                            frame[a]->value.i = constant->value->i;
                            break;

                        case CONST_BOOL:
                            frame[a]->type = OBJECT_BOOL;
                            frame[a]->value.i = constant->value->i;
                            break;

                        case CONST_REAL:
                            frame[a]->type = OBJECT_REAL;
                            frame[a]->value.d = constant->value->d;
                            break;

                        case CONST_NULL:
                            frame[a]->type = OBJECT_NULL;
                            break;

                        case CONST_STRING:
                            frame[a]->type = OBJECT_STRING;
                            frame[a]->value.s = constant->value->s;
                            break;
                    }
                }

                goto exit;
            } break;

            case OP_JUMP:
                if (c) {
                    pc -= b;
                } else {
                    pc += b;
                }
                break;

            case OP_JUMP_TRUE:
                if (frame[a]->type != OBJECT_BOOL) {
                    fatal("Expected boolean type, not %d.", frame[a]->type);
                }

                if (frame[a]->value.i == 1) {
                    if (c) {
                        pc -= b;
                    } else {
                        pc += b;
                    }
                }
                break;

            case OP_JUMP_FALSE:
                if (frame[a]->type != OBJECT_BOOL) {
                    fatal("Expected boolean type, not %d.", frame[a]->type);
                }

                if (frame[a]->value.i == 0) {
                    if (c) {
                        pc -= b;
                    } else {
                        pc += b;
                    }
                }
                break;
        }

        pc++;
    }

exit:
    printf("Returning %d\n", frame[0]->value.i);
    return frame[0];
}

Object *execute(Chunk *chunk) {
    return execute_function(chunk, make_frame(chunk));
}
