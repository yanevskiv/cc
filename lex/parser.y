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
    #include "cc.h"
}

%{
#include <stdio.h>
#include <stdlib.h>
#include "cc.h"

int  yylex(void);
extern int yylineno;
void yyerror(const char *s);

/* State for the function definition currently being parsed. */
static char    *cur_func_name;
static Ast_Var *cur_params;
static Ast_Var *cur_params_tail;
static int      cur_nparams;

/* The program is assembled here as functions are reduced. */
static Ast_Func *prog_head, *prog_tail;

static void add_param(Ast_Var *v)
{
    v->av_param_next = NULL;
    if (!cur_params) cur_params = cur_params_tail = v;
    else { cur_params_tail->av_param_next = v; cur_params_tail = v; }
    cur_nparams++;
}

static void add_function(Ast_Func *fn)
{
    fn->af_next = NULL;
    if (!prog_head) prog_head = prog_tail = fn;
    else { prog_tail->af_next = fn; prog_tail = fn; }
    Ast_Program = prog_head;
}
%}

%union {
    long      num;
    char     *str;
    Ast_Node *node;
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
            Ast_BeginScope();
        }
      params ')' func_tail
    ;

func_tail
    : compound_stmt
        {
            Ast_Func *fn = calloc(1, sizeof(Ast_Func));
            fn->af_name    = cur_func_name;
            fn->af_body    = $1;
            fn->af_params  = cur_params;
            fn->af_nparams = cur_nparams;
            fn->af_locals  = Ast_CurrentLocals();
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
    : type_name IDENT   { add_param(Ast_DeclareVar($2)); }
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
        { Ast_Node *n = Ast_NewNode(AST_NODE_KIND_BLOCK); n->an_body = $2; $$ = n; }
    ;

stmt_list
    : /* empty */          { $$ = NULL; }
    | stmt stmt_list       { $1->an_next = $2; $$ = $1; }
    ;

stmt
    : RETURN expr ';'      { $$ = Ast_NewUnary(AST_NODE_KIND_RETURN, $2); }
    | RETURN ';'           { $$ = Ast_NewUnary(AST_NODE_KIND_RETURN, NULL); }
    | IF '(' expr ')' stmt %prec LOWER_THAN_ELSE
        { Ast_Node *n = Ast_NewNode(AST_NODE_KIND_IF); n->an_cond = $3; n->an_then = $5; $$ = n; }
    | IF '(' expr ')' stmt ELSE stmt
        { Ast_Node *n = Ast_NewNode(AST_NODE_KIND_IF); n->an_cond = $3; n->an_then = $5; n->an_els = $7; $$ = n; }
    | FOR '(' expr_opt ';' expr_opt ';' expr_opt ')' stmt
        { Ast_Node *n = Ast_NewNode(AST_NODE_KIND_FOR);
          n->an_init = $3; n->an_cond = $5; n->an_inc = $7; n->an_body = $9; $$ = n; }
    | WHILE '(' expr ')' stmt
        { Ast_Node *n = Ast_NewNode(AST_NODE_KIND_FOR); n->an_cond = $3; n->an_body = $5; $$ = n; }
    | compound_stmt        { $$ = $1; }
    | decl ';'             { $$ = $1; }
    | expr ';'             { $$ = Ast_NewUnary(AST_NODE_KIND_EXPR_STMT, $1); }
    | ';'                  { $$ = Ast_NewNode(AST_NODE_KIND_NOP); }
    ;

decl
    : type_name IDENT
        { Ast_DeclareVar($2); $$ = Ast_NewNode(AST_NODE_KIND_NOP); }
    | type_name IDENT '=' expr
        { Ast_Var *v = Ast_DeclareVar($2);
          $$ = Ast_NewUnary(AST_NODE_KIND_EXPR_STMT,
                            Ast_NewBinary(AST_NODE_KIND_ASSIGN, Ast_NewVarNode(v), $4)); }
    ;

expr_opt
    : /* empty */          { $$ = NULL; }
    | expr                 { $$ = $1; }
    ;

/* ---- expressions --------------------------------------------------- */

expr
    : NUM                  { $$ = Ast_NewNum($1); }
    | STR                  { Ast_Node *n = Ast_NewNode(AST_NODE_KIND_STR);
                             n->an_str_idx = Ast_AddString($1); $$ = n; }
    | IDENT
        { Ast_Var *v = Ast_FindVar($1);
          if (!v) error("use of undeclared identifier '%s'", $1);
          $$ = Ast_NewVarNode(v); }
    | IDENT '(' args ')'
        { Ast_Node *n = Ast_NewNode(AST_NODE_KIND_CALL); n->an_funcname = $1; n->an_args = $3; $$ = n; }
    | '(' expr ')'         { $$ = $2; }
    | expr '+' expr        { $$ = Ast_NewBinary(AST_NODE_KIND_ADD, $1, $3); }
    | expr '-' expr        { $$ = Ast_NewBinary(AST_NODE_KIND_SUB, $1, $3); }
    | expr '*' expr        { $$ = Ast_NewBinary(AST_NODE_KIND_MUL, $1, $3); }
    | expr '/' expr        { $$ = Ast_NewBinary(AST_NODE_KIND_DIV, $1, $3); }
    | expr '%' expr        { $$ = Ast_NewBinary(AST_NODE_KIND_MOD, $1, $3); }
    | expr EQ expr         { $$ = Ast_NewBinary(AST_NODE_KIND_EQ, $1, $3); }
    | expr NE expr         { $$ = Ast_NewBinary(AST_NODE_KIND_NE, $1, $3); }
    | expr '<' expr        { $$ = Ast_NewBinary(AST_NODE_KIND_LT, $1, $3); }
    | expr '>' expr        { $$ = Ast_NewBinary(AST_NODE_KIND_LT, $3, $1); }  /* a>b  == b<a  */
    | expr LE expr         { $$ = Ast_NewBinary(AST_NODE_KIND_LE, $1, $3); }
    | expr GE expr         { $$ = Ast_NewBinary(AST_NODE_KIND_LE, $3, $1); }  /* a>=b == b<=a */
    | expr AND expr        { $$ = Ast_NewBinary(AST_NODE_KIND_AND, $1, $3); }
    | expr OR expr         { $$ = Ast_NewBinary(AST_NODE_KIND_OR, $1, $3); }
    | expr '=' expr
        { if ($1->an_kind != AST_NODE_KIND_VAR) error("expression is not assignable");
          $$ = Ast_NewBinary(AST_NODE_KIND_ASSIGN, $1, $3); }
    | '-' expr %prec UMINUS { $$ = Ast_NewUnary(AST_NODE_KIND_NEG, $2); }
    | '!' expr %prec UMINUS { $$ = Ast_NewUnary(AST_NODE_KIND_NOT, $2); }
    ;

args
    : /* empty */          { $$ = NULL; }
    | arg_list             { $$ = $1; }
    ;

arg_list
    : expr                 { $$ = $1; }
    | expr ',' arg_list    { $1->an_next = $3; $$ = $1; }
    ;

%%

void yyerror(const char *s)
{
    fprintf(stderr, "cc: parse error: %s near line %d\n", s, yylineno);
    exit(1);
}
