#include "rsys.h"
#include "rstring.h"
#include "devtool.h"
#include "types.h"

#include "token.h"

#include "type.h"
#include "var.h"
#include "func.h"
#include "atom.h"
#include "gstr.h"

#include "parse.h"

void _write(char *s) {
    write(1, s, strlen(s));
}

void out_comment(char *str) {
    _write("# ");
    _write(str);
    _write("\n");
}

void out_label(char *str) {
    _write(str);
    _write(":\n");
}

void out(char *str) {
    _write("\t");
    _write(str);
    _write("\n");
}

void out_x(char *fmt, int size) {
    if (size != 8 && size != 4 && size != 1) {
        debug_s(" ", fmt);
        error_i("unknown size:", size);
    }
    char buf[RCC_BUF_SIZE] = {0};
    char *d = &buf[0];
    while (*fmt) {
        switch (*fmt) {
            case 'X':
                *d = (size == 8) ? 'q' : (size == 4)? 'l' : 'b';
                break;
            case 'Z':
                *d = (size == 8) ? 'r' : (size == 4)? 'e' : 'B';
                break;
            default:
                *d = *fmt;
        }
        if (*d == 'B') {
            char c1 = *(fmt+1);
            char c2 = *(fmt+2);
            if ((c1 == 'd' || c1 == 's') && c2 == 'i') {
                *d = c1; *(d+1) = 'i'; *(d+2) = 'l'; // '%Zdi' -> '%dil' for Zdi, Zsi
            } else if ((c1 == 'a' || c1 == 'b' || c1 == 'c' || c1 == 'd') && c2 == 'x') {
                *d = c1; *(d+1) = 'l'; *(d+2) = ' '; // '%Zax' -> '%al ' for Zax, Zbx, Zcx, Zdx
            } else {
                error_s("unknown register name: ", fmt);
            }
            fmt += 2;
            d += 2;
        }
        fmt++;
        d++;
    }
    *d = 0;
    out(buf);
}

void out_int(char *str1, int i, char *str2) {
    char buf[RCC_BUF_SIZE] = {0};
    _strcat3(buf, str1, i, str2);
    out(buf);
}

void out_int4(char *str1, char *str2, char *str3, int i, char *str4) {
    char buf[RCC_BUF_SIZE] = {0};
    strcat(buf, str1);
    strcat(buf, str2);
    _strcat3(buf, str3, i, str4);
    out(buf);
}

void out_intx(char *str1, char *str2, int i, char *str3, int size) {
    char buf[RCC_BUF_SIZE] = {0};
    strcat(buf, str1);
    _strcat3(buf, str2, i, str3);
    out_x(buf, size);
}

void out_str(char *str1, char *str2, char *str3) {
    char buf[RCC_BUF_SIZE] = {0};
    strcat(buf, str1);
    strcat(buf, str2);
    strcat(buf, str3);
    out(buf);
}

void out_strx(char *str1, char *str2, char *str3, char *str4, int size) {
    char buf[RCC_BUF_SIZE] = {0};
    strcat(buf, str1);
    strcat(buf, str2);
    strcat(buf, str3);
    strcat(buf, str4);
    out_x(buf, size);
}

int label_index = 0;
int new_label() {
    return label_index++;
}

void emit_int(int i, int size) {
    if (size == 8) {
        out_int("movq	$", i, ", %rax");
    } else {
        out_int("movl	$", i, ", %eax");
    }
    out("pushq	%rax");
}

void emit_string(char* str) {
    char buf[RCC_BUF_SIZE] = {0};
    char *d;

    strcat(buf, ".string\t");
    d = buf + strlen(buf);
    *d++ = '"';
    while (*str) {
        switch(*str) {
            case '\n': *d++='\\'; *d = 'n'; break;
            case '\r': *d++='\\'; *d = 'r'; break;
            case '\t': *d++='\\'; *d = 't'; break;
            case '\f': *d++='\\'; *d = 'f'; break;
            case '\a': *d++='\\'; *d = 'a'; break;
            case '\b': *d++='\\'; *d = 'b'; break;
            case '\e': *d++='\\'; *d = 'e'; break;
            case '\"': *d++='\\'; *d = '"'; break;
            case '\'': *d++='\\'; *d = '\''; break;
            case '\\': *d++='\\'; *d = '\\'; break;
            default: *d = *str;
        }
        str++;
        d++;
    }
    *d++ = '"';
    *d = 0;
    out(buf);
}

