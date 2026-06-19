#ifndef AST_H
#define AST_H

// Maximum number of distinct string literals in one translation unit.
#define MAX_STRINGS 1024

// The kind of an AST node.
typedef enum {
    AST_NODE_KIND_NUM,       // integer literal
    AST_NODE_KIND_STR,       // string literal
    AST_NODE_KIND_VAR,       // a reference to a local variable
    AST_NODE_KIND_ADD,       // lhs + rhs
    AST_NODE_KIND_SUB,       // lhs - rhs
    AST_NODE_KIND_MUL,       // lhs * rhs
    AST_NODE_KIND_DIV,       // lhs / rhs
    AST_NODE_KIND_MOD,       // lhs % rhs
    AST_NODE_KIND_NEG,       // -lhs
    AST_NODE_KIND_NOT,       // !lhs
    AST_NODE_KIND_EQ,        // lhs == rhs
    AST_NODE_KIND_NE,        // lhs != rhs
    AST_NODE_KIND_LT,        // lhs <  rhs   (> is LT reversed)
    AST_NODE_KIND_LE,        // lhs <= rhs   (>= is LE reversed)
    AST_NODE_KIND_AND,       // lhs && rhs
    AST_NODE_KIND_OR,        // lhs || rhs
    AST_NODE_KIND_ASSIGN,    // lhs = rhs
    AST_NODE_KIND_CALL,      // function call
    AST_NODE_KIND_RETURN,    // return lhs;
    AST_NODE_KIND_IF,        // if (cond) then; else els;
    AST_NODE_KIND_FOR,       // for (init; cond; inc) body;
    AST_NODE_KIND_BLOCK,     // { ... }
    AST_NODE_KIND_EXPR_STMT, // expression used as a statement
    AST_NODE_KIND_NOP        // empty statement / bare declaration
} Ast_NodeKind;

// A local variable or function parameter.
typedef struct Ast_Var Ast_Var;
struct Ast_Var {
    Ast_Var *av_next;       // chains every local in a function
    Ast_Var *av_param_next; // chains parameters in declaration order
    char    *av_name;       // identifier as written in the source
    int      av_offset;     // offset from %rbp, filled in by the back end
};

// A node in the abstract syntax tree.
typedef struct Ast_Node Ast_Node;
struct Ast_Node {
    Ast_NodeKind an_kind;     // which kind of node this is
    Ast_Node    *an_next;     // next node in a statement / argument list
    Ast_Node    *an_lhs;      // generic left operand
    Ast_Node    *an_rhs;      // generic right operand
    Ast_Node    *an_cond;     // condition of AST_NODE_KIND_IF / AST_NODE_KIND_FOR
    Ast_Node    *an_then;     // then branch of AST_NODE_KIND_IF
    Ast_Node    *an_els;      // else branch of AST_NODE_KIND_IF
    Ast_Node    *an_init;     // initialiser of AST_NODE_KIND_FOR
    Ast_Node    *an_inc;      // increment of AST_NODE_KIND_FOR
    Ast_Node    *an_body;     // statement list for AST_NODE_KIND_BLOCK / FOR body
    char        *an_funcname; // callee name for AST_NODE_KIND_CALL
    Ast_Node    *an_args;     // argument list for AST_NODE_KIND_CALL
    long         an_val;      // integer value for AST_NODE_KIND_NUM
    int          an_str_idx;  // string table slot for AST_NODE_KIND_STR
    Ast_Var     *an_var;      // referenced variable for AST_NODE_KIND_VAR
};

// A function definition.
typedef struct Ast_Func Ast_Func;
struct Ast_Func {
    Ast_Func *af_next;       // next function in the program
    char     *af_name;       // function name
    Ast_Node *af_body;       // function body (AST_NODE_KIND_BLOCK)
    Ast_Var  *af_params;     // parameters, in declaration order
    int       af_nparams;    // number of parameters
    Ast_Var  *af_locals;     // every local, including parameters
    int       af_stack_size; // frame size, filled in by the code generator
};

// The finished program, produced by the parser.
extern Ast_Func *Ast_Program;

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

// Returns the number of interned string literals.
int Ast_StringCount(void);

// Returns the interned string literal in the given slot.
char *Ast_StringAt(int idx);

#endif // AST_H
