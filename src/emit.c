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
#include "gstr.h"

#include "parse.h"

int output_fd = 1;

void _write(char *s) {
    write(output_fd, s, strlen(s));
}

void genf(char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    char buf[RCC_BUF_SIZE];
    vsnprintf(buf, RCC_BUF_SIZE, fmt, va);
    va_end(va);
    _write(buf);
    _write("\n");
}

void gen_label(char *str) {
    genf("%s:", str);
}

void gen(char *str) {
    genf("\t%s", str);
}


int label_index = 0;
int new_label() {
    return label_index++;
}

void emit_string(char* str) {
    char buf[RCC_BUF_SIZE] = {0};

    strcat(buf, ".string \"");
    escape_string(buf + strlen(buf), str);
    strcat(buf, "\"");
    gen(buf);
}

void emit_label(int i) {
    genf(".L%d:", i);
}

void emit_global_label(int i) {
    genf(".G%d:", i);
}

int func_return_label;
int func_void_return_label;

typedef struct {
    int break_label;
    int continue_label;
} break_label_t;

VEC_HEADER(break_label_t, break_label_vec)
VEC_BODY(break_label_t, break_label_vec)

break_label_vec break_labels = 0;

int get_break_label() {
    if (!break_label_vec_len(break_labels)) {
        error("cannot emit break");
    }
    return break_label_vec_top(break_labels)->break_label;
}
int get_continue_label() {
    if (!break_label_vec_len(break_labels)) {
        error("cannot emit break");
    }
    return break_label_vec_top(break_labels)->continue_label;
}
void enter_break_label(int break_label, int continue_label) {
    if (!break_labels) break_labels = break_label_vec_new();
    break_label_t b;
    b.break_label = break_label;
    b.continue_label = continue_label;
    break_label_vec_push(break_labels, b);
}
void exit_break_label() {
    if (!break_label_vec_len(break_labels)) {
        error("cannot exit break label");
    }
    break_label_vec_pop(break_labels);
}

#ifdef TARGET_ARM64
#include "emit_arm64.h"
#else 
#include "emit_x64.h"
#endif // TARGET_ARM64

void compilep(atom_t *p, reg_e reg_out);

void compile(int pos, reg_e reg_out) {
    atom_t *p = &(program[pos]);

#ifdef DEBUG
    char ast_text[RCC_BUF_SIZE] = {0};
    dump_atom3(ast_text, p, 0, pos);
    debug("compiling out:R#%d atom_t: %s", reg_out, ast_text);
#endif

    compilep(p, reg_out);

#ifdef DEBUG
    if (p->type == TYPE_EXPR_STATEMENT || p->type == TYPE_APPLY || p->type == TYPE_RETURN || p->type == TYPE_IF || p->type == TYPE_FOR || p->type == TYPE_WHILE || p->type == TYPE_DO_WHILE) {
        genf("# %s", ast_text);
    }
    debug("compiled out:R#%d atom_t: %s", reg_out, ast_text);
#endif
}