void emit_global_ref(int i) {
    out_int("leaq	.G", i, "(%rip), %rax");
    out("pushq	%rax");
}

void emit_var_val(int i, int size) {
    if (size == 1) {
        out_int("movzbl	-", i, "(%rbp), %eax");
    } else {
        out_intx("movX	", "-", i, "(%rbp), %Zax", size);
    }
    out("pushq	%rax");
}

void emit_global_var_val(char *name, int size) {
    if (size == 1) {
        out_str("movzbl	", name, "(%rip), %eax");
    } else {
        out_strx("movX	", "", name, "(%rip), %Zax", size);
    }
    out("pushq	%rax");
}

char *reg(int no, int size) {
    char *regs8[] = { "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9" };
    char *regs4[] = { "%edi", "%esi", "%edx", "%ecx", "%r8d", "%r9d" };
    char *regs1[] = { "%dil", "%sil", "%dl", "%cl", "%r8b", "%r9b" };
    return size == 8 ? regs8[no] : size == 4 ? regs4[no] : regs1[no];
}

void emit_var_arg_init(int no, int offset, int size) {
    char buf[RCC_BUF_SIZE] = {0};
    strcat(buf, size == 8 ? "movq" : size == 4 ? "movl" : "movzbl");
    strcat(buf, "\t");
    strcat(buf, reg(no, (size == 1) ? 4 : size));
    strcat(buf, ", ");
    out_int(buf, -offset, "(%rbp)");
}

void emit_pop_argv(int no) {
    out_str("popq	", reg(no, 8), "");
}

void emit_call(char *name) {
    out("movb $0, %al");
    out_str("call	", name, "");
    out("pushq	%rax");
}

void emit_plt_call(char *name) {
    out("movb $0, %al");
    out_str("call	", name, "@PLT");
    out("pushq	%rax");
}

void emit_deref(int size) {
    out("popq	%rax");
    if (size == 1) {
        out("movzbl	(%rax), %eax");
    } else {
        out_x("movX	(%rax), %Zax", size);
    }
    out("pushq	%rax");
}

void emit_var_ref(int i) {
    out_int("lea	-", i, "(%rbp), %rax");
    out("pushq	%rax");
}

void emit_global_var_ref(char *name) {
    out_str("lea	", name, "(%rip), %rax");
    out("pushq	%rax");
}

void emit_postfix_add(int size, int ptr_size) {
    out("popq	%rax");
    out_x("movX	(%rax), %Zdx", size);
    out("pushq	%rdx");
    out_intx("add",  "X	$", ptr_size, ", %Zdx", size);
    out_x("movX	%Zdx, (%rax)", size);
}

void emit_copy(int size) {
    out("popq	%rax");
    out("popq	%rdx");
    if (size == 1) {
        out("movb	%dl, (%rax)");
    } else {
        out_x("movX	%Zdx, (%rax)", size);
    }
    out("pushq	%rdx");
}

void emit_pop() {
    out("popq	%rax");
    out("");
}

void emit_zcast(int size) {
    if (size == 8) {
        return;
    }
    out("popq	%rax");
    if (size == 1) {
        out("movzbq	%al, %rax");
    } else if (size == 4) {
        out("movzlq	%eax, %rax");
    } else {
        error("invalid size for emit_zcast");
    }
    out("pushq %rax");
}

void emit_scast(int size) {
    if (size == 8) {
        return;
    }
    out("popq	%rax");
    if (size == 1) {
        out("movsbq	%al, %rax");
    } else if (size == 4) {
        out("movslq	%eax, %rax");
    } else {
        error("invalid size for emit_scast");
    }
    out("pushq %rax");
}

