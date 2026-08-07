/*
 * irgen.c - lowers the AST (atom_t tree) into the linear IR of ir.h
 *
 * The result is *not* in SSA form yet: every local variable is still a
 * memory location addressed by IR_LOCAL / IR_LOAD / IR_STORE. ssa.c turns
 * those memory operations into SSA values afterwards.
 *
 * Values that merge at a join point (?:, &&, ||) are emitted as IR_PHI
 * directly, because the shape of the control flow is known here.
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
#include "ir.h"

int irgen_cur_bb = 0;
int_vec irgen_break_bb;
int_vec irgen_continue_bb;

int irgen(int pos);

/*
 * Append an instruction to the current block. Code that follows an
 * unconditional terminator is unreachable; it is collected into a fresh
 * block so that the 'one terminator per block' invariant always holds.
 */
ir_t *irgen_emit(int op) {
    if (ir_bb_is_closed(irgen_cur_bb)) {
        irgen_cur_bb = ir_new_bb();
    }
    return ir_add(irgen_cur_bb, op);
}

int irgen_emit_v(int op, int size) {
    ir_t *ins = irgen_emit(op);
    ins->dst = ir_new_vreg();
    ins->size = size;
    return ins->dst;
}

void irgen_jmp(int target) {
    if (ir_bb_is_closed(irgen_cur_bb)) {
        return;
    }
    ir_t *ins = ir_add(irgen_cur_bb, IR_JMP);
    ins->bb0 = target;
}

void irgen_br(int cond, int bb_true, int bb_false) {
    ir_t *ins = irgen_emit(IR_BR);
    ins->a0 = cond;
    ins->bb0 = bb_true;
    ins->bb1 = bb_false;
}

int irgen_const(long value, int size) {
    ir_t *ins = irgen_emit(IR_CONST);
    ins->dst = ir_new_vreg();
    ins->imm = value;
    ins->size = size;
    return ins->dst;
}

int irgen_binop(int op, int lhs, int rhs, int size) {
    ir_t *ins = irgen_emit(op);
    ins->dst = ir_new_vreg();
    ins->a0 = lhs;
    ins->a1 = rhs;
    ins->size = size;
    return ins->dst;
}

int irgen_unop(int op, int a0, int size) {
    ir_t *ins = irgen_emit(op);
    ins->dst = ir_new_vreg();
    ins->a0 = a0;
    ins->size = size;
    return ins->dst;
}

void irgen_store(int value, int addr, int size) {
    ir_t *ins = irgen_emit(IR_STORE);
    ins->a0 = value;
    ins->a1 = addr;
    ins->size = size;
}

/* a two-way join: dst = phi(v0 from bb0, v1 from bb1); a value < 0 is dropped */
int irgen_phi2(int v0, int bb0, int v1, int bb1, int size) {
    ir_t *ins = irgen_emit(IR_PHI);
    ins->dst = ir_new_vreg();
    ins->size = size;
    ins->phi_val = int_vec_new();
    ins->phi_bb = int_vec_new();
    if (v0 >= 0) {
        int_vec_push(ins->phi_val, v0);
        int_vec_push(ins->phi_bb, bb0);
    }
    if (v1 >= 0) {
        int_vec_push(ins->phi_val, v1);
        int_vec_push(ins->phi_bb, bb1);
    }
    return ins->dst;
}

/* value of an operand which must produce one */
int irgen_value(int pos) {
    int v = irgen(pos);
    if (v < 0) {
        debug("statement used as a value at atom #%d - substituting 0", pos);
        return irgen_const(0, 8);
    }
    return v;
}

