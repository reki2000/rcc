/*
 * emit_ir.c - x64 code generation from the SSA backend IR
 *
 * Register conventions inside a function body:
 *
 *   %rbx %r12 %r13 %r14 %r15   allocatable, callee saved (saved in the frame)
 *   %r10                       allocatable, only for values not live across a call
 *   %r11 %rcx                  scratch for the first / second operand
 *   %rax                       scratch for the result and for block copies
 *   %rdx                       scratch (also clobbered by idiv)
 *   %rdi %rsi %rdx %rcx %r8 %r9  argument registers, only used while making a call
 *
 * Spilled values live in a slot of the frame and are moved through the
 * scratch registers at each use.
 */
#include "types.h"
#include "rsys.h"
#include "rstring.h"
#include "devtool.h"
#include "vec.h"

#include "token.h"
#include "type.h"
#include "var.h"
#include "func.h"
#include "atom.h"
#include "gstr.h"
#include "ir.h"
#include "emit.h"

#define EI_SCRATCH0 R_11
#define EI_SCRATCH1 R_CX
#define EI_RESULT R_AX

int *ei_block_label;
int ei_epilogue_label = 0;
int ei_frame_bytes = 0;
int ei_callee_slot[R_LAST];

bool ei_is_callee_saved(int r) {
    return (r == R_BX || r == R_12 || r == R_13 || r == R_14 || r == R_15);
}

/* the register holding 'vreg', loading it into 'scratch' when it is spilled */
int ei_operand(int vreg, int scratch) {
    if (vreg < 0) {
        error("missing operand in the IR");
    }
    if (vreg_reg[vreg] >= 0) {
        return vreg_reg[vreg];
    }
    if (!vreg_slot[vreg]) {
        error("vreg v%d has neither a register nor a slot", vreg);
    }
    genf(" movq %d(%%rbp), %s", -vreg_slot[vreg], reg(scratch, 8));
    return scratch;
}

/* the register a result should be produced in */
int ei_dst(int vreg) {
    if (vreg >= 0 && vreg_reg[vreg] >= 0) {
        return vreg_reg[vreg];
    }
    return EI_RESULT;
}

void ei_writeback(int vreg, int from) {
    if (vreg < 0) {
        return;
    }
    if (vreg_reg[vreg] >= 0) {
        if (vreg_reg[vreg] != from) {
            genf(" movq %s, %s", reg(from, 8), reg(vreg_reg[vreg], 8));
        }
        return;
    }
    if (!vreg_slot[vreg]) {
        error("vreg v%d has no slot to write back to", vreg);
    }
    genf(" movq %s, %d(%%rbp)", reg(from, 8), -vreg_slot[vreg]);
}

/* block copy of 'size' bytes from (from) to (to), using %rax as a scratch */
void ei_copy(int size, int from, int to) {
    int offset = 0;
    while (size >= 8) {
        genf(" movq %d(%s), %%rax", offset, reg(from, 8));
        genf(" movq %%rax, %d(%s)", offset, reg(to, 8));
        size -= 8;
        offset += 8;
    }
    while (size >= 4) {
        genf(" movl %d(%s), %%eax", offset, reg(from, 8));
        genf(" movl %%eax, %d(%s)", offset, reg(to, 8));
        size -= 4;
        offset += 4;
    }
    while (size > 0) {
        genf(" movb %d(%s), %%al", offset, reg(from, 8));
        genf(" movb %%al, %d(%s)", offset, reg(to, 8));
        size--;
        offset++;
    }
}

char *ei_binop_name(int op) {
    switch (op) {
        case IR_ADD: return "add";
        case IR_SUB: return "sub";
        case IR_AND: return "and";
        case IR_OR: return "or";
        case IR_XOR: return "xor";
    }
    error("not a simple binary operator: %d", op);
    return "";
}

char *ei_setcc_name(int op) {
    switch (op) {
        case IR_EQ: return "e";
        case IR_NE: return "ne";
        case IR_LT: return "nge";
        case IR_LE: return "le";
        case IR_GT: return "nle";
        case IR_GE: return "ge";
    }
    error("not a comparison: %d", op);
    return "";
}

void ei_simple_binop(ir_t *ins) {
    int ra = ei_operand(ins->a0, EI_SCRATCH0);
    int rb = ei_operand(ins->a1, EI_SCRATCH1);
    int rd = ei_dst(ins->dst);
    char *op = ei_binop_name(ins->op);
    int size = ins->size;

    if (rd == rb) {
        rd = EI_RESULT;     // the destination would destroy the second operand
    }
    if (rd != ra) {
        genf(" movq %s, %s", reg(ra, 8), reg(rd, 8));
    }
    genf(" %s%s %s, %s", op, opsize(size), reg(rb, size), reg(rd, size));
    ei_writeback(ins->dst, rd);
}

