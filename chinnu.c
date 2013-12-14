#include <stdio.h>
#include <stdlib.h>

#include "chinnu.h"

extern FILE *yyin;
extern int yyerror(const char *);

struct node *allocnode() {
    struct node *node = malloc(sizeof(struct node));

    if (!node) {
        yyerror("Out of memory.");
        exit(1);
    }

    node->left = N;
    node->right = N;
    node->cond = N;
    node->body = NL;
    node->orelse = NL;
    node->value = (struct val *)0;

    return node;
}

struct nodelist *alloclist() {
    struct nodelist *list = malloc(sizeof(struct nodelist));

    if (!list) {
        yyerror("Out of memory.");
        exit(1);
    }

    return list;
}

struct val *allocval() {
    struct val *val = malloc(sizeof(struct val));

    if (!val) {
        yyerror("Out of memory.");
        exit(1);
    }

    return val;
}

void freenode(struct node *node) {
    if (node) {
        freenode(node->left);
        freenode(node->right);
        freenode(node->cond);
        freelist(node->body);
        freelist(node->orelse);

        if (node->value) {
            if (node->type == TYPE_VARREF || node->type == TYPE_STRING) {
                free(node->value->s);
            }

            free(node->value);
        }

        free(node);
    }
}

void freelist(struct nodelist *list) {
    if (list) {
        freenode(list->node);
        freelist(list->next);
        free(list);
    }
}

struct nodelist *makelist(struct node *node)  {
    struct nodelist *list = alloclist();

    list->node = node;
    list->next = NL;
    return list;
}

struct nodelist *append(struct nodelist *list, struct node *node) {
    struct nodelist *head = list;
    struct nodelist *rear = makelist(node);

    while (head->next) {
        head = head->next;
    }

    head->next = rear;
    return list;
}

struct node *makeif(struct node *cond, struct nodelist *body, struct nodelist *orelse) {
    struct node *node = allocnode();

    node->type = TYPE_IF;
    node->cond = cond;
    node->body = body;
    node->orelse = orelse;
    return node;
}

struct node *makewhile(struct node *cond, struct nodelist *body) {
    struct node *node = allocnode();

    node->type = TYPE_WHILE;
    node->cond = cond;
    node->body = body;
    return node;
}

struct node *makebinop(int type, struct node *left, struct node *right) {
    struct node *node = allocnode();

    node->type = type;
    node->left = left;
    node->right = right;
    return node;
}

struct node *makeuop(int type, struct node *left) {
    struct node *node = allocnode();

    node->type = type;
    node->left = left;
    return node;
}

struct node *makeassignment(struct node *left, struct node *right) {
    struct node *node = allocnode();

    node->type = TYPE_ASSIGN;
    node->left = left;
    node->right = right;
    return node;
}

struct node *makevarref(char *name) {
    struct node *node = allocnode();
    struct val *val = allocval();

    // TODO - intern?

    node->type = TYPE_VARREF;
    node->value = val;
    val->s = name;
    return node;
}

struct node *makenum(double d) {
    struct node *node = allocnode();
    struct val *val = allocval();

    val->d = d;
    node->type = TYPE_NUMBER;
    node->value = val;
    return node;
}

struct node *makestr(char *str) {
    struct node *node = allocnode();
    struct val *val = allocval();

    node->type = TYPE_STRING;
    node->value = val;
    val->s = str;
    return node;
}

int main(int argc, const char **argv) {
    FILE *f = fopen("test.ch", "r");

    if (!f) {
        fprintf(stderr, "Could not open file.");
    }

    buffer = malloc(1000 * sizeof(char));

    yyin = f;
    yyparse();

    freelist(program);
    free(buffer);

    return 0;
}