int irgen_arith_op(int atom_type) {
    switch (atom_type) {
        case TYPE_MEMBER_OFFSET: return IR_ADD;
        case TYPE_ADD: return IR_ADD;
        case TYPE_SUB: return IR_SUB;
        case TYPE_MUL: return IR_MUL;
        case TYPE_DIV: return IR_DIV;
        case TYPE_MOD: return IR_MOD;
        case TYPE_AND: return IR_AND;
        case TYPE_OR: return IR_OR;
        case TYPE_XOR: return IR_XOR;
        case TYPE_LSHIFT: return IR_SHL;
        case TYPE_RSHIFT: return IR_SHR;
        case TYPE_EQ_EQ: return IR_EQ;
        case TYPE_EQ_NE: return IR_NE;
        case TYPE_EQ_LT: return IR_LT;
        case TYPE_EQ_LE: return IR_LE;
        case TYPE_EQ_GT: return IR_GT;
        case TYPE_EQ_GE: return IR_GE;
    }
    error("no IR operator for atom type %d", atom_type);
    return IR_NOP;
}

void irgen_enter_loop(int break_bb, int continue_bb) {
    if (!irgen_break_bb) {
        irgen_break_bb = int_vec_new();
        irgen_continue_bb = int_vec_new();
    }
    int_vec_push(irgen_break_bb, break_bb);
    int_vec_push(irgen_continue_bb, continue_bb);
}

void irgen_exit_loop() {
    int_vec_pop(irgen_break_bb);
    int_vec_pop(irgen_continue_bb);
}

/*
 * Translate the atom at 'pos'. Returns the vreg holding its value, or -1
 * when the atom is a statement without a value.
 */
