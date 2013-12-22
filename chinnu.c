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
#include <stdarg.h>
#include <string.h>

#include "chinnu.h"
#include "symbol.h"

extern FILE *yyin;
extern int yyparse();
extern void yylex_destroy();

/* for debugging */

/* forward */
void expression_list_print(ExpressionList *list, int indent);

void expression_print(Expression *expr, int indent) {
    if (expr) {
        int i;
        for (i = 0; i < indent; i++) {
            printf("\t");
        }

        switch (expr->type) {
            case TYPE_VARREF:
                printf("[ref %d]\n", expr->symbol->id);
                break;

            case TYPE_DECLARATION:
                printf("[decl %d]\n", expr->symbol->id);
                break;

            case TYPE_NUMBER:
                printf("[%.2f or %d]\n", expr->value->d, expr->value->i);
                break;

            case TYPE_STRING:
                printf("[\"%s\"]\n", expr->value->s);
                break;

            default:
                printf("[type %d]\n", expr->type);
        }

        if (expr->cond) expression_print(expr->cond, indent + 1);
        if (expr->lexpr) expression_print(expr->lexpr, indent + 1);
        if (expr->rexpr) expression_print(expr->rexpr, indent + 1);
        if (expr->llist) expression_list_print(expr->llist, indent + 1);
        if (expr->rlist) expression_list_print(expr->rlist, indent + 1);
    }
}

void expression_list_print(ExpressionList *list, int indent) {
    if (list) {
        ExpressionNode *head = list->head;

        while (head) {
            expression_print(head->expr, indent);
            head = head->next;
        }
    }
}

void fatal(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "Internal compiler error: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    va_end(args);
    exit(1);
}

void error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    va_end(args);
}

int main(int argc, const char **argv) {
    if (argc != 2) {
        printf("chinnu [script]\n");
        return 1;
    }


    FILE *f = fopen(argv[1], "r");

    if (!f) {
        fatal("Could not open file.");
    }

    yyin = f;
    yyparse();
    yylex_destroy();
    fclose(f);

    resolve(program);
    expression_list_print(program, 0);
    free_expression_list(program);

    return 0;
}
