#include "rsys.h"
#include "rstring.h"
#include "devtool.h"
#include "types.h"
#include "atom.h"

char src[1024];
int src_pos = 0;
int src_len = 0;

int ch() {
    if (src_pos >= src_len) {
        return -1;
    }
    //{char buf[2]; buf[0]=src[src_pos]; buf[1]=0; debug(buf);}
    return src[src_pos];
}

void next() {
    src_pos++;
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

// block := '{' (block | statement)* '}'
// statement := expr_statement
// expr_statement := (expr)? ';'
// expr := mul (['+' | '-'] mul)*
// mul := primary ( ['*' | '/' | '%'] primary)*
// primary := int | '()' expr ')'
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

int parse_expr_statement() {
    int pos;

    pos = parse_expr();

    if (pos != 0 && expect(';')) {
        int oppos;
        oppos = alloc_atom(2);
        build_pos_atom(oppos, TYPE_EXPR_STATEMENT, pos);
        build_pos_atom(oppos+1, TYPE_ARG, 0);
        debug_i("parse_expr_statement: parsed @", oppos);
        return oppos;
    }
    debug("parse_expr_statement: not found");
    return 0;
}

int parse_statement() {
    return parse_expr_statement();
}

int parse_block() {
    int prev_pos = 0;
    int first_pos = 0;
    int pos;

    if (!expect('{')) {
        debug("parse_block: not found '{'");
        return 0;
    }
    for (;;) {
        int pos;
        pos = parse_statement();
        if (pos == 0) {
            pos = parse_block();
        }
        if (pos == 0) {
            break;
        }
        if (first_pos == 0) {
            first_pos = pos;
        } else {
            build_pos_atom(prev_pos+1, TYPE_ARG, pos);
        }
        prev_pos = pos;
    }
    if (!expect('}')) {
        debug("parse_block: not found '}'");
        return 0;
    }

    if (first_pos == 0) {
        debug("parse_block: empty");
        return 0;
    }

    pos = alloc_atom(2);
    build_pos_atom(pos, TYPE_BLOCK, first_pos);
    build_pos_atom(pos+1, TYPE_ARG, 0);
    return pos;
}

int parse() {
    int pos;
    pos = parse_block();
    dump_atom_all();
    return pos;
}

void parse_init() {
    src_len = _read(0, src, 1024);
}