void compilep(atom_t *p, reg_e reg_out) {

    set_token_pos(p->token_pos);
    //dump_token_by_id(p->token_pos);

    switch (p->type) {
        case TYPE_VAR_REF:
            emit_var_ref(p->int_value, reg_out);
            break;

        case TYPE_GLOBAL_VAR_REF:
            emit_global_var_ref(p->ptr_value, reg_out);
            break;

        case TYPE_BIND: {
            reg_e i1 = reg_assign();
            compile(p->atom_pos, reg_out); // rvalue
            compile((p+1)->atom_pos, i1); // lvalue - should be an address
            if (p->t->struct_of) {
                emit_copy(type_size(p->t), reg_out, i1);
            } else {
                emit_store(type_size(p->t), reg_out, i1);
            }
            reg_release(i1);
            break;
        }
        case TYPE_PTR:
        case TYPE_PTR_DEREF:
            compile(p->atom_pos, reg_out);
            break;
        case TYPE_RVALUE:
            compile(p->atom_pos, reg_out);
            if (p->t->array_length >= 0 || p->t->struct_of) {
                // rvalue of array / struct will be a pointer for itself
            } else {
                emit_deref(type_size(p->t), reg_out);
            }
            break;

        case TYPE_CONVERT: {
            compile(p->atom_pos, reg_out);
            int org_size = type_size(program[p->atom_pos].t);
            int new_size = type_size(p->t);
            if (!p->t->ptr_to && org_size < new_size) {
                emit_scast(type_size(program[p->atom_pos].t), reg_out);
            }
            break;
        }
        case TYPE_CAST:
            compile(p->atom_pos, reg_out);
            emit_scast(type_size(p->t), reg_out);
            break;

        case TYPE_INTEGER: 
            if (type_size(p->t) == 8) {
                emit_int(p->long_value, 8, reg_out);
            } else {
                emit_int((long)(p->int_value), type_size(p->t), reg_out);
            }
            break;

        case TYPE_ADD:
        case TYPE_SUB:
        case TYPE_DIV:
        case TYPE_MOD:
        case TYPE_MUL:
        case TYPE_EQ_EQ:
        case TYPE_EQ_NE:
        case TYPE_EQ_LT:
        case TYPE_EQ_LE:
        case TYPE_EQ_GT:
        case TYPE_EQ_GE:
        case TYPE_OR:
        case TYPE_AND:
        case TYPE_XOR:
        case TYPE_LSHIFT:
        case TYPE_RSHIFT:
        case TYPE_MEMBER_OFFSET:
        case TYPE_ARRAY_INDEX:
            compile(p->atom_pos, reg_out);
            reg_e i1 = reg_assign();
            compile((p+1)->atom_pos, i1);
            if (p->type == TYPE_ARRAY_INDEX) {
                emit_array_index((p+2)->int_value, i1, reg_out);
            } else {
                emit_binop_switch(p->type, type_size(p->t), i1, reg_out);
            }
            reg_release(i1);
            break;

        case TYPE_POSTFIX_DEC: {
            compile(p->atom_pos, reg_out);
            type_t *target_t = p->t;
            emit_postfix_add(type_size(target_t), (target_t->ptr_to) ? -type_size(target_t->ptr_to) : -1, reg_out);
            break;
        }
        case TYPE_POSTFIX_INC:  {
            compile(p->atom_pos, reg_out);
            type_t *target_t = p->t;
            emit_postfix_add(type_size(target_t), (target_t->ptr_to) ? type_size(target_t->ptr_to) : 1, reg_out);
            break;
        }
        
        case TYPE_NOP:
            break;

        case TYPE_EXPR_STATEMENT:
            compile(p->atom_pos, reg_out);
            break;

        case TYPE_ANDTHEN:
            compile(p->atom_pos, reg_out);
            compile((p+1)->atom_pos, reg_out);
            break;

        case TYPE_LOG_AND: {
                int l_end = new_label();
                compile(p->atom_pos, reg_out);
                emit_jmp_false(l_end, reg_out); // short circuit of '&&'
                compile((p+1)->atom_pos, reg_out);
                emit_label(l_end);
            }
            break;

        case TYPE_LOG_OR: {
                int l_end = new_label();
                compile(p->atom_pos, reg_out);
                emit_jmp_true(l_end, reg_out);   // short circuit of '||'
                compile((p+1)->atom_pos, reg_out);
                emit_label(l_end);
            }
            break;

        case TYPE_LOG_NOT:
            compile(p->atom_pos, reg_out);
            emit_log_not(type_size(p->t), reg_out);
            break;

        case TYPE_NEG:
            compile(p->atom_pos, reg_out);
            emit_neg(type_size(p->t), reg_out);
            break;

        case TYPE_TERNARY: {
            int l_end = new_label();
            int l_else = new_label();
            compile(p->atom_pos, reg_out);
            emit_jmp_false(l_else, reg_out);
            compile((p+1)->atom_pos, reg_out);
            emit_jmp(l_end);
            emit_label(l_else);
            compile((p+2)->atom_pos, reg_out);
            emit_label(l_end);
        } 
            break;

        case TYPE_IF: {
            bool has_else = ((p+2)->atom_pos != 0);
            int l_end = new_label();
            int l_else = new_label();

            compile(p->atom_pos, reg_out);
            emit_jmp_false(has_else ? l_else : l_end, reg_out);
            compile((p+1)->atom_pos, reg_out);

            if (has_else) {
                emit_jmp(l_end);
                emit_label(l_else);
                compile((p+2)->atom_pos, reg_out);
            }
            emit_label(l_end);
        }
            break;

        case TYPE_FOR: {
            int l_body = new_label();
            int l_loop = new_label();
            int l_end = new_label();
            enter_break_label(l_end, l_loop);

            compile((p+2)->atom_pos, reg_out);
            emit_label(l_body);
            compile((p+1)->atom_pos, reg_out);
            emit_jmp_false(l_end, reg_out);

            compile(p->atom_pos, reg_out);

            emit_label(l_loop);
            compile((p+3)->atom_pos, reg_out);
            emit_jmp(l_body);
            emit_label(l_end);

            exit_break_label();
        }
            break;
        case TYPE_WHILE:
        {
            int l_body = new_label();
            int l_end = new_label();
            enter_break_label(l_end, l_body);

            emit_label(l_body);
            compile((p+1)->atom_pos, reg_out);
            emit_jmp_false(l_end, reg_out);
            compile(p->atom_pos, reg_out);
            emit_jmp(l_body);
            emit_label(l_end);

            exit_break_label();
        }
            break;
        case TYPE_DO_WHILE:
        {
            int l_body = new_label();
            int l_end = new_label();
            enter_break_label(l_end, l_body);

            emit_label(l_body);
            compile(p->atom_pos, reg_out);
            compile((p+1)->atom_pos, reg_out);
            emit_jmp_true(l_body, reg_out);
            emit_label(l_end);

            exit_break_label();
        }
            break;

        case TYPE_RETURN:
            if (p->t != type_void) {
                compile(p->atom_pos, reg_out);
                emit_return(reg_out);
                emit_jmp(func_return_label);
            } else {
                emit_jmp(func_void_return_label);
            }
            break;
        
        case TYPE_BREAK:
            emit_jmp(get_break_label());
            break;

        case TYPE_CONTINUE:
            emit_jmp(get_continue_label());
            break;

        case TYPE_APPLY:
            compile_apply(p, reg_out);
            break;
        
        case TYPE_STRING:
            emit_global_ref(p->int_value, reg_out);
            break;

        case TYPE_SWITCH: {
            int l_end = new_label();
            enter_break_label(l_end, 0);
            compile(p->atom_pos, reg_out);
            int size = type_size(p->t);

            p++;
            int l_fallthrough = new_label();
            while (p->type == TYPE_ARG) {
                int l_next_case = new_label(); 
                atom_t *case_atom = &program[p->atom_pos];
                int pos;

                if (case_atom->type == TYPE_CASE) {
                    compile((case_atom  )->atom_pos, reg_tmp());
                    emit_jmp_ne(l_next_case, size, reg_out, reg_tmp()); 
                    pos = (case_atom+1)->atom_pos;
                } else if (case_atom->type == TYPE_DEFAULT) {
                    pos = case_atom->atom_pos;
                } else {
                    dump_atom_tree(p->atom_pos, 0);
                    error("invalid child under switch node");
                }

                emit_label(l_fallthrough);
                compile(pos, reg_out);
                l_fallthrough = new_label();    // points the body of the next case
                emit_jmp(l_fallthrough);

                emit_label(l_next_case);
                p++;
            }

            emit_label(l_fallthrough);
            exit_break_label();
            emit_label(l_end);
        }
            break;


        default:
            dump_atom2(p, 0, 0);
            error("Invalid program");
    }
}


