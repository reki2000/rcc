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

#include "parse.h"

int output_fd = 1;

void _write(char *s) {
    write(output_fd, s, strlen(s));
}

void genf(char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    char buf[RCC_BUF_SIZE];
    vsnprintf(buf, RCC_BUF_SIZE, fmt, va);
    va_end(va);
    _write(buf);
    _write("\n");
}

void gen_label(char *str) {
    genf("%s:", str);
}

void gen(char *str) {
    genf("\t%s", str);
}


int label_index = 0;
int new_label() {
    return label_index++;
}

typedef enum reg {
    R_DI, R_SI, R_DX, R_CX, R_8, R_9, R_AX, R_BX, R_10, R_11, R_12, R_13, R_14, R_15, R_LAST
} reg_e;

char *reg(int no, int size) {
    // todo: make name table to be global variables, but for now it's not supported by rcc. (initializing global variables with a string array)
    char *regs8[] = { "%rdi", "%rsi", "%rdx", "%rcx", "%r8" , "%r9" , "%rax", "%rbx", "%r10" , "%r11" , "%r12" , "%r13" , "%r14" , "%r15"  };
    char *regs4[] = { "%edi", "%esi", "%edx", "%ecx", "%r8d", "%r9d", "%eax", "%ebx", "%r10d", "%r11d", "%r12d", "%r13d", "%r14d", "%r15d" };
    char *regs1[] = { "%dil", "%sil", "%dl" , "%cl" , "%r8b", "%r9b", "%al" , "%bl",  "%r10b", "%r11b", "%r12b", "%r13b", "%r14b", "%r15b" };
    if (no >= 16) {
        error("invalid reg no:%d", no);
    }
    switch (size) {
        case 8: return regs8[no];
        case 4: return regs4[no];
        case 1: return regs1[no];
        default: error("invalid size for reg:%d", size); return (void *)0;
    }
}

char *opsize(int size) {
    return (size == 8) ? "q" : (size == 4) ? "l" : (size == 1) ? "b" : "?";
}

int stack_offset = 0;

/*
 * reg_in_use stores a status for the register.
 * 0: not in use (freely assignable)
 * 1: used
 * 2+: used and had 1+ pushed on the stack 
 */
int reg_in_use[R_LAST];

void dump_reg_is_use() {
    char b1[100] = {0}; 
    char b0[100] = {0}; 
    for(reg_e i=0; i<R_LAST; i++) { 
        snprintf(b1, 100, "%d[%d] ", i, reg_in_use[i]); 
        strcat(b0, b1); 
    } 
    debug("before pop_all: %s", b0);
}

bool reg_is_callee_saved(reg_e i) {
    return (i==R_BX || i==R_12 || i==R_13 || i==R_14 || i==R_15);
}

void init_reg_in_use() {
    for (reg_e i=0; i<R_LAST; i++) {
        reg_in_use[i] = reg_is_callee_saved(i) ? 1 : 0;
    }
}

void emit_push(reg_e r) {
    genf(" pushq %s", reg(r,8));
    stack_offset -= 8;
}

void emit_pop(reg_e r) {
    genf(" popq %s", reg(r,8));
    stack_offset += 8;
}

void reg_push_all() {
    for (reg_e i=0; i<R_LAST; i++) {
        if (i == R_AX) continue;
        if (reg_is_callee_saved(i)) continue;
        if (reg_in_use[i]) {
            emit_push(i);
        }
    }
}

void reg_pop_all() {
    for (reg_e i=0; i<R_LAST; i++) {
        if (i == R_AX) continue;
        reg_e r = R_LAST-i-1;
        if (reg_is_callee_saved(r)) continue;
        if (reg_in_use[r]) {
            emit_pop(r);
        }
    }
}

/*
 * assign a register which is the least pushed on the stack, except it's the specified one.
 */
reg_e reg_assign_keep(reg_e keep) {
    reg_e reg_min = R_LAST;
    int val_min = INT32_MAX;
    for (reg_e i=0; i<R_LAST; i++) {
        if (i == R_AX || i == keep) continue;
        if (val_min > reg_in_use[i]) {
            reg_min = i;
            val_min = reg_in_use[i];
        }
    }
    if (reg_in_use[reg_min] > 0) {
        emit_push(reg_min);
    }
    reg_in_use[reg_min]++;
    return reg_min;
}

