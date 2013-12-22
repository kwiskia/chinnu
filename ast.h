#pragma once

#include "common.h"
#include "symbol.h"

enum {
    TYPE_IF,
    TYPE_WHILE,
    TYPE_ADD,
    TYPE_SUB,
    TYPE_MUL,
    TYPE_DIV,
    TYPE_NEG,
    TYPE_NOT,
    TYPE_ASSIGN,
    TYPE_EQEQ,
    TYPE_NEQ,
    TYPE_LT,
    TYPE_LEQ,
    TYPE_GT,
    TYPE_GEQ,
    TYPE_AND,
    TYPE_OR,
    TYPE_VARREF,
    TYPE_NUMBER,
    TYPE_STRING,
    TYPE_CALL,
    TYPE_FUNC,
    TYPE_DECLARATION
};

struct Val {
    union {
        int i;
        double d;
        char *s;
    };
};

struct Node {
    unsigned char type;

    Node *cond;
    Node *lnode;
    Node *rnode;
    NodeList *llist;
    NodeList *rlist;

    Val *value;
    Symbol *symbol;
    int immutable;
};

struct ListItem {
    Node *node;
    ListItem *next;
};

struct NodeList {
    ListItem *head;
    ListItem *tail;
};

void free_node(Node *node);
void free_list(NodeList *list);

NodeList *make_list();
NodeList *list1(Node *node);
NodeList *append(NodeList *list, Node *node);

Node *make_if(Node *cond, NodeList *body, NodeList *orelse);
Node *make_while(Node *cond, NodeList *body);
Node *make_binop(int type, Node *left, Node *right);
Node *make_uop(int type, Node *left);
Node *make_declaration(char *name, Node *value, int immutable);
Node *make_assignment(Node *varref, Node *value);
Node *make_varref(char *name);
Node *make_int(int i);
Node *make_real(double d);
Node *make_str(char *str);
Node *make_call(Node *target, NodeList *arguments);
Node *make_func(NodeList *parameters, NodeList *body);
