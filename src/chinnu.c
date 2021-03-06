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

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <getopt.h>

#include "chinnu.h"
#include "vm.h"
#include "bytecode.h"

extern FILE *yyin;
extern int yyparse();
extern void yylex_destroy();

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

void message(SourcePos pos, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vmessage(pos, fmt, args);
    va_end(args);
}

/*
 * !!! EXTREME HACK !!!
 */

char *get_line(char *filename, int lineno) {
    FILE *fp = fopen(filename, "r");

    if (!fp) {
        fatal("Could not open file for error reporting.");
    }

    char *line = 0;
    size_t len = 0;

    while (getline(&line, &len, fp) != -1) {
        if (--lineno == 0) {
            break;
        }
    }

    fclose(fp);
    return line;
}

void highlight_line(SourcePos pos) {
    if (pos.first_line == pos.last_line) {
        fprintf(stderr, "%d: %s", pos.first_line, get_line(pos.filename, pos.first_line));

        int line = pos.first_line;
        while (line > 0) {
            fprintf(stderr, " ");
            line /= 10;
        }

        fprintf(stderr, "  \033[32m");
        int i;
        for (i = 1; i <= pos.last_column; i++) {
            fprintf(stderr, i < pos.first_column ? " " : "^");
        }

        fprintf(stderr, "\033[30m\n");
    } else {
        int i;
        for (i = pos.first_line; i <= pos.last_line; i++) {
            fprintf(stderr, "%d: %s", i, get_line(pos.filename, i));
        }
    }
}

static int numwarnings = 0;
static int numerrors = 0;

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

    highlight_line(pos);

    numwarnings++;
}

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

    highlight_line(pos);

    numerrors++;
    if (numerrors >= 10) {
        fprintf(stderr, "Too many errors, aborting.\n");
        exit(EXIT_FAILURE);
    }
}

void vmessage(SourcePos pos, const char *fmt, va_list args) {
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

    fprintf(stderr, "note: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\033[0m\n");

    highlight_line(pos);
}

void show_usage(char *program) {
    printf("Usage: %s [switches] ... [files] ...\n", program);
    printf("  -w<type>      display warnings\n");
    printf("  -d            disassemble\n");
    printf("  -c            compile only\n");
    printf("  -o            optimize before running\n");
    printf("  -h --help     display usage and exit\n");
    printf("  -v --version  display version and exit\n");
}

void show_version(char *program) {
    printf("%s v%s\n", program, CHINNU_VERSION);
}

static int help_flag = 0;
static int version_flag = 0;
static int disassemble_flag = 0;
static int compile_flag = 0;
static int optimize_flag = 0;

static struct option options[] = {
    {"help",    no_argument,       &help_flag,    1},
    {"version", no_argument,       &version_flag, 1},
    {"w",       required_argument, 0,             'w'},
    {"d",       no_argument,       0,             'd'},
    {"c",       no_argument,       0,             'c'},
    {"o",       no_argument,       0,             'o'},
    {"h",       no_argument,       0,             'h'},
    {"v",       no_argument,       0,             'v'},
    {0,         0,                 0,             0}
};

char *get_cache_name(char *name) {
    char *cache = malloc((strlen(name) + 2) * sizeof *cache);

    if (!cache) {
        fatal("Out of memory.");
    }

    strcpy(cache, name);
    strcat(cache, ".b");

    return cache;
}

void dump_int(FILE *fp, int i) {
    fwrite(&i, sizeof(int), 1, fp);
}

void dump_real(FILE *fp, double d) {
    fwrite(&d, sizeof(double), 1, fp);
}

void dump_string(FILE *fp, char *s) {
    int n = strlen(s) + 1;
    dump_int(fp, n);
    fwrite(s, sizeof(char), n, fp);
}

void dump_chunk(Chunk *chunk, FILE *fp) {
    // TODO - may be wasteful, check size limits
    dump_int(fp, chunk->numtemps);
    dump_int(fp, chunk->numconstants);
    dump_int(fp, chunk->numinstructions);
    dump_int(fp, chunk->numchildren);
    dump_int(fp, chunk->numlocals);
    dump_int(fp, chunk->numupvars);
    dump_int(fp, chunk->numparams);

    int i;
    for (i = 0; i < chunk->numinstructions; i++) {
        dump_int(fp, chunk->instructions[i]);
    }

    for (i = 0; i < chunk->numconstants; i++) {
        // TODO - wasteful, doesn't need nearly 32 bits
        dump_int(fp, chunk->constants[i]->type);

        switch (chunk->constants[i]->type) {
            case CONST_INT:
            case CONST_BOOL:
                dump_int(fp, chunk->constants[i]->value.i);
                break;

            case CONST_REAL:
                dump_real(fp, chunk->constants[i]->value.d);
                break;

            case CONST_NULL:
                break;

            case CONST_STRING:
            {
                dump_string(fp, chunk->constants[i]->value.s);
            } break;
        }
    }

    for (i = 0; i < chunk->numchildren; i++) {
        dump_chunk(chunk->children[i], fp);
    }
}

