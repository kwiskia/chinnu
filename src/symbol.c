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
    int level;
    Scope *top;
    FunctionDesc *func;
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

FunctionDesc *make_desc(Expression *expr) {
    FunctionDesc *desc = malloc(sizeof(FunctionDesc));
    Local **locals = malloc(sizeof(Local *) * 8);
    Upvar **upvars = malloc(sizeof(Upvar *) * 8);
    FunctionDesc **functions = malloc(sizeof(FunctionDesc *) * 4);

    if (!desc || !locals || !upvars || !functions) {
        fatal("Out of memory.");
    }

    desc->expr = expr;
    desc->parent = NULL;
    desc->locals = locals;
    desc->upvars = upvars;
    desc->functions = functions;
    desc->numlocals = 0;
    desc->numupvars = 0;
    desc->numfunctions = 0;
    desc->maxlocals = 8;
    desc->maxupvars = 8;
    desc->maxfunctions = 4;

    return desc;
}

void free_desc(FunctionDesc *desc) {
    int i;
    for (i = 0; i < desc->numlocals; i++) {
        free(desc->locals[i]);
    }

    for (i = 0; i < desc->numupvars; i++) {
        free(desc->upvars[i]);
    }

    free(desc->locals);
    free(desc->upvars);
    free(desc->functions);
    free(desc);
}

int add_local(FunctionDesc *desc, Local *local) {
    if (desc->numlocals == desc->maxlocals) {
        desc->maxlocals *= 2;

        Local **resize = realloc(desc->locals, sizeof(Local *) * desc->maxlocals);

        if (!resize) {
            fatal("Out of memory.");
        }

        desc->locals = resize;
    }

    desc->locals[desc->numlocals] = local;
    return desc->numlocals++;
}

int add_upvar(FunctionDesc *desc, Upvar *upvar) {
    if (desc->numupvars == desc->maxupvars) {
        desc->maxupvars *= 2;

        Upvar **resize = realloc(desc->upvars, sizeof(Upvar *) * desc->maxupvars);

        if (!resize) {
            fatal("Out of memory.");
        }

        desc->upvars = resize;
    }

    desc->upvars[desc->numupvars] = upvar;
    return desc->numupvars++;
}

int add_function(FunctionDesc *desc, FunctionDesc *function) {
    if (desc->numfunctions == desc->maxfunctions) {
        desc->maxfunctions *= 2;

        FunctionDesc **resize = realloc(desc->functions, sizeof(FunctionDesc *) * desc->maxfunctions);

        if (!resize) {
            fatal("Out of memory.");
        }

        desc->functions = resize;
    }

    desc->functions[desc->numfunctions] = function;
    return desc->numfunctions++;
}

int local_index(FunctionDesc *desc, Symbol *symbol) {
    int i;
    for (i = 0; i < desc->numlocals; i++) {
        if (desc->locals[i]->symbol == symbol) {
            return i;
        }
    }

    return -1;
}

int upvar_index(FunctionDesc *desc, Symbol *symbol) {
    int i;
    for (i = 0; i < desc->numupvars; i++) {
        if (desc->upvars[i]->symbol == symbol) {
            return i;
        }
    }

    return -1;
}

int register_upvar(FunctionDesc *desc, Symbol *symbol) {
    int index = upvar_index(desc, symbol);

    if (index != -1) {
        return index;
    }

    index = local_index(desc->parent, symbol);

    Upvar *upvar = make_upvar(symbol);

    if (index != -1) {
        upvar->refslot = index;
        upvar->reftype = PARENT_LOCAL;
    } else {
        index = register_upvar(desc->parent, symbol);

        upvar->refslot = index;
        upvar->reftype = PARENT_UPVAR;
    }

    return add_upvar(desc, upvar);
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

Symbol *find_symbol(SymbolTable *table, char *name) {
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

void enter_scope(SymbolTable *table) {
    Scope *scope = malloc(sizeof(Scope));
    HashItem **buckets = malloc(sizeof(HashItem) * MAP_SIZE);

    if (!scope || !buckets) {
        fatal("Out of memory.");
    }

    int i;
    for (i = 0; i < MAP_SIZE; i++) {
        buckets[i] = NULL;
    }

    scope->buckets = buckets;
    scope->next = table->top;
    table->top = scope;
}

void free_scope(Scope *scope) {
    int i;
    for (i = 0; i < MAP_SIZE; i++) {
        HashItem *head = scope->buckets[i];
        while (head != NULL) {
            HashItem *temp = head->next;
            free(head);
            head = temp;
        }
    }

    free(scope->buckets);
    free(scope);
}

void leave_scope(SymbolTable *table) {
    if (!table->top) {
        fatal("Empty scope.");
    }

    Scope *temp = table->top;
    table->top = table->top->next;
    free_scope(temp);
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
            add_local(table->func, make_local(symbol));
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
                add_local(table->func, make_local(symbol));
            }

            FunctionDesc *temp = table->func;
            FunctionDesc *desc = make_desc(expr);

            expr->desc = desc;
            desc->parent = temp;
            add_function(temp, desc);

            table->func = desc;

            table->level++;
            enter_scope(table);
            resolve_list(table, expr->llist);
            resolve_expr(table, expr->rexpr);
            leave_scope(table);
            table->level--;

            table->func = temp;
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
                    register_upvar(table->func, symbol);
                }

                expr->symbol = symbol;
            }
        } break;

        /* control flow */
        case TYPE_IF:
            resolve_expr(table, expr->cond);

            enter_scope(table);
            resolve_expr(table, expr->lexpr);
            leave_scope(table);

            if (expr->rexpr) {
                enter_scope(table);
                resolve_expr(table, expr->rexpr);
                leave_scope(table);
            }
            break;

        case TYPE_WHILE:
            resolve_expr(table, expr->cond);

            enter_scope(table);
            resolve_expr(table, expr->lexpr);
            leave_scope(table);
            break;

        case TYPE_CALL:
            resolve_expr(table, expr->lexpr);

            enter_scope(table);
            resolve_list(table, expr->llist);
            leave_scope(table);
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
            enter_scope(table);
            resolve_list(table, expr->llist);
            leave_scope(table);
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

    FunctionDesc *desc = make_desc(expr);
    expr->desc = desc;

    table->top = NULL;
    table->level = 0;
    table->func = desc;

    enter_scope(table);
    resolve_expr(table, expr);
    leave_scope(table);

    free(table);
}
