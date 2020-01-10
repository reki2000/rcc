#include "rio.h"
#include "rstring.h"

char src[1024];
int pos = 0;
int len = 0;

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

int ch() {
    if (pos >= len) {
        return -1;
    }
    return src[pos];
}

void next() {
    pos++;
}

void skip() {
    int c;
    for (;;) {
        c = ch();
        if (c != ' ' && c != '\t' && c != '\n') {
            break;
        }
        next();
    }
}

int op_add() {
    if (ch() == '+') {
        next();
        return 0;
    }
    return 1;
}

int number(int* retval) {
    int c;
    int result = 0;
    for (;;) {
        c = ch();
        if (c >= '0' && c <= '9') {
            result *= 10;
            result += ((char)c - '0');
        } else {
            break;
        }
        next();
    }
    *retval = result;
    return 0;
}

int program[100];

void parse() {
    skip();
    number(&program[0]);
    skip();
    op_add();
    skip();
    number(&program[1]);
    skip();
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

void emit() {
    emit_push(program[0]);
    emit_push(program[1]);
    emit_add();
    emit_pop();
}

int main() {
    out(".file \"main.c\"");
    out(".text");
    out(".globl main");
    out(".type main, @function");
    out_label("main:");
    out("pushq  %rbp");
    out("movq   %rsp, %rbp");

    len = _read(0, src, 1024);

    parse();
    emit();

    out("leave");
    out("ret");
}