/*
 * opt.c - optimizations that run while the IR is in SSA form
 *
 *   - constant folding and propagation
 *   - folding of conditional branches with a constant condition
 *   - copy propagation (removal of IR_MOV chains)
 *   - dead code elimination
 *
 * Every one of them relies on the single-assignment property: the defining
 * instruction of a vreg is unique, so 'the value of v' is a property of v
 * alone and can be looked up in a flat array.
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

ir_t **opt_def;             // defining instruction of each vreg
int *opt_use_count;
int opt_cap = 0;

void opt_build_def() {
    opt_cap = ir_nvreg + 1;
    opt_def = calloc(opt_cap, sizeof(ir_t *));
    opt_use_count = calloc(opt_cap, sizeof(int));

    for (int i = 0; i < ir_num_bb(); i++) {
        bb_t *b = ir_get_bb(i);
        if (b->rpo < 0) {
            continue;
        }
        for (int j = 0; j < ir_vec_len(b->insns); j++) {
            ir_t *ins = ir_vec_get(b->insns, j);
            if (ins->dst >= 0) {
                opt_def[ins->dst] = ins;
            }
            int nuse = ir_num_use(ins);
            for (int k = 0; k < nuse; k++) {
                int u = ir_get_use(ins, k);
                if (u >= 0) {
                    opt_use_count[u]++;
                }
            }
        }
    }
}

bool opt_is_const(int vreg) {
    return (vreg >= 0 && vreg < opt_cap && opt_def[vreg] != 0 && opt_def[vreg]->op == IR_CONST);
}

long opt_const_of(int vreg) {
    return opt_def[vreg]->imm;
}

/* turn 'ins' into a constant */
void opt_make_const(ir_t *ins, long value, int size) {
    ins->op = IR_CONST;
    ins->a0 = -1;
    ins->a1 = -1;
    ins->imm = value;
    ins->size = size;
}

long opt_truncate(long value, int size) {
    if (size == 4) {
        return (long)((int)value);
    }
    return value;
}

long opt_zero_extend(long value, int size) {
    if (size == 1) {
        return value & 0xffL;
    }
    if (size == 4) {
        return value & 0xffffffffL;
    }
    return value;
}

long opt_sign_extend(long value, int size) {
    if (size == 1) {
        return (long)((char)value);
    }
    if (size == 4) {
        return (long)((int)value);
    }
    return value;
}

/*
 * Fold a binary operation on two constants. Returns FALSE when the result
 * cannot be modelled exactly (byte sized arithmetic keeps the upper bits of
 * the register, division by zero, out of range shifts).
 */
bool opt_fold_binop(int op, long l, long r, int size, long *out) {
    bool is_compare = (op == IR_EQ || op == IR_NE || op == IR_LT ||
                       op == IR_LE || op == IR_GT || op == IR_GE);

    if (size == 1 && !is_compare) {
        return FALSE;
    }
    if (is_compare) {
        l = opt_sign_extend(l, size);
        r = opt_sign_extend(r, size);
        int yes;    // an int, not a bool: rcc cannot assign a long comparison to an enum
        switch (op) {
            case IR_EQ: yes = (l == r); break;
            case IR_NE: yes = (l != r); break;
            case IR_LT: yes = (l < r); break;
            case IR_LE: yes = (l <= r); break;
            case IR_GT: yes = (l > r); break;
            case IR_GE: yes = (l >= r); break;
            default: return FALSE;
        }
        long value = yes ? 1 : 0;
        *out = value;
        return TRUE;
    }

    if ((op == IR_DIV || op == IR_MOD) && r == 0) {
        return FALSE;
    }
    if ((op == IR_SHL || op == IR_SHR) && (r < 0 || r >= size * 8)) {
        return FALSE;
    }

    long v;
    switch (op) {
        case IR_ADD: v = l + r; break;
        case IR_SUB: v = l - r; break;
        case IR_MUL: v = l * r; break;
        case IR_DIV: v = l / r; break;
        case IR_MOD: v = l % r; break;
        case IR_AND: v = l & r; break;
        case IR_OR:  v = l | r; break;
        case IR_XOR: v = l ^ r; break;
        case IR_SHL: v = l << r; break;
        case IR_SHR: v = l >> r; break;
        default: return FALSE;
    }
    *out = opt_truncate(v, size);
    return TRUE;
}

