/*
 * Copyright (c) 2014, Eric Fritz
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
#include <stdarg.h>
#include <string.h>
#include "chinnu.h"

extern int yylineno;
extern int yylex(void);

extern Expression *program;

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

%token IF THEN ELIF ELSE WHILE DO END FUN VAR VAL TRUE FALSE NIL THROW CATCH

%right '=' ASN_ADD ASN_SUB ASN_MUL ASN_DIV ASN_MOD ASN_POW
%left '('
%left AND OR
%nonassoc EQEQ NEQ '<' LEQ '>' GEQ
%left '+' '-'
%left '*' '/' '%'
%left POW
%left NOT UNARY

%type <expr> program expr block else_block
%type <list> expr_list arg_list arg_list2 param_list param_list2

%start program

%%

program : block                              { program = make_module(@$, $1); }
        ;

block : expr_list                            { $$ = make_block(@$, $1, NULL); }
      | expr_list CATCH expr_list END        { $$ = make_block(@$, $1, $3); }
      ;

expr_list : expr_list ';' expr               { $$ = expression_list_append($1, $3); }
          | expr                             { $$ = list1($1); }
          |                                  { $$ = make_list(); }
          ;

expr : IF expr THEN block else_block END     { $$ = make_if(@$, $2, $4, $5); }
     | WHILE expr DO block END               { $$ = make_while(@$, $2, $4); }
     | expr '+' expr                         { $$ = make_binop(@$, TYPE_ADD, $1, $3); }
     | expr '-' expr                         { $$ = make_binop(@$, TYPE_SUB, $1, $3); }
     | expr '*' expr                         { $$ = make_binop(@$, TYPE_MUL, $1, $3); }
     | expr '/' expr                         { $$ = make_binop(@$, TYPE_DIV, $1, $3); }
     | expr '%' expr                         { $$ = make_binop(@$, TYPE_MOD, $1, $3); }
     | expr POW expr                         { $$ = make_binop(@$, TYPE_POW, $1, $3); }
     | '-' expr %prec UNARY                  { $$ = make_uop(@$, TYPE_NEG, $2); }
     | NOT expr                              { $$ = make_uop(@$, TYPE_NOT, $2); }
     | '(' expr ')'                          { $$ = $2; }
     | VAR IDENT '=' expr                    { $$ = make_declaration(@$, $2, $4, 0); }
     | VAL IDENT '=' expr                    { $$ = make_declaration(@$, $2, $4, 1); }
     | IDENT '=' expr                        { $$ = make_assignment(@$, make_varref(@1, $1), $3); }
     | IDENT ASN_ADD expr                    { $$ = make_assignment(@$, make_varref(@1, $1), make_binop(@$, TYPE_ADD, make_varref(@1, strdup($1)), $3)); }
     | IDENT ASN_SUB expr                    { $$ = make_assignment(@$, make_varref(@1, $1), make_binop(@$, TYPE_SUB, make_varref(@1, strdup($1)), $3)); }
     | IDENT ASN_MUL expr                    { $$ = make_assignment(@$, make_varref(@1, $1), make_binop(@$, TYPE_MUL, make_varref(@1, strdup($1)), $3)); }
     | IDENT ASN_DIV expr                    { $$ = make_assignment(@$, make_varref(@1, $1), make_binop(@$, TYPE_DIV, make_varref(@1, strdup($1)), $3)); }
     | IDENT ASN_MOD expr                    { $$ = make_assignment(@$, make_varref(@1, $1), make_binop(@$, TYPE_MOD, make_varref(@1, strdup($1)), $3)); }
     | IDENT ASN_POW expr                    { $$ = make_assignment(@$, make_varref(@1, $1), make_binop(@$, TYPE_POW, make_varref(@1, strdup($1)), $3)); }
     | expr EQEQ expr                        { $$ = make_binop(@$, TYPE_EQEQ, $1, $3); }
     | expr NEQ expr                         { $$ = make_binop(@$, TYPE_NEQ, $1, $3); }
     | expr '<' expr                         { $$ = make_binop(@$, TYPE_LT, $1, $3); }
     | expr LEQ expr                         { $$ = make_binop(@$, TYPE_LEQ, $1, $3); }
     | expr '>' expr                         { $$ = make_binop(@$, TYPE_GT, $1, $3); }
     | expr GEQ expr                         { $$ = make_binop(@$, TYPE_GEQ, $1, $3); }
     | expr AND expr                         { $$ = make_binop(@$, TYPE_AND, $1, $3); }
     | expr OR expr                          { $$ = make_binop(@$, TYPE_OR, $1, $3); }
     | IDENT                                 { $$ = make_varref(@$, $1); }
     | INTEGER_LITERAL                       { $$ = make_int(@$, $1); }
     | REAL_LITERAL                          { $$ = make_real(@$, $1); }
     | STRING_LITERAL                        { $$ = make_str(@$, $1); }
     | TRUE                                  { $$ = make_bool(@$, 1); }
     | FALSE                                 { $$ = make_bool(@$, 0); }
     | NIL                                   { $$ = make_null(@$); }
     | expr arg_list                         { $$ = make_call(@$, $1, $2); }
     | FUN param_list block END              { $$ = make_func(@$, NULL, $2, $3); }
     | FUN IDENT param_list block END        { $$ = make_func(@$, $2, $3, $4); }
     | DO block END                          { $$ = $2; }
     | THROW STRING_LITERAL                  { $$ = make_throw(@$, make_str(@2, $2)); }
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

else_block : ELSE block                      { $$ = $2; }
           | ELIF expr THEN block else_block { $$ = make_if(@$, $2, $4, $5); }
           |                                 { $$ = NULL; }
           ;

%%

void yyerror(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    verror(yylloc, fmt, args);
    va_end(args);
}
