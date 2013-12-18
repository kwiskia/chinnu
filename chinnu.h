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

typedef struct Val {
    union {
        int i;
        double d;
        char *s;
    };
} Val;

typedef struct Node {
    unsigned char type;

    int nchildren;
    struct NodeList *children;

    struct Val *value;
} Node;

typedef struct ListItem {
    struct Node *node;
    struct ListItem *next;
} ListItem;

typedef struct NodeList {
    struct ListItem *head;
    struct ListItem *tail;
} NodeList;

Node *program;

void freenode(Node *node);
void freelist(NodeList *list);

NodeList *makelist();
void append(NodeList *list, Node *node);

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
