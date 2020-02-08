#include "rsys.h"
#include "rstring.h"
#include "devtool.h"

#include "types.h"
#include "type.h"

#include "token.h"
#include "var.h"
#include "func.h"
#include "atom.h"
#include "gstr.h"

int parse_expr();
int parse_primary();

int parse_int() {
    int value;
    if (expect_int(&value)) {
        return alloc_typed_int_atom(TYPE_INT, value, find_type("int"));
    }
    return 0;
}

int parse_signed_int() {
    int pos;
    int start_pos = get_token_pos();
    if (expect(T_PLUS)) {
        return parse_int();
    }

    if (expect(T_MINUS)) {
        pos = parse_int();
        if (!pos) {
            set_token_pos(start_pos);
            return 0;
        }
        return alloc_binop_atom(TYPE_SUB, alloc_typed_int_atom(TYPE_INT, 0, find_type("int")), pos);
    }

    return 0;
}

int parse_string() {
    char *s;
    if (expect_string(&s)) {
        int index = add_global_string(s);
        return alloc_typed_int_atom(TYPE_STRING, index, add_pointer_type(find_type("char")));
    }
    return 0;
}

int parse_char() {
    char value;
    if (expect_char(&value)) {
        return alloc_typed_int_atom(TYPE_INT, value, find_type("char"));
    }
    return 0;
}

int parse_literal() {
    int pos;

    pos = parse_string();
    if (pos) return pos;

    pos = parse_char();
    if (pos) return pos;

    pos = parse_int();
    if (pos) return pos;

    pos = parse_signed_int();
    if (pos) return pos;

    return 0;
}

var_t *parse_var_name() {
    int pos = get_token_pos();
    char *ident;
    if (!expect_ident(&ident)) {
        return 0;
    }
    var_t *v = find_var(ident);
    if (!v) {
        debug_s("variable not declared: ", ident);
        set_token_pos(pos);
        return 0;
    }
    return v;
}

int parse_var() {
    var_t *v = parse_var_name();
    if (v) {
        return alloc_var_atom(v);
    }
    return 0;
}

int parse_primary() {
    int pos;
    pos = parse_literal();
    if (pos) return pos;

    pos = parse_var();
    if (pos) return pos;

    if (expect(T_LPAREN)) {
        pos = parse_expr();
        if (!pos) {
            error("Invalid expr within '()'");
        }
        if (!expect(T_RPAREN)) {
            error("no ')' after '('");
        }
        return pos;
    }
    return 0;
}

func *parse_func_name() {
    int pos = get_token_pos();
    char *ident;
    if (!expect_ident(&ident)) {
        return 0;
    }
    func *f = find_func_name(ident);
    if (!f) {
        debug_s("function not declared: ", ident);
        set_token_pos(pos);
        return 0;
    }
    return f;
}

int parse_apply_func() {
    int pos;
    func *f = parse_func_name();
    if (!f) {
        return 0;
    }
    if (!expect(T_LPAREN)) {
        error("no '(' after func name");
    }

    pos = alloc_func_atom(f);
    for (int i=1; i<f->argc + 1; i++) {
        int arg_pos;
        if (i != 1 && !expect(T_COMMA)) {
            error("no comma between args");
        }
        arg_pos = parse_expr();
        build_pos_atom(pos+i, TYPE_ARG, arg_pos);
    }

    if (!expect(T_RPAREN)) {
        error("too much args or no ')' after func name");
    }
    return pos;
}

int parse_postfix_array(int pos) {
    if(expect(T_LBRACKET)) {
        int index = parse_expr();
        if (!index) {
            error("invalid array index");
        }
        if(!expect(T_RBRACKET)) {
            error("no closing ]");
        }
        return alloc_index_atom(pos, index);
    }
    return pos;
}

int parse_struct_member(int pos) {
    if (expect(T_PERIOD)) {
        char *name;
        if (!expect_ident(&name)) {
            error("invalid member name");
        }
        member_t *m = find_struct_member(program[pos].t, name);
        if (!m) {
            error_s("no member: ", name);
        }
        int lval = atom_to_lvalue(pos);
        if (!lval) {
            error("item is not struct");
        }
        return alloc_offset_atom(lval, m->t, m->offset);
    } else if (expect(T_ALLOW)) {
        char *name;
        if (!expect_ident(&name)) {
            error("invalid member name");
        }
        member_t *m = find_struct_member(program[pos].t->ptr_to, name);
        if (!m) {
            error_s("no member: ", name);
        }
        return alloc_offset_atom(pos, m->t, m->offset);
    }
    return pos;
}

