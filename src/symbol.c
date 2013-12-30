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

typedef struct Contour Contour;
typedef struct HashItem HashItem;
typedef struct SymbolTable SymbolTable;

const int MAP_SIZE = 32;

struct HashItem {
    Symbol *symbol;
    HashItem *next;
};

struct Contour {
    HashItem **buckets;
    Contour *next;
};

struct SymbolTable {
    int level;
    Contour *top;
    Scope *scope;
};

Local *make_local(Symbol *symbol) {
    Local *local = malloc(sizeof(Local));

    if (!local) {
        fatal("Out of memory.");
    }

    local->symbol = symbol;
    return local;
}

Upvar *make_upvar(Symbol *symbol) {
    Upvar *upvar = malloc(sizeof(Upvar));

    if (!upvar) {
        fatal("Out of memory.");
    }

    upvar->symbol = symbol;
    return upvar;
}

Scope *make_scope(Expression *expr) {
    Scope *scope = malloc(sizeof(Scope));
    Local **locals = malloc(sizeof(Local *) * 8);
    Upvar **upvars = malloc(sizeof(Upvar *) * 8);
    Scope **children = malloc(sizeof(Scope *) * 4);

    if (!scope || !locals || !upvars || !children) {
        fatal("Out of memory.");
    }

    scope->expr = expr;
    scope->parent = NULL;
    scope->locals = locals;
    scope->upvars = upvars;
    scope->children = children;
    scope->numlocals = 0;
    scope->numupvars = 0;
    scope->numchildren = 0;
    scope->maxlocals = 8;
    scope->maxupvars = 8;
    scope->maxchildren = 4;

    return scope;
}

void free_scope(Scope *scope) {
    int i;
    for (i = 0; i < scope->numlocals; i++) {
        free(scope->locals[i]);
    }

    for (i = 0; i < scope->numupvars; i++) {
        free(scope->upvars[i]);
    }

    free(scope->locals);
    free(scope->upvars);
    free(scope->children);
    free(scope);
}

int add_local(Scope *scope, Local *local) {
    if (scope->numlocals == scope->maxlocals) {
        scope->maxlocals *= 2;

        Local **resize = realloc(scope->locals, sizeof(Local *) * scope->maxlocals);

        if (!resize) {
            fatal("Out of memory.");
        }

        scope->locals = resize;
    }

    scope->locals[scope->numlocals] = local;
    return scope->numlocals++;
}

int add_upvar(Scope *scope, Upvar *upvar) {
    if (scope->numupvars == scope->maxupvars) {
        scope->maxupvars *= 2;

        Upvar **resize = realloc(scope->upvars, sizeof(Upvar *) * scope->maxupvars);

        if (!resize) {
            fatal("Out of memory.");
        }

        scope->upvars = resize;
    }

    scope->upvars[scope->numupvars] = upvar;
    return scope->numupvars++;
}

int add_child(Scope *scope, Scope *child) {
    if (scope->numchildren == scope->maxchildren) {
        scope->maxchildren *= 2;

        Scope **resize = realloc(scope->children, sizeof(Scope *) * scope->maxchildren);

        if (!resize) {
            fatal("Out of memory.");
        }

        scope->children = resize;
    }

    scope->children[scope->numchildren] = child;
    return scope->numchildren++;
}

int local_index(Scope *scope, Symbol *symbol) {
    int i;
    for (i = 0; i < scope->numlocals; i++) {
        if (scope->locals[i]->symbol == symbol) {
            return i;
        }
    }

    return -1;
}

int upvar_index(Scope *scope, Symbol *symbol) {
    int i;
    for (i = 0; i < scope->numupvars; i++) {
        if (scope->upvars[i]->symbol == symbol) {
            return i;
        }
    }

    return -1;
}

int register_upvar(Scope *scope, Symbol *symbol) {
    int index = upvar_index(scope, symbol);

    if (index != -1) {
        return index;
    }

    index = local_index(scope->parent, symbol);

    Upvar *upvar = make_upvar(symbol);

    if (index != -1) {
        upvar->refslot = index;
        upvar->reftype = PARENT_LOCAL;
    } else {
        index = register_upvar(scope->parent, symbol);

        upvar->refslot = index;
        upvar->reftype = PARENT_UPVAR;
    }

    return add_upvar(scope, upvar);
}

static int symbol_id = 0;

