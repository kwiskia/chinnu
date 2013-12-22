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
void print_list(NodeList *list, int indent);

void print_node(Node *node, int indent) {
    if (node) {
        int i;
        for (i = 0; i < indent; i++) {
            printf("\t");
        }

        switch (node->type) {
            case TYPE_VARREF:
                printf("[ref %d]\n", node->symbol->id);
                break;

            case TYPE_DECLARATION:
                printf("[decl %d]\n", node->symbol->id);
                break;

            case TYPE_NUMBER:
                printf("[%.2f or %d]\n", node->value->d, node->value->i);
                break;

            case TYPE_STRING:
                printf("[\"%s\"]\n", node->value->s);
                break;

            default:
                printf("[type %d]\n", node->type);
        }

        if (node->cond) print_node(node->cond, indent + 1);
        if (node->lnode) print_node(node->lnode, indent + 1);
        if (node->rnode) print_node(node->rnode, indent + 1);
        if (node->llist) print_list(node->llist, indent + 1);
        if (node->rlist) print_list(node->rlist, indent + 1);
    }
}

void print_list(NodeList *list, int indent) {
    if (list) {
        ListItem *item = list->head;

        while (item) {
            print_node(item->node, indent);
            item = item->next;
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

    print_list(program, 0);
    free_list(program);
    fclose(f);

    return 0;
}