void ei_shift(ir_t *ins) {
    int ra = ei_operand(ins->a0, EI_SCRATCH0);
    int rb = ei_operand(ins->a1, EI_SCRATCH1);
    int rd = ei_dst(ins->dst);

    if (rb != R_CX) {
        genf(" movq %s, %%rcx", reg(rb, 8));
    }
    if (rd != ra) {
        genf(" movq %s, %s", reg(ra, 8), reg(rd, 8));
    }
    genf(" sa%s%s %%cl, %s", (ins->op == IR_SHL) ? "l" : "r", opsize(ins->size), reg(rd, ins->size));
    ei_writeback(ins->dst, rd);
}

void ei_mul(ir_t *ins) {
    int ra = ei_operand(ins->a0, EI_SCRATCH0);
    int rb = ei_operand(ins->a1, EI_SCRATCH1);
    int size = (ins->size == 1) ? 4 : ins->size;

    genf(" mov%s %s, %s", opsize(size), reg(ra, size), reg(R_AX, size));
    genf(" imul%s %s, %s", opsize(size), reg(rb, size), reg(R_AX, size));
    ei_writeback(ins->dst, R_AX);
}

void ei_divmod(ir_t *ins) {
    int ra = ei_operand(ins->a0, EI_SCRATCH0);
    int rb = ei_operand(ins->a1, EI_SCRATCH1);
    int size = ins->size;

    if (size == 1) {
        // idivb is not modelled; widen both operands keeping their sign
        genf(" movsbl %s, %%eax", reg(ra, 1));
        genf(" movsbl %s, %%ecx", reg(rb, 1));
        rb = R_CX;
        size = 4;
    } else {
        genf(" mov%s %s, %s", opsize(size), reg(ra, size), reg(R_AX, size));
    }
    genf(" %s", (size == 8) ? "cqo" : "cdq");
    genf(" idiv%s %s", opsize(size), reg(rb, size));
    ei_writeback(ins->dst, (ins->op == IR_DIV) ? R_AX : R_DX);
}

void ei_compare(ir_t *ins) {
    int ra = ei_operand(ins->a0, EI_SCRATCH0);
    int rb = ei_operand(ins->a1, EI_SCRATCH1);
    int rd = ei_dst(ins->dst);

    genf(" cmp%s %s, %s", opsize(ins->size), reg(rb, ins->size), reg(ra, ins->size));
    genf(" set%s %s", ei_setcc_name(ins->op), reg(rd, 1));
    genf(" andq $1, %s", reg(rd, 8));
    ei_writeback(ins->dst, rd);
}

void ei_extend(ir_t *ins) {
    int ra = ei_operand(ins->a0, EI_SCRATCH0);
    int rd = ei_dst(ins->dst);

    if (ins->op == IR_SEXT) {
        if (ins->size == 1) {
            genf(" movsbq %s, %s", reg(ra, 1), reg(rd, 8));
        } else if (ins->size == 4) {
            genf(" movslq %s, %s", reg(ra, 4), reg(rd, 8));
        } else if (rd != ra) {
            genf(" movq %s, %s", reg(ra, 8), reg(rd, 8));
        }
    } else {
        if (ins->size == 1) {
            genf(" movzbl %s, %s", reg(ra, 1), reg(rd, 4));
        } else if (ins->size == 4) {
            genf(" movl %s, %s", reg(ra, 4), reg(rd, 4));
        } else if (rd != ra) {
            genf(" movq %s, %s", reg(ra, 8), reg(rd, 8));
        }
    }
    ei_writeback(ins->dst, rd);
}

/* SysV classification: which arguments travel in registers */
void ei_classify_args(ir_t *ins, bool *use_reg, int *num_reg_args, int *stack_size, int *stage_size) {
    int argc = ir_arg_vec_len(ins->args);
    *num_reg_args = 0;
    *stack_size = 0;
    *stage_size = 0;

    for (int i = 0; i < argc; i++) {
        ir_arg_t *a = ir_arg_vec_get(ins->args, i);
        if (a->is_struct) {
            use_reg[i] = ((a->size <= 8 && *num_reg_args < ABI_NUM_GP) ||
                          (a->size <= 16 && *num_reg_args < ABI_NUM_GP - 1));
            if (use_reg[i]) {
                *num_reg_args += (a->size <= 8) ? 1 : 2;
                *stage_size += align(a->size, 8);
            } else {
                *stack_size += align(a->size, 8);
            }
        } else {
            use_reg[i] = (*num_reg_args < ABI_NUM_GP);
            if (use_reg[i]) {
                (*num_reg_args)++;
            } else {
                *stack_size += 8;
            }
        }
    }
}

