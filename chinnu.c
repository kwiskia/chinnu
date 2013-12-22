#include <stdio.h>
#include <stdlib.h>
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

int main(int argc, const char **argv) {
    FILE *f = fopen("test.ch", "r");

    if (!f) {
        fprintf(stderr, "Could not open file.");
    }

    yyin = f;
    yyparse();
    yylex_destroy();

    resolve(program);

    expression_list_print(program, 0);
    expression_list_free(program);
    fclose(f);

    return 0;
}
