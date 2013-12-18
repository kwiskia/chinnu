#include <stdio.h>
#include <stdlib.h>

#include "chinnu.h"

extern FILE *yyin;
extern int yyerror(const char *);

Node *allocnode() {
    Node *node = malloc(sizeof(Node));

    if (!node) {
        yyerror("Out of memory.");
        exit(1);
    }

    node->nchildren = 0;
    node->children = (NodeList *) 0;
    node->value = (Val *) 0;

    return node;
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
        if (node->nchildren) {
            freelist(node->children);
        }

        if (node->value) {
            if (node->type == TYPE_VARREF || node->type == TYPE_STRING) {
                free(node->value->s);
            }

            free(node->value);
        }

        free(node);
    }
}

void freeitem(ListItem *item) {
    if (item) {
        freenode(item->node);
        freeitem(item->next);
        free(item);
    }
}

void freelist(NodeList *list) {
    if (list) {
        freeitem(list->head);
        free(list);
    }
}

NodeList *makelist()  {
    NodeList *list = malloc(sizeof(NodeList));

    if (!list) {
        yyerror("Out of memory.");
        exit(1);
    }

    list->head = (ListItem *) 0;
    list->tail = (ListItem *) 0;
    return list;
}

void append(NodeList *list, Node *node) {
    ListItem *item = malloc(sizeof(ListItem));

    if (!item) {
        yyerror("Out of memory.");
        exit(1);
    }

    item->node = node;
    item->next = (ListItem *) 0;

    if (!list->tail) {
        list->head = item;
        list->tail = item;
    } else {
        list->tail->next = item;
        list->tail = item;
    }
}

void addchild(Node *node, Node *child) {
    if (!node->nchildren) {
        node->children = makelist();
    }

    append(node->children, child);
    node->nchildren++;
}

Node *makeempty() {
    Node *node = allocnode();

    node->type = TYPE_EMPTY;
    return node;
}

Node *makeseq(Node *child1, Node *child2) {
    if (child1->type == TYPE_SEQUENCE) {
        addchild(child1, child2);
        return child1;
    } else {
        Node *node = allocnode();

        node->type = TYPE_SEQUENCE;
        addchild(node, child1);
        addchild(node, child2);
        return node;
    }
}

Node *makeif(Node *cond, Node *body, Node *orelse) {
    Node *node = allocnode();

    node->type = TYPE_IF;
    addchild(node, cond);
    addchild(node, body);
    addchild(node, orelse);
    return node;
}

Node *makewhile(Node *cond, Node *body) {
    Node *node = allocnode();

    node->type = TYPE_WHILE;
    addchild(node, cond);
    addchild(node, body);
    return node;
}

Node *makebinop(int type, Node *left, Node *right) {
    Node *node = allocnode();

    node->type = type;
    addchild(node, left);
    addchild(node, right);
    return node;
}

Node *makeuop(int type, Node *left) {
    Node *node = allocnode();

    node->type = type;
    addchild(node, left);
    return node;
}

Node *makeassignment(Node *left, Node *right) {
    Node *node = allocnode();

    node->type = TYPE_ASSIGN;
    addchild(node, left);
    addchild(node, right);
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

Node *makeint(int i) {
    Node *node = allocnode();
    Val *val = allocval();

    val->i = i;
    node->type = TYPE_NUMBER;
    node->value = val;
    return node;
}

Node *makereal(double d) {
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

/* for debugging */

void printnode(Node *node, int indent) {
    if (node) {
        for (int i = 0; i < indent; i++) {
            printf("\t");
        }

        if (node->value) {
            if (node->type == TYPE_STRING || node->type == TYPE_VARREF) {
                printf("[%d]: %s\n", node->type, node->value->s);
            } else {
                printf("[%d]: %.02f\n", node->type, node->value->d);
            }
        } else {
            printf("[%d]\n", node->type);
        }

        if (node->children) {
            ListItem *head = node->children->head;

            while (head) {
                printnode(head->node, indent + 1);
                head = head->next;
            }
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

    printnode(program, 0);
    freenode(program);

    return 0;
}
