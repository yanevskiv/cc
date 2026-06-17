#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "cc.h"

// The finished program, filled in by the parser.
Ast_Func *Ast_Program;

// Table of interned string literals, indexed by AST_NODE_KIND_STR slot.
static char *Ast_Strings[MAX_STRINGS];

// Number of entries currently used in Ast_Strings.
static int Ast_NumStrings;

// Locals of the function currently being parsed.
static Ast_Var *Ast_Locals;

// Destination stream for emitted assembly.
static FILE *Gen_OutputFile;

// Number of values currently pushed with Gen_EmitPush().
static int Gen_Depth;

// Source of unique label numbers.
static int Gen_LabelId;

// Name of the function currently being emitted.
static const char *Gen_CurrFunc;

// Registers used to pass the first six integer arguments, in ABI order.
static const char *Gen_ArgReg[6] = { "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9" };

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
        return var; // re-declaration: reuse the existing slot
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

// Writes one formatted line of assembly to the output.
static void Gen_EmitLine(const char *fmt, ...)
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
static void Gen_EmitPush(void)
{
    Gen_EmitLine("  push %%rax");
    Gen_Depth++;
}

// Pops the top of the stack into reg and tracks the depth.
static void Gen_EmitPop(const char *reg)
{
    Gen_EmitLine("  pop %s", reg);
    Gen_Depth--;
}

// Rounds n up to the nearest multiple of align.
static int Gen_AlignTo(int n, int align)
{
    return (n + align - 1) / align * align;
}

// Computes the address of an lvalue into %rax.
static void Gen_EmitAddr(Ast_Node *node)
{
    if (node->an_kind == AST_NODE_KIND_VAR) {
        Gen_EmitLine("  lea %d(%%rbp), %%rax", node->an_var->av_offset);
        return;
    }
    error("codegen: not an lvalue");
}

// Emits code for an expression, leaving its result in %rax.
static void Gen_EmitExpr(Ast_Node *node)
{
    switch (node->an_kind) {
        case AST_NODE_KIND_NUM: {
            Gen_EmitLine("  mov $%ld, %%rax", node->an_val);
        } break;
        case AST_NODE_KIND_STR: {
            Gen_EmitLine("  lea .Lstr%d(%%rip), %%rax", node->an_str_idx);
        } break;
        case AST_NODE_KIND_VAR: {
            Gen_EmitAddr(node);
            Gen_EmitLine("  mov (%%rax), %%rax");
        } break;
        case AST_NODE_KIND_ASSIGN: {
            Gen_EmitAddr(node->an_lhs);
            Gen_EmitPush();
            Gen_EmitExpr(node->an_rhs);
            Gen_EmitPop("%rdi");
            Gen_EmitLine("  mov %%rax, (%%rdi)");
        } break;
        case AST_NODE_KIND_NEG: {
            Gen_EmitExpr(node->an_lhs);
            Gen_EmitLine("  neg %%rax");
        } break;
        case AST_NODE_KIND_NOT: {
            Gen_EmitExpr(node->an_lhs);
            Gen_EmitLine("  cmp $0, %%rax");
            Gen_EmitLine("  sete %%al");
            Gen_EmitLine("  movzb %%al, %%rax");
        } break;
        case AST_NODE_KIND_AND: {
            int count = Gen_Count();
            Gen_EmitExpr(node->an_lhs);
            Gen_EmitLine("  cmp $0, %%rax");
            Gen_EmitLine("  je .L.false.%d", count);
            Gen_EmitExpr(node->an_rhs);
            Gen_EmitLine("  cmp $0, %%rax");
            Gen_EmitLine("  je .L.false.%d", count);
            Gen_EmitLine("  mov $1, %%rax");
            Gen_EmitLine("  jmp .L.end.%d", count);
            Gen_EmitLine(".L.false.%d:", count);
            Gen_EmitLine("  mov $0, %%rax");
            Gen_EmitLine(".L.end.%d:", count);
        } break;
        case AST_NODE_KIND_OR: {
            int count = Gen_Count();
            Gen_EmitExpr(node->an_lhs);
            Gen_EmitLine("  cmp $0, %%rax");
            Gen_EmitLine("  jne .L.true.%d", count);
            Gen_EmitExpr(node->an_rhs);
            Gen_EmitLine("  cmp $0, %%rax");
            Gen_EmitLine("  jne .L.true.%d", count);
            Gen_EmitLine("  mov $0, %%rax");
            Gen_EmitLine("  jmp .L.end.%d", count);
            Gen_EmitLine(".L.true.%d:", count);
            Gen_EmitLine("  mov $1, %%rax");
            Gen_EmitLine(".L.end.%d:", count);
        } break;
        case AST_NODE_KIND_CALL: {
            int nargs = 0;
            for (Ast_Node *arg = node->an_args; arg; arg = arg->an_next) {
                Gen_EmitExpr(arg);
                Gen_EmitPush();
                nargs++;
            }
            for (int i = nargs - 1; i >= 0; i--) {
                Gen_EmitPop(Gen_ArgReg[i]);
            }

            // The ABI requires %rsp to be 16-byte aligned at the `call`.  Each
            // outstanding Gen_EmitPush() moved it by 8, so realign when depth is odd.
            int realign = Gen_Depth % 2;
            if (realign) {
                Gen_EmitLine("  sub $8, %%rsp");
            }
            Gen_EmitLine("  mov $0, %%al"); // no vector regs for varargs
            Gen_EmitLine("  call %s", node->an_funcname);
            if (realign) {
                Gen_EmitLine("  add $8, %%rsp");
            }
        } break;
        default: {
            // Binary operators: right operand spilled, left operand in %rax.
            Gen_EmitExpr(node->an_rhs);
            Gen_EmitPush();
            Gen_EmitExpr(node->an_lhs);
            Gen_EmitPop("%rdi");
            // now: %rax = lhs, %rdi = rhs

            switch (node->an_kind) {
                case AST_NODE_KIND_ADD: {
                    Gen_EmitLine("  add %%rdi, %%rax");
                } break;
                case AST_NODE_KIND_SUB: {
                    Gen_EmitLine("  sub %%rdi, %%rax");
                } break;
                case AST_NODE_KIND_MUL: {
                    Gen_EmitLine("  imul %%rdi, %%rax");
                } break;
                case AST_NODE_KIND_DIV: {
                    Gen_EmitLine("  cqo");
                    Gen_EmitLine("  idiv %%rdi");
                } break;
                case AST_NODE_KIND_MOD: {
                    Gen_EmitLine("  cqo");
                    Gen_EmitLine("  idiv %%rdi");
                    Gen_EmitLine("  mov %%rdx, %%rax");
                } break;
                case AST_NODE_KIND_EQ: {
                    Gen_EmitLine("  cmp %%rdi, %%rax");
                    Gen_EmitLine("  sete %%al");
                    Gen_EmitLine("  movzb %%al, %%rax");
                } break;
                case AST_NODE_KIND_NE: {
                    Gen_EmitLine("  cmp %%rdi, %%rax");
                    Gen_EmitLine("  setne %%al");
                    Gen_EmitLine("  movzb %%al, %%rax");
                } break;
                case AST_NODE_KIND_LT: {
                    Gen_EmitLine("  cmp %%rdi, %%rax");
                    Gen_EmitLine("  setl %%al");
                    Gen_EmitLine("  movzb %%al, %%rax");
                } break;
                case AST_NODE_KIND_LE: {
                    Gen_EmitLine("  cmp %%rdi, %%rax");
                    Gen_EmitLine("  setle %%al");
                    Gen_EmitLine("  movzb %%al, %%rax");
                } break;
                default: {
                    error("codegen: unexpected node kind %d", node->an_kind);
                }
            }
        }
    }
}

