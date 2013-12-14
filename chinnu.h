extern int yyparse();

struct nodelist *program;

char *buffer;

enum {
    TYPE_IF,
    TYPE_WHILE,
    TYPE_ADD,
    TYPE_SUB,
    TYPE_MUL,
    TYPE_DIV,
    TYPE_NEG,
    TYPE_NOT,
    TYPE_ASSIGN,
    TYPE_EQEQ,
    TYPE_NEQ,
    TYPE_LT,
    TYPE_LEQ,
    TYPE_GT,
    TYPE_GEQ,
    TYPE_AND,
    TYPE_OR,
    TYPE_VARREF,
    TYPE_NUMBER,
    TYPE_STRING
};

struct val {
    union {
        double d;
        char *s;
    };
};

struct node {
    unsigned char type;

    // TODO - restructure

    struct node *left;
    struct node *right;
    struct node *cond;
    struct nodelist *body;
    struct nodelist *orelse;

    // TODO - restructure

    struct val *value;
};

#define N ((struct node *)0)

struct nodelist {
    struct node *node;
    struct nodelist *next;
};

#define NL ((struct nodelist *)0)

void freenode(struct node *node);
void freelist(struct nodelist *list);

struct nodelist *makelist(struct node *node);
struct nodelist *append(struct nodelist *list, struct node *node);
struct node *makeif(struct node *cond, struct nodelist *body, struct nodelist *orelse);
struct node *makewhile(struct node *cond, struct nodelist *body);
struct node *makebinop(int type, struct node *left, struct node *right);
struct node *makeuop(int type, struct node *left);
struct node *makeassignment(struct node *varref, struct node *value);
struct node *makevarref(char *name);
struct node *makenum(double d);
struct node *makestr(char *str);
