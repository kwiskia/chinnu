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

%token IF THEN ELIF ELSE WHILE DO END FUN

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
          |                                  { $$ = makelist(); }
          | error ';'                        { $$ = 0; yyclearin; yyerrok; }
          ;

expr : IF expr THEN expr_list else_block END { $$ = makeif($2, $4, $5); }
     | WHILE expr DO expr_list END           { $$ = makewhile($2, $4); }
     | expr '+' expr                         { $$ = makebinop(TYPE_ADD, $1, $3); }
     | expr '-' expr                         { $$ = makebinop(TYPE_SUB, $1, $3); }
     | expr '*' expr                         { $$ = makebinop(TYPE_MUL, $1, $3); }
     | expr '/' expr                         { $$ = makebinop(TYPE_DIV, $1, $3); }
     | '-' expr %prec UNARY                  { $$ = makeuop(TYPE_NEG, $2); }
     | NOT expr                              { $$ = makeuop(TYPE_NOT, $2); }
     | '(' expr ')'                          { $$ = $2; }
     | lhs '=' expr                          { $$ = makeassignment($1, $3); }
     | expr EQEQ expr                        { $$ = makebinop(TYPE_EQEQ, $1, $3); }
     | expr NEQ expr                         { $$ = makebinop(TYPE_NEQ, $1, $3); }
     | expr '<' expr                         { $$ = makebinop(TYPE_LT, $1, $3); }
     | expr LEQ expr                         { $$ = makebinop(TYPE_LEQ, $1, $3); }
     | expr '>' expr                         { $$ = makebinop(TYPE_GT, $1, $3); }
     | expr GEQ expr                         { $$ = makebinop(TYPE_GEQ, $1, $3); }
     | expr AND expr                         { $$ = makebinop(TYPE_AND, $1, $3); }
     | expr OR expr                          { $$ = makebinop(TYPE_OR, $1, $3); }
     | lhs                                   { $$ = $1; }
     | INTEGER_LITERAL                       { $$ = makeint($1); }
     | REAL_LITERAL                          { $$ = makereal($1); }
     | STRING_LITERAL                        { $$ = makestr($1); }
     | expr arg_list                         { $$ = makecall($1, $2); }
     | FUN param_list expr_list END          { $$ = makefunc($2, $3); }
     | error END                             { $$ = 0; yyclearin; yyerrok; }
     ;

arg_list : '(' arg_list2 ')'                 { $$ = $2; }
         | '(' ')'                           { $$ = makelist(); }
         ;

arg_list2 : arg_list2 ',' expr               { $$ = append($1, $3); }
          | expr                             { $$ = list1($1); }
          ;

param_list : '(' param_list2 ')'             { $$ = $2; }
           | '(' ')'                         { $$ = makelist(); }
           ;

param_list2 : param_list2 ',' IDENT          { $$ = append($1, makevarref($3)); }
            | IDENT                          { $$ = list1(makevarref($1)); }
            ;

lhs : IDENT                                  { $$ = makevarref($1); }
    ;

else_block : ELSE expr_list                      { $$ = $2; }
           | ELIF expr THEN expr_list else_block { $$ = list1(makeif($2, $4, $5)); }
           |                                     { $$ = makelist(); }
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
