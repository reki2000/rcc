# SSA backend

This document describes the SSA based backend of rcc: what it does, where the
code lives, and which algorithms are used.

Before this change rcc walked the AST and emitted x64 directly
(`src/emit.c`), assigning registers round robin inside a single expression.
There was no intermediate representation, so nothing could be analysed or
optimized across statements.

The SSA backend inserts a real middle end between the parser and the
assembler output:

```
  tokenize -> parse -> AST
                        |
                        |  src/irgen.c
                        v
              linear IR + control flow graph        (locals still in memory)
                        |
                        |  src/ssa.c   ssa_construct()
                        v
                     SSA form                       (values in vregs, phi nodes)
                        |
                        |  src/opt.c   ssa_optimize()
                        v
                  optimized SSA
                        |
                        |  src/ssa.c   ssa_destruct()
                        v
              CFG of three address code             (phis turned into copies)
                        |
                        |  src/regalloc.c
                        v
              same code + a register or a stack slot for every vreg
                        |
                        |  src/emit_ir.c
                        v
                     x64 assembly
```

The old emitter is still there and reachable with `-fno-ssa`, which makes it
easy to tell a backend bug from a front end bug.

## Files

| file | role |
| --- | --- |
| `include/ir.h` | IR data types (`ir_t`, `bb_t`), opcode list, pipeline prototypes |
| `src/ir.c` | containers, instruction constructors, CFG edges, `-fdump-ssa` output |
| `src/irgen.c` | AST (`atom_t`) -> IR |
| `src/ssa.c` | dominator tree, dominance frontiers, mem2reg, phi elimination |
| `src/opt.c` | constant folding/propagation, branch folding, copy propagation, DCE |
| `src/regalloc.c` | linear order, liveness, live intervals, linear scan |
| `src/emit_ir.c` | IR -> x64, SysV call ABI, prologue/epilogue, driver |
| `include/emit.h` | output primitives shared with the legacy emitter |

## Command line

```
rcc -S -o out.s in.c              # SSA backend (default)
rcc -S -fno-ssa -o out.s in.c     # legacy AST emitter
rcc -S -fdump-ssa -o out.s in.c   # dump the IR after each stage to stderr
```

## The IR

A function is a vector of basic blocks. A block holds a vector of
instructions and ends with exactly one terminator (`IR_JMP`, `IR_BR`,
`IR_RET`). Instructions operate on virtual registers - plain integers, `-1`
meaning "no value". Every vreg is written exactly once, which is what makes
the optimizations in `opt.c` single pass lookups instead of data flow
problems.

```c
typedef struct {
    int op;             // IR_ADD, IR_LOAD, ...
    int dst;            // defined vreg, -1 when the instruction defines nothing
    int a0, a1;         // operand vregs
    long imm;           // constant, frame offset, argument index
    int size;           // operand width in bytes: 1, 4 or 8
    char *name;         // IR_GLOBAL
    func *f;            // IR_CALL
    ir_arg_vec args;    // IR_CALL
    int_vec phi_val;    // IR_PHI - incoming values
    int_vec phi_bb;     //          and the block each one comes from
    int bb0, bb1;       // branch targets
    int token_pos;
} ir_t;
```

Phi operands are stored as explicit `(value, block)` pairs rather than
positionally against `bb->preds`. Nothing then has to keep two lists in the
same order while the CFG is being rewritten.

`size` mirrors what the legacy emitter did: it is the width of the machine
instruction, taken from `type_size()` of the atom. Keeping it explicit is
what allows byte comparisons (`cmpb`, needed by
`test/097-compare-char-vs-char`) to stay byte comparisons.

## AST lowering (`src/irgen.c`)

`irgen()` is a recursive walk that returns the vreg holding the value of an
atom, or `-1` for a statement. Local variables are *not* resolved to values
here: `TYPE_VAR_REF` becomes `IR_LOCAL` (the address of a frame slot) and
reads and writes become `IR_LOAD` / `IR_STORE`. Turning that memory into
values is `ssa.c`'s job, which keeps the lowering simple and free of
scoping rules.

Control flow is built structurally. `if`, `for`, `while`, `do`, `switch`,
`break` and `continue` allocate their blocks up front and are wired
together; `break`/`continue` targets live on a stack. Code that follows an
unconditional terminator is collected into a fresh unreachable block, so the
"one terminator per block" invariant always holds and no special case is
needed for `return` in the middle of a body.

The three expressions that merge a value from two paths - `?:`, `&&` and
`||` - emit an `IR_PHI` directly, because the exact predecessors are known
at that point.

At function entry the incoming argument registers are stored into the slots
the parser assigned (`IR_ARG` followed by `IR_STORE`). Those stores are
ordinary IR, so mem2reg promotes arguments exactly like any other local.