// Emits code for a statement.
static void Gen_EmitStmt(Ast_Node *node)
{
    switch (node->an_kind) {
        case AST_NODE_KIND_RETURN: {
            if (node->an_lhs) {
                Gen_EmitExpr(node->an_lhs);
            } else {
                Gen_EmitLine("  mov $0, %%rax");
            }
            Gen_EmitLine("  jmp .L.return.%s", Gen_CurrFunc);
        } break;
        case AST_NODE_KIND_IF: {
            int count = Gen_Count();
            Gen_EmitExpr(node->an_cond);
            Gen_EmitLine("  cmp $0, %%rax");
            Gen_EmitLine("  je .L.else.%d", count);
            Gen_EmitStmt(node->an_then);
            Gen_EmitLine("  jmp .L.endif.%d", count);
            Gen_EmitLine(".L.else.%d:", count);
            if (node->an_els) {
                Gen_EmitStmt(node->an_els);
            }
            Gen_EmitLine(".L.endif.%d:", count);
        } break;
        case AST_NODE_KIND_FOR: {
            int count = Gen_Count();
            if (node->an_init) {
                Gen_EmitExpr(node->an_init);
            }
            Gen_EmitLine(".L.begin.%d:", count);
            if (node->an_cond) {
                Gen_EmitExpr(node->an_cond);
                Gen_EmitLine("  cmp $0, %%rax");
                Gen_EmitLine("  je .L.endfor.%d", count);
            }
            Gen_EmitStmt(node->an_body);
            if (node->an_inc) {
                Gen_EmitExpr(node->an_inc);
            }
            Gen_EmitLine("  jmp .L.begin.%d", count);
            Gen_EmitLine(".L.endfor.%d:", count);
        } break;
        case AST_NODE_KIND_BLOCK: {
            for (Ast_Node *stmt = node->an_body; stmt; stmt = stmt->an_next) {
                Gen_EmitStmt(stmt);
            }
        } break;
        case AST_NODE_KIND_EXPR_STMT: {
            Gen_EmitExpr(node->an_lhs);
        } break;
        case AST_NODE_KIND_NOP: {
            // nothing to emit
        } break;
        default: {
            error("codegen: unexpected statement kind %d", node->an_kind);
        }
    }
}

