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

    struct Node *cond;
    struct Node *lnode;
    struct Node *rnode;
    struct NodeList *llist;
    struct NodeList *rlist;

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

NodeList *program;

void freenode(Node *node);
void freelist(NodeList *list);

NodeList *makelist();
NodeList *list1(Node *node);
NodeList *append(NodeList *list, Node *node);

Node *makeempty();
Node *makeif(Node *cond, NodeList *body, NodeList *orelse);
Node *makewhile(Node *cond, NodeList *body);
Node *makebinop(int type, Node *left, Node *right);
Node *makeuop(int type, Node *left);
Node *makeassignment(Node *varref, Node *value);
Node *makevarref(char *name);
Node *makeint(int i);
Node *makereal(double d);
Node *makestr(char *str);
