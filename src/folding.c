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

#include "ast.h"
#include "chinnu.h"
#include "folding.h"

int is_arith_const(Expression *expr) {
    return expr->type == TYPE_INT || expr->type == TYPE_REAL;
}

int is_logic_const(Expression *expr) {
    return expr->type == TYPE_BOOL;
}

int is_zero(Expression *expr) {
    if (expr->type == TYPE_INT) {
        return expr->value->i == 0;
    }

    return expr->value->d == 0;
}

double to_real(Expression *expr) {
    if (expr->type == TYPE_INT) {
        return (double) expr->value->i;
    }

    return expr->value->d;
}

Expression *fold_arith(Expression *expr) {
    if (expr->lexpr->type == TYPE_INT && expr->rexpr->type == TYPE_INT) {
        int a = expr->lexpr->value->i;
        int b = expr->rexpr->value->i;
        int c;

        switch (expr->type) {
            case TYPE_ADD: c = a + b; break;
            case TYPE_SUB: c = a - b; break;
            case TYPE_MUL: c = a * b; break;
            case TYPE_DIV: c = a / b; break;
        }

        return make_int(expr->pos, c);
    } else {
        double a = to_real(expr->lexpr);
        double b = to_real(expr->rexpr);
        double c;

        switch (expr->type) {
            case TYPE_ADD: c = a + b; break;
            case TYPE_SUB: c = a - b; break;
            case TYPE_MUL: c = a * b; break;
            case TYPE_DIV: c = a / b; break;
        }

        return make_real(expr->pos, c);
    }
}

/* forward */
ExpressionList *fold_list(ExpressionList *list);

