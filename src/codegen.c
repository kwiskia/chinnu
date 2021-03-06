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

#include "chinnu.h"
#include "codegen.h"
#include "bytecode.h"

#define MAX(a, b) ((a > b) ? a : b)

#define CONSTANT_CHUNK_SIZE 4
#define INSTRUCTION_CHUNK_SIZE 32
#define CHUNK_CHUNK_SIZE 2

Chunk *make_chunk() {
    Chunk *chunk = malloc(sizeof *chunk);
    Constant **constants = malloc(CONSTANT_CHUNK_SIZE * sizeof **constants);
    int *instructions = malloc(INSTRUCTION_CHUNK_SIZE * sizeof *instructions);
    Chunk **children = malloc(CHUNK_CHUNK_SIZE * sizeof **children);

    if (!chunk || !constants || !instructions || !children) {
        fatal("Out of memory.");
    }

    chunk->constants = constants;
    chunk->instructions = instructions;
    chunk->children = children;

    chunk->numtemps = 0;
    chunk->numconstants = 0;
    chunk->numinstructions = 0;
    chunk->numchildren = 0;

    return chunk;
}

void free_chunk(Chunk *chunk) {
    int i;
    for (i = 0; i < chunk->numconstants; i++) {
        if (chunk->constants[i]->type == CONST_STRING) {
            free(chunk->constants[i]->value.s);
        }

        free(chunk->constants[i]);
    }

    for (i = 0; i < chunk->numchildren; i++) {
        free_chunk(chunk->children[i]);
    }

    free(chunk->constants);
    free(chunk->instructions);
    free(chunk->children);
    free(chunk);
}

int add_constant(Chunk *chunk, Constant *constant) {
    if (chunk->numconstants % CONSTANT_CHUNK_SIZE == 0) {
        Constant **resize = realloc(chunk->constants, (chunk->numconstants + CONSTANT_CHUNK_SIZE) * sizeof **resize);

        if (!resize) {
            fatal("Out of memory.");
        }

        chunk->constants = resize;
    }

    chunk->constants[chunk->numconstants] = constant;
    return chunk->numconstants++;
}

Constant *make_const() {
    Constant *c = malloc(sizeof *c);

    if (!c) {
        fatal("Out of memory.");
    }

    return c;
}

int add_int(Chunk *chunk, int value) {
    int i;
    for (i = 0; i < chunk->numconstants; i++) {
        if (chunk->constants[i]->type == CONST_INT && chunk->constants[i]->value.i == value) {
            return i;
        }
    }

    Constant *c = make_const();

    c->type = CONST_INT;
    c->value.i = value;

    return add_constant(chunk, c);
}

int add_real(Chunk *chunk, double value) {
    int i;
    for (i = 0; i < chunk->numconstants; i++) {
        if (chunk->constants[i]->type == CONST_REAL && chunk->constants[i]->value.d == value) {
            return i;
        }
    }

    Constant *c = make_const();

    c->type = CONST_REAL;
    c->value.d = value;

    return add_constant(chunk, c);
}

int add_bool(Chunk *chunk, int value) {
    int i;
    for (i = 0; i < chunk->numconstants; i++) {
        if (chunk->constants[i]->type == CONST_BOOL && chunk->constants[i]->value.i == value) {
            return i;
        }
    }

    Constant *c = make_const();

    c->type = CONST_BOOL;
    c->value.i = value;

    return add_constant(chunk, c);
}

int add_null(Chunk *chunk) {
    int i;
    for (i = 0; i < chunk->numconstants; i++) {
        if (chunk->constants[i]->type == CONST_NULL) {
            return i;
        }
    }

    Constant *c = make_const();

    c->type = CONST_NULL;

    return add_constant(chunk, c);
}

int add_string(Chunk *chunk, char *value) {
    int i;
    for (i = 0; i < chunk->numconstants; i++) {
        if (chunk->constants[i]->type == CONST_STRING && strcmp(chunk->constants[i]->value.s, value) == 0) {
            return i;
        }
    }

    Constant *c = make_const();

    c->type = CONST_STRING;
    c->value.s = strdup(value);

    return add_constant(chunk, c);
}

int add_func_child(Chunk *chunk, Chunk *child) {
    if (chunk->numchildren % CHUNK_CHUNK_SIZE == 0) {
        Chunk **resize = realloc(chunk->children, (chunk->numchildren + CHUNK_CHUNK_SIZE) * sizeof **resize);

        if (!resize) {
            fatal("Out of memory.");
        }

        chunk->children = resize;
    }

    chunk->children[chunk->numchildren] = child;
    return chunk->numchildren++;
}

