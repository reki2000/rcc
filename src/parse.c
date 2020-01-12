#include "rsys.h"
#include "rstring.h"
#include "devtool.h"
#include "types.h"
#include "atom.h"

char src[1024];
int pos = 0;
int len = 0;

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

bool expect(char c) {
    skip();
    if (ch() == c) {
        next();
        skip();
        return TRUE;
    }
    return FALSE;
}

// expr := mul (['+' | '-'] mul)*
// mul := primary ( ['*' | '/' | '%'] primary)*
// primary := int | '(' expr ')'
// int := ['0' - '9']*

bool parse_int() {
    int value = 0;
    int count = 0;
    int pos;

    for (;;) {
        int c;
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

int parse_expr();

int parse_primary() {
    int pos;

    skip();
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

int parse_mul() {
    int lpos;

    skip();
    lpos = parse_primary();
    if (lpos == 0) {
        debug("parse_mul: not found primary");
        return 0;
    }

    for (;;) {
        int rpos;
        int oppos;
        int type;

        if (expect('*')) {
            type = TYPE_MUL;
        } else if (expect('/')) {
            type = TYPE_DIV;
        } else if (expect('%')) {
            type = TYPE_MOD;
        } else {
            break;
        }

        rpos = parse_primary();
        if (rpos == 0) {
            debug("parse_mul: not found primary after '*/%'");
            return 0;
        }

        oppos = alloc_atom(2);
        build_pos_atom(oppos, type, lpos);
        build_pos_atom(oppos + 1, TYPE_ARG, rpos);
        lpos = oppos;
        debug("parse_mul: parsed primary once");
    }
    debug_i("parse_mul: parsed mul @", lpos);
    return lpos;
}

int parse_expr() {
    int lpos;

    skip();
    lpos = parse_mul();
    if (lpos == 0) {
        debug("parse_expr: not found mul");
        return 0;
    }

    for (;;) {
        int rpos;
        int oppos;
        int type;

        if (expect('+')) {
            type = TYPE_ADD;
        } else if (expect('-')) {
            type = TYPE_SUB;
        } else {
            break;
        }
        rpos = parse_mul();
        if (rpos == 0) {
            debug("parse_expr: not found mul after '+'");
            return 0;
        }
        oppos = alloc_atom(2);
        build_pos_atom(oppos, type, lpos);
        build_pos_atom(oppos + 1, TYPE_ARG, rpos);
        lpos = oppos;
        debug("parse_expr: parsed mul once");
    }
    debug_i("parse_expr: parsed @", lpos);
    return lpos;
}

int parse() {
    int pos;
    pos = parse_expr();
    dump_atom_all();
    return pos;
}

void parse_init() {
    len = _read(0, src, 1024);
}