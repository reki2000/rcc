/*
 * ssa.c - SSA construction and destruction
 *
 *  ssa_construct()
 *    1. reverse post order + dominator tree (Cooper/Harvey/Kennedy)
 *    2. dominance frontiers
 *    3. mem2reg: local variables whose address never escapes stop being
 *       memory and become SSA values; phi nodes are placed on the iterated
 *       dominance frontier of their assignments and operands are filled in
 *       by a walk over the dominator tree.
 *
 *  ssa_destruct()
 *    critical edges are split and every phi turns into copies at the end of
 *    its predecessors, so the register allocator never sees a phi.
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

int *ssa_post_order;        // block ids in post order
int ssa_post_count = 0;
char *ssa_visited;

void ssa_dfs(int b) {
    ssa_visited[b] = 1;
    bb_t *bb = ir_get_bb(b);
    for (int i = 0; i < int_vec_len(bb->succs); i++) {
        int s = *int_vec_get(bb->succs, i);
        if (!ssa_visited[s]) {
            ssa_dfs(s);
        }
    }
    ssa_post_order[ssa_post_count++] = b;
}

int ssa_intersect(int a, int b) {
    while (a != b) {
        while (ir_get_bb(a)->rpo > ir_get_bb(b)->rpo) {
            a = ir_get_bb(a)->idom;
            if (a < 0) return b;
        }
        while (ir_get_bb(b)->rpo > ir_get_bb(a)->rpo) {
            b = ir_get_bb(b)->idom;
            if (b < 0) return a;
        }
    }
    return a;
}

void ssa_add_unique(int_vec v, int value) {
    for (int i = 0; i < int_vec_len(v); i++) {
        if (*int_vec_get(v, i) == value) {
            return;
        }
    }
    int_vec_push(v, value);
}

void ssa_compute_dominators() {
    int n = ir_num_bb();

    ssa_visited = calloc(n, 1);
    ssa_post_order = calloc(n, sizeof(int));
    ssa_post_count = 0;

    for (int i = 0; i < n; i++) {
        bb_t *b = ir_get_bb(i);
        b->rpo = -1;
        b->idom = -1;
        b->df = int_vec_new();
        b->dom_children = int_vec_new();
    }

    ssa_dfs(0);
    for (int i = 0; i < ssa_post_count; i++) {
        ir_get_bb(ssa_post_order[i])->rpo = ssa_post_count - 1 - i;
    }

    ir_get_bb(0)->idom = 0;
    bool changed = TRUE;
    while (changed) {
        changed = FALSE;
        for (int i = ssa_post_count - 2; i >= 0; i--) {   // reverse post order, skipping the entry
            int id = ssa_post_order[i];
            bb_t *b = ir_get_bb(id);
            int new_idom = -1;
            for (int j = 0; j < int_vec_len(b->preds); j++) {
                int p = *int_vec_get(b->preds, j);
                if (ir_get_bb(p)->rpo < 0 || ir_get_bb(p)->idom < 0) {
                    continue;
                }
                new_idom = (new_idom < 0) ? p : ssa_intersect(p, new_idom);
            }
            if (new_idom >= 0 && b->idom != new_idom) {
                b->idom = new_idom;
                changed = TRUE;
            }
        }
    }

    // dominance frontiers
    for (int i = 0; i < n; i++) {
        bb_t *b = ir_get_bb(i);
        if (b->rpo < 0 || int_vec_len(b->preds) < 2) {
            continue;
        }
        for (int j = 0; j < int_vec_len(b->preds); j++) {
            int runner = *int_vec_get(b->preds, j);
            if (ir_get_bb(runner)->rpo < 0) {
                continue;
            }
            while (runner != b->idom) {
                ssa_add_unique(ir_get_bb(runner)->df, i);
                runner = ir_get_bb(runner)->idom;
                if (runner < 0) {
                    break;
                }
            }
        }
    }

    // dominator tree children
    for (int i = 1; i < n; i++) {
        bb_t *b = ir_get_bb(i);
        if (b->rpo < 0 || b->idom < 0 || b->idom == i) {
            continue;
        }
        int_vec_push(ir_get_bb(b->idom)->dom_children, i);
    }
}

/*
 * ---------------------------------------------------------------- mem2reg
 */
