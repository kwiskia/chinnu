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

const char *const expression_type_names[] = {
    "If",
    "While",
    "Add",
    "Sub",
    "Mul",
    "Div",
    "Neg",
    "Not",
    "Assign",
    "Eqeq",
    "Neq",
    "Lt",
    "Leq",
    "Gt",
    "Geq",
    "And",
    "Or",
    "Varref",
    "Int",
    "Real",
    "Bool",
    "Null",
    "String",
    "Call",
    "Func",
    "Declaration",
    "Block"
};

Expression *allocexpr() {
    Expression *expr = malloc(sizeof(Expression));

    if (!expr) {
        fatal("Out of memory.");
    }

    expr->cond = NULL;
    expr->lexpr = NULL;
    expr->rexpr = NULL;
    expr->llist = NULL;
    expr->value = NULL;
    expr->symbol = NULL;
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

void free_expr(Expression *expr) {
    if (expr) {
        if (expr->cond)  free_expr(expr->cond);
        if (expr->lexpr) free_expr(expr->lexpr);
        if (expr->rexpr) free_expr(expr->rexpr);
        if (expr->llist) free_list(expr->llist);

        if (expr->value) {
            if (expr->type == TYPE_VARREF || expr->type == TYPE_DECLARATION || expr->type == TYPE_STRING || expr->type == TYPE_FUNC) {
                free(expr->value->s);
            }

            free(expr->value);
        }

        if (expr->type == TYPE_DECLARATION || (expr->type == TYPE_FUNC && expr->value)) {
            free(expr->symbol->name);
            free(expr->symbol);
        }

        free(expr);
    }
}

void free_node(ExpressionNode *item) {
    if (item) {
        free_expr(item->expr);
        free_node(item->next);
        free(item);
    }
}

void free_list(ExpressionList *list) {
    if (list) {
        free_node(list->head);
        free(list);
    }
}

ExpressionList *make_list() {
    ExpressionList *list = malloc(sizeof(ExpressionList));

    if (!list) {
        fatal("Out of memory.");
    }

    list->head = NULL;
    list->tail = NULL;
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
    node->next = NULL;

    if (!list->head) {
        list->head = node;
        list->tail = node;
        node->prev = NULL;
    } else {
        node->prev = list->tail;
        list->tail->next = node;
        list->tail = node;
    }

    return list;
}

Expression *make_if(SourcePos pos, Expression *cond, Expression *body, Expression *orelse) {
    Expression *expr = allocexpr();

    expr->type = TYPE_IF;
    expr->pos = pos;
    expr->cond = cond;
    expr->lexpr = body;
    expr->rexpr = orelse;
    return expr;
}

Expression *make_while(SourcePos pos, Expression *cond, Expression *body) {
    Expression *expr = allocexpr();

    expr->type = TYPE_WHILE;
    expr->pos = pos;
    expr->cond = cond;
    expr->lexpr = body;
    return expr;
}

Expression *make_binop(SourcePos pos, int type, Expression *left, Expression *right) {
    Expression *expr = allocexpr();

    expr->type = type;
    expr->pos = pos;
    expr->lexpr = left;
    expr->rexpr = right;
    return expr;
}

Expression *make_uop(SourcePos pos, int type, Expression *left) {
    Expression *expr = allocexpr();

    expr->type = type;
    expr->pos = pos;
    expr->lexpr = left;
    return expr;
}

Expression *make_declaration(SourcePos pos, char *name, Expression *value, int immutable) {
    Expression *expr = allocexpr();
    Val *val = allocval();

    // TODO - intern?

    expr->type = TYPE_DECLARATION;
    expr->pos = pos;
    expr->rexpr = value;
    expr->value = val;
    val->s = name;
    expr->immutable = immutable;
    return expr;
}

Expression *make_assignment(SourcePos pos, Expression *left, Expression *right) {
    Expression *expr = allocexpr();

    expr->type = TYPE_ASSIGN;
    expr->pos = pos;
    expr->lexpr = left;
    expr->rexpr = right;
    return expr;
}

Expression *make_varref(SourcePos pos, char *name) {
    Expression *expr = allocexpr();
    Val *val = allocval();

    // TODO - intern?

    expr->type = TYPE_VARREF;
    expr->pos = pos;
    expr->value = val;
    val->s = name;
    return expr;
}

Expression *make_int(SourcePos pos, int i) {
    Expression *expr = allocexpr();
    Val *val = allocval();

    expr->type = TYPE_INT;
    expr->pos = pos;
    expr->value = val;
    val->i = i;
    return expr;
}

Expression *make_real(SourcePos pos, double d) {
    Expression *expr = allocexpr();
    Val *val = allocval();

    expr->type = TYPE_REAL;
    expr->pos = pos;
    expr->value = val;
    val->d = d;
    return expr;
}

Expression *make_bool(SourcePos pos, int i) {
    Expression *expr = allocexpr();
    Val *val = allocval();

    expr->type = TYPE_BOOL;
    expr->pos = pos;
    expr->value = val;
    val->i = i;

    return expr;
}

Expression *make_null(SourcePos pos) {
    Expression *expr = allocexpr();

    expr->type = TYPE_NULL;
    expr->pos = pos;

    return expr;
}

Expression *make_str(SourcePos pos, char *str) {
    Expression *expr = allocexpr();
    Val *val = allocval();

    expr->type = TYPE_STRING;
    expr->pos = pos;
    expr->value = val;
    val->s = str;
    return expr;
}

Expression *make_call(SourcePos pos, Expression *target, ExpressionList *arguments) {
    Expression *expr = allocexpr();

    expr->type = TYPE_CALL;
    expr->pos = pos;
    expr->lexpr = target;
    expr->llist = arguments;
    return expr;
}

Expression *make_func(SourcePos pos, char *name, ExpressionList *parameters, Expression *body) {
    Expression *expr = allocexpr();
    Val *val = allocval();

    expr->type = TYPE_FUNC;
    expr->pos = pos;
    expr->llist = parameters;
    expr->rexpr = body;
    expr->value = val;
    val->s = name;
    return expr;
}

Expression *make_block(SourcePos pos, ExpressionList *block) {
    Expression *expr = allocexpr();

    expr->type = TYPE_BLOCK;
    expr->pos = pos;
    expr->llist = block;

    return expr;
}
