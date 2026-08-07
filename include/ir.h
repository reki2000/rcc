/*
 * ir.h - linear intermediate representation in SSA form
 *
 * The IR is a list of basic blocks. Each basic block holds a list of
 * instructions which operate on an unbounded set of virtual registers
 * ('vreg'). A vreg is identified by a small integer; -1 means 'no value'.
 *
 * Every block ends with exactly one terminator (IR_JMP / IR_BR / IR_RET).
 *
 * Requires: vec.h, type.h, var.h, func.h
 */

typedef enum {
    IR_NOP = 0,
    IR_CONST,       // dst = imm
    IR_LOCAL,       // dst = %rbp - imm            (address of a local variable)
                    //   size == 0 marks an address inside a larger object,
                    //   whose slot must stay in memory

    IR_GLOBAL,      // dst = &name                 (address of a global variable)
    IR_STRLIT,      // dst = &.G<imm>              (address of a string literal)
    IR_ARG,         // dst = <imm>-th incoming argument register
    IR_LOAD,        // dst = *(a0)                 zero extended from 'size' bytes
    IR_STORE,       // *(a1) = a0                  'size' bytes
    IR_MEMCPY,      // memcpy(a1, a0, size)
    IR_MOV,         // dst = a0
    IR_ZEXT,        // dst = zero_extend(a0, size)
    IR_SEXT,        // dst = sign_extend(a0, size)
    IR_ADD, IR_SUB, IR_MUL, IR_DIV, IR_MOD,
    IR_AND, IR_OR, IR_XOR, IR_SHL, IR_SHR,
    IR_EQ, IR_NE, IR_LT, IR_LE, IR_GT, IR_GE,
    IR_LOGNOT,      // dst = (a0 == 0)
    IR_BITNOT,      // dst = ~a0
    IR_CALL,        // dst = f(args...)
    IR_PHI,         // dst = phi(phi_val[i] from phi_bb[i], ...)
    IR_JMP,         // goto bb0
    IR_BR,          // if (a0) goto bb0 else goto bb1
    IR_RET,         // return a0   (a0 == -1 for 'return;')
    IR_OP_LAST
} ir_op_e;

extern char *ir_op_name[];

#define NUM_MAX_ARGS 200

typedef struct {
    int vreg;
    int size;
    bool is_struct;     // TRUE: 'vreg' holds the address of a struct of 'size' bytes
} ir_arg_t;

VEC_HEADER(ir_arg_t, ir_arg_vec)

typedef struct {
    int op;
    int dst;            // defined vreg, or -1
    int a0;             // operand vreg, or -1
    int a1;             // operand vreg, or -1
    long imm;
    int size;           // operand size in bytes (1, 4 or 8)
    char *name;         // IR_GLOBAL
    func *f;            // IR_CALL
    ir_arg_vec args;    // IR_CALL
    int_vec phi_val;    // IR_PHI - incoming values
    int_vec phi_bb;     // IR_PHI - incoming blocks (parallel to phi_val)
    int bb0;
    int bb1;
    int token_pos;
} ir_t;

VEC_HEADER(ir_t, ir_vec)

typedef struct {
    int id;
    ir_vec insns;
    int_vec preds;
    int_vec succs;
    int idom;               // immediate dominator, -1 for the entry block
    int_vec df;             // dominance frontier
    int_vec dom_children;   // children in the dominator tree
    int rpo;                // reverse post order number, -1 when unreachable
    int first_index;        // position of the first instruction in the linear order
    int last_index;
} bb_t;

VEC_HEADER(bb_t, bb_vec)

/* the function currently being compiled */
extern bb_vec ir_blocks;
extern int ir_nvreg;
extern func *ir_current_func;
extern int ir_frame_size;   // bytes of the stack frame below %rbp

/* filled in by regalloc.c, consumed by emit_ir.c */
extern int *vreg_reg;       // assigned physical register, or -1 when spilled
extern int *vreg_slot;      // frame offset of the spill slot (address = %rbp - slot)
extern int *ir_block_order; // reachable blocks in reverse post order
extern int ir_block_count;
extern char *ir_callee_used;// indexed by reg_e: the function must save it

void ir_init_func(func *f);
int ir_new_vreg();
int ir_new_bb();
bb_t *ir_get_bb(int id);
int ir_num_bb();
ir_t *ir_add(int bb, int op);
ir_t *ir_insert(int bb, int index, int op);
ir_t *ir_last(int bb);
bool ir_is_terminator(int op);
bool ir_bb_is_closed(int bb);
bool ir_has_side_effect(ir_t *ins);
int ir_alloc_slot(int size);

/* generic operand access, used by the optimizer and the register allocator */
int ir_num_use(ir_t *ins);
int ir_get_use(ir_t *ins, int i);
void ir_set_use(ir_t *ins, int i, int vreg);

void ir_build_edges();
void ir_dump(char *phase);

/* pipeline stages */
void irgen_function(func *f);       // irgen.c   : AST -> IR
void ssa_construct();               // ssa.c     : memory -> SSA values
void ssa_compute_dominators();      // ssa.c     : rpo, idom, dominance frontiers
void ssa_optimize();                // opt.c     : optimizations on SSA form
void ssa_destruct();                // ssa.c     : phi elimination
void regalloc_run();                // regalloc.c: liveness + linear scan
void emit_ir_function(func *f);     // emit_ir.c : IR -> x64
void ssa_compile_file(int fd);      // emit_ir.c : driver

extern bool opt_use_ssa;
extern bool opt_dump_ssa;