int emit_global_constant_by_type(type_t *pt, int value) {
    int filled_size = 0;
    if (pt == type_char) {
        genf(".byte %d", value);
        filled_size += 1;
    } else if (pt == type_int) {
        genf(".long %d", value);
        filled_size += 4;
    } else if (pt == type_long) {
        genf(".quad %d", value);
        filled_size += 4;
    } else if (pt == type_char_ptr) {
        genf(".quad .G%d", value);
        filled_size += 8;
    } else if (pt->ptr_to) {
        genf(".quad %d", value);
        filled_size += 8;
    }
    return filled_size;
}

void emit_global_constant(var_t *v) {
    genf(".globl %s", v->name);
    genf(".data");
    genf(".align 4");
    genf(".type %s, @object", v->name);
    genf(".size %s, %d", v->name, type_size(v->t));
    gen_label(v->name);
    if (v->t->array_length >= 0) {
        int filled_size = 0;
        int pos = v->int_value;
        int len = get_global_array_length(pos);
        type_t *pt = v->t->ptr_to;
        for (int index = 0; index < len; index++) {
            int value = get_global_array(pos, index);
            filled_size += emit_global_constant_by_type(pt, value);
        }
        if (type_size(v->t) > filled_size) {
            genf(".zero %d", type_size(v->t) - filled_size);
        }
    } else {
        int filled_size = emit_global_constant_by_type(v->t, v->int_value);
        if (!filled_size) {
            error("unknown size for global variable:%s %s", v->name, dump_type(v->t));
        }
    }
    gen("");
}

void emit_global_declaration(var_t *v) {
    genf(".comm %s, %d", v->name, type_size(v->t));
}

void compile_file(int fd) {
    output_fd = fd;
    
    gen(".file \"main.c\"");
    gen("");

    var_vec vars = get_global_frame()->vars;
    for (int i=0; i<var_vec_len(vars); i++) {
        var_t *v = var_vec_get(vars, i);
        if (v->is_constant) {
            continue;
        }
        if (v->has_value) {
            emit_global_constant(v);
        } else if (!v->is_external) {
            emit_global_declaration(v);
        }
    }

    gen(".text");
    gen(".section .rodata");

    int gstr_i=0;
    char *gstr;
    while ((gstr = find_global_string(gstr_i)) != 0) {
        emit_global_label(gstr_i);
        emit_string(gstr);
        gstr_i++;
    }
    gen("");

    gen(".text");
    gen("");

    for (int i=0; i<func_vec_len(functions); i++) {
        func *f = func_vec_get(functions, i);
        if (f->body_pos != 0) {
            debug("%s --------------------- ", f->name);
            //dump_atom_tree(f->body_pos, 0);
            emit_function(f);
        }
    }
}
