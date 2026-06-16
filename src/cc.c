#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "cc.h"

// The finished program, filled in by the parser.
Ast_Function *Ast_Program;

// Table of interned string literals, indexed by AST_NODE_KIND_STR slot.
static char *Ast_Strings[MAX_STRINGS];

// Number of entries currently used in Ast_Strings.
static int Ast_NumStrings;

// Locals of the function currently being parsed.
static Ast_Var *Ast_Locals;

// Destination stream for emitted assembly.
static FILE *Gen_OutputFile;

// Number of values currently pushed with Gen_Push().
static int Gen_Depth;

// Source of unique label numbers.
static int Gen_LabelId;

// Name of the function currently being emitted.
static const char *Gen_CurFn;

// Registers used to pass the first six integer arguments, in ABI order.
static const char *Gen_ArgReg[6] = { "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9" };

// Interns a string literal and returns its table slot.
int Ast_AddString(char *s)
{
    if (Ast_NumStrings >= MAX_STRINGS)
        error("too many string literals (max %d)", MAX_STRINGS);
    Ast_Strings[Ast_NumStrings] = s;
    return Ast_NumStrings++;
}

// Allocates a zeroed node of the given kind.
Ast_Node *Ast_NewNode(Ast_NodeKind kind)
{
    Ast_Node *n = calloc(1, sizeof(Ast_Node));
    n->an_kind = kind;
    return n;
}

// Builds a binary-operator node with the given operands.
Ast_Node *Ast_NewBinary(Ast_NodeKind kind, Ast_Node *lhs, Ast_Node *rhs)
{
    Ast_Node *n = Ast_NewNode(kind);
    n->an_lhs = lhs;
    n->an_rhs = rhs;
    return n;
}

// Builds a unary-operator node with the given operand.
Ast_Node *Ast_NewUnary(Ast_NodeKind kind, Ast_Node *lhs)
{
    Ast_Node *n = Ast_NewNode(kind);
    n->an_lhs = lhs;
    return n;
}

// Builds an integer-literal node.
Ast_Node *Ast_NewNum(long val)
{
    Ast_Node *n = Ast_NewNode(AST_NODE_KIND_NUM);
    n->an_val = val;
    return n;
}

// Builds a node that references a local variable.
Ast_Node *Ast_NewVarNode(Ast_Var *var)
{
    Ast_Node *n = Ast_NewNode(AST_NODE_KIND_VAR);
    n->an_var = var;
    return n;
}

// Starts a fresh variable scope for a new function.
void Ast_BeginScope(void)
{
    Ast_Locals = NULL;
}

// Looks up a variable by name in the current scope, or NULL.
Ast_Var *Ast_FindVar(const char *name)
{
    for (Ast_Var *v = Ast_Locals; v; v = v->av_next)
        if (strcmp(v->av_name, name) == 0)
            return v;
    return NULL;
}

// Declares a variable in the current scope, reusing any existing slot.
Ast_Var *Ast_DeclareVar(const char *name)
{
    Ast_Var *v = Ast_FindVar(name);
    if (v)
        return v; // re-declaration: reuse the existing slot
    v = calloc(1, sizeof(Ast_Var));
    v->av_name = strdup(name);
    v->av_next = Ast_Locals;
    Ast_Locals = v;
    return v;
}

// Returns the list of locals declared in the current scope.
Ast_Var *Ast_CurrentLocals(void)
{
    return Ast_Locals;
}

// Writes one formatted line of assembly to the output.
static void Gen_Emit(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(Gen_OutputFile, fmt, ap);
    va_end(ap);
    fputc('\n', Gen_OutputFile);
}

// Returns the next unique label number.
static int Gen_Count(void)
{
    return Gen_LabelId++;
}

// Pushes %rax onto the stack and tracks the depth.
static void Gen_Push(void)
{
    Gen_Emit("  push %%rax");
    Gen_Depth++;
}

// Pops the top of the stack into reg and tracks the depth.
static void Gen_Pop(const char *reg)
{
    Gen_Emit("  pop %s", reg);
    Gen_Depth--;
}

// Rounds n up to the nearest multiple of align.
static int Gen_AlignTo(int n, int align)
{
    return (n + align - 1) / align * align;
}

// Computes the address of an lvalue into %rax.
static void Gen_Addr(Ast_Node *node)
{
    if (node->an_kind == AST_NODE_KIND_VAR) {
        Gen_Emit("  lea %d(%%rbp), %%rax", node->an_var->av_offset);
        return;
    }
    error("codegen: not an lvalue");
}