int ssa_nvar = 0;
int *ssa_var_offset;        // frame offset identifying the variable
int *ssa_var_size;          // access size, -1 until the first access is seen
char *ssa_var_ok;           // promotable
int *ssa_var_undef;         // vreg holding the value of a read-before-write
int *vreg_local;            // for a vreg defined by IR_LOCAL: its variable, else -1
int vreg_local_cap = 0;     // vregs created later than this are never IR_LOCAL

/*
 * TRUE when the address of some slot of this frame is used for something
 * else than a direct load or store - an '&x', an array index, a struct
 * member. Pointer arithmetic can then reach a neighbouring slot, so the
 * stores to promoted variables are kept and memory stays up to date. It is
 * exactly what test/008-pointer-addsub relies on.
 */
bool ssa_frame_escapes = FALSE;

int ssa_find_var(int offset) {
    for (int i = 0; i < ssa_nvar; i++) {
        if (ssa_var_offset[i] == offset) {
            return i;
        }
    }
    ssa_var_offset[ssa_nvar] = offset;
    ssa_var_size[ssa_nvar] = -1;
    /*
     * A slot above %rbp is an argument the caller placed on the stack: it
     * already holds a value that no instruction of this function wrote, so
     * it cannot become an SSA value.
     */
    ssa_var_ok[ssa_nvar] = (offset > 0) ? 1 : 0;
    ssa_var_undef[ssa_nvar] = -1;
    return ssa_nvar++;
}

void ssa_note_access(int var, int size) {
    if (size != 1 && size != 4 && size != 8) {
        ssa_var_ok[var] = 0;
        return;
    }
    if (ssa_var_size[var] < 0) {
        ssa_var_size[var] = size;
    } else if (ssa_var_size[var] != size) {
        ssa_var_ok[var] = 0;   // the same slot is accessed with different widths
    }
}

/* collect the local variables that can become SSA values */
void ssa_collect_vars() {
    int n = ir_num_bb();

    ssa_nvar = 0;
    ssa_frame_escapes = FALSE;
    ssa_var_offset = calloc(ir_nvreg + 1, sizeof(int));
    ssa_var_size = calloc(ir_nvreg + 1, sizeof(int));
    ssa_var_ok = calloc(ir_nvreg + 1, 1);
    ssa_var_undef = calloc(ir_nvreg + 1, sizeof(int));
    vreg_local_cap = ir_nvreg;
    vreg_local = calloc(vreg_local_cap + 1, sizeof(int));

    for (int i = 0; i < vreg_local_cap; i++) {
        vreg_local[i] = -1;
    }

    for (int i = 0; i < n; i++) {
        bb_t *b = ir_get_bb(i);
        if (b->rpo < 0) {
            continue;
        }
        for (int j = 0; j < ir_vec_len(b->insns); j++) {
            ir_t *ins = ir_vec_get(b->insns, j);
            if (ins->op == IR_LOCAL && ins->dst >= 0) {
                int var = ssa_find_var((int)(ins->imm));
                vreg_local[ins->dst] = var;
                if (ins->size == 0) {
                    ssa_var_ok[var] = 0;    // one word of a bigger object
                }
            }
        }
    }

    for (int i = 0; i < n; i++) {
        bb_t *b = ir_get_bb(i);
        if (b->rpo < 0) {
            continue;
        }
        for (int j = 0; j < ir_vec_len(b->insns); j++) {
            ir_t *ins = ir_vec_get(b->insns, j);
            int nuse = ir_num_use(ins);
            for (int k = 0; k < nuse; k++) {
                int u = ir_get_use(ins, k);
                if (u < 0 || u >= vreg_local_cap || vreg_local[u] < 0) {
                    continue;
                }
                int var = vreg_local[u];
                if (ins->op == IR_LOAD && k == 0) {
                    ssa_note_access(var, ins->size);
                } else if (ins->op == IR_STORE && k == 1) {
                    ssa_note_access(var, ins->size);
                } else {
                    ssa_var_ok[var] = 0;   // the address escapes
                    ssa_frame_escapes = TRUE;
                }
            }
        }
    }

    for (int i = 0; i < ssa_nvar; i++) {
        if (ssa_var_size[i] < 0) {
            ssa_var_ok[i] = 0;
        }
    }
}