// Assigns each local a stack slot and records the frame size.
static void Gen_AssignLvarOffsets(Ast_Func *func)
{
    int offset = 0;
    for (Ast_Var *var = func->af_locals; var; var = var->av_next) {
        offset += 8;
        var->av_offset = -offset;
    }
    func->af_stack_size = Gen_AlignTo(offset, 16);
}

// Emits the .rodata section holding all string literals.
static void Gen_EmitDataSection(void)
{
    if (Ast_NumStrings == 0) {
        return;
    }
    Gen_EmitLine("  .section .rodata");
    for (int i = 0; i < Ast_NumStrings; i++) {
        Gen_EmitLine(".Lstr%d:", i);
        // Emit raw bytes so that any contents survive intact.
        for (char *cursor = Ast_Strings[i]; *cursor; cursor++) {
            Gen_EmitLine("  .byte %d", (unsigned char)*cursor);
        }
        Gen_EmitLine("  .byte 0");
    }
}

// Emits the .text section: prologue, body and epilogue for each function.
static void Gen_EmitTextSection(Ast_Func *prog)
{
    Gen_EmitLine("  .text");
    for (Ast_Func *func = prog; func; func = func->af_next) {
        Gen_AssignLvarOffsets(func);
        Gen_CurrFunc = func->af_name;

        Gen_EmitLine("  .globl %s", func->af_name);
        Gen_EmitLine("%s:", func->af_name);

        // prologue
        Gen_EmitLine("  push %%rbp");
        Gen_EmitLine("  mov %%rsp, %%rbp");
        if (func->af_stack_size) {
            Gen_EmitLine("  sub $%d, %%rsp", func->af_stack_size);
        }

        // spill incoming parameters to their stack slots
        int i = 0;
        for (Ast_Var *param = func->af_params; param; param = param->av_param_next) {
            Gen_EmitLine("  mov %s, %d(%%rbp)", Gen_ArgReg[i++], param->av_offset);
        }

        Gen_EmitStmt(func->af_body);

        // epilogue (fall-through returns 0)
        Gen_EmitLine("  mov $0, %%rax");
        Gen_EmitLine(".L.return.%s:", func->af_name);
        Gen_EmitLine("  mov %%rbp, %%rsp");
        Gen_EmitLine("  pop %%rbp");
        Gen_EmitLine("  ret");
    }
}

// Emits assembly for the whole program to out.
void Gen_Codegen(FILE *out, Ast_Func *prog)
{
    Gen_OutputFile = out;
    Gen_Depth      = 0;
    Gen_LabelId    = 0;

    Gen_EmitLine("  .file \"cc\"");
    Gen_EmitDataSection();
    Gen_EmitTextSection(prog);
    // Mark the stack as non-executable to silence the linker warning.
    Gen_EmitLine("  .section .note.GNU-stack,\"\",@progbits");
}

// Input stream read by the generated lexer.
extern FILE *yyin;

// Entry point of the generated parser; fills in Ast_Program.
int yyparse(void);

// Show usage information and exits.
static void Show_Usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s [-o OUTPUT] INPUT.c\n"
        "  -o OUTPUT   write assembly to OUTPUT (default: INPUT.s)\n",
        prog);
    exit(1);
}

// Change or append extension ('main.c' -> 'main.s').  
static char *Str_ChangeOrAppendExt(const char *input, const char *suffix)
{
    const char *slash = strrchr(input, '/');
    const char *dot   = strrchr(input, '.');

    // Only a '.' in the final path component counts as an extension separator,
    // so a dot in a directory name ('a.b/main') is not mistaken for one.
    if (!dot || (slash && dot < slash)) {
        dot = NULL;
    }

    size_t stem = dot ? (size_t)(dot - input) : strlen(input);
    size_t slen = strlen(suffix);
    char  *out  = malloc(stem + slen + 1);
    memcpy(out, input, stem);
    memcpy(out + stem, suffix, slen + 1); // copy suffix including its NUL
    return out;
}

// Main function
int main(int argc, char **argv)
{
    const char *output = NULL;

    int opt;
    while ((opt = getopt(argc, argv, "o:")) != -1) {
        switch (opt) {
            case 'o': {
                output = optarg;
            } break;
            default: {
                Show_Usage(argv[0]);
            } break;
        }
    }

    if (optind >= argc) {
        Show_Usage(argv[0]);
    }
    const char *input = argv[optind];

    char *outbuf = NULL;
    if (! output) {
        outbuf = Str_ChangeOrAppendExt(input, ".s");
        output = outbuf;
    }

    int result = 0;

    // Front end: build the AST
    yyin = fopen(input, "r");
    if (! yyin) {
        perror(input);
        result = 1;
        goto cleanup;
    }
    yyparse();
    fclose(yyin);

    // Back end: emit assembly
    FILE *asm_file = fopen(output, "w");
    if (! asm_file) {
        perror(output);
        result = 1;
        goto cleanup;
    }
    Gen_Codegen(asm_file, Ast_Program);
    fclose(asm_file);

cleanup:
    free(outbuf);
    return result;
}
