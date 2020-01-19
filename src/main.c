#include "rsys.h"
#include "rstring.h"
#include "devtool.h"

#include "types.h"
#include "type.h"
#include "token.h"
#include "var.h"
#include "atom.h"

#include "parse.h"

void out_label(char *str) {
    int len;
    len = strlen(str);
    _write(1, str, len);
    _write(1, "\n", 1);
}

void out(char *str) {
    _write(1, "\t", 1);
    out_label(str);
}

void out_int(char *str1, int i, char *str2) {
    char buf[1024];
    buf[0] = 0;
    _strcat3(buf, str1, i, str2);
    out(buf);
}

int label_index = 0;
int new_label() {
    return label_index++;
}

void emit_int(int i) {
    out_int("movq	$", i, ", %rax");
    out("pushq	%rax");
}

void emit_var_val(int i) {
    out_int("movq	-", i, "(%rbp), %rax");
    out("pushq	%rax");
}

void emit_deref() {
    out("popq	%rax");
    out("movq	(%rax), %rax");
    out("pushq	%rax");
}

void emit_var_ref(int i) {
    out_int("lea	-", i, "(%rbp), %rax");
    out("pushq	%rax");
}

void emit_copy() {
    out("popq	%rax");
    out("popq	%rdx");
    out("movq	%rdx, (%rax)");
    out("pushq	%rdx");
}

void emit_pop() {
    out("popq	%rax");
    out("");
}

void emit_add() {
    out("popq	%rdx");
    out("popq	%rax");
    out("addq	%rdx, %rax");
    out("pushq	%rax");
}

void emit_sub() {
    out("popq	%rdx");
    out("popq	%rax");
    out("subq	%rdx, %rax");
    out("pushq	%rax");
}

void emit_mul() {
    out("popq	%rdx");
    out("popq	%rax");
    out("imulq	%rdx, %rax");
    out("pushq	%rax");
}

void emit_div() {
    out("popq	%rcx");
    out("xorq	%rdx, %rdx");
    out("popq	%rax");
    out("idivq	%rcx");
    out("pushq	%rax");
}

void emit_mod() {
    out("popq	%rcx");
    out("xorq	%rdx, %rdx");
    out("popq	%rax");
    out("idivq	%rcx");
    out("pushq	%rdx");
}

void emit_eq_x(char *set) {
    out("popq	%rdx");
    out("popq	%rcx");
    out("xorq   %rax, %rax");
    out("subq	%rdx, %rcx");
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
    out("orq	%rdx, %rax");
    out("pushq	%rax");
}

void emit_log_and() {
    out("popq	%rdx");
    out("popq	%rax");
    out("andq	%rdx, %rax");
    out("pushq	%rax");
}

void emit_log_not() {
    out("popq	%rdx");
    out("xorq   %rax, %rax");
    out("orq	%rdx, %rdx");
    out("setz %al");
    out("pushq	%rax");
}

void emit_printi() {
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
    _strcat3(buf, ".L", i, ":");
    out_label(buf);
}

void emit_jmp(int i) {
    out_int("jmp .L", i, "");
}

void emit_jmp_false(int i) {
    out("popq	%rax");
    out("orq	%rax, %rax");
    out_int("jz	.L", i, "");
}

void emit_jmp_true(int i) {
    out("popq	%rax");
    out("orq	%rax, %rax");
    out_int("jnz	.L", i, "");
}

void compile(int pos) {
    debug_i("compiling atom @", pos);
    switch (program[pos].type) {
        case TYPE_VAR_REF:
            emit_var_ref(program[pos].value.int_value);
            break;
        case TYPE_VAR_VAL:
            emit_var_val(program[pos].value.int_value);
            break;

        case TYPE_BIND:
            compile(program[pos].value.atom_pos); // rvalue
            compile(program[pos+1].value.atom_pos); // lvalue - should be an address
            emit_copy();
            break;
        case TYPE_PTR:
            compile(program[pos].value.atom_pos);
            break;
        case TYPE_PTR_DEREF:
            compile(program[pos].value.atom_pos);
            emit_deref();
            break;

        case TYPE_INT: 
            emit_int(program[pos].value.int_value);
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
            compile(program[pos].value.atom_pos);
            compile(program[pos+1].value.atom_pos);
            switch (program[pos].type) {
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
            compile(program[pos].value.atom_pos);
            emit_pop();
            break;
        case TYPE_ANDTHEN:
            compile(program[pos].value.atom_pos);
            compile(program[pos+1].value.atom_pos);
            break;
        case TYPE_PRINTI:
            compile(program[pos].value.atom_pos);
            emit_printi();
            break;
            
        case TYPE_LOG_NOT:
            compile(program[pos].value.atom_pos);
            emit_log_not();
            break;
        case TYPE_IF: 
        {
            bool has_else = (program[pos+2].value.atom_pos != 0);
            int l_end = new_label();
            int l_else = new_label();

            compile(program[pos].value.atom_pos);
            emit_jmp_false(has_else ? l_else : l_end);
            compile(program[pos+1].value.atom_pos);

            if (has_else) {
                emit_jmp(l_end);
                emit_label(l_else);
                compile(program[pos+2].value.atom_pos);
            }
            emit_label(l_end);
        }
            break;
        case TYPE_FOR:
        {
            int l_body = new_label();
            int l_end = new_label();
            compile(program[pos+2].value.atom_pos);
            emit_pop();
            emit_label(l_body);
            compile(program[pos+1].value.atom_pos);
            emit_jmp_false(l_end);
            compile(program[pos].value.atom_pos);
            compile(program[pos+3].value.atom_pos);
            emit_jmp(l_body);
            emit_label(l_end);
        }
            break;
        case TYPE_WHILE:
        {
            int l_body = new_label();
            int l_end = new_label();
            emit_label(l_body);
            compile(program[pos+1].value.atom_pos);
            emit_jmp_false(l_end);
            compile(program[pos].value.atom_pos);
            emit_jmp(l_body);
            emit_label(l_end);
        }
            break;
        case TYPE_DO_WHILE:
        {
            int l_body = new_label();
            emit_label(l_body);
            compile(program[pos].value.atom_pos);
            compile(program[pos+1].value.atom_pos);
            emit_jmp_true(l_body);
        }
            break;
        default:
            error("Invalid program");
    }
    debug_i("compiled @", pos);
}

int main() {
    int pos;

    init();
    tokenize();

    init_types();
    pos = parse();
    if (pos == 0) {
        error("Invalid source code");
    }

    out(".file	\"main.c\"");
    out(".section	.rodata");
    out_label(".LC0:");
    out(".string	\"%d\\n\"");
    out(".text");
    out(".globl	main");
    out(".type	main, @function");
    out_label("main:");
    out("pushq	%rbp");
    out("movq	%rsp, %rbp");
    out("subq	$800, %rsp");

    compile(pos);

    out("leave");
    out("ret");
}
