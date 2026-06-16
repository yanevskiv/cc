/* parser.y - grammar for the cc compiler (a small subset of C).
 *
 * Generates build/parser.tab.c and build/parser.tab.h
 * (via `bison -d -o build/parser.tab.c`).
 *
 * The grammar is deliberately flat: operator precedence is resolved through
 * the %left / %right declarations below rather than through a tower of
 * non-terminals, which keeps the grammar short and easy to extend.
 */

%code requires {
    #include "ast.h"
}

%{
#include <stdio.h>
#include <stdlib.h>
#include "ast.h"

int  yylex(void);
extern int yylineno;
void yyerror(const char *s);

/* State for the function definition currently being parsed. */
static char *cur_func_name;
static Var  *cur_params;
static Var  *cur_params_tail;
static int   cur_nparams;

/* The program is assembled here as functions are reduced. */
static Function *prog_head, *prog_tail;

static void add_param(Var *v)
{
    v->param_next = NULL;
    if (!cur_params) cur_params = cur_params_tail = v;
    else { cur_params_tail->param_next = v; cur_params_tail = v; }
    cur_nparams++;
}

static void add_function(Function *fn)
{
    fn->next = NULL;
    if (!prog_head) prog_head = prog_tail = fn;
    else { prog_tail->next = fn; prog_tail = fn; }
    program = prog_head;
}
%}

%union {
    long  num;
    char *str;
    Node *node;
}

%token <num> NUM
%token <str> IDENT STR
%token INT CHAR VOID CONST RETURN IF ELSE FOR WHILE BREAK CONTINUE
%token EQ NE LE GE AND OR ELLIPSIS

%type <node> stmt stmt_list compound_stmt decl expr expr_opt args arg_list

/* Lowest precedence first. */
%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE
%right '='
%left OR
%left AND
%left EQ NE
%left '<' '>' LE GE
%left '+' '-'
%left '*' '/' '%'
%right '!' UMINUS

%start translation_unit

%%

/* ---- top level: function definitions and prototypes ---------------- */

translation_unit
    : /* empty */
    | translation_unit external_decl
    ;

external_decl
    : type_name IDENT '('
        {
            cur_func_name   = $2;
            cur_params      = NULL;
            cur_params_tail = NULL;
            cur_nparams     = 0;
            begin_scope();
        }
      params ')' func_tail
    ;

func_tail
    : compound_stmt
        {
            Function *fn  = calloc(1, sizeof(Function));
            fn->name      = cur_func_name;
            fn->body      = $1;
            fn->params    = cur_params;
            fn->nparams   = cur_nparams;
            fn->locals    = current_locals();
            add_function(fn);
        }
    | ';'   /* a prototype, e.g. `int printf(const char *, ...);` -- discard */
    ;

params
    : /* empty */
    | param_list
    ;

param_list
    : param
    | param_list ',' param
    ;

param
    : type_name IDENT   { add_param(declare_var($2)); }
    | type_name         /* unnamed parameter, e.g. `void` */
    | ELLIPSIS          /* variadic marker, ignored */
    ;

/* ---- types (parsed, but otherwise ignored) ------------------------- */

type_name
    : quals base stars
    ;

quals
    : /* empty */
    | quals CONST
    ;

base
    : INT
    | CHAR
    | VOID
    ;

stars
    : /* empty */
    | stars '*'
    ;

/* ---- statements ---------------------------------------------------- */

compound_stmt
    : '{' stmt_list '}'
        { Node *n = new_node(ND_BLOCK); n->body = $2; $$ = n; }
    ;

stmt_list
    : /* empty */          { $$ = NULL; }
    | stmt stmt_list       { $1->next = $2; $$ = $1; }
    ;