reg_e reg_assign() {
    return reg_assign_keep(R_LAST);
}

void reg_release(reg_e r) {
    if (!reg_in_use[r]) {
        error("invalid release for reg %d", r);
    }
    if (reg_in_use[r] > 1) {
        emit_pop(r);
    }
    reg_in_use[r]--;
}

reg_e reg_reserve(reg_e org, reg_e keep) {
    emit_push(keep);
    if (org == keep) {
        reg_e tmp = reg_assign_keep(keep);
        genf(" movq %s, %s", reg(org,8), reg(tmp,8));
        return tmp;
    }
    return org;
}

void reg_restore(reg_e tmp, reg_e org, reg_e keep) {
    if (tmp != org) {
        reg_release(tmp);
    }
    emit_pop(keep);
}


void emit_int(long val, int size, reg_e out) {
    genf(" mov%s $%ld, %s", opsize(size), val, reg(out, size));
}

void emit_string(char* str) {
    char buf[RCC_BUF_SIZE] = {0};

    strcat(buf, ".string \"");
    escape_string(buf + strlen(buf), str);
    strcat(buf, "\"");
    gen(buf);
}

void emit_global_ref(int i, reg_e out) {
    genf(" leaq .G%d(%%rip), %s", i, reg(out, 8));
}

void emit_var_val(int i, int size, reg_e out) {
    if (size == 1) {
        genf(" movzbl %d(%%rbp), %s", -i, reg(out, 4));
    } else {
        genf(" mov%s %d(%%rbp), %s", opsize(size), -i, reg(out, size));
    }
}

void emit_global_var_val(char *name, int size, reg_e out) {
    if (size == 1) {
        genf(" movzbl %s(%%rip), %s", name, reg(out, 4));
    } else {
        genf(" mov%s %s($%rip), %s", opsize(size), name, reg(out, size));
    }
}

void emit_var_arg_init(reg_e no, int offset, int size) {
    if (size == 1) {
        genf(" movzbl %s, %s", reg(no,1), reg(no,4));
        size = 4;
    }
    genf(" mov%s %s, %d(%%rbp)", opsize(size), reg(no, size), -offset);
}

void emit_deref(int size, reg_e inout) {
    if (size == 1) {
        genf(" movzbl (%s), %s", reg(inout,8), reg(inout,4));
    } else {
        genf(" mov%s (%s), %s", opsize(size), reg(inout,8), reg(inout,size));
    }
}

void emit_var_ref(int i, reg_e ret) {
    genf(" leaq	%d(%%rbp), %s", -i, reg(ret,8));
}

void emit_global_var_ref(char *name, reg_e out) {
    genf(" leaq %s(%%rip), %s", name, reg(out, 8));
}

void emit_postfix_add(int size, int ptr_size, reg_e inout) {
    reg_e tmp = R_AX;
    genf(" mov%s (%s), %s", opsize(size), reg(inout, 8), reg(tmp, size));
    genf(" add%s $%d, (%s)", opsize(size), ptr_size, reg(inout,8));
    genf(" mov%s %s, %s", opsize(size), reg(tmp,size), reg(inout, size));
}

void emit_copy(int size, reg_e in, reg_e out) {
    reg_e tmp = R_AX;
    int offset = 0;
    while (size>=8) {
        genf(" movq %d(%s), %s", offset, reg(in,8), reg(tmp,8));
        genf(" movq %s, %d(%s)", reg(tmp,8), offset, reg(out,8));
        size -= 8;
        offset += 8;
    }
    while (size>=4) {
        genf(" movl %d(%s), %s", offset, reg(in,8), reg(tmp,4));
        genf(" movl %s, %d(%s)", reg(tmp,4), offset, reg(out,8));
        offset += 4;
        size -= 4;
    }
    while (size>0) {
        genf(" movb %d(%s), %s", offset, reg(in,8), reg(tmp,1));
        genf(" movb %s, %d(%s)", reg(tmp,1), offset, reg(out,8));
        size--;
        offset++;
    }
}