int irgen(int pos) {
    atom_t *p = &program[pos];
    set_token_pos(p->token_pos);

    switch (p->type) {
        case TYPE_NOP:
            return -1;

        case TYPE_INTEGER: {
            int size = type_size(p->t);
            long value = p->int_value;      // note: (long)int does not sign extend in rcc
            if (size == 8) {
                value = p->long_value;
            }
            return irgen_const(value, size);
        }

        case TYPE_STRING: {
            ir_t *ins = irgen_emit(IR_STRLIT);
            ins->dst = ir_new_vreg();
            ins->imm = p->int_value;
            return ins->dst;
        }

        case TYPE_VAR_REF: {
            ir_t *ins = irgen_emit(IR_LOCAL);
            ins->dst = ir_new_vreg();
            ins->imm = p->int_value;
            return ins->dst;
        }

        case TYPE_GLOBAL_VAR_REF: {
            ir_t *ins = irgen_emit(IR_GLOBAL);
            ins->dst = ir_new_vreg();
            ins->name = p->ptr_value;
            return ins->dst;
        }

        case TYPE_PTR:
        case TYPE_PTR_DEREF:
            return irgen(p->atom_pos);

        case TYPE_RVALUE: {
            int v = irgen_value(p->atom_pos);
            if (p->t->array_length >= 0 || p->t->struct_of) {
                return v;   // arrays and structs are handled through their address
            }
            return irgen_unop(IR_LOAD, v, type_size(p->t));
        }

        case TYPE_CONVERT: {
            int v = irgen_value(p->atom_pos);
            int org_size = type_size(program[p->atom_pos].t);
            int new_size = type_size(p->t);
            if (!p->t->ptr_to && org_size < new_size) {
                return irgen_unop(IR_SEXT, v, org_size);
            }
            return v;
        }

        case TYPE_CAST: {
            int v = irgen_value(p->atom_pos);
            int size = type_size(p->t);
            if (size == 8 || size == 0) {
                return v;
            }
            return irgen_unop(IR_SEXT, v, size);
        }

        case TYPE_BIND: {
            int rval = irgen_value(p->atom_pos);
            int lval = irgen_value((p + 1)->atom_pos);
            if (p->t->struct_of) {
                ir_t *ins = irgen_emit(IR_MEMCPY);
                ins->a0 = rval;
                ins->a1 = lval;
                ins->size = type_size(p->t);
            } else {
                irgen_store(rval, lval, type_size(p->t));
            }
            return rval;
        }

        case TYPE_ARRAY_INDEX: {
            int base = irgen_value(p->atom_pos);
            int index = irgen_value((p + 1)->atom_pos);
            long item_bytes = (p + 2)->int_value;
            int item_size = irgen_const(item_bytes, 8);
            int offset = irgen_binop(IR_MUL, index, item_size, 8);
            return irgen_binop(IR_ADD, base, offset, 8);
        }

        case TYPE_MEMBER_OFFSET:
        case TYPE_ADD: case TYPE_SUB: case TYPE_MUL: case TYPE_DIV: case TYPE_MOD:
        case TYPE_AND: case TYPE_OR: case TYPE_XOR:
        case TYPE_LSHIFT: case TYPE_RSHIFT:
        case TYPE_EQ_EQ: case TYPE_EQ_NE:
        case TYPE_EQ_LT: case TYPE_EQ_LE: case TYPE_EQ_GT: case TYPE_EQ_GE: {
            int lhs = irgen_value(p->atom_pos);
            int rhs = irgen_value((p + 1)->atom_pos);
            return irgen_binop(irgen_arith_op(p->type), lhs, rhs, type_size(p->t));
        }

        case TYPE_LOG_NOT:
            return irgen_unop(IR_LOGNOT, irgen_value(p->atom_pos), type_size(p->t));

        case TYPE_NEG:
            return irgen_unop(IR_BITNOT, irgen_value(p->atom_pos), type_size(p->t));

        case TYPE_POSTFIX_INC:
        case TYPE_POSTFIX_DEC: {
            int addr = irgen_value(p->atom_pos);
            int size = type_size(p->t);
            int delta = (p->t->ptr_to) ? type_size(p->t->ptr_to) : 1;
            if (p->type == TYPE_POSTFIX_DEC) {
                delta = -delta;
            }
            long step = delta;
            int old = irgen_unop(IR_LOAD, addr, size);
            int inc = irgen_binop(IR_ADD, old, irgen_const(step, size), size);
            irgen_store(inc, addr, size);
            return old;
        }

        case TYPE_EXPR_STATEMENT:
            return irgen(p->atom_pos);

        case TYPE_ANDTHEN:
            irgen(p->atom_pos);
            return irgen((p + 1)->atom_pos);

        case TYPE_LOG_AND:
        case TYPE_LOG_OR: {
            int size = type_size(p->t);
            int bb_rhs = ir_new_bb();
            int bb_end = ir_new_bb();
            int lhs = irgen_value(p->atom_pos);
            if (p->type == TYPE_LOG_AND) {
                irgen_br(lhs, bb_rhs, bb_end);
            } else {
                irgen_br(lhs, bb_end, bb_rhs);
            }
            int bb_lhs = irgen_cur_bb;

            irgen_cur_bb = bb_rhs;
            int rhs = irgen_value((p + 1)->atom_pos);
            int bb_rhs_end = irgen_cur_bb;
            bool rhs_reaches = !ir_bb_is_closed(bb_rhs_end);
            irgen_jmp(bb_end);

            irgen_cur_bb = bb_end;
            return irgen_phi2(lhs, bb_lhs, rhs_reaches ? rhs : -1, bb_rhs_end, size);
        }

        case TYPE_TERNARY: {
            int size = type_size(p->t);
            int bb_then = ir_new_bb();
            int bb_else = ir_new_bb();
            int bb_end = ir_new_bb();
            irgen_br(irgen_value(p->atom_pos), bb_then, bb_else);

            irgen_cur_bb = bb_then;
            int v1 = irgen_value((p + 1)->atom_pos);
            int bb_then_end = irgen_cur_bb;
            bool then_reaches = !ir_bb_is_closed(bb_then_end);
            irgen_jmp(bb_end);

            irgen_cur_bb = bb_else;
            int v2 = irgen_value((p + 2)->atom_pos);
            int bb_else_end = irgen_cur_bb;
            bool else_reaches = !ir_bb_is_closed(bb_else_end);
            irgen_jmp(bb_end);

            irgen_cur_bb = bb_end;
            return irgen_phi2(then_reaches ? v1 : -1, bb_then_end,
                              else_reaches ? v2 : -1, bb_else_end, size);
        }

        case TYPE_IF: {
            bool has_else = ((p + 2)->atom_pos != 0);
            int bb_then = ir_new_bb();
            int bb_end = ir_new_bb();
            int bb_else = has_else ? ir_new_bb() : bb_end;

            irgen_br(irgen_value(p->atom_pos), bb_then, bb_else);

            irgen_cur_bb = bb_then;
            irgen((p + 1)->atom_pos);
            irgen_jmp(bb_end);

            if (has_else) {
                irgen_cur_bb = bb_else;
                irgen((p + 2)->atom_pos);
                irgen_jmp(bb_end);
            }
            irgen_cur_bb = bb_end;
            return -1;
        }

        case TYPE_FOR: {
            int bb_cond = ir_new_bb();
            int bb_body = ir_new_bb();
            int bb_post = ir_new_bb();
            int bb_end = ir_new_bb();

            irgen((p + 2)->atom_pos);       // initializer
            irgen_jmp(bb_cond);

            irgen_cur_bb = bb_cond;
            irgen_br(irgen_value((p + 1)->atom_pos), bb_body, bb_end);

            irgen_enter_loop(bb_end, bb_post);
            irgen_cur_bb = bb_body;
            irgen(p->atom_pos);             // body
            irgen_jmp(bb_post);

            irgen_cur_bb = bb_post;
            irgen((p + 3)->atom_pos);       // post expression
            irgen_jmp(bb_cond);
            irgen_exit_loop();

            irgen_cur_bb = bb_end;
            return -1;
        }

        case TYPE_WHILE: {
            int bb_cond = ir_new_bb();
            int bb_body = ir_new_bb();
            int bb_end = ir_new_bb();

            irgen_jmp(bb_cond);
            irgen_cur_bb = bb_cond;
            irgen_br(irgen_value((p + 1)->atom_pos), bb_body, bb_end);

            irgen_enter_loop(bb_end, bb_cond);
            irgen_cur_bb = bb_body;
            irgen(p->atom_pos);
            irgen_jmp(bb_cond);
            irgen_exit_loop();

            irgen_cur_bb = bb_end;
            return -1;
        }

        case TYPE_DO_WHILE: {
            int bb_body = ir_new_bb();
            int bb_end = ir_new_bb();

            irgen_jmp(bb_body);
            irgen_enter_loop(bb_end, bb_body);
            irgen_cur_bb = bb_body;
            irgen(p->atom_pos);
            irgen_br(irgen_value((p + 1)->atom_pos), bb_body, bb_end);
            irgen_exit_loop();

            irgen_cur_bb = bb_end;
            return -1;
        }

        case TYPE_BREAK: {
            if (!int_vec_len(irgen_break_bb)) {
                error("cannot use 'break' here");
            }
            irgen_jmp(*int_vec_top(irgen_break_bb));
            return -1;
        }

        case TYPE_CONTINUE: {
            if (!int_vec_len(irgen_continue_bb)) {
                error("cannot use 'continue' here");
            }
            irgen_jmp(*int_vec_top(irgen_continue_bb));
            return -1;
        }

        case TYPE_RETURN: {
            int v = -1;
            int size = 8;
            if (p->t != type_void) {
                v = irgen_value(p->atom_pos);
                size = type_size(p->t);
            }
            ir_t *ins = irgen_emit(IR_RET);
            ins->a0 = v;
            ins->size = size;
            return -1;
        }

        case TYPE_SWITCH: {
            int value = irgen_value(p->atom_pos);
            int size = type_size(p->t);
            int bb_end = ir_new_bb();
            int bb_fall = ir_new_bb();

            irgen_enter_loop(bb_end, -1);
            atom_t *q = p + 1;
            while (q->type == TYPE_ARG) {
                int bb_next = ir_new_bb();
                atom_t *clause = &program[q->atom_pos];
                int body_pos;

                if (clause->type == TYPE_CASE) {
                    int cond = irgen_binop(IR_EQ, value, irgen_value(clause->atom_pos), size);
                    irgen_br(cond, bb_fall, bb_next);
                    body_pos = (clause + 1)->atom_pos;
                } else if (clause->type == TYPE_DEFAULT) {
                    irgen_jmp(bb_fall);
                    body_pos = clause->atom_pos;
                } else {
                    error("invalid child under a switch node");
                    return -1;
                }

                irgen_cur_bb = bb_fall;
                irgen(body_pos);
                bb_fall = ir_new_bb();
                irgen_jmp(bb_fall);         // fall through to the next clause body

                irgen_cur_bb = bb_next;
                q++;
            }
            irgen_jmp(bb_fall);
            irgen_cur_bb = bb_fall;
            irgen_jmp(bb_end);
            irgen_exit_loop();

            irgen_cur_bb = bb_end;
            return -1;
        }

        case TYPE_APPLY: {
            func *f = (func *)(p->ptr_value);
            int argc = (p + 1)->int_value;
            int arg_vreg[NUM_MAX_ARGS];

            // arguments are evaluated right to left, as the legacy emitter does
            for (int i = argc - 1; i >= 0; i--) {
                arg_vreg[i] = irgen_value((p + i + 2)->atom_pos);
            }

            ir_t *ins = irgen_emit(IR_CALL);
            ins->f = f;
            ins->args = ir_arg_vec_new();
            for (int i = 0; i < argc; i++) {
                type_t *t = program[(p + i + 2)->atom_pos].t;
                ir_arg_t *a = ir_arg_vec_extend(ins->args, 1);
                a->vreg = arg_vreg[i];
                if (t->struct_of) {
                    a->size = type_size(t);
                    a->is_struct = TRUE;
                } else {
                    a->size = 8;
                    a->is_struct = FALSE;
                }
            }
            ins->size = type_size(f->ret_type);
            if (ins->size > 0) {
                ins->dst = ir_new_vreg();
                return ins->dst;
            }
            return -1;
        }
    }

    dump_atom(pos, 0);
    error("cannot generate IR for this atom");
    return -1;
}

