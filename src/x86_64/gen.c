#include <string.h>

#include "common.h"
#include "ast/ast.h"
#include "util/elf.h"
#include "x86_64/asm.h"
#include "x86_64/gen.h"

// Number of integer arguments the ABI passes in registers; the rest go on the stack.
#define MAX_REG_ARGS 6

// Size in bytes of a stack slot and a general-purpose register.
#define WORD_SIZE 8

// Required %rsp alignment, in bytes, at the point of a `call`.
#define STACK_ALIGN 16

// Linux x86-64 syscall number for exit(2).
#define SYS_EXIT 60

// Number of values currently pushed with Gen_x86_64_EmitPush().
static int Gen_x86_64_Depth;

// Source of unique label numbers.
static int Gen_x86_64_LabelId;

// Name of the function currently being emitted.
static const char *Gen_x86_64_CurrFunc;

// Registers used to pass the first six integer arguments, in ABI order.
static const Asm_x86_64_Reg Gen_x86_64_ArgReg[6] = {
    ASM_X86_64_REG_RDI,
    ASM_X86_64_REG_RSI,
    ASM_X86_64_REG_RDX,
    ASM_X86_64_REG_RCX,
    ASM_X86_64_REG_R8,
    ASM_X86_64_REG_R9
};

// Returns the next unique label number.
int Gen_x86_64_Count(void)
{
    return Gen_x86_64_LabelId++;
}

// Pushes %rax onto the stack and tracks the depth.
void Gen_x86_64_EmitPush(void)
{
    Asm_x86_64_EmitPush(ASM_X86_64_REG_RAX);
    Gen_x86_64_Depth++;
}

// Pops the top of the stack into reg and tracks the depth.
void Gen_x86_64_EmitPop(Asm_x86_64_Reg reg)
{
    Asm_x86_64_EmitPop(reg);
    Gen_x86_64_Depth--;
}

// Rounds n up to the nearest multiple of align.
int Gen_x86_64_AlignTo(int n, int align)
{
    return (n + align - 1) / align * align;
}

// Computes the address of an lvalue into %rax.
void Gen_x86_64_EmitAddr(Ast_Node *node)
{
    if (node->an_kind == AST_NODE_KIND_VAR) {
        Asm_x86_64_EmitLea(ASM_X86_64_REG_RBP, node->an_var->av_offset, ASM_X86_64_REG_RAX);
        return;
    }
    error("codegen: not an lvalue");
}

// Counts the arguments in a call's argument list.
static int Gen_x86_64_CallCountArgs(Ast_Node *args)
{
    int nArgs = 0;
    for (Ast_Node *arg = args; arg; arg = arg->an_next) {
        nArgs++;
    }
    return nArgs;
}

// Evaluates call arguments and pushes them so the first lands on top.
static void Gen_x86_64_CallPushArgs(Ast_Node *arg)
{
    if (!arg) {
        return;
    }
    Gen_x86_64_CallPushArgs(arg->an_next);
    Gen_x86_64_EmitExpr(arg);
    Gen_x86_64_EmitPush();
}

// Pops the first nReg pushed arguments into the ABI argument registers.
static void Gen_x86_64_CallPopArgs(int nReg)
{
    for (int i = 0; i < nReg; i++) {
        Gen_x86_64_EmitPop(Gen_x86_64_ArgReg[i]);
    }
}

