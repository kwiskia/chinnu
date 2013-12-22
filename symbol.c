#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "symbol.h"

typedef struct SymbolNode SymbolNode;
typedef struct ScopeNode ScopeNode;
typedef struct SymbolTable SymbolTable;

struct SymbolNode {
    Symbol *symbol;
    SymbolNode *next;
};

struct ScopeNode {
    SymbolNode *first;
    ScopeNode *next;
};

struct SymbolTable {
    ScopeNode *top;
};

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

void symbol_table_insert(SymbolTable *table, Symbol *symbol) {
    if (!table->top) {
        fprintf(stderr, "Empty scope. Aborting.");
        exit(1);
    }

    SymbolNode *node = malloc(sizeof(SymbolNode));

    if (!node) {
        fprintf(stderr, "Out of memory.");
        exit(1);
    }

    node->symbol = symbol;
    node->next = table->top->first;

    table->top->first = node;
}

Symbol *symbol_table_search_scope(SymbolNode *node, char *name) {
    SymbolNode *head = node;

    while (head) {
        if (strcmp(head->symbol->name, name) == 0) {
            return head->symbol;
        }

        head = head->next;
    }

    return 0;
}

Symbol *symbol_table_search(SymbolTable *table, char *name) {
    if (!table->top) {
        fprintf(stderr, "Empty scope. Aborting.");
        exit(1);
    }

    ScopeNode *head = table->top;

    while (head) {
        Symbol *s = symbol_table_search_scope(head->first, name);

        if (s) {
            return s;
        }

        head = head->next;
    }

    return 0;
}

Symbol *symbol_table_search_current_scope(SymbolTable *table, char *name) {
    if (!table->top) {
        fprintf(stderr, "Empty scope. Aborting.");
        exit(1);
    }

    return symbol_table_search_scope(table->top->first, name);
}

void symbol_table_enter_scope(SymbolTable *table) {
    ScopeNode *scope = malloc(sizeof(ScopeNode));

    if (!scope) {
        fprintf(stderr, "Out of memory.");
        exit(1);
    }

    scope->first = (SymbolNode *) 0;
    scope->next = table->top;
    table->top = scope;
}

void scope_node_free(ScopeNode *scope) {
    SymbolNode *head = scope->first;

    while (head) {
        SymbolNode *temp = head;
        head = head->next;
        free(temp);
    }

    free(scope);
}

void symbol_table_exit_scope(SymbolTable *table) {
    if (!table->top) {
        fprintf(stderr, "Empty scope. Aborting.");
        exit(1);
    }

    ScopeNode *temp = table->top;
    table->top = table->top->next;
    scope_node_free(temp);
}

/* forward */
void resolve_list(SymbolTable *table, NodeList *list);

void resolve_node(SymbolTable *table, Node *node) {
    switch (node->type) {
        case TYPE_DECLARATION:;
            Symbol *s1 = symbol_table_search_current_scope(table, node->value->s);

            if (!s1) {
                s1 = make_symbol(node->value->s, node);
                node->symbol = s1;
                symbol_table_insert(table, s1);
            } else {
                fprintf(stderr, "Duplicate declaration of %s\n", node->value->s);
                exit(1);
            }

            if (node->rnode) {
                resolve_node(table, node->rnode);
            }

            break;

        case TYPE_VARREF:;
            Symbol *s2 = symbol_table_search(table, node->value->s);

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

            symbol_table_enter_scope(table);
            resolve_list(table, node->llist);
            symbol_table_exit_scope(table);

            symbol_table_enter_scope(table);
            resolve_list(table, node->rlist);
            symbol_table_exit_scope(table);
            break;

        case TYPE_WHILE:
            resolve_node(table, node->cond);

            symbol_table_enter_scope(table);
            resolve_list(table, node->llist);
            symbol_table_exit_scope(table);
            break;

        case TYPE_CALL:
            resolve_node(table, node->lnode);

            symbol_table_enter_scope(table);
            resolve_list(table, node->rlist);
            symbol_table_exit_scope(table);
            break;

        case TYPE_FUNC:
            symbol_table_enter_scope(table);
            resolve_list(table, node->llist);

            symbol_table_enter_scope(table);
            resolve_list(table, node->rlist);

            symbol_table_exit_scope(table);
            symbol_table_exit_scope(table);
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

void resolve(NodeList *program) {
    SymbolTable *table = malloc(sizeof(SymbolTable));

    if (!table) {
        fprintf(stderr, "Out of memory.");
        exit(1);
    }

    table->top = (ScopeNode *) 0;
    symbol_table_enter_scope(table);
    resolve_list(table, program);
    symbol_table_exit_scope(table);
    free(table);
}