void emit_store(int size, reg_e in, reg_e out) {
    genf(" mov%s %s, (%s)", opsize(size), reg(in,size), reg(out, 8));
}

void emit_zcast(int size, reg_e inout) {
    if (size == 8) {
        return;
    }
    genf(" movz%sq %s, %s", opsize(size), reg(inout,size), reg(inout,8));
}

void emit_scast(int size, reg_e inout) {
    if (size == 8) {
        return;
    }
    genf(" movs%sq %s, %s", opsize(size), reg(inout,size), reg(inout,8));
}

void emit_array_index(int item_size, reg_e in, reg_e out) {
    // out = out + in * item_size
    reg_e in2 = reg_reserve(in, R_DX);
    genf(" movl $%d,%%eax", item_size);
    genf(" imulq %s", reg(in2,8));
    genf(" addq %%rax, %s", reg(out,8));
    reg_restore(in2, in, R_DX);
}

void emit_binop(char *binop, int size, reg_e in, reg_e out) {
    genf(" %s%s %s,%s", binop, opsize(size), reg(in,size), reg(out,size));
}

void emit_bit_shift(char* op, int size, reg_e in, reg_e out) {
    genf(" movq %%rcx, %%rax");
    genf(" movb %s,%%cl", reg(in,1));
    genf(" sa%s%s %%cl, %s", op, opsize(size), reg(out,size));
    genf(" movq %%rax, %%rcx");
}

void emit_mul(int size, reg_e in, reg_e inout) {
    reg_e in2 = reg_reserve(in, R_DX);
    genf(" mov%s %s,%s", opsize(size), reg(inout,size), reg(R_AX,size));
    genf(" imul%s %s", opsize(size), reg(in2,size));
    genf(" mov%s %s,%s", opsize(size), reg(R_AX,size), reg(inout,size));
    reg_restore(in2, in, R_DX);
}

void emit_divmod(int size, reg_e in, reg_e out, reg_e ret_reg) { 
    // out = out [ / | % ] in
    reg_e in2 = reg_reserve(in, R_DX);
    genf(" mov%s %s,%s", opsize(size), reg(out,size), reg(R_AX,size));
    genf(" %s", (size == 8) ? "cqo" : (size == 4) ? "cdq" : "???");
    genf(" idiv%s %s", opsize(size), reg(in2,size));
    genf(" mov%s %s,%s", opsize(size), reg(ret_reg,size), reg(out,size));
    reg_restore(in2, in, R_DX);
}

void emit_div(int size, reg_e in, reg_e out) {
    emit_divmod(size, in, out, R_AX);
}

void emit_mod(int size, reg_e in, reg_e out) {
    emit_divmod(size, in, out, R_DX);
}

void emit_eq_x(char *set, int size, reg_e in, reg_e out) {
    genf(" cmp%s %s, %s", opsize(size), reg(in,size), reg(out,size));
    genf(" set%s %s", set, reg(out,1));
    genf(" andq $1,%s", reg(out, 8));
}

void emit_log_not(int size, reg_e out) {
    genf(" or%s %s, %s", opsize(size), reg(out,size), reg(out,size));
    genf(" setz %s", reg(out,1));
    genf(" andq $1,%s", reg(out, 8));
}

void emit_neg(int size, reg_e out) {
    genf(" not%s %s", opsize(size), reg(out,size));
}

void emit_print(reg_e in) {
	genf(" movl	%s, %%esi", reg(in,4));
	genf(" leaq	.LC0(%%rip), %%rdi");
	genf(" movl	$0, %%eax");
	genf(" call	printf@PLT");
}

void emit_label(int i) {
    genf(".L%d:", i);
}

void emit_global_label(int i) {
    genf(".G%d:", i);
}

void emit_jmp(int i) {
    genf(" jmp .L%d", i);
}

void emit_jmp_false(int i, reg_e in) {
    genf(" orl %s, %s", reg(in,4), reg(in,4));
    genf(" jz .L%d", i);
}

void emit_jmp_true(int i, reg_e in) {
    genf(" orl %s, %s", reg(in,4), reg(in,4));
    genf(" jnz .L%d", i);
}

