#pragma once

#include "common.h"
#include "ast.h"

struct Symbol {
    int id;
    char *name;
    Expression *declaration;
};

void resolve(ExpressionList *list);
