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

void emit_int(int i) {
    char buf[1024];
    buf[0] = 0;
    _strcat(buf, "movq $");
    _stritoa(buf, i);
    _strcat(buf, ", %rax");
    out(buf);
    out("pushq %rax");
}

void emit_ref(int i) {
    char buf[1024];
    buf[0] = 0;
    _strcat(buf, "movq -");
    _stritoa(buf, i);
    _strcat(buf, "(%rbp), %rax");
    out(buf);
    out("pushq %rax");
}

void emit_pop() {
    out("popq %rax");
}

void emit_add() {
    out("popq %rdx");
    out("popq %rax");
    out("addq %rdx, %rax");
    out("pushq %rax");
}

void emit_sub() {
    out("popq %rdx");
    out("popq %rax");
    out("subq %rdx, %rax");
    out("pushq %rax");
}

void emit_mul() {
    out("popq %rdx");
    out("popq %rax");
    out("imulq %rdx, %rax");
    out("pushq %rax");
}

void emit_div() {
    out("popq %rcx");
    out("xorq %rdx, %rdx");
    out("popq %rax");
    out("idivq %rcx");
    out("pushq %rax");
}

void emit_mod() {
    out("popq %rcx");
    out("xorq %rdx, %rdx");
    out("popq %rax");
    out("idivq %rcx");
    out("pushq %rdx");
}

void emit_printi() {
    out("popq   %rax");
	out("movl	%eax, %esi");
	out("movl	$.LC0, %edi");
	out("movl   $0, %eax");
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
            compile(program[pos].value.atom_pos);
            compile(program[pos+1].value.atom_pos);
            switch (program[pos].type) {
                case TYPE_ADD: emit_add(); break;
                case TYPE_SUB: emit_sub(); break;
                case TYPE_DIV: emit_div(); break;
                case TYPE_MOD: emit_mod(); break;
                case TYPE_MUL: emit_mul(); break;
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
        default:
            error("Invalid program");
    }
    debug_i("compiled @", pos);
}

int main() {
    int pos;

    out(".file \"main.c\"");
    out(".section   .rodata");
    out_label(".LC0:");
    out(".string \"%d\\n\"");
    out(".text");
    out(".globl main");
    out(".type main, @function");
    out_label("main:");
    out("pushq  %rbp");
    out("movq   %rsp, %rbp");
    out("subq   $800, %rsp");

    parse_init();

    pos = parse();
    if (pos == 0) {
        error("Invalid source code");
    }
    compile(pos);

    out("leave");
    out("ret");
}