void ei_call(ir_t *ins) {
    int argc = ir_arg_vec_len(ins->args);
    bool use_reg[NUM_MAX_ARGS];
    int stage_at[NUM_MAX_ARGS];
    int num_reg_args;
    int stack_size;
    int stage_size;

    ei_classify_args(ins, use_reg, &num_reg_args, &stack_size, &stage_size);

    /*
     * %rsp is 16 bytes aligned everywhere but inside this sequence, because
     * the prologue reserves a multiple of 16 and nothing else moves it.
     */
    int total = align(stack_size + stage_size, 16);
    if (total > 0) {
        genf(" subq $%d, %%rsp", total);
    }

    int offset = 0;
    for (int i = 0; i < argc; i++) {
        if (use_reg[i]) {
            continue;
        }
        ir_arg_t *a = ir_arg_vec_get(ins->args, i);
        int r = ei_operand(a->vreg, EI_SCRATCH0);
        if (a->is_struct) {
            genf(" leaq %d(%%rsp), %%rcx", offset);
            ei_copy(a->size, r, R_CX);
            offset += align(a->size, 8);
        } else {
            genf(" movq %s, %d(%%rsp)", reg(r, 8), offset);
            offset += 8;
        }
    }

    // structs passed in registers are staged on the stack, so that reading a
    // whole 8 byte word never touches memory outside the object
    int stage_offset = stack_size;
    for (int i = 0; i < argc; i++) {
        ir_arg_t *a = ir_arg_vec_get(ins->args, i);
        if (!use_reg[i] || !a->is_struct) {
            continue;
        }
        int r = ei_operand(a->vreg, EI_SCRATCH0);
        genf(" leaq %d(%%rsp), %%rcx", stage_offset);
        ei_copy(a->size, r, R_CX);
        stage_at[i] = stage_offset;
        stage_offset += align(a->size, 8);
    }

    int reg_index = 0;
    for (int i = 0; i < argc; i++) {
        ir_arg_t *a = ir_arg_vec_get(ins->args, i);
        if (!use_reg[i]) {
            continue;
        }
        if (a->is_struct) {
            genf(" movq %d(%%rsp), %s", stage_at[i], reg(reg_index, 8));
            reg_index++;
            if (a->size > 8) {
                genf(" movq %d(%%rsp), %s", stage_at[i] + 8, reg(reg_index, 8));
                reg_index++;
            }
        } else {
            int r = ei_operand(a->vreg, EI_SCRATCH0);
            genf(" movq %s, %s", reg(r, 8), reg(reg_index, 8));
            reg_index++;
        }
    }

    genf(" movb $0, %%al");
    genf(" call %s%s", ins->f->name, ins->f->is_external ? "@PLT" : "");
    if (total > 0) {
        genf(" addq $%d, %%rsp", total);
    }

    if (ins->dst >= 0) {
        if (ins->size == 1) {
            genf(" movzbl %%al, %%eax");
        } else if (ins->size == 4) {
            genf(" movl %%eax, %%eax");
        }
        ei_writeback(ins->dst, R_AX);
    }
}