void emit_binop(char *op, int size) {
    out("popq	%rdx");
    out("popq	%rax");
    out_strx(op, "X", "	%Zdx, %Zax", "", size);
    out("pushq	%rax");
}

void emit_add(int size) {
    emit_binop("add", size);
}

void emit_sub(int size) {
    emit_binop("sub", size);
}

void emit_bit_and(int size) {
    emit_binop("and", size);
}

void emit_bit_or(int size) {
    emit_binop("or", size);
}

void emit_bit_xor(int size) {
    emit_binop("xor", size);
}

void emit_mul(int size) {
    out("popq	%rdx");
    out("popq	%rax");
    out_x("imulX	%Zdx", size);
    out("pushq	%rax");
}

void emit_div(int size) {
    out("popq	%rcx");
    out("popq	%rax");
    out("cdq");
    out_x("idivX	%Zcx", size);
    out("pushq	%rax");
}

void emit_mod(int size) {
    out("popq	%rcx");
    out("popq	%rax");
    out("cdq");
    out_x("idivX	%Zcx", size);
    out("pushq	%rdx");
}

void emit_eq_x(char *set, int size) {
    out("popq	%rdx");
    out("popq	%rcx");
    out("xorl   %eax, %eax");
    out_x("subX	%Zdx, %Zcx", size);
    out(set);
    out("pushq	%rax");
}

void emit_eq_eq(int size) {
    emit_eq_x("sete %al", size);
}

void emit_eq_ne(int size) {
    emit_eq_x("setne %al", size);
}

void emit_eq_lt(int size) {
    emit_eq_x("setnge %al", size);
}

void emit_eq_le(int size) {
    emit_eq_x("setle %al", size);
}

void emit_eq_gt(int size) {
    emit_eq_x("setnle %al", size);
}

void emit_eq_ge(int size) {
    emit_eq_x("setge %al", size);
}

void emit_log_or(int size) {
    out("popq	%rdx");
    out("popq	%rax");
    out_x("orX	%Zdx, %Zax", size);
    out("pushq	%rax");
}

void emit_log_and(int size) {
    out("popq	%rdx");
    out("popq	%rax");
    out_x("andX	%Zdx, %Zax", size);
    out("pushq	%rax");
}

void emit_log_not(int size) {
    out("popq	%rdx");
    out("xorl   %eax, %eax");
    out_x("orX	%Zdx, %Zdx", size);
    out("setz %al");
    out("pushq	%rax");
}

void emit_print() {
    out("popq	%rax");
	out("movl	%eax, %esi");
	out("leaq	.LC0(%rip), %rdi");
	out("movl	$0, %eax");
	out("call	printf@PLT");
	out("movl	$0, %eax");
}

void emit_label(int i) {
    char buf[RCC_BUF_SIZE] = {0};
    _strcat3(buf, ".L", i, "");
    out_label(buf);
}

void emit_global_label(int i) {
    char buf[RCC_BUF_SIZE] = {0};
    _strcat3(buf, ".G", i, "");
    out_label(buf);
}

void emit_jmp(int i) {
    out_int("jmp .L", i, "");
}

void emit_jmp_false(int i) {
    out("popq	%rax");
    out("orl	%eax, %eax");
    out_int("jz	.L", i, "");
}

void emit_jmp_true(int i) {
    out("popq	%rax");
    out("orl	%eax, %eax");
    out_int("jnz	.L", i, "");
}

void emit_jmp_case(int i, int size) {
    out("popq	%rax");
    out("popq   %rcx");
    out("pushq   %rcx");
    out_x("subX	%Zcx, %Zax", size);
    out_int("jz	.L", i, "");
}


int func_return_label;

#define NUM_BREAK_LABELS 1000

struct {
    int break_label;
    int continue_label;
} break_labels[NUM_BREAK_LABELS];

int break_label_top = -1;

