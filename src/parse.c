#include "rsys.h"
#include "rstring.h"
#include "devtool.h"
#include "types.h"
#include "atom.h"
#include "var.h"

char src[1024];
int src_pos = 0;
int src_len = 0;

bool is_eof() {
    return src_pos >= src_len;
}

int ch() {
    if (is_eof()) {
        return -1;
    }
    return src[src_pos];
}

void next() {
    src_pos++;
}

int ch2(int offset) {
    if (src_pos + offset >= src_len) {
        return -1;
    }
    return src[src_pos + offset];
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

bool is_alpha(int ch) {
    return (ch >= 'a' && ch <= 'z')
        || (ch >= 'A' && ch <= 'Z');
}

bool is_digit(int ch) {
    return (ch >= '0' && ch <= '9');
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

bool expect_str(char *str) {
    skip();
    int offset = 0;
    for (;*str != 0; str++) {
        if (ch2(offset) != *str) {
            return FALSE;
        }
        offset++;
    }
    for(;offset>0;offset--) {
        next();
    }
    skip();
    return TRUE;
}



// block := '{' var_delcare* (statement | block)* '}'
// var_declare := "int" var_name ';'
// statement := ';' | print_statement | expr_statement
// print_statement := "print" expt ';'
// expr_statement := expr ';'
// expr := mul ([+-] mul)*
// mul := primary ( [*/%] primary)*
// primary := int | var_ref | '()' expr ')'
// var_ref := IDENT
// var_name := IDENT
// int := DIGIT*

// IDENT := IDENT_CHAR (IDENT_CHAR | DIGIT)*;
// IDENT_CHAR := ALPHA | '_'
// ALPHA := [A-za-z]
// DIGIT := [0-9]

bool parse_int() {
    int value = 0;
    int count = 0;

    for (;;) {
        int c = ch();
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

    debug_i("parse_int: parsed: ", value);
    return alloc_int_atom(TYPE_NOP, value);
}

char *parse_ident_token() {
    bool is_first = TRUE;
    char buf[100];
    int buf_pos = 0;
    char *str;
    int i;

    skip();
    for (;;) {
        int c;
        c = ch();
        if (!is_alpha(c) && c != '_' && (is_first || !is_digit(c))) {
            break;
        }
        buf[buf_pos++] = c;
        next();
        is_first = FALSE;
    }

    if (buf_pos == 0) {
        return 0;
    }

    str = _malloc(buf_pos + 1);
    for (i=0; i<buf_pos; i++) {
        str[i] = buf[i];
    }
    str[i] = 0;
    return str;
}

int parse_expr();

int parse_primary() {
    int pos;
    char *ident;

    skip();
    pos = parse_int();
    if (pos != 0) {
        debug("parse_primary: parsed int");
        return pos;
    } 

    ident = parse_ident_token();
    if (ident != 0) {
        int offset = find_var_offset(ident);
        if (offset == 0) {
            error("Unknown variable");
        }
        pos = alloc_atom(1);
        build_int_atom(pos, TYPE_VAR_REF, offset);
        debug_s("parse_primary: parsed variabe_ref:", ident);
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
        oppos = alloc_atom(1);
        build_pos_atom(oppos, TYPE_EXPR_STATEMENT, pos);
        debug_i("parse_expr_statement: parsed @", oppos);
        return oppos;
    }
    debug("parse_expr_statement: not found");
    return 0;
}


int parse_print_statement() {
    int pos;

    if (expect_str("print")) {
        pos = parse_expr_statement();
        if (pos != 0) {
            int oppos = alloc_pos_atom(TYPE_PRINTI, pos);
            debug_i("parse_print_statement: parsed @", oppos);
            return oppos;
        }
    }
    debug("parse_print_statement: not found");
    return 0;
}

int parse_statement() {
    if (expect(';')) {
        return alloc_int_atom(TYPE_NOP, 0);
    } 
    int pos = parse_print_statement();
    if (pos == 0) {
        pos = parse_expr_statement();
    }
    return pos;
}

int parse_var_declare() {
    char *ident;
    if (!expect_str("int")) {
        debug("parse_var_declare: not found 'int'");
        return 0;
    }

    ident = parse_ident_token();
    if (ident == 0) {
        error("parse_var_declare: invalid name");
    }
    if (!expect(';')) {
        error("parse_var_declare: no ;");
    }
    add_var(ident);
    debug_s("parse_var_declare: parsed: ", ident);
    return 1;
}

int parse_block() {
    int prev_pos = 0;
    int pos;

    if (!expect('{')) {
        debug("parse_block: not found '{'");
        return 0;
    }

    enter_var_frame();
    for (;;) {
        if (parse_var_declare() == 0) {
            break;
        }
    }

    for (;;) {
        int new_pos = parse_statement();
        if (new_pos == 0) {
            new_pos = parse_block();
        }
        if (new_pos == 0) {
            break;
        }
        if (prev_pos != 0) {
            pos = alloc_atom(2);
            build_pos_atom(pos, TYPE_ANDTHEN, prev_pos);
            build_pos_atom(pos+1, TYPE_ARG, new_pos);
        } else {
            pos = new_pos;
        }
        prev_pos = pos;
    }

    if (!expect('}')) {
        error("parse_block: not found '}'");
    }

    exit_var_frame();
    if (pos == 0) {
        pos = alloc_int_atom(TYPE_NOP, 0);
    }
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