// Emits code for an expression, leaving its result in %rax.
void Gen_x86_64_EmitExpr(Ast_Node *node)
{
    switch (node->an_kind) {
        case AST_NODE_KIND_NUM: {
            Asm_x86_64_EmitMovImm(node->an_val, ASM_X86_64_REG_RAX);
        } break;
        case AST_NODE_KIND_STR: {
            Asm_x86_64_EmitLeaRip(ASM_X86_64_REG_RAX, ".Lstr%d", node->an_str_idx);
        } break;
        case AST_NODE_KIND_VAR: {
            Gen_x86_64_EmitAddr(node);
            Asm_x86_64_EmitMovLoad(ASM_X86_64_REG_RAX, 0, ASM_X86_64_REG_RAX);
        } break;
        case AST_NODE_KIND_ASSIGN: {
            Gen_x86_64_EmitAddr(node->an_lhs);
            Gen_x86_64_EmitPush();
            Gen_x86_64_EmitExpr(node->an_rhs);
            Gen_x86_64_EmitPop(ASM_X86_64_REG_RDI);
            Asm_x86_64_EmitMovStore(ASM_X86_64_REG_RAX, ASM_X86_64_REG_RDI, 0);
        } break;
        case AST_NODE_KIND_NEG: {
            Gen_x86_64_EmitExpr(node->an_lhs);
            Asm_x86_64_EmitNeg(ASM_X86_64_REG_RAX);
        } break;
        case AST_NODE_KIND_NOT: {
            Gen_x86_64_EmitExpr(node->an_lhs);
            Asm_x86_64_EmitCmpImm(0, ASM_X86_64_REG_RAX);
            Asm_x86_64_EmitSete(ASM_X86_64_REG_RAX);
            Asm_x86_64_EmitMovzb(ASM_X86_64_REG_RAX, ASM_X86_64_REG_RAX);
        } break;
        case AST_NODE_KIND_AND: {
            int count = Gen_x86_64_Count();
            Gen_x86_64_EmitExpr(node->an_lhs);
            Asm_x86_64_EmitCmpImm(0, ASM_X86_64_REG_RAX);
            Asm_x86_64_EmitJe(".L.false.%d", count);
            Gen_x86_64_EmitExpr(node->an_rhs);
            Asm_x86_64_EmitCmpImm(0, ASM_X86_64_REG_RAX);
            Asm_x86_64_EmitJe(".L.false.%d", count);
            Asm_x86_64_EmitMovImm(1, ASM_X86_64_REG_RAX);
            Asm_x86_64_EmitJmp(".L.end.%d", count);
            Asm_x86_64_EmitLabel(".L.false.%d", count);
            Asm_x86_64_EmitMovImm(0, ASM_X86_64_REG_RAX);
            Asm_x86_64_EmitLabel(".L.end.%d", count);
        } break;
        case AST_NODE_KIND_OR: {
            int count = Gen_x86_64_Count();
            Gen_x86_64_EmitExpr(node->an_lhs);
            Asm_x86_64_EmitCmpImm(0, ASM_X86_64_REG_RAX);
            Asm_x86_64_EmitJne(".L.true.%d", count);
            Gen_x86_64_EmitExpr(node->an_rhs);
            Asm_x86_64_EmitCmpImm(0, ASM_X86_64_REG_RAX);
            Asm_x86_64_EmitJne(".L.true.%d", count);
            Asm_x86_64_EmitMovImm(0, ASM_X86_64_REG_RAX);
            Asm_x86_64_EmitJmp(".L.end.%d", count);
            Asm_x86_64_EmitLabel(".L.true.%d", count);
            Asm_x86_64_EmitMovImm(1, ASM_X86_64_REG_RAX);
            Asm_x86_64_EmitLabel(".L.end.%d", count);
        } break;
        case AST_NODE_KIND_CALL: {
            int nArgs  = Gen_x86_64_CallCountArgs(node->an_args);
            int nReg   = nArgs < MAX_REG_ARGS ? nArgs : MAX_REG_ARGS;
            int nStack = nArgs - nReg;

            int nAlignPad = (Gen_x86_64_Depth + nStack) % (STACK_ALIGN / WORD_SIZE);
            if (nAlignPad) {
                Asm_x86_64_EmitSubImm(WORD_SIZE, ASM_X86_64_REG_RSP);
                Gen_x86_64_Depth++;
            }

            Gen_x86_64_CallPushArgs(node->an_args);
            Gen_x86_64_CallPopArgs(nReg);

            Asm_x86_64_EmitMovImm8(0, ASM_X86_64_REG_RAX);
            Asm_x86_64_EmitCall(node->an_funcname);

            if (nStack + nAlignPad > 0) {
                Asm_x86_64_EmitAddImm(WORD_SIZE * (nStack + nAlignPad), ASM_X86_64_REG_RSP);
                Gen_x86_64_Depth -= nStack + nAlignPad;
            }
        } break;
        default: {
            Gen_x86_64_EmitExpr(node->an_rhs);
            Gen_x86_64_EmitPush();
            Gen_x86_64_EmitExpr(node->an_lhs);
            Gen_x86_64_EmitPop(ASM_X86_64_REG_RDI);

            switch (node->an_kind) {
                case AST_NODE_KIND_ADD: {
                    Asm_x86_64_EmitAdd(ASM_X86_64_REG_RDI, ASM_X86_64_REG_RAX);
                } break;
                case AST_NODE_KIND_SUB: {
                    Asm_x86_64_EmitSub(ASM_X86_64_REG_RDI, ASM_X86_64_REG_RAX);
                } break;
                case AST_NODE_KIND_MUL: {
                    Asm_x86_64_EmitImul(ASM_X86_64_REG_RDI, ASM_X86_64_REG_RAX);
                } break;
                case AST_NODE_KIND_DIV: {
                    Asm_x86_64_EmitCqo();
                    Asm_x86_64_EmitIdiv(ASM_X86_64_REG_RDI);
                } break;
                case AST_NODE_KIND_MOD: {
                    Asm_x86_64_EmitCqo();
                    Asm_x86_64_EmitIdiv(ASM_X86_64_REG_RDI);
                    Asm_x86_64_EmitMovRR(ASM_X86_64_REG_RDX, ASM_X86_64_REG_RAX);
                } break;
                case AST_NODE_KIND_EQ: {
                    Asm_x86_64_EmitCmp(ASM_X86_64_REG_RDI, ASM_X86_64_REG_RAX);
                    Asm_x86_64_EmitSete(ASM_X86_64_REG_RAX);
                    Asm_x86_64_EmitMovzb(ASM_X86_64_REG_RAX, ASM_X86_64_REG_RAX);
                } break;
                case AST_NODE_KIND_NE: {
                    Asm_x86_64_EmitCmp(ASM_X86_64_REG_RDI, ASM_X86_64_REG_RAX);
                    Asm_x86_64_EmitSetne(ASM_X86_64_REG_RAX);
                    Asm_x86_64_EmitMovzb(ASM_X86_64_REG_RAX, ASM_X86_64_REG_RAX);
                } break;
                case AST_NODE_KIND_LT: {
                    Asm_x86_64_EmitCmp(ASM_X86_64_REG_RDI, ASM_X86_64_REG_RAX);
                    Asm_x86_64_EmitSetl(ASM_X86_64_REG_RAX);
                    Asm_x86_64_EmitMovzb(ASM_X86_64_REG_RAX, ASM_X86_64_REG_RAX);
                } break;
                case AST_NODE_KIND_LE: {
                    Asm_x86_64_EmitCmp(ASM_X86_64_REG_RDI, ASM_X86_64_REG_RAX);
                    Asm_x86_64_EmitSetle(ASM_X86_64_REG_RAX);
                    Asm_x86_64_EmitMovzb(ASM_X86_64_REG_RAX, ASM_X86_64_REG_RAX);
                } break;
                default: {
                    error("codegen: unexpected node kind %d", node->an_kind);
                }
            }
        }
    }
}

