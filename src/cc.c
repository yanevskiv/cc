/* cc.c - the cc compiler in a single translation unit.
 *
 * Three sections, formerly three files:
 *   Ast_*   AST/variable constructors, the string table and the per-function
 *           variable scope used by the parser actions.
 *   Gen_*   the x86-64 back end (System V AMD64 ABI, AT&T syntax).
 *   main    the command line driver.
 *
 * Pipeline:   source.c --[flex+bison]--> AST --[Gen_Codegen]--> x86-64 asm
 *                       --[system assembler+linker]--> executable
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "cc.h"

/* ================================================================== */
/* AST: constructors, string table, scope and diagnostics            */
/* ================================================================== */

/* The finished program, filled in by the parser. */
Ast_Function *Ast_Program;

/* ------------------------------------------------------------------ */
/* String literal table                                               */
/* ------------------------------------------------------------------ */
char *Ast_Strings[MAX_STRINGS];
int   Ast_NumStrings;

int Ast_AddString(char *s)
{
    if (Ast_NumStrings >= MAX_STRINGS)
        error("too many string literals (max %d)", MAX_STRINGS);
    Ast_Strings[Ast_NumStrings] = s;
    return Ast_NumStrings++;
}

/* ------------------------------------------------------------------ */
/* Node constructors                                                  */
/* ------------------------------------------------------------------ */
Ast_Node *Ast_NewNode(Ast_NodeKind kind)
{
    Ast_Node *n = calloc(1, sizeof(Ast_Node));
    n->kind = kind;
    return n;
}

Ast_Node *Ast_NewBinary(Ast_NodeKind kind, Ast_Node *lhs, Ast_Node *rhs)
{
    Ast_Node *n = Ast_NewNode(kind);
    n->lhs = lhs;
    n->rhs = rhs;
    return n;
}

Ast_Node *Ast_NewUnary(Ast_NodeKind kind, Ast_Node *lhs)
{
    Ast_Node *n = Ast_NewNode(kind);
    n->lhs = lhs;
    return n;
}

Ast_Node *Ast_NewNum(long val)
{
    Ast_Node *n = Ast_NewNode(ND_NUM);
    n->val = val;
    return n;
}

Ast_Node *Ast_NewVarNode(Ast_Var *var)
{
    Ast_Node *n = Ast_NewNode(ND_VAR);
    n->var = var;
    return n;
}

/* ------------------------------------------------------------------ */
/* Per-function variable scope                                        */
/* ------------------------------------------------------------------ */
static Ast_Var *Ast_Locals;   /* locals of the function being parsed */

void Ast_BeginScope(void)
{
    Ast_Locals = NULL;
}

Ast_Var *Ast_FindVar(const char *name)
{
    for (Ast_Var *v = Ast_Locals; v; v = v->next)
        if (strcmp(v->name, name) == 0)
            return v;
    return NULL;
}

Ast_Var *Ast_DeclareVar(const char *name)
{
    Ast_Var *v = Ast_FindVar(name);
    if (v)
        return v;            /* re-declaration: reuse the existing slot */
    v = calloc(1, sizeof(Ast_Var));
    v->name    = strdup(name);
    v->next    = Ast_Locals;
    Ast_Locals = v;
    return v;
}

