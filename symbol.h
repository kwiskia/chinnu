#pragma once

#include "common.h"
#include "ast.h"

struct Symbol {
    int id;
    char *name;
    Node *declaration;
};

void resolve(NodeList *list);
