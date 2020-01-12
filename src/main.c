#include "rsys.h"
#include "rstring.h"
#include "devtool.h"

#include "types.h"
#include "atom.h"
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

void emit_push(int i) {
    char buf[1024];
    buf[0] = 0;
    _strcat(buf, "movq $");
    _stritoa(buf, i);
    _strcat(buf, ", %rax");
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

void compile(int pos) {
    if (program[pos].type == TYPE_INT) {
        emit_push(program[pos].value.int_value);
    } else if (program[pos].type == TYPE_ADD) {
        compile(program[pos].value.atom_pos);
        compile(program[pos+1].value.atom_pos);
        emit_add();
    } else if (program[pos].type == TYPE_SUB) {
        compile(program[pos].value.atom_pos);
        compile(program[pos+1].value.atom_pos);
        emit_sub();
    } else if (program[pos].type == TYPE_DIV) {
        compile(program[pos].value.atom_pos);
        compile(program[pos+1].value.atom_pos);
        emit_div();
    } else if (program[pos].type == TYPE_MOD) {
        compile(program[pos].value.atom_pos);
        compile(program[pos+1].value.atom_pos);
        emit_mod();
    } else if (program[pos].type == TYPE_MUL) {
        compile(program[pos].value.atom_pos);
        compile(program[pos+1].value.atom_pos);
        emit_mul();
    } else if (program[pos].type == TYPE_EXPR_STATEMENT) {
        for (;;) {
            compile(program[pos].value.atom_pos);
            emit_pop();
            if (program[pos+1].value.atom_pos == 0) {
                break;
            }
            pos = program[pos+1].value.atom_pos;
        }
    } else if (program[pos].type == TYPE_BLOCK) {
        for (;;) {
            compile(program[pos].value.atom_pos);
            if (program[pos+1].value.atom_pos == 0) {
                break;
            }
            pos = program[pos+1].value.atom_pos;
        }
    } else {
        error("Invalid program");
    }
    debug_i("compiled 1 atom @", pos);
}

int main() {
    int pos;

    out(".file \"main.c\"");
    out(".text");
    out(".globl main");
    out(".type main, @function");
    out_label("main:");
    out("pushq  %rbp");
    out("movq   %rsp, %rbp");

    parse_init();

    pos = parse();
    if (pos == 0) {
        error("Invalid source code");
    }
    compile(pos);

    out("leave");
    out("ret");
}