Symbol *make_symbol(char *name, int level, Expression *declaration) {
    Symbol *symbol = malloc(sizeof(Symbol));

    if (!symbol) {
        fatal("Out of memory.");
    }

    symbol->id = symbol_id++;
    symbol->level = level;
    symbol->name = strdup(name);
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

void add_symbol(SymbolTable *table, Symbol *symbol) {
    if (!table->top) {
        fatal("Empty contour.");
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

Symbol *contour_search(Contour *contour, char *name) {
    HashItem *head;
    for (head = contour->buckets[hash(name)]; head != NULL; head = head->next) {
        if (strcmp(head->symbol->name, name) == 0) {
            return head->symbol;
        }
    }

    return NULL;
}

Symbol *find_symbol(SymbolTable *table, char *name) {
    if (!table->top) {
        fatal("Empty contour.");
    }

    Contour *head;
    for (head = table->top; head != NULL; head = head->next) {
        Symbol *s;
        if ((s = contour_search(head, name)) != NULL) {
            return s;
        }
    }

    return NULL;
}

void enter_contour(SymbolTable *table) {
    Contour *contour = malloc(sizeof(Contour));
    HashItem **buckets = malloc(sizeof(HashItem) * MAP_SIZE);

    if (!contour || !buckets) {
        fatal("Out of memory.");
    }

    int i;
    for (i = 0; i < MAP_SIZE; i++) {
        buckets[i] = NULL;
    }

    contour->buckets = buckets;
    contour->next = table->top;
    table->top = contour;
}

void free_contour(Contour *contour) {
    int i;
    for (i = 0; i < MAP_SIZE; i++) {
        HashItem *head = contour->buckets[i];
        while (head != NULL) {
            HashItem *temp = head->next;
            free(head);
            head = temp;
        }
    }

    free(contour->buckets);
    free(contour);
}

void leave_contour(SymbolTable *table) {
    if (!table->top) {
        fatal("Empty contour.");
    }

    Contour *temp = table->top;
    table->top = table->top->next;
    free_contour(temp);
}

/* forward */
void resolve_list(SymbolTable *table, ExpressionList *list);

void resolve_expr(SymbolTable *table, Expression *expr) {
    switch (expr->type) {
        case TYPE_MODULE:
            resolve_expr(table, expr->lexpr);
            break;

        case TYPE_DECLARATION:
        {
            Symbol *symbol = find_symbol(table, expr->value->s);

            if (symbol && symbol->level == table->level) {
                error(expr->pos, "Redefinition of '%s'.", expr->value->s);
                message(symbol->declaration->pos, "Previous definition is here.");
            }

            if (symbol && warning_flags[WARNING_SHADOW]) {
                warning(expr->pos, "Shadowing declaration of '%s'.", expr->value->s);
                message(symbol->declaration->pos, "Previous definition is here.");
            }

            if (expr->rexpr) {
                resolve_expr(table, expr->rexpr);
            }

            symbol = make_symbol(expr->value->s, table->level, expr);
            expr->symbol = symbol;

            add_symbol(table, symbol);
            add_local(table->scope, make_local(symbol));
        } break;

        case TYPE_FUNC:
        {
            if (expr->value->s) {
                Symbol *symbol = find_symbol(table, expr->value->s);

                if (symbol && symbol->level == table->level) {
                    error(expr->pos, "Redefinition of '%s'.", expr->value->s);
                    message(symbol->declaration->pos, "Previous definition is here.");
                }

                if (symbol && warning_flags[WARNING_SHADOW]) {
                    warning(expr->pos, "Shadowing declaration of '%s'.", expr->value->s);
                    message(symbol->declaration->pos, "Previous definition is here.");
                }

                symbol = make_symbol(expr->value->s, table->level, expr);
                expr->symbol = symbol;

                add_symbol(table, symbol);
                add_local(table->scope, make_local(symbol));
            }

            Scope *temp = table->scope;
            Scope *scope = make_scope(expr);

            expr->scope = scope;
            scope->parent = temp;
            add_child(temp, scope);

            table->scope = scope;

            table->level++;
            enter_contour(table);
            resolve_list(table, expr->llist);
            resolve_expr(table, expr->rexpr);
            leave_contour(table);
            table->level--;

            table->scope = temp;
        } break;

        case TYPE_VARREF:
        {
            Symbol *symbol = find_symbol(table, expr->value->s);

            if (!symbol) {
                error(expr->pos, "Use of undeclared identifier '%s'.", expr->value->s);

                // TODO - will cause error here when traversing tree
                // put something here to find more errors [?]
            } else {
                if (symbol->level != table->level) {
                    register_upvar(table->scope, symbol);
                }

                expr->symbol = symbol;
            }
        } break;

        /* control flow */
        case TYPE_IF:
            resolve_expr(table, expr->cond);

            enter_contour(table);
            resolve_expr(table, expr->lexpr);
            leave_contour(table);

            if (expr->rexpr) {
                enter_contour(table);
                resolve_expr(table, expr->rexpr);
                leave_contour(table);
            }
            break;

        case TYPE_WHILE:
            resolve_expr(table, expr->cond);

            enter_contour(table);
            resolve_expr(table, expr->lexpr);
            leave_contour(table);
            break;

        case TYPE_CALL:
            resolve_expr(table, expr->lexpr);

            enter_contour(table);
            resolve_list(table, expr->llist);
            leave_contour(table);
            break;

        /* binary cases */
        case TYPE_ASSIGN:
            resolve_expr(table, expr->lexpr);
            resolve_expr(table, expr->rexpr);

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
            resolve_expr(table, expr->lexpr);
            resolve_expr(table, expr->rexpr);
            break;

        /* unary cases */
        case TYPE_NEG:
        case TYPE_NOT:
            resolve_expr(table, expr->lexpr);
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
            enter_contour(table);
            resolve_list(table, expr->llist);
            leave_contour(table);
            break;
    }
}

void resolve_list(SymbolTable *table, ExpressionList *list) {
    ExpressionNode *head;
    for (head = list->head; head != NULL; head = head->next) {
        resolve_expr(table, head->expr);
    }
}

void resolve(Expression *expr) {
    SymbolTable *table = malloc(sizeof(SymbolTable));

    if (!table) {
        fatal("Out of memory.");
    }

    Scope *scope = make_scope(expr);
    expr->scope = scope;

    table->top = NULL;
    table->level = 0;
    table->scope = scope;

    enter_contour(table);
    resolve_expr(table, expr);
    leave_contour(table);

    free(table);
}