/* is 'ins' an access to a promoted variable? returns its index or -1 */
int ssa_access_var(ir_t *ins) {
    int addr = -1;
    if (ins->op == IR_LOAD) {
        addr = ins->a0;
    } else if (ins->op == IR_STORE) {
        addr = ins->a1;
    } else {
        return -1;
    }
    if (addr < 0 || addr >= vreg_local_cap || vreg_local[addr] < 0) {
        return -1;
    }
    int var = vreg_local[addr];
    return ssa_var_ok[var] ? var : -1;
}

void ssa_insert_phis() {
    int n = ir_num_bb();
    char *has_phi = calloc(n, 1);
    char *in_worklist = calloc(n, 1);
    int *worklist = calloc(n + 1, sizeof(int));

    for (int var = 0; var < ssa_nvar; var++) {
        if (!ssa_var_ok[var]) {
            continue;
        }
        int wcount = 0;
        for (int i = 0; i < n; i++) {
            has_phi[i] = 0;
            in_worklist[i] = 0;
        }
        for (int i = 0; i < n; i++) {
            bb_t *b = ir_get_bb(i);
            if (b->rpo < 0) {
                continue;
            }
            for (int j = 0; j < ir_vec_len(b->insns); j++) {
                ir_t *ins = ir_vec_get(b->insns, j);
                if (ins->op == IR_STORE && ssa_access_var(ins) == var && !in_worklist[i]) {
                    in_worklist[i] = 1;
                    worklist[wcount++] = i;
                }
            }
        }
        while (wcount > 0) {
            int b = worklist[--wcount];
            int_vec df = ir_get_bb(b)->df;
            for (int i = 0; i < int_vec_len(df); i++) {
                int d = *int_vec_get(df, i);
                if (has_phi[d]) {
                    continue;
                }
                has_phi[d] = 1;
                ir_t *phi = ir_insert(d, 0, IR_PHI);
                phi->dst = ir_new_vreg();
                phi->size = ssa_var_size[var];
                phi->imm = var + 1;           // marks a mem2reg phi
                phi->phi_val = int_vec_new();
                phi->phi_bb = int_vec_new();
                if (!in_worklist[d]) {
                    in_worklist[d] = 1;
                    worklist[wcount++] = d;
                }
            }
        }
    }
}

/*
 * Put an IR_ZEXT right after every store to a promotable variable. That
 * instruction becomes the SSA definition of the value the variable holds
 * from there on; the store itself is dropped later unless the frame escapes.
 *
 * Storing 's' bytes and loading them back yields the value zero extended
 * from 's' bytes, which is what the extension expresses.
 */
void ssa_insert_defs() {
    for (int i = 0; i < ir_num_bb(); i++) {
        bb_t *b = ir_get_bb(i);
        if (b->rpo < 0) {
            continue;
        }
        for (int j = ir_vec_len(b->insns) - 1; j >= 0; j--) {
            ir_t *ins = ir_vec_get(b->insns, j);
            if (ins->op != IR_STORE) {
                continue;
            }
            int var = ssa_access_var(ins);
            if (var < 0) {
                continue;
            }
            ir_t *def = ir_insert(i, j + 1, IR_ZEXT);
            def->dst = ir_new_vreg();
            def->a0 = ins->a0;
            def->size = ins->size;
            def->imm = var + 1;             // marks a mem2reg definition
        }
    }
}

int_vec *ssa_stack;

int ssa_top(int var) {
    int_vec s = ssa_stack[var];
    if (!int_vec_len(s)) {
        return ssa_var_undef[var];
    }
    return *int_vec_top(s);
}