Two behaviours are deliberately kept identical to the legacy emitter:
arguments are evaluated right to left, and `&&` / `||` yield the raw operand
value rather than a normalized 0/1.

## SSA construction (`src/ssa.c`)

### Dominators

`ssa_compute_dominators()` runs a depth first search to get a post order,
numbers the blocks in reverse post order, then iterates the
Cooper/Harvey/Kennedy fixed point:

```
idom[entry] = entry
repeat until nothing changes:
    for each block b in reverse post order, except the entry:
        new = the first processed predecessor
        for every other processed predecessor p: new = intersect(p, new)
        idom[b] = new
```

`intersect` walks two idom chains upwards comparing reverse post order
numbers. Dominance frontiers follow directly: for every block with several
predecessors, walk from each predecessor up the dominator tree until
`idom[b]` is reached, adding `b` to the frontier of each block on the way.

### mem2reg

A frame slot becomes an SSA value when nothing can observe it as memory.
`ssa_collect_vars()` keys candidates by frame offset and rejects a slot when

* its address is used for anything but a direct `IR_LOAD` / `IR_STORE`
  (an `&x`, an array index, a struct member, a call argument),
* it is accessed with different widths (`char c; c += 1;` stores four bytes
  into a one byte slot),
* it lies above `%rbp`, i.e. it is an argument the caller put on the stack
  and that no instruction of this function ever wrote,
* it is one word of a struct passed in registers, which overlaps the struct
  it belongs to. `irgen` marks those with `size == 0` on the `IR_LOCAL`.

Phi placement is the textbook iterated dominance frontier of the blocks that
assign the variable. Renaming is a walk over the dominator tree with one
stack of reaching definitions per variable:

* an `IR_LOAD` becomes `IR_MOV` from the top of the stack,
* a store pushes a new definition,
* on the way out, the phis of every successor get an operand for this block.

A read that no write dominates - a variable used before it is assigned -
takes an `IR_CONST 0` inserted once at the top of the entry block, so the
stacks are never empty.

#### Why a store is modelled as a zero extension

Storing `s` bytes and loading them back yields the value zero extended from
`s` bytes, because rcc loads a `char` with `movzbl` and a 4 byte value with
`movl`. If a promoted variable simply carried the stored register, a `char`
holding 300 would keep reading back as 300 instead of 44. `ssa_insert_defs()`
therefore puts an `IR_ZEXT` of the stored value right after each store, and
that extension - not the raw value - is the SSA definition. Constant folding
removes it whenever the value is known.

#### Why the stores usually stay

Dropping the store as well is only safe if nothing can reach the slot through
a pointer. rcc cannot prove that, and `test/008-pointer-addsub` deliberately
walks off the end of one local into its neighbour:

```c
int i0; int i1; int *ip;
i0 = 100; i1 = 200; ip = &i1;
print(*(ip+1));         // expects 100
```

So `ssa.c` tracks one bit per function, `ssa_frame_escapes`: it is set as
soon as the address of *any* slot of the frame is used for something other
than a direct load or store. When it is set, the stores to promoted
variables are kept and memory stays up to date; the loads still become SSA
values, which is where most of the benefit is. When no address escapes -
plain scalar code - the stores go away and the variable lives only in
registers.

### Phi elimination

`ssa_destruct()` first splits critical edges: an edge from a block with
several successors into a block with several predecessors has nowhere to put
a copy, so an empty block is inserted in the middle. Each phi then becomes a
`IR_MOV` appended before the terminator of every incoming block.

All phis of a block conceptually read their operands at the same instant.
When one phi of a block reads the result of another phi of the same block -
two variables swapping inside a loop - sequential copies would be wrong, so
the copies go through temporaries. That is detected per block and only paid
for where it is needed.

## Optimizations (`src/opt.c`)

`ssa_optimize()` runs up to eight rounds of:

* **constant folding** of `IR_MOV`, `IR_ZEXT`, `IR_SEXT`, `IR_LOGNOT`,
  `IR_BITNOT`, comparisons and arithmetic. Byte wide arithmetic is left
  alone, because `addb` keeps the upper bits of the register and folding it
  would not reproduce that. Division by zero and out of range shifts are not
  folded either. A phi whose operands are all the same value becomes a copy.
* **branch folding**: `IR_BR` on a known value becomes `IR_JMP`. The CFG
  changed, so edges, dominators and phi operands are rebuilt afterwards -
  `opt_prune_phi_entries()` drops operands whose block is no longer a
  predecessor, otherwise phi elimination would put a copy on a path that no
  longer reaches the phi.
* **copy propagation**: every reader of `v = mov w` is rewritten to read `w`
  directly, chasing chains in one pass.
