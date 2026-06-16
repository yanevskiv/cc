/* ast.c - AST/variable constructors, the string table and the per-function
 * variable scope used by the parser actions. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

/* The finished program, filled in by the parser. */
Function *program;

/* ------------------------------------------------------------------ */
/* String literal table                                               */
/* ------------------------------------------------------------------ */
char *strings[MAX_STRINGS];
int   num_strings;

int add_string(char *s)
{
    if (num_strings >= MAX_STRINGS)
        error("too many string literals (max %d)", MAX_STRINGS);
    strings[num_strings] = s;
    return num_strings++;
}

/* ------------------------------------------------------------------ */
/* Node constructors                                                  */
/* ------------------------------------------------------------------ */
Node *new_node(NodeKind kind)
{
    Node *n = calloc(1, sizeof(Node));
    n->kind = kind;
    return n;
}

Node *new_binary(NodeKind kind, Node *lhs, Node *rhs)
{
    Node *n = new_node(kind);
    n->lhs = lhs;
    n->rhs = rhs;
    return n;
}

Node *new_unary(NodeKind kind, Node *lhs)
{
    Node *n = new_node(kind);
    n->lhs = lhs;
    return n;
}

Node *new_num(long val)
{
    Node *n = new_node(ND_NUM);
    n->val = val;
    return n;
}

Node *new_var_node(Var *var)
{
    Node *n = new_node(ND_VAR);
    n->var = var;
    return n;
}

/* ------------------------------------------------------------------ */
/* Per-function variable scope                                        */
/* ------------------------------------------------------------------ */
static Var *locals;   /* locals of the function being parsed */

void begin_scope(void)
{
    locals = NULL;
}

Var *find_var(const char *name)
{
    for (Var *v = locals; v; v = v->next)
        if (strcmp(v->name, name) == 0)
            return v;
    return NULL;
}

Var *declare_var(const char *name)
{
    Var *v = find_var(name);
    if (v)
        return v;            /* re-declaration: reuse the existing slot */
    v = calloc(1, sizeof(Var));
    v->name = strdup(name);
    v->next = locals;
    locals  = v;
    return v;
}

Var *current_locals(void)
{
    return locals;
}

/* ------------------------------------------------------------------ */
/* Diagnostics                                                        */
/* ------------------------------------------------------------------ */
void error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fputs("cc: error: ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    exit(1);
}