void emit_jmp_eq(int i, int size, reg_e in1, reg_e in2) {
    genf(" cmp%s %s,%s", opsize(size), reg(in1,size), reg(in2,size));
    genf(" jz .L%d", i);
}

void emit_jmp_ne(int i, int size, reg_e in1, reg_e in2) {
    genf(" cmp%s %s,%s", opsize(size), reg(in1,size), reg(in2,size));
    genf(" jnz .L%d", i);
}

int emit_push_struct(int size, reg_e from) {
    reg_e to = reg_assign();
    int offset = align(size, 8);
    stack_offset += offset;
    genf(" subq $%d, %%rsp", offset);
    genf(" movq %%rsp, %s", reg(to, 8));
    emit_copy(size, from, to);
    reg_release(to);
    return offset / 8;
}


int func_return_label;
int func_void_return_label;

typedef struct {
    int break_label;
    int continue_label;
} break_label_t;

VEC_HEADER(break_label_t, break_label_vec)
VEC_BODY(break_label_t, break_label_vec)

break_label_vec break_labels = 0;

int get_break_label() {
    if (!break_label_vec_len(break_labels)) {
        error("cannot emit break");
    }
    return break_label_vec_top(break_labels)->break_label;
}
int get_continue_label() {
    if (!break_label_vec_len(break_labels)) {
        error("cannot emit break");
    }
    return break_label_vec_top(break_labels)->continue_label;
}
void enter_break_label(int break_label, int continue_label) {
    if (!break_labels) break_labels = break_label_vec_new();
    break_label_t b;
    b.break_label = break_label;
    b.continue_label = continue_label;
    break_label_vec_push(break_labels, b);
}
void exit_break_label() {
    if (!break_label_vec_len(break_labels)) {
        error("cannot exit break label");
    }
    break_label_vec_pop(break_labels);
}


