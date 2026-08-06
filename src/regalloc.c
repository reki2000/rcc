/*
 * regalloc.c - liveness analysis and linear scan register allocation
 *
 * Runs after phi elimination, so the IR is an ordinary control flow graph of
 * three address instructions over virtual registers.
 *
 *   1. the reachable blocks are put in reverse post order and every
 *      instruction gets a position in that linear order
 *   2. live-in / live-out sets are computed with a backward data flow
 *      iteration over bit sets
 *   3. each vreg gets one live interval covering every position at which it
 *      is live, and the intervals are handed to a linear scan
 *
 * A vreg whose interval spans a call can only live in a callee saved
 * register; anything that does not get a register is spilled to a slot of
 * the stack frame and loaded into a scratch register at each use.
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
#include "emit.h"

int *ir_block_order;
int ir_block_count = 0;
char *ir_callee_used;

int ra_ninsn = 0;
int ra_nwords = 0;

long *ra_live_in;
long *ra_live_out;
long *ra_def;
long *ra_use;
int ra_live_cap = 0;        // in longs, per set

int *ra_start;
int *ra_end;
int *ra_call_prefix;

/* allocatable registers; %rax and %r11 stay free as scratch for spilled values */
int ra_caller_saved[] = { R_10 };
int ra_callee_saved[] = { R_BX, R_12, R_13, R_14, R_15 };
#define RA_NUM_CALLER_SAVED 1
#define RA_NUM_CALLEE_SAVED 5

long *ra_alloc_bits(long *buf, int need) {
    buf = realloc(buf, need * sizeof(long));
    for (int i = 0; i < need; i++) {
        buf[i] = 0;
    }
    return buf;
}

bool ra_test(long *set, int i) {
    return (set[i >> 6] & (1L << (i & 63))) != 0;
}

void ra_mark(long *set, int i) {
    set[i >> 6] = set[i >> 6] | (1L << (i & 63));
}

void ra_order_blocks() {
    int n = ir_num_bb();
    ir_block_order = calloc(n + 1, sizeof(int));
    ir_block_count = 0;

    for (int rpo = 0; rpo < n; rpo++) {
        for (int i = 0; i < n; i++) {
            if (ir_get_bb(i)->rpo == rpo) {
                ir_block_order[ir_block_count++] = i;
                break;
            }
        }
    }

    ra_ninsn = 0;
    for (int i = 0; i < ir_block_count; i++) {
        bb_t *b = ir_get_bb(ir_block_order[i]);
        b->first_index = ra_ninsn;
        ra_ninsn += ir_vec_len(b->insns);
        b->last_index = ra_ninsn - 1;
        if (b->last_index < b->first_index) {
            b->last_index = b->first_index;     // empty block
            ra_ninsn++;
        }
    }
}

void ra_compute_liveness() {
    ra_nwords = (ir_nvreg >> 6) + 1;
    int need = ir_block_count * ra_nwords;
    if (need > ra_live_cap) {
        ra_live_cap = need;
    }
    ra_live_in = ra_alloc_bits(ra_live_in, ra_live_cap);
    ra_live_out = ra_alloc_bits(ra_live_out, ra_live_cap);
    ra_def = ra_alloc_bits(ra_def, ra_live_cap);
    ra_use = ra_alloc_bits(ra_use, ra_live_cap);

    // local use / def sets; 'use' only holds vregs read before being written
    for (int i = 0; i < ir_block_count; i++) {
        bb_t *b = ir_get_bb(ir_block_order[i]);
        long *def = &ra_def[i * ra_nwords];
        long *use = &ra_use[i * ra_nwords];
        for (int j = 0; j < ir_vec_len(b->insns); j++) {
            ir_t *ins = ir_vec_get(b->insns, j);
            int nuse = ir_num_use(ins);
            for (int k = 0; k < nuse; k++) {
                int u = ir_get_use(ins, k);
                if (u >= 0 && !ra_test(def, u)) {
                    ra_mark(use, u);
                }
            }
            if (ins->dst >= 0) {
                ra_mark(def, ins->dst);
            }
        }
    }

    bool changed = TRUE;
    while (changed) {
        changed = FALSE;
        for (int i = ir_block_count - 1; i >= 0; i--) {
            bb_t *b = ir_get_bb(ir_block_order[i]);
            long *out = &ra_live_out[i * ra_nwords];
            long *in = &ra_live_in[i * ra_nwords];
            long *def = &ra_def[i * ra_nwords];
            long *use = &ra_use[i * ra_nwords];

            for (int s = 0; s < int_vec_len(b->succs); s++) {
                bb_t *sb = ir_get_bb(*int_vec_get(b->succs, s));
                if (sb->rpo < 0) {
                    continue;
                }
                long *sin = &ra_live_in[sb->rpo * ra_nwords];
                for (int w = 0; w < ra_nwords; w++) {
                    long merged = out[w] | sin[w];
                    if (merged != out[w]) {
                        out[w] = merged;
                        changed = TRUE;
                    }
                }
            }
            for (int w = 0; w < ra_nwords; w++) {
                long killed = out[w] & def[w];
                long merged = in[w] | use[w] | (out[w] ^ killed);
                if (merged != in[w]) {
                    in[w] = merged;
                    changed = TRUE;
                }
            }
        }
    }
}

