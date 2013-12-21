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
    TYPE_STRING,
    TYPE_CALL,
    TYPE_FUNC,
    TYPE_DECLARATION
};

typedef struct Symbol {
    int id;
    char *name;
} Symbol;

typedef struct ScopeItem {
    struct Symbol *symbol;
    struct ScopeItem *next;
} ScopeItem;

typedef struct SymbolItem {
    struct ScopeItem *scope;
    struct SymbolItem *next;
} SymbolItem;

typedef struct SymbolTable {
    struct SymbolItem *top;
} SymbolTable;

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
    struct Symbol *symbol;
    int immutable;
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

void free_node(Node *node);
void free_list(NodeList *list);

NodeList *make_list();
NodeList *list1(Node *node);
NodeList *append(NodeList *list, Node *node);

Node *make_if(Node *cond, NodeList *body, NodeList *orelse);
Node *make_while(Node *cond, NodeList *body);
Node *make_binop(int type, Node *left, Node *right);
Node *make_uop(int type, Node *left);
Node *make_declaration(char *name, int immutable);
Node *make_assignment(Node *varref, Node *value);
Node *make_varref(char *name);
Node *make_int(int i);
Node *make_real(double d);
Node *make_str(char *str);
Node *make_call(Node *target, NodeList *arguments);
Node *make_func(NodeList *parameters, NodeList *body);
