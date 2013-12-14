#include <stdio.h>
#include <stdlib.h>

#include "chinnu.h"

extern FILE *yyin;
extern int yyerror(const char *);

Node *allocnode() {
    Node *node = malloc(sizeof(Node ));

    if (!node) {
        yyerror("Out of memory.");
        exit(1);
    }

    node->left = N;
    node->right = N;
    node->cond = N;
    node->body = NL;
    node->orelse = NL;
    node->value = (Val *)0;

    return node;
}

Nodelist *alloclist() {
    Nodelist *list = malloc(sizeof(Nodelist));

    if (!list) {
        yyerror("Out of memory.");
        exit(1);
    }

    return list;
}

Val *allocval() {
    Val *val = malloc(sizeof(Val));

    if (!val) {
        yyerror("Out of memory.");
        exit(1);
    }

    return val;
}

void freenode(Node *node) {
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

void freelist(Nodelist *list) {
    if (list) {
        freenode(list->node);
        freelist(list->next);
        free(list);
    }
}

Nodelist *makelist(Node *node)  {
    Nodelist *list = alloclist();

    list->node = node;
    list->next = NL;
    return list;
}

Nodelist *append(Nodelist *list, Node *node) {
    Nodelist *head = list;
    Nodelist *rear = makelist(node);

    while (head->next) {
        head = head->next;
    }

    head->next = rear;
    return list;
}

Node *makeif(Node *cond, Nodelist *body, Nodelist *orelse) {
    Node *node = allocnode();

    node->type = TYPE_IF;
    node->cond = cond;
    node->body = body;
    node->orelse = orelse;
    return node;
}

Node *makewhile(Node *cond, Nodelist *body) {
    Node *node = allocnode();

    node->type = TYPE_WHILE;
    node->cond = cond;
    node->body = body;
    return node;
}

Node *makebinop(int type, Node *left, Node *right) {
    Node *node = allocnode();

    node->type = type;
    node->left = left;
    node->right = right;
    return node;
}

Node *makeuop(int type, Node *left) {
    Node *node = allocnode();

    node->type = type;
    node->left = left;
    return node;
}

Node *makeassignment(Node *left, Node *right) {
    Node *node = allocnode();

    node->type = TYPE_ASSIGN;
    node->left = left;
    node->right = right;
    return node;
}

Node *makevarref(char *name) {
    Node *node = allocnode();
    Val *val = allocval();

    // TODO - intern?

    node->type = TYPE_VARREF;
    node->value = val;
    val->s = name;
    return node;
}

Node *makenum(double d) {
    Node *node = allocnode();
    Val *val = allocval();

    val->d = d;
    node->type = TYPE_NUMBER;
    node->value = val;
    return node;
}

Node *makestr(char *str) {
    Node *node = allocnode();
    Val *val = allocval();

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

    yyin = f;
    yyparse();

    freelist(program);

    return 0;
}
