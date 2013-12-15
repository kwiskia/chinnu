extern int yyparse();

enum {
    TYPE_EMPTY,
    TYPE_SEQUENCE,
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

typedef struct val {
    union {
        int i;
        double d;
        char *s;
    };
} Val;

typedef struct node {
    unsigned char type;

    int nchildren;
    struct nodelist *children;

    struct val *value;
} Node;

typedef struct nodelist {
    struct node *node;
    struct nodelist *next;
} Nodelist;

Node *program;

void freenode(Node *node);
void freelist(Nodelist *list);

Node *makeempty();
Node *makeseq(Node *parent, Node *child);
Node *makeif(Node *cond, Node *body, Node *orelse);
Node *makewhile(Node *cond, Node *body);
Node *makebinop(int type, Node *left, Node *right);
Node *makeuop(int type, Node *left);
Node *makeassignment(Node *varref, Node *value);
Node *makevarref(char *name);
Node *makeint(int i);
Node *makereal(double d);
Node *makestr(char *str);
