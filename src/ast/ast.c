#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "ast/ast.h"

// The finished program, filled in by the parser.
Ast_Func *Ast_Program;

// Table of interned string literals, indexed by AST_NODE_KIND_STR slot.
static char *Ast_Strings[MAX_STRINGS];

// Number of entries currently used in Ast_Strings.
static int Ast_NumStrings;

// Locals of the function currently being parsed.
static Ast_Var *Ast_Locals;

// Interns a string literal and returns its table slot.
int Ast_AddString(char *str)
{
    if (Ast_NumStrings >= MAX_STRINGS) {
        error("too many string literals (max %d)", MAX_STRINGS);
    }
    Ast_Strings[Ast_NumStrings] = str;
    return Ast_NumStrings++;
}

// Allocates a zeroed node of the given kind.
Ast_Node *Ast_NewNode(Ast_NodeKind kind)
{
    Ast_Node *node = calloc(1, sizeof(Ast_Node));
    node->an_kind = kind;
    return node;
}

// Builds a binary-operator node with the given operands.
Ast_Node *Ast_NewBinary(Ast_NodeKind kind, Ast_Node *lhs, Ast_Node *rhs)
{
    Ast_Node *node = Ast_NewNode(kind);
    node->an_lhs = lhs;
    node->an_rhs = rhs;
    return node;
}

// Builds a unary-operator node with the given operand.
Ast_Node *Ast_NewUnary(Ast_NodeKind kind, Ast_Node *lhs)
{
    Ast_Node *node = Ast_NewNode(kind);
    node->an_lhs = lhs;
    return node;
}

// Builds an integer-literal node.
Ast_Node *Ast_NewNum(long val)
{
    Ast_Node *node = Ast_NewNode(AST_NODE_KIND_NUM);
    node->an_val = val;
    return node;
}

// Builds a node that references a local variable.
Ast_Node *Ast_NewVarNode(Ast_Var *var)
{
    Ast_Node *node = Ast_NewNode(AST_NODE_KIND_VAR);
    node->an_var = var;
    return node;
}

// Starts a fresh variable scope for a new function.
void Ast_BeginScope(void)
{
    Ast_Locals = NULL;
}

// Looks up a variable by name in the current scope, or NULL.
Ast_Var *Ast_FindVar(const char *name)
{
    for (Ast_Var *var = Ast_Locals; var; var = var->av_next) {
        if (strcmp(var->av_name, name) == 0) {
            return var;
        }
    }
    return NULL;
}

// Declares a variable in the current scope, reusing any existing slot.
Ast_Var *Ast_DeclareVar(const char *name)
{
    Ast_Var *var = Ast_FindVar(name);
    if (var) {
        return var;
    }
    var = calloc(1, sizeof(Ast_Var));
    var->av_name = strdup(name);
    var->av_next = Ast_Locals;
    Ast_Locals = var;
    return var;
}

// Returns the list of locals declared in the current scope.
Ast_Var *Ast_CurrentLocals(void)
{
    return Ast_Locals;
}

// Returns the number of interned string literals.
int Ast_StringCount(void)
{
    return Ast_NumStrings;
}

// Returns the interned string literal in the given slot.
char *Ast_StringAt(int idx)
{
    return Ast_Strings[idx];
}