int parse_postfix_incdec(int pos) {
    int op_type = 0;

    if (expect(T_INC)) {
        op_type = TYPE_POSTFIX_INC;
    } else if(expect(T_DEC)) {
        op_type = TYPE_POSTFIX_DEC;
    }
    if (op_type) {
        return alloc_postincdec_atom(op_type, pos);
    }
    return pos;
}

int parse_postfix() {
    int pos;

    pos = parse_primary();
    if (!pos) {
        pos = parse_apply_func();
        if (!pos) {
           return 0;
        }
    }

    int prev_pos = 0;
    while (prev_pos != pos) {
        prev_pos = pos;
        pos = parse_struct_member(pos);
        pos = parse_postfix_array(pos);
        pos = parse_postfix_incdec(pos);
    }

    return pos;
}

int parse_prefix_incdec() {
    int pos;
    int op_type = 0;
    if (expect(T_INC)) {
        op_type = TYPE_ADD;
    } else if (expect(T_DEC)) {
        op_type = TYPE_SUB;
    } else {
        return 0;
    }
    pos = parse_primary();
    if (!pos) {
        error("Invalid expr after '++'|'--'");
    }
    return alloc_assign_op_atom(op_type, pos, alloc_typed_int_atom(TYPE_INT, 1, find_type("int")));
}

int parse_prefix();

int parse_ptr_deref() {
    int pos;
    if (!expect(T_ASTERISK)) {
        return 0;
    }
    pos = parse_prefix();
    if (pos == 0) {
        error("invalid expr after *");
    }
    return alloc_deref_atom(pos);
}

int parse_ptr() {
    int pos;
    if (!expect(T_AMP)) {
        return 0;
    }
    pos = parse_var();
    if (pos == 0) {
        error("invalid expr after &");
    }
    return alloc_ptr_atom(pos);
}

int parse_signed() {
    int pos;
    if (expect(T_PLUS)) {
        pos = parse_prefix();
        if (!pos) {
            error("Invalid '+'");
        }
        return pos;
    }
    if (expect(T_MINUS)) {
        pos = parse_prefix();
        if (!pos) {
            error("Invalid '-'");
        }
        return alloc_binop_atom(TYPE_SUB, alloc_typed_int_atom(TYPE_INT, 0, find_type("int")), pos);
    }
    return 0;
}

int parse_logical_not() {
    int pos;
    if (expect(T_L_NOT)) {
        pos = parse_primary();
        if (!pos) {
            error("Invalid '!'");
        }
        return alloc_typed_pos_atom(TYPE_LOG_NOT, pos, find_type("int"));
    }
    return 0;
}

int parse_prefix() {
    int pos;

    pos = parse_postfix();
    if (pos) return pos;

    pos = parse_logical_not();
    if (pos) return pos;

    pos = parse_signed();
    if (pos) return pos;

    pos = parse_ptr();
    if (pos) return pos;

    pos = parse_ptr_deref();
    if (pos) return pos;

    pos = parse_prefix_incdec();
    if (pos) return pos;

    return 0;
}

int parse_unary() {
    return parse_prefix();
}

