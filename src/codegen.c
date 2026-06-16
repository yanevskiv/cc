/* codegen.c - emit x86-64 assembly (System V AMD64 ABI, AT&T syntax).
 *
 * The strategy is the classic "stack machine": every expression leaves its
 * result in %rax, and temporaries are spilled to the hardware stack with
 * push/pop.  It is not fast code, but it is small and easy to follow -- the
 * point of this base is clarity, not optimisation.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "codegen.h"

static FILE *out;
static int   depth;        /* number of values currently push()ed   */
static int   label_id;     /* source of unique label numbers        */
static const char *cur_fn; /* name of the function being emitted     */

static const char *argreg[6] = { "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9" };

static void emit(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(out, fmt, ap);
    va_end(ap);
    fputc('\n', out);
}

static int count(void) { return label_id++; }

static void push(void)
{
    emit("  push %%rax");
    depth++;
}

static void pop(const char *reg)
{
    emit("  pop %s", reg);
    depth--;
}

static int align_to(int n, int align)
{
    return (n + align - 1) / align * align;
}

static void gen_expr(Node *node);
static void gen_stmt(Node *node);

/* Compute the address of an lvalue into %rax. */
static void gen_addr(Node *node)
{
    if (node->kind == ND_VAR) {
        emit("  lea %d(%%rbp), %%rax", node->var->offset);
        return;
    }
    error("codegen: not an lvalue");
}

static void gen_expr(Node *node)
{
    switch (node->kind) {
    case ND_NUM:
        emit("  mov $%ld, %%rax", node->val);
        return;
    case ND_STR:
        emit("  lea .Lstr%d(%%rip), %%rax", node->str_idx);
        return;
    case ND_VAR:
        gen_addr(node);
        emit("  mov (%%rax), %%rax");
        return;
    case ND_ASSIGN:
        gen_addr(node->lhs);
        push();
        gen_expr(node->rhs);
        pop("%rdi");
        emit("  mov %%rax, (%%rdi)");
        return;
    case ND_NEG:
        gen_expr(node->lhs);
        emit("  neg %%rax");
        return;
    case ND_NOT:
        gen_expr(node->lhs);
        emit("  cmp $0, %%rax");
        emit("  sete %%al");
        emit("  movzb %%al, %%rax");
        return;
    case ND_AND: {
        int c = count();
        gen_expr(node->lhs);
        emit("  cmp $0, %%rax");
        emit("  je .L.false.%d", c);
        gen_expr(node->rhs);
        emit("  cmp $0, %%rax");
        emit("  je .L.false.%d", c);
        emit("  mov $1, %%rax");
        emit("  jmp .L.end.%d", c);
        emit(".L.false.%d:", c);
        emit("  mov $0, %%rax");
        emit(".L.end.%d:", c);
        return;
    }
    case ND_OR: {
        int c = count();
        gen_expr(node->lhs);
        emit("  cmp $0, %%rax");
        emit("  jne .L.true.%d", c);
        gen_expr(node->rhs);
        emit("  cmp $0, %%rax");
        emit("  jne .L.true.%d", c);
        emit("  mov $0, %%rax");
        emit("  jmp .L.end.%d", c);
        emit(".L.true.%d:", c);
        emit("  mov $1, %%rax");
        emit(".L.end.%d:", c);
        return;
    }
    case ND_CALL: {
        int nargs = 0;
        for (Node *a = node->args; a; a = a->next) {
            gen_expr(a);
            push();
            nargs++;
        }
        for (int i = nargs - 1; i >= 0; i--)
            pop(argreg[i]);

        /* The ABI requires %rsp to be 16-byte aligned at the `call`.  Each
         * outstanding push() moved it by 8, so realign when depth is odd. */
        int realign = depth % 2;
        if (realign)
            emit("  sub $8, %%rsp");
        emit("  mov $0, %%al");          /* no vector regs for varargs */
        emit("  call %s", node->funcname);
        if (realign)
            emit("  add $8, %%rsp");
        return;
    }
    default:
        break;
    }

    /* Binary operators: right operand spilled, left operand in %rax. */
    gen_expr(node->rhs);
    push();
    gen_expr(node->lhs);
    pop("%rdi");
    /* now: %rax = lhs, %rdi = rhs */

    switch (node->kind) {
    case ND_ADD: emit("  add %%rdi, %%rax"); return;
    case ND_SUB: emit("  sub %%rdi, %%rax"); return;
    case ND_MUL: emit("  imul %%rdi, %%rax"); return;
    case ND_DIV: emit("  cqo"); emit("  idiv %%rdi"); return;
    case ND_MOD: emit("  cqo"); emit("  idiv %%rdi"); emit("  mov %%rdx, %%rax"); return;
    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE:
        emit("  cmp %%rdi, %%rax");
        if      (node->kind == ND_EQ) emit("  sete %%al");
        else if (node->kind == ND_NE) emit("  setne %%al");
        else if (node->kind == ND_LT) emit("  setl %%al");
        else                          emit("  setle %%al");
        emit("  movzb %%al, %%rax");
        return;
    default:
        error("codegen: unexpected node kind %d", node->kind);
    }
}

