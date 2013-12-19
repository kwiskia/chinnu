%{
#include <stdarg.h>

#include "chinnu.h"
#include "chinnu.tab.h"

extern void yyerror(char *fmt, ...);

static void clearbuffer();
static void appendtobuffer(const char *fmt, ...);

static int token_for(const char *);
static int is_punctuator(const char);

const int MAX_LITERAL = 4096;

static int length = 0;
static char buffer[MAX_LITERAL];
%}

%option noyywrap
%option yylineno
%option stack
%option nounput

whitespace          ([ \t\r\n])
line_comment        ("#".*)
identifier          ([a-zA-Z_][a-zA-Z0-9_]*)

dec_literal         (0|[1-9][0-9]*)
hex_literal         (0[xX][0-9A-fa-f]+)
bin_literal         (0[bB][01]+)
oct_literal         (0[0-7]+)

exponent_part       ([eE][+-]?[0-9]+)
real_literal1       ([0-9]+{exponent_part})
real_literal2       ([0-9]+"."[0-9]+{exponent_part}?)

oct_escape          ("\\"[0-3][0-7]{2})
hex_escape          ("\\"x[0-9A-Fa-f]{2})

%state s1 s2 s3 s4

%%

<INITIAL>{whitespace}       { ; /* ignore */ }
<INITIAL>{line_comment}     { ; /* ignore */ }

<INITIAL>{dec_literal}      { yylval.i = atoi(yytext); return INTEGER_LITERAL; }
<INITIAL>{hex_literal}      { yylval.i = strtol(yytext + 2, 0, 16); return INTEGER_LITERAL; }
<INITIAL>{bin_literal}      { yylval.i = strtol(yytext + 2, 0, 2); return INTEGER_LITERAL; }
<INITIAL>{oct_literal}      { yylval.i = strtol(yytext + 1, 0, 8); return INTEGER_LITERAL; }

<INITIAL>{real_literal1}    { yylval.d = atof(yytext); return REAL_LITERAL; }
<INITIAL>{real_literal2}    { yylval.d = atof(yytext); return REAL_LITERAL; }

<INITIAL>\"                 { BEGIN s1; clearbuffer(); }
<s1>\"                      { BEGIN INITIAL; yylval.s = strdup(buffer); return STRING_LITERAL; }
<s1>[^\"\\\n]+              { appendtobuffer("%s", yytext); }

<INITIAL>'                  { BEGIN s2; clearbuffer(); }
<s2>'                       { BEGIN INITIAL; yylval.s = strdup(buffer); return STRING_LITERAL; }
<s2>[^'\\\n]+               { appendtobuffer("%s", yytext); }

<INITIAL>\"\"\"             { BEGIN s3; clearbuffer(); }
<s3>\"\"\"                  { BEGIN INITIAL; yylval.s = strdup(buffer); return STRING_LITERAL; }
<s3>\"                      { appendtobuffer("\""); }
<s3>[^\\\"]+                { appendtobuffer("%s", yytext); }

<INITIAL>'''                { BEGIN s4; clearbuffer(); }
<s4>'''                     { BEGIN INITIAL; yylval.s = strdup(buffer); return STRING_LITERAL; }
<s4>'                       { appendtobuffer("'"); }
<s4>[^\\\']+                { appendtobuffer("%s", yytext); }

<s1,s2,s3,s4>"\\b"          { appendtobuffer("\b"); }
<s1,s2,s3,s4>"\\t"          { appendtobuffer("\t"); }
<s1,s2,s3,s4>"\\n"          { appendtobuffer("\n"); }
<s1,s2,s3,s4>"\\f"          { appendtobuffer("\f"); }
<s1,s2,s3,s4>"\\r"          { appendtobuffer("\r"); }
<s1,s2,s3,s4>"\\\""         { appendtobuffer("\""); }
<s1,s2,s3,s4>"\\'"          { appendtobuffer("'"); }
<s1,s2,s3,s4>"\\\\"         { appendtobuffer("\\"); }
<s1,s2,s3,s4>{oct_escape}   { appendtobuffer("%c", strtol(yytext + 1, 0, 8)); }
<s1,s2,s3,s4>{hex_escape}   { appendtobuffer("%c", strtol(yytext + 2, 0, 16)); }
<s1,s2,s3,s4>"\\".          { yyerror("Illegal escape sequence.\n"); }
<s1,s2,s3,s4><<EOF>>        { BEGIN INITIAL; yyerror("Unterminated string literal."); }
<s1,s2>\n                   { yyerror("Newlines in string literals are not allowed."); }

<INITIAL>"=="               { return EQEQ; }
<INITIAL>"!="               { return NEQ; }
<INITIAL>"<="               { return LEQ; }
<INITIAL>">="               { return GEQ; }

<INITIAL>{identifier}       { return token_for(yytext); }
<INITIAL>.                  { if (is_punctuator(yytext[0])) return yytext[0]; yyerror("Unknown character.\n"); }

%%

static void clearbuffer() {
    length = 0;
    buffer[0] = '\0';
}

static void appendtobuffer(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    int n = vsnprintf(buffer + length, MAX_LITERAL - length, fmt, args);

    if (n < 0 || n >= MAX_LITERAL) {
        yyerror("String literal too large, truncated.");
    }

    length += n;
}

static struct name_value {
    char *name;
    int value;
} name_value;

static struct name_value keywords[] = {
    {"if", IF},
    {"then", THEN},
    {"elif", ELIF},
    {"else", ELSE},
    {"while", WHILE},
    {"do", DO},
    {"end", END},
    {"and", AND},
    {"or", OR},
    {"not", NOT},
    {"function", FUN}
};

static char punctuators[] = {
    '+', '-', '*', '/',
    '(', ')', '<', '>',
    '=', ',', ';',
};

static int num_keywords = sizeof(keywords) / sizeof(name_value);
static int num_punctuators = sizeof(punctuators) / sizeof(char);

static int token_for(const char *lexeme) {
    int i;
    for (i = 0; i < num_keywords; i++) {
        int cmp = strcmp(keywords[i].name, lexeme);
        if (cmp == 0) {
            return keywords[i].value;
        }
    }

    yylval.s = strdup(yytext);
    return IDENT;
}

static int is_punctuator(const char c) {
    int i;
    for (i = 0; i < num_punctuators; i++) {
        if (punctuators[i] == c) {
            return 1;
        }
    }

    return 0;
}
