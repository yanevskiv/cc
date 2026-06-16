/* cc.h - shared declarations for the cc compiler.
 *
 * This single header is shared by the bison parser (lex/parser.y), the flex
 * lexer (lex/lexer.flex) and the C back end (src/cc.c).  It is also pulled into
 * the generated parser.tab.h via a `%code requires` block so that the semantic
 * value union can refer to these types.
 *
 * Naming convention:
 *   Ast_*  abstract syntax tree, the string table and the parser-driven scope
 *   Gen_*  the x86-64 code generator
 */
#ifndef CC_H
#define CC_H

#include <stdio.h>   /* FILE, for Gen_Codegen */

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
} Ast_NodeKind;

/* ------------------------------------------------------------------ */
/* Local variables                                                    */
/* ------------------------------------------------------------------ */
typedef struct Ast_Var Ast_Var;
struct Ast_Var {
    Ast_Var *next;       /* chains every local in a function           */
    Ast_Var *param_next; /* chains parameters in declaration order     */
    char    *name;
    int      offset;     /* offset from %rbp, filled in by the back end */
};

/* ------------------------------------------------------------------ */
/* AST node                                                           */
/* ------------------------------------------------------------------ */
typedef struct Ast_Node Ast_Node;
struct Ast_Node {
    Ast_NodeKind kind;
    Ast_Node    *next;     /* next node in a statement / argument list */

    Ast_Node    *lhs;      /* generic left operand  */
    Ast_Node    *rhs;      /* generic right operand */

    /* control flow */
    Ast_Node    *cond;
    Ast_Node    *then;
    Ast_Node    *els;
    Ast_Node    *init;
    Ast_Node    *inc;
    Ast_Node    *body;     /* statement list for ND_BLOCK */

    /* function call */
    char        *funcname;
    Ast_Node    *args;

    /* leaves */
    long         val;      /* ND_NUM            */
    int          str_idx;  /* ND_STR table slot */
    Ast_Var     *var;      /* ND_VAR            */
};

/* ------------------------------------------------------------------ */
/* Functions                                                          */
/* ------------------------------------------------------------------ */
typedef struct Ast_Function Ast_Function;
struct Ast_Function {
    Ast_Function *next;
    char         *name;
    Ast_Node     *body;        /* ND_BLOCK */
    Ast_Var      *params;      /* parameters, in declaration order */
    int           nparams;
    Ast_Var      *locals;      /* every local, including parameters */
    int           stack_size;  /* filled in by the code generator   */
};

/* ------------------------------------------------------------------ */
/* Whole program (produced by the parser)                             */
/* ------------------------------------------------------------------ */
extern Ast_Function *Ast_Program;

/* String literal table */
#define MAX_STRINGS 1024
extern char *Ast_Strings[MAX_STRINGS];
extern int   Ast_NumStrings;
int          Ast_AddString(char *s);

/* Node / variable constructors (used by the parser actions) */
Ast_Node *Ast_NewNode(Ast_NodeKind kind);
Ast_Node *Ast_NewBinary(Ast_NodeKind kind, Ast_Node *lhs, Ast_Node *rhs);
Ast_Node *Ast_NewUnary(Ast_NodeKind kind, Ast_Node *lhs);
Ast_Node *Ast_NewNum(long val);
Ast_Node *Ast_NewVarNode(Ast_Var *var);

/* Per-function variable scope, driven by the parser */
void     Ast_BeginScope(void);
Ast_Var *Ast_FindVar(const char *name);
Ast_Var *Ast_DeclareVar(const char *name);
Ast_Var *Ast_CurrentLocals(void);

/* Diagnostics (shared by the lexer, parser and back end) */
void error(const char *fmt, ...) __attribute__((noreturn));

/* ------------------------------------------------------------------ */
/* Code generator                                                     */
/* ------------------------------------------------------------------ */
/* Emit x86-64 assembly (System V AMD64, AT&T syntax) for the whole
 * program to `out`. */
void Gen_Codegen(FILE *out, Ast_Function *prog);

#endif /* CC_H */
