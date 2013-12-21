#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "symbol.h"

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

void resolve(NodeList *program) {
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
}

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
