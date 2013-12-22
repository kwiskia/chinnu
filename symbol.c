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

Symbol *make_symbol(char *name, Expression *declaration) {
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
void expression_list_resolve(SymbolTable *table, ExpressionList *list);

void expression_resolve(SymbolTable *table, Expression *expr) {
    switch (expr->type) {
        case TYPE_DECLARATION:;
            Symbol *s1 = symbol_table_search_current_scope(table, expr->value->s);

            if (!s1) {
                s1 = make_symbol(expr->value->s, expr);
                expr->symbol = s1;
                symbol_table_insert(table, s1);
            } else {
                fprintf(stderr, "Duplicate declaration of %s\n", expr->value->s);
                exit(1);
            }

            if (expr->rexpr) {
                expression_resolve(table, expr->rexpr);
            }

            break;

        case TYPE_VARREF:;
            Symbol *s2 = symbol_table_search(table, expr->value->s);

            if (!s2) {
                fprintf(stderr, "Use of undeclared variable %s!\n", expr->value->s);
                exit(1);
            } else {
                expr->symbol = s2;
            }

            break;

        /* control flow */
        case TYPE_IF:
            expression_resolve(table, expr->cond);

            symbol_table_enter_scope(table);
            expression_list_resolve(table, expr->llist);
            symbol_table_exit_scope(table);

            symbol_table_enter_scope(table);
            expression_list_resolve(table, expr->rlist);
            symbol_table_exit_scope(table);
            break;

        case TYPE_WHILE:
            expression_resolve(table, expr->cond);

            symbol_table_enter_scope(table);
            expression_list_resolve(table, expr->llist);
            symbol_table_exit_scope(table);
            break;

        case TYPE_CALL:
            expression_resolve(table, expr->lexpr);

            symbol_table_enter_scope(table);
            expression_list_resolve(table, expr->rlist);
            symbol_table_exit_scope(table);
            break;

        case TYPE_FUNC:
            symbol_table_enter_scope(table);
            expression_list_resolve(table, expr->llist);

            symbol_table_enter_scope(table);
            expression_list_resolve(table, expr->rlist);

            symbol_table_exit_scope(table);
            symbol_table_exit_scope(table);
            break;

        /* binary cases */
        case TYPE_ASSIGN:
            expression_resolve(table, expr->lexpr);
            expression_resolve(table, expr->rexpr);

            if (expr->lexpr->symbol->declaration->immutable == 1) {
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
            expression_resolve(table, expr->lexpr);
            expression_resolve(table, expr->rexpr);
            break;

        /* unary cases */
        case TYPE_NEG:
        case TYPE_NOT:
            expression_resolve(table, expr->lexpr);
            break;

        /* constants */
        case TYPE_NUMBER:
        case TYPE_STRING:
            /* ignore */
            break;
    }
}

void expression_list_resolve(SymbolTable *table, ExpressionList *list) {
    if (!list) {
        fprintf(stderr, "Did not expect empty list. Aborting.");
        exit(1);
    }

    ExpressionNode *head = list->head;

    while (head) {
        expression_resolve(table, head->expr);
        head = head->next;
    }
}

void resolve(ExpressionList *program) {
    SymbolTable *table = malloc(sizeof(SymbolTable));

    if (!table) {
        fprintf(stderr, "Out of memory.");
        exit(1);
    }

    table->top = (ScopeNode *) 0;
    symbol_table_enter_scope(table);
    expression_list_resolve(table, program);
    symbol_table_exit_scope(table);
    free(table);
}
