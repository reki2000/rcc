
typedef enum reg { 
    R_0, R_1, R_2, R_3, R_4, R_5, R_6, R_7, R_8, R_9, R_10, R_11, R_12, R_13, R_14, R_15, 
    R_16, R_17, R_18, R_19, R_20, R_21, R_22, R_23, R_24, R_25, R_26, R_27, R_28, R_LAST
} reg_e;

char *regs8[] = {
    "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", 
    "x16", "x17", "x18", "x19", "x20", "x21", "x22", "x23", "x24", "x25", "x26", "x27", "x28"
};
char *regs4[] = {
    "w0", "w1", "w2", "w3", "w4", "w5", "w6", "w7", "w8", "w9", "w10", "w11", "w12", "w13", "w14", "w15", 
    "w16", "w17", "w18", "w19", "w20", "w21", "w22", "w23", "w24", "w25", "w26", "w27", "w28"
};

reg_e reg_tmp() {
    return R_0;
}

char *reg(int no, int size) {
    if (no >= R_LAST) {
        error("invalid reg no:%d", no);
    }
    switch (size) {
    case 8:
        return regs8[no];
    case 4:
        return regs4[no];
    default:
        error("invalid size for reg:%d", size);
        return (void *)0;
    }
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
    for (reg_e i = 0; i < R_LAST; i++) {
        snprintf(b1, 100, "%d[%d] ", i, reg_in_use[i]);
        strcat(b0, b1);
    }
    debug("before pop_all: %s", b0);
}

bool reg_is_callee_saved(reg_e i) {
    return (R_19 <= i && i <= R_28);
}

void init_reg_in_use() {
    for (reg_e i = 0; i < R_LAST; i++) {
        reg_in_use[i] = reg_is_callee_saved(i) ? 1 : 0;
    }
}

void emit_push(reg_e r) {
    genf(" str %s, [sp, -8]!", reg(r, 8));
}

void emit_pop(reg_e r) {
    genf(" ldr %s, [sp], 8", reg(r, 8));
}

void reg_push_all() {
    for (reg_e i = 0; i < R_LAST; i++) {
        if (i == R_0)
            continue;
        if (reg_is_callee_saved(i))
            continue;
        if (reg_in_use[i]) {
            emit_push(i);
        }
    }
}

