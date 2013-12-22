#pragma once

#include "common.h"
#include "symbol.h"

enum {
    TYPE_IF,
    TYPE_WHILE,
    TYPE_ADD,
    TYPE_SUB,
    TYPE_MUL,
    TYPE_DIV,
    TYPE_NEG,
    TYPE_NOT,
    TYPE_ASSIGN,
    TYPE_EQEQ,
    TYPE_NEQ,
    TYPE_LT,
    TYPE_LEQ,
    TYPE_GT,
    TYPE_GEQ,
    TYPE_AND,
    TYPE_OR,
    TYPE_VARREF,
    TYPE_NUMBER,
    TYPE_STRING,
    TYPE_CALL,
    TYPE_FUNC,
    TYPE_DECLARATION
};

struct Val {
    union {
        int i;
        double d;
        char *s;
    };
};

struct Expression {
    unsigned char type;

    Expression *cond;
    Expression *lexpr;
    Expression *rexpr;
    ExpressionList *llist;
    ExpressionList *rlist;

    Val *value;
    Symbol *symbol;
    int immutable;
};

struct ExpressionNode {
    Expression *expr;
    ExpressionNode *next;
};

struct ExpressionList {
    ExpressionNode *head;
    ExpressionNode *tail;
};

void free_expression(Expression *expr);
void free_expression_list(ExpressionList *list);

ExpressionList *make_list();
ExpressionList *list1(Expression *expr);
ExpressionList *expression_list_append(ExpressionList *list, Expression *expr);

Expression *make_if(Expression *cond, ExpressionList *body, ExpressionList *orelse);
Expression *make_while(Expression *cond, ExpressionList *body);
Expression *make_binop(int type, Expression *left, Expression *right);
Expression *make_uop(int type, Expression *left);
Expression *make_declaration(char *name, Expression *value, int immutable);
Expression *make_assignment(Expression *varref, Expression *value);
Expression *make_varref(char *name);
Expression *make_int(int i);
Expression *make_real(double d);
Expression *make_str(char *str);
Expression *make_call(Expression *target, ExpressionList *arguments);
Expression *make_func(ExpressionList *parameters, ExpressionList *body);
