/*
 * Copyright (C) 2012, Eric Fritz
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chinnu.h"
#include "symbol.h"

typedef struct Scope Scope;
typedef struct HashItem HashItem;
typedef struct SymbolTable SymbolTable;

const int MAP_SIZE = 32;

struct HashItem {
    Symbol *symbol;
    HashItem *next;
};

struct Scope {
    HashItem **buckets;
    Scope *next;
};

struct SymbolTable {
    Scope *top;
};

static int symbol_id = 0;

Symbol *make_symbol(char *name, Expression *declaration) {
    Symbol *symbol = malloc(sizeof(Symbol));

    if (!symbol) {
        fatal("Out of memory.");
    }

    symbol->name = strdup(name);
    symbol->id = symbol_id++;
    symbol->declaration = declaration;
    return symbol;
}

// djb2 by Dan Bernstein
int hash(char *str) {
    int hash = 5381;
    int c;

    while ((c = *str++)) {
        hash = ((hash << 5) + hash) ^ c;
    }

    return hash % MAP_SIZE;
}

void symbol_table_insert(SymbolTable *table, Symbol *symbol) {
    if (!table->top) {
        fatal("Empty scope.");
    }

    HashItem *item = malloc(sizeof(HashItem));
    HashItem **head = &table->top->buckets[hash(symbol->name)];

    if (!item) {
        fatal("Out of memory.");
    }

    item->symbol = symbol;
    item->next = *head;
    *head = item;
}

Symbol *scope_search(Scope *scope, char *name) {
    HashItem *head;
    for (head = scope->buckets[hash(name)]; head != NULL; head = head->next) {
        if (strcmp(head->symbol->name, name) == 0) {
            return head->symbol;
        }
    }

    return NULL;
}

Symbol *symbol_table_search(SymbolTable *table, char *name) {
    if (!table->top) {
        fatal("Empty scope.");
    }

    Scope *head;
    for (head = table->top; head != NULL; head = head->next) {
        Symbol *s;
        if ((s = scope_search(head, name)) != NULL) {
            return s;
        }
    }

    return NULL;
}

Symbol *symbol_table_search_current_scope(SymbolTable *table, char *name) {
    if (!table->top) {
        fatal("Empty scope.");
    }

    return scope_search(table->top, name);
}

void symbol_table_enter_scope(SymbolTable *table) {
    Scope *scope = malloc(sizeof(Scope));
    HashItem **buckets = malloc(sizeof(HashItem) * MAP_SIZE);

    if (!scope || !buckets) {
        fatal("Out of memory.");
    }

    for (int i = 0; i < MAP_SIZE; i++) {
        buckets[i] = NULL;
    }

    scope->buckets = buckets;
    scope->next = table->top;
    table->top = scope;
}

void free_scope(Scope *scope) {
    free(scope->buckets);
    free(scope);
}

void symbol_table_exit_scope(SymbolTable *table) {
    if (!table->top) {
        fatal("Empty scope.");
    }

    Scope *temp = table->top;
    table->top = table->top->next;
    free_scope(temp);
}

/* forward */
void expression_list_resolve(SymbolTable *table, ExpressionList *list);

void expression_resolve(SymbolTable *table, Expression *expr) {
    switch (expr->type) {
        case TYPE_DECLARATION:
        {
            Symbol *symbol = symbol_table_search_current_scope(table, expr->value->s);

            if (!symbol) {
                symbol = make_symbol(expr->value->s, expr);
                expr->symbol = symbol;
                symbol_table_insert(table, symbol);
            } else {
                error(expr->pos, "Redefinition of '%s'.", expr->value->s);
                message(symbol->declaration->pos, "Previous definition is here.");
            }

            if (expr->rexpr) {
                expression_resolve(table, expr->rexpr);
            }
        } break;

        case TYPE_FUNC:
        {
            if (expr->value->s) {
                Symbol *symbol = symbol_table_search_current_scope(table, expr->value->s);

                if (!symbol) {
                    symbol = make_symbol(expr->value->s, expr);
                    expr->symbol = symbol;
                    symbol_table_insert(table, symbol);
                } else {
                    error(expr->pos, "Redefinition of '%s'.", expr->value->s);
                    message(symbol->declaration->pos, "Previous definition is here.");
                }
            }

            symbol_table_enter_scope(table);
            expression_list_resolve(table, expr->llist);
            expression_resolve(table, expr->rexpr);
            symbol_table_exit_scope(table);
        } break;

        case TYPE_VARREF:
        {
            Symbol *symbol = symbol_table_search(table, expr->value->s);

            if (!symbol) {
                error(expr->pos, "Use of undeclared identifier '%s'.", expr->value->s);
            } else {
                expr->symbol = symbol;
            }
        } break;

        /* control flow */
        case TYPE_IF:
            expression_resolve(table, expr->cond);

            symbol_table_enter_scope(table);
            expression_resolve(table, expr->lexpr);
            symbol_table_exit_scope(table);

            if (expr->rexpr) {
                symbol_table_enter_scope(table);
                expression_resolve(table, expr->rexpr);
                symbol_table_exit_scope(table);
            }
            break;

        case TYPE_WHILE:
            expression_resolve(table, expr->cond);

            symbol_table_enter_scope(table);
            expression_resolve(table, expr->lexpr);
            symbol_table_exit_scope(table);
            break;

        case TYPE_CALL:
            expression_resolve(table, expr->lexpr);

            symbol_table_enter_scope(table);
            expression_list_resolve(table, expr->llist);
            symbol_table_exit_scope(table);
            break;

        /* binary cases */
        case TYPE_ASSIGN:
            expression_resolve(table, expr->lexpr);
            expression_resolve(table, expr->rexpr);

            if (expr->lexpr->symbol->declaration->immutable == 1) {
                error(expr->pos, "Assignment to a single-assignment variable.");
                message(expr->lexpr->symbol->declaration->pos, "Variable is defined here.");
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
        case TYPE_INT:
        case TYPE_REAL:
        case TYPE_BOOL:
        case TYPE_NULL:
        case TYPE_STRING:
            /* ignore */
            break;

        case TYPE_BLOCK:
            symbol_table_enter_scope(table);
            expression_list_resolve(table, expr->llist);
            symbol_table_exit_scope(table);
            break;
    }
}

void expression_list_resolve(SymbolTable *table, ExpressionList *list) {
    ExpressionNode *head;
    for (head = list->head; head != NULL; head = head->next) {
        expression_resolve(table, head->expr);
    }
}

void resolve(Expression *expr) {
    SymbolTable *table = malloc(sizeof(SymbolTable));

    if (!table) {
        fatal("Out of memory.");
    }

    table->top = NULL;
    symbol_table_enter_scope(table);
    expression_resolve(table, expr);
    symbol_table_exit_scope(table);
    free(table);
}
