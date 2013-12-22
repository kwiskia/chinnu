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

Symbol *make_symbol(char *name, Expression *declaration) {
    static int id = 0;

    Symbol *symbol = malloc(sizeof(Symbol));

    if (!symbol) {
        fatal("Out of memory.");
    }

    symbol->name = strdup(name);
    symbol->id = id++;
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
    HashItem *head = scope->buckets[hash(name)];

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
        fatal("Empty scope.");
    }

    Scope *head = table->top;
    Symbol *s;

    while (head) {
        if ((s = scope_search(head, name))) {
            return s;
        }

        head = head->next;
    }

    return 0;
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
        buckets[i] = (HashItem *) 0;
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
        case TYPE_DECLARATION:;
            Symbol *s1 = symbol_table_search_current_scope(table, expr->value->s);

            if (!s1) {
                s1 = make_symbol(expr->value->s, expr);
                expr->symbol = s1;
                symbol_table_insert(table, s1);
            } else {
                error(expr->pos, "Redefinition of '%s'.\n", expr->value->s);
            }

            if (expr->rexpr) {
                expression_resolve(table, expr->rexpr);
            }

            break;

        case TYPE_VARREF:;
            Symbol *s2 = symbol_table_search(table, expr->value->s);

            if (!s2) {
                error(expr->pos, "Use of undeclared identifier '%s'.", expr->value->s);
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
            expression_list_resolve(table, expr->rlist);
            symbol_table_exit_scope(table);
            break;

        /* binary cases */
        case TYPE_ASSIGN:
            expression_resolve(table, expr->lexpr);
            expression_resolve(table, expr->rexpr);

            if (expr->lexpr->symbol->declaration->immutable == 1) {
                error(expr->pos, "Assignment to a single-assignment variable.");
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
    ExpressionNode *head = list->head;

    while (head) {
        expression_resolve(table, head->expr);
        head = head->next;
    }
}

void resolve(ExpressionList *program) {
    SymbolTable *table = malloc(sizeof(SymbolTable));

    if (!table) {
        fatal("Out of memory.");
    }

    table->top = (Scope *) 0;
    symbol_table_enter_scope(table);
    expression_list_resolve(table, program);
    symbol_table_exit_scope(table);
    free(table);
}
