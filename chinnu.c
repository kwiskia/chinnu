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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <getopt.h>

#include "chinnu.h"
#include "symbol.h"

extern FILE *yyin;
extern int yyparse();
extern void yylex_destroy();

/* for debugging */

/* forward */
void expression_list_print(ExpressionList *list, int indent);

void expression_print(Expression *expr, int indent) {
    if (expr) {
        int i;
        for (i = 0; i < indent; i++) {
            printf("\t");
        }

        printf("%s:%d.%d-%d.%d: ",
            expr->pos.filename,
            expr->pos.first_line, expr->pos.first_column,
            expr->pos.last_line, expr->pos.last_column);

        switch (expr->type) {
            case TYPE_VARREF:
                printf("[ref %d]\n", expr->symbol->id);
                break;

            case TYPE_DECLARATION:
                printf("[decl %d]\n", expr->symbol->id);
                break;

            case TYPE_NUMBER:
                printf("[%.2f or %d]\n", expr->value->d, expr->value->i);
                break;

            case TYPE_STRING:
                printf("[\"%s\"]\n", expr->value->s);
                break;

            default:
                printf("[type %d]\n", expr->type);
        }

        if (expr->cond) expression_print(expr->cond, indent + 1);
        if (expr->lexpr) expression_print(expr->lexpr, indent + 1);
        if (expr->rexpr) expression_print(expr->rexpr, indent + 1);
        if (expr->llist) expression_list_print(expr->llist, indent + 1);
        if (expr->rlist) expression_list_print(expr->rlist, indent + 1);
    }
}

void expression_list_print(ExpressionList *list, int indent) {
    if (list) {
        ExpressionNode *head = list->head;

        while (head) {
            expression_print(head->expr, indent);
            head = head->next;
        }
    }
}

void fatal(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "Internal compiler error: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    va_end(args);
    exit(EXIT_FAILURE);
}

void warning(SourcePos pos, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vwarning(pos, fmt, args);
    va_end(args);
}

void error(SourcePos pos, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    verror(pos, fmt, args);
    va_end(args);
}

void vwarning(SourcePos pos, const char *fmt, va_list args) {
    fprintf(stderr, "\033[1m");

    if (pos.first_line < pos.last_line) {
        fprintf(stderr, "%s:%d.%d-%d.%d: ",
            pos.filename,
            pos.first_line, pos.first_column,
            pos.last_line, pos.last_column);
    } else if (pos.first_column < pos.last_column) {
        fprintf(stderr, "%s:%d.%d-%d: ",
            pos.filename,
            pos.first_line, pos.first_column,
            pos.last_column);
    } else {
        fprintf(stderr, "%s:%d.%d: ",
            pos.filename,
            pos.first_line, pos.first_column);
    }

    fprintf(stderr, "\033[1;35mwarning:\033[1;30m ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\033[0m\n");
}

static int numerrors = 0;

void verror(SourcePos pos, const char *fmt, va_list args) {
    fprintf(stderr, "\033[1m");

    if (pos.first_line < pos.last_line) {
        fprintf(stderr, "%s:%d.%d-%d.%d: ",
            pos.filename,
            pos.first_line, pos.first_column,
            pos.last_line, pos.last_column);
    } else if (pos.first_column < pos.last_column) {
        fprintf(stderr, "%s:%d.%d-%d: ",
            pos.filename,
            pos.first_line, pos.first_column,
            pos.last_column);
    } else {
        fprintf(stderr, "%s:%d.%d: ",
            pos.filename,
            pos.first_line, pos.first_column);
    }

    fprintf(stderr, "\033[1;31merror:\033[1;30m ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\033[0m\n");

    numerrors++;
    if (numerrors >= 10) {
        fprintf(stderr, "Too many errors, aborting.\n");
        exit(EXIT_FAILURE);
    }
}

void show_usage(char *program) {
    printf("Usage: %s [switches] ... [files] ...\n", program);
    printf("  -d --debug    display generated AST\n");
    printf("  -h --help     display usage and exit\n");
    printf("  -v --version  display version and exit\n");
}

void show_version(char *program) {
    printf("%s version %s.\n", program, CHINNU_VERSION);
}

static int debug_flag;
static int help_flag;
static int version_flag;

static struct option options[] = {
    {"debug",   no_argument,       &debug_flag,   1},
    {"help",    no_argument,       &help_flag,    1},
    {"version", no_argument,       &version_flag, 1},
    {"d",       no_argument,       0,             'd'},
    {"h",       no_argument,       0,             'h'},
    {"v",       no_argument,       0,             'v'},
    {0,         0,                 0,             0}
};

int main(int argc, char **argv) {
    int c;
    int i = 0;

    while ((c = getopt_long(argc, argv, "dhv", options, &i)) != -1) {
        switch (c) {
            case 0:
                /* getopt_long set a flag */
                break;

            case 'd':
                debug_flag = 1;
                break;

            case 'h':
                help_flag = 1;
                break;

            case 'v':
                version_flag = 1;
                break;

            default:
                /* unrecognized option */
                return EXIT_FAILURE;
        }
    }

    if (help_flag) {
        show_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    if (version_flag) {
        show_version(argv[0]);
        return EXIT_SUCCESS;
    }

    if (optind == argc) {
        printf("No files supplied.\n");
        return EXIT_FAILURE;
    }

    for ( ; optind < argc; optind++) {
        filename = argv[optind];

        FILE *fp = fopen(filename, "r");

        if (!fp) {
            printf("Could not open input file '%s'.\n", filename);
            return EXIT_FAILURE;
        }

        yyin = fp;
        yyparse();
        yylex_destroy();
        fclose(fp);

        resolve(program);

        if (debug_flag) {
            expression_list_print(program, 0);
        }

        free_expression_list(program);
    }

    return EXIT_SUCCESS;
}