int add_instruction(Chunk *chunk, int instruction) {
    if (chunk->numinstructions % INSTRUCTION_CHUNK_SIZE == 0) {
        int *resize = realloc(chunk->instructions, (chunk->numinstructions + INSTRUCTION_CHUNK_SIZE) * sizeof *resize);

        if (!resize) {
            fatal("Out of memory.");
        }

        chunk->instructions = resize;
    }

    chunk->instructions[chunk->numinstructions] = instruction;
    return chunk->numinstructions++;
}

int get_local_index(Scope *scope, Symbol *symbol) {
    int i;
    for (i = 0; i < scope->numlocals; i++) {
        if (scope->locals[i] == symbol) {
            return i + 1;
        }
    }

    return -1;
}

int get_upvar_index(Scope *scope, Symbol *symbol) {
    int i;
    for (i = 0; i < scope->numupvars; i++) {
        if (scope->upvars[i] == symbol) {
            return i;
        }
    }

    return -1;
}

int get_temp_index(Scope *scope, int temp) {
    return scope->numlocals + temp + 1;
}

/* forward */
int compile_list(ExpressionList *list, Chunk *chunk, Scope *scope, int dest, int temp);

int compile_expr(Expression *expr, Chunk *chunk, Scope *scope, int dest, int temp) {
    switch (expr->type) {
        case TYPE_MODULE:
        {
            int max = compile_expr(expr->lexpr, chunk, scope, dest, temp);
            add_instruction(chunk, CREATE(OP_RETURN, 0, 0, 0));

            return max;
        }

        case TYPE_DECLARATION:
        {
            if (expr->rexpr) {
                int max = compile_expr(expr->rexpr, chunk, scope, dest, temp);
                add_instruction(chunk, CREATE(OP_MOVE, get_local_index(scope, expr->symbol), dest, 0));

                return max;
            }

            return temp;
        }

        case TYPE_FUNC:
        {
            Chunk *child = make_chunk();
            int max = compile_expr(expr->rexpr, child, expr->scope, 0, 0);
            add_instruction(child, CREATE(OP_RETURN, 0, 0, 0));

            child->numtemps = max;
            child->numlocals = expr->scope->numlocals;
            child->numupvars = expr->scope->numupvars;
            child->numparams = expr->scope->numparams;

            int index = add_func_child(chunk, child);
            add_instruction(chunk, CREATE(OP_CLOSURE, dest, index, 0));

            int i;
            for (i = 0; i < expr->scope->numupvars; i++) {
                int index = get_local_index(scope, expr->scope->upvars[i]);

                if (index != -1) {
                    add_instruction(chunk, CREATE(OP_MOVE, i, index, 0));
                } else {
                    index = get_upvar_index(scope, expr->scope->upvars[i]);
                    add_instruction(chunk, CREATE(OP_GETUPVAR, i, index, 0));
                }
            }

            if (expr->symbol) {
                int index = get_local_index(scope, expr->symbol);
                add_instruction(chunk, CREATE(OP_MOVE, index, dest, 0));
            }

            return temp;
        }

        case TYPE_CALL:
        {
            // compile receiver
            int max = compile_expr(expr->lexpr, chunk, scope, dest, temp);

            /**
             * TODO
             * How to pass params safely? Currently, if you pass the wrong
             * number of params you will either waste temporaries (okay), or
             * the receiver will read into your local space (bad), or segfault
             * (also bad). Not sure the correct thing to do in this case.
             */

            if (expr->llist->head == NULL) {
                add_instruction(chunk, CREATE(OP_CALL, dest, dest, 0));
            } else {
                int f = get_temp_index(scope, temp);
                int t = f;

                ExpressionNode *head;
                for (head = expr->llist->head; head != NULL; head = head->next) {
                    int n = compile_expr(head->expr, chunk, scope, t, ++temp);
                    max = MAX(n, max);

                    t = get_temp_index(scope, temp);
                }

                add_instruction(chunk, CREATE(OP_CALL, dest, dest, f));
            }

            return max;
        }

        case TYPE_VARREF:
        {
            int index = get_local_index(scope, expr->symbol);

            if (index != -1) {
                add_instruction(chunk, CREATE(OP_MOVE, dest, index, 0));
            } else {
                index = get_upvar_index(scope, expr->symbol);
                add_instruction(chunk, CREATE(OP_GETUPVAR, dest, index, 0));
            }

            return temp;
        }

        /* control flow */
        case TYPE_IF:
        {
            //      cond
            // [t1] jump if false [t3 - t1]
            //      true branch
            // [t2] jump [nm - t2]
            // [t2 + 1] false branch
            // [nm]

            // condition
            int max1 = compile_expr(expr->cond, chunk, scope, dest, temp);

            // dummy jump 1
            int t1 = add_instruction(chunk, 0);

            // true branch
            int max2 = compile_expr(expr->lexpr, chunk, scope, dest, temp);

            // dummy jump 2
            int t2 = add_instruction(chunk, 0);

            int max3 = 0;
            if (expr->rexpr) {
                max3 = compile_expr(expr->rexpr, chunk, scope, dest, temp);
            } else {
                int index = add_null(chunk);
                add_instruction(chunk, CREATE(OP_MOVE, dest, index + 256, 0));
            }

            int nm = chunk->numinstructions;

            // fill in 1st jump
            chunk->instructions[t1] = CREATE(OP_JUMP_FALSE, dest, t2 - t1, 0);

            // fill in 2nd jump
            chunk->instructions[t2] = CREATE(OP_JUMP, 0, nm - t2 - 1, 0);

            return MAX(max1, MAX(max2, max3));
        } break;

        case TYPE_WHILE:
        {
            // [t0] cond
            // [t1] jump if false [t3 - t1]
            //      body
            // [t2] jump [t2 - t0]
            // [t3] null

            int t0 = chunk->numinstructions;
            int max1 = compile_expr(expr->cond, chunk, scope, dest, temp);

            // dummy jump 1
            int t1 = add_instruction(chunk, 0);

            // compile body
            int max2 = compile_expr(expr->lexpr, chunk, scope, dest, temp);

            // dummy jump 2
            int t2 = add_instruction(chunk, 0);

            // return null
            int index = add_null(chunk);
            int t3 = add_instruction(chunk, CREATE(OP_MOVE, dest, index + 256, 0));

            // fill in 1st jump
            chunk->instructions[t1] = CREATE(OP_JUMP_FALSE, dest, t3 - t1 - 1, 0);

            // fill in 2nd jump
            chunk->instructions[t2] = CREATE(OP_JUMP, 0, t3 - t0, 1);

            return MAX(max1, max2);
        }

        /* binary cases */
        case TYPE_ASSIGN:
        {
            int max = compile_expr(expr->rexpr, chunk, scope, dest, temp);

            int index = get_local_index(scope, expr->lexpr->symbol);

            if (index != -1) {
                add_instruction(chunk, CREATE(OP_MOVE, index, dest, 0));
            } else {
                index = get_upvar_index(scope, expr->lexpr->symbol);
                add_instruction(chunk, CREATE(OP_SETUPVAR, dest, index, 0));
            }

            return max;
        }

        case TYPE_ADD:
        {
            int max1 = compile_expr(expr->lexpr, chunk, scope, dest, temp);

            int t = get_temp_index(scope, temp);
            int max2 = compile_expr(expr->rexpr, chunk, scope, t, temp + 1);

            add_instruction(chunk, CREATE(OP_ADD, dest, dest, t));

            return MAX(max1, max2);
        }

        case TYPE_SUB:
        {
            int max1 = compile_expr(expr->lexpr, chunk, scope, dest, temp);

            int t = get_temp_index(scope, temp);
            int max2 = compile_expr(expr->rexpr, chunk, scope, t, temp + 1);

            add_instruction(chunk, CREATE(OP_SUB, dest, dest, t));

            return MAX(max1, max2);
        }

        case TYPE_MUL:
        {
            int max1 = compile_expr(expr->lexpr, chunk, scope, dest, temp);

            int t = get_temp_index(scope, temp);
            int max2 = compile_expr(expr->rexpr, chunk, scope, t, temp + 1);

            add_instruction(chunk, CREATE(OP_MUL, dest, dest, t));

            return MAX(max1, max2);
        }

        case TYPE_DIV:
        {
            int max1 = compile_expr(expr->lexpr, chunk, scope, dest, temp);

            int t = get_temp_index(scope, temp);
            int max2 = compile_expr(expr->rexpr, chunk, scope, t, temp + 1);

            add_instruction(chunk, CREATE(OP_DIV, dest, dest, t));

            return MAX(max1, max2);
        }

        case TYPE_MOD:
        {
            int max1 = compile_expr(expr->lexpr, chunk, scope, dest, temp);

            int t = get_temp_index(scope, temp);
            int max2 = compile_expr(expr->rexpr, chunk, scope, t, temp + 1);

            add_instruction(chunk, CREATE(OP_MOD, dest, dest, t));

            return MAX(max1, max2);
        }

        case TYPE_POW:
        {
            int max1 = compile_expr(expr->lexpr, chunk, scope, dest, temp);

            int t = get_temp_index(scope, temp);
            int max2 = compile_expr(expr->rexpr, chunk, scope, t, temp + 1);

            add_instruction(chunk, CREATE(OP_POW, dest, dest, t));

            return MAX(max1, max2);
        }

        /**
         * optimization - boolean-resulting commands should have a second
         * form that takes branch statements to jump to on true/false instead
         * of actually generating that value (for control flow).
         */

        case TYPE_EQEQ:
        {
            int max1 = compile_expr(expr->lexpr, chunk, scope, dest, temp);

            int t = get_temp_index(scope, temp);
            int max2 = compile_expr(expr->rexpr, chunk, scope, t, temp + 1);

            add_instruction(chunk, CREATE(OP_EQ, dest, dest, t));

            return MAX(max1, max2);
        }

        case TYPE_NEQ:
        {
            int max1 = compile_expr(expr->lexpr, chunk, scope, dest, temp);

            int t = get_temp_index(scope, temp);
            int max2 = compile_expr(expr->rexpr, chunk, scope, t, temp + 1);

            add_instruction(chunk, CREATE(OP_EQ, dest, dest, t));
            add_instruction(chunk, CREATE(OP_NOT, dest, dest, 0));

            return MAX(max1, max2);
        }

        case TYPE_LT:
        {
            int max1 = compile_expr(expr->lexpr, chunk, scope, dest, temp);

            int t = get_temp_index(scope, temp);
            int max2 = compile_expr(expr->rexpr, chunk, scope, t, temp + 1);

            add_instruction(chunk, CREATE(OP_LT, dest, dest, t));

            return MAX(max1, max2);
        }

        case TYPE_LEQ:
        {
            int max1 = compile_expr(expr->lexpr, chunk, scope, dest, temp);

            int t = get_temp_index(scope, temp);
            int max2 = compile_expr(expr->rexpr, chunk, scope, t, temp + 1);

            add_instruction(chunk, CREATE(OP_LE, dest, dest, t));

            return MAX(max1, max2);
        }

        case TYPE_GT:
        {
            int max1 = compile_expr(expr->lexpr, chunk, scope, dest, temp);

            int t = get_temp_index(scope, temp);
            int max2 = compile_expr(expr->rexpr, chunk, scope, t, temp + 1);

            add_instruction(chunk, CREATE(OP_LT, dest, t, dest));

            return MAX(max1, max2);
        }

        case TYPE_GEQ:
        {
            int max1 = compile_expr(expr->lexpr, chunk, scope, dest, temp);

            int t = get_temp_index(scope, temp);
            int max2 = compile_expr(expr->rexpr, chunk, scope, t, temp + 1);

            add_instruction(chunk, CREATE(OP_LE, dest, t, dest));

            return MAX(max1, max2);
        }

        case TYPE_AND:
        {
            int tindex = add_bool(chunk, 1);
            int findex = add_bool(chunk, 0);

            //      lexpr
            // [t1] if false [t3 - t1]
            //      rexpr
            // [t2] if false [t3 - t2]
            //      load true
            // [t3] load false

            // lexpr
            int max1 = compile_expr(expr->lexpr, chunk, scope, dest, temp);

            // dummy jump
            int t1 = add_instruction(chunk, 0);

            // rexpr
            int max2 = compile_expr(expr->rexpr, chunk, scope, dest, temp);

            // dummy jump
            int t2 = add_instruction(chunk, 0);

            // load true
            add_instruction(chunk, CREATE(OP_MOVE, dest, tindex + 256, 0));

            // load false
            int t3 = add_instruction(chunk, CREATE(OP_MOVE, dest, findex + 256, 0));

            // fill in 1st jump
            chunk->instructions[t1] = CREATE(OP_JUMP_FALSE, dest, t3 - t1 - 1, 0);

            // fill in 2nd jump
            chunk->instructions[t2] = CREATE(OP_JUMP_FALSE, dest, t3 - t2 - 1, 0);

            return MAX(max1, max2);
        }

        case TYPE_OR:
        {
            int tindex = add_bool(chunk, 1);
            int findex = add_bool(chunk, 0);

            //      lexpr
            // [t1] if true [t3 - t1]
            //      rexpr
            // [t2] if true [t3 - t2]
            //      load false
            // [t3] load true

            // lexpr
            int max1 = compile_expr(expr->lexpr, chunk, scope, dest, temp);

            // dummy jump
            int t1 = add_instruction(chunk, 0);

            // rexpr
            int max2 = compile_expr(expr->rexpr, chunk, scope, dest, temp);

            // dummy jump
            int t2 = add_instruction(chunk, 0);

            // load false
            add_instruction(chunk, CREATE(OP_MOVE, dest, findex + 256, 0));

            // load true
            int t3 = add_instruction(chunk, CREATE(OP_MOVE, dest, tindex + 256, 0));

            // fill in 1st jump
            chunk->instructions[t1] = CREATE(OP_JUMP_FALSE, dest, t3 - t1 - 1, 0);

            // fill in 2nd jump
            chunk->instructions[t2] = CREATE(OP_JUMP_FALSE, dest, t3 - t2 - 1, 0);

            return MAX(max1, max2);
        }

        /* unary cases */
        case TYPE_NEG:
        {
            int max = compile_expr(expr->lexpr, chunk, scope, dest, temp);
            add_instruction(chunk, CREATE(OP_NEG, dest, dest, 0));
            return max;
        }

        case TYPE_NOT:
        {
            int max = compile_expr(expr->lexpr, chunk, scope, dest, temp);
            add_instruction(chunk, CREATE(OP_NOT, dest, dest, 0));
            return max;
        }

        /* constants */
        case TYPE_INT:
        {
            int index = add_int(chunk, expr->value.i);
            add_instruction(chunk, CREATE(OP_MOVE, dest, index + 256, 0));
            return temp;
        }

        case TYPE_REAL:
        {
            int index = add_real(chunk, expr->value.d);
            add_instruction(chunk, CREATE(OP_MOVE, dest, index + 256, 0));
            return temp;
        }

        case TYPE_BOOL:
        {
            int index = add_bool(chunk, expr->value.i);
            add_instruction(chunk, CREATE(OP_MOVE, dest, index + 256, 0));
            return temp;
        }

        case TYPE_NULL:
        {
            int index = add_null(chunk);
            add_instruction(chunk, CREATE(OP_MOVE, dest, index + 256, 0));
            return temp;
        }

        case TYPE_STRING:
        {
            int index = add_string(chunk, expr->value.s);
            add_instruction(chunk, CREATE(OP_MOVE, dest, index + 256, 0));
            return temp;
        }

        case TYPE_BLOCK:
        {
            // [t1]     enter try [t2 - t1 - 1]
            //          protected block
            //          leave try
            // [t2]     jump [nm]
            // [t2 + 1] handler
            // [nm]

            if (expr->rlist) {
                // dummy enter try
                int t1 = add_instruction(chunk, 0);

                // protected block
                int max1 = compile_list(expr->llist, chunk, scope, dest, temp);

                // leave try instruction
                add_instruction(chunk, CREATE(OP_LEAVE_TRY, 0, 0, 0));

                // dummy jump instruction
                int t2 = add_instruction(chunk, 0);

                // handler block
                int max2 = compile_list(expr->rlist, chunk, scope, dest, temp);

                int nm = chunk->numinstructions;

                // fill in 1st target
                chunk->instructions[t1] = CREATE(OP_ENTER_TRY, 0, t2 - t1 + 1, 0);

                // fill in 2nd target
                chunk->instructions[t2] = CREATE(OP_JUMP, 0, nm - t2 - 1, 0);

                return MAX(max1, max2);
            } else {
                return compile_list(expr->llist, chunk, scope, dest, temp);
            }
        }

        case TYPE_THROW:
        {
            int max = compile_expr(expr->lexpr, chunk, scope, dest, temp);
            add_instruction(chunk, CREATE(OP_THROW, dest, 0, 0));
            return max;
        }
    }

    fatal("Could not generate code.");
    return 0;
}

int compile_list(ExpressionList *list, Chunk *chunk, Scope *scope, int dest, int temp) {
    int max = temp;

    ExpressionNode *head;
    for (head = list->head; head != NULL; head = head->next) {
        int n = compile_expr(head->expr, chunk, scope, dest, temp);
        if (n > max) {
            max = n;
        }
    }

    return max;
}

Chunk *compile(Expression *expr) {
    Chunk *chunk = make_chunk();
    int max = compile_expr(expr, chunk, expr->scope, 0, 0);

    chunk->numtemps = max;
    chunk->numlocals = expr->scope->numlocals;
    chunk->numupvars = expr->scope->numupvars;
    chunk->numparams = expr->scope->numparams;
    return chunk;
}