void reg_pop_all() {
    for (reg_e i = 0; i < R_LAST; i++) {
        if (i == R_0)
            continue;
        reg_e r = R_LAST - i - 1;
        if (reg_is_callee_saved(r))
            continue;
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
    for (reg_e i = 0; i < R_LAST; i++) {
        if (i == R_0 || i == keep)
            continue;
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

void emit_int(long val, int size, reg_e out) {
    genf(" mov %s, %ld", reg(out, size), val);
}

void emit_global_ref(int i, reg_e out) {
    genf(" adrp %s, .G", reg(out, 8), i);
    genf(" add %s, %s, :lo12:.G", reg(out, 8), reg(out, 8), i);
}

void emit_var_val(int i, int size, reg_e out) {
    if (size == 1) {
        genf(" ldrb %s, [x29, %d]", -i, reg(out, 4));
    } else {
        genf(" ldr %s, [x29, %d]", reg(out, size), -i);
    }
}

void emit_global_var_val(char *name, int size, reg_e out) {
    genf(" adrp %s, %s", reg(out, 8), name);
    genf(" add %s, %s, :lo12:%s", reg(out, 8), reg(out, 8), name);
    if (size == 1) {
        genf(" ldrb %s, [%s] ", reg(out, 4), reg(out, 8));
    } else {
        genf(" ldr %s, [%s] ", reg(out, size), reg(out, 8));
    }
}

void emit_var_arg_init(reg_e no, int offset, int size) {
    if (size == 1) {
        genf(" movb %s, %s", reg(no, 1), reg(no, 4));
        size = 4;
    }
    genf(" str %s, [x29, %d]", reg(no, size), -offset);
}

void emit_deref(int size, reg_e inout) {
    if (size == 1) {
        genf(" ldrb %s, [%s]", reg(inout, 4), reg(inout, 8));
    } else {
        genf(" ldr %s, [%s]", reg(inout, size), reg(inout, 8));
    }
}

void emit_var_ref(int i, reg_e ret) {
    genf(" adr %s, [x29, %d]", reg(ret, 8), -i);
}

void emit_global_var_ref(char *name, reg_e out) {
    genf(" adrp %s, %s", reg(out, 8), name);
    genf(" add %s, %s, :lo12:%s", reg(out, 8), reg(out, 8), name);
}

void emit_postfix_add(int size, int ptr_size, reg_e inout) {
    reg_e tmp = R_0;
    reg_e tmp2 = R_1;
    if (size == 1) {
        genf(" ldrb %s, [%s]", reg(tmp, 4), reg(inout, 8));
        genf(" mov %s, %s", reg(tmp2, 4), reg(tmp, 4));
        genf(" add %s, %s, #%d", reg(tmp2, 4), reg(tmp2, 4), ptr_size);
        genf(" strb %s, [%s]", reg(tmp2, 4), reg(inout, 8));
        genf(" mov %s, %s", reg(inout, 4), reg(tmp, 4));
    } else {
        genf(" ldr %s, [%s]", reg(tmp, size), reg(inout, 8));
        genf(" mov %s, %s", reg(tmp2, size), reg(tmp, size));
        genf(" add %s, %s, #%d", reg(tmp2, size), reg(tmp2, size), ptr_size);
        genf(" str %s, [%s]", reg(tmp2, size), reg(inout, 8));
        genf(" mov %s, %s", reg(inout, size), reg(tmp, size));
    }
}

void emit_copy(int size, reg_e in, reg_e out) {
    reg_e tmp = R_0;
    int offset = 0;
    while (size >= 8) {
        genf(" ldr %s, [%s, %d]", reg(tmp, 8), reg(in, 8), offset);
        genf(" str %s, [%s, %d]", reg(tmp, 8), reg(out, 8), offset);
        size -= 8;
        offset += 8;
    }
    while (size >= 4) {
        genf(" ldr %s, [%s, %d]", reg(tmp, 4), reg(in, 8), offset);
        genf(" str %s, [%s, %d]", reg(tmp, 4), reg(out, 8), offset);
        offset += 4;
        size -= 4;
    }
    while (size > 0) {
        genf(" ldrb %s, [%s, %d]", reg(tmp, 4), reg(in, 8), offset);
        genf(" strb %s, [%s, %d]", reg(tmp, 4), reg(out, 8), offset);
        size--;
        offset++;
    }
}

void emit_store(int size, reg_e in, reg_e out) {
    if (size == 1) {
        genf(" strb %s, [%s]", reg(in, 4), reg(out, 8));
    } else {
        genf(" str %s, [%s]", reg(in, size), reg(out, 8));
    }
}

void emit_zcast(int size, reg_e inout) {
    if (size == 1) {
        genf(" movb %s, %s", reg(inout, size), reg(inout, 8));
    }
    else if (size == 4) {
        genf(" mov %s, %s", reg(inout, size), reg(inout, 8));
    }
}

void emit_scast(int size, reg_e inout) {
    if (size == 1) {
        genf(" sxtb %s, %s", reg(inout, size), reg(inout, 8));
    }
    else if (size == 4) {
        genf(" sxtw %s, %s", reg(inout, size), reg(inout, 8));
    }
}

void emit_array_index(int item_size, reg_e in, reg_e out) {
    // out = out + in * item_size
    genf(" mov %s, %d", R_0, item_size);
    genf(" madd %s, %s, %s, %s", reg(out, 8), reg(in, 8), R_0, reg(out, 8));
}

void emit_binop(char *binop, int size, reg_e in, reg_e out) {
    genf(" %s %s,%s %s", binop, reg(out, size), reg(in, size), (size == 1) ? ", sxtb" : (size == 4) ? ", sxtw" : "");
}

void emit_bit_shift(char *op, int size, reg_e in, reg_e out) {
    genf(" %s %s, %s, %s", op, reg(out, size), reg(out, size), reg(in, size));
}

void emit_mul(int size, reg_e in, reg_e inout) {
    genf(" mul %s, %s, %s", reg(inout, size), reg(inout, size), reg(in, size));
}

void emit_div(int size, reg_e in, reg_e out) {
    genf(" sdiv %s, %s, %s", reg(out, size), reg(out, size), reg(in, size));
}

void emit_mod(int size, reg_e in, reg_e out) {
    reg_e tmp = R_0;
    genf(" sdiv %s, %s, %s", reg(tmp, size), reg(out, size), reg(in, size));
    genf(" msub %s, %s, %s", reg(out, size), reg(tmp, size), reg(in, size), reg(out, size));
}

void emit_eq_x(char *set, int size, reg_e in, reg_e out) {
    genf(" cmp %s, %s", reg(in, size), reg(out, size));
    genf(" cset %s, %s", reg(out, 8), set);
}

void emit_log_not(int size, reg_e out) {
    genf(" tst %s, %s", reg(out, size), reg(out, size));
    genf(" cset %s, EQ", reg(out, 8));
}

void emit_neg(int size, reg_e out) {
    genf(" eon %s, xzr", reg(out, size));
}

void emit_jmp(int i) {
    genf(" b .L%d", i);
}

void emit_jmp_false(int i, reg_e in) {
    genf(" tst %s, %s", reg(in, 4), reg(in, 4));
    genf(" b.eq .L%d", i);
}

void emit_jmp_true(int i, reg_e in) {
    genf(" tst %s, %s", reg(in, 4), reg(in, 4));
    genf(" b.ne .L%d", i);
}

void emit_jmp_eq(int i, int size, reg_e in1, reg_e in2) {
    genf(" cmp %s,%s", reg(in1, size), reg(in2, size));
    genf(" b.eq .L%d", i);
}

void emit_jmp_ne(int i, int size, reg_e in1, reg_e in2) {
    genf(" cmp %s,%s", reg(in1, size), reg(in2, size));
    genf(" b.nez .L%d", i);
}

int emit_push_struct(int size, reg_e from) {
    int offset = align(size, 8);
    stack_offset += offset;
    genf(" sub sp, sp, #%d", offset);
    genf(" mov %s, sp", reg(R_0, 8));
    emit_copy(size, from, R_0);
    return offset / 8;
}

void compile(int pos, reg_e reg_out);

void emit_binop_switch(int type, int size, reg_e i1, reg_e reg_out) {
    switch (type) {
        case TYPE_MEMBER_OFFSET:
        case TYPE_ADD:  emit_binop("adds", size, i1, reg_out); break;
        case TYPE_SUB:  emit_binop("subs", size, i1, reg_out); break;
        case TYPE_DIV:  emit_div(size, i1, reg_out); break;
        case TYPE_MOD:  emit_mod(size, i1, reg_out); break;
        case TYPE_MUL:  emit_mul(size, i1, reg_out); break;
        case TYPE_EQ_EQ:  emit_eq_x("eq", size, i1, reg_out); break;
        case TYPE_EQ_NE:  emit_eq_x("ne", size, i1, reg_out); break;
        case TYPE_EQ_LE:  emit_eq_x("le", size, i1, reg_out); break;
        case TYPE_EQ_LT:  emit_eq_x("lt", size, i1, reg_out); break;
        case TYPE_EQ_GE:  emit_eq_x("ge", size, i1, reg_out); break;
        case TYPE_EQ_GT:  emit_eq_x("gt", size, i1, reg_out); break;
        case TYPE_OR:  emit_binop("orr", size, i1, reg_out); break;
        case TYPE_AND:  emit_binop("and", size, i1, reg_out); break;
        case TYPE_XOR:  emit_binop("eor", size, i1, reg_out); break;
        case TYPE_LSHIFT:  emit_bit_shift("lsl", size, i1, reg_out); break;
        case TYPE_RSHIFT:  emit_bit_shift("arr", size, i1, reg_out); break;
        default: error("unknown binop type %d", type);
    }
}

void emit_return(reg_e in) {
    genf(" mov x0, %s", reg(in, 8));
}

void compile_apply(atom_t *p, reg_e reg_out) {
    func *f = (func *)(p->ptr_value);
    int argc = (p + 1)->int_value;

    int num_reg_args = 0;

    bool use_reg[100]; // NUM_ARGC
    int struct_size[100];
    int stack_size = 0;
    for (int i = 0; i < argc; i++) {
        type_t *t = program[(p + i + 2)->atom_pos].t;
        if (t->struct_of) {
            int size = type_size(t);
            struct_size[i] = size;
            use_reg[i] = ((size <= 8 && num_reg_args < ABI_NUM_GP) || (size <= 16 && num_reg_args < ABI_NUM_GP - 1));
            if (use_reg[i]) {
                num_reg_args += (size <= 8) ? 1 : 2;
            } else {
                stack_size += align(size, 8);
            }
        } else {
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
        genf(" sub sp, sp, #8");
        stack_size += 8;
    }
    for (int i = argc - 1; i >= 0; i--) {
        if (use_reg[i])
            continue;
        debug("compiling stack passing values %d, to R#%d", i, reg_out);
        compile((p + i + 2)->atom_pos, reg_out);
        emit_push(reg_out);
        if (struct_size[i] > 0) {
            emit_push_struct(struct_size[i], reg_out);
        }
    }

    // push for register-passing
    for (int i = argc - 1; i >= 0; i--) {
        if (!use_reg[i])
            continue;
        debug("compiling register #%d passing value, to R#%d", i, reg_out);
        compile((p + i + 2)->atom_pos, reg_out); // todo: direct assign to registers
        if (struct_size[i] > 0) {
            emit_push_struct(struct_size[i], reg_out);
        } else {
            emit_push(reg_out);
        }
    }

    for (int i = 0; i < num_reg_args; i++) {
        emit_pop(i);
    }

    //genf(" mov w0,0");
    genf(" bl %s%s", f->name, f->is_external ? "" : "");
    if (stack_size > 0) {
        genf(" add sp, sp, #%d", stack_size);
    }
    reg_pop_all();
    int size = type_size(f->ret_type);
    if (size > 0) {
        genf(" mov %s, %s", reg(reg_out, size), reg(R_0, size));
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
    genf(" stp x29, x30, [sp, %d]!", -16 - align(f->max_offset, 16));
    genf(" mov x29, sp");

    stack_offset = 0; // at this point, %rsp must be 16-bytes aligned
    init_reg_in_use();

    int arg_offset = 0;
    int reg_index = 0;
    for (int i = 0; i < f->argc; i++) {
        var_t *v = var_vec_get(f->argv, i);
        debug("emitting function:%s arg:%s", f->name, v->name);
        if (v->t->struct_of) {
            int size = type_size(v->t);
            if (size > 16 || reg_index >= ABI_NUM_GP || (size > 8 && reg_index == ABI_NUM_GP - 1)) {
                debug("is on the stack. do nothing here", f->name, v->name);
            }
            else if (size <= 8) {
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
            switch (type_size(v->t)) {
            case 1:
            case 4:
            case 8:
                break;
            default:
                error("invalid size for funciton arg:%s", v->name);
            }
        }
        if (reg_index < ABI_NUM_GP) {
            emit_var_arg_init(reg_index, v->offset, type_size(v->t));
            arg_offset = align(v->offset, ALIGN_OF_STACK);
            reg_index++;
        }
    }
    if (f->is_variadic) {
        for (int i = 0; i < ABI_NUM_GP; i++) {
            emit_var_arg_init(ABI_NUM_GP - i - 1, 8 + i * 8 + arg_offset + 8 * 16, 8);
        }
    }

    reg_e ret = reg_assign();
    compile(f->body_pos, ret);

    emit_label(func_void_return_label);
    genf(" mov w0, 0"); // set default return value to $0
    emit_label(func_return_label);
    genf(" ldp x29, x30, [sp], 16");
    genf(" ret");
    genf("");
}

