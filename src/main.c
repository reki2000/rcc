#include "rsys.h"
#include "rstring.h"
#include "devtool.h"

#include "types.h"
#include "type.h"
#include "token.h"
#include "var.h"
#include "atom.h"
#include "func.h"

#include "parse.h"

void out_label(char *str) {
    int len = strlen(str);
    _write(1, str, len);
    _write(1, ":\n", 2);
}

void out(char *str) {
    int len = strlen(str);
    _write(1, "\t", 1);
    _write(1, str, len);
    _write(1, "\n", 1);
}

void out_int(char *str1, int i, char *str2) {
    char buf[1024];
    buf[0] = 0;
    _strcat3(buf, str1, i, str2);
    out(buf);
}

void out_str(char *str1, char *str2, char *str3) {
    char buf[1024];
    buf[0] = 0;
    strcat(buf, str1);
    strcat(buf, str2);
    strcat(buf, str3);
    out(buf);
}

int label_index = 0;
int new_label() {
    return label_index++;
}

void emit_int(int i) {
    out_int("movl	$", i, ", %eax");
    out("pushq	%rax");
}

void emit_var_val(int i) {
    out_int("movl	-", i, "(%rbp), %eax");
    out("pushq	%rax");
}

void emit_deref() {
    out("popq	%rax");
    out("movl	(%rax), %eax");
    out("pushq	%rax");
}

void emit_var_ref(int i) {
    out_int("lea	-", i, "(%rbp), %rax");
    out("pushq	%rax");
}

void emit_copy() {
    out("popq	%rax");
    out("popq	%rdx");
    out("movl	%edx, (%rax)");
    out("pushq	%rdx");
}

void emit_pop() {
    out("popq	%rax");
    out("");
}

void emit_add() {
    out("popq	%rdx");
    out("popq	%rax");
    out("addl	%edx, %eax");
    out("pushq	%rax");
}

void emit_sub() {
    out("popq	%rdx");
    out("popq	%rax");
    out("subl	%edx, %eax");
    out("pushq	%rax");
}

void emit_mul() {
    out("popq	%rdx");
    out("popq	%rax");
    out("imull	%edx, %eax");
    out("pushq	%rax");
}

void emit_div() {
    out("popq	%rcx");
    out("popq	%rax");
    out("cdq");
    out("idivl	%ecx");
    out("pushq	%rax");
}

void emit_mod() {
    out("popq	%rcx");
    out("popq	%rax");
    out("cdq");
    out("idivl	%ecx");
    out("pushq	%rdx");
}

void emit_eq_x(char *set) {
    out("popq	%rdx");
    out("popq	%rcx");
    out("xorl   %eax, %eax");
    out("subl	%edx, %ecx");
    out(set);
    out("pushq	%rax");
}

void emit_eq_eq() {
    emit_eq_x("sete %al");
}

void emit_eq_ne() {
    emit_eq_x("setne %al");
}

void emit_eq_lt() {
    emit_eq_x("setnge %al");
}

void emit_eq_le() {
    emit_eq_x("setle %al");
}

void emit_eq_gt() {
    emit_eq_x("setnle %al");
}

void emit_eq_ge() {
    emit_eq_x("setge %al");
}

void emit_log_or() {
    out("popq	%rdx");
    out("popq	%rax");
    out("orl	%edx, %eax");
    out("pushq	%rax");
}

void emit_log_and() {
    out("popq	%rdx");
    out("popq	%rax");
    out("andl	%edx, %eax");
    out("pushq	%rax");
}

void emit_log_not() {
    out("popq	%rdx");
    out("xorq   %rax, %rax");
    out("orl	%edx, %edx");
    out("setz %al");
    out("pushq	%rax");
}

void emit_print() {
    out("popq	%rax");
	out("movl	%eax, %esi");
	out("movl	$.LC0, %edi");
	out("movl	$0, %eax");
	out("call	printf");
	out("nop");
}