// Emits code for an expression, leaving its result in %rax.
static void Gen_Expr(Ast_Node *node)
{
    switch (node->an_kind) {
    case AST_NODE_KIND_NUM:
        Gen_Emit("  mov $%ld, %%rax", node->an_val);
        return;
    case AST_NODE_KIND_STR:
        Gen_Emit("  lea .Lstr%d(%%rip), %%rax", node->an_str_idx);
        return;
    case AST_NODE_KIND_VAR:
        Gen_Addr(node);
        Gen_Emit("  mov (%%rax), %%rax");
        return;
    case AST_NODE_KIND_ASSIGN:
        Gen_Addr(node->an_lhs);
        Gen_Push();
        Gen_Expr(node->an_rhs);
        Gen_Pop("%rdi");
        Gen_Emit("  mov %%rax, (%%rdi)");
        return;
    case AST_NODE_KIND_NEG:
        Gen_Expr(node->an_lhs);
        Gen_Emit("  neg %%rax");
        return;
    case AST_NODE_KIND_NOT:
        Gen_Expr(node->an_lhs);
        Gen_Emit("  cmp $0, %%rax");
        Gen_Emit("  sete %%al");
        Gen_Emit("  movzb %%al, %%rax");
        return;
    case AST_NODE_KIND_AND: {
        int c = Gen_Count();
        Gen_Expr(node->an_lhs);
        Gen_Emit("  cmp $0, %%rax");
        Gen_Emit("  je .L.false.%d", c);
        Gen_Expr(node->an_rhs);
        Gen_Emit("  cmp $0, %%rax");
        Gen_Emit("  je .L.false.%d", c);
        Gen_Emit("  mov $1, %%rax");
        Gen_Emit("  jmp .L.end.%d", c);
        Gen_Emit(".L.false.%d:", c);
        Gen_Emit("  mov $0, %%rax");
        Gen_Emit(".L.end.%d:", c);
        return;
    }
    case AST_NODE_KIND_OR: {
        int c = Gen_Count();
        Gen_Expr(node->an_lhs);
        Gen_Emit("  cmp $0, %%rax");
        Gen_Emit("  jne .L.true.%d", c);
        Gen_Expr(node->an_rhs);
        Gen_Emit("  cmp $0, %%rax");
        Gen_Emit("  jne .L.true.%d", c);
        Gen_Emit("  mov $0, %%rax");
        Gen_Emit("  jmp .L.end.%d", c);
        Gen_Emit(".L.true.%d:", c);
        Gen_Emit("  mov $1, %%rax");
        Gen_Emit(".L.end.%d:", c);
        return;
    }
    case AST_NODE_KIND_CALL: {
        int nargs = 0;
        for (Ast_Node *a = node->an_args; a; a = a->an_next) {
            Gen_Expr(a);
            Gen_Push();
            nargs++;
        }
        for (int i = nargs - 1; i >= 0; i--)
            Gen_Pop(Gen_ArgReg[i]);

        // The ABI requires %rsp to be 16-byte aligned at the `call`.  Each
        // outstanding Gen_Push() moved it by 8, so realign when depth is odd.
        int realign = Gen_Depth % 2;
        if (realign)
            Gen_Emit("  sub $8, %%rsp");
        Gen_Emit("  mov $0, %%al"); // no vector regs for varargs
        Gen_Emit("  call %s", node->an_funcname);
        if (realign)
            Gen_Emit("  add $8, %%rsp");
        return;
    }
    default:
        break;
    }

    // Binary operators: right operand spilled, left operand in %rax.
    Gen_Expr(node->an_rhs);
    Gen_Push();
    Gen_Expr(node->an_lhs);
    Gen_Pop("%rdi");
    // now: %rax = lhs, %rdi = rhs

    switch (node->an_kind) {
    case AST_NODE_KIND_ADD: Gen_Emit("  add %%rdi, %%rax"); return;
    case AST_NODE_KIND_SUB: Gen_Emit("  sub %%rdi, %%rax"); return;
    case AST_NODE_KIND_MUL: Gen_Emit("  imul %%rdi, %%rax"); return;
    case AST_NODE_KIND_DIV: Gen_Emit("  cqo"); Gen_Emit("  idiv %%rdi"); return;
    case AST_NODE_KIND_MOD: Gen_Emit("  cqo"); Gen_Emit("  idiv %%rdi"); Gen_Emit("  mov %%rdx, %%rax"); return;
    case AST_NODE_KIND_EQ:
    case AST_NODE_KIND_NE:
    case AST_NODE_KIND_LT:
    case AST_NODE_KIND_LE:
        Gen_Emit("  cmp %%rdi, %%rax");
        if      (node->an_kind == AST_NODE_KIND_EQ) Gen_Emit("  sete %%al");
        else if (node->an_kind == AST_NODE_KIND_NE) Gen_Emit("  setne %%al");
        else if (node->an_kind == AST_NODE_KIND_LT) Gen_Emit("  setl %%al");
        else                                        Gen_Emit("  setle %%al");
        Gen_Emit("  movzb %%al, %%rax");
        return;
    default:
        error("codegen: unexpected node kind %d", node->an_kind);
    }
}

