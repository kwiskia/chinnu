%{
#include "chinnu.h"

extern int yylex(void);
extern int yyerror(const char *);
extern struct node *nodelist;
%}

%error-verbose

%union{
    struct node *node;
    struct nodelist *nodelist;
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

%type <nodelist> program expr_list else_block
%type <node> expr

%start program

%%

program : expr_list                          { program = $1; }
        ;

expr_list : expr_list ';' expr               { $$ = append($1, $3); }
          | expr                             { $$ = makelist($1); }
          |                                  { $$ = NL; }
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
     | IDENTIFIER '=' expr                   { $$ = makeassignment(makevarref($1), $3); }
     | expr EQEQ expr                        { $$ = makebinop(TYPE_EQEQ, $1, $3); }
     | expr NEQ expr                         { $$ = makebinop(TYPE_NEQ, $1, $3); }
     | expr '<' expr                         { $$ = makebinop(TYPE_LT, $1, $3); }
     | expr LEQ expr                         { $$ = makebinop(TYPE_LEQ, $1, $3); }
     | expr '>' expr                         { $$ = makebinop(TYPE_GT, $1, $3); }
     | expr GEQ expr                         { $$ = makebinop(TYPE_GEQ, $1, $3); }
     | expr AND expr                         { $$ = makebinop(TYPE_AND, $1, $3); }
     | expr OR expr                          { $$ = makebinop(TYPE_OR, $1, $3); }
     | IDENTIFIER                            { $$ = makevarref($1); }
     | INTEGER_LITERAL                       { $$ = makenum($1); }
     | REAL_LITERAL                          { $$ = makenum($1); }
     | STRING_LITERAL                        { $$ = makestr($1); }
     ;

else_block : ELSE expr_list                  { $$ = $2; }
           |                                 { $$ = NL; }
           ;

%%
