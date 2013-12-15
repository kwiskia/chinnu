%{
#include "chinnu.h"

extern int yylex(void);
extern int yyerror(const char *);
extern Node *program;
%}

%error-verbose

%union{
    Node *node;
    int i;
    double d;
    char *s;
}

%token <s> IDENTIFIER
%token <i> INTEGER_LITERAL
%token <d> REAL_LITERAL
%token <s> STRING_LITERAL

%token IF THEN ELIF ELSE WHILE DO END

%right '='
%left AND OR
%nonassoc EQEQ NEQ '<' LEQ '>' GEQ
%left '+' '-'
%left '*' '/'
%left NOT UNARY

%type <node> program expr_list expr lhs else_block

%start program

%%

program : expr_list                          { program = $1; }
        ;

expr_list : expr_list ';' expr               { $$ = makeseq($1, $3); }
          | expr                             { $$ = $1; }
          |                                  { $$ = makeempty(); }
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
     ;

lhs : IDENTIFIER { $$ = makevarref($1); }
    ;

else_block : ELSE expr_list                  { $$ = $2; }
           |                                 { $$ = makeempty(); }
           ;

%%