void ssa_rename(int b) {
    bb_t *bb = ir_get_bb(b);
    int *pushed = calloc(ssa_nvar + 1, sizeof(int));

    for (int j = 0; j < ir_vec_len(bb->insns); j++) {
        ir_t *ins = ir_vec_get(bb->insns, j);

        if ((ins->op == IR_PHI || ins->op == IR_ZEXT) && ins->imm > 0) {
            int var = (int)(ins->imm) - 1;
            int_vec_push(ssa_stack[var], ins->dst);
            pushed[var]++;
            continue;
        }

        int var = ssa_access_var(ins);
        if (var < 0) {
            continue;
        }
        if (ins->op == IR_LOAD) {
            // the load result becomes a plain copy of the value in flight
            ins->op = IR_MOV;
            ins->a0 = ssa_top(var);
            ins->size = 8;
        } else if (!ssa_frame_escapes) {
            ins->op = IR_NOP;           // the value only lives in a vreg now
            ins->a0 = -1;
            ins->a1 = -1;
        }
    }

    for (int i = 0; i < int_vec_len(bb->succs); i++) {
        int s = *int_vec_get(bb->succs, i);
        bb_t *sb = ir_get_bb(s);
        for (int j = 0; j < ir_vec_len(sb->insns); j++) {
            ir_t *phi = ir_vec_get(sb->insns, j);
            if (phi->op != IR_PHI || phi->imm <= 0) {
                continue;   // not a phi, or one built by irgen with known operands
            }
            int_vec_push(phi->phi_val, ssa_top((int)(phi->imm) - 1));
            int_vec_push(phi->phi_bb, b);
        }
    }

    for (int i = 0; i < int_vec_len(bb->dom_children); i++) {
        ssa_rename(*int_vec_get(bb->dom_children, i));
    }

    for (int var = 0; var < ssa_nvar; var++) {
        for (int i = 0; i < pushed[var]; i++) {
            int_vec_pop(ssa_stack[var]);
        }
    }
}

void ssa_clear_marks() {
    for (int i = 0; i < ir_num_bb(); i++) {
        bb_t *b = ir_get_bb(i);
        for (int j = 0; j < ir_vec_len(b->insns); j++) {
            ir_t *ins = ir_vec_get(b->insns, j);
            if (ins->op == IR_PHI || ins->op == IR_ZEXT) {
                ins->imm = 0;
            }
        }
    }
}

void ssa_mem2reg() {
    ssa_collect_vars();

    int promoted = 0;
    for (int i = 0; i < ssa_nvar; i++) {
        if (ssa_var_ok[i]) {
            promoted++;
        }
    }
    if (!promoted) {
        return;
    }

    // one 'undefined' value per promoted variable, for reads before any write
    for (int var = 0; var < ssa_nvar; var++) {
        if (!ssa_var_ok[var]) {
            continue;
        }
        ir_t *ins = ir_insert(0, 0, IR_CONST);
        ins->dst = ir_new_vreg();
        ins->imm = 0;
        ins->size = 8;
        ssa_var_undef[var] = ins->dst;
    }

    ssa_insert_phis();
    ssa_insert_defs();

    ssa_stack = calloc(ssa_nvar + 1, sizeof(int_vec));
    for (int i = 0; i < ssa_nvar; i++) {
        ssa_stack[i] = int_vec_new();
    }
    ssa_rename(0);
    ssa_clear_marks();

    debug("mem2reg: promoted %d of %d local slots in %s (frame escapes:%d)",
          promoted, ssa_nvar, ir_current_func->name, ssa_frame_escapes);
}

void ssa_construct() {
    ssa_compute_dominators();
    ssa_mem2reg();
    ir_dump("after ssa construction");
}

/*
 * ------------------------------------------------------------ destruction
 */

/* replace every reference to block 'from' in the terminator of 'bb' */
void ssa_retarget(int bb, int from, int to) {
    ir_t *last = ir_last(bb);
    if (!last) {
        return;
    }
    if (last->bb0 == from) {
        last->bb0 = to;
    }
    if (last->bb1 == from) {
        last->bb1 = to;
    }
}

/*
 * A critical edge (a predecessor with several successors feeding a block with
 * several predecessors) has no place to put the copies a phi turns into, so
 * an empty block is inserted in the middle.
 */
