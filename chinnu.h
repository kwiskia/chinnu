extern int yyparse();

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

typedef struct val {
    union {
        double d;
        char *s;
    };
} Val;

typedef struct node {
    unsigned char type;

    // TODO - restructure

    struct node *left;
    struct node *right;
    struct node *cond;
    struct nodelist *body;
    struct nodelist *orelse;

    // TODO - restructure

    struct val *value;
} Node;

#define N ((Node *)0)

typedef struct nodelist {
    struct node *node;
    struct nodelist *next;
} Nodelist;

#define NL ((Nodelist *)0)

Nodelist *program;

void freenode(Node *node);
void freelist(Nodelist *list);

Nodelist *makelist(Node *node);
Nodelist *append(Nodelist *list, Node *node);
Node *makeif(Node *cond, Nodelist *body, Nodelist *orelse);
Node *makewhile(Node *cond, Nodelist *body);
Node *makebinop(int type, Node *left, Node *right);
Node *makeuop(int type, Node *left);
Node *makeassignment(Node *varref, Node *value);
Node *makevarref(char *name);
Node *makenum(double d);
Node *makestr(char *str);
