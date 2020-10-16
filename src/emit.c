#include "types.h"
#include "rsys.h"
#include "rstring.h"
#include "devtool.h"

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

/**
 * Build a proper assembly instruction which suits for the specified size.
 * ex:
 *   movX Zedx, %Zax (size=4) --> movl %edx, %eax
 *   addX $2, %Zdi   (size=1) --> addb $2, %dil
 */
void out_x(char *fmt, int size) {
    if (size != 8 && size != 4 && size != 1) {
        error("fmt:%s unknown size:%d", fmt, size);
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
                error("unknown register name: %s", fmt);
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
    if (no >= 6) {
        error("invalid reg no:%d", no);
    }
    switch (size) {
        case 8: return regs8[no];
        case 4: return regs4[no];
        case 1: return regs1[no];
        default: error("invalid size for reg:%d", size); return (void *)0;
    }
}

void emit_var_arg_init(int no, int offset, int size) {
    char buf[RCC_BUF_SIZE] = {0};
    if (size == 1) {
        strcat(buf, "movzbl\t");
        strcat(buf, reg(no, 1));
        strcat(buf, ",\t");
        strcat(buf, reg(no, 4));
        out(buf);
        size = 4;
        buf[0] = '\0';
    }
    strcat(buf, size == 8 ? "movq" : "movl");
    strcat(buf, "\t");
    strcat(buf, reg(no, size));
    strcat(buf, ", ");
    out_int(buf, -offset, "(%rbp)");
}

void emit_pop_argv(int no) {
    out_str("popq	", reg(no, 8), "");
}

void emit_call(char *name, int num_stack_args, char *plt) {
    out("movb $0, %al");
    out_str("call	", name, plt);
    if (num_stack_args > 0) {
        out_int("addq $", num_stack_args * 8, ", %rsp");
    }
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
    out_int("leaq	", -i, "(%rbp), %rax");
    out("pushq	%rax");
}

void emit_global_var_ref(char *name) {
    out_str("leaq	", name, "(%rip), %rax");
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
    out("popq	%rdi");
    out("popq	%rsi");
    if (size > 8) { // for size > 8, %rdx is an address, not value
        while (size>=8) {
            out("movq (%rsi), %rax");
            out("movq %rax, (%rdi)");
            out("addq $8, %rsi");
            out("addq $8, %rdi");
            size-=8;
        }
        while (size>=4) {
            out("movl (%rsi), %ecx");
            out("movl %ecx, (%rdi)");
            out("addq $4, %rsi");
            out("addq $4, %rdi");
            size-=4;
        }
        while (size>0) {
            out("movb (%rsi), %al");
            out("movb %al, (%rdi)");
            out("incq %rsi");
            out("incq %rdi");
            size-=4;
        }
    } else {
        out_x("movX	%Zsi, (%rdi)", size);
    }
    out("pushq	%rsi");
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

void emit_array_index(int item_size) {
    out("popq	%rax");
    out_int("movl	$", item_size, ",%ecx");
    out("imulq %rcx");
    out("popq	%rdx");
    out("addq %rdx, %rax");
    out("pushq	%rax");
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

void emit_bit_lshift(int size) {
    out("popq	%rcx");
    out("popq	%rax");
    out_x("salX %cl, %Zax", size);
    out("pushq	%rax");
}

void emit_bit_rshift(int size) {
    out("popq	%rcx");
    out("popq	%rax");
    out_x("sarX %cl, %Zax", size);
    out("pushq	%rax");
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

void emit_neg(int size) {
    out("popq	%rax");
    out_x("notX	%Zax", size);
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

void emit_jmp_true_keepvalue(int i) {
    out("popq	%rax");
    out("pushq	%rax");
    out("orl	%eax, %eax");
    out_int("jnz	.L", i, "");
}

void emit_jmp_false_keepvalue(int i) {
    out("popq	%rax");
    out("pushq	%rax");
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

void emit_jmp_case_if_not(int i, int size) {
    out("popq	%rax");
    out("popq   %rcx");
    out("pushq   %rcx");
    out_x("subX	%Zcx, %Zax", size);
    out_int("jnz	.L", i, "");
}

int emit_push_struct(int size) {
    int offset = align(size, 8);
    out("popq %rsi"); // has address of the struct value
    out_int("addq $-", offset, ", %rsp");
    out("movq %rsp, %rdi");
    out("pushq %rsi");
    out("pushq %rdi");
    emit_copy(size);
    out("popq %rax"); // drop
    return offset / 8;
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
    debug("compiling atom_t: %s", ast_text);
    set_token_pos(p->token_pos);
    //dump_token_by_id(p->token_pos);

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
            emit_copy(type_size(p->t));
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
                emit_deref(type_size(p->t));
            }
            break;

        case TYPE_CONVERT:
            compile(p->atom_pos);
            break;

        case TYPE_CAST:
            compile(p->atom_pos);
            emit_scast(type_size(p->t));
            break;

        case TYPE_INTEGER: 
            emit_int(p->int_value, type_size(p->t));
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
            compile(p->atom_pos);
            compile((p+1)->atom_pos);
            switch (p->type) {
                case TYPE_MEMBER_OFFSET:
                case TYPE_ADD: emit_add(type_size(p->t)); break;
                case TYPE_SUB: emit_sub(type_size(p->t)); break;
                case TYPE_DIV: emit_div(type_size(p->t)); break;
                case TYPE_MOD: emit_mod(type_size(p->t)); break;
                case TYPE_MUL: emit_mul(type_size(p->t)); break;
                case TYPE_EQ_EQ: emit_eq_eq(type_size(p->t)); break;
                case TYPE_EQ_NE: emit_eq_ne(type_size(p->t)); break;
                case TYPE_EQ_LE: emit_eq_le(type_size(p->t)); break;
                case TYPE_EQ_LT: emit_eq_lt(type_size(p->t)); break;
                case TYPE_EQ_GE: emit_eq_ge(type_size(p->t)); break;
                case TYPE_EQ_GT: emit_eq_gt(type_size(p->t)); break;
                case TYPE_OR: emit_bit_or(type_size(p->t)); break;
                case TYPE_AND: emit_bit_and(type_size(p->t)); break;
                case TYPE_XOR: emit_bit_xor(type_size(p->t)); break;
                case TYPE_LSHIFT: emit_bit_lshift(type_size(p->t)); break;
                case TYPE_RSHIFT: emit_bit_rshift(type_size(p->t)); break;
            }
            break;

        case TYPE_ARRAY_INDEX:
            compile(p->atom_pos);
            compile((p+1)->atom_pos);
            emit_array_index((p+2)->int_value);
            break;

        case TYPE_POSTFIX_DEC: {
            compile(p->atom_pos);
            type_t *target_t = p->t;
            emit_postfix_add(type_size(target_t), (target_t->ptr_to) ? -type_size(target_t->ptr_to) : -1);
            break;
        }
        case TYPE_POSTFIX_INC:  {
            compile(p->atom_pos);
            type_t *target_t = p->t;
            emit_postfix_add(type_size(target_t), (target_t->ptr_to) ? type_size(target_t->ptr_to) : 1);
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

        case TYPE_LOG_AND: {
                compile(p->atom_pos);
                int l_end = new_label();
                emit_jmp_false_keepvalue(l_end); // short circuit of '&&'
                compile((p+1)->atom_pos);
                emit_label(l_end);
            }
            break;

        case TYPE_LOG_OR: {
                compile(p->atom_pos);
                int l_end = new_label();
                emit_jmp_true_keepvalue(l_end);   // short circuit of '||'
                compile((p+1)->atom_pos);
                emit_label(l_end);
            }
            break;

        case TYPE_PRINT:
            compile(p->atom_pos);
            emit_print();
            break;
            
        case TYPE_LOG_NOT:
            compile(p->atom_pos);
            emit_log_not(type_size(p->t));
            break;

        case TYPE_NEG:
            compile(p->atom_pos);
            emit_neg(type_size(p->t));
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
            dump_atom_tree(pos,0);
            func *f = (func *)(p->ptr_value);
            int argc = (p+1)->int_value;

            int num_reg_args = 0;
            int num_stack_args = 0;

            bool use_reg[100]; // NUM_ARGC
            int struct_size[100]; 
            for (int i=0; i<argc; i++) {
                type_t *t = program[(p+i+2)->atom_pos].t;
                if (t->struct_of) {
                    struct_size[i] = t->size;
                    use_reg[i] = (num_reg_args < 5 && t->size <= 16);
                    if (use_reg[i]) num_reg_args += (t->size <= 8) ? 1 : 2;
                } else  {
                    struct_size[i] = 0;
                    use_reg[i] = (num_reg_args < ABI_NUM_GP);
                    if (use_reg[i]) num_reg_args++;
                }
            }

            // push for stack-passing
            for (int i=argc-1; i>=0; i--) {
                if (use_reg[i]) continue;
                debug("compiling stack passing values %d", i);
                compile((p+i+2)->atom_pos);
                if (struct_size[i] > 0) {
                    num_stack_args += emit_push_struct(struct_size[i]);
                } else {
                    num_stack_args++;
                }
            }

            // push for register-passing
            for (int i=argc-1; i>=0; i--) {
                if (!use_reg[i]) continue;
                debug("compiling register passing values %d", i);
                compile((p+i+2)->atom_pos);
                if (struct_size[i] > 0) {
                    emit_push_struct(struct_size[i]);
                }
            }

            for (int i=0; i<num_reg_args; i++) {
                emit_pop_argv(i);
            }
            emit_call(f->name, num_stack_args, f->is_external ? "@PLT" : "");
        }
            break;
        
        case TYPE_STRING:
            emit_global_ref(p->int_value);
            break;

        case TYPE_SWITCH: {
            int l_end = new_label();
            enter_break_label(l_end, 0);
            compile(p->atom_pos);
            int size = type_size(p->t);

            p++;
            int l_fallthrough = new_label();
            while (p->type == TYPE_ARG) {
                int l_next_case = new_label(); 
                atom_t *case_atom = &program[p->atom_pos];
                int pos;

                if (case_atom->type == TYPE_CASE) {
                    compile((case_atom  )->atom_pos);
                    emit_jmp_case_if_not(l_next_case, size);
                    pos = (case_atom+1)->atom_pos;
                } else if (case_atom->type == TYPE_DEFAULT) {
                    emit_label(l_fallthrough);
                    pos = case_atom->atom_pos;
                } else {
                    dump_atom_tree(p->atom_pos, 0);
                    error("invalid child under switch node");
                }

                emit_label(l_fallthrough);
                compile(pos);
                l_fallthrough = new_label();    // points to the body of the next case
                emit_jmp(l_fallthrough);

                emit_label(l_next_case);
                p++;
            }

            emit_label(l_fallthrough);
            exit_break_label();
            emit_label(l_end);
            emit_pop();
        }
            break;


        default:
            dump_atom(pos, 0);
            error("Invalid program");
    }
    out_comment(ast_text);
    debug("compiled %s", ast_text);
}

void compile_func(func *f) {
    if (!f->body_pos) {
        return;
    }
    set_token_pos(program[f->body_pos].token_pos);

    func_return_label = new_label();

    out_str(".globl	", f->name, "");
    out_str(".type	", f->name, ", @function");
    out_label(f->name);
    out("pushq	%rbp");
    out("movq	%rsp, %rbp");

    out_int("subq	$", align(f->max_offset, 16), ", %rsp");

    int arg_offset = 0;
    int reg_index = 0;
    for (int i=0; i<f->argc; i++) {
        var_t *v = &(f->argv[i]);
        debug("emitting function:%s arg:%s", f->name, v->name);
        if (v->t->struct_of) {
            if (v->t->size > 16 || reg_index >= ABI_NUM_GP || (v->t->size > 8 && reg_index == ABI_NUM_GP - 1)) {
                debug("is on the stack. do nothing here", f->name, v->name);
            } else if (v->t->size <= 8) {
                debug("is passed by one register. do nothing here", f->name, v->name);
            } else {
                debug("is passed by two registers. ", f->name, v->name);
                emit_var_arg_init(reg_index++, v->offset, 8);
                emit_var_arg_init(reg_index++, v->offset - 8, type_size(v->t) - 8);
                arg_offset = align(v->offset, ALIGN_OF_STACK);
            }
            continue;
        } else {
            switch (type_size(v->t))  { 
                case 1: case 4: case 8: break;
                default: error("invalid size for funciton arg:%s", v->name); 
            }
        }
        if (reg_index < ABI_NUM_GP) {
            emit_var_arg_init(reg_index, v->offset, type_size(v->t));
            arg_offset = align(v->offset, ALIGN_OF_STACK);
            reg_index++;
        }
    }
    if (f->is_variadic) {
        for (int i=0; i<ABI_NUM_GP; i++) {
            emit_var_arg_init(ABI_NUM_GP-i-1, 8 + i*8 + arg_offset + 8*16, 8);
        }
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
    if (pt == type_char) {
        out_int(".byte\t", value, "");
        filled_size += 1;
    } else if (pt == type_int) {
        out_int(".long\t", value, "");
        filled_size += 4;
    } else if (pt == type_long) {
        out_int(".quad\t", value, "");
        filled_size += 4;
    } else if (pt == type_char_ptr) {
        out_int(".quad\t.G", value, "");
        filled_size += 8;
    } else if (pt->ptr_to) {
        out_int(".quad\t", value, "");
        filled_size += 8;
    }
    return filled_size;
}

void out_global_constant(var_t *v) {
    out_str(".globl\t", v->name, "");
    out(".data");
    out_int(".align\t", 4, "");
    out_str(".type\t", v->name, ", @object");
    out_int4(".size\t", v->name, ", ", type_size(v->t), "");
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
        if (type_size(v->t) > filled_size) {
            out_int(".zero\t", type_size(v->t) - filled_size, "");
        }
    } else {
        int filled_size = out_global_constant_by_type(v->t, v->int_value);
        if (!filled_size) {
            char buf[RCC_BUF_SIZE] = {0};
            dump_type(buf, v->t);
            error("unknown size for global variable:%s %s", v->name, buf);
        }
    }
    out("");
}

void out_global_declare(var_t *v) {
    char buf[RCC_BUF_SIZE];
    buf[0] = 0;
    strcat(buf, ".comm	");
    strcat(buf, v->name);
    _strcat3(buf, ", ", type_size(v->t), "");
    out(buf);
}

void emit(int fd) {
    output_fd = fd;
    
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
        if (f->body_pos != 0) {
            debug("%s --------------------- ", f->name);
            //dump_atom_tree(f->body_pos, 0);
            compile_func(f);
        }
        f++;
    }
}