void ei_insn(ir_t *ins, int next_block) {
    switch (ins->op) {
        case IR_NOP:
        case IR_PHI:
            return;

        case IR_CONST: {
            int rd = ei_dst(ins->dst);
            long imm32_max = 2147483647L;
            long imm32_min = 0 - imm32_max - 1;
            if (ins->size == 8 && (ins->imm > imm32_max || ins->imm < imm32_min)) {
                genf(" movabsq $%ld, %s", ins->imm, reg(rd, 8));
            } else if (ins->size == 8) {
                genf(" movq $%ld, %s", ins->imm, reg(rd, 8));
            } else {
                genf(" movl $%ld, %s", ins->imm, reg(rd, 4));
            }
            ei_writeback(ins->dst, rd);
            break;
        }

        case IR_LOCAL: {
            int rd = ei_dst(ins->dst);
            genf(" leaq %ld(%%rbp), %s", -(ins->imm), reg(rd, 8));
            ei_writeback(ins->dst, rd);
            break;
        }

        case IR_GLOBAL: {
            int rd = ei_dst(ins->dst);
            genf(" leaq %s(%%rip), %s", ins->name, reg(rd, 8));
            ei_writeback(ins->dst, rd);
            break;
        }

        case IR_STRLIT: {
            int rd = ei_dst(ins->dst);
            genf(" leaq .G%ld(%%rip), %s", ins->imm, reg(rd, 8));
            ei_writeback(ins->dst, rd);
            break;
        }

        case IR_ARG: {
            int rd = ei_dst(ins->dst);
            int from = (int)(ins->imm);
            if (ins->size == 1) {
                genf(" movzbl %s, %s", reg(from, 1), reg(rd, 4));
            } else if (ins->size == 4) {
                genf(" movl %s, %s", reg(from, 4), reg(rd, 4));
            } else {
                genf(" movq %s, %s", reg(from, 8), reg(rd, 8));
            }
            ei_writeback(ins->dst, rd);
            break;
        }

        case IR_LOAD: {
            int ra = ei_operand(ins->a0, EI_SCRATCH0);
            int rd = ei_dst(ins->dst);
            if (ins->size == 1) {
                genf(" movzbl (%s), %s", reg(ra, 8), reg(rd, 4));
            } else {
                genf(" mov%s (%s), %s", opsize(ins->size), reg(ra, 8), reg(rd, ins->size));
            }
            ei_writeback(ins->dst, rd);
            break;
        }

        case IR_STORE: {
            int ra = ei_operand(ins->a0, EI_SCRATCH0);
            int rb = ei_operand(ins->a1, EI_SCRATCH1);
            genf(" mov%s %s, (%s)", opsize(ins->size), reg(ra, ins->size), reg(rb, 8));
            break;
        }

        case IR_MEMCPY: {
            int ra = ei_operand(ins->a0, EI_SCRATCH0);
            int rb = ei_operand(ins->a1, EI_SCRATCH1);
            ei_copy(ins->size, ra, rb);
            break;
        }

        case IR_MOV: {
            int ra = ei_operand(ins->a0, EI_SCRATCH0);
            int rd = ei_dst(ins->dst);
            if (rd != ra) {
                genf(" movq %s, %s", reg(ra, 8), reg(rd, 8));
            }
            ei_writeback(ins->dst, rd);
            break;
        }

        case IR_ZEXT:
        case IR_SEXT:
            ei_extend(ins);
            break;

        case IR_ADD: case IR_SUB: case IR_AND: case IR_OR: case IR_XOR:
            ei_simple_binop(ins);
            break;

        case IR_SHL: case IR_SHR:
            ei_shift(ins);
            break;

        case IR_MUL:
            ei_mul(ins);
            break;

        case IR_DIV: case IR_MOD:
            ei_divmod(ins);
            break;

        case IR_EQ: case IR_NE: case IR_LT: case IR_LE: case IR_GT: case IR_GE:
            ei_compare(ins);
            break;

        case IR_LOGNOT: {
            int ra = ei_operand(ins->a0, EI_SCRATCH0);
            int rd = ei_dst(ins->dst);
            genf(" test%s %s, %s", opsize(ins->size), reg(ra, ins->size), reg(ra, ins->size));
            genf(" setz %s", reg(rd, 1));
            genf(" andq $1, %s", reg(rd, 8));
            ei_writeback(ins->dst, rd);
            break;
        }

        case IR_BITNOT: {
            int ra = ei_operand(ins->a0, EI_SCRATCH0);
            int rd = ei_dst(ins->dst);
            if (rd != ra) {
                genf(" movq %s, %s", reg(ra, 8), reg(rd, 8));
            }
            genf(" not%s %s", opsize(ins->size), reg(rd, ins->size));
            ei_writeback(ins->dst, rd);
            break;
        }

        case IR_CALL:
            ei_call(ins);
            break;

        case IR_JMP:
            if (ins->bb0 != next_block) {
                genf(" jmp .L%d", ei_block_label[ins->bb0]);
            }
            break;

        case IR_BR: {
            int ra = ei_operand(ins->a0, EI_SCRATCH0);
            genf(" testq %s, %s", reg(ra, 8), reg(ra, 8));
            if (ins->bb1 == next_block) {
                genf(" jnz .L%d", ei_block_label[ins->bb0]);
            } else if (ins->bb0 == next_block) {
                genf(" jz .L%d", ei_block_label[ins->bb1]);
            } else {
                genf(" jnz .L%d", ei_block_label[ins->bb0]);
                genf(" jmp .L%d", ei_block_label[ins->bb1]);
            }
            break;
        }

        case IR_RET:
            if (ins->a0 >= 0) {
                int ra = ei_operand(ins->a0, EI_SCRATCH0);
                if (ra != R_AX) {
                    genf(" movq %s, %%rax", reg(ra, 8));
                }
            } else {
                genf(" xorq %%rax, %%rax");
            }
            genf(" jmp .L%d", ei_epilogue_label);
            break;

        default:
            error("cannot emit IR op %d", ins->op);
    }
}