// Emits code for a statement.
static void Gen_Stmt(Ast_Node *node)
{
    switch (node->an_kind) {
    case AST_NODE_KIND_RETURN:
        if (node->an_lhs)
            Gen_Expr(node->an_lhs);
        else
            Gen_Emit("  mov $0, %%rax");
        Gen_Emit("  jmp .L.return.%s", Gen_CurFn);
        return;
    case AST_NODE_KIND_IF: {
        int c = Gen_Count();
        Gen_Expr(node->an_cond);
        Gen_Emit("  cmp $0, %%rax");
        Gen_Emit("  je .L.else.%d", c);
        Gen_Stmt(node->an_then);
        Gen_Emit("  jmp .L.endif.%d", c);
        Gen_Emit(".L.else.%d:", c);
        if (node->an_els)
            Gen_Stmt(node->an_els);
        Gen_Emit(".L.endif.%d:", c);
        return;
    }
    case AST_NODE_KIND_FOR: {
        int c = Gen_Count();
        if (node->an_init)
            Gen_Expr(node->an_init);
        Gen_Emit(".L.begin.%d:", c);
        if (node->an_cond) {
            Gen_Expr(node->an_cond);
            Gen_Emit("  cmp $0, %%rax");
            Gen_Emit("  je .L.endfor.%d", c);
        }
        Gen_Stmt(node->an_body);
        if (node->an_inc)
            Gen_Expr(node->an_inc);
        Gen_Emit("  jmp .L.begin.%d", c);
        Gen_Emit(".L.endfor.%d:", c);
        return;
    }
    case AST_NODE_KIND_BLOCK:
        for (Ast_Node *n = node->an_body; n; n = n->an_next)
            Gen_Stmt(n);
        return;
    case AST_NODE_KIND_EXPR_STMT:
        Gen_Expr(node->an_lhs);
        return;
    case AST_NODE_KIND_NOP:
        return;
    default:
        error("codegen: unexpected statement kind %d", node->an_kind);
    }
}

// Assigns each local a stack slot and records the frame size.
static void Gen_AssignLvarOffsets(Ast_Function *fn)
{
    int offset = 0;
    for (Ast_Var *v = fn->af_locals; v; v = v->av_next) {
        offset += 8;
        v->av_offset = -offset;
    }
    fn->af_stack_size = Gen_AlignTo(offset, 16);
}

// Emits the .rodata section holding all string literals.
static void Gen_EmitData(void)
{
    if (Ast_NumStrings == 0)
        return;
    Gen_Emit("  .section .rodata");
    for (int i = 0; i < Ast_NumStrings; i++) {
        Gen_Emit(".Lstr%d:", i);
        // Emit raw bytes so that any contents survive intact.
        for (char *p = Ast_Strings[i]; *p; p++)
            Gen_Emit("  .byte %d", (unsigned char)*p);
        Gen_Emit("  .byte 0");
    }
}

// Emits the .text section: prologue, body and epilogue for each function.
static void Gen_EmitText(Ast_Function *prog)
{
    Gen_Emit("  .text");
    for (Ast_Function *fn = prog; fn; fn = fn->af_next) {
        Gen_AssignLvarOffsets(fn);
        Gen_CurFn = fn->af_name;

        Gen_Emit("  .globl %s", fn->af_name);
        Gen_Emit("%s:", fn->af_name);

        // prologue
        Gen_Emit("  push %%rbp");
        Gen_Emit("  mov %%rsp, %%rbp");
        if (fn->af_stack_size)
            Gen_Emit("  sub $%d, %%rsp", fn->af_stack_size);

        // spill incoming parameters to their stack slots
        int i = 0;
        for (Ast_Var *p = fn->af_params; p; p = p->av_param_next)
            Gen_Emit("  mov %s, %d(%%rbp)", Gen_ArgReg[i++], p->av_offset);

        Gen_Stmt(fn->af_body);

        // epilogue (fall-through returns 0)
        Gen_Emit("  mov $0, %%rax");
        Gen_Emit(".L.return.%s:", fn->af_name);
        Gen_Emit("  mov %%rbp, %%rsp");
        Gen_Emit("  pop %%rbp");
        Gen_Emit("  ret");
    }
}

// Emits assembly for the whole program to fp.
void Gen_Codegen(FILE *fp, Ast_Function *prog)
{
    Gen_OutputFile = fp;
    Gen_Depth      = 0;
    Gen_LabelId    = 0;

    Gen_Emit("  .file \"cc\"");
    Gen_EmitData();
    Gen_EmitText(prog);
    // Mark the stack as non-executable to silence the linker warning.
    Gen_Emit("  .section .note.GNU-stack,\"\",@progbits");
}

// Input stream read by the generated lexer.
extern FILE *yyin;

// Entry point of the generated parser; fills in Ast_Program.
int yyparse(void);

// Prints usage information and exits.
static void usage(const char *prog)
{
    fprintf(stderr,
        "usage: %s [-S] [-o OUTPUT] INPUT.c\n"
        "  -S          emit assembly only (do not assemble or link)\n"
        "  -o OUTPUT   write the result to OUTPUT (default: a.out / out.s)\n",
        prog);
    exit(1);
}

// Compiler entry point: parse arguments, build the AST, emit and link.
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

    // front end: build the AST
    yyin = fopen(input, "r");
    if (!yyin) {
        perror(input);
        return 1;
    }
    yyparse();
    fclose(yyin);

    // back end: emit assembly
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

    // assemble + link with the system C toolchain
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
