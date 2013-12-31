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

#include "chinnu.h"
#include "compile.h"
#include "bytecode.h"

Constant *make_constant(int type, Val *value) {
    Constant *constant = malloc(sizeof(Constant));

    if (!constant) {
        fatal("Out of memory.");
    }

    constant->type = type;
    constant->value = value;
    return constant;
}

Chunk *make_chunk(Scope *scope) {
    Chunk *chunk = malloc(sizeof(Chunk));
    Constant **constants = malloc(sizeof(Constant *) * 4);
    int *instructions = malloc(sizeof(int) * 32);

    if (!chunk || !constants || !instructions) {
        fatal("Out of memory.");
    }

    chunk->scope = scope;
    chunk->constants = constants;
    chunk->instructions = instructions;

    chunk->numtemps = 0;
    chunk->numconstants = 0;
    chunk->maxconstants = 4;
    chunk->numinstructions = 0;
    chunk->maxinstructions = 32;

    return chunk;
}

int add_constant(Chunk *chunk, Constant *constant) {
    // TODO - return index of duplicate constant

    if (chunk->numconstants == chunk->maxconstants) {
        chunk->maxconstants *= 2;

        Constant **resize = realloc(chunk->constants, sizeof(Constant *) * chunk->maxconstants);

        if (!resize) {
            fatal("Out of memory.");
        }

        chunk->constants = resize;
    }

    chunk->constants[chunk->numconstants] = constant;
    return chunk->numconstants++;
}

int add_instruction(Chunk *chunk, int instruction) {
    if (chunk->numinstructions == chunk->maxinstructions) {
        chunk->maxinstructions *= 2;

        int *resize = realloc(chunk->instructions, sizeof(int) * chunk->maxinstructions);

        if (!resize) {
            fatal("Out of memory.");
        }

        chunk->instructions = resize;
    }

    chunk->instructions[chunk->numinstructions] = instruction;
    return chunk->numinstructions++;
}

int get_local_index(Chunk *chunk, Symbol *symbol) {
    int i;
    for (i = 0; i < chunk->scope->numlocals; i++) {
        if (chunk->scope->locals[i]->symbol == symbol) {
            return i + 1;
        }
    }

    return -1;
}

static int temp = 0;
static int maxt = 0;

void _mktemp() {
    temp++;

    if (temp > maxt) {
        maxt = temp;
    }
}

void _rmtemp() {
    temp--;
}

int get_temp_index(Chunk *chunk) {
    return chunk->scope->numlocals + temp + 1;
}

/* forward */
void compile_list(ExpressionList *list, Chunk *chunk, int dest);

