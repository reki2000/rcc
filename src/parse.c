#include "rsys.h"
#include "rstring.h"
#include "devtool.h"
#include "types.h"
#include "token.h"
#include "atom.h"
#include "var.h"


int parse_int() {
    int value;
    if (expect_int(&value)) {
        return alloc_int_atom(TYPE_INT, value);
    }
    return 0;
}

int parse_expr();

int parse_primary() {
    int pos;
    char *ident;

    pos = parse_int();
    if (pos != 0) {
        debug("parse_primary: parsed int");
        return pos;
    } 

    if (expect_ident(&ident)) {
        int offset = find_var_offset(ident);
        if (offset == 0) {
            error_s("Unknown variable:", ident);
        }
        pos = alloc_int_atom(TYPE_VAR_REF, offset);
        debug_s("parse_primary: parsed variabe_ref:", ident);
        return pos;
    }

    if (expect(T_LPAREN)) {
        pos = parse_expr();
        if (pos == 0) {
            return 0;
        }
        if (expect(T_RPAREN)) {
            debug("parse_primary: parsed expr");
            return pos;
        }
        return 0;
    }
    return 0;
}

int parse_not() {
    if (expect(T_L_NOT)) {
        int pos = parse_primary();
        if (pos == 0) {
            error("Invalid '!'");
        }
        debug("parse_not: parsed");
        return alloc_pos_atom(TYPE_LOG_NOT, pos);
    }
    return parse_primary();
}

int parse_unary() {
    return parse_not();
}

int parse_mul() {
    int lpos = parse_unary();
    if (lpos == 0) {
        return 0;
    }

    for (;;) {
        int rpos;
        int type;
        if (expect(T_MUL)) {
            type = TYPE_MUL;
        } else if (expect(T_DIV)) {
            type = TYPE_DIV;
        } else if (expect(T_MOD)) {
            type = TYPE_MOD;
        } else {
            break;
        }

        rpos = parse_unary();
        if (rpos == 0) {
            return 0;
        }

        lpos = alloc_binop_atom(type, lpos, rpos);
    }
    debug_i("parse_mul: parsed mul @", lpos);
    return lpos;
}

int parse_add() {
    int lpos = parse_mul();
    if (lpos == 0) {
        return 0;
    }

    for (;;) {
        int rpos;
        int type;
        if (expect(T_ADD)) {
            type = TYPE_ADD;
        } else if (expect(T_SUB)) {
            type = TYPE_SUB;
        } else {
            break;
        }
        rpos = parse_mul();
        if (rpos == 0) {
            return 0;
        }
        lpos = alloc_binop_atom(type, lpos, rpos);
    }
    debug_i("parse_expr: parsed @", lpos);
    return lpos;
}


int parse_lessgreat() {
    int lpos = parse_add();
    if (lpos == 0) {
        return 0;
    }
    for (;;) {
        int type_eq;
        if (expect(T_LE)) {
            type_eq = TYPE_EQ_LE;
        } else if (expect(T_LT)) {
            type_eq = TYPE_EQ_LT;
        } else if (expect(T_GE)) {
            type_eq = TYPE_EQ_GE;
        } else if (expect(T_GT)) {
            type_eq = TYPE_EQ_GT;
        } else {
            break;
        }
        int rpos = parse_add();
        if (rpos == 0) {
            error("Inalid rval for lessthan");
        }
        lpos = alloc_binop_atom(type_eq, lpos, rpos);
    }
    return lpos;
}

int parse_equality() {
    int lpos = parse_lessgreat();
    if (lpos == 0) {
        return 0;
    }
    for (;;) {
        int type_eq;
        if (expect(T_EQ)) {
            type_eq = TYPE_EQ_EQ;
        } else if (expect(T_NE)) {
            type_eq = TYPE_EQ_NE;
        } else {
            break;
        }
        int rpos = parse_lessgreat();
        if (rpos == 0) {
            error("Inalid rval for == / !=");
        }
        lpos = alloc_binop_atom(type_eq, lpos, rpos);
    }
    return lpos;
}