bool opt_fold_constants() {
    bool changed = FALSE;

    for (int i = 0; i < ir_num_bb(); i++) {
        bb_t *b = ir_get_bb(i);
        if (b->rpo < 0) {
            continue;
        }
        for (int j = 0; j < ir_vec_len(b->insns); j++) {
            ir_t *ins = ir_vec_get(b->insns, j);
            if (ins->dst < 0 || ins->op == IR_CONST) {
                continue;
            }
            switch (ins->op) {
                case IR_MOV:
                    if (opt_is_const(ins->a0)) {
                        opt_make_const(ins, opt_const_of(ins->a0), opt_def[ins->a0]->size);
                        changed = TRUE;
                    }
                    break;
                case IR_ZEXT:
                    if (opt_is_const(ins->a0)) {
                        opt_make_const(ins, opt_zero_extend(opt_const_of(ins->a0), ins->size), 8);
                        changed = TRUE;
                    }
                    break;
                case IR_SEXT:
                    if (opt_is_const(ins->a0)) {
                        opt_make_const(ins, opt_sign_extend(opt_const_of(ins->a0), ins->size), 8);
                        changed = TRUE;
                    }
                    break;
                case IR_LOGNOT:
                    if (opt_is_const(ins->a0)) {
                        long zero = 0;
                        long one = 1;
                        opt_make_const(ins, (opt_const_of(ins->a0) == 0) ? one : zero, 8);
                        changed = TRUE;
                    }
                    break;
                case IR_BITNOT:
                    if (ins->size != 1 && opt_is_const(ins->a0)) {
                        long v = opt_const_of(ins->a0);
                        opt_make_const(ins, opt_truncate(0 - v - 1, ins->size), ins->size);
                        changed = TRUE;
                    }
                    break;
                case IR_PHI: {
                    // a phi whose incoming values are all the same is a copy
                    int n = int_vec_len(ins->phi_val);
                    int same = -1;
                    bool uniform = (n > 0);
                    for (int k = 0; k < n; k++) {
                        int v = *int_vec_get(ins->phi_val, k);
                        if (v == ins->dst) {
                            continue;
                        }
                        if (same < 0) {
                            same = v;
                        } else if (same != v) {
                            uniform = FALSE;
                        }
                    }
                    if (uniform && same >= 0) {
                        ins->op = IR_MOV;
                        ins->a0 = same;
                        ins->size = 8;
                        ins->phi_val = (void *)0;
                        ins->phi_bb = (void *)0;
                        changed = TRUE;
                    }
                    break;
                }
                default: {
                    long value;
                    if (opt_is_const(ins->a0) && opt_is_const(ins->a1) &&
                        opt_fold_binop(ins->op, opt_const_of(ins->a0), opt_const_of(ins->a1),
                                       ins->size, &value)) {
                        bool is_compare = (ins->op == IR_EQ || ins->op == IR_NE || ins->op == IR_LT ||
                                           ins->op == IR_LE || ins->op == IR_GT || ins->op == IR_GE);
                        opt_make_const(ins, value, is_compare ? 8 : ins->size);
                        changed = TRUE;
                    }
                }
            }
        }
    }
    return changed;
}

/*
 * Replace a conditional branch on a known value by an unconditional jump.
 * The CFG changes, so dominance information is rebuilt afterwards.
 */
bool opt_fold_branches() {
    bool changed = FALSE;

    for (int i = 0; i < ir_num_bb(); i++) {
        bb_t *b = ir_get_bb(i);
        if (b->rpo < 0) {
            continue;
        }
        ir_t *last = ir_vec_top(b->insns);
        if (!last || last->op != IR_BR || !opt_is_const(last->a0)) {
            continue;
        }
        int target = (opt_const_of(last->a0) != 0) ? last->bb0 : last->bb1;
        last->op = IR_JMP;
        last->a0 = -1;
        last->bb0 = target;
        last->bb1 = -1;
        changed = TRUE;
    }
    return changed;
}

bool opt_int_vec_has(int_vec v, int value) {
    for (int i = 0; i < int_vec_len(v); i++) {
        if (*int_vec_get(v, i) == value) {
            return TRUE;
        }
    }
    return FALSE;
}