int get_break_label() {
    if (break_label_top < 0) {
        error("cannot emit break");
    }
    return break_labels[break_label_top].break_label;
}
int get_continue_label() {
    if (break_label_top < 0) {
        error("cannot emit break");
    }
    return break_labels[break_label_top].continue_label;
}
void enter_break_label(int break_label, int continue_label) {
    if (break_label_top >= NUM_BREAK_LABELS) {
        error("too many break label");
    }
    break_label_top++;
    break_labels[break_label_top].break_label = break_label;
    break_labels[break_label_top].continue_label = continue_label;
}
void exit_break_label() {
    if (break_label_top < 0) {
        error("cannot exit break label");
    }
    break_label_top--;
}


void compile(int pos) {
    atom_t *p = &(program[pos]);

    char ast_text[RCC_BUF_SIZE] = {0};
    dump_atom3(ast_text, p, 0, pos);
    debug_s("compiling atom_t: ", ast_text);

    switch (p->type) {
        case TYPE_VAR_REF:
            emit_var_ref(p->int_value);
            break;

        case TYPE_GLOBAL_VAR_REF:
            emit_global_var_ref(p->ptr_value);
            break;

        case TYPE_BIND:
            compile(p->atom_pos); // rvalue
            compile((p+1)->atom_pos); // lvalue - should be an address
            emit_copy(p->t->size);
            break;
        case TYPE_PTR:
        case TYPE_PTR_DEREF:
            compile(p->atom_pos);
            break;
        case TYPE_RVALUE:
            compile(p->atom_pos);
            if (p->t->array_length >= 0 || p->t->struct_of) {
                // rvalue of array / struct will be a pointer for itself
            } else {
                emit_deref(p->t->size);
            }
            break;

        case TYPE_CONVERT:
            compile(p->atom_pos);
            break;

        case TYPE_CAST:
            compile(p->atom_pos);
            emit_scast(p->t->size);
            break;

        case TYPE_INTEGER: 
            emit_int(p->int_value, p->t->size);
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
        case TYPE_LOG_OR:
        case TYPE_LOG_AND:
        case TYPE_OR:
        case TYPE_AND:
        case TYPE_XOR:
        case TYPE_ARRAY_INDEX:
        case TYPE_MEMBER_OFFSET:
            compile(p->atom_pos);
            compile((p+1)->atom_pos);
            switch (p->type) {
                case TYPE_ARRAY_INDEX:
                case TYPE_MEMBER_OFFSET:
                case TYPE_ADD: emit_add(p->t->size); break;
                case TYPE_SUB: emit_sub(p->t->size); break;
                case TYPE_DIV: emit_div(p->t->size); break;
                case TYPE_MOD: emit_mod(p->t->size); break;
                case TYPE_MUL: emit_mul(p->t->size); break;
                case TYPE_EQ_EQ: emit_eq_eq(p->t->size); break;
                case TYPE_EQ_NE: emit_eq_ne(p->t->size); break;
                case TYPE_EQ_LE: emit_eq_le(p->t->size); break;
                case TYPE_EQ_LT: emit_eq_lt(p->t->size); break;
                case TYPE_EQ_GE: emit_eq_ge(p->t->size); break;
                case TYPE_EQ_GT: emit_eq_gt(p->t->size); break;
                case TYPE_LOG_OR: emit_log_or(p->t->size); break;
                case TYPE_LOG_AND: emit_log_and(p->t->size); break;
                case TYPE_OR: emit_bit_or(p->t->size); break;
                case TYPE_AND: emit_bit_and(p->t->size); break;
                case TYPE_XOR: emit_bit_xor(p->t->size); break;
            }
            break;

        case TYPE_POSTFIX_DEC: {
            compile(p->atom_pos);
            type_t *target_t = p->t;
            emit_postfix_add(target_t->size, (target_t->ptr_to) ? -(target_t->ptr_to->size) : -1);
            break;
        }
        case TYPE_POSTFIX_INC:  {
            compile(p->atom_pos);
            type_t *target_t = p->t;
            emit_postfix_add(target_t->size, (target_t->ptr_to) ? target_t->ptr_to->size : 1);
            break;
        }
        case TYPE_NOP:
            break;

        case TYPE_EXPR_STATEMENT:
            compile(p->atom_pos);
            emit_pop();
            break;
        case TYPE_ANDTHEN:
            compile(p->atom_pos);
            compile((p+1)->atom_pos);
            break;

        case TYPE_PRINT:
            compile(p->atom_pos);
            emit_print();
            break;
            
        case TYPE_LOG_NOT:
            compile(p->atom_pos);
            emit_log_not(p->t->size);
            break;

        case TYPE_TERNARY: {
            int l_end = new_label();
            int l_else = new_label();
            compile(p->atom_pos);
            emit_jmp_false(l_else);
            compile((p+1)->atom_pos);
            emit_jmp(l_end);
            emit_label(l_else);
            compile((p+2)->atom_pos);
            emit_label(l_end);
        } 
            break;

        case TYPE_IF: {
            bool has_else = ((p+2)->atom_pos != 0);
            int l_end = new_label();
            int l_else = new_label();

            compile(p->atom_pos);
            emit_jmp_false(has_else ? l_else : l_end);
            compile((p+1)->atom_pos);

            if (has_else) {
                emit_jmp(l_end);
                emit_label(l_else);
                compile((p+2)->atom_pos);
            }
            emit_label(l_end);
        }
            break;

        case TYPE_FOR: {
            int l_body = new_label();
            int l_loop = new_label();
            int l_end = new_label();
            enter_break_label(l_end, l_loop);

            compile((p+2)->atom_pos);
            emit_label(l_body);
            compile((p+1)->atom_pos);
            emit_jmp_false(l_end);

            compile(p->atom_pos);

            emit_label(l_loop);
            compile((p+3)->atom_pos);
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
            compile((p+1)->atom_pos);
            emit_jmp_false(l_end);
            compile(p->atom_pos);
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
            compile(p->atom_pos);
            compile((p+1)->atom_pos);
            emit_jmp_true(l_body);
            emit_label(l_end);

            exit_break_label();
        }
            break;

        case TYPE_RETURN:
            compile(p->atom_pos);
            emit_pop();
            emit_jmp(func_return_label);
            break;
        
        case TYPE_BREAK:
            emit_jmp(get_break_label());
            break;

        case TYPE_CONTINUE:
            emit_jmp(get_continue_label());
            break;

        case TYPE_APPLY: {
            func *f = (func *)(p->ptr_value);
            for (int i=0; i<f->argc; i++) {
                compile((p+i+1)->atom_pos);
                emit_pop_argv(i);
            }
            if (f->is_external) {
                emit_plt_call(f->name);
            } else {
                emit_call(f->name);
            }
        }
            break;
        
        case TYPE_STRING:
            emit_global_ref(p->int_value);
            break;

        case TYPE_SWITCH: {
            atom_t *top_p = p;
            int l_table = new_label();
            int l_end = new_label();
            enter_break_label(l_end, 0);
            emit_jmp(l_table);

            int case_label[RCC_BUF_SIZE];
            int case_label_index = 0;
            for (p = top_p + 1; p->type == TYPE_ARG; p++) {
                int label = new_label();
                if (case_label_index >= 200) {
                    error("too much labels");
                }
                case_label[case_label_index++] = label;
                emit_label(label);
                atom_t *case_atom = &program[p->atom_pos];
                if (case_atom->type == TYPE_CASE) {
                    compile((case_atom+1)->atom_pos);
                } else if (case_atom->type == TYPE_DEFAULT) {
                    compile(case_atom->atom_pos);
                } else {
                    dump_atom_tree(p->atom_pos, 0);
                    error("invalid child under switch node");
                }
                compile((case_atom+1)->atom_pos);
            }
            emit_jmp(l_end);

            // jump table
            emit_label(l_table);
            compile(top_p->atom_pos);

            int i=0;
            for (p = top_p + 1; p->type == TYPE_ARG; p++) {
                atom_t *case_atom = &program[p->atom_pos];
                if (case_atom->type == TYPE_CASE) {
                    compile(case_atom->atom_pos);
                    emit_jmp_case(case_label[i], p->t->size);
                    i++;
                } else if (case_atom->type == TYPE_DEFAULT) {
                    compile(case_atom->atom_pos);
                } else {
                    dump_atom_tree(p->atom_pos, 0);
                    error("invalid child under switch node");
                }
            }
            emit_label(l_end);
            emit_pop();
        }
            break;


        default:
            dump_atom(pos, 0);
            error("Invalid program");
    }
    out_comment(ast_text);
    debug_s("compiled ", ast_text);
}