void compile_expr(Expression *expr, Chunk *chunk, int dest) {
    switch (expr->type) {
        case TYPE_MODULE:
            compile_expr(expr->lexpr, chunk, dest);
            add_instruction(chunk, CREATE(OP_RETURN, 0, 0, 0));
            break;

        case TYPE_DECLARATION:
            if (expr->rexpr) {
                int index = get_local_index(chunk, expr->symbol);
                compile_expr(expr->rexpr, chunk, index);
                // TODO - peephole optimize
                add_instruction(chunk, CREATE(OP_MOVE, dest, index, 0));
            }
            break;

        case TYPE_FUNC:
            fatal("Func Unimplemented.\n");
            break;

        case TYPE_CALL:
            fatal("Call Unimplemented.\n");
            break;

        case TYPE_VARREF:
            // TODO - upvars
            add_instruction(chunk, CREATE(OP_MOVE, dest, get_local_index(chunk, expr->symbol), 0));
            break;

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
            compile_expr(expr->cond, chunk, dest);

            // dummy jump 1
            int t1 = add_instruction(chunk, 0);

            // true branch
            compile_expr(expr->lexpr, chunk, dest);

            // dummy jump 2
            int t2 = add_instruction(chunk, 0);

            if (expr->rexpr) {
                compile_expr(expr->rexpr, chunk, dest);
            } else {
                int index = add_constant(chunk, make_constant(CONST_NULL, expr->value));
                add_instruction(chunk, CREATE(OP_MOVE, dest, index + 256, 0));
            }

            int nm = chunk->numinstructions;

            // fill in 1st jump
            chunk->instructions[t1] = CREATE(OP_JUMP_FALSE, dest, t2 - t1, 0);

            // fill in 2nd jump
            chunk->instructions[t2] = CREATE(OP_JUMP, 0, nm - t2 - 1, 0);
        } break;

        case TYPE_WHILE:
        {
            // [t0] cond
            // [t1] jump if false [t3 - t1]
            //      body
            // [t2] jump [t2 - t0]
            // [t3] null

            int t0 = chunk->numinstructions;
            compile_expr(expr->cond, chunk, dest);

            // dummy jump 1
            int t1 = add_instruction(chunk, 0);

            // compile body
            compile_expr(expr->lexpr, chunk, dest);

            // dummy jump 2
            int t2 = add_instruction(chunk, 0);

            // return null
            int index = add_constant(chunk, make_constant(CONST_NULL, expr->value));
            int t3 = add_instruction(chunk, CREATE(OP_MOVE, dest, index + 256, 0));

            // fill in 1st jump
            chunk->instructions[t1] = CREATE(OP_JUMP_FALSE, dest, t3 - t1 - 1, 0);

            // fill in 2nd jump
            chunk->instructions[t2] = CREATE(OP_JUMP, 0, t3 - t0, 1);
        } break;

        /* binary cases */
        case TYPE_ASSIGN:
        {
            // TODO - upvars
            int index = get_local_index(chunk, expr->lexpr->symbol);
            compile_expr(expr->rexpr, chunk, index);
            // TODO - peephole optimize
            add_instruction(chunk, CREATE(OP_MOVE, dest, index, 0));
        } break;

        case TYPE_ADD:
        {
            compile_expr(expr->lexpr, chunk, dest);

            int t = get_temp_index(chunk);
            _mktemp();
            compile_expr(expr->rexpr, chunk, t);
            _rmtemp();

            // TODO - binops with constants without load
            add_instruction(chunk, CREATE(OP_ADD, dest, dest, t));
        } break;

        case TYPE_SUB:
        {
            compile_expr(expr->lexpr, chunk, dest);

            int t = get_temp_index(chunk);
            _mktemp();
            compile_expr(expr->rexpr, chunk, t);
            _rmtemp();

            // TODO - binops with constants without load
            add_instruction(chunk, CREATE(OP_SUB, dest, dest, t));
        } break;

        case TYPE_MUL:
        {
            compile_expr(expr->lexpr, chunk, dest);

            int t = get_temp_index(chunk);
            _mktemp();
            compile_expr(expr->rexpr, chunk, t);
            _rmtemp();

            // TODO - binops with constants without load
            add_instruction(chunk, CREATE(OP_MUL, dest, dest, t));
        } break;

        case TYPE_DIV:
        {
            compile_expr(expr->lexpr, chunk, dest);

            int t = get_temp_index(chunk);
            _mktemp();
            compile_expr(expr->rexpr, chunk, t);
            _rmtemp();

            // TODO - binops with constants without load
            add_instruction(chunk, CREATE(OP_DIV, dest, dest, t));
        } break;

        /**
         * optimization - boolean-resulting commands should have a second
         * form that takes branch statements to jump to on true/false instead
         * of actually generating that value (for control flow).
         */

        case TYPE_EQEQ:
        {
            compile_expr(expr->lexpr, chunk, dest);

            int t = get_temp_index(chunk);
            _mktemp();
            compile_expr(expr->rexpr, chunk, t);
            _rmtemp();

            // TODO - binops with constants without load
            add_instruction(chunk, CREATE(OP_EQ, dest, dest, t));
        } break;

        case TYPE_NEQ:
        {
            compile_expr(expr->lexpr, chunk, dest);

            int t = get_temp_index(chunk);
            _mktemp();
            compile_expr(expr->rexpr, chunk, t);
            _rmtemp();

            // TODO - binops with constants without load
            add_instruction(chunk, CREATE(OP_EQ, dest, dest, t));
            add_instruction(chunk, CREATE(OP_NOT, dest, dest, 0));
        } break;

        case TYPE_LT:
        {
            compile_expr(expr->lexpr, chunk, dest);

            int t = get_temp_index(chunk);
            _mktemp();
            compile_expr(expr->rexpr, chunk, t);
            _rmtemp();

            // TODO - binops with constants without load
            add_instruction(chunk, CREATE(OP_LT, dest, dest, t));
        } break;

        case TYPE_LEQ:
        {
            compile_expr(expr->lexpr, chunk, dest);

            int t = get_temp_index(chunk);
            _mktemp();
            compile_expr(expr->rexpr, chunk, t);
            _rmtemp();

            // TODO - binops with constants without load
            add_instruction(chunk, CREATE(OP_LE, dest, dest, t));
        } break;

        case TYPE_GT:
        {
            compile_expr(expr->lexpr, chunk, dest);

            int t = get_temp_index(chunk);
            _mktemp();
            compile_expr(expr->rexpr, chunk, t);
            _rmtemp();

            // TODO - binops with constants without load
            add_instruction(chunk, CREATE(OP_LT, dest, t, dest));
        } break;

        case TYPE_GEQ:
        {
            compile_expr(expr->lexpr, chunk, dest);

            int t = get_temp_index(chunk);
            _mktemp();
            compile_expr(expr->rexpr, chunk, t);
            _rmtemp();

            // TODO - binops with constants without load
            add_instruction(chunk, CREATE(OP_LE, dest, t, dest));
        } break;

        case TYPE_AND:
        {
            // TODO - constant management

            Val *tvalue = malloc(sizeof(Val));
            Val *fvalue = malloc(sizeof(Val));

            if (!tvalue || !fvalue) {
                fatal("Out of memory.");
            }

            tvalue->i = 1;
            fvalue->i = 0;

            int tindex = add_constant(chunk, make_constant(CONST_BOOL, tvalue));
            int findex = add_constant(chunk, make_constant(CONST_BOOL, fvalue));

            //      lexpr
            // [t1] if false [t3 - t1]
            //      rexpr
            // [t2] if false [t3 - t2]
            //      load true
            // [t3] load false

            // lexpr
            compile_expr(expr->lexpr, chunk, dest);

            // dummy jump
            int t1 = add_instruction(chunk, 0);

            // rexpr
            compile_expr(expr->rexpr, chunk, dest);

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
        } break;

        case TYPE_OR:
        {
            // TODO - constant management

            Val *tvalue = malloc(sizeof(Val));
            Val *fvalue = malloc(sizeof(Val));

            if (!tvalue || !fvalue) {
                fatal("Out of memory.");
            }

            tvalue->i = 1;
            fvalue->i = 0;

            int tindex = add_constant(chunk, make_constant(CONST_BOOL, tvalue));
            int findex = add_constant(chunk, make_constant(CONST_BOOL, fvalue));

            //      lexpr
            // [t1] if true [t3 - t1]
            //      rexpr
            // [t2] if true [t3 - t2]
            //      load false
            // [t3] load true

            // lexpr
            compile_expr(expr->lexpr, chunk, dest);

            // dummy jump
            int t1 = add_instruction(chunk, 0);

            // rexpr
            compile_expr(expr->rexpr, chunk, dest);

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
        } break;

        /* unary cases */
        case TYPE_NEG:
            compile_expr(expr->lexpr, chunk, dest);
            add_instruction(chunk, CREATE(OP_NEG, dest, dest, 0));
            break;

        case TYPE_NOT:
            compile_expr(expr->lexpr, chunk, dest);
            add_instruction(chunk, CREATE(OP_NOT, dest, dest, 0));
            break;

        /* constants */
        case TYPE_INT:
        {
            int index = add_constant(chunk, make_constant(CONST_INT, expr->value));
            add_instruction(chunk, CREATE(OP_MOVE, dest, index + 256, 0));
        } break;

        case TYPE_REAL:
        {
            int index = add_constant(chunk, make_constant(CONST_REAL, expr->value));
            add_instruction(chunk, CREATE(OP_MOVE, dest, index + 256, 0));
        } break;

        case TYPE_BOOL:
        {
            int index = add_constant(chunk, make_constant(CONST_BOOL, expr->value));
            add_instruction(chunk, CREATE(OP_MOVE, dest, index + 256, 0));
        } break;

        case TYPE_NULL:
        {
            int index = add_constant(chunk, make_constant(CONST_NULL, expr->value));
            add_instruction(chunk, CREATE(OP_MOVE, dest, index + 256, 0));
        } break;

        case TYPE_STRING:
        {
            int index = add_constant(chunk, make_constant(CONST_STRING, expr->value));
            add_instruction(chunk, CREATE(OP_MOVE, dest, index + 256, 0));
        } break;

        case TYPE_BLOCK:
            compile_list(expr->llist, chunk, dest);
            break;
    }
}

void compile_list(ExpressionList *list, Chunk *chunk, int dest) {
    ExpressionNode *head;
    for (head = list->head; head != NULL; head = head->next) {
        compile_expr(head->expr, chunk, dest);
    }
}

Chunk *compile(Expression *expr) {
    Chunk *chunk = make_chunk(expr->scope);
    compile_expr(expr, chunk, 0);

    chunk->numtemps = maxt;
    return chunk;
}
