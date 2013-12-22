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

enum {
    TYPE_IF, // cond, llist, rlist
    TYPE_WHILE, // cond, llist
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
    TYPE_NUMBER, // value (i or d)
    TYPE_STRING, // value (s)
    TYPE_CALL, // lexpr, rlist
    TYPE_FUNC, // llist, rlist
    TYPE_DECLARATION // rexpr, value (s)
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
    ExpressionList *rlist;

    Val *value;
    Symbol *symbol;
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

void free_expression(Expression *expr);
void free_expression_list(ExpressionList *list);

ExpressionList *make_list();
ExpressionList *list1(Expression *expr);
ExpressionList *expression_list_append(ExpressionList *list, Expression *expr);

Expression *make_if(Expression *cond, ExpressionList *body, ExpressionList *orelse);
Expression *make_while(Expression *cond, ExpressionList *body);
Expression *make_binop(int type, Expression *left, Expression *right);
Expression *make_uop(int type, Expression *left);
Expression *make_declaration(char *name, Expression *value, int immutable);
Expression *make_assignment(Expression *varref, Expression *value);
Expression *make_varref(char *name);
Expression *make_int(int i);
Expression *make_real(double d);
Expression *make_str(char *str);
Expression *make_call(Expression *target, ExpressionList *arguments);
Expression *make_func(ExpressionList *parameters, ExpressionList *body);
