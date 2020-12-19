
typedef enum reg {
    R_DI, R_SI, R_DX, R_CX, R_8, R_9, R_AX, R_BX, R_10, R_11, R_12, R_13, R_14, R_15, R_LAST
} reg_e;

reg_e reg_tmp() {
    return R_AX;
}

char *regs8[] = { "%rdi", "%rsi", "%rdx", "%rcx", "%r8" , "%r9" , "%rax", "%rbx", "%r10" , "%r11" , "%r12" , "%r13" , "%r14" , "%r15"  };
char *regs4[] = { "%edi", "%esi", "%edx", "%ecx", "%r8d", "%r9d", "%eax", "%ebx", "%r10d", "%r11d", "%r12d", "%r13d", "%r14d", "%r15d" };
char *regs1[] = { "%dil", "%sil", "%dl" , "%cl" , "%r8b", "%r9b", "%al" , "%bl",  "%r10b", "%r11b", "%r12b", "%r13b", "%r14b", "%r15b" };

char *reg(int no, int size) {
    if (no >= R_LAST) {
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
    genf(" leaq %d(%%rbp), %s", -i, reg(ret,8));
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

void emit_binop_switch(int type, int size, reg_e i1, reg_e reg_out) {
    switch (type) {
        case TYPE_MEMBER_OFFSET:
        case TYPE_ADD: emit_binop("add", size, i1, reg_out); break;
        case TYPE_SUB: emit_binop("sub", size, i1, reg_out); break;
        case TYPE_DIV: emit_div(size, i1, reg_out); break;
        case TYPE_MOD: emit_mod(size, i1, reg_out); break;
        case TYPE_MUL: emit_mul(size, i1, reg_out); break;
        case TYPE_EQ_EQ: emit_eq_x("e", size, i1, reg_out); break;
        case TYPE_EQ_NE: emit_eq_x("ne", size, i1, reg_out); break;
        case TYPE_EQ_LE: emit_eq_x("le", size, i1, reg_out); break;
        case TYPE_EQ_LT: emit_eq_x("nge", size, i1, reg_out); break;
        case TYPE_EQ_GE: emit_eq_x("ge", size, i1, reg_out); break;
        case TYPE_EQ_GT: emit_eq_x("nle", size, i1, reg_out); break;
        case TYPE_OR: emit_binop("or", size, i1, reg_out); break;
        case TYPE_AND: emit_binop("and", size, i1, reg_out); break;
        case TYPE_XOR: emit_binop("xor", size, i1, reg_out); break;
        case TYPE_LSHIFT: emit_bit_shift("l", size, i1, reg_out); break;
        case TYPE_RSHIFT: emit_bit_shift("r", size, i1, reg_out); break;
        default: error("unknown binop type %d", type);
    }
}

void emit_return(reg_e reg_in) {
    genf(" movq %s, %%rax", reg(reg_in, 8));
}

void compile(int pos, reg_e reg_out);

void compile_apply(atom_t *p, reg_e reg_out) {
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

void emit_function(func *f) {
    if (!f->body_pos) {
        return;
    }
    set_token_pos(program[f->body_pos].token_pos);

    func_return_label = new_label();
    func_void_return_label = new_label();

    genf(".globl %s", f->name);
    genf(".type %s, @function", f->name);
    gen_label(f->name);
    genf(" pushq %%rbp");
    genf(" movq %%rsp, %%rbp");

    genf(" subq $%d, %%rsp", align(f->max_offset, 16));
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
