#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "cc.h"
#include "str.h"

// The finished program, filled in by the parser.
Ast_Func *Ast_Program;

// Table of interned string literals, indexed by AST_NODE_KIND_STR slot.
static char *Ast_Strings[MAX_STRINGS];

// Number of entries currently used in Ast_Strings.
static int Ast_NumStrings;

// Locals of the function currently being parsed.
static Ast_Var *Ast_Locals;

// Number of values currently pushed with Gen_EmitPush().
static int Gen_Depth;

// Source of unique label numbers.
static int Gen_LabelId;

// Name of the function currently being emitted.
static const char *Gen_CurrFunc;

// Registers used to pass the first six integer arguments, in ABI order.
static const Asm_Reg Gen_ArgReg[6] = {
    ASM_REG_RDI,
    ASM_REG_RSI,
    ASM_REG_RDX,
    ASM_REG_RCX,
    ASM_REG_R8,
    ASM_REG_R9
};

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

// Returns the next unique label number.
int Gen_Count(void)
{
    return Gen_LabelId++;
}

// Pushes %rax onto the stack and tracks the depth.
void Gen_EmitPush(void)
{
    Asm_EmitPush(ASM_REG_RAX);
    Gen_Depth++;
}

// Pops the top of the stack into reg and tracks the depth.
void Gen_EmitPop(Asm_Reg reg)
{
    Asm_EmitPop(reg);
    Gen_Depth--;
}

// Rounds n up to the nearest multiple of align.
int Gen_AlignTo(int n, int align)
{
    return (n + align - 1) / align * align;
}

// Computes the address of an lvalue into %rax.
void Gen_EmitAddr(Ast_Node *node)
{
    if (node->an_kind == AST_NODE_KIND_VAR) {
        Asm_EmitLea(ASM_REG_RBP, node->an_var->av_offset, ASM_REG_RAX);
        return;
    }
    error("codegen: not an lvalue");
}