int parse_logical_and() {
    int lpos = parse_equality();
    if (lpos == 0) {
        return 0;
    }
    for (;;) {
        if (!expect(T_L_AND)) {
            break;
        }
        int rpos = parse_equality();
        if (rpos == 0) {
            error("Inalid rval for &&");
        }
        lpos = alloc_binop_atom(TYPE_LOG_AND, lpos, rpos);
    }
    return lpos;
}

int parse_logical_or() {
    int lpos = parse_logical_and();
    if (lpos == 0) {
        return 0;
    }
    for (;;) {
        if (!expect(T_L_OR)) {
            break;
        }
        int rpos = parse_logical_and();
        if (rpos == 0) {
            error("Inalid rval for ||");
        }
        lpos = alloc_binop_atom(TYPE_LOG_OR, lpos, rpos);
    }
    return lpos;
}

int parse_rvalue() {
    return parse_logical_or();
}

int parse_var_ref() {
    char *ident;
    if (expect_ident(&ident)) {
        int offset = find_var_offset(ident);
        if (offset == 0) {
            error_s("variable not found: ", ident);
        }
        return offset;
    }
    return 0;
}

int parse_expr() {
    int pos = get_token_pos();
    int offset = parse_var_ref();
    if (offset != 0 && expect(T_BIND)) {
        int pos = parse_rvalue();
        if (pos != 0) {
            int oppos = alloc_atom(2);
            build_int_atom(oppos, TYPE_BIND, offset);
            build_pos_atom(oppos+1, TYPE_ARG, pos);
            debug("parse_expr: parsed");
            return oppos;
        } else {
            error("cannot find rvalue");
        }
    }
    set_token_pos(pos);
    return parse_rvalue();
}

int parse_expr_statement() {
    int pos;

    pos = parse_expr();
    if (pos != 0 && expect(T_SEMICOLON)) {
        int oppos = alloc_pos_atom(TYPE_EXPR_STATEMENT, pos);
        debug_i("parse_expr_statement: parsed @", oppos);
        return oppos;
    }
    return 0;
}

int parse_block_or_statement();
int parse_block();
int parse_statement();

int parse_if_statement() {
    int pos;
    int eq_pos;
    int body_pos;
    int else_body_pos = 0;
    if (expect(T_IF)) {
        if (!expect(T_LPAREN)) {
            error("no '(' after if");
        }
        eq_pos = parse_expr();
        if (eq_pos == 0) {
            error("no expr after if");
        }
        if (!expect(T_RPAREN)) {
            error("no ')' after if");
        }
        body_pos = parse_block();
        if (body_pos != 0) {
            if (expect(T_ELSE)) {
                else_body_pos = parse_block_or_statement();
            }
        } else {
            body_pos = parse_statement();
            if (body_pos == 0) {
                error("no body after if");
            }
        }
        pos = alloc_atom(3);
        build_pos_atom(pos, TYPE_IF, eq_pos);
        build_pos_atom(pos+1, TYPE_ARG, body_pos);
        build_pos_atom(pos+2, TYPE_ARG, else_body_pos);
        return pos;
    }
    return 0;
}

int parse_for_statement() {
    int pre_pos;
    int cond_pos;
    int post_pos;
    int body_pos;
    int pos;
    if (expect(T_FOR)) {
        if (!expect(T_LPAREN)) {
            error("no '(' after for");
        }
        pre_pos = parse_expr();
        if (!expect(T_SEMICOLON)) {
            error("no first ';' after for");
        }
        cond_pos = parse_expr();
        if (!expect(T_SEMICOLON)) {
            error("no second ';' after for");
        }
        post_pos = parse_expr();
        if (!expect(T_RPAREN)) {
            error("no ')' after for");
        }
        body_pos = parse_block_or_statement();
        if (body_pos != 0) {
            pos = alloc_atom(4);
            build_pos_atom(pos, TYPE_FOR, body_pos);
            build_pos_atom(pos+1, TYPE_ARG, cond_pos);
            build_pos_atom(pos+2, TYPE_ARG, pre_pos);
            build_pos_atom(pos+3, TYPE_ARG, post_pos);
            return pos;
        }
    }
    return 0;
}