int parse_mul() {
    int lpos = parse_unary();
    if (lpos == 0) {
        return 0;
    }

    for (;;) {
        int rpos;
        int type;
        if (expect(T_ASTERISK)) {
            type = TYPE_MUL;
        } else if (expect(T_SLASH)) {
            type = TYPE_DIV;
        } else if (expect(T_PERCENT)) {
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
        if (expect(T_PLUS)) {
            type = TYPE_ADD;
        } else if (expect(T_MINUS)) {
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

int parse_value() {
    return parse_logical_or();
}

int parse_postfix_assignment(int pos) {
    int op = 0;
    if (expect(T_PLUS_EQUAL)) {
        op = TYPE_ADD;
    } else if (expect(T_MINUS_EQUAL)) {
        op = TYPE_SUB;
    } else if (expect(T_ASTERISK_EQUAL)) {
        op = TYPE_MUL;
    } else if (expect(T_SLASH_EQUAL)) {
        op = TYPE_DIV;
    } else if (expect(T_PERCENT_EQUAL)) {
        op = TYPE_MOD;
    } else if (expect(T_AMP_EQUAL)) {
        op = TYPE_AND;
    } else if (expect(T_PIPE_EQUAL)) {
        op = TYPE_OR;
    } else if (expect(T_HAT_EQUAL)) {
        op = TYPE_XOR;
    }

    if (!op) {
        return pos;
    }
    
    int expr_pos = parse_expr();
    if (!pos) {
        error_i("no expr after assignment postfix", pos);
    }
    return alloc_assign_op_atom(op, pos, expr_pos);
}



int parse_expr() {
    int lval = parse_value();
    if (lval == 0) {
        return 0;
    }

    while (TRUE) {
        if (expect(T_EQUAL)) {
            int rval = parse_expr();
            if (!rval) {
                error("cannot bind - no rvalue");
            }
            if (!atom_same_type(lval, rval)) {
                dump_atom_tree(lval, 0);
                dump_atom_tree(rval, 0);
                error("cannot bind - not same type");
            }
            lval = atom_to_lvalue(lval);
            if (!lval) {
                error("cannot bind - lhs cannot be a lvalue");
            }
            lval = alloc_binop_atom(TYPE_BIND, rval, lval);
        } else {
            int pos = parse_postfix_assignment(lval);
            if (pos == lval) {
                break;
            }
            lval = pos;
        }
    }
    return lval;
}

int parse_expr_statement() {
    int pos;

    pos = parse_expr();
    if (pos != 0 && expect(T_SEMICOLON)) {
        return alloc_typed_pos_atom(TYPE_EXPR_STATEMENT, pos, find_type("void"));
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

    if (expect(T_PRINT)) {
        if (!expect(T_LPAREN)) return 0;
        pos = parse_expr();
        if (pos != 0 && expect(T_RPAREN) && expect(T_SEMICOLON)) {
            int oppos = alloc_typed_pos_atom(TYPE_PRINT, pos, find_type("void"));
            debug_i("parse_print_statement: parsed @", oppos);
            return oppos;
        }
    }
    return 0;
}

int parse_return_statement() {
    int pos;

    if (expect(T_RETURN)) {
        pos = parse_expr();
        if (pos != 0 && expect(T_SEMICOLON)) {
            int oppos = alloc_typed_pos_atom(TYPE_RETURN, pos, find_type("void"));
            return oppos;
        }
    }
    return 0;
}

int parse_statement() {
    if (expect(T_SEMICOLON)) {
        return alloc_typed_int_atom(TYPE_NOP, 0, find_type("void"));
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
        pos = parse_return_statement();
    } 
    if (pos == 0) {
        pos = parse_expr_statement();
    }
    return pos;
}

type_t *parse_primitive_type() {
    type_t *t;
    char *type_name;
    int pos = get_token_pos();

    if (!expect_ident(&type_name)) {
        debug("parse_var_declare: not ident");
        return 0;
    }
    if (0 == (t = find_type(type_name))) {
        debug_s("parse_var_declare: not type name: ", type_name);
        set_token_pos(pos);
        return 0;
    };
    return t;
}

type_t *parse_type_declare();

type_t *parse_var_array_declare(type_t *t) {
    if (!expect(T_LBRACKET)) {
        return t;
    }

    int length = 0;
    expect_int(&length);
    debug_i("array length:", length);

    if (!expect(T_RBRACKET)) {
        error("array declarator doesn't have closing ]");
    }

    return add_array_type(parse_var_array_declare(t), length);
}

int parse_struct_member_declare(type_t *st) {
    char *ident;

    type_t *t = parse_type_declare();
    if (!t) {
        return 0;
    }

    if (!expect_ident(&ident)) {
        error("parse_var_declare: invalid name");
    }

    t = parse_var_array_declare(t);

    if (!expect(T_SEMICOLON)) {
        error("parse_var_declare: no ;");
    }

    add_struct_member(st, ident, t);
    debug_s("parse_var_declare: parsed: ", ident);

    return alloc_typed_int_atom(TYPE_NOP, 0, find_type("void"));
}

type_t *parse_struct_type() {
    type_t *t;
    char *type_name;

    if (!expect(T_STRUCT)) {
        return 0;
    }

    if (expect_ident(&type_name)) {
        debug_s("struct name: ", type_name);
        t = add_struct_type(type_name);
    } else {
        t = add_struct_type("---");
    }

    if (expect(T_LBLACE)) {
        debug("parsing struct member...");
        while (parse_struct_member_declare(t) != 0) {
        }
        if (!expect(T_RBLACE)) {
            error_s("struct no close } : ", type_name);
        }
    }

    return t;
}

type_t *parse_type_declare() {
    type_t *t;

    t = parse_struct_type();
    if (!t) {
        t = parse_primitive_type();
    }
    if (!t) {
        return 0;
    }

    while (expect(T_ASTERISK)) {
        t = add_pointer_type(t);
    }
    return t;
}

int parse_func_var_declare() {
    char *ident;

    type_t *t = parse_type_declare();
    if (!t) {
        return 0;
    }

    if (!expect_ident(&ident)) {
        error("parse_var_declare: invalid name");
    }
    add_var(ident, t);
    debug_s("parse_var_declare: parsed: ", ident);
    return 1;
}

int parse_func_args() {
    int argc = 0;

    if (!parse_func_var_declare()) {
        return 0;
    }
    argc++;

    while (expect(T_COMMA)) {
        if (!parse_func_var_declare()) {
            error("Invalid argv");
        }
        argc++;
    }
    return argc;
}

int parse_var_declare() {
    char *ident;

    type_t *t = parse_type_declare();
    if (!t) {
        return 0;
    }

    if (!expect_ident(&ident)) {
        error("parse_var_declare: invalid name");
    }

    t = parse_var_array_declare(t);

    if (!expect(T_SEMICOLON)) {
        error("parse_var_declare: no ;");
    }

    add_var(ident, t);
    debug_s("parse_var_declare: parsed: ", ident);

    return alloc_typed_int_atom(TYPE_NOP, 0, find_type("void"));
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

    if (!expect(T_LBLACE)) {
        return 0;
    }

    enter_var_frame();
    for (;;) {
        int new_pos = parse_var_declare();
        if (!new_pos) {
            break;
        }
        if (prev_pos) {
            prev_pos = alloc_binop_atom(TYPE_ANDTHEN, prev_pos, new_pos);
        } else {
            prev_pos = new_pos;
        }
    }

    for (;;) {
        int new_pos = parse_block_or_statement();
        if (!new_pos) {
            break;
        }
        if (prev_pos) {
            prev_pos = alloc_binop_atom(TYPE_ANDTHEN, prev_pos, new_pos);
        } else {
            prev_pos = new_pos;
        }
    }

    if (!expect(T_RBLACE)) {
        error("parse_block: not found '}'");
    }

    exit_var_frame();
    if (prev_pos == 0) {
        return alloc_typed_int_atom(TYPE_NOP, 0, find_type("void"));
    }
    return prev_pos;
}

int parse_function() {
    int pos = get_token_pos();

    type_t *t = parse_type_declare();
    if (!t) {
        set_token_pos(pos);
        return 0;
    };

    char *ident;
    if (!expect_ident(&ident)) {
        error("parse_function: invalid name");
    }
    
    if (!expect(T_LPAREN)) {
        debug("parse_function failed: no '('");
        set_token_pos(pos);
        return 0;
    }

    reset_var_max_offset();
    enter_var_frame();

    parse_func_args();

    if (!expect(T_RPAREN)) {
        error("parse_function: no ')'");
    }

    frame_t *frame = get_top_frame();
    func *f = add_function(ident, t, frame->num_vars, frame->vars);

    int body_pos = parse_block();
    if (!body_pos) {
        error_s("No body for function: ", ident);
    }

    func_set_body(f, body_pos, var_max_offset());

    exit_var_frame();
    return 1;
}

void parse() {

    enter_var_frame();
    while (!expect(T_EOF)) {
        int pos;
        pos = parse_function();
        if (pos) continue;
        pos = parse_var_declare();
        if (pos) continue;
    }
    exit_var_frame();
}
