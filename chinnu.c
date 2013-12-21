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

/* for semantic analysis */

Symbol *make_symbol(char *name, Node *declaration) {
    static int id = 0;

    Symbol *symbol = malloc(sizeof(Symbol));

    if (!symbol) {
        fprintf(stderr, "Out of memory.");
        exit(1);
    }

    symbol->name = strdup(name);
    symbol->id = id++;
    symbol->declaration = declaration;
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

Symbol *search_in_scope(ScopeItem *scope, char *name) {
    ScopeItem *head = scope;

    while (head) {
        if (strcmp(head->symbol->name, name) == 0) {
            return head->symbol;
        }

        head = head->next;
    }

    return 0;
}
Symbol *search(SymbolTable *table, char *name) {
    if (!table->top) {
        fprintf(stderr, "Empty scope. Aborting.");
        exit(1);
    }

    SymbolItem *head = table->top;

    while (head) {
        Symbol *s = search_in_scope(head->scope, name);

        if (s) {
            return s;
        }

        head = head->next;
    }

    return 0;
}

Symbol *search_in_current_scope(SymbolTable *table, char *name) {
    if (!table->top) {
        fprintf(stderr, "Empty scope. Aborting.");
        exit(1);
    }

    return search_in_scope(table->top->scope, name);
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
void resolve_list(SymbolTable *table, NodeList *list);

void resolve_node(SymbolTable *table, Node *node) {
    switch (node->type) {
        case TYPE_DECLARATION:;
            Symbol *s1 = search_in_current_scope(table, node->value->s);

            if (!s1) {
                s1 = make_symbol(node->value->s, node);
                node->symbol = s1;
                insert(table, s1);
            } else {
                fprintf(stderr, "Duplicate declaration of %s\n", node->value->s);
                exit(1);
            }

            if (node->rnode) {
                resolve_node(table, node->rnode);
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
            resolve_node(table, node->cond);

            enter_scope(table);
            resolve_list(table, node->llist);
            exit_scope(table);

            enter_scope(table);
            resolve_list(table, node->rlist);
            exit_scope(table);
            break;

        case TYPE_WHILE:
            resolve_node(table, node->cond);

            enter_scope(table);
            resolve_list(table, node->llist);
            exit_scope(table);
            break;

        case TYPE_CALL:
            resolve_node(table, node->lnode);

            enter_scope(table);
            resolve_list(table, node->rlist);
            exit_scope(table);
            break;

        case TYPE_FUNC:
            enter_scope(table);
            resolve_list(table, node->llist);

            enter_scope(table);
            resolve_list(table, node->rlist);

            exit_scope(table);
            exit_scope(table);
            break;

        /* binary cases */
        case TYPE_ASSIGN:
            resolve_node(table, node->lnode);
            resolve_node(table, node->rnode);

            if (node->lnode->symbol->declaration->immutable == 1) {
                fprintf(stderr, "Assignment to a VAL.\n");
                exit(1);
            }

            break;

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
            resolve_node(table, node->lnode);
            resolve_node(table, node->rnode);
            break;

        /* unary cases */
        case TYPE_NEG:
        case TYPE_NOT:
            resolve_node(table, node->lnode);
            break;

        /* constants */
        case TYPE_NUMBER:
        case TYPE_STRING:
            /* ignore */
            break;
    }
}

void resolve_list(SymbolTable *table, NodeList *list) {
    if (!list) {
        fprintf(stderr, "Did not expect empty list. Aborting.");
        exit(1);
    }

    ListItem *item = list->head;

    while (item) {
        resolve_node(table, item->node);
        item = item->next;
    }
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

    SymbolTable *table = malloc(sizeof(SymbolTable));

    if (!table) {
        fprintf(stderr, "Out of memory.");
        exit(1);
    }

    table->top = (SymbolItem *) 0;
    enter_scope(table);
    resolve_list(table, program);
    exit_scope(table);
    free(table);

    print_list(program, 0);
    free_list(program);
    fclose(f);

    return 0;
}
