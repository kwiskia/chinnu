%{
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "chinnu.h"

extern int yylineno;
extern int yylex(void);
void yyerror(const char *fmt, ...);
extern NodeList *program;

char *filename = "<unknown>";
int numerrors = 0;
%}

%error-verbose

%union{
    Node *node;
    NodeList *list;
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

%type <node> expr lhs
%type <list> program expr_list else_block arg_list arg_list2 param_list param_list2

%start program

%%

program : expr_list                          { program = $1; }
        ;

expr_list : expr_list ';' expr               { $$ = append($1, $3); }
          | expr                             { $$ = list1($1); }
          |                                  { $$ = make_list(); }
          ;

expr : IF expr THEN expr_list else_block END { $$ = make_if($2, $4, $5); }
     | WHILE expr DO expr_list END           { $$ = make_while($2, $4); }
     | expr '+' expr                         { $$ = make_binop(TYPE_ADD, $1, $3); }
     | expr '-' expr                         { $$ = make_binop(TYPE_SUB, $1, $3); }
     | expr '*' expr                         { $$ = make_binop(TYPE_MUL, $1, $3); }
     | expr '/' expr                         { $$ = make_binop(TYPE_DIV, $1, $3); }
     | '-' expr %prec UNARY                  { $$ = make_uop(TYPE_NEG, $2); }
     | NOT expr                              { $$ = make_uop(TYPE_NOT, $2); }
     | '(' expr ')'                          { $$ = $2; }
     | VAR IDENT '=' expr                    { $$ = make_assignment(make_declaration($2, 0), $4); }
     | VAL IDENT '=' expr                    { $$ = make_assignment(make_declaration($2, 1), $4); }
     | lhs '=' expr                          { $$ = make_assignment($1, $3); }
     | expr EQEQ expr                        { $$ = make_binop(TYPE_EQEQ, $1, $3); }
     | expr NEQ expr                         { $$ = make_binop(TYPE_NEQ, $1, $3); }
     | expr '<' expr                         { $$ = make_binop(TYPE_LT, $1, $3); }
     | expr LEQ expr                         { $$ = make_binop(TYPE_LEQ, $1, $3); }
     | expr '>' expr                         { $$ = make_binop(TYPE_GT, $1, $3); }
     | expr GEQ expr                         { $$ = make_binop(TYPE_GEQ, $1, $3); }
     | expr AND expr                         { $$ = make_binop(TYPE_AND, $1, $3); }
     | expr OR expr                          { $$ = make_binop(TYPE_OR, $1, $3); }
     | lhs                                   { $$ = $1; }
     | INTEGER_LITERAL                       { $$ = make_int($1); }
     | REAL_LITERAL                          { $$ = make_real($1); }
     | STRING_LITERAL                        { $$ = make_str($1); }
     | expr arg_list                         { $$ = make_call($1, $2); }
     | FUN param_list expr_list END          { $$ = make_func($2, $3); }
     ;

arg_list : '(' arg_list2 ')'                 { $$ = $2; }
         | '(' ')'                           { $$ = make_list(); }
         ;

arg_list2 : arg_list2 ',' expr               { $$ = append($1, $3); }
          | expr                             { $$ = list1($1); }
          ;

param_list : '(' param_list2 ')'             { $$ = $2; }
           | '(' ')'                         { $$ = make_list(); }
           ;

param_list2 : param_list2 ',' IDENT          { $$ = append($1, make_declaration($3, 1)); }
            | IDENT                          { $$ = list1(make_declaration($1, 1)); }
            ;

lhs : IDENT                                  { $$ = make_varref($1); }
    ;

else_block : ELSE expr_list                      { $$ = $2; }
           | ELIF expr THEN expr_list else_block { $$ = list1(make_if($2, $4, $5)); }
           |                                     { $$ = make_list(); }
           ;

%%

void yyerror(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "%s:%d: ", filename, yylineno);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    numerrors++;
    if (numerrors >= 10) {
        fprintf(stderr, "Too many errors, aborting.\n");
        exit(1);
    }
}