void save(Chunk *chunk, char *filename) {
    FILE *fp = fopen(filename, "wb");

    if (!fp) {
        fatal("Could not open bytecode cache file.");
    }

    int mag = MAGIC_BYTE;
    int maj = MAJOR_VERSION;
    int min = MINOR_VERSION;

    dump_int(fp, mag);
    dump_int(fp, maj);
    dump_int(fp, min);

    dump_chunk(chunk, fp);
    fclose(fp);
}

int read_int(FILE *fp) {
    // TODO - will not work on different endian systems
    // Add byte to header and then swap it here

    int n;
    fread(&n, sizeof(int), 1, fp);
    return n;
}

double read_real(FILE *fp) {
    int d;
    fread(&d, sizeof(double), 1, fp);
    return d;
}

char *read_string(FILE *fp) {
    int n = read_int(fp);

    char *s = malloc(n * sizeof *s);

    if (!s) {
        fatal("Out of memory.");
    }

    fread(s, sizeof(char), n, fp);

    return s;
}

Chunk *read_chunk(FILE *fp) {
    Chunk *chunk = malloc(sizeof *chunk);

    if (!chunk) {
        fatal("Out of memory.");
    }

    chunk->numtemps = read_int(fp);
    chunk->numconstants = read_int(fp);
    chunk->numinstructions = read_int(fp);
    chunk->numchildren = read_int(fp);
    chunk->numlocals = read_int(fp);
    chunk->numupvars = read_int(fp);
    chunk->numparams = read_int(fp);

    Constant **constants = malloc(chunk->numconstants * sizeof **constants);
    int *instructions = malloc(chunk->numinstructions * sizeof *instructions);
    Chunk **children = malloc(chunk->numchildren * sizeof **children);

    if (!constants || !instructions || !children) {
        fatal("Out of memory.");
    }

    chunk->constants = constants;
    chunk->instructions = instructions;
    chunk->children = children;

    int i;
    for (i = 0; i < chunk->numinstructions; i++) {
        chunk->instructions[i] = read_int(fp);
    }

    for (i = 0; i < chunk->numconstants; i++) {
        int type = read_int(fp);

        Constant *c = malloc(sizeof *c);

        if (!c) {
            fatal("Out of memory.");
        }

        c->type = type;

        switch (type) {
            case CONST_INT:
            case CONST_BOOL:
                c->value.i = read_int(fp);
                break;

            case CONST_REAL:
                c->value.d = read_real(fp);
                break;

            case CONST_NULL:
                break;

            case CONST_STRING:
            {
                c->value.s = read_string(fp);
            } break;
        }

        chunk->constants[i] = c;
    }

    for (i = 0; i < chunk->numchildren; i++) {
        chunk->children[i] = read_chunk(fp);
    }

    return chunk;
}

void print_const(Constant *c) {
    switch (c->type) {
        case CONST_INT:
            printf("%d", c->value.i);
            break;

        case CONST_REAL:
            printf("%.2f", c->value.d);
            break;

        case CONST_BOOL:
            printf("%s", c->value.i == 1 ? "true" : "false");
            break;

        case CONST_NULL:
            printf("null");
            break;

        case CONST_STRING:
            printf("\"%s\"", c->value.s);
            break;
    }
}

