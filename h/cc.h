#ifndef CC_H
#define CC_H

#include <stdio.h>
#include <stdlib.h>

// The kind of an AST node.
typedef enum {
    ND_NUM,       // integer literal
    ND_STR,       // string literal
    ND_VAR,       // a reference to a local variable
    ND_ADD,       // lhs + rhs
    ND_SUB,       // lhs - rhs
    ND_MUL,       // lhs * rhs
    ND_DIV,       // lhs / rhs
    ND_MOD,       // lhs % rhs
    ND_NEG,       // -lhs
    ND_NOT,       // !lhs
    ND_EQ,        // lhs == rhs
    ND_NE,        // lhs != rhs
    ND_LT,        // lhs <  rhs   (> is LT reversed)
    ND_LE,        // lhs <= rhs   (>= is LE reversed)
    ND_AND,       // lhs && rhs
    ND_OR,        // lhs || rhs
    ND_ASSIGN,    // lhs = rhs
    ND_CALL,      // function call
    ND_RETURN,    // return lhs;
    ND_IF,        // if (cond) then; else els;
    ND_FOR,       // for (init; cond; inc) body;
    ND_BLOCK,     // { ... }
    ND_EXPR_STMT, // expression used as a statement
    ND_NOP        // empty statement / bare declaration
} Ast_NodeKind;

// A local variable or function parameter.
typedef struct Ast_Var Ast_Var;
struct Ast_Var {
    Ast_Var *next;       // chains every local in a function
    Ast_Var *param_next; // chains parameters in declaration order
    char    *name;       // identifier as written in the source
    int      offset;     // offset from %rbp, filled in by the back end
};

// A node in the abstract syntax tree.
typedef struct Ast_Node Ast_Node;
struct Ast_Node {
    Ast_NodeKind kind;     // which kind of node this is
    Ast_Node    *next;     // next node in a statement / argument list
    Ast_Node    *lhs;      // generic left operand
    Ast_Node    *rhs;      // generic right operand
    Ast_Node    *cond;     // condition of ND_IF / ND_FOR
    Ast_Node    *then;     // then branch of ND_IF
    Ast_Node    *els;      // else branch of ND_IF
    Ast_Node    *init;     // initialiser of ND_FOR
    Ast_Node    *inc;      // increment of ND_FOR
    Ast_Node    *body;     // statement list for ND_BLOCK / ND_FOR body
    char        *funcname; // callee name for ND_CALL
    Ast_Node    *args;     // argument list for ND_CALL
    long         val;      // integer value for ND_NUM
    int          str_idx;  // string table slot for ND_STR
    Ast_Var     *var;      // referenced variable for ND_VAR
};

// A function definition.
typedef struct Ast_Function Ast_Function;
struct Ast_Function {
    Ast_Function *next;       // next function in the program
    char         *name;       // function name
    Ast_Node     *body;       // function body (ND_BLOCK)
    Ast_Var      *params;     // parameters, in declaration order
    int           nparams;    // number of parameters
    Ast_Var      *locals;     // every local, including parameters
    int           stack_size; // frame size, filled in by the code generator
};

// The finished program, produced by the parser.
extern Ast_Function *Ast_Program;

// Maximum number of distinct string literals in one translation unit.
#define MAX_STRINGS 1024

// Interns a string literal and returns its table slot.
int Ast_AddString(char *s);

// Allocates a zeroed node of the given kind.
Ast_Node *Ast_NewNode(Ast_NodeKind kind);

// Builds a binary-operator node with the given operands.
Ast_Node *Ast_NewBinary(Ast_NodeKind kind, Ast_Node *lhs, Ast_Node *rhs);

// Builds a unary-operator node with the given operand.
Ast_Node *Ast_NewUnary(Ast_NodeKind kind, Ast_Node *lhs);

// Builds an integer-literal node.
Ast_Node *Ast_NewNum(long val);

// Builds a node that references a local variable.
Ast_Node *Ast_NewVarNode(Ast_Var *var);

// Starts a fresh variable scope for a new function.
void Ast_BeginScope(void);

// Looks up a variable by name in the current scope, or NULL.
Ast_Var *Ast_FindVar(const char *name);

// Declares a variable in the current scope, reusing any existing slot.
Ast_Var *Ast_DeclareVar(const char *name);

// Returns the list of locals declared in the current scope.
Ast_Var *Ast_CurrentLocals(void);

// Prints a diagnostic and exits; shared by the lexer, parser and back end.
#define error(...)                      \
    do {                                \
        fprintf(stderr, "cc: error: "); \
        fprintf(stderr, __VA_ARGS__);   \
        fprintf(stderr, "\n");          \
        exit(1);                        \
    } while (0)

// Emits x86-64 assembly (System V AMD64, AT&T syntax) for the program to out.
void Gen_Codegen(FILE *out, Ast_Function *prog);

#endif // CC_H
