#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chinnu.h"
#include "symbol.h"

extern FILE *yyin;
extern void yylex_destroy();

Node *allocnode() {
    Node *node = malloc(sizeof(Node));

    if (!node) {
        fprintf(stderr, "Out of memory.");
        exit(1);
    }

    node->cond = (Node *) 0;
    node->lnode = (Node *) 0;
    node->rnode = (Node *) 0;
    node->llist = (NodeList *) 0;
    node->rlist = (NodeList *) 0;
    node->value = (Val *) 0;
    node->symbol = (Symbol *) 0;
    node->immutable = 0;

    return node;
}

Val *allocval() {
    Val *val = malloc(sizeof(Val));

    if (!val) {
        fprintf(stderr, "Out of memory.");
        exit(1);
    }

    return val;
}

void free_node(Node *node) {
    if (node) {
        if (node->cond)  free_node(node->cond);
        if (node->lnode) free_node(node->lnode);
        if (node->rnode) free_node(node->rnode);
        if (node->llist) free_list(node->llist);
        if (node->rlist) free_list(node->rlist);

        if (node->value) {
            if (node->type == TYPE_VARREF || node->type == TYPE_DECLARATION || node->type == TYPE_STRING) {
                free(node->value->s);
            }

            free(node->value);
        }

        if (node->type == TYPE_DECLARATION) {
            free(node->symbol->name);
            free(node->symbol);
        }

        free(node);
    }
}

void free_item(ListItem *item) {
    if (item) {
        free_node(item->node);
        free_item(item->next);
        free(item);
    }
}

void free_list(NodeList *list) {
    if (list) {
        free_item(list->head);
        free(list);
    }
}

NodeList *make_list() {
    NodeList *list = malloc(sizeof(NodeList));

    if (!list) {
        fprintf(stderr, "Out of memory.");
        exit(1);
    }

    list->head = (ListItem *) 0;
    list->tail = (ListItem *) 0;
    return list;
}

NodeList *list1(Node *node) {
    NodeList *list = make_list();
    append(list, node);
    return list;
}

NodeList *append(NodeList *list, Node *node) {
    ListItem *item = malloc(sizeof(ListItem));

    if (!item) {
        fprintf(stderr, "Out of memory.");
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

Node *make_if(Node *cond, NodeList *body, NodeList *orelse) {
    Node *node = allocnode();

    node->type = TYPE_IF;
    node->cond = cond;
    node->llist = body;
    node->rlist = orelse;
    return node;
}

Node *make_while(Node *cond, NodeList *body) {
    Node *node = allocnode();

    node->type = TYPE_WHILE;
    node->cond = cond;
    node->llist = body;
    return node;
}

Node *make_binop(int type, Node *left, Node *right) {
    Node *node = allocnode();

    node->type = type;
    node->lnode = left;
    node->rnode = right;
    return node;
}

Node *make_uop(int type, Node *left) {
    Node *node = allocnode();

    node->type = type;
    node->lnode = left;
    return node;
}

Node *make_declaration(char *name, Node *value, int immutable) {
    Node *node = allocnode();
    Val *val = allocval();

    // TODO - intern?

    node->type = TYPE_DECLARATION;
    node->rnode = value;
    node->value = val;
    val->s = name;
    node->immutable = immutable;
    return node;
}

Node *make_assignment(Node *left, Node *right) {
    Node *node = allocnode();

    node->type = TYPE_ASSIGN;
    node->lnode = left;
    node->rnode = right;
    return node;
}

Node *make_varref(char *name) {
    Node *node = allocnode();
    Val *val = allocval();

    // TODO - intern?

    node->type = TYPE_VARREF;
    node->value = val;
    val->s = name;
    return node;
}

Node *make_int(int i) {
    Node *node = allocnode();
    Val *val = allocval();

    val->i = i;
    node->type = TYPE_NUMBER;
    node->value = val;
    return node;
}

Node *make_real(double d) {
    Node *node = allocnode();
    Val *val = allocval();

    val->d = d;
    node->type = TYPE_NUMBER;
    node->value = val;
    return node;
}

Node *make_str(char *str) {
    Node *node = allocnode();
    Val *val = allocval();

    node->type = TYPE_STRING;
    node->value = val;
    val->s = str;
    return node;
}

Node *make_call(Node *target, NodeList *arguments) {
    Node *node = allocnode();

    node->type = TYPE_CALL;
    node->lnode = target;
    node->rlist = arguments;
    return node;
}

Node *make_func(NodeList *parameters, NodeList *body) {
    Node *node = allocnode();

    node->type = TYPE_FUNC;
    node->llist = parameters;
    node->rlist = body;
    return node;
}

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
