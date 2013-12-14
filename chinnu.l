%{
#include <stdlib.h>
#include <string.h>

#include "chinnu.h"
#include "chinnu.tab.h"

extern int yyerror(const char *);

static void clearbuffer();
static char *clonebuffer();
static void appendtobuffer(const char *);

static int token_for(const char *);
static int is_punctuator(const char);
%}

%option noyywrap
%option yylineno
%option stack
%option nounput

whitespace          ([ \t\r\n])
line_comment        ("#".*)
integer_literal     ([0-9]+)
real_literal        ([0-9]+"."[0-9]+)
identifier          ([a-zA-Z_][a-zA-Z0-9_]*)

%state s1 s2 s3 s4

%%

<INITIAL>{whitespace}       { ; /* ignore */ }
<INITIAL>{line_comment}     { ; /* ignore */ }

<INITIAL>{integer_literal}  { yylval.i = atoi(yytext); return INTEGER_LITERAL; }
<INITIAL>{real_literal}     { yylval.d = atof(yytext); return REAL_LITERAL; }

<INITIAL>\"                 { BEGIN s1; clearbuffer(); }
<s1>\"                      { BEGIN INITIAL; yylval.s = clonebuffer(); return STRING_LITERAL; }
<s1>[^\"\\\n]+              { appendtobuffer(yytext); }

<INITIAL>'                  { BEGIN s2; clearbuffer(); }
<s2>'                       { BEGIN INITIAL; yylval.s = clonebuffer(); return STRING_LITERAL; }
<s2>[^'\\\n]+               { appendtobuffer(yytext); }

<INITIAL>\"\"\"             { BEGIN s3; clearbuffer(); }
<s3>\"\"\"                  { BEGIN INITIAL; yylval.s = clonebuffer(); return STRING_LITERAL; }
<s3>\"                      { appendtobuffer("\""); }
<s3>[^\\\"]+                { appendtobuffer(yytext); }

<INITIAL>'''                { BEGIN s4; clearbuffer(); }
<s4>'''                     { BEGIN INITIAL; yylval.s = clonebuffer(); return STRING_LITERAL; }
<s4>'                       { appendtobuffer("'"); }
<s4>[^\\\']+                { appendtobuffer(yytext); }

<s1,s2,s3,s4>"\\t"          { appendtobuffer("\t"); }
<s1,s2,s3,s4>"\\r"          { appendtobuffer("\r"); }
<s1,s2,s3,s4>"\\n"          { appendtobuffer("\n"); }
<s1,s2,s3,s4>"\\\""         { appendtobuffer("\""); }
<s1,s2,s3,s4>"\\'"          { appendtobuffer("'"); }
<s1,s2,s3,s4>"\\\\"         { appendtobuffer("\\"); }
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

static struct strbuffer {
    char *s;
    size_t size;
    size_t capacity;
} strbuffer;

static struct strbuffer buffer = {0, 0, 0};

static void clearbuffer() {
    // TODO - free buffer after lexing

    if (!buffer.s) {
        buffer.s = malloc(16);
        buffer.capacity = 16;
    }

    buffer.size = 0;
}

static char *clonebuffer() {
    char *str = malloc(buffer.size + 1);

    if (!str) {
        yyerror("Could not allocate memory for string literal.");
    }

    memcpy(str, buffer.s, buffer.size);
    str[buffer.size] = 0;

    return str;
}

static void appendtobuffer(const char *str) {
    size_t len = strlen(str);
    size_t min = buffer.size + len;

    if (buffer.capacity < min) {
        size_t newcapacity = buffer.capacity * 2;

        if (newcapacity < min) {
            newcapacity = min;
        }

        buffer.s = realloc(buffer.s, newcapacity);
        buffer.capacity = newcapacity;

        if (!buffer.s) {
            yyerror("String literal too large.");
            exit(1);
        }
    }

    sprintf(buffer.s + buffer.size, "%s", str);
    buffer.size += len;
}

static struct name_value {
    char *name;
    int value;
} name_value;

static struct name_value keywords[] = {
    {"if", IF},
    {"then", THEN},
    {"else", ELSE},
    {"while", WHILE},
    {"do", DO},
    {"end", END},
    {"and", AND},
    {"or", OR},
    {"not", NOT},
};

static char punctuators[] = {
    '+', '-', '*', '/',
    '(', ')', '=', ';',
    '<', '>'
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
    return IDENTIFIER;
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

int yyerror(const char *msg) {
    fprintf(stderr, "%d: lexical error (%s) [%s]\n", yylineno, msg, yytext);
    return 0;
}