void compile(int pos, reg_e reg_out) {
    atom_t *p = &(program[pos]);

    char ast_text[RCC_BUF_SIZE] = {0};
    dump_atom3(ast_text, p, 0, pos);
    debug("compiling out:R#%d atom_t: %s", reg_out, ast_text);
    set_token_pos(p->token_pos);
    //dump_token_by_id(p->token_pos);

    switch (p->type) {
        case TYPE_VAR_REF:
            emit_var_ref(p->int_value, reg_out);
            break;

        case TYPE_GLOBAL_VAR_REF:
            emit_global_var_ref(p->ptr_value, reg_out);
            break;

        case TYPE_BIND: {
            reg_e i1 = reg_assign();
            compile(p->atom_pos, reg_out); // rvalue
            compile((p+1)->atom_pos, i1); // lvalue - should be an address
            if (p->t->struct_of) {
                emit_copy(type_size(p->t), reg_out, i1);
            } else {
                emit_store(type_size(p->t), reg_out, i1);
            }
            reg_release(i1);
            break;
        }
        case TYPE_PTR:
        case TYPE_PTR_DEREF:
            compile(p->atom_pos, reg_out);
            break;
        case TYPE_RVALUE:
            compile(p->atom_pos, reg_out);
            if (p->t->array_length >= 0 || p->t->struct_of) {
                // rvalue of array / struct will be a pointer for itself
            } else {
                emit_deref(type_size(p->t), reg_out);
            }
            break;

        case TYPE_CONVERT:
            compile(p->atom_pos, reg_out);
            break;

        case TYPE_CAST:
            compile(p->atom_pos, reg_out);
            emit_scast(type_size(p->t), reg_out);
            break;

        case TYPE_INTEGER: 
            if (type_size(p->t) == 8) {
                emit_int(p->long_value, 8, reg_out);
            } else {
                emit_int((long)(p->int_value), type_size(p->t), reg_out);
            }
            break;

        case TYPE_ADD:
        case TYPE_SUB:
        case TYPE_DIV:
        case TYPE_MOD:
        case TYPE_MUL:
        case TYPE_EQ_EQ:
        case TYPE_EQ_NE:
        case TYPE_EQ_LT:
        case TYPE_EQ_LE:
        case TYPE_EQ_GT:
        case TYPE_EQ_GE:
        case TYPE_OR:
        case TYPE_AND:
        case TYPE_XOR:
        case TYPE_LSHIFT:
        case TYPE_RSHIFT:
        case TYPE_MEMBER_OFFSET:
        case TYPE_ARRAY_INDEX:
            compile(p->atom_pos, reg_out);
            reg_e i1 = reg_assign();
            compile((p+1)->atom_pos, i1);
            switch (p->type) {
                case TYPE_MEMBER_OFFSET:
                case TYPE_ADD: emit_binop("add", type_size(p->t), i1, reg_out); break;
                case TYPE_SUB: emit_binop("sub", type_size(p->t), i1, reg_out); break;
                case TYPE_DIV: emit_div(type_size(p->t), i1, reg_out); break;
                case TYPE_MOD: emit_mod(type_size(p->t), i1, reg_out); break;
                case TYPE_MUL: emit_mul(type_size(p->t), i1, reg_out); break;
                case TYPE_EQ_EQ: emit_eq_x("e", type_size(p->t), i1, reg_out); break;
                case TYPE_EQ_NE: emit_eq_x("ne", type_size(p->t), i1, reg_out); break;
                case TYPE_EQ_LE: emit_eq_x("le", type_size(p->t), i1, reg_out); break;
                case TYPE_EQ_LT: emit_eq_x("nge", type_size(p->t), i1, reg_out); break;
                case TYPE_EQ_GE: emit_eq_x("ge", type_size(p->t), i1, reg_out); break;
                case TYPE_EQ_GT: emit_eq_x("nle", type_size(p->t), i1, reg_out); break;
                case TYPE_OR: emit_binop("or", type_size(p->t), i1, reg_out); break;
                case TYPE_AND: emit_binop("and", type_size(p->t), i1, reg_out); break;
                case TYPE_XOR: emit_binop("xor", type_size(p->t), i1, reg_out); break;
                case TYPE_LSHIFT: emit_bit_shift("l", type_size(p->t), i1, reg_out); break;
                case TYPE_RSHIFT: emit_bit_shift("r", type_size(p->t), i1, reg_out); break;
                case TYPE_ARRAY_INDEX: emit_array_index((p+2)->int_value, i1, reg_out); break;
            }
            reg_release(i1);
            break;

        case TYPE_POSTFIX_DEC: {
            compile(p->atom_pos, reg_out);
            type_t *target_t = p->t;
            emit_postfix_add(type_size(target_t), (target_t->ptr_to) ? -type_size(target_t->ptr_to) : -1, reg_out);
            break;
        }
        case TYPE_POSTFIX_INC:  {
            compile(p->atom_pos, reg_out);
            type_t *target_t = p->t;
            emit_postfix_add(type_size(target_t), (target_t->ptr_to) ? type_size(target_t->ptr_to) : 1, reg_out);
            break;
        }
        
        case TYPE_NOP:
            break;

        case TYPE_EXPR_STATEMENT:
            compile(p->atom_pos, reg_out);
            break;

        case TYPE_ANDTHEN:
            compile(p->atom_pos, reg_out);
            compile((p+1)->atom_pos, reg_out);
            break;

        case TYPE_LOG_AND: {
                int l_end = new_label();
                compile(p->atom_pos, reg_out);
                emit_jmp_false(l_end, reg_out); // short circuit of '&&'
                compile((p+1)->atom_pos, reg_out);
                emit_label(l_end);
            }
            break;

        case TYPE_LOG_OR: {
                int l_end = new_label();
                compile(p->atom_pos, reg_out);
                emit_jmp_true(l_end, reg_out);   // short circuit of '||'
                compile((p+1)->atom_pos, reg_out);
                emit_label(l_end);
            }
            break;

        case TYPE_PRINT:
            compile(p->atom_pos, reg_out);
            emit_print(reg_out);
            break;
            
        case TYPE_LOG_NOT:
            compile(p->atom_pos, reg_out);
            emit_log_not(type_size(p->t), reg_out);
            break;

        case TYPE_NEG:
            compile(p->atom_pos, reg_out);
            emit_neg(type_size(p->t), reg_out);
            break;

        case TYPE_TERNARY: {
            int l_end = new_label();
            int l_else = new_label();
            compile(p->atom_pos, reg_out);
            emit_jmp_false(l_else, reg_out);
            compile((p+1)->atom_pos, reg_out);
            emit_jmp(l_end);
            emit_label(l_else);
            compile((p+2)->atom_pos, reg_out);
            emit_label(l_end);
        } 
            break;

        case TYPE_IF: {
            bool has_else = ((p+2)->atom_pos != 0);
            int l_end = new_label();
            int l_else = new_label();

            compile(p->atom_pos, reg_out);
            emit_jmp_false(has_else ? l_else : l_end, reg_out);
            compile((p+1)->atom_pos, reg_out);

            if (has_else) {
                emit_jmp(l_end);
                emit_label(l_else);
                compile((p+2)->atom_pos, reg_out);
            }
            emit_label(l_end);
        }
            break;

        case TYPE_FOR: {
            int l_body = new_label();
            int l_loop = new_label();
            int l_end = new_label();
            enter_break_label(l_end, l_loop);

            compile((p+2)->atom_pos, reg_out);
            emit_label(l_body);
            compile((p+1)->atom_pos, reg_out);
            emit_jmp_false(l_end, reg_out);

            compile(p->atom_pos, reg_out);

            emit_label(l_loop);
            compile((p+3)->atom_pos, reg_out);
            emit_jmp(l_body);
            emit_label(l_end);

            exit_break_label();
        }
            break;
        case TYPE_WHILE:
        {
            int l_body = new_label();
            int l_end = new_label();
            enter_break_label(l_end, l_body);

            emit_label(l_body);
            compile((p+1)->atom_pos, reg_out);
            emit_jmp_false(l_end, reg_out);
            compile(p->atom_pos, reg_out);
            emit_jmp(l_body);
            emit_label(l_end);

            exit_break_label();
        }
            break;
        case TYPE_DO_WHILE:
        {
            int l_body = new_label();
            int l_end = new_label();
            enter_break_label(l_end, l_body);

            emit_label(l_body);
            compile(p->atom_pos, reg_out);
            compile((p+1)->atom_pos, reg_out);
            emit_jmp_true(l_body, reg_out);
            emit_label(l_end);

            exit_break_label();
        }
            break;

        case TYPE_RETURN:
            if (p->t != type_void) {
                compile(p->atom_pos, reg_out);
                genf(" movq %s, %%rax", reg(reg_out, 8));
                emit_jmp(func_return_label);
            } else {
                emit_jmp(func_void_return_label);
            }
            break;
        
        case TYPE_BREAK:
            emit_jmp(get_break_label());
            break;

        case TYPE_CONTINUE:
            emit_jmp(get_continue_label());
            break;

        case TYPE_APPLY: {
            dump_atom_tree(pos,0);
            func *f = (func *)(p->ptr_value);
            int argc = (p+1)->int_value;

            int num_reg_args = 0;

            bool use_reg[100]; // NUM_ARGC
            int struct_size[100]; 
            int stack_size = 0;
            for (int i=0; i<argc; i++) {
                type_t *t = program[(p+i+2)->atom_pos].t;
                if (t->struct_of) {
                    int size = type_size(t);
                    struct_size[i] = size;
                    use_reg[i] = ((size <= 8 && num_reg_args < ABI_NUM_GP) || (size <= 16 && num_reg_args < ABI_NUM_GP - 1));
                    if (use_reg[i]) {
                        num_reg_args += (size <= 8) ? 1 : 2;
                    } else {
                        stack_size += align(size, 8);
                    }
                } else  {
                    struct_size[i] = 0;
                    use_reg[i] = (num_reg_args < ABI_NUM_GP);
                    if (use_reg[i]) {
                        num_reg_args++;
                    } else {
                        stack_size += 8;
                    }
                }
            }

            // push for stack-passing
            reg_push_all();
            if ((stack_size + stack_offset) % 16 != 0) {
                genf(" subq $8, %%rsp");
                stack_size += 8;
            }
            for (int i=argc-1; i>=0; i--) {
                if (use_reg[i]) continue;
                debug("compiling stack passing values %d, to R#%d", i, reg_out);
                compile((p+i+2)->atom_pos, reg_out);
                emit_push(reg_out);
                if (struct_size[i] > 0) {
                    emit_push_struct(struct_size[i], reg_out);
                }
            }

            // push for register-passing
            for (int i=argc-1; i>=0; i--) {
                if (!use_reg[i]) continue;
                debug("compiling register #%d passing value, to R#%d", i, reg_out);
                compile((p+i+2)->atom_pos, reg_out); // todo: direct assign to registers
                if (struct_size[i] > 0) {
                    emit_push_struct(struct_size[i], reg_out);
                } else {
                    emit_push(reg_out);
                }
            }

            for (int i=0; i<num_reg_args; i++) {
                emit_pop(i);
            }

            genf(" movb $0, %%al");
            genf(" call %s%s", f->name, f->is_external ? "@PLT" : "");
            if (stack_size > 0) {
                genf(" addq $%d, %%rsp", stack_size);
            }
            reg_pop_all();
            int size = type_size(f->ret_type);
            if (size > 0) {
                genf(" mov%s %s, %s", opsize(size), reg(R_AX, size), reg(reg_out, size));
            }
        }
            break;
        
        case TYPE_STRING:
            emit_global_ref(p->int_value, reg_out);
            break;

        case TYPE_SWITCH: {
            int l_end = new_label();
            enter_break_label(l_end, 0);
            compile(p->atom_pos, reg_out);
            int size = type_size(p->t);

            p++;
            int l_fallthrough = new_label();
            while (p->type == TYPE_ARG) {
                int l_next_case = new_label(); 
                atom_t *case_atom = &program[p->atom_pos];
                int pos;

                if (case_atom->type == TYPE_CASE) {
                    reg_e tmp = reg_assign();
                    compile((case_atom  )->atom_pos, tmp);
                    genf(" movq %s, %%rax", reg(tmp,8));
                    reg_release(tmp);
                    emit_jmp_ne(l_next_case, size, reg_out, R_AX); 
                    pos = (case_atom+1)->atom_pos;
                } else if (case_atom->type == TYPE_DEFAULT) {
                    pos = case_atom->atom_pos;
                } else {
                    dump_atom_tree(p->atom_pos, 0);
                    error("invalid child under switch node");
                }

                emit_label(l_fallthrough);
                compile(pos, reg_out);
                l_fallthrough = new_label();    // points the body of the next case
                emit_jmp(l_fallthrough);

                emit_label(l_next_case);
                p++;
            }

            emit_label(l_fallthrough);
            exit_break_label();
            emit_label(l_end);
        }
            break;


        default:
            dump_atom(pos, 0);
            error("Invalid program");
    }
    if (p->type == TYPE_EXPR_STATEMENT || p->type == TYPE_APPLY || p->type == TYPE_RETURN || p->type == TYPE_IF || p->type == TYPE_FOR || p->type == TYPE_WHILE || p->type == TYPE_DO_WHILE) {
        genf("# %s", ast_text);
    }
    debug("compiled out:R#%d atom_t: %s", reg_out, ast_text);
}