/*
 * Offset of the last argument that arrives in a register; the register save
 * area of a variadic function starts below it (see include/rcc/args.h).
 */
int ei_variadic_arg_offset(func *f) {
    int arg_offset = 0;
    int reg_index = 0;

    for (int i = 0; i < f->argc; i++) {
        var_t *v = var_vec_get(f->argv, i);
        if (v->t->struct_of) {
            int size = type_size(v->t);
            if (size > 16 || reg_index >= ABI_NUM_GP || (size > 8 && reg_index == ABI_NUM_GP - 1)) {
                continue;
            }
            reg_index += (size <= 8) ? 1 : 2;
            arg_offset = align(v->offset, ALIGN_OF_STACK);
            continue;
        }
        if (reg_index < ABI_NUM_GP) {
            arg_offset = align(v->offset, ALIGN_OF_STACK);
            reg_index++;
        }
    }
    return arg_offset;
}

void emit_ir_function(func *f) {
    for (int r = 0; r < R_LAST; r++) {
        ei_callee_slot[r] = 0;
        if (ir_callee_used[r] && ei_is_callee_saved(r)) {
            ei_callee_slot[r] = ir_alloc_slot(8);
        }
    }
    ei_frame_bytes = align(ir_frame_size, 16);
    ei_epilogue_label = new_label();

    ei_block_label = calloc(ir_num_bb() + 1, sizeof(int));
    for (int i = 0; i < ir_block_count; i++) {
        ei_block_label[ir_block_order[i]] = new_label();
    }

    genf(".globl %s", f->name);
    genf(".type %s, @function", f->name);
    gen_label(f->name);
    genf(" pushq %%rbp");
    genf(" movq %%rsp, %%rbp");
    genf(" subq $%d, %%rsp", ei_frame_bytes);

    for (int r = 0; r < R_LAST; r++) {
        if (ei_callee_slot[r]) {
            genf(" movq %s, %d(%%rbp)", reg(r, 8), -ei_callee_slot[r]);
        }
    }

    if (f->is_variadic) {
        int arg_offset = ei_variadic_arg_offset(f);
        for (int i = 0; i < ABI_NUM_GP; i++) {
            genf(" movq %s, %d(%%rbp)", reg(ABI_NUM_GP - i - 1, 8), -(8 + i * 8 + arg_offset + 8 * 16));
        }
    }

    for (int i = 0; i < ir_block_count; i++) {
        int id = ir_block_order[i];
        int next_block = (i + 1 < ir_block_count) ? ir_block_order[i + 1] : -1;
        bb_t *b = ir_get_bb(id);

        genf(".L%d:", ei_block_label[id]);
        for (int j = 0; j < ir_vec_len(b->insns); j++) {
            ei_insn(ir_vec_get(b->insns, j), next_block);
        }
    }

    genf(".L%d:", ei_epilogue_label);
    for (int r = 0; r < R_LAST; r++) {
        if (ei_callee_slot[r]) {
            genf(" movq %d(%%rbp), %s", -ei_callee_slot[r], reg(r, 8));
        }
    }
    genf(" leave");
    genf(" ret");
    genf("");
}

void ssa_compile_function(func *f) {
    if (!f->body_pos) {
        return;
    }
    set_token_pos(program[f->body_pos].token_pos);
    debug("compiling function %s with the SSA backend", f->name);

    irgen_function(f);
    ssa_construct();
    ssa_optimize();
    ssa_destruct();
    regalloc_run();
    emit_ir_function(f);
}

void ssa_compile_file(int fd) {
    output_fd = fd;

    gen(".file \"main.c\"");
    gen("");

    var_vec vars = get_global_frame()->vars;
    for (int i = 0; i < var_vec_len(vars); i++) {
        var_t *v = var_vec_get(vars, i);
        if (v->is_constant) {
            continue;
        }
        if (v->has_value) {
            emit_global_constant(v);
        } else if (!v->is_external) {
            emit_global_declaration(v);
        }
    }

    gen(".text");
    gen(".section .rodata");

    int gstr_i = 0;
    char *gstr;
    while ((gstr = find_global_string(gstr_i)) != 0) {
        emit_global_label(gstr_i);
        emit_string(gstr);
        gstr_i++;
    }
    gen("");

    gen(".text");
    gen("");

    for (int i = 0; i < func_vec_len(functions); i++) {
        ssa_compile_function(func_vec_get(functions, i));
    }
}