stmt
    : RETURN expr ';'      { $$ = new_unary(ND_RETURN, $2); }
    | RETURN ';'           { $$ = new_unary(ND_RETURN, NULL); }
    | IF '(' expr ')' stmt %prec LOWER_THAN_ELSE
        { Node *n = new_node(ND_IF); n->cond = $3; n->then = $5; $$ = n; }
    | IF '(' expr ')' stmt ELSE stmt
        { Node *n = new_node(ND_IF); n->cond = $3; n->then = $5; n->els = $7; $$ = n; }
    | FOR '(' expr_opt ';' expr_opt ';' expr_opt ')' stmt
        { Node *n = new_node(ND_FOR);
          n->init = $3; n->cond = $5; n->inc = $7; n->body = $9; $$ = n; }
    | WHILE '(' expr ')' stmt
        { Node *n = new_node(ND_FOR); n->cond = $3; n->body = $5; $$ = n; }
    | compound_stmt        { $$ = $1; }
    | decl ';'             { $$ = $1; }
    | expr ';'             { $$ = new_unary(ND_EXPR_STMT, $1); }
    | ';'                  { $$ = new_node(ND_NOP); }
    ;

decl
    : type_name IDENT
        { declare_var($2); $$ = new_node(ND_NOP); }
    | type_name IDENT '=' expr
        { Var *v = declare_var($2);
          $$ = new_unary(ND_EXPR_STMT,
                         new_binary(ND_ASSIGN, new_var_node(v), $4)); }
    ;

expr_opt
    : /* empty */          { $$ = NULL; }
    | expr                 { $$ = $1; }
    ;

/* ---- expressions --------------------------------------------------- */

expr
    : NUM                  { $$ = new_num($1); }
    | STR                  { Node *n = new_node(ND_STR);
                             n->str_idx = add_string($1); $$ = n; }
    | IDENT
        { Var *v = find_var($1);
          if (!v) error("use of undeclared identifier '%s'", $1);
          $$ = new_var_node(v); }
    | IDENT '(' args ')'
        { Node *n = new_node(ND_CALL); n->funcname = $1; n->args = $3; $$ = n; }
    | '(' expr ')'         { $$ = $2; }
    | expr '+' expr        { $$ = new_binary(ND_ADD, $1, $3); }
    | expr '-' expr        { $$ = new_binary(ND_SUB, $1, $3); }
    | expr '*' expr        { $$ = new_binary(ND_MUL, $1, $3); }
    | expr '/' expr        { $$ = new_binary(ND_DIV, $1, $3); }
    | expr '%' expr        { $$ = new_binary(ND_MOD, $1, $3); }
    | expr EQ expr         { $$ = new_binary(ND_EQ, $1, $3); }
    | expr NE expr         { $$ = new_binary(ND_NE, $1, $3); }
    | expr '<' expr        { $$ = new_binary(ND_LT, $1, $3); }
    | expr '>' expr        { $$ = new_binary(ND_LT, $3, $1); }  /* a>b  == b<a  */
    | expr LE expr         { $$ = new_binary(ND_LE, $1, $3); }
    | expr GE expr         { $$ = new_binary(ND_LE, $3, $1); }  /* a>=b == b<=a */
    | expr AND expr        { $$ = new_binary(ND_AND, $1, $3); }
    | expr OR expr         { $$ = new_binary(ND_OR, $1, $3); }
    | expr '=' expr
        { if ($1->kind != ND_VAR) error("expression is not assignable");
          $$ = new_binary(ND_ASSIGN, $1, $3); }
    | '-' expr %prec UMINUS { $$ = new_unary(ND_NEG, $2); }
    | '!' expr %prec UMINUS { $$ = new_unary(ND_NOT, $2); }
    ;

args
    : /* empty */          { $$ = NULL; }
    | arg_list             { $$ = $1; }
    ;

arg_list
    : expr                 { $$ = $1; }
    | expr ',' arg_list    { $1->next = $3; $$ = $1; }
    ;

%%

void yyerror(const char *s)
{
    fprintf(stderr, "cc: parse error: %s near line %d\n", s, yylineno);
    exit(1);
}