static void gen_stmt(Node *node)
{
    switch (node->kind) {
    case ND_RETURN:
        if (node->lhs)
            gen_expr(node->lhs);
        else
            emit("  mov $0, %%rax");
        emit("  jmp .L.return.%s", cur_fn);
        return;
    case ND_IF: {
        int c = count();
        gen_expr(node->cond);
        emit("  cmp $0, %%rax");
        emit("  je .L.else.%d", c);
        gen_stmt(node->then);
        emit("  jmp .L.endif.%d", c);
        emit(".L.else.%d:", c);
        if (node->els)
            gen_stmt(node->els);
        emit(".L.endif.%d:", c);
        return;
    }
    case ND_FOR: {
        int c = count();
        if (node->init)
            gen_expr(node->init);
        emit(".L.begin.%d:", c);
        if (node->cond) {
            gen_expr(node->cond);
            emit("  cmp $0, %%rax");
            emit("  je .L.endfor.%d", c);
        }
        gen_stmt(node->body);
        if (node->inc)
            gen_expr(node->inc);
        emit("  jmp .L.begin.%d", c);
        emit(".L.endfor.%d:", c);
        return;
    }
    case ND_BLOCK:
        for (Node *n = node->body; n; n = n->next)
            gen_stmt(n);
        return;
    case ND_EXPR_STMT:
        gen_expr(node->lhs);
        return;
    case ND_NOP:
        return;
    default:
        error("codegen: unexpected statement kind %d", node->kind);
    }
}

static void assign_lvar_offsets(Function *fn)
{
    int offset = 0;
    for (Var *v = fn->locals; v; v = v->next) {
        offset += 8;
        v->offset = -offset;
    }
    fn->stack_size = align_to(offset, 16);
}

static void emit_data(void)
{
    if (num_strings == 0)
        return;
    emit("  .section .rodata");
    for (int i = 0; i < num_strings; i++) {
        emit(".Lstr%d:", i);
        /* Emit raw bytes so that any contents survive intact. */
        for (char *p = strings[i]; *p; p++)
            emit("  .byte %d", (unsigned char)*p);
        emit("  .byte 0");
    }
}

static void emit_text(Function *prog)
{
    emit("  .text");
    for (Function *fn = prog; fn; fn = fn->next) {
        assign_lvar_offsets(fn);
        cur_fn = fn->name;

        emit("  .globl %s", fn->name);
        emit("%s:", fn->name);

        /* prologue */
        emit("  push %%rbp");
        emit("  mov %%rsp, %%rbp");
        if (fn->stack_size)
            emit("  sub $%d, %%rsp", fn->stack_size);

        /* spill incoming parameters to their stack slots */
        int i = 0;
        for (Var *p = fn->params; p; p = p->param_next)
            emit("  mov %s, %d(%%rbp)", argreg[i++], p->offset);

        gen_stmt(fn->body);

        /* epilogue (fall-through returns 0) */
        emit("  mov $0, %%rax");
        emit(".L.return.%s:", fn->name);
        emit("  mov %%rbp, %%rsp");
        emit("  pop %%rbp");
        emit("  ret");
    }
}

void codegen(FILE *fp, Function *prog)
{
    out      = fp;
    depth    = 0;
    label_id = 0;

    emit("  .file \"cc\"");
    emit_data();
    emit_text(prog);
    /* Mark the stack as non-executable to silence the linker warning. */
    emit("  .section .note.GNU-stack,\"\",@progbits");
}