void ra_extend(int vreg, int index) {
    if (vreg < 0) {
        return;
    }
    if (ra_start[vreg] < 0 || index < ra_start[vreg]) {
        ra_start[vreg] = index;
    }
    if (index > ra_end[vreg]) {
        ra_end[vreg] = index;
    }
}

void ra_build_intervals() {
    ra_start = calloc(ir_nvreg + 1, sizeof(int));
    ra_end = calloc(ir_nvreg + 1, sizeof(int));
    ra_call_prefix = calloc(ra_ninsn + 2, sizeof(int));

    for (int i = 0; i < ir_nvreg; i++) {
        ra_start[i] = -1;
        ra_end[i] = -1;
    }

    for (int i = 0; i < ir_block_count; i++) {
        bb_t *b = ir_get_bb(ir_block_order[i]);
        long *in = &ra_live_in[i * ra_nwords];
        long *out = &ra_live_out[i * ra_nwords];

        for (int v = 0; v < ir_nvreg; v++) {
            if (ra_test(in, v)) {
                ra_extend(v, b->first_index);
            }
            if (ra_test(out, v)) {
                ra_extend(v, b->last_index);
            }
        }
        for (int j = 0; j < ir_vec_len(b->insns); j++) {
            ir_t *ins = ir_vec_get(b->insns, j);
            int at = b->first_index + j;
            if (ins->op == IR_CALL) {
                ra_call_prefix[at + 1] = 1;
            }
            int nuse = ir_num_use(ins);
            for (int k = 0; k < nuse; k++) {
                ra_extend(ir_get_use(ins, k), at);
            }
            ra_extend(ins->dst, at);
        }
    }

    for (int i = 1; i <= ra_ninsn; i++) {
        ra_call_prefix[i] += ra_call_prefix[i - 1];
    }
}

bool ra_crosses_call(int vreg) {
    int s = ra_start[vreg];
    int e = ra_end[vreg];
    if (s < 0 || e <= s) {
        return FALSE;
    }
    // calls strictly after the definition and up to the last use
    return (ra_call_prefix[e + 1] - ra_call_prefix[s + 1]) > 0;
}

void ra_linear_scan() {
    vreg_reg = calloc(ir_nvreg + 1, sizeof(int));
    vreg_slot = calloc(ir_nvreg + 1, sizeof(int));
    ir_callee_used = calloc(R_LAST + 1, 1);

    for (int i = 0; i < ir_nvreg; i++) {
        vreg_reg[i] = -1;
        vreg_slot[i] = 0;
    }

    // bucket the vregs by the start of their interval - a counting sort
    int *bucket = calloc(ra_ninsn + 2, sizeof(int));
    int *next = calloc(ir_nvreg + 1, sizeof(int));
    for (int i = 0; i <= ra_ninsn + 1; i++) {
        bucket[i] = -1;
    }
    for (int v = ir_nvreg - 1; v >= 0; v--) {
        if (ra_start[v] < 0) {
            continue;
        }
        next[v] = bucket[ra_start[v]];
        bucket[ra_start[v]] = v;
    }

    // reg_owner[r] holds the vreg currently occupying the register, or -1
    int reg_owner[R_LAST];
    for (int r = 0; r < R_LAST; r++) {
        reg_owner[r] = -1;
    }

    int spilled = 0;
    int assigned = 0;
    for (int at = 0; at < ra_ninsn; at++) {
        // release the registers whose interval already ended
        for (int r = 0; r < R_LAST; r++) {
            if (reg_owner[r] >= 0 && ra_end[reg_owner[r]] < at) {
                reg_owner[r] = -1;
            }
        }
        for (int v = bucket[at]; v >= 0; v = next[v]) {
            bool crosses = ra_crosses_call(v);
            int chosen = -1;

            if (!crosses) {
                for (int i = 0; i < RA_NUM_CALLER_SAVED && chosen < 0; i++) {
                    if (reg_owner[ra_caller_saved[i]] < 0) {
                        chosen = ra_caller_saved[i];
                    }
                }
            }
            for (int i = 0; i < RA_NUM_CALLEE_SAVED && chosen < 0; i++) {
                if (reg_owner[ra_callee_saved[i]] < 0) {
                    chosen = ra_callee_saved[i];
                }
            }
            if (chosen < 0) {
                vreg_slot[v] = ir_alloc_slot(8);
                spilled++;
                continue;
            }
            reg_owner[chosen] = v;
            vreg_reg[v] = chosen;
            ir_callee_used[chosen] = 1;
            assigned++;
        }
    }
    debug("regalloc %s: %d vregs in registers, %d spilled", ir_current_func->name, assigned, spilled);
}

void regalloc_run() {
    ra_order_blocks();
    ra_compute_liveness();
    ra_build_intervals();
    ra_linear_scan();
}