Ast_Var *Ast_CurrentLocals(void)
{
    return Ast_Locals;
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

/* ================================================================== */
/* Code generator                                                    */
/*                                                                    */
/* The strategy is the classic "stack machine": every expression     */
/* leaves its result in %rax, and temporaries are spilled to the      */
/* hardware stack with push/pop.  It is not fast code, but it is      */
/* small and easy to follow -- the point of this base is clarity,     */
/* not optimisation.                                                  */
/* ================================================================== */

static FILE       *Gen_OutputFile;
static int         Gen_Depth;     /* number of values currently Gen_Push()ed */
static int         Gen_LabelId;   /* source of unique label numbers          */
static const char *Gen_CurFn;     /* name of the function being emitted       */

static const char *Gen_ArgReg[6] = { "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9" };

static void Gen_Emit(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(Gen_OutputFile, fmt, ap);
    va_end(ap);
    fputc('\n', Gen_OutputFile);
}

static int Gen_Count(void) { return Gen_LabelId++; }

static void Gen_Push(void)
{
    Gen_Emit("  push %%rax");
    Gen_Depth++;
}

static void Gen_Pop(const char *reg)
{
    Gen_Emit("  pop %s", reg);
    Gen_Depth--;
}

static int Gen_AlignTo(int n, int align)
{
    return (n + align - 1) / align * align;
}

static void Gen_Expr(Ast_Node *node);
static void Gen_Stmt(Ast_Node *node);

/* Compute the address of an lvalue into %rax. */
static void Gen_Addr(Ast_Node *node)
{
    if (node->kind == ND_VAR) {
        Gen_Emit("  lea %d(%%rbp), %%rax", node->var->offset);
        return;
    }
    error("codegen: not an lvalue");
}

static void Gen_Expr(Ast_Node *node)
{
    switch (node->kind) {
    case ND_NUM:
        Gen_Emit("  mov $%ld, %%rax", node->val);
        return;
    case ND_STR:
        Gen_Emit("  lea .Lstr%d(%%rip), %%rax", node->str_idx);
        return;
    case ND_VAR:
        Gen_Addr(node);
        Gen_Emit("  mov (%%rax), %%rax");
        return;
    case ND_ASSIGN:
        Gen_Addr(node->lhs);
        Gen_Push();
        Gen_Expr(node->rhs);
        Gen_Pop("%rdi");
        Gen_Emit("  mov %%rax, (%%rdi)");
        return;
    case ND_NEG:
        Gen_Expr(node->lhs);
        Gen_Emit("  neg %%rax");
        return;
    case ND_NOT:
        Gen_Expr(node->lhs);
        Gen_Emit("  cmp $0, %%rax");
        Gen_Emit("  sete %%al");
        Gen_Emit("  movzb %%al, %%rax");
        return;
    case ND_AND: {
        int c = Gen_Count();
        Gen_Expr(node->lhs);
        Gen_Emit("  cmp $0, %%rax");
        Gen_Emit("  je .L.false.%d", c);
        Gen_Expr(node->rhs);
        Gen_Emit("  cmp $0, %%rax");
        Gen_Emit("  je .L.false.%d", c);
        Gen_Emit("  mov $1, %%rax");
        Gen_Emit("  jmp .L.end.%d", c);
        Gen_Emit(".L.false.%d:", c);
        Gen_Emit("  mov $0, %%rax");
        Gen_Emit(".L.end.%d:", c);
        return;
    }
    case ND_OR: {
        int c = Gen_Count();
        Gen_Expr(node->lhs);
        Gen_Emit("  cmp $0, %%rax");
        Gen_Emit("  jne .L.true.%d", c);
        Gen_Expr(node->rhs);
        Gen_Emit("  cmp $0, %%rax");
        Gen_Emit("  jne .L.true.%d", c);
        Gen_Emit("  mov $0, %%rax");
        Gen_Emit("  jmp .L.end.%d", c);
        Gen_Emit(".L.true.%d:", c);
        Gen_Emit("  mov $1, %%rax");
        Gen_Emit(".L.end.%d:", c);
        return;
    }
    case ND_CALL: {
        int nargs = 0;
        for (Ast_Node *a = node->args; a; a = a->next) {
            Gen_Expr(a);
            Gen_Push();
            nargs++;
        }
        for (int i = nargs - 1; i >= 0; i--)
            Gen_Pop(Gen_ArgReg[i]);

        /* The ABI requires %rsp to be 16-byte aligned at the `call`.  Each
         * outstanding Gen_Push() moved it by 8, so realign when depth is odd. */
        int realign = Gen_Depth % 2;
        if (realign)
            Gen_Emit("  sub $8, %%rsp");
        Gen_Emit("  mov $0, %%al");          /* no vector regs for varargs */
        Gen_Emit("  call %s", node->funcname);
        if (realign)
            Gen_Emit("  add $8, %%rsp");
        return;
    }
    default:
        break;
    }

    /* Binary operators: right operand spilled, left operand in %rax. */
    Gen_Expr(node->rhs);
    Gen_Push();
    Gen_Expr(node->lhs);
    Gen_Pop("%rdi");
    /* now: %rax = lhs, %rdi = rhs */

    switch (node->kind) {
    case ND_ADD: Gen_Emit("  add %%rdi, %%rax"); return;
    case ND_SUB: Gen_Emit("  sub %%rdi, %%rax"); return;
    case ND_MUL: Gen_Emit("  imul %%rdi, %%rax"); return;
    case ND_DIV: Gen_Emit("  cqo"); Gen_Emit("  idiv %%rdi"); return;
    case ND_MOD: Gen_Emit("  cqo"); Gen_Emit("  idiv %%rdi"); Gen_Emit("  mov %%rdx, %%rax"); return;
    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE:
        Gen_Emit("  cmp %%rdi, %%rax");
        if      (node->kind == ND_EQ) Gen_Emit("  sete %%al");
        else if (node->kind == ND_NE) Gen_Emit("  setne %%al");
        else if (node->kind == ND_LT) Gen_Emit("  setl %%al");
        else                          Gen_Emit("  setle %%al");
        Gen_Emit("  movzb %%al, %%rax");
        return;
    default:
        error("codegen: unexpected node kind %d", node->kind);
    }
}

static void Gen_Stmt(Ast_Node *node)
{
    switch (node->kind) {
    case ND_RETURN:
        if (node->lhs)
            Gen_Expr(node->lhs);
        else
            Gen_Emit("  mov $0, %%rax");
        Gen_Emit("  jmp .L.return.%s", Gen_CurFn);
        return;
    case ND_IF: {
        int c = Gen_Count();
        Gen_Expr(node->cond);
        Gen_Emit("  cmp $0, %%rax");
        Gen_Emit("  je .L.else.%d", c);
        Gen_Stmt(node->then);
        Gen_Emit("  jmp .L.endif.%d", c);
        Gen_Emit(".L.else.%d:", c);
        if (node->els)
            Gen_Stmt(node->els);
        Gen_Emit(".L.endif.%d:", c);
        return;
    }
    case ND_FOR: {
        int c = Gen_Count();
        if (node->init)
            Gen_Expr(node->init);
        Gen_Emit(".L.begin.%d:", c);
        if (node->cond) {
            Gen_Expr(node->cond);
            Gen_Emit("  cmp $0, %%rax");
            Gen_Emit("  je .L.endfor.%d", c);
        }
        Gen_Stmt(node->body);
        if (node->inc)
            Gen_Expr(node->inc);
        Gen_Emit("  jmp .L.begin.%d", c);
        Gen_Emit(".L.endfor.%d:", c);
        return;
    }
    case ND_BLOCK:
        for (Ast_Node *n = node->body; n; n = n->next)
            Gen_Stmt(n);
        return;
    case ND_EXPR_STMT:
        Gen_Expr(node->lhs);
        return;
    case ND_NOP:
        return;
    default:
        error("codegen: unexpected statement kind %d", node->kind);
    }
}

static void Gen_AssignLvarOffsets(Ast_Function *fn)
{
    int offset = 0;
    for (Ast_Var *v = fn->locals; v; v = v->next) {
        offset += 8;
        v->offset = -offset;
    }
    fn->stack_size = Gen_AlignTo(offset, 16);
}

static void Gen_EmitData(void)
{
    if (Ast_NumStrings == 0)
        return;
    Gen_Emit("  .section .rodata");
    for (int i = 0; i < Ast_NumStrings; i++) {
        Gen_Emit(".Lstr%d:", i);
        /* Emit raw bytes so that any contents survive intact. */
        for (char *p = Ast_Strings[i]; *p; p++)
            Gen_Emit("  .byte %d", (unsigned char)*p);
        Gen_Emit("  .byte 0");
    }
}

static void Gen_EmitText(Ast_Function *prog)
{
    Gen_Emit("  .text");
    for (Ast_Function *fn = prog; fn; fn = fn->next) {
        Gen_AssignLvarOffsets(fn);
        Gen_CurFn = fn->name;

        Gen_Emit("  .globl %s", fn->name);
        Gen_Emit("%s:", fn->name);

        /* prologue */
        Gen_Emit("  push %%rbp");
        Gen_Emit("  mov %%rsp, %%rbp");
        if (fn->stack_size)
            Gen_Emit("  sub $%d, %%rsp", fn->stack_size);

        /* spill incoming parameters to their stack slots */
        int i = 0;
        for (Ast_Var *p = fn->params; p; p = p->param_next)
            Gen_Emit("  mov %s, %d(%%rbp)", Gen_ArgReg[i++], p->offset);

        Gen_Stmt(fn->body);

        /* epilogue (fall-through returns 0) */
        Gen_Emit("  mov $0, %%rax");
        Gen_Emit(".L.return.%s:", fn->name);
        Gen_Emit("  mov %%rbp, %%rsp");
        Gen_Emit("  pop %%rbp");
        Gen_Emit("  ret");
    }
}

void Gen_Codegen(FILE *fp, Ast_Function *prog)
{
    Gen_OutputFile = fp;
    Gen_Depth      = 0;
    Gen_LabelId    = 0;

    Gen_Emit("  .file \"cc\"");
    Gen_EmitData();
    Gen_EmitText(prog);
    /* Mark the stack as non-executable to silence the linker warning. */
    Gen_Emit("  .section .note.GNU-stack,\"\",@progbits");
}

/* ================================================================== */
/* main: command line driver                                         */
/*                                                                    */
/* The lexer and parser do the real compiler work; an external        */
/* assembler/linker (gcc by default, overridable with $CC) turns our  */
/* assembly into an ELF binary and pulls in the C runtime.  This      */
/* mirrors how small compilers such as tcc's early versions or        */
/* chibicc bootstrap themselves.                                      */
/* ================================================================== */

extern FILE *yyin;
int          yyparse(void);

static void usage(const char *prog)
{
    fprintf(stderr,
        "usage: %s [-S] [-o OUTPUT] INPUT.c\n"
        "  -S          emit assembly only (do not assemble or link)\n"
        "  -o OUTPUT   write the result to OUTPUT (default: a.out / out.s)\n",
        prog);
    exit(1);
}

int main(int argc, char **argv)
{
    const char *input    = NULL;
    const char *output   = NULL;
    int         asm_only = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-S") == 0) {
            asm_only = 1;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (++i >= argc) usage(argv[0]);
            output = argv[i];
        } else if (argv[i][0] == '-' && argv[i][1] != '\0') {
            fprintf(stderr, "cc: unknown option '%s'\n", argv[i]);
            usage(argv[0]);
        } else {
            input = argv[i];
        }
    }

    if (!input)
        usage(argv[0]);
    if (!output)
        output = asm_only ? "out.s" : "a.out";

    /* ---- front end: build the AST ---- */
    yyin = fopen(input, "r");
    if (!yyin) {
        perror(input);
        return 1;
    }
    yyparse();
    fclose(yyin);

    /* ---- back end: emit assembly ---- */
    char *asmpath;
    if (asm_only) {
        asmpath = strdup(output);
    } else {
        asmpath = malloc(strlen(output) + 4);
        sprintf(asmpath, "%s.s", output);
    }

    FILE *asmfile = fopen(asmpath, "w");
    if (!asmfile) {
        perror(asmpath);
        return 1;
    }
    Gen_Codegen(asmfile, Ast_Program);
    fclose(asmfile);

    if (asm_only)
        return 0;

    /* ---- assemble + link with the system C toolchain ---- */
    const char *driver = getenv("CC");
    if (!driver || !*driver)
        driver = "gcc";

    size_t cmdlen = strlen(driver) + strlen(asmpath) + strlen(output) + 32;
    char  *cmd    = malloc(cmdlen);
    snprintf(cmd, cmdlen, "%s \"%s\" -o \"%s\"", driver, asmpath, output);

    int rc = system(cmd);
    unlink(asmpath);
    free(cmd);
    free(asmpath);

    if (rc != 0) {
        fprintf(stderr, "cc: assembler/linker step failed\n");
        return 1;
    }
    return 0;
}
