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

%code requires {
typedef struct SourcePos YYLTYPE;

#define YYLTYPE_IS_DECLARED 1

#define YYLLOC_DEFAULT(Cur, Rhs, N)                                                \
    do {                                                                           \
        if (N) {                                                                   \
            (Cur).first_line   = YYRHSLOC(Rhs, 1).first_line;                      \
            (Cur).first_column = YYRHSLOC(Rhs, 1).first_column;                    \
            (Cur).last_line    = YYRHSLOC(Rhs, N).last_line;                       \
            (Cur).last_column  = YYRHSLOC(Rhs, N).last_column;                     \
            (Cur).filename     = YYRHSLOC(Rhs, 1).filename;                        \
        } else {                                                                   \
            (Cur).first_line   = (Cur).last_line   = YYRHSLOC(Rhs, 0).last_line;   \
            (Cur).first_column = (Cur).last_column = YYRHSLOC(Rhs, 0).last_column; \
            (Cur).filename     = "<unknown>";                                      \
        }                                                                          \
    } while (0)
}

%{
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "chinnu.h"

extern int yylineno;
extern int yylex(void);

extern ExpressionList *program;

/* forward */
void yyerror(const char *fmt, ...);
%}

%output "chinnu.tab.c"
%defines "chinnu.tab.h"

%locations
%error-verbose

%union{
    Expression *expr;
    ExpressionList *list;
    int i;
    double d;
    char *s;
}

%token <s> IDENT
%token <i> INTEGER_LITERAL
%token <d> REAL_LITERAL
%token <s> STRING_LITERAL

%token IF THEN ELIF ELSE WHILE DO END FUN VAR VAL

%left '('
%right '='
%left AND OR
%nonassoc EQEQ NEQ '<' LEQ '>' GEQ
%left '+' '-'
%left '*' '/'
%left NOT UNARY

%type <expr> expr lhs
%type <list> program expr_list else_block arg_list arg_list2 param_list param_list2

%start program

%%

program : expr_list                          { program = $1; }
        ;

expr_list : expr_list ';' expr               { $$ = expression_list_append($1, $3); }
          | expr                             { $$ = list1($1); }
          |                                  { $$ = make_list(); }
          ;

expr : IF expr THEN expr_list else_block END { $$ = make_if(@$, $2, $4, $5); }
     | WHILE expr DO expr_list END           { $$ = make_while(@$, $2, $4); }
     | expr '+' expr                         { $$ = make_binop(@$, TYPE_ADD, $1, $3); }
     | expr '-' expr                         { $$ = make_binop(@$, TYPE_SUB, $1, $3); }
     | expr '*' expr                         { $$ = make_binop(@$, TYPE_MUL, $1, $3); }
     | expr '/' expr                         { $$ = make_binop(@$, TYPE_DIV, $1, $3); }
     | '-' expr %prec UNARY                  { $$ = make_uop(@$, TYPE_NEG, $2); }
     | NOT expr                              { $$ = make_uop(@$, TYPE_NOT, $2); }
     | '(' expr ')'                          { $$ = $2; }
     | VAR IDENT '=' expr                    { $$ = make_declaration(@$, $2, $4, 0); }
     | VAL IDENT '=' expr                    { $$ = make_declaration(@$, $2, $4, 1); }
     | lhs '=' expr                          { $$ = make_assignment(@$, $1, $3); }
     | expr EQEQ expr                        { $$ = make_binop(@$, TYPE_EQEQ, $1, $3); }
     | expr NEQ expr                         { $$ = make_binop(@$, TYPE_NEQ, $1, $3); }
     | expr '<' expr                         { $$ = make_binop(@$, TYPE_LT, $1, $3); }
     | expr LEQ expr                         { $$ = make_binop(@$, TYPE_LEQ, $1, $3); }
     | expr '>' expr                         { $$ = make_binop(@$, TYPE_GT, $1, $3); }
     | expr GEQ expr                         { $$ = make_binop(@$, TYPE_GEQ, $1, $3); }
     | expr AND expr                         { $$ = make_binop(@$, TYPE_AND, $1, $3); }
     | expr OR expr                          { $$ = make_binop(@$, TYPE_OR, $1, $3); }
     | lhs                                   { $$ = $1; }
     | INTEGER_LITERAL                       { $$ = make_int(@$, $1); }
     | REAL_LITERAL                          { $$ = make_real(@$, $1); }
     | STRING_LITERAL                        { $$ = make_str(@$, $1); }
     | expr arg_list                         { $$ = make_call(@$, $1, $2); }
     | FUN param_list expr_list END          { $$ = make_func(@$, NULL, $2, $3); }
     | FUN IDENT param_list expr_list END    { $$ = make_func(@$, $2, $3, $4); }
     ;

arg_list : '(' arg_list2 ')'                 { $$ = $2; }
         | '(' ')'                           { $$ = make_list(); }
         ;

arg_list2 : arg_list2 ',' expr               { $$ = expression_list_append($1, $3); }
          | expr                             { $$ = list1($1); }
          ;

param_list : '(' param_list2 ')'             { $$ = $2; }
           | '(' ')'                         { $$ = make_list(); }
           ;

param_list2 : param_list2 ',' IDENT          { $$ = expression_list_append($1, make_declaration(@3, $3, 0, 1)); }
            | IDENT                          { $$ = list1(make_declaration(@1, $1, 0, 1)); }
            ;

lhs : IDENT                                  { $$ = make_varref(@$, $1); }
    ;

else_block : ELSE expr_list                      { $$ = $2; }
           | ELIF expr THEN expr_list else_block { $$ = list1(make_if(@$, $2, $4, $5)); }
           |                                     { $$ = make_list(); }
           ;

%%

void yyerror(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    verror(yylloc, fmt, args);
    va_end(args);
}
