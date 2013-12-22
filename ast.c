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

#include "ast.h"
#include "chinnu.h"

Expression *allocexpr() {
    Expression *expr = malloc(sizeof(Expression));

    if (!expr) {
        fatal("Out of memory.");
    }

    expr->cond = (Expression *) 0;
    expr->lexpr = (Expression *) 0;
    expr->rexpr = (Expression *) 0;
    expr->llist = (ExpressionList *) 0;
    expr->rlist = (ExpressionList *) 0;
    expr->value = (Val *) 0;
    expr->symbol = (Symbol *) 0;
    expr->immutable = 0;

    return expr;
}

Val *allocval() {
    Val *val = malloc(sizeof(Val));

    if (!val) {
        fatal("Out of memory.");
    }

    return val;
}

void free_expression(Expression *expr) {
    if (expr) {
        if (expr->cond)  free_expression(expr->cond);
        if (expr->lexpr) free_expression(expr->lexpr);
        if (expr->rexpr) free_expression(expr->rexpr);
        if (expr->llist) free_expression_list(expr->llist);
        if (expr->rlist) free_expression_list(expr->rlist);

        if (expr->value) {
            if (expr->type == TYPE_VARREF || expr->type == TYPE_DECLARATION || expr->type == TYPE_STRING) {
                free(expr->value->s);
            }

            free(expr->value);
        }

        if (expr->type == TYPE_DECLARATION) {
            free(expr->symbol->name);
            free(expr->symbol);
        }

        free(expr);
    }
}

void free_expression_node(ExpressionNode *item) {
    if (item) {
        free_expression(item->expr);
        free_expression_node(item->next);
        free(item);
    }
}

void free_expression_list(ExpressionList *list) {
    if (list) {
        free_expression_node(list->head);
        free(list);
    }
}

ExpressionList *make_list() {
    ExpressionList *list = malloc(sizeof(ExpressionList));

    if (!list) {
        fatal("Out of memory.");
    }

    list->head = (ExpressionNode *) 0;
    list->tail = (ExpressionNode *) 0;
    return list;
}

ExpressionList *list1(Expression *expr) {
    ExpressionList *list = make_list();
    expression_list_append(list, expr);
    return list;
}

ExpressionList *expression_list_append(ExpressionList *list, Expression *expr) {
    ExpressionNode *node = malloc(sizeof(ExpressionNode));

    if (!node) {
        fatal("Out of memory.");
    }

    node->expr = expr;
    node->next = (ExpressionNode *) 0;

    if (!list->head) {
        list->head = node;
        list->tail = node;
        node->prev = (ExpressionNode *) 0;
    } else {
        node->prev = list->tail;
        list->tail->next = node;
        list->tail = node;
    }

    return list;
}

Expression *make_if(Expression *cond, ExpressionList *body, ExpressionList *orelse) {
    Expression *expr = allocexpr();

    expr->type = TYPE_IF;
    expr->cond = cond;
    expr->llist = body;
    expr->rlist = orelse;
    return expr;
}

Expression *make_while(Expression *cond, ExpressionList *body) {
    Expression *expr = allocexpr();

    expr->type = TYPE_WHILE;
    expr->cond = cond;
    expr->llist = body;
    return expr;
}

Expression *make_binop(int type, Expression *left, Expression *right) {
    Expression *expr = allocexpr();

    expr->type = type;
    expr->lexpr = left;
    expr->rexpr = right;
    return expr;
}

Expression *make_uop(int type, Expression *left) {
    Expression *expr = allocexpr();

    expr->type = type;
    expr->lexpr = left;
    return expr;
}

Expression *make_declaration(char *name, Expression *value, int immutable) {
    Expression *expr = allocexpr();
    Val *val = allocval();

    // TODO - intern?

    expr->type = TYPE_DECLARATION;
    expr->rexpr = value;
    expr->value = val;
    val->s = name;
    expr->immutable = immutable;
    return expr;
}

Expression *make_assignment(Expression *left, Expression *right) {
    Expression *expr = allocexpr();

    expr->type = TYPE_ASSIGN;
    expr->lexpr = left;
    expr->rexpr = right;
    return expr;
}

Expression *make_varref(char *name) {
    Expression *expr = allocexpr();
    Val *val = allocval();

    // TODO - intern?

    expr->type = TYPE_VARREF;
    expr->value = val;
    val->s = name;
    return expr;
}

Expression *make_int(int i) {
    Expression *expr = allocexpr();
    Val *val = allocval();

    val->i = i;
    expr->type = TYPE_NUMBER;
    expr->value = val;
    return expr;
}

Expression *make_real(double d) {
    Expression *expr = allocexpr();
    Val *val = allocval();

    val->d = d;
    expr->type = TYPE_NUMBER;
    expr->value = val;
    return expr;
}

Expression *make_str(char *str) {
    Expression *expr = allocexpr();
    Val *val = allocval();

    expr->type = TYPE_STRING;
    expr->value = val;
    val->s = str;
    return expr;
}

Expression *make_call(Expression *target, ExpressionList *arguments) {
    Expression *expr = allocexpr();

    expr->type = TYPE_CALL;
    expr->lexpr = target;
    expr->rlist = arguments;
    return expr;
}

Expression *make_func(ExpressionList *parameters, ExpressionList *body) {
    Expression *expr = allocexpr();

    expr->type = TYPE_FUNC;
    expr->llist = parameters;
    expr->rlist = body;
    return expr;
}
