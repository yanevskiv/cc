/* ast.h - Abstract syntax tree and shared declarations for the cc compiler.
 *
 * This header is shared by the bison parser (lex/parser.y), the flex lexer
 * (lex/lexer.flex) and the C back end in src/.  It is also pulled into the
 * generated parser.tab.h via a `%code requires` block so that the semantic
 * value union can refer to these types.
 */
#ifndef CC_AST_H
#define CC_AST_H

/* ------------------------------------------------------------------ */
/* AST node kinds                                                     */
/* ------------------------------------------------------------------ */
typedef enum {
    ND_NUM,       /* integer literal                  */
    ND_STR,       /* string literal                   */
    ND_VAR,       /* a reference to a local variable  */
    ND_ADD,       /* lhs + rhs                        */
    ND_SUB,       /* lhs - rhs                        */
    ND_MUL,       /* lhs * rhs                        */
    ND_DIV,       /* lhs / rhs                        */
    ND_MOD,       /* lhs % rhs                        */
    ND_NEG,       /* -lhs                             */
    ND_NOT,       /* !lhs                             */
    ND_EQ,        /* lhs == rhs                       */
    ND_NE,        /* lhs != rhs                       */
    ND_LT,        /* lhs <  rhs   (> is LT reversed)  */
    ND_LE,        /* lhs <= rhs   (>= is LE reversed) */
    ND_AND,       /* lhs && rhs                       */
    ND_OR,        /* lhs || rhs                       */
    ND_ASSIGN,    /* lhs = rhs                        */
    ND_CALL,      /* function call                    */
    ND_RETURN,    /* return lhs;                      */
    ND_IF,        /* if (cond) then; else els;        */
    ND_FOR,       /* for (init; cond; inc) body;      */
    ND_BLOCK,     /* { ... }                          */
    ND_EXPR_STMT, /* expression used as a statement   */
    ND_NOP        /* empty statement / bare declaration */
} NodeKind;

/* ------------------------------------------------------------------ */
/* Local variables                                                    */
/* ------------------------------------------------------------------ */
typedef struct Var Var;
struct Var {
    Var  *next;       /* chains every local in a function           */
    Var  *param_next; /* chains parameters in declaration order     */
    char *name;
    int   offset;     /* offset from %rbp, filled in by the back end */
};

/* ------------------------------------------------------------------ */
/* AST node                                                           */
/* ------------------------------------------------------------------ */
typedef struct Node Node;
struct Node {
    NodeKind kind;
    Node    *next;     /* next node in a statement / argument list */

    Node    *lhs;      /* generic left operand  */
    Node    *rhs;      /* generic right operand */

    /* control flow */
    Node    *cond;
    Node    *then;
    Node    *els;
    Node    *init;
    Node    *inc;
    Node    *body;     /* statement list for ND_BLOCK */

    /* function call */
    char    *funcname;
    Node    *args;

    /* leaves */
    long     val;      /* ND_NUM            */
    int      str_idx;  /* ND_STR table slot */
    Var     *var;      /* ND_VAR            */
};

/* ------------------------------------------------------------------ */
/* Functions                                                          */
/* ------------------------------------------------------------------ */
typedef struct Function Function;
struct Function {
    Function *next;
    char     *name;
    Node     *body;        /* ND_BLOCK */
    Var      *params;      /* parameters, in declaration order */
    int       nparams;
    Var      *locals;      /* every local, including parameters */
    int       stack_size;  /* filled in by the code generator   */
};

/* ------------------------------------------------------------------ */
/* Whole program (produced by the parser)                             */
/* ------------------------------------------------------------------ */
extern Function *program;

/* String literal table */
#define MAX_STRINGS 1024
extern char *strings[MAX_STRINGS];
extern int   num_strings;
int          add_string(char *s);

/* Node / variable constructors (used by the parser actions) */
Node *new_node(NodeKind kind);
Node *new_binary(NodeKind kind, Node *lhs, Node *rhs);
Node *new_unary(NodeKind kind, Node *lhs);
Node *new_num(long val);
Node *new_var_node(Var *var);

/* Per-function variable scope, driven by the parser */
void begin_scope(void);
Var *find_var(const char *name);
Var *declare_var(const char *name);
Var *current_locals(void);

/* Diagnostics */
void error(const char *fmt, ...) __attribute__((noreturn));

#endif /* CC_AST_H */
