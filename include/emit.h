/*
 * emit.h - x64 output primitives shared by the legacy emitter (emit.c) and
 * the SSA backend (emit_ir.c).
 *
 * Requires: type.h, var.h
 */

typedef enum reg {
    R_DI, R_SI, R_DX, R_CX, R_8, R_9, R_AX, R_BX, R_10, R_11, R_12, R_13, R_14, R_15, R_LAST
} reg_e;

extern int output_fd;

void genf(char *fmt, ...);
void gen(char *str);
void gen_label(char *str);
int new_label();
char *reg(int no, int size);
char *opsize(int size);

void emit_string(char *str);
void emit_global_label(int i);
void emit_global_constant(var_t *v);
void emit_global_declaration(var_t *v);