void emit_function(func *f) {
    if (!f->body_pos) {
        return;
    }
    set_token_pos(program[f->body_pos].token_pos);

    func_return_label = new_label();
    func_void_return_label = new_label();

    genf(".globl %s", f->name);
    genf(".type	%s, @function", f->name);
    gen_label(f->name);
    genf(" pushq %%rbp");
    genf(" movq %%rsp, %%rbp");

    genf(" subq	$%d, %%rsp", align(f->max_offset, 16));
    stack_offset = 0; // at this point, %rsp must be 16-bytes aligned
    init_reg_in_use();

    int arg_offset = 0;
    int reg_index = 0;
    for (int i=0; i<f->argc; i++) {
        var_t *v = var_vec_get(f->argv, i);
        debug("emitting function:%s arg:%s", f->name, v->name);
        if (v->t->struct_of) {
            int size = type_size(v->t);
            if (size > 16 || reg_index >= ABI_NUM_GP || (size > 8 && reg_index == ABI_NUM_GP - 1)) {
                debug("is on the stack. do nothing here", f->name, v->name);
            } else if (size <= 8) {
                debug("is passed by one register. do nothing here", f->name, v->name);
                emit_var_arg_init(reg_index++, v->offset, type_size(v->t));
                arg_offset = align(v->offset, ALIGN_OF_STACK);
            } else {
                debug("is passed by two registers. ", f->name, v->name);
                emit_var_arg_init(reg_index++, v->offset, 8);
                emit_var_arg_init(reg_index++, v->offset - 8, size - 8);
                arg_offset = align(v->offset, ALIGN_OF_STACK);
            }
            continue;
        } else {
            switch (type_size(v->t))  { 
                case 1: case 4: case 8: break;
                default: error("invalid size for funciton arg:%s", v->name); 
            }
        }
        if (reg_index < ABI_NUM_GP) {
            emit_var_arg_init(reg_index, v->offset, type_size(v->t));
            arg_offset = align(v->offset, ALIGN_OF_STACK);
            reg_index++;
        }
    }
    if (f->is_variadic) {
        for (int i=0; i<ABI_NUM_GP; i++) {
            emit_var_arg_init(ABI_NUM_GP-i-1, 8 + i*8 + arg_offset + 8*16, 8);
        }
    }

    reg_e ret = reg_assign();
    compile(f->body_pos, ret);

    emit_label(func_void_return_label);
    genf(" xorq %%rax, %%rax"); // set default return value to $0
    emit_label(func_return_label);
    genf(" leave");
    genf(" ret");
    genf("");
}