/*
 * After the CFG changed, some phi operands may come from blocks that are no
 * longer predecessors. Such an operand must go, otherwise phi elimination
 * would place its copy on a path that does not reach the phi any more.
 */
void opt_prune_phi_entries() {
    for (int i = 0; i < ir_num_bb(); i++) {
        bb_t *b = ir_get_bb(i);
        for (int j = 0; j < ir_vec_len(b->insns); j++) {
            ir_t *phi = ir_vec_get(b->insns, j);
            if (phi->op != IR_PHI) {
                continue;
            }
            int_vec val = int_vec_new();
            int_vec bbs = int_vec_new();
            for (int k = 0; k < int_vec_len(phi->phi_bb); k++) {
                int pred = *int_vec_get(phi->phi_bb, k);
                if (!opt_int_vec_has(b->preds, pred)) {
                    continue;
                }
                int_vec_push(val, *int_vec_get(phi->phi_val, k));
                int_vec_push(bbs, pred);
            }
            phi->phi_val = val;
            phi->phi_bb = bbs;
        }
    }
}

/*
 * Copy propagation. 'v = mov w' makes every later reader of v able to read w
 * instead; chains are collapsed in one pass because the IR is in SSA form.
 */
bool opt_copy_propagate() {
    int *rep = calloc(ir_nvreg + 1, sizeof(int));
    for (int i = 0; i < ir_nvreg; i++) {
        rep[i] = i;
    }

    for (int i = 0; i < ir_num_bb(); i++) {
        bb_t *b = ir_get_bb(i);
        if (b->rpo < 0) {
            continue;
        }
        for (int j = 0; j < ir_vec_len(b->insns); j++) {
            ir_t *ins = ir_vec_get(b->insns, j);
            if (ins->op == IR_MOV && ins->dst >= 0 && ins->a0 >= 0 && ins->size == 8) {
                rep[ins->dst] = ins->a0;
            }
        }
    }

    bool changed = FALSE;
    for (int i = 0; i < ir_num_bb(); i++) {
        bb_t *b = ir_get_bb(i);
        if (b->rpo < 0) {
            continue;
        }
        for (int j = 0; j < ir_vec_len(b->insns); j++) {
            ir_t *ins = ir_vec_get(b->insns, j);
            int nuse = ir_num_use(ins);
            for (int k = 0; k < nuse; k++) {
                int u = ir_get_use(ins, k);
                if (u < 0) {
                    continue;
                }
                int limit = ir_nvreg;
                int v = u;
                while (rep[v] != v && limit-- > 0) {
                    v = rep[v];
                }
                if (v != u) {
                    ir_set_use(ins, k, v);
                    changed = TRUE;
                }
            }
        }
    }
    return changed;
}

bool opt_dead_code() {
    opt_build_def();
    bool changed = FALSE;

    for (int i = 0; i < ir_num_bb(); i++) {
        bb_t *b = ir_get_bb(i);
        for (int j = 0; j < ir_vec_len(b->insns); j++) {
            ir_t *ins = ir_vec_get(b->insns, j);
            if (ins->op == IR_NOP) {
                continue;
            }
            if (b->rpo < 0) {
                ins->op = IR_NOP;       // unreachable code is never emitted
                ins->dst = -1;
                changed = TRUE;
                continue;
            }
            if (ins->dst < 0 || ir_has_side_effect(ins)) {
                continue;
            }
            if (opt_use_count[ins->dst] == 0) {
                ins->op = IR_NOP;
                ins->dst = -1;
                ins->a0 = -1;
                ins->a1 = -1;
                changed = TRUE;
            }
        }
    }
    return changed;
}

void ssa_optimize() {
    for (int round = 0; round < 8; round++) {
        opt_build_def();
        bool changed = opt_fold_constants();

        if (opt_fold_branches()) {
            ir_build_edges();
            ssa_compute_dominators();
            opt_prune_phi_entries();
            changed = TRUE;
        }
        if (opt_copy_propagate()) {
            changed = TRUE;
        }
        if (opt_dead_code()) {
            changed = TRUE;
        }
        if (!changed) {
            break;
        }
    }
    ir_dump("after optimization");
}
