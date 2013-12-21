#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chinnu.h"

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

void freenode(Node *node) {
    if (node) {
        if (node->cond)  freenode(node->cond);
        if (node->lnode) freenode(node->lnode);
        if (node->rnode) freenode(node->rnode);
        if (node->llist) freelist(node->llist);
        if (node->rlist) freelist(node->rlist);

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
        fprintf(stderr, "Out of memory.");
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

Node *makedeclaration(char *name) {
    Node *node = allocnode();
    Val *val = allocval();

    // TODO - intern?

    node->type = TYPE_DECLARATION;
    node->value = val;
    val->s = name;
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

/* for semantic analysis */

Symbol *makesymbol(char *name) {
    static int id = 0;

    Symbol *symbol = malloc(sizeof(Symbol));

    if (!symbol) {
        fprintf(stderr, "Out of memory.");
        exit(1);
    }

    symbol->name = strdup(name);
    symbol->id = id++;
    return symbol;
}

void insert(SymbolTable *table, Symbol *symbol) {
    if (!table->top) {
        fprintf(stderr, "Empty scope. Aborting.");
        exit(1);
    }

    ScopeItem *item = malloc(sizeof(ScopeItem));

    if (!item) {
        fprintf(stderr, "Out of memory.");
        exit(1);
    }

    item->symbol = symbol;
    item->next = table->top->scope;

    table->top->scope = item;
}

Symbol *search(SymbolTable *table, char *name) {
    if (!table->top) {
        fprintf(stderr, "Empty scope. Aborting.");
        exit(1);
    }

    SymbolItem *head1 = table->top;

    while (head1) {
        ScopeItem *head2 = head1->scope;

        while (head2) {
            if (strcmp(head2->symbol->name, name) == 0) {
                return head2->symbol;
            }

            head2 = head2->next;
        }

        head1 = head1->next;
    }

    return 0;
}

void enter_scope(SymbolTable *table) {
    SymbolItem *scope = malloc(sizeof(SymbolItem));

    if (!scope) {
        fprintf(stderr, "Out of memory.");
        exit(1);
    }

    scope->scope = (ScopeItem *) 0;
    scope->next = table->top;
    table->top = scope;
}

void free_scope(SymbolItem *top) {
    ScopeItem *head = top->scope;

    while (head) {
        ScopeItem *temp = head;
        head = head->next;
        free(temp);
    }

    free(top);
}

void exit_scope(SymbolTable *table) {
    if (!table->top) {
        fprintf(stderr, "Empty scope. Aborting.");
        exit(1);
    }

    SymbolItem *next = table->top->next;
    free_scope(table->top);
    table->top = next;
}

/* forward */
void resolveList(SymbolTable *table, NodeList *list);

void resolveNode(SymbolTable *table, Node *node) {
    switch (node->type) {
        case TYPE_DECLARATION:;
            Symbol *s1 = search(table, node->value->s);

            if (!s1) {
                s1 = makesymbol(node->value->s);
                node->symbol = s1;
                insert(table, s1);
            } else {
                fprintf(stderr, "Duplicate declaration of %s\n", node->value->s);
                exit(1);
            }

            break;

        case TYPE_VARREF:;
            Symbol *s2 = search(table, node->value->s);

            if (!s2) {
                fprintf(stderr, "Use of undeclared variable %s!\n", node->value->s);
                exit(1);
            } else {
                node->symbol = s2;
            }

            break;

        /* control flow */
        case TYPE_IF:
            resolveNode(table, node->cond);

            enter_scope(table);
            resolveList(table, node->llist);
            exit_scope(table);

            enter_scope(table);
            resolveList(table, node->rlist);
            exit_scope(table);
            break;

        case TYPE_WHILE:
            resolveNode(table, node->cond);

            enter_scope(table);
            resolveList(table, node->llist);
            exit_scope(table);
            break;

        case TYPE_CALL:
            resolveNode(table, node->lnode);

            enter_scope(table);
            resolveList(table, node->rlist);
            exit_scope(table);
            break;

        case TYPE_FUNC:
            enter_scope(table);
            resolveList(table, node->llist);

            enter_scope(table);
            resolveList(table, node->rlist);

            exit_scope(table);
            exit_scope(table);
            break;

        /* binary cases */
        case TYPE_ASSIGN:
        case TYPE_ADD:
        case TYPE_SUB:
        case TYPE_MUL:
        case TYPE_DIV:
        case TYPE_EQEQ:
        case TYPE_NEQ:
        case TYPE_LT:
        case TYPE_LEQ:
        case TYPE_GT:
        case TYPE_GEQ:
        case TYPE_AND:
        case TYPE_OR:
            resolveNode(table, node->lnode);
            resolveNode(table, node->rnode);
            break;

        /* unary cases */
        case TYPE_NEG:
        case TYPE_NOT:
            resolveNode(table, node->lnode);
            break;

        /* constants */
        case TYPE_NUMBER:
        case TYPE_STRING:
            /* ignore */
            break;
    }
}

void resolveList(SymbolTable *table, NodeList *list) {
    if (!list) {
        fprintf(stderr, "Did not expect empty list. Aborting.");
        exit(1);
    }

    ListItem *item = list->head;

    while (item) {
        resolveNode(table, item->node);
        item = item->next;
    }
}

/* for debugging */

/* forward */
void displist(NodeList *list, int indent);

void dispnode(Node *node, int indent) {
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

        if (node->cond) dispnode(node->cond, indent + 1);
        if (node->lnode) dispnode(node->lnode, indent + 1);
        if (node->rnode) dispnode(node->rnode, indent + 1);
        if (node->llist) displist(node->llist, indent + 1);
        if (node->rlist) displist(node->rlist, indent + 1);
    }
}

void displist(NodeList *list, int indent) {
    if (list) {
        ListItem *item = list->head;

        while (item) {
            dispnode(item->node, indent);
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

    SymbolTable *table = malloc(sizeof(SymbolTable));

    if (!table) {
        fprintf(stderr, "Out of memory.");
        exit(1);
    }

    table->top = (SymbolItem *) 0;
    enter_scope(table);
    resolveList(table, program);
    exit_scope(table);
    free(table);

    displist(program, 0);
    freelist(program);
    fclose(f);

    return 0;
}
