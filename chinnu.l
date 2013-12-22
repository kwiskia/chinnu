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

%{
#include <stdarg.h>

#include "chinnu.h"
#include "chinnu.tab.h"

extern char *filename;
extern void yyerror(char *fmt, ...);

static void clear_buffer();
static void append_to_buffer(const char *fmt, ...);

static int token_for(const char *);
static int is_punctuator(const char);

#define MAX_LITERAL 4096

static int length = 0;
static char buffer[MAX_LITERAL];

int yycolumn = 1;

#define YY_USER_ACTION                          \
    yylloc.filename = filename;                 \
    yylloc.first_line = yylineno;               \
    yylloc.last_line = yylineno;                \
    yylloc.first_column = yycolumn;             \
    yylloc.last_column = yycolumn + yyleng - 1; \
    yycolumn += yyleng;
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

%state s1 s2 s3 s4 s5 s6 s7 s8

%%

<INITIAL>\n                 { yycolumn = 1; }
<INITIAL>{whitespace}       { ; /* ignore */ }
<INITIAL>{line_comment}     { ; /* ignore */ }

<INITIAL>{dec_literal}      { yylval.i = atoi(yytext); return INTEGER_LITERAL; }
<INITIAL>{hex_literal}      { yylval.i = strtol(yytext + 2, 0, 16); return INTEGER_LITERAL; }
<INITIAL>{bin_literal}      { yylval.i = strtol(yytext + 2, 0, 2); return INTEGER_LITERAL; }
<INITIAL>{oct_literal}      { yylval.i = strtol(yytext + 1, 0, 8); return INTEGER_LITERAL; }

<INITIAL>{real_literal1}    { yylval.d = atof(yytext); return REAL_LITERAL; }
<INITIAL>{real_literal2}    { yylval.d = atof(yytext); return REAL_LITERAL; }

<INITIAL>\"                 { BEGIN s1; clear_buffer(); }
<s1>\"                      { BEGIN INITIAL; yylval.s = strdup(buffer); return STRING_LITERAL; }
<s1>[^\"\\\n]+              { append_to_buffer("%s", yytext); }

<INITIAL>r\"                { BEGIN s5; clear_buffer(); }
<s5>\"                      { BEGIN INITIAL; yylval.s = strdup(buffer); return STRING_LITERAL; }
<s5>[^\"\n]+                { append_to_buffer("%s", yytext); }

<INITIAL>'                  { BEGIN s2; clear_buffer(); }
<s2>'                       { BEGIN INITIAL; yylval.s = strdup(buffer); return STRING_LITERAL; }
<s2>[^'\\\n]+               { append_to_buffer("%s", yytext); }

<INITIAL>r'                 { BEGIN s6; clear_buffer(); }
<s6>'                       { BEGIN INITIAL; yylval.s = strdup(buffer); return STRING_LITERAL; }
<s6>[^'\n]+                 { append_to_buffer("%s", yytext); }

<INITIAL>\"\"\"             { BEGIN s3; clear_buffer(); }
<s3>\"\"\"                  { BEGIN INITIAL; yylval.s = strdup(buffer); return STRING_LITERAL; }
<s3>\"                      { append_to_buffer("\""); }
<s3>\n                      { append_to_buffer("\n"); yycolumn = 1; }
<s3>[^\\\"\n]+              { append_to_buffer("%s", yytext); }

<INITIAL>r\"\"\"            { BEGIN s7; clear_buffer(); }
<s7>\"\"\"                  { BEGIN INITIAL; yylval.s = strdup(buffer); return STRING_LITERAL; }
<s7>\"                      { append_to_buffer("\""); }
<s7>\n                      { append_to_buffer("\n"); yycolumn = 1; }
<s7>[^\"\n]+                { append_to_buffer("%s", yytext); }

<INITIAL>'''                { BEGIN s4; clear_buffer(); }
<s4>'''                     { BEGIN INITIAL; yylval.s = strdup(buffer); return STRING_LITERAL; }
<s4>'                       { append_to_buffer("'"); }
<s4>\n                      { append_to_buffer("\n"); yycolumn = 1; }
<s4>[^\\\'\n]+              { append_to_buffer("%s", yytext); }

<INITIAL>r'''               { BEGIN s8; clear_buffer(); }
<s8>'''                     { BEGIN INITIAL; yylval.s = strdup(buffer); return STRING_LITERAL; }
<s8>'                       { append_to_buffer("'"); }
<s8>\n                      { append_to_buffer("\n"); yycolumn = 1; }
<s8>[^\'\n]+                { append_to_buffer("%s", yytext); }

<s1,s2,s3,s4>"\\b"          { append_to_buffer("\b"); }
<s1,s2,s3,s4>"\\t"          { append_to_buffer("\t"); }
<s1,s2,s3,s4>"\\n"          { append_to_buffer("\n"); }
<s1,s2,s3,s4>"\\f"          { append_to_buffer("\f"); }
<s1,s2,s3,s4>"\\r"          { append_to_buffer("\r"); }
<s1,s2,s3,s4>"\\\""         { append_to_buffer("\""); }
<s1,s2,s3,s4>"\\'"          { append_to_buffer("'"); }
<s1,s2,s3,s4>"\\\\"         { append_to_buffer("\\"); }
<s1,s2,s3,s4>{oct_escape}   { append_to_buffer("%c", strtol(yytext + 1, 0, 8)); }
<s1,s2,s3,s4>{hex_escape}   { append_to_buffer("%c", strtol(yytext + 2, 0, 16)); }
<s1,s2,s3,s4>"\\".          { yyerror("Illegal escape sequence."); }
<s1,s2,s3,s4><<EOF>>        { BEGIN INITIAL; yyerror("Unterminated string literal."); }
<s5,s6,s7,s8><<EOF>>        { BEGIN INITIAL; yyerror("Unterminated string literal."); }
<s1,s2>\n                   { yycolumn = 1; yyerror("Newlines in string literals are not allowed."); }

<INITIAL>"=="               { return EQEQ; }
<INITIAL>"!="               { return NEQ; }
<INITIAL>"<="               { return LEQ; }
<INITIAL>">="               { return GEQ; }

<INITIAL>{identifier}       { int token = token_for(yytext); if (token == IDENT) { yylval.s = strdup(yytext); } return token; }
<INITIAL>.                  { if (is_punctuator(yytext[0])) return yytext[0]; yyerror("Unknown character."); }

%%

static void clear_buffer() {
    length = 0;
    buffer[0] = '\0';
}

static void append_to_buffer(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    int n = vsnprintf(buffer + length, MAX_LITERAL - length, fmt, args);

    va_end(args);

    if (n < 0 || length + n >= MAX_LITERAL) {
        length = MAX_LITERAL;
        yyerror("String literal too large, truncated.");
    } else {
        length += n;
    }
}

static struct {
    char *name;
    int value;
} keywords[] = {
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
    {"function", FUN},
    {"var", VAR},
    {"val", VAL}
};

static int token_for(const char *lexeme) {
    int i;
    int n = sizeof(keywords) / sizeof(keywords[0]);

    for (i = 0; i < n; i++) {
        int cmp = strcmp(keywords[i].name, lexeme);
        if (cmp == 0) {
            return keywords[i].value;
        }
    }

    return IDENT;
}

static char punctuators[] = {
    '+', '-', '*', '/',
    '(', ')', '<', '>',
    '=', ',', ';',
};

static int is_punctuator(const char c) {
    int i;
    int n = sizeof(punctuators) / sizeof(punctuators[0]);

    for (i = 0; i < n; i++) {
        if (punctuators[i] == c) {
            return 1;
        }
    }

    return 0;
}