// Emits code for a statement.
void Gen_x86_64_EmitStmt(Ast_Node *node)
{
    switch (node->an_kind) {
        case AST_NODE_KIND_RETURN: {
            if (node->an_lhs) {
                Gen_x86_64_EmitExpr(node->an_lhs);
            } else {
                Asm_x86_64_EmitMovImm(0, ASM_X86_64_REG_RAX);
            }
            Asm_x86_64_EmitJmp(".L.return.%s", Gen_x86_64_CurrFunc);
        } break;
        case AST_NODE_KIND_IF: {
            int count = Gen_x86_64_Count();
            Gen_x86_64_EmitExpr(node->an_cond);
            Asm_x86_64_EmitCmpImm(0, ASM_X86_64_REG_RAX);
            Asm_x86_64_EmitJe(".L.else.%d", count);
            Gen_x86_64_EmitStmt(node->an_then);
            Asm_x86_64_EmitJmp(".L.endif.%d", count);
            Asm_x86_64_EmitLabel(".L.else.%d", count);
            if (node->an_els) {
                Gen_x86_64_EmitStmt(node->an_els);
            }
            Asm_x86_64_EmitLabel(".L.endif.%d", count);
        } break;
        case AST_NODE_KIND_FOR: {
            int count = Gen_x86_64_Count();
            if (node->an_init) {
                Gen_x86_64_EmitExpr(node->an_init);
            }
            Asm_x86_64_EmitLabel(".L.begin.%d", count);
            if (node->an_cond) {
                Gen_x86_64_EmitExpr(node->an_cond);
                Asm_x86_64_EmitCmpImm(0, ASM_X86_64_REG_RAX);
                Asm_x86_64_EmitJe(".L.endfor.%d", count);
            }
            Gen_x86_64_EmitStmt(node->an_body);
            if (node->an_inc) {
                Gen_x86_64_EmitExpr(node->an_inc);
            }
            Asm_x86_64_EmitJmp(".L.begin.%d", count);
            Asm_x86_64_EmitLabel(".L.endfor.%d", count);
        } break;
        case AST_NODE_KIND_BLOCK: {
            for (Ast_Node *stmt = node->an_body; stmt; stmt = stmt->an_next) {
                Gen_x86_64_EmitStmt(stmt);
            }
        } break;
        case AST_NODE_KIND_EXPR_STMT: {
            Gen_x86_64_EmitExpr(node->an_lhs);
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
void Gen_x86_64_AssignLvarOffsets(Ast_Func *func)
{
    int offset = 0;
    for (Ast_Var *var = func->af_locals; var; var = var->av_next) {
        offset += WORD_SIZE;
        var->av_offset = -offset;
    }
    func->af_stack_size = Gen_x86_64_AlignTo(offset, STACK_ALIGN);
}

// Emits the .rodata section holding all string literals.
void Gen_x86_64_EmitDataSection(void)
{
    int count = Ast_StringCount();
    if (count == 0) {
        return;
    }
    Asm_x86_64_EmitSection(ASM_X86_64_SECTION_RODATA);
    for (int i = 0; i < count; i++) {
        char *str = Ast_StringAt(i);
        Asm_x86_64_EmitLabel(".Lstr%d", i);
        Asm_x86_64_EmitBytes(str, strlen(str) + 1);
    }
}

// Emits the _start runtime: call main, then exit with its return value.
static void Gen_x86_64_EmitStartup(void)
{
    Asm_x86_64_EmitGlobl("_start");
    Asm_x86_64_EmitLabel("_start");
    Asm_x86_64_EmitCall("main");
    Asm_x86_64_EmitMovRR(ASM_X86_64_REG_RAX, ASM_X86_64_REG_RDI);
    Asm_x86_64_EmitMovImm(SYS_EXIT, ASM_X86_64_REG_RAX);
    Asm_x86_64_EmitSyscall();
}

// Emits the prologue, body and epilogue for every function.
static void Gen_x86_64_EmitFunctions(Ast_Func *prog)
{
    for (Ast_Func *func = prog; func; func = func->af_next) {
        Gen_x86_64_AssignLvarOffsets(func);
        Gen_x86_64_CurrFunc = func->af_name;

        Asm_x86_64_EmitGlobl(func->af_name);
        Asm_x86_64_EmitLabel(func->af_name);

        // prologue
        Asm_x86_64_EmitPush(ASM_X86_64_REG_RBP);
        Asm_x86_64_EmitMovRR(ASM_X86_64_REG_RSP, ASM_X86_64_REG_RBP);
        if (func->af_stack_size) {
            Asm_x86_64_EmitSubImm(func->af_stack_size, ASM_X86_64_REG_RSP);
        }

        // spill incoming parameters
        int i = 0;
        for (Ast_Var *param = func->af_params; param; param = param->av_param_next) {
            if (i < MAX_REG_ARGS) {
                Asm_x86_64_EmitMovStore(Gen_x86_64_ArgReg[i], ASM_X86_64_REG_RBP, param->av_offset);
            } else {
                Asm_x86_64_EmitMovLoad(ASM_X86_64_REG_RBP, 2 * WORD_SIZE + (i - MAX_REG_ARGS) * WORD_SIZE, ASM_X86_64_REG_RAX);
                Asm_x86_64_EmitMovStore(ASM_X86_64_REG_RAX, ASM_X86_64_REG_RBP, param->av_offset);
            }
            i++;
        }

        Gen_x86_64_EmitStmt(func->af_body);

        // epilogue
        Asm_x86_64_EmitMovImm(0, ASM_X86_64_REG_RAX);
        Asm_x86_64_EmitLabel(".L.return.%s", func->af_name);
        Asm_x86_64_EmitMovRR(ASM_X86_64_REG_RBP, ASM_X86_64_REG_RSP);
        Asm_x86_64_EmitPop(ASM_X86_64_REG_RBP);
        Asm_x86_64_EmitRet();
    }
}

// Emits the .text section; freestanding output also gets a _start runtime.
void Gen_x86_64_EmitTextSection(Ast_Func *prog, int freestanding)
{
    Asm_x86_64_EmitSection(ASM_X86_64_SECTION_TEXT);
    if (freestanding) {
        Gen_x86_64_EmitStartup();
    }
    Gen_x86_64_EmitFunctions(prog);
}

// Builds the instruction list for the whole program.
static void Gen_x86_64_BuildProgram(Ast_Func *prog, int freestanding)
{
    Asm_x86_64_Reset();
    Gen_x86_64_Depth   = 0;
    Gen_x86_64_LabelId = 0;

    Asm_x86_64_EmitDirective(".file \"cc\"");
    Gen_x86_64_EmitDataSection();
    Gen_x86_64_EmitTextSection(prog, freestanding);
    Asm_x86_64_EmitDirective(".section .note.GNU-stack,\"\",@progbits");
}

// Emits assembly text for the whole program to out.
void Gen_x86_64_Codegen(FILE *out, Ast_Func *prog)
{
    Gen_x86_64_BuildProgram(prog, 0);
    Asm_x86_64_PrintText(out);
}

// Encodes the whole program as a freestanding ELF executable to out.
void Gen_x86_64_CodegenElf(FILE *out, Ast_Func *prog)
{
    Elf_Reset();
    Gen_x86_64_BuildProgram(prog, 1);
    Asm_x86_64_Encode();
    Elf_Finish(out, "_start");
}