int parse_while_statement() {
    int cond_pos;
    int body_pos;
    int pos;
    if (expect(T_WHILE)) {
        if (!expect(T_LPAREN)) {
            error("no '(' after while");
        }
        cond_pos = parse_expr();
        if (!expect(T_RPAREN)) {
            error("no ')' after while");
        }
        body_pos = parse_block_or_statement();
        if (body_pos != 0) {
            pos = alloc_atom(2);
            build_pos_atom(pos, TYPE_WHILE, body_pos);
            build_pos_atom(pos+1, TYPE_ARG, cond_pos);
            return pos;
        }
    }
    return 0;
}

int parse_do_while_statement() {
    int cond_pos;
    int body_pos;
    int pos;
    if (expect(T_DO)) {
        body_pos = parse_block();
        if (body_pos == 0) {
            error("no block after do");
        }
        if (!expect(T_WHILE)) {
            error("no 'while' after do block");
        }
        if (!expect(T_LPAREN)) {
            error("no '(' after while");
        }
        cond_pos = parse_expr();
        if (!expect(T_RPAREN)) {
            error("no ')' after while");
        }
        pos = alloc_atom(2);
        build_pos_atom(pos, TYPE_DO_WHILE, body_pos);
        build_pos_atom(pos+1, TYPE_ARG, cond_pos);
        return pos;
    }
    return 0;
}


int parse_print_statement() {
    int pos;

    if (expect(T_PRINTI)) {
        pos = parse_expr();
        if (pos != 0 && expect(T_SEMICOLON)) {
            int oppos = alloc_pos_atom(TYPE_PRINTI, pos);
            debug_i("parse_print_statement: parsed @", oppos);
            return oppos;
        }
    }
    return 0;
}

int parse_statement() {
    if (expect(T_SEMICOLON)) {
        return alloc_int_atom(TYPE_NOP, 0);
    } 
    int pos = parse_print_statement();
    if (pos == 0) {
        pos = parse_if_statement();
    } 
    if (pos == 0) {
        pos = parse_for_statement();
    } 
    if (pos == 0) {
        pos = parse_while_statement();
    } 
    if (pos == 0) {
        pos = parse_do_while_statement();
    } 
    if (pos == 0) {
        pos = parse_expr_statement();
    }
    return pos;
}

int parse_var_declare() {
    char *ident;
    if (!expect(T_TYPE_INT)) {
        return 0;
    }

    if (!expect_ident(&ident)) {
        error("parse_var_declare: invalid name");
    }
    if (!expect(T_SEMICOLON)) {
        error("parse_var_declare: no ;");
    }
    add_var(ident);
    debug_s("parse_var_declare: parsed: ", ident);
    return 1;
}

int parse_block();

int parse_block_or_statement() {
    int pos = parse_statement();
    if (pos == 0) {
        pos = parse_block();
    }
    return pos;
}

int parse_block() {
    int prev_pos = 0;
    int pos;

    if (!expect(T_LBLACE)) {
        return 0;
    }

    enter_var_frame();
    for (;;) {
        if (parse_var_declare() == 0) {
            break;
        }
    }

    for (;;) {
        int new_pos = parse_block_or_statement();
        if (new_pos == 0) {
            break;
        }
        if (prev_pos != 0) {
            pos = alloc_binop_atom(TYPE_ANDTHEN, prev_pos, new_pos);
        } else {
            pos = new_pos;
        }
        prev_pos = pos;
    }

    if (!expect(T_RBLACE)) {
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