int emit_global_constant_by_type(type_t *pt, int value) {
    int filled_size = 0;
    if (pt == type_char) {
        genf(".byte %d", value);
        filled_size += 1;
    } else if (pt == type_int) {
        genf(".long %d", value);
        filled_size += 4;
    } else if (pt == type_long) {
        genf(".quad %d", value);
        filled_size += 4;
    } else if (pt == type_char_ptr) {
        genf(".quad .G%d", value);
        filled_size += 8;
    } else if (pt->ptr_to) {
        genf(".quad %d", value);
        filled_size += 8;
    }
    return filled_size;
}

void emit_global_constant(var_t *v) {
    genf(".globl %s", v->name);
    genf(".data");
    genf(".align 4");
    genf(".type %s, @object", v->name);
    genf(".size %s, %d", v->name, type_size(v->t));
    gen_label(v->name);
    if (v->t->array_length >= 0) {
        int filled_size = 0;
        int pos = v->int_value;
        int len = get_global_array_length(pos);
        type_t *pt = v->t->ptr_to;
        for (int index = 0; index < len; index++) {
            int value = get_global_array(pos, index);
            filled_size += emit_global_constant_by_type(pt, value);
        }
        if (type_size(v->t) > filled_size) {
            genf(".zero %d", type_size(v->t) - filled_size);
        }
    } else {
        int filled_size = emit_global_constant_by_type(v->t, v->int_value);
        if (!filled_size) {
            char buf[RCC_BUF_SIZE] = {0};
            dump_type(buf, v->t);
            error("unknown size for global variable:%s %s", v->name, buf);
        }
    }
    gen("");
}

void emit_global_declaration(var_t *v) {
    genf(".comm %s, %d", v->name, type_size(v->t));
}

void compile_file(int fd) {
    output_fd = fd;
    
    gen(".file	\"main.c\"");
    gen("");

    var_vec vars = get_global_frame()->vars;
    for (int i=0; i<var_vec_len(vars); i++) {
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
    gen_label(".LC0");
    gen(".string \"%d\\n\"");

    int gstr_i=0;
    char *gstr;
    while ((gstr = find_global_string(gstr_i)) != 0) {
        emit_global_label(gstr_i);
        emit_string(gstr);
        gstr_i++;
    }
    gen("");

    gen(".text");
    gen("");

    for (int i=0; i<func_vec_len(functions); i++) {
        func *f = func_vec_get(functions, i);
        if (f->body_pos != 0) {
            debug("%s --------------------- ", f->name);
            //dump_atom_tree(f->body_pos, 0);
            emit_function(f);
        }
    }
}