// Emits code for an expression, leaving its result in %rax.
void Gen_EmitExpr(Ast_Node *node)
{
    switch (node->an_kind) {
        case AST_NODE_KIND_NUM: {
            Asm_EmitMovImm(node->an_val, ASM_REG_RAX);
        } break;
        case AST_NODE_KIND_STR: {
            Asm_EmitLeaRip(ASM_REG_RAX, ".Lstr%d", node->an_str_idx);
        } break;
        case AST_NODE_KIND_VAR: {
            Gen_EmitAddr(node);
            Asm_EmitMovLoad(ASM_REG_RAX, 0, ASM_REG_RAX);
        } break;
        case AST_NODE_KIND_ASSIGN: {
            Gen_EmitAddr(node->an_lhs);
            Gen_EmitPush();
            Gen_EmitExpr(node->an_rhs);
            Gen_EmitPop(ASM_REG_RDI);
            Asm_EmitMovStore(ASM_REG_RAX, ASM_REG_RDI, 0);
        } break;
        case AST_NODE_KIND_NEG: {
            Gen_EmitExpr(node->an_lhs);
            Asm_EmitNeg(ASM_REG_RAX);
        } break;
        case AST_NODE_KIND_NOT: {
            Gen_EmitExpr(node->an_lhs);
            Asm_EmitCmpImm(0, ASM_REG_RAX);
            Asm_EmitSete(ASM_REG_RAX);
            Asm_EmitMovzb(ASM_REG_RAX, ASM_REG_RAX);
        } break;
        case AST_NODE_KIND_AND: {
            int count = Gen_Count();
            Gen_EmitExpr(node->an_lhs);
            Asm_EmitCmpImm(0, ASM_REG_RAX);
            Asm_EmitJe(".L.false.%d", count);
            Gen_EmitExpr(node->an_rhs);
            Asm_EmitCmpImm(0, ASM_REG_RAX);
            Asm_EmitJe(".L.false.%d", count);
            Asm_EmitMovImm(1, ASM_REG_RAX);
            Asm_EmitJmp(".L.end.%d", count);
            Asm_EmitLabel(".L.false.%d", count);
            Asm_EmitMovImm(0, ASM_REG_RAX);
            Asm_EmitLabel(".L.end.%d", count);
        } break;
        case AST_NODE_KIND_OR: {
            int count = Gen_Count();
            Gen_EmitExpr(node->an_lhs);
            Asm_EmitCmpImm(0, ASM_REG_RAX);
            Asm_EmitJne(".L.true.%d", count);
            Gen_EmitExpr(node->an_rhs);
            Asm_EmitCmpImm(0, ASM_REG_RAX);
            Asm_EmitJne(".L.true.%d", count);
            Asm_EmitMovImm(0, ASM_REG_RAX);
            Asm_EmitJmp(".L.end.%d", count);
            Asm_EmitLabel(".L.true.%d", count);
            Asm_EmitMovImm(1, ASM_REG_RAX);
            Asm_EmitLabel(".L.end.%d", count);
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
                Asm_EmitSubImm(8, ASM_REG_RSP);
            }
            Asm_EmitMovImm8(0, ASM_REG_RAX); // no vector regs for varargs
            Asm_EmitCall(node->an_funcname);
            if (realign) {
                Asm_EmitAddImm(8, ASM_REG_RSP);
            }
        } break;
        default: {
            // Binary operators: right operand spilled, left operand in %rax.
            Gen_EmitExpr(node->an_rhs);
            Gen_EmitPush();
            Gen_EmitExpr(node->an_lhs);
            Gen_EmitPop(ASM_REG_RDI);
            // now: %rax = lhs, %rdi = rhs

            switch (node->an_kind) {
                case AST_NODE_KIND_ADD: {
                    Asm_EmitAdd(ASM_REG_RDI, ASM_REG_RAX);
                } break;
                case AST_NODE_KIND_SUB: {
                    Asm_EmitSub(ASM_REG_RDI, ASM_REG_RAX);
                } break;
                case AST_NODE_KIND_MUL: {
                    Asm_EmitImul(ASM_REG_RDI, ASM_REG_RAX);
                } break;
                case AST_NODE_KIND_DIV: {
                    Asm_EmitCqo();
                    Asm_EmitIdiv(ASM_REG_RDI);
                } break;
                case AST_NODE_KIND_MOD: {
                    Asm_EmitCqo();
                    Asm_EmitIdiv(ASM_REG_RDI);
                    Asm_EmitMovRR(ASM_REG_RDX, ASM_REG_RAX);
                } break;
                case AST_NODE_KIND_EQ: {
                    Asm_EmitCmp(ASM_REG_RDI, ASM_REG_RAX);
                    Asm_EmitSete(ASM_REG_RAX);
                    Asm_EmitMovzb(ASM_REG_RAX, ASM_REG_RAX);
                } break;
                case AST_NODE_KIND_NE: {
                    Asm_EmitCmp(ASM_REG_RDI, ASM_REG_RAX);
                    Asm_EmitSetne(ASM_REG_RAX);
                    Asm_EmitMovzb(ASM_REG_RAX, ASM_REG_RAX);
                } break;
                case AST_NODE_KIND_LT: {
                    Asm_EmitCmp(ASM_REG_RDI, ASM_REG_RAX);
                    Asm_EmitSetl(ASM_REG_RAX);
                    Asm_EmitMovzb(ASM_REG_RAX, ASM_REG_RAX);
                } break;
                case AST_NODE_KIND_LE: {
                    Asm_EmitCmp(ASM_REG_RDI, ASM_REG_RAX);
                    Asm_EmitSetle(ASM_REG_RAX);
                    Asm_EmitMovzb(ASM_REG_RAX, ASM_REG_RAX);
                } break;
                default: {
                    error("codegen: unexpected node kind %d", node->an_kind);
                }
            }
        }
    }
}