* **dead code elimination**: an instruction with a result nobody reads and no
  side effect is removed; everything in an unreachable block is removed.

## Register allocation (`src/regalloc.c`)

Blocks are laid out in reverse post order and every instruction gets a
position in that linear order. Liveness is a backward data flow iteration on
bit sets of 64 vregs per `long`:

```
live_out[b] = union of live_in[s] for each successor s
live_in[b]  = use[b] union (live_out[b] minus def[b])
```

A live interval is then the smallest range covering every position at which
the vreg is live: the start of a block where it is live-in, the end of a
block where it is live-out, and every position that defines or uses it.
Deriving intervals from real liveness rather than from "first definition to
last use" is what makes them correct around loops, where a value defined in
the body is used by a phi copy earlier in the linear order.

The scan walks positions in order, frees registers whose interval ended, and
gives each interval starting there a register:

* `%rbx %r12 %r13 %r14 %r15` are callee saved and are the only choice for a
  value that is live across a call. The function saves the ones it uses into
  its own frame.
* `%r10` is caller saved and is used for values that do not cross a call.
* anything left over is spilled to a slot of the frame and moved through the
  scratch registers at each use.

A prefix sum over the call positions answers "does this interval cross a
call" in constant time.

## Code generation (`src/emit_ir.c`)

Register roles inside a function body:

| register | role |
| --- | --- |
| `%rbx %r12-%r15` | allocatable, callee saved |
| `%r10` | allocatable, caller saved |
| `%r11`, `%rcx` | scratch for a spilled first / second operand |
| `%rax` | scratch for the result, and temporary for block copies |
| `%rdx` | scratch, also clobbered by `idiv` |
| `%rdi %rsi %rdx %rcx %r8 %r9` | argument registers, only written while making a call |

`ei_operand()` returns the register a vreg lives in, emitting a load from its
slot when it is spilled; `ei_writeback()` does the reverse. Because argument
registers are never allocatable, filling them at a call can never destroy a
value that is still needed.

`%rsp` is only moved inside a call sequence, and always by a multiple of 16,
so it is 16 byte aligned at every `call` without any bookkeeping. Structs
passed in registers are copied to a staging area on the stack first, so that
reading a whole 8 byte word of a 12 byte struct never touches memory outside
the object. Structs passed on the stack are copied byte-exactly.

Blocks are emitted in the allocator's linear order, and a jump to the block
that physically follows is dropped.

Two places differ from the legacy emitter on purpose:

* branches test the full 64 bit register (`testq`) instead of `orl` on the
  low half. `orl` also *clears* the upper half of its operand, which
  corrupts a value that is still live, and it misses a pointer whose low 32
  bits happen to be zero.
* `char` division sign extends both operands into 32 bits and uses `idivl`.
  The legacy emitter emitted a literal `???` for that case.

## Writing code in this backend

The backend has to compile with rcc itself (`make test-gen2`,
`make test-gen3`), so it lives inside the C subset rcc supports. Things that
look fine to gcc but are miscompiled or rejected by rcc:

* `(long)some_int` does **not** sign extend - the cast only widens the type.
  Assign through a `long` variable instead, because assignment does convert.
* function arguments are not converted to the parameter type either. Passing
  an `int` to a `long` parameter passes it zero extended.
* `~x` on a `long` produces an `int` typed `notl`, which only flips the low
  32 bits. Use `0 - x - 1`.
* assigning the result of a comparison between two `long`s to a `bool` is
  rejected; the comparison takes the type of its left operand and rcc has no
  conversion from `long` to an enum. Use an `int`.
* a global pointer must not be initialized to `0`: for `char *` the
  initializer is read as a string literal index. Leave it uninitialized, it
  lands in `.comm` and is zeroed.
* prefer `vec_extend(v, 1)` over `vec_push(v, value)` for big structs: push
  passes the struct by value.

## Result

All 98 tests of `test/` pass with the SSA backend, and rcc still reproduces
itself through three generations, with gen2 and gen3 emitting identical
assembly.

Instruction counts of the generated assembly, SSA backend against `-fno-ssa`:

| source | SSA | legacy |
| --- | --- | --- |
| `src/parse.c` | 7823 | 10556 |
| `src/token.c` | 5271 | 6424 |
| `src/type.c` | 3806 | 4174 |

## Possible next steps

* drop the stores of promoted variables once an aliasing analysis can prove
  no pointer reaches the frame, instead of the current per function
  `ssa_frame_escapes` bit
* global value numbering, and moving loop invariant code out of loops
* interval splitting in the allocator, and coalescing the copies that phi
  elimination introduces
* the parser still assigns frame offsets, so `IR_LOCAL` inherits the ABI
  decisions made during parsing. Doing that here would let the backend place
  variables freely (see `Todo.md`).
