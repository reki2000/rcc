#include <unistd.h>
#include <string.h>
#include <stdio.h>

char src[1024];
int pos = 0;
int len = 0;

void out_label(char *str) {
    int len;
    len = strlen(str);
    write(1, str, len);
    write(1, "\n", 1);
}

void out(char *str) {
    write(1, "\t", 1);
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

int program;

void parse() {
    skip();
    number(&program);
    skip();
}

void emit() {
    char buf[1024];
    sprintf(buf, "movq $%d, %%rax", program);
    out(buf);
}

int main() {
    out(".file \"main.c\"");
    out(".text");
    out(".globl main");
    out(".type main, @function");
    out_label("main:");
    out("pushq  %rbp");
    out("movq   %rsp, %rbp");

    len = read(0, src, 1024);

    parse();
    emit();

    out("leave");
    out("ret");
}