// Emits code for a statement.
void Gen_EmitStmt(Ast_Node *node)
{
    switch (node->an_kind) {
        case AST_NODE_KIND_RETURN: {
            if (node->an_lhs) {
                Gen_EmitExpr(node->an_lhs);
            } else {
                Asm_EmitMovImm(0, ASM_REG_RAX);
            }
            Asm_EmitJmp(".L.return.%s", Gen_CurrFunc);
        } break;
        case AST_NODE_KIND_IF: {
            int count = Gen_Count();
            Gen_EmitExpr(node->an_cond);
            Asm_EmitCmpImm(0, ASM_REG_RAX);
            Asm_EmitJe(".L.else.%d", count);
            Gen_EmitStmt(node->an_then);
            Asm_EmitJmp(".L.endif.%d", count);
            Asm_EmitLabel(".L.else.%d", count);
            if (node->an_els) {
                Gen_EmitStmt(node->an_els);
            }
            Asm_EmitLabel(".L.endif.%d", count);
        } break;
        case AST_NODE_KIND_FOR: {
            int count = Gen_Count();
            if (node->an_init) {
                Gen_EmitExpr(node->an_init);
            }
            Asm_EmitLabel(".L.begin.%d", count);
            if (node->an_cond) {
                Gen_EmitExpr(node->an_cond);
                Asm_EmitCmpImm(0, ASM_REG_RAX);
                Asm_EmitJe(".L.endfor.%d", count);
            }
            Gen_EmitStmt(node->an_body);
            if (node->an_inc) {
                Gen_EmitExpr(node->an_inc);
            }
            Asm_EmitJmp(".L.begin.%d", count);
            Asm_EmitLabel(".L.endfor.%d", count);
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
void Gen_AssignLvarOffsets(Ast_Func *func)
{
    int offset = 0;
    for (Ast_Var *var = func->af_locals; var; var = var->av_next) {
        offset += 8;
        var->av_offset = -offset;
    }
    func->af_stack_size = Gen_AlignTo(offset, 16);
}

// Emits the .rodata section holding all string literals.
void Gen_EmitDataSection(void)
{
    if (Ast_NumStrings == 0) {
        return;
    }
    Asm_EmitSection(ASM_SECTION_RODATA);
    for (int i = 0; i < Ast_NumStrings; i++) {
        Asm_EmitLabel(".Lstr%d", i);
        // Emit raw bytes (including the terminating NUL) so any contents survive.
        Asm_EmitBytes(Ast_Strings[i], strlen(Ast_Strings[i]) + 1);
    }
}

// Emits the .text section: prologue, body and epilogue for each function.
void Gen_EmitTextSection(Ast_Func *prog)
{
    Asm_EmitSection(ASM_SECTION_TEXT);
    for (Ast_Func *func = prog; func; func = func->af_next) {
        Gen_AssignLvarOffsets(func);
        Gen_CurrFunc = func->af_name;

        Asm_EmitGlobl(func->af_name);
        Asm_EmitLabel(func->af_name);

        // prologue
        Asm_EmitPush(ASM_REG_RBP);
        Asm_EmitMovRR(ASM_REG_RSP, ASM_REG_RBP);
        if (func->af_stack_size) {
            Asm_EmitSubImm(func->af_stack_size, ASM_REG_RSP);
        }

        // spill incoming parameters to their stack slots
        int i = 0;
        for (Ast_Var *param = func->af_params; param; param = param->av_param_next) {
            Asm_EmitMovStore(Gen_ArgReg[i++], ASM_REG_RBP, param->av_offset);
        }

        Gen_EmitStmt(func->af_body);

        // epilogue (fall-through returns 0)
        Asm_EmitMovImm(0, ASM_REG_RAX);
        Asm_EmitLabel(".L.return.%s", func->af_name);
        Asm_EmitMovRR(ASM_REG_RBP, ASM_REG_RSP);
        Asm_EmitPop(ASM_REG_RBP);
        Asm_EmitRet();
    }
}

// Emits assembly for the whole program to out.
void Gen_Codegen(FILE *out, Ast_Func *prog)
{
    Asm_Reset();
    Gen_Depth   = 0;
    Gen_LabelId = 0;

    Asm_EmitDirective(".file \"cc\"");
    Gen_EmitDataSection();
    Gen_EmitTextSection(prog);

    // Mark the stack as non-executable to silence the linker warning.
    Asm_EmitDirective(".section .note.GNU-stack,\"\",@progbits");

    Asm_PrintText(out);
}

// Input stream read by the generated lexer.
extern FILE *yyin;

// Entry point of the generated parser; fills in Ast_Program.
int yyparse(void);

// Show usage information and exits.
static void Show_Usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s [options] INPUT.c\n"
        "  -o OUTPUT   write assembly to OUTPUT (default: INPUT.s)\n",
        prog);
    exit(1);
}

// Main function
int main(int argc, char **argv)
{
    const char *output = NULL;

    int opt;
    while ((opt = getopt(argc, argv, "o:cESgI:D:U:l:L:W:f:m:O::")) != -1) {
        switch (opt) {
            case 'o': {
                output = optarg;
            } break;
            case 'c': case 'E': case 'S': case 'g':
            case 'I': case 'D': case 'U': case 'l':
            case 'L': case 'W': case 'f': case 'm':
            case 'O': {
                // Recognised compiler flag with no effect here; ignore it.
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