Expression *fold_expr(Expression *expr) {
    switch (expr->type) {
        case TYPE_DECLARATION:
            if (expr->rexpr) {
                expr->rexpr = fold_expr(expr->rexpr);
            }
            break;

        case TYPE_FUNC:
            expr->llist = fold_list(expr->llist);
            expr->rexpr = fold_expr(expr->rexpr);
            break;

        case TYPE_VARREF:
            break;

        /* control flow */
        case TYPE_IF:
            expr->cond = fold_expr(expr->cond);
            expr->lexpr = fold_expr(expr->lexpr);

            if (expr->rexpr) {
                expr->rexpr = fold_expr(expr->rexpr);
            }

            // TODO - clean up logic
            // TODO - find a nicer way to free unused portion
            // TODO - add warning for unreachable code?

            if (is_logic_const(expr->cond)) {
                Expression *n;

                if (expr->cond->value->i) {
                    n = expr->lexpr;

                    if (expr->rexpr) {
                        if (warning_flags[WARNING_SHADOW]) {
                            warning(expr->rexpr->pos, "Unreachable code.");
                        }

                        free_expr(expr->rexpr);
                    }
                } else {
                    if (expr->rexpr) {
                        n = expr->rexpr;
                    } else {
                        n = make_null(expr->pos);
                    }

                    if (warning_flags[WARNING_SHADOW]) {
                        warning(expr->lexpr->pos, "Unreachable code.");
                    }

                    free_expr(expr->lexpr);
                }

                free_expr(expr->cond);
                free_expr_shallow(expr);
                return n;
            }
            break;

        case TYPE_WHILE:
            expr->cond = fold_expr(expr->cond);
            expr->lexpr = fold_expr(expr->lexpr);

            if (is_logic_const(expr->cond)) {
                Expression *n;

                if (expr->cond->value->i) {
                    n = expr->lexpr;
                } else {
                    if (warning_flags[WARNING_SHADOW]) {
                        warning(expr->lexpr->pos, "Unreachable code.");
                    }

                    n = make_null(expr->pos);
                    free_expr(expr->lexpr);
                }

                free_expr(expr->cond);
                free_expr_shallow(expr);
                return n;
            }
            break;

        case TYPE_CALL:
            expr->lexpr = fold_expr(expr->lexpr);
            expr->llist = fold_list(expr->llist);
            break;

        /* binary cases */
        case TYPE_ASSIGN:
            expr->lexpr = fold_expr(expr->lexpr);
            expr->rexpr = fold_expr(expr->rexpr);
            break;

        case TYPE_ADD:
        case TYPE_SUB:
        case TYPE_MUL:
        case TYPE_DIV:
            expr->lexpr = fold_expr(expr->lexpr);
            expr->rexpr = fold_expr(expr->rexpr);

            // protect against div by zero
            if (expr->type == TYPE_DIV && is_arith_const(expr->rexpr) && is_zero(expr->rexpr)) {
                break;
            }

            // operation can be folded
            if (is_arith_const(expr->lexpr) && is_arith_const(expr->rexpr)) {
                Expression *n = fold_arith(expr);

                if (n != expr) {
                    // does this belong somewhere else?
                    free_expr(expr);
                }

                return n;
            }

            // lexpr = 0
            if (is_arith_const(expr->lexpr) && is_zero(expr->lexpr)) {
                switch (expr->type) {
                    case TYPE_ADD:
                    {
                        // 0 + x = x
                        Expression *n = expr->rexpr;
                        free_expr(expr->lexpr);
                        free_expr_shallow(expr);
                        return n;
                    }

                    case TYPE_MUL:
                    case TYPE_DIV:
                    {
                        // 0 * x = 0
                        // 0 / x = 0
                        Expression *n;
                        if (expr->lexpr->type == TYPE_REAL || expr->rexpr->type == TYPE_REAL) {
                            n = make_real(expr->pos, 0);
                        } else {
                            n = make_int(expr->pos, 0);
                        }

                        free_expr(expr);
                        return n;
                    }
                }
            }

            // rexpr = 0
            if (is_arith_const(expr->rexpr) && is_zero(expr->rexpr)) {
                switch (expr->type) {
                    case TYPE_SUB:
                    case TYPE_ADD:
                    {
                        // x + 0 = x
                        // x - 0 = x
                        Expression *n = expr->lexpr;
                        free_expr(expr->rexpr);
                        free_expr_shallow(expr);
                        return n;
                    }

                    case TYPE_MUL:
                    {
                        // x * 0 = 0
                        Expression *n;
                        if (expr->lexpr->type == TYPE_REAL || expr->rexpr->type == TYPE_REAL) {
                            n = make_real(expr->pos, 0);
                        } else {
                            n = make_int(expr->pos, 0);
                        }

                        free_expr(expr);
                        return n;
                    }
                }
            }

            // lexpr = 1
            if (is_arith_const(expr->lexpr) && to_real(expr->lexpr) == 1) {
                switch (expr->type) {
                    case TYPE_MUL:
                    {
                        // 1 * x = x
                        Expression *n = expr->rexpr;
                        free_expr(expr->lexpr);
                        free_expr_shallow(expr);
                        return n;
                    }
                }
            }

            // rexpr = 1
            if (is_arith_const(expr->rexpr) && to_real(expr->rexpr) == 1) {
                switch (expr->type) {
                    case TYPE_MUL:
                    case TYPE_DIV:
                    {
                        // x * 1 = x
                        // x / 1 = x
                        Expression *n = expr->lexpr;
                        free_expr(expr->rexpr);
                        free_expr_shallow(expr);
                        return n;
                    }
                }
            }
            break;

        case TYPE_EQEQ:
        case TYPE_NEQ:
        case TYPE_LT:
        case TYPE_LEQ:
        case TYPE_GT:
        case TYPE_GEQ:
            expr->lexpr = fold_expr(expr->lexpr);
            expr->rexpr = fold_expr(expr->rexpr);
            break;

        case TYPE_AND:
        case TYPE_OR:
            expr->lexpr = fold_expr(expr->lexpr);
            expr->rexpr = fold_expr(expr->rexpr);

            if (is_logic_const(expr->lexpr) && is_logic_const(expr->rexpr)) {
                int a = expr->lexpr->value->i;
                int b = expr->rexpr->value->i;
                int c;

                switch (expr->type) {
                    case TYPE_AND: c = a & b; break;
                    case TYPE_OR:  c = a | b; break;
                }

                Expression *n = make_bool(expr->pos, c);
                free_expr(expr);
                return n;
            }
            break;

        /* unary cases */
        case TYPE_NEG:
            expr->lexpr = fold_expr(expr->lexpr);

            if (is_arith_const(expr->lexpr)) {
                if (expr->lexpr->type == TYPE_INT) {
                    Expression *n = make_int(expr->pos, -expr->lexpr->value->i);
                    free_expr(expr);
                    return n;
                } else {
                    Expression *n = make_real(expr->pos, -expr->lexpr->value->d);
                    free_expr(expr);
                    return n;
                }
            }
            break;

        case TYPE_NOT:
            expr->lexpr = fold_expr(expr->lexpr);

            if (is_logic_const(expr->lexpr)) {
                Expression *n = make_bool(expr->pos, ~expr->lexpr->value->i);
                free_expr(expr);
                return n;
            }
            break;

        /* constants */
        case TYPE_INT:
        case TYPE_REAL:
        case TYPE_BOOL:
        case TYPE_NULL:
        case TYPE_STRING:
            /* ignore */
            break;

        case TYPE_BLOCK:
            expr->llist = fold_list(expr->llist);

            if (expr->llist->head == NULL) {
                Expression *n = make_null(expr->pos);
                free_expr(expr);
                return n;
            }
            break;
    }

    return expr;
}

ExpressionList *fold_list(ExpressionList *list) {
    ExpressionNode *head;
    for (head = list->head; head != NULL; head = head->next) {
        head->expr = fold(head->expr);
    }

    head = list->head;
    while (head != NULL) {
        if (head->expr->type == TYPE_NULL && head->next != NULL) {
            free_expr(head->expr);
            ExpressionNode *temp = head->next;
            head->expr = head->next->expr;
            head->next = head->next->next;
            free(temp);
        } else {
            head = head->next;
        }
    }

    return list;
}

Expression *fold(Expression *expr) {
    return fold_expr(expr);
}