void ssa_split_critical_edges() {
    int n = ir_num_bb();
    bool split = FALSE;

    for (int i = 0; i < n; i++) {
        bb_t *b = ir_get_bb(i);
        if (b->rpo < 0 || int_vec_len(b->preds) < 2) {
            continue;
        }
        for (int j = 0; j < int_vec_len(b->preds); j++) {
            int p = *int_vec_get(b->preds, j);
            if (int_vec_len(ir_get_bb(p)->succs) < 2) {
                continue;
            }
            int mid = ir_new_bb();
            ir_t *jmp = ir_add(mid, IR_JMP);
            jmp->bb0 = i;
            ssa_retarget(p, i, mid);

            for (int k = 0; k < ir_vec_len(b->insns); k++) {
                ir_t *phi = ir_vec_get(b->insns, k);
                if (phi->op != IR_PHI) {
                    continue;
                }
                for (int m = 0; m < int_vec_len(phi->phi_bb); m++) {
                    if (*int_vec_get(phi->phi_bb, m) == p) {
                        int_vec_set(phi->phi_bb, m, mid);
                    }
                }
            }
            split = TRUE;
        }
    }
    if (split) {
        ir_build_edges();
        ssa_compute_dominators();
    }
}

/*
 * Collect the phis of a block. The optimizer may have turned some of them
 * into copies or removed them, so they are not necessarily a contiguous run
 * at the top of the block any more.
 */
int ssa_collect_phis(bb_t *b, ir_t **out, int limit) {
    int count = 0;
    for (int i = 0; i < ir_vec_len(b->insns) && count < limit; i++) {
        ir_t *ins = ir_vec_get(b->insns, i);
        if (ins->op == IR_PHI) {
            out[count++] = ins;
        }
    }
    return count;
}

/*
 * All phis of a block read their operands simultaneously. When one phi reads
 * the result of another phi of the same block, the copies have to go through
 * temporaries to keep that meaning.
 */
bool ssa_phis_need_temps(ir_t **phis, int count) {
    for (int i = 0; i < count; i++) {
        for (int j = 0; j < int_vec_len(phis[i]->phi_val); j++) {
            int v = *int_vec_get(phis[i]->phi_val, j);
            for (int k = 0; k < count; k++) {
                if (phis[k]->dst == v) {
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}

/* append 'dst = src' just before the terminator of block 'bb' */
void ssa_append_copy(int bb, int dst, int src) {
    int len = ir_vec_len(ir_get_bb(bb)->insns);
    int at = (len > 0 && ir_is_terminator(ir_vec_get(ir_get_bb(bb)->insns, len - 1)->op)) ? len - 1 : len;
    ir_t *mov = ir_insert(bb, at, IR_MOV);
    mov->dst = dst;
    mov->a0 = src;
    mov->size = 8;
}

void ssa_destruct() {
    ssa_split_critical_edges();

    int n = ir_num_bb();
    for (int i = 0; i < n; i++) {
        bb_t *b = ir_get_bb(i);
        if (b->rpo < 0) {
            continue;
        }
        int limit = ir_vec_len(b->insns) + 1;
        ir_t **phis = calloc(limit, sizeof(ir_t *));
        int count = ssa_collect_phis(b, phis, limit);
        if (!count) {
            continue;
        }
        bool use_temps = ssa_phis_need_temps(phis, count);
        int *temp = calloc(count + 1, sizeof(int));
        if (use_temps) {
            for (int j = 0; j < count; j++) {
                temp[j] = ir_new_vreg();
            }
        }

        for (int j = 0; j < count; j++) {
            ir_t *phi = phis[j];
            for (int k = 0; k < int_vec_len(phi->phi_val); k++) {
                int src = *int_vec_get(phi->phi_val, k);
                int pred = *int_vec_get(phi->phi_bb, k);
                if (src < 0 || ir_get_bb(pred)->rpo < 0) {
                    continue;
                }
                ssa_append_copy(pred, use_temps ? temp[j] : phi->dst, src);
            }
        }
        if (use_temps) {
            for (int j = 0; j < count; j++) {
                ir_t *phi = phis[j];
                for (int k = 0; k < int_vec_len(phi->phi_bb); k++) {
                    int pred = *int_vec_get(phi->phi_bb, k);
                    if (*int_vec_get(phi->phi_val, k) < 0 || ir_get_bb(pred)->rpo < 0) {
                        continue;
                    }
                    ssa_append_copy(pred, phi->dst, temp[j]);
                }
            }
        }
        for (int j = 0; j < count; j++) {
            phis[j]->op = IR_NOP;
            phis[j]->dst = -1;
        }
    }
    ir_dump("after phi elimination");
}
