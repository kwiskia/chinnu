#ifndef __SYMBOL_H
#define __SYMBOL_H

#include "common.h"
#include "ast.h"

struct Symbol {
    int id;
    char *name;
    Node *declaration;
};

struct ScopeItem {
    Symbol *symbol;
    ScopeItem *next;
};

struct SymbolItem {
    ScopeItem *scope;
    SymbolItem *next;
};

struct SymbolTable {
    SymbolItem *top;
};

void resolve(NodeList *list);
void resolve_list(SymbolTable *table, NodeList *list);
void resolve_node(SymbolTable *table, Node *node);

#endif /* __SYMBOL_H */