void emit_label(int i) {
    char buf[100];
    buf[0] = 0;
    _strcat3(buf, ".L", i, "");
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


int func_return_label;

void compile(int pos) {
    atom *p = &(program[pos]);
    debug_i("compiling atom @", pos);
    switch (p->type) {
        case TYPE_VAR_REF:
            emit_var_ref(p->value.int_value);
            break;
        case TYPE_VAR_VAL:
            emit_var_val(p->value.int_value);
            break;

        case TYPE_BIND:
            compile(p->value.atom_pos); // rvalue
            compile((p+1)->value.atom_pos); // lvalue - should be an address
            emit_copy();
            break;
        case TYPE_PTR:
            compile(p->value.atom_pos);
            break;
        case TYPE_PTR_DEREF:
            compile(p->value.atom_pos);
            emit_deref();
            break;

        case TYPE_INT: 
            emit_int(p->value.int_value);
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
            compile(p->value.atom_pos);
            compile((p+1)->value.atom_pos);
            switch (p->type) {
                case TYPE_ADD: emit_add(); break;
                case TYPE_SUB: emit_sub(); break;
                case TYPE_DIV: emit_div(); break;
                case TYPE_MOD: emit_mod(); break;
                case TYPE_MUL: emit_mul(); break;
                case TYPE_EQ_EQ: emit_eq_eq(); break;
                case TYPE_EQ_NE: emit_eq_ne(); break;
                case TYPE_EQ_LE: emit_eq_le(); break;
                case TYPE_EQ_LT: emit_eq_lt(); break;
                case TYPE_EQ_GE: emit_eq_ge(); break;
                case TYPE_EQ_GT: emit_eq_gt(); break;
                case TYPE_LOG_OR: emit_log_or(); break;
                case TYPE_LOG_AND: emit_log_and(); break;
            }
            break;
        case TYPE_NOP:
            break;

        case TYPE_EXPR_STATEMENT:
            compile(p->value.atom_pos);
            emit_pop();
            break;
        case TYPE_ANDTHEN:
            compile(p->value.atom_pos);
            compile((p+1)->value.atom_pos);
            break;

        case TYPE_PRINT:
            compile(p->value.atom_pos);
            emit_print();
            break;
            
        case TYPE_LOG_NOT:
            compile(p->value.atom_pos);
            emit_log_not();
            break;
        case TYPE_IF: 
        {
            bool has_else = ((p+2)->value.atom_pos != 0);
            int l_end = new_label();
            int l_else = new_label();

            compile(p->value.atom_pos);
            emit_jmp_false(has_else ? l_else : l_end);
            compile((p+1)->value.atom_pos);

            if (has_else) {
                emit_jmp(l_end);
                emit_label(l_else);
                compile((p+2)->value.atom_pos);
            }
            emit_label(l_end);
        }
            break;
        case TYPE_FOR:
        {
            int l_body = new_label();
            int l_end = new_label();
            compile((p+2)->value.atom_pos);
            emit_pop();
            emit_label(l_body);
            compile((p+1)->value.atom_pos);
            emit_jmp_false(l_end);
            compile(p->value.atom_pos);
            compile((p+3)->value.atom_pos);
            emit_jmp(l_body);
            emit_label(l_end);
        }
            break;
        case TYPE_WHILE:
        {
            int l_body = new_label();
            int l_end = new_label();
            emit_label(l_body);
            compile((p+1)->value.atom_pos);
            emit_jmp_false(l_end);
            compile(p->value.atom_pos);
            emit_jmp(l_body);
            emit_label(l_end);
        }
            break;
        case TYPE_DO_WHILE:
        {
            int l_body = new_label();
            emit_label(l_body);
            compile(p->value.atom_pos);
            compile((p+1)->value.atom_pos);
            emit_jmp_true(l_body);
        }
            break;

        case TYPE_RETURN:
            compile(p->value.atom_pos);
            emit_pop();
            emit_jmp(func_return_label);
            break;

        default:
            error("Invalid program");
    }
    debug_i("compiled @", pos);
}

void compile_func(func *f) {
    func_return_label = new_label();

    out_str(".globl	", f->name, "");
    out_str(".type	", f->name, ", @function");
    out_label(f->name);
    out("pushq	%rbp");
    out("movq	%rsp, %rbp");
    out_int("subq	$", f->max_offset, ", %rsp");

    compile(f->body_pos);

    emit_label(func_return_label);
    out("leave");
    out("ret");
    out("");
}

int main() {
    int pos;
    func *f;

    init();
    tokenize();

    init_types();

    pos = parse();
    if (pos == 0) {
        error("Invalid source code");
    }

    out(".file	\"main.c\"");
    out("");

    out(".section	.rodata");
    out_label(".LC0");
    out(".string	\"%d\\n\"");

    out(".text");
    out("");

    f = &functions[0];
    while (f->name != 0) {
        compile_func(f);
        f++;
    }
}
