#include "rsys.h"
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

void debug(char *str) {
    char buf[1024];
    buf[0] = 0;
    _strcat(buf, str);
    _strcat(buf, "\n");
    _write(2, buf, _strlen(buf));
}

void error(char *str) {
    _write(2, str, _strlen(str));
    __exit(1);
}

typedef enum {
    FALSE = 0,
    TRUE = 1
} bool;

enum {
    TYPE_ARG = 0,
    TYPE_INT,
    TYPE_ADD,
    TYPE_MUL,
    TYPE_IDENT
};

typedef union value_t {
    char *str_value;
    int int_value;
    int atom_pos;
} value;

typedef struct atom_t {
    int type;
    value value;
} atom;

atom program[10000];
int atom_pos = 1;

int alloc_atom(int size) {
    int current;
    current = atom_pos;
    if (atom_pos + size >= 100) {
        error("Source code too long");
    }
    atom_pos += size;
    return current;
}

void dump_atom(int pos) {
    char buf[1024];
    buf[0] = 0;
    _strcat(buf, "[");
    _stritoa(buf, pos);
    _strcat(buf, "] type:");
    _stritoa(buf, program[pos].type);
    _strcat(buf, " value:");
    if (program[pos].type == TYPE_IDENT) {
        _strcat(buf, program[pos].value.str_value);
    } else {
        _stritoa(buf, program[pos].value.int_value);
    }
    _strcat(buf, "\n");
    _write(2, buf, _strlen(buf));
}

void dump_atom_all() {
    int i;
    for (i=1; i<atom_pos; i++) {
        dump_atom(i);
    }
}

void build_int_atom(int pos, int type, int value) {
    program[pos].type = type;
    program[pos].value.int_value = value;
}

void build_string_atom(int pos, int type, char * value) {
    program[pos].type = type;
    program[pos].value.str_value = value;
}

void build_pos_atom(int pos, int type, int value) {
    program[pos].type = type;
    program[pos].value.atom_pos = value;
}

int ch() {
    char str[100];

    if (pos >= len) {
        return -1;
    }

    /*
    str[0] = ':';
    str[1] = src[pos];
    str[2] = ' ';
    str[3] = 0;
    debug(str);
    */

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

bool expect(char c) {
    skip();
    if (ch() == c) {
        next();
        skip();
        return TRUE;
    }
    return FALSE;
}

bool is_delimiter() {
    if (ch() == ';') {
        next();
        return FALSE;
    }
    return TRUE;
}

bool parse_expr();
bool parse_add();
bool parse_int();
bool parse_mul();

// expr := mul ('+' mul)*
// mul := primary ('*' primary)*
// primary := int | '(' expr ')'
// int := ['0' - '9']*

bool parse_int() {
    int c;
    int value = 0;
    int count = 0;
    int pos = 0;

    for (;;) {
        c = ch();
        if (c >= '0' && c <= '9') {
            value *= 10;
            value += ((char)c - '0');
        } else {
            break;
        }
        count++;
        next();
    }

    if (count == 0) {
        debug("parse_int: not int");
        return 0;
    }

    pos = alloc_atom(1);
    build_int_atom(pos, TYPE_INT, value);
    debug("parse_int: parsed");
    return pos;
}

int parse_primary() {
    int pos;
    pos = parse_int();
    if (pos != 0) {
        debug("parse_primary: parsed int");
        return pos;
    } 

    if (expect('(')) {
        pos = parse_expr();
        if (pos == 0) {
            debug("parse_primary: not expr");
            return 0;
        }
        if (expect(')')) {
            debug("parse_primary: parsed expr");
            return pos;
        }
        debug("parse_primary: no closing parenthesis");
        return 0;
    }
    debug("parse_primary: not int neither (expr)");
    return 0;
}

bool parse_expr() {
    int lpos;
    int rpos;
    int oppos = 0;

    skip();
    lpos = parse_mul();
    if (lpos == 0) {
        debug("parse_expr: not found mul");
        return 0;
    }

    for (;;) {
        skip();
        if (expect('+')) {
            rpos = parse_mul();
            if (rpos == 0) {
                debug("parse_expr: not found mul after '+'");
                return 0;
            }
            oppos = alloc_atom(2);
            build_pos_atom(oppos, TYPE_ADD, lpos);
            build_pos_atom(oppos + 1, TYPE_ARG, rpos);
            lpos = oppos;
            debug("parse_expr: parsed mul once");
        } else {
            break;
        }
    }
    debug("parse_expr: parsed");
    return lpos;
}

bool parse_mul() {
    int lpos;
    int rpos;
    int oppos = 0;

    skip();
    lpos = parse_primary();
    if (lpos == 0) {
        debug("parse_mul: not found primary");
        return 0;
    }

    for (;;) {
        skip();
        if (expect('*')) {
            debug("parse_mul: not found primary after '*'");
            rpos = parse_primary();
            if (rpos == 0) {
                return 0;
            }
            oppos = alloc_atom(2);
            build_pos_atom(oppos, TYPE_MUL, lpos);
            build_pos_atom(oppos + 1, TYPE_ARG, rpos);
            lpos = oppos;
            debug("parse_mul: parsed primary once");
        } else {
            break;
        }
    }
    debug("parse_mul: parsed mul");
    return lpos;
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

void emit_mul() {
    out("popq %rdx");
    out("popq %rax");
    out("imulq %rdx, %rax");
    out("pushq %rax");
}

void compile(int pos) {
    if (program[pos].type == TYPE_INT) {
        emit_push(program[pos].value.int_value);
    } else if (program[pos].type == TYPE_ADD) {
        compile(program[pos].value.atom_pos);
        compile(program[pos+1].value.atom_pos);
        emit_add();
    } else if (program[pos].type == TYPE_MUL) {
        compile(program[pos].value.atom_pos);
        compile(program[pos+1].value.atom_pos);
        emit_mul();
    }
    debug("compiled 1 atom");
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

    len = _read(0, src, 1024);

    pos = parse_expr();
    if (pos == 0) {
        error("Invalid source code");
    }
    dump_atom_all();
    compile(pos);

    out("leave");
    out("ret");
}