void compile_func(func *f) {
    if (!f->body_pos) {
        return;
    }
    func_return_label = new_label();

    out_str(".globl	", f->name, "");
    out_str(".type	", f->name, ", @function");
    out_label(f->name);
    out("pushq	%rbp");
    out("movq	%rsp, %rbp");
    out_int("subq	$", f->max_offset, ", %rsp");

    for (int i=0; i<f->argc; i++) {
        var_t *v = &(f->argv[i]);
        emit_var_arg_init(i, v->offset, v->t->size);
    }

    compile(f->body_pos);

    out("xor	%eax, %eax");
    emit_label(func_return_label);
    out("leave");
    out("ret");
    out("");
}

int out_global_constant_by_type(type_t *pt, int value) {
    int filled_size = 0;
    if (pt == find_type("char")) {
        out_int(".byte\t", value, "");
        filled_size += 1;
    } else if (pt == find_type("int")) {
        out_int(".long\t", value, "");
        filled_size += 4;
    } else if (pt == find_type("long")) {
        out_int(".quad\t", value, "");
        filled_size += 4;
    } else if (pt == add_pointer_type(find_type("char"))) {
        out_int(".quad\t.G", value, "");
        filled_size += 8;
    }
    return filled_size;
}

void out_global_constant(var_t *v) {
    out_str(".globl\t", v->name, "");
    out(".data");
    out_int(".align\t", 4, "");
    out_str(".type\t", v->name, ", @object");
    out_int4(".size\t", v->name, ", ", v->t->size, "");
    out_label(v->name);
    if (v->t->array_length >= 0) {
        int filled_size = 0;
        int pos = v->int_value;
        int len = get_global_array_length(pos);
        type_t *pt = v->t->ptr_to;
        for (int index = 0; index < len; index++) {
            int value = get_global_array(pos, index);
            filled_size += out_global_constant_by_type(pt, value);
        }
        if (v->t->size > filled_size) {
            out_int(".zero\t", v->t->size - filled_size, "");
        }
    } else {
        int filled_size = out_global_constant_by_type(v->t, v->int_value);
        if (!filled_size) {
            error_s("unknown size for global variable:", v->name);
        }
    }
    out("");
}

void out_global_declare(var_t *v) {
    char buf[RCC_BUF_SIZE];
    buf[0] = 0;
    strcat(buf, ".comm	");
    strcat(buf, v->name);
    _strcat3(buf, ", ", v->t->size, "");
    out(buf);
}

void emit() {
    out(".file	\"main.c\"");
    out("");

    for (int i=0; i<env[0].num_vars; i++) {
        var_t *v = &(env[0].vars[i]);
        if (v->is_constant) {
            continue;
        }
        if (v->has_value) {
            out_global_constant(v);
        } else if (!v->is_external) {
            out_global_declare(v);
        }
    }

    out(".text");
    out(".section	.rodata");
    out_label(".LC0");
    out(".string	\"%d\\n\"");

    int gstr_i=0;
    char *gstr;
    while ((gstr = find_global_string(gstr_i)) != 0) {
        emit_global_label(gstr_i);
        emit_string(gstr);
        gstr_i++;
    }
    out("");

    out(".text");
    out("");

    func *f = &functions[0];
    while (f->name != 0) {
        if (!f->is_external) {
            debug_s(f->name, " --------------------- ");
            dump_atom_tree(f->body_pos, 0);
            compile_func(f);
        }
        f++;
    }
}