/* size actually used to spill an incoming argument register into its slot */
int irgen_arg_store_size(int size) {
    if (size == 1) {
        return 1;
    }
    if (size == 4) {
        return 4;
    }
    return 8;
}

/*
 * 'composite' tells that the slot is one word of a struct passed in
 * registers: such a slot overlaps the struct it belongs to and must not be
 * turned into an SSA value on its own.
 */
void irgen_store_arg(int reg_index, int offset, int size, bool composite) {
    int store_size = irgen_arg_store_size(size);

    ir_t *arg = irgen_emit(IR_ARG);
    arg->dst = ir_new_vreg();
    arg->imm = reg_index;
    arg->size = store_size;

    ir_t *local = irgen_emit(IR_LOCAL);
    local->dst = ir_new_vreg();
    local->imm = offset;
    if (composite) {
        local->size = 0;
    }

    irgen_store(arg->dst, local->dst, store_size);
}

void irgen_function(func *f) {
    ir_init_func(f);
    irgen_cur_bb = ir_new_bb();
    irgen_break_bb = int_vec_new();
    irgen_continue_bb = int_vec_new();

    // spill the incoming argument registers into the slots the parser assigned
    int reg_index = 0;
    for (int i = 0; i < f->argc; i++) {
        var_t *v = var_vec_get(f->argv, i);
        if (v->t->struct_of) {
            int size = type_size(v->t);
            if (size > 16 || reg_index >= ABI_NUM_GP || (size > 8 && reg_index == ABI_NUM_GP - 1)) {
                continue;   // passed on the stack: already in place
            }
            if (size <= 8) {
                irgen_store_arg(reg_index, v->offset, size, TRUE);
                reg_index++;
            } else {
                irgen_store_arg(reg_index, v->offset, 8, TRUE);
                reg_index++;
                irgen_store_arg(reg_index, v->offset - 8, 8, TRUE);
                reg_index++;
            }
            continue;
        }
        if (reg_index < ABI_NUM_GP) {
            irgen_store_arg(reg_index, v->offset, type_size(v->t), FALSE);
            reg_index++;
        }
    }

    irgen(f->body_pos);

    if (!ir_bb_is_closed(irgen_cur_bb)) {
        ir_t *ins = ir_add(irgen_cur_bb, IR_RET);
        ins->a0 = -1;
    }
    ir_build_edges();
    ir_dump("after irgen");
}
