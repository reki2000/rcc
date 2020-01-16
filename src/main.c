#include "rsys.h"
#include "rstring.h"
#include "devtool.h"

#include "types.h"
#include "atom.h"
#include "var.h"

#include "parse.h"

void out_label(char *str) {
    int len;
    len = _strlen(str);
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
    _strcat(buf, str1);
    _stritoa(buf, i);
    _strcat(buf, str2);
    out(buf);
}

void emit_int(int i) {
    out_int("movq	$", i, ", %rax");
    out("pushq	%rax");
}

void emit_ref(int i) {
    out_int("movq	-", i, "(%rbp), %rax");
    out("pushq	%rax");
}

void emit_bind(int i) {
    out("popq	%rax");
    out_int("movq	%rax, -", i, "(%rbp)");
    out("pushq	%rax");
}

void emit_pop() {
    out("popq	%rax");
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

void emit_eq_eq() {
    out("popq	%rdx");
    out("popq	%rcx");
    out("xorq   %rax, %rax");
    out("subq	%rdx, %rcx");
    out("sete %al");
    out("pushq	%rax");
}

void emit_eq_ne() {
    out("popq	%rdx");
    out("popq	%rcx");
    out("xorq   %rax, %rax");
    out("subq	%rdx, %rcx");
    out("setne %al");
    out("pushq	%rax");
}

void emit_eq_lt() {
    out("popq	%rdx");
    out("popq	%rcx");
    out("xorq   %rax, %rax");
    out("subq	%rdx, %rcx");
    out("setnge %al");
    out("pushq	%rax");
}

void emit_eq_le() {
    out("popq	%rdx");
    out("popq	%rcx");
    out("xorq   %rax, %rax");
    out("subq	%rdx, %rcx");
    out("setle %al");
    out("pushq	%rax");
}

void emit_eq_gt() {
    out("popq	%rdx");
    out("popq	%rcx");
    out("xorq   %rax, %rax");
    out("subq	%rdx, %rcx");
    out("setnle %al");
    out("pushq	%rax");
}

void emit_eq_ge() {
    out("popq	%rdx");
    out("popq	%rcx");
    out("xorq   %rax, %rax");
    out("subq	%rdx, %rcx");
    out("setge %al");
    out("pushq	%rax");
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

void compile(int pos) {
    debug_i("compiling atom @", pos);
    switch (program[pos].type) {
        case TYPE_INT: 
            emit_int(program[pos].value.int_value);
            break;
        case TYPE_VAR_REF:
            emit_ref(program[pos].value.int_value);
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
        case TYPE_BIND:
            compile(program[pos+1].value.atom_pos);
            emit_bind(program[pos].value.int_value);
            break;
        case TYPE_LOG_NOT:
            compile(program[pos].value.atom_pos);
            emit_log_not();
            break;
        default:
            error("Invalid program");
    }
    debug_i("compiled @", pos);
}

int main() {
    int pos;

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

    parse_init();

    pos = parse();
    if (pos == 0) {
        error("Invalid source code");
    }
    compile(pos);

    out("leave");
    out("ret");
}