void dis(Chunk *chunk) {
    int i;
    for (i = 0; i < chunk->numinstructions; i++) {
        int instruction = chunk->instructions[i];

        OpCode o = GET_O(instruction);
        int a = GET_A(instruction);
        int b = GET_B(instruction);
        int c = GET_C(instruction);

        switch (o) {
                case OP_RETURN:
                case OP_LEAVE_TRY:
                    printf("%d\t%-15s%d", i + 1, opcode_names[o], b);
                    break;

                case OP_THROW:
                    printf("%d\t%-15s%d", i + 1, opcode_names[0], a);
                    break;

                case OP_MOVE:
                case OP_NEG:
                case OP_NOT:
                {
                    printf("%d\t%-15s%d %d", i + 1, opcode_names[o], a, b);

                    if (b > 255) {
                        printf("\t; b=");
                        print_const(chunk->constants[b - 256]);
                    }
                } break;

                case OP_GETUPVAR:
                case OP_SETUPVAR:
                case OP_CLOSURE:
                    printf("%d\t%-15s%d %d", i + 1, opcode_names[o], a, b);
                    break;

                case OP_JUMP:
                case OP_JUMP_TRUE:
                case OP_JUMP_FALSE:
                {
                    int offset = c == 1 ? -b : b;
                    printf("%d\t%-15s%d %d\t; j=%d", i + 1, opcode_names[o], a, b, i + offset + 2);
                } break;

                case OP_ADD:
                case OP_SUB:
                case OP_MUL:
                case OP_DIV:
                case OP_MOD:
                case OP_POW:
                case OP_EQ:
                case OP_LT:
                case OP_LE:
                {
                    printf("%d\t%-15s%d %d %d", i + 1, opcode_names[o], a, b, c);

                    if (a > 255) {
                        printf("\t; a=");
                        print_const(chunk->constants[a - 256]);
                    }

                    if (b > 255) {
                        printf("\t; b=");
                        print_const(chunk->constants[b - 256]);
                    }
                } break;

                case OP_CALL:
                    printf("%d\t%-15s%d %d %d", i + 1, opcode_names[o], a, b, c);
                    break;

                case OP_ENTER_TRY:
                    printf("%d\t%-15s%d\t; j=%d", i + 1, opcode_names[o], b, i + b + 1);
                    break;
        }

        printf("\n");
    }

    for (i = 0; i < chunk->numchildren; i++) {
        dis(chunk->children[i]);
    }
}

int valid_cache(char *filename) {
    FILE *fp = fopen(filename, "rb");

    if (!fp) {
        fatal("Could not open bytecode cache file.");
    }

    int mag = read_int(fp);
    int maj = read_int(fp);
    int min = read_int(fp);
    fclose(fp);

    if (mag == MAGIC_BYTE) {
        return (maj == MAJOR_VERSION && min == MINOR_VERSION) ? 1 : 2;
    }

    return 0;
}

Chunk *load(char *filename) {
    FILE *fp = fopen(filename, "rb");

    if (!fp) {
        fatal("Could not open bytecode cache file.");
    }

    int mag = read_int(fp);
    int maj = read_int(fp);
    int min = read_int(fp);

    Chunk *c = read_chunk(fp);
    fclose(fp);
    return c;
}

Chunk *make(char *file) {
    // TODO - make non-global
    filename = file;

    FILE *fp = fopen(file, "r");

    if (!fp) {
        printf("Could not open input file '%s'.\n", file);
        exit(EXIT_FAILURE);
    }

    yyin = fp;
    yyparse();
    yylex_destroy();
    fclose(fp);

    resolve(program);

    if (numerrors > 0) {
        exit(EXIT_FAILURE);
    }

    Chunk *chunk = compile(program);
    free_expr(program);

    return chunk;
}

int main(int argc, char **argv) {
    int c;
    int i = 0;

    while ((c = getopt_long(argc, argv, "w:dcohv", options, &i)) != -1) {
        switch (c) {
            case 'w':
            {
                if (strcmp(optarg, "all") == 0) {
                    int i;
                    for (i = 0; i < NUM_WARNING_TYPES; i++) {
                        warning_flags[i] = 1;
                    }
                }

                if (strcmp(optarg, "shadow") == 0) {
                    warning_flags[WARNING_SHADOW] = 1;
                }

                if (strcmp(optarg, "unreachable") == 0) {
                    warning_flags[WARNING_UNREACHABLE] = 1;
                }
            } break;

            case 'd':
                disassemble_flag = 1;
                break;

            case 'c':
                compile_flag = 1;
                break;

            case 'o':
                optimize_flag = 1;
                break;

            case 'h':
                help_flag = 1;
                break;

            case 'v':
                version_flag = 1;
                break;

            case 0:
                /* getopt_long set a flag */
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
        if (compile_flag == 1) {
            Chunk *chunk = make(argv[optind]);
            char *output = get_cache_name(argv[optind]);

            save(chunk, output);

            free(output);
            free_chunk(chunk);
        } else {
            Chunk *chunk;

            int ret = valid_cache(argv[optind]);

            if (ret == 2) {
                fatal("Bytecode cache compiled with a different version.");
            }

            if (ret == 1) {
                chunk = load(argv[optind]);
            } else {
                chunk = make(argv[optind]);
            }

            if (disassemble_flag) {
                dis(chunk);
            } else {
                execute(chunk);
            }

            free_chunk(chunk);
        }
    }

    return EXIT_SUCCESS;
}
