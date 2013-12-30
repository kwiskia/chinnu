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

#pragma once

#include "common.h"
#include "symbol.h"

typedef enum {
    TYPE_IF, // cond, lexpr, rexpr
    TYPE_WHILE, // cond, lexpr
    TYPE_ADD, // lexpr, rexpr
    TYPE_SUB,
    TYPE_MUL,
    TYPE_DIV,
    TYPE_NEG, // lexpr
    TYPE_NOT,
    TYPE_ASSIGN,
    TYPE_EQEQ,
    TYPE_NEQ,
    TYPE_LT,
    TYPE_LEQ,
    TYPE_GT,
    TYPE_GEQ,
    TYPE_AND,
    TYPE_OR,
    TYPE_VARREF, // value (s)
    TYPE_INT, // value (i)
    TYPE_REAL, // value (d)
    TYPE_BOOL, // value (i)
    TYPE_NULL,
    TYPE_STRING, // value (s)
    TYPE_CALL, // lexpr, llist
    TYPE_FUNC, // llist, rexpr
    TYPE_DECLARATION, // rexpr, value (s),
    TYPE_BLOCK, // llist
    TYPE_MODULE, // lexpr

    NUM_EXPRESSION_TYPES
} ExpressionType;

typedef struct SourcePos SourcePos;

struct SourcePos {
    int first_line;
    int first_column;
    int last_line;
    int last_column;
    char *filename;
};

struct Val {
    union {
        int i;
        double d;
        char *s;
    };
};

struct Expression {
    unsigned int type      : 5;
    unsigned int immutable : 1;

    Expression *cond;
    Expression *lexpr;
    Expression *rexpr;
    ExpressionList *llist;

    Val *value;
    Symbol *symbol;
    FunctionDesc *desc;
    SourcePos pos;
};

struct ExpressionNode {
    Expression *expr;
    ExpressionNode *next;
    ExpressionNode *prev;
};

struct ExpressionList {
    ExpressionNode *head;
    ExpressionNode *tail;
};

const char *const expression_type_names[NUM_EXPRESSION_TYPES];

void free_expr(Expression *expr);
void free_list(ExpressionList *list);

ExpressionList *make_list();
ExpressionList *list1(Expression *expr);
ExpressionList *expression_list_append(ExpressionList *list, Expression *expr);

Expression *make_if(SourcePos pos, Expression *cond, Expression *body, Expression *orelse);
Expression *make_while(SourcePos pos, Expression *cond, Expression *body);
Expression *make_binop(SourcePos pos, int type, Expression *left, Expression *right);
Expression *make_uop(SourcePos pos, int type, Expression *left);
Expression *make_declaration(SourcePos pos, char *name, Expression *value, int immutable);
Expression *make_assignment(SourcePos pos, Expression *varref, Expression *value);
Expression *make_varref(SourcePos pos, char *name);
Expression *make_int(SourcePos pos, int i);
Expression *make_real(SourcePos pos, double d);
Expression *make_bool(SourcePos pos, int i);
Expression *make_null(SourcePos pos);
Expression *make_str(SourcePos pos, char *str);
Expression *make_call(SourcePos pos, Expression *target, ExpressionList *arguments);
Expression *make_func(SourcePos pos, char *name, ExpressionList *parameters, Expression *body);
Expression *make_block(SourcePos pos, ExpressionList *block);
Expression *make_module(SourcePos pos, Expression *block);
