/*
 * ir.c - containers and helpers for the SSA intermediate representation
 */
#include "types.h"
#include "rsys.h"
#include "rstring.h"
#include "devtool.h"
#include "vec.h"

#include "type.h"
#include "var.h"
#include "func.h"
#include "ir.h"

VEC_BODY(ir_arg_t, ir_arg_vec)
VEC_BODY(ir_t, ir_vec)
VEC_BODY(bb_t, bb_vec)

char *ir_op_name[] = {
    "nop", "const", "local", "global", "strlit", "arg",
    "load", "store", "memcpy", "mov", "zext", "sext",
    "add", "sub", "mul", "div", "mod",
    "and", "or", "xor", "shl", "shr",
    "eq", "ne", "lt", "le", "gt", "ge",
    "lognot", "bitnot",
    "call", "phi", "jmp", "br", "ret"
};

bb_vec ir_blocks;
int ir_nvreg = 0;
func *ir_current_func;
int ir_frame_size = 0;

int *vreg_reg;
int *vreg_slot;

bool opt_use_ssa = TRUE;
bool opt_dump_ssa = FALSE;

void ir_init_func(func *f) {
    ir_blocks = bb_vec_new();
    ir_nvreg = 0;
    ir_current_func = f;
    ir_frame_size = f->max_offset;
    vreg_reg = (void *)0;
    vreg_slot = (void *)0;
}

int ir_new_vreg() {
    return ir_nvreg++;
}

int ir_num_bb() {
    return bb_vec_len(ir_blocks);
}

bb_t *ir_get_bb(int id) {
    bb_t *b = bb_vec_get(ir_blocks, id);
    if (!b) {
        error("invalid basic block id: %d", id);
    }
    return b;
}

int ir_new_bb() {
    int id = bb_vec_len(ir_blocks);
    bb_t *b = bb_vec_extend(ir_blocks, 1);
    b->id = id;
    b->insns = ir_vec_new();
    b->preds = int_vec_new();
    b->succs = int_vec_new();
    b->df = int_vec_new();
    b->dom_children = int_vec_new();
    b->idom = -1;
    b->rpo = -1;
    b->first_index = 0;
    b->last_index = 0;
    return id;
}

void ir_clear(ir_t *ins, int op) {
    ins->op = op;
    ins->dst = -1;
    ins->a0 = -1;
    ins->a1 = -1;
    ins->imm = 0;
    ins->size = 8;
    ins->name = (void *)0;
    ins->f = (void *)0;
    ins->args = (void *)0;
    ins->phi_val = (void *)0;
    ins->phi_bb = (void *)0;
    ins->bb0 = -1;
    ins->bb1 = -1;
    ins->token_pos = 0;
}

ir_t *ir_add(int bb, int op) {
    ir_t *ins = ir_vec_extend(ir_get_bb(bb)->insns, 1);
    ir_clear(ins, op);
    return ins;
}

/*
 * Insert a new instruction before position 'index' of the block.
 * Only the item pointers held by the vector are shifted, so pointers to
 * instructions stay valid.
 */
ir_t *ir_insert(int bb, int index, int op) {
    ir_vec v = ir_get_bb(bb)->insns;
    int len = ir_vec_len(v);
    ir_vec_extend(v, 1);
    ir_t *fresh = v->items[len];
    for (int i = len; i > index; i--) {
        v->items[i] = v->items[i - 1];
    }
    v->items[index] = fresh;
    ir_clear(fresh, op);
    return fresh;
}

ir_t *ir_last(int bb) {
    return ir_vec_top(ir_get_bb(bb)->insns);
}

bool ir_is_terminator(int op) {
    return (op == IR_JMP || op == IR_BR || op == IR_RET);
}

bool ir_bb_is_closed(int bb) {
    ir_t *last = ir_last(bb);
    return (last != 0 && ir_is_terminator(last->op));
}

bool ir_has_side_effect(ir_t *ins) {
    switch (ins->op) {
        case IR_STORE:
        case IR_MEMCPY:
        case IR_CALL:
        case IR_JMP:
        case IR_BR:
        case IR_RET:
            return TRUE;
    }
    return FALSE;
}

int ir_alloc_slot(int size) {
    int a = (size >= 8) ? 8 : 4;
    ir_frame_size = align(ir_frame_size, a) + size;
    return ir_frame_size;
}

int ir_num_use(ir_t *ins) {
    if (ins->op == IR_CALL) {
        return ir_arg_vec_len(ins->args);
    }
    if (ins->op == IR_PHI) {
        return int_vec_len(ins->phi_val);
    }
    return 2;
}

int ir_get_use(ir_t *ins, int i) {
    if (ins->op == IR_CALL) {
        return ir_arg_vec_get(ins->args, i)->vreg;
    }
    if (ins->op == IR_PHI) {
        return *int_vec_get(ins->phi_val, i);
    }
    return (i == 0) ? ins->a0 : ins->a1;
}

