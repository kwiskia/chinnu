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

    node->cond = (Node *) 0;
    node->lnode = (Node *) 0;
    node->rnode = (Node *) 0;
    node->llist = (NodeList *) 0;
    node->rlist = (NodeList *) 0;
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
        if (node->cond)  freenode(node->cond);
        if (node->lnode) freenode(node->lnode);
        if (node->rnode) freenode(node->rnode);
        if (node->llist) freelist(node->llist);
        if (node->rlist) freelist(node->rlist);

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

NodeList *makelist() {
    NodeList *list = malloc(sizeof(NodeList));

    if (!list) {
        yyerror("Out of memory.");
        exit(1);
    }

    list->head = (ListItem *) 0;
    list->tail = (ListItem *) 0;
    return list;
}

NodeList *list1(Node *node) {
    NodeList *list = makelist();
    append(list, node);
    return list;
}

NodeList *append(NodeList *list, Node *node) {
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

    return list;
}

Node *makeif(Node *cond, NodeList *body, NodeList *orelse) {
    Node *node = allocnode();

    node->type = TYPE_IF;
    node->cond = cond;
    node->llist = body;
    node->rlist = orelse;
    return node;
}

Node *makewhile(Node *cond, NodeList *body) {
    Node *node = allocnode();

    node->type = TYPE_WHILE;
    node->cond = cond;
    node->llist = body;
    return node;
}

Node *makebinop(int type, Node *left, Node *right) {
    Node *node = allocnode();

    node->type = type;
    node->lnode = left;
    node->rnode = right;
    return node;
}

Node *makeuop(int type, Node *left) {
    Node *node = allocnode();

    node->type = type;
    node->lnode = left;
    return node;
}

Node *makeassignment(Node *left, Node *right) {
    Node *node = allocnode();

    node->type = TYPE_ASSIGN;
    node->lnode = left;
    node->rnode = right;
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

Node *makecall(Node *target, NodeList *arguments) {
    Node *node = allocnode();

    node->type = TYPE_CALL;
    node->lnode = target;
    node->rlist = arguments;
    return node;
}

Node *makefunc(NodeList *parameters, NodeList *body) {
    Node *node = allocnode();

    node->type = TYPE_FUNC;
    node->llist = parameters;
    node->rlist = body;
    return node;
}

/* for debugging */

/* forward */
void displist(NodeList *list, int indent);

void dispnode(Node *node, int indent) {
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

        if (node->cond) dispnode(node->cond, indent + 1);
        if (node->lnode) dispnode(node->lnode, indent + 1);
        if (node->rnode) dispnode(node->rnode, indent + 1);
        if (node->llist) displist(node->llist, indent + 1);
        if (node->rlist) displist(node->rlist, indent + 1);
    }
}

void displist(NodeList *list, int indent) {
    ListItem *item = list->head;

    while (item) {
        dispnode(item->node, indent);
        item = item->next;
    }
}

int main(int argc, const char **argv) {
    FILE *f = fopen("test.ch", "r");

    if (!f) {
        fprintf(stderr, "Could not open file.");
    }

    yyin = f;
    yyparse();

    displist(program, 0);
    freelist(program);

    return 0;
}