void ir_set_use(ir_t *ins, int i, int vreg) {
    if (ins->op == IR_CALL) {
        ir_arg_vec_get(ins->args, i)->vreg = vreg;
        return;
    }
    if (ins->op == IR_PHI) {
        int_vec_set(ins->phi_val, i, vreg);
        return;
    }
    if (i == 0) {
        ins->a0 = vreg;
    } else {
        ins->a1 = vreg;
    }
}

void ir_build_edges() {
    int n = ir_num_bb();
    for (int i = 0; i < n; i++) {
        bb_t *b = ir_get_bb(i);
        b->preds = int_vec_new();
        b->succs = int_vec_new();
    }
    for (int i = 0; i < n; i++) {
        bb_t *b = ir_get_bb(i);
        ir_t *last = ir_vec_top(b->insns);
        if (!last) {
            continue;
        }
        if (last->op == IR_JMP) {
            int_vec_push(b->succs, last->bb0);
            int_vec_push(ir_get_bb(last->bb0)->preds, i);
        } else if (last->op == IR_BR) {
            int_vec_push(b->succs, last->bb0);
            int_vec_push(ir_get_bb(last->bb0)->preds, i);
            if (last->bb1 != last->bb0) {
                int_vec_push(b->succs, last->bb1);
                int_vec_push(ir_get_bb(last->bb1)->preds, i);
            }
        }
    }
}

void ir_dump_insn(char *buf, ir_t *ins) {
    if (ins->dst >= 0) {
        snprintf(buf + strlen(buf), RCC_BUF_SIZE, "v%d = ", ins->dst);
    }
    snprintf(buf + strlen(buf), RCC_BUF_SIZE, "%s.%d", ir_op_name[ins->op], ins->size);

    switch (ins->op) {
        case IR_CONST:
            snprintf(buf + strlen(buf), RCC_BUF_SIZE, " $%ld", ins->imm);
            break;
        case IR_LOCAL:
            snprintf(buf + strlen(buf), RCC_BUF_SIZE, " [rbp-%ld]", ins->imm);
            break;
        case IR_GLOBAL:
            snprintf(buf + strlen(buf), RCC_BUF_SIZE, " %s", ins->name);
            break;
        case IR_STRLIT:
            snprintf(buf + strlen(buf), RCC_BUF_SIZE, " .G%ld", ins->imm);
            break;
        case IR_ARG:
            snprintf(buf + strlen(buf), RCC_BUF_SIZE, " #%ld", ins->imm);
            break;
        case IR_JMP:
            snprintf(buf + strlen(buf), RCC_BUF_SIZE, " -> B%d", ins->bb0);
            break;
        case IR_BR:
            snprintf(buf + strlen(buf), RCC_BUF_SIZE, " v%d ? B%d : B%d", ins->a0, ins->bb0, ins->bb1);
            break;
        case IR_CALL:
            snprintf(buf + strlen(buf), RCC_BUF_SIZE, " %s(", ins->f->name);
            for (int i = 0; i < ir_arg_vec_len(ins->args); i++) {
                ir_arg_t *a = ir_arg_vec_get(ins->args, i);
                snprintf(buf + strlen(buf), RCC_BUF_SIZE, "%sv%d:%d%s",
                         (i > 0) ? ", " : "", a->vreg, a->size, a->is_struct ? "s" : "");
            }
            strcat(buf, ")");
            break;
        case IR_PHI:
            for (int i = 0; i < int_vec_len(ins->phi_val); i++) {
                snprintf(buf + strlen(buf), RCC_BUF_SIZE, " [v%d B%d]",
                         *int_vec_get(ins->phi_val, i), *int_vec_get(ins->phi_bb, i));
            }
            break;
        default:
            if (ins->a0 >= 0) {
                snprintf(buf + strlen(buf), RCC_BUF_SIZE, " v%d", ins->a0);
            }
            if (ins->a1 >= 0) {
                snprintf(buf + strlen(buf), RCC_BUF_SIZE, ", v%d", ins->a1);
            }
    }
}

void ir_dump(char *phase) {
    if (!opt_dump_ssa) {
        return;
    }
    char buf[RCC_BUF_SIZE];
    snprintf(buf, RCC_BUF_SIZE, "==== %s : %s (vregs:%d frame:%d)",
             ir_current_func->name, phase, ir_nvreg, ir_frame_size);
    debug("%s", buf);

    for (int i = 0; i < ir_num_bb(); i++) {
        bb_t *b = ir_get_bb(i);
        buf[0] = 0;
        snprintf(buf, RCC_BUF_SIZE, "B%d: idom=B%d preds=[", b->id, b->idom);
        for (int j = 0; j < int_vec_len(b->preds); j++) {
            snprintf(buf + strlen(buf), RCC_BUF_SIZE, "%sB%d", (j > 0) ? " " : "", *int_vec_get(b->preds, j));
        }
        strcat(buf, "]");
        debug("%s", buf);

        for (int j = 0; j < ir_vec_len(b->insns); j++) {
            ir_t *ins = ir_vec_get(b->insns, j);
            if (ins->op == IR_NOP) {
                continue;
            }
            buf[0] = 0;
            strcat(buf, "    ");
            ir_dump_insn(buf, ins);
            debug("%s", buf);
        }
    }
}
