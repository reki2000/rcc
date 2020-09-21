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
int parse_prefix();
int parse_unary();
type_t *parse_type_declaration();
type_t *parse_pointer(type_t *t);
int parse_local_variable_declaration();
bool expect_int_expr(int *v);

bool expect_enum_member(int *v) {
    int pos = get_token_pos();
    char *ident;
    if (expect_ident(&ident)) {
        var_t *var = find_var(ident);
        if (var && var->t->enum_of && var->is_constant) {
            *v = var->int_value;
            return TRUE;
        }
    }
    set_token_pos(pos);
    return FALSE;
}

bool expect_int_unary(int *v) {
    if (expect_int(v)) {
        return TRUE;
    }

    char ch;
    if (expect_char(&ch)) {
        *v = (int)ch;
        return TRUE;
    }

    return expect_enum_member(v);
}

bool expect_int_factor(int *v) {
    int pos = get_token_pos();
    if (expect(T_LPAREN)) {
        int v2;
        if (!expect_int_expr(&v2)) {
            set_token_pos(pos);
            return FALSE;
        }
        if (!expect(T_RPAREN)) {
            error("invalid end of constant expression");
        }
        *v = v2;
        return TRUE;
    }

    if (expect(T_PLUS)) {
        return expect_int_unary(v);
    }
    
    if (expect(T_MINUS)) {
        int v2;
        if(!expect_int_unary(&v2)) {
            set_token_pos(pos);
            return FALSE;
        }
        *v = -v2;
        return TRUE;
    }
    return expect_int_unary(v);
}

bool expect_int_term(int *v) {
    int pos = get_token_pos();
    if (!expect_int_factor(v)) {
        set_token_pos(pos);
        return FALSE;
    }

    pos = get_token_pos();
    for (;;) {
        int op;
        if (expect(T_ASTERISK)) {
            op = 0;
        } else if (expect(T_SLASH)) {
            op = 1;
        } else if (expect(T_PERCENT)) {
            op = 2;
        } else {
            break;
        }
        int v2;
        if (!expect_int_factor(&v2)) {
            set_token_pos(pos);
            return TRUE;
        }
        if (op == 0) {
            *v *= v2;
        } else if (op == 1) {
            *v /= v2;
        } else if (op == 2) {
            *v %= v2;
        }
    }
    return TRUE;
}

bool expect_int_expr(int *v) {
    int pos = get_token_pos();
    if (!expect_int_term(v)) {
        set_token_pos(pos);
        return FALSE;
    }

    pos = get_token_pos();
    for (;;) {
        bool op_is_plus = TRUE;
        if (expect(T_MINUS)) {
            op_is_plus = FALSE;
        } else if (!expect(T_PLUS)) {
            break;
        }
        int v2;
        if (!expect_int_term(&v2)) {
            set_token_pos(pos);
            return TRUE;
        }
        debug_i("found added constant:", *v);
        debug_i("found added constant:", v2);
        if (op_is_plus) {
            *v += v2;
        } else {
            *v -= v2;
        }
        debug_i("found added constant:", *v);
    }
    return TRUE;
}

int parse_int_expr() {
    int value;
    int pos = get_token_pos();
    if (expect_int_expr(&value)) {
        return alloc_typed_int_atom(TYPE_INTEGER, value, find_type("int"));
    }
    set_token_pos(pos);
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

int parse_int_literal() {
    return parse_int_expr();
}

int parse_literal() {
    int pos;
    pos = parse_string();
    if (pos) return pos;

    int p = get_token_pos();
    pos = parse_int_literal();
    if (!pos) {
        set_token_pos(p);
        return 0;
    }

    return pos;
}

var_t *parse_var_name() {
    int pos = get_token_pos();
    char *ident;
    if (!expect_ident(&ident)) {
        return 0;
    }
    var_t *v = find_var(ident);
    if (!v) {
        set_token_pos(pos);
        return 0;
    }
    return v;
}

int parse_var() {
    var_t *v = parse_var_name();
    if (v) {
        if (v->is_constant) {
            return alloc_typed_int_atom(TYPE_INTEGER, v->int_value, find_type("int"));
        } else {
            return alloc_var_atom(v);
        }
    }
    return 0;
}

int parse_primary() {
    int pos;
    int start_pos = get_token_pos();
    pos = parse_literal();
    if (pos) return pos;

    pos = parse_var();
    if (pos) return pos;

    if (expect(T_LPAREN)) {
        pos = parse_expr();
        if (!pos) {
            set_token_pos(start_pos);
            return 0;
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
        build_pos_atom(pos+i, TYPE_ARG, atom_to_rvalue(arg_pos));
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
    } else if (expect(T_ALLOW)) {
        pos = alloc_deref_atom(pos);
    } else {
        return pos;
    }
    char *name;
    if (!expect_ident(&name)) {
        error("invalid member name");
    }
    type_t *t = program[pos].t->ptr_to;
    member_t *m = find_struct_member(t, name);
    if (!m) {
        error_s("this type doesn't has member: ", name);
    }
    return alloc_offset_atom(pos, m->t, m->offset);
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
    return alloc_assign_op_atom(op_type, pos, alloc_typed_int_atom(TYPE_INTEGER, 1, find_type("int")));
}


int parse_ptr_deref() {
    int pos;
    if (!expect(T_ASTERISK)) {
        return 0;
    }
    pos = parse_unary();
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
    pos = parse_unary();
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
        return alloc_binop_atom(TYPE_SUB, alloc_typed_int_atom(TYPE_INTEGER, 0, find_type("int")), atom_to_rvalue(pos));
    }
    return 0;
}

int parse_sizeof() {
    int pos = 0;
    if (!expect(T_SIZEOF)) {
        return 0;
    }

    int start_pos = get_token_pos();
    if (expect(T_LPAREN)) {
        type_t *t = parse_type_declaration();
        if (t) {
            t = parse_pointer(t);
            if (!expect(T_RPAREN)) {
                error("no () after sizeof");
            }
            pos = alloc_typed_int_atom(TYPE_INTEGER, t->size, find_type("int"));
        } else {
            set_token_pos(start_pos);
        }
    }
    if (!pos) {
        pos = parse_unary();
        if (pos) {
            if (program[pos].t->array_length < 0) {
                pos = atom_to_rvalue(pos);
            }
            int size = program[pos].t->size;
            pos = alloc_typed_int_atom(TYPE_INTEGER, size, find_type("int"));
        } else {
            error("invliad expr after sizeof");
        }
    }
    return pos;
}

int parse_bitwise_not() {
    int pos;
    if (expect(T_TILDE)) {
        pos = parse_unary();
        if (!pos) {
            error("Invalid '~'");
        }
        return alloc_typed_pos_atom(TYPE_NEG, atom_to_rvalue(pos), find_type("int"));
    }
    return 0;
}

int parse_logical_not() {
    int pos;
    if (expect(T_L_NOT)) {
        pos = parse_unary();
        if (!pos) {
            error("Invalid '!'");
        }
        return alloc_typed_pos_atom(TYPE_LOG_NOT, atom_to_rvalue(pos), find_type("int"));
    }
    return 0;
}

int parse_cast() {
    int pos = get_token_pos();
    if (!expect(T_LPAREN)) {
        return 0;
    }

    type_t *t = parse_type_declaration();
    if (!t) {
        set_token_pos(pos);
        return 0;
    }

    t = parse_pointer(t);

    if (!expect(T_RPAREN)) {
        error("invalid end of cast");
    }

    pos = parse_unary();
    if (!pos) {
        error("invalid cast");
    }
    return alloc_typed_pos_atom(TYPE_CAST, atom_to_rvalue(pos), t);
}

int parse_prefix() {
    int pos;

    pos = parse_postfix();
    if (pos) return pos;

    pos = parse_cast();
    if (pos) return pos;

    pos = parse_sizeof();
    if (pos) return pos;

    pos = parse_logical_not();
    if (pos) return pos;

    pos = parse_bitwise_not();
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

        lpos = alloc_binop_atom(type, atom_to_rvalue(lpos), atom_to_rvalue(rpos));
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
        lpos =  alloc_binop_atom(type, atom_to_rvalue(lpos), atom_to_rvalue(rpos));
    }
    return lpos;
}

int parse_bitshift() {
    int lpos = parse_add();
    if (lpos == 0) {
        return 0;
    }

    for (;;) {
        int rpos;
        int type;
    if (expect(T_LSHIFT)) {
            type = TYPE_LSHIFT;
        } else if (expect(T_RSHIFT)) {
            type = TYPE_RSHIFT;
        } else {
            break;
        }
        rpos = parse_add();
        if (rpos == 0) {
            return 0;
        }
        lpos =  alloc_binop_atom(type, atom_to_rvalue(lpos), atom_to_rvalue(rpos));
    }
    return lpos;
}

int parse_lessgreat() {
    int lpos = parse_bitshift();
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
        int rpos = parse_bitshift();
        if (rpos == 0) {
            error("Inalid rval for lessthan");
        }
        lpos = alloc_binop_atom(type_eq, atom_to_rvalue(lpos), atom_to_rvalue(rpos));
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
        lpos = alloc_binop_atom(type_eq, atom_to_rvalue(lpos), atom_to_rvalue(rpos));
    }
    return lpos;
}

int parse_bitwise_and() {
    int lpos = parse_equality();
    if (lpos == 0) {
        return 0;
    }
    for (;;) {
        if (!expect(T_AMP)) {
            break;
        }
        int rpos = parse_equality();
        if (rpos == 0) {
            error("Inalid rval for &(bitwise and)");
        }
        lpos = alloc_binop_atom(TYPE_AND, atom_to_rvalue(lpos), atom_to_rvalue(rpos));
    }
    return lpos;
}

int parse_bitwise_xor() {
    int lpos = parse_bitwise_and();
    if (lpos == 0) {
        return 0;
    }
    for (;;) {
        if (!expect(T_HAT)) {
            break;
        }
        int rpos = parse_bitwise_and();
        if (rpos == 0) {
            error("Inalid rval for ^(bitwise xor)");
        }
        lpos = alloc_binop_atom(TYPE_XOR, atom_to_rvalue(lpos), atom_to_rvalue(rpos));
    }
    return lpos;
}

int parse_bitwise_or() {
    int lpos = parse_bitwise_xor();
    if (lpos == 0) {
        return 0;
    }
    for (;;) {
        if (!expect(T_PIPE)) {
            break;
        }
        int rpos = parse_bitwise_xor();
        if (rpos == 0) {
            error("Inalid rval for |(bitwise or)");
        }
        lpos = alloc_binop_atom(TYPE_OR, atom_to_rvalue(lpos), atom_to_rvalue(rpos));
    }
    return lpos;
}

int parse_logical_and() {
    int lpos = parse_bitwise_or();
    if (lpos == 0) {
        return 0;
    }
    for (;;) {
        if (!expect(T_L_AND)) {
            break;
        }
        int rpos = parse_bitwise_or();
        if (rpos == 0) {
            error("Inalid rval for &&");
        }
        lpos = alloc_binop_atom(TYPE_LOG_AND, atom_to_rvalue(lpos), atom_to_rvalue(rpos));
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
        lpos = alloc_binop_atom(TYPE_LOG_OR, atom_to_rvalue(lpos), atom_to_rvalue(rpos));
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
    } else if (expect(T_LSHIFT_EQUAL)) {
        op = TYPE_LSHIFT;
    } else if (expect(T_RSHIFT_EQUAL)) {
        op = TYPE_RSHIFT;
    } else if (expect(T_TILDE_EQUAL)) {
        op = TYPE_NEG;
    }

    if (!op) {
        return pos;
    }
    
    int expr_pos = parse_expr();
    if (!pos) {
        error_i("no expr after assignment postfix", pos);
    }
    return alloc_assign_op_atom(op, pos, atom_to_rvalue(expr_pos));
}

int parse_variable_initializer(int lhs) {
    int rhs = parse_expr();
    if (!rhs) {
        error("cannot bind - no rvalue");
    }
    rhs = atom_convert_type(atom_to_rvalue(lhs), atom_to_rvalue(rhs));
    return alloc_binop_atom(TYPE_BIND, rhs, lhs);
}

int parse_assignment(int lval) {
    if (expect(T_EQUAL)) {
        return parse_variable_initializer(lval);
    }
    return 0;
}

int parse_ternary(int eq_pos) {
    if (expect(T_QUESTION)) {
        int val1 = parse_expr();
        if (!val1) {
            error("invalid end of ternary operator");
        }
        if (!expect(T_COLON)) {
            error("no colon for ternary operator");
        }
        int val2 = parse_expr();
        if (!val2) {
            error("nosecond value for ternary operator");
        }
        int first = atom_to_rvalue(val1);
        type_t *first_t = program[first].t;
        int second = atom_to_rvalue(val2);
        type_t *second_t = program[second].t;

        int pos = alloc_typed_pos_atom(TYPE_TERNARY, atom_to_rvalue(eq_pos), first_t);
        alloc_typed_pos_atom(TYPE_ARG, first, first_t);
        alloc_typed_pos_atom(TYPE_ARG, second, second_t);
        return pos;
    }
    return 0;
}

int parse_expr() {
    int lval = parse_value();
    if (!lval) {
        return 0;
    }

    while (TRUE) {
        int lval_prev = lval;
        int new_lval = parse_ternary(lval);
        if (new_lval) {
            lval = new_lval;
            continue;
        }

        new_lval = parse_assignment(lval);
        if (new_lval) {
            lval = new_lval;
            continue;
        }
        lval = parse_postfix_assignment(lval);
        if (lval == lval_prev) {
            break;
        }
    }

    return lval;
}

int parse_expr_sequence() {
    int lval = parse_expr();
    if (!lval) {
        return 0;
    }

    lval = atom_to_rvalue(lval);
    while (expect(T_COMMA)) {
        int pos = parse_expr();
        if (!pos) {
            error("no expression after comma");
        }
        lval = alloc_binop_atom(TYPE_ANDTHEN, lval, atom_to_rvalue(pos));
    }
    return lval;
}

int parse_expr_statement() {
    int pos;

    pos = parse_expr_sequence();
    if (pos != 0 && expect(T_SEMICOLON)) {
        return alloc_typed_pos_atom(TYPE_EXPR_STATEMENT, pos, find_type("void"));
    }
    return 0;
}

int parse_block_or_statement();
int parse_block_or_statement_series();
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
        eq_pos = parse_expr_sequence();
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

int parse_case_clause() {
    if (expect(T_CASE)) {
        int cond_pos = parse_int_literal();
        if (!cond_pos) {
            error("no target value for 'case'");
        }

        if (!expect(T_COLON)) {
            error("colon ':' is needed after case target");
        }

        int body_pos = parse_block_or_statement_series();
        int pos = alloc_atom(2);
        build_pos_atom(pos, TYPE_CASE, cond_pos);
        build_pos_atom(pos+1, TYPE_ARG, body_pos);
        return pos;
    }
    return 0;
}

int parse_default_clause() {
    if (expect(T_DEFAULT)) {
        if (!expect(T_COLON)) {
            error("colon ':' is needed after 'default'");
        }
        return alloc_typed_pos_atom(TYPE_DEFAULT, parse_block_or_statement_series(), find_type("void"));
    }
    return 0;
}

#define NUM_CASE_CLAUSES 200

int parse_switch_statement() {
    if (!expect(T_SWITCH)) {
        return 0;
    }
    if (!expect(T_LPAREN)) {
        error("no target expression for 'switch'");
    }
    int pos = parse_expr_sequence();
    if (!pos) {
        error("invlid target expression for 'switch'");
    }
    if (!expect(T_RPAREN)) {
        error("invalid end of expression for 'switch'");
    }
    if (!expect(T_LBLACE)) {
        error("no body for 'switch'");
    }
    int body_pos[NUM_CASE_CLAUSES + 1];
    int n;
    for (n=1; n<NUM_CASE_CLAUSES; n++) {
        int pos_case = parse_case_clause();
        if (!pos_case) {
            break;
        }
        body_pos[n] = pos_case;
    }
    int pos_default = parse_default_clause();
    if (pos_default) {
        body_pos[n] = pos_default;
    } else {
        n--;
    }
    if (!expect(T_RBLACE)) {
        error("invalid end of 'switch' body");
    }
    int pos2 = alloc_atom(n + 1);
    build_pos_atom(pos2, TYPE_SWITCH, pos);
    for (int i=1; i<=n; i++) {
        build_pos_atom(pos2+i, TYPE_ARG, body_pos[i]);
    }
    return pos2;
}

int wrap_expr_sequence(int pos) {
    if (!pos) {
        return alloc_nop_atom();
    } else {
        return  alloc_typed_pos_atom(TYPE_EXPR_STATEMENT, pos, find_type("void"));
    }
}

int parse_for_statement() {
    int pre_pos;
    int cond_pos;
    int post_pos;
    int body_pos;
    int pos;
    if (!expect(T_FOR)) {
        return 0;
    }
    if (!expect(T_LPAREN)) {
        error("no contition part after 'for'");
    }

    enter_var_frame();

    pre_pos = parse_local_variable_declaration();
    if (!pre_pos) {
        pre_pos = parse_expr_statement();
    }
    if (!pre_pos) {
        if (expect(T_SEMICOLON)) {
            pre_pos = alloc_nop_atom();
        } else {
            error("invalid end of the first part of 'for' conditions");
        }
    }
    cond_pos = parse_expr_sequence();
    if (!cond_pos) {
        cond_pos = alloc_typed_int_atom(TYPE_INTEGER, 1, find_type("int")); // TRUE
    }   
    if (!expect(T_SEMICOLON)) {
        error("invalid end of the second part of 'for' conditions");
    }
    post_pos = wrap_expr_sequence(parse_expr_sequence());
    if (!expect(T_RPAREN)) {
        error("invalid end of 'for' conditions");
    }
    body_pos = parse_block_or_statement();
    exit_var_frame();
    if (!body_pos) {
        return 0;
    }

    pos = alloc_atom(4);
    build_pos_atom(pos, TYPE_FOR, body_pos);
    build_pos_atom(pos+1, TYPE_ARG, cond_pos);
    build_pos_atom(pos+2, TYPE_ARG, pre_pos);
    build_pos_atom(pos+3, TYPE_ARG, post_pos);
    return pos;
}

int parse_while_statement() {
    int cond_pos;
    int body_pos;
    int pos;
    if (expect(T_WHILE)) {
        if (!expect(T_LPAREN)) {
            error("no contition part after 'while'");
        }
        cond_pos = parse_expr_sequence();
        if (!expect(T_RPAREN)) {
            error("invalid end of 'while' condition");
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
            error("no block after do-while's 'do'");
        }
        if (!expect(T_WHILE)) {
            error("no 'while' after do-while body");
        }
        if (!expect(T_LPAREN)) {
            error("no condition for do-while");
        }
        cond_pos = parse_expr_sequence();
        if (!expect(T_RPAREN)) {
            error("invalid end of do-while contidion");
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
            int oppos = alloc_typed_pos_atom(TYPE_PRINT, atom_to_rvalue(pos), find_type("void"));
            return oppos;
        }
    }
    return 0;
}

int parse_break_statement() {
    if (expect(T_BREAK)) {
        if (expect(T_SEMICOLON)) {
            return alloc_typed_pos_atom(TYPE_BREAK, 0, find_type("void"));
        }
    }
    return 0;
}

int parse_continue_statement() {
    if (expect(T_CONTINUE)) {
        if (expect(T_SEMICOLON)) {
            return alloc_typed_pos_atom(TYPE_CONTINUE, 0, find_type("void"));
        }
    }
    return 0;
}

int parse_return_statement() {
    int pos;

    if (!expect(T_RETURN)) {
        return 0;
    }

    pos = parse_expr_sequence();
    if (!pos) {
        pos = alloc_typed_int_atom(TYPE_INTEGER, 0, find_type("void"));
    }
    if (!expect(T_SEMICOLON)) {
        error("invalid expr for return");
    }
    return alloc_typed_pos_atom(TYPE_RETURN, pos, find_type("void"));
}

int parse_statement() {
    if (expect(T_SEMICOLON)) {
        return alloc_nop_atom();
    } 
    int pos = parse_local_variable_declaration();
    if (pos == 0) {
        pos = parse_print_statement();
    }
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
        pos = parse_break_statement();
    } 
    if (pos == 0) {
        pos = parse_continue_statement();
    } 
    if (pos == 0) {
        pos = parse_switch_statement();
    } 
    if (pos == 0) {
        pos = parse_expr_statement();
    }
    return pos;
}


type_t *parse_type_declaration();
type_t *parse_pointer(type_t *);


type_t *parse_primitive_type() {
    type_t *t;
    char *type_name;
    int pos = get_token_pos();

    if (!expect_ident(&type_name)) {
        debug("parse_primitive_type: not ident");
        return 0;
    }
    if (0 == (t = find_type(type_name))) {
        debug_s("parse_primitive_type: not type name: ", type_name);
        set_token_pos(pos);
        return 0;
    };
    return t;
}

type_t *parse_var_array_declare(type_t *t) {
    if (!expect(T_LBRACKET)) {
        return t;
    }

    int length = 0;
    expect_int_expr(&length);
    if (!expect(T_RBRACKET)) {
        error("array declarator doesn't have closing ]");
    }

    return add_array_type(parse_var_array_declare(t), length);
}

type_t *parse_enum_type() {
    type_t *t;
    char *tag_name;

    if (!expect(T_ENUM)) {
        return 0;
    }

    if (expect_ident(&tag_name)) {
        t = add_enum_type(tag_name);
    } else {
        t = add_enum_type("");
    }

    if (expect(T_LBLACE)) {
        debug_s("parsing members for enum : ", t->enum_of->name);
        for (;;) {
            char *member_name;
            if (!expect_ident(&member_name)) {
                error("enum member: expected identifier");
            }
            int value = t->enum_of->next_value++;

            if (expect(T_EQUAL)) {
                if (!expect_int(&value)) {
                    error_s("invalid value for enum member: ", member_name);
                }
                t->enum_of->next_value = value + 1;
                debug_i("enum value assigned: ", value);
            }
            add_constant_int(member_name, t, value);

            if (expect(T_COMMA)) {
                if (expect(T_RBLACE)) {
                    break;
                }
                continue;
            } else if (expect(T_RBLACE)) {
                break;
            } else {
                error("syntax error in enum member declaration");
            }
        }
    }
    return t;
}


int parse_struct_member_declare(type_t *st) {
    char *ident;

    type_t *t = parse_type_declaration();
    if (!t) {
        return 0;
    }
    t = parse_pointer(t);

    if (!expect_ident(&ident)) {
        if (t->struct_of) {
            struct_t *u = t->struct_of;
            if (u->is_union && u->is_anonymous) {
                if (!expect(T_SEMICOLON)) {
                    error("parse_var_declare: no ;");
                }
                copy_union_member_to_struct(st, t);
            } else {
                error("parse_var_declare: no variable name after union declaration");
            }
        } else {
            error("parse_var_declare: invalid name");
        }
    } else {
        t = parse_var_array_declare(t);

        if (!expect(T_SEMICOLON)) {
            error("parse_var_declare: no ;");
        }

        add_struct_member(st, ident, t, st->struct_of->is_union);
    }
    return 1;
}


type_t *parse_union_or_struct_type() {
    bool is_union;
    if (expect(T_STRUCT)) {
        is_union = FALSE;
    } else if (expect(T_UNION)) {
        is_union = TRUE;
    } else {
        return 0;
    }

    type_t *t;
    char *type_name;
    bool is_anonymous = FALSE;
    if (!expect_ident(&type_name)) {
        type_name = "";
        is_anonymous = TRUE;
    }
    if (is_union) {
        t = add_union_type(type_name, is_anonymous);
    } else {
        t = add_struct_type(type_name, is_anonymous);
    }

    if (expect(T_LBLACE)) {
        while (parse_struct_member_declare(t) != 0) {
        }
        if (!expect(T_RBLACE)) {
            error_s("struct/union no close } : ", type_name);
        }
    }

    return t;
}

type_t *parse_typedef() {
    if (!expect(T_TYPEDEF)) {
        return 0;
    }
    type_t *t = parse_type_declaration();
    if (!t) {
        error("invalid type declaration for typedef");
    }

    char *type_name;
    if (!expect_ident(&type_name)) {
        error("no type name for typedef");
    }

    type_t *defined_type = add_type(type_name, t->size, t->ptr_to, t->array_length);
    defined_type->struct_of = t->struct_of;
    defined_type->enum_of = t->enum_of;
    defined_type->typedef_of = t;

    return defined_type;
}

type_t *parse_defined_type() {
    return 0;
}

type_t *parse_type_declaration() {
    type_t *t;

    t = parse_typedef();
    if (!t) {
        t = parse_enum_type();
    }
    if (!t) {
        t = parse_union_or_struct_type();
    }
    if (!t) {
        t = parse_defined_type();
    }
    if (!t) {
        t = parse_primitive_type();
    }
    return t;

}

type_t *parse_pointer(type_t *t) {
    while (expect(T_ASTERISK)) {
        t = add_pointer_type(t);
    }
    return t;
}

var_t *add_var_with_check(type_t *t, char *name) {
    var_t *v = find_var_in_current_frame(name);
    if (!v) {
        return add_var(name, t);
    }

    if (type_is_same(v->t, t)) {
        if (v->has_value) {
            error_s("variable is already initialized:", v->name);
        }
        return v;
    }

    if (v->t->ptr_to && !type_is_same(v->t->ptr_to, t->ptr_to)) {
        char buf[RCC_BUF_SIZE] = {0};
        strcat(buf, "variable is already declared with different type: ");
        strcat(buf, v->name);
        strcat(buf, " ");
        dump_type(buf, t);
        strcat(buf, " previously ");
        dump_type(buf, v->t);
        error(buf);
    }

    if (v->t->array_length <= 0 && t->array_length >= 0) {
        v->t = t; // declaring 'a[100]' can overwrite 'a[]' or '*a'
        return v;
    }
    debug_i("", v->t->array_length);
    error_i("variable is already declared with different size: ", t->array_length);
    return 0;
}

type_t *parse_local_variable_typepart() {
    expect(T_CONST);

    type_t *t = parse_type_declaration();
    if (!t) {
        return 0;
    }
    t = parse_pointer(t);

    return t;
}

var_t *parse_func_arg_declare() {
    char *ident;

    type_t *t = parse_local_variable_typepart();
    if (!t) {
        return 0;
    }

    if (!expect_ident(&ident)) {
        error("parse_var_declare: invalid name");
    }

    t = parse_var_array_declare(t);

    return add_var_with_check(t, ident);
}

int parse_global_var_initializer() {
    int num = 0;
    if (expect_int_expr(&num)) {
        return num;
    }

    char *str = (void *)0;
    if (expect_string(&str)) {
        return add_global_string(str);
    }

    error("invalid initializer for global variable");
    return 0;
}

int parse_global_var_array_initializer(var_t *v, int array_length) {
    if (!expect(T_LBLACE)) {
        return 0;
    }

    int index;
    int pos = alloc_global_array();
    for (index = 0; array_length == 0 || index < array_length; index++) {
        debug_i("parsing array initializer at index:", index);
        int val = parse_global_var_initializer();
        add_global_array(pos, val);
        if (!expect(T_COMMA)) {
            break;
        }
    }
    if (!get_global_array_length(pos)) {
        error("empty array initializer");
    }
    if (array_length > 0 && expect(T_COMMA)) {
        error("too many array initializer elements");
    }
    if (array_length == 0) {
        type_t *t = add_array_type(v->t->ptr_to, index + 1);
        v->t = t;
    }
    if (!expect(T_RBLACE)) {
        error("Invalid end of array initializer");
    }
    return pos;
}


int parse_global_variable(type_t *t, bool is_external) {
    int pos = get_token_pos();
    t = parse_pointer(t);

    char *ident;
    if (!expect_ident(&ident)) {
        set_token_pos(pos);
        return 0;
    }
    t = parse_var_array_declare(t);

    var_t *v = add_var_with_check(t, ident);
    v->is_external = is_external;

    if (expect(T_EQUAL)) {
        if (v->is_external) {
            debug_s("variable is initialized but delcared as 'extern':", v->name);
        }
        if (v->t->array_length >= 0) {
            v->int_value = parse_global_var_array_initializer(v, v->t->array_length);
        } else {
            v->int_value = parse_global_var_initializer();
        }
        v->has_value = TRUE;
    }

    return pos;
}

int parse_array_initializer(var_t *v, int array, int array_length) {
    if (!expect(T_LBLACE)) {
        return 0;
    }
    int index;
    int pos = 0;

    for (index = 0; array_length == 0 || index < array_length; index++) {
        debug_i("parsing array initializer at index:", index);
        int lval = alloc_index_atom(array, alloc_typed_int_atom(TYPE_INTEGER, index, find_type("int")));
        int assign = parse_variable_initializer(lval);
        if (assign) {
            if (!pos) {
                pos = assign;
            } else {
                pos = alloc_binop_atom(TYPE_ANDTHEN, pos, assign);
            }
        }
        if (!expect(T_COMMA)) {
            break;
        }
    }
    if (!pos) {
        error("emptry array initializer");
    }
    if (array_length > 0 && expect(T_COMMA)) {
        error("too many array initializer elements");
    }
    if (array_length == 0) {
        int old_offset = v->offset;

        type_t *t = add_array_type(v->t->ptr_to, index + 1);
        debug_i("realloc var with size: ", index + 1);
        var_realloc(v, t);
        int new_offset = v->offset;

        // hacky - traversing AST to update offset of base array variable
        atom_t *a = &program[pos];
        while (a) {
            atom_t *bind_atom;
            if (a->type == TYPE_ANDTHEN) {
                bind_atom = &program[(a+1)->int_value];
                a = &program[(a)->int_value];
            } else {
                bind_atom = a;
                a = (void *)0; // exit loop
            }
            if (bind_atom->type == TYPE_BIND) {
                atom_t *array_index_atom = &program[(bind_atom+1)->int_value];
                if (array_index_atom->type == TYPE_ARRAY_INDEX) {
                    atom_t *var_ref_atom = &program[(array_index_atom)->int_value];
                    if (var_ref_atom->type == TYPE_VAR_REF && var_ref_atom->int_value == old_offset) {
                        var_ref_atom->int_value = new_offset;
                        var_ref_atom->t = t;
                    }
                }
            } else {
                break;
            }
        }
    }

    if (!expect(T_RBLACE)) {
        error("Invalid end of array initializer");
    }
    return pos;
}

int parse_local_variable_identifier(type_t *t) {
    char *ident;
    if (!expect_ident(&ident)) {
        error("parse_var_declare: invalid name");
    }

    t = parse_var_array_declare(t);

    var_t *v = add_var_with_check(t, ident);

    int pos = alloc_var_atom(v);
    // dump_atom(pos,0);

    if (expect(T_EQUAL)) {
        if (v->t->array_length >= 0) {
            pos = parse_array_initializer(v, pos, v->t->array_length);
        } else {
            pos = parse_variable_initializer(pos);
        }
    } else {
        pos = alloc_nop_atom();
    }
    return pos;
}

int parse_local_variable_declaration() {

    type_t *t = parse_local_variable_typepart();
    if (!t) {
        return 0;
    }

    int pos = parse_local_variable_identifier(t);

    while (expect(T_COMMA)) {
        int pos2 = parse_local_variable_identifier(t);
        pos = alloc_binop_atom(TYPE_ANDTHEN, pos, pos2);
    }

    if (!expect(T_SEMICOLON)) {
        error("parse_var_declare: no semicolon delimiter");
    }

    return pos;
}

int parse_block();

int parse_block_or_statement() {
    int pos = parse_statement();
    if (pos == 0) {
        pos = parse_block();
    }
    return pos;
}

int parse_block_or_statement_series() {
    int pos = alloc_nop_atom();
    for (;;) {
        int new_pos = parse_block_or_statement();
        if (!new_pos) {
            break;
        }
        pos = alloc_binop_atom(TYPE_ANDTHEN, pos, new_pos);
    }
    return pos;
}

int parse_block() {
    int pos = alloc_nop_atom();

    if (!expect(T_LBLACE)) {
        return 0;
    }

    enter_var_frame();

    pos = alloc_binop_atom(TYPE_ANDTHEN, pos, parse_block_or_statement_series());

    if (!expect(T_RBLACE)) {
        error("invalid block end");
    }

    exit_var_frame();
    return pos;
}

var_t *parse_funcion_prototype_arg() {
    char *ident;

    expect(T_CONST);

    type_t *t = parse_type_declaration();
    if (!t) {
        return 0;
    }
    t = parse_pointer(t);

    if (expect_ident(&ident)) {
        t = parse_var_array_declare(t);
    } else {
        ident = "-";
    }

    return add_var(ident, t);
}

int parse_funcion_prototype_arg_seq() {
    int argc = 0;

    if (!parse_funcion_prototype_arg()) {
        return 0;
    }
    argc++;

    while (expect(T_COMMA)) {
        if (!parse_funcion_prototype_arg()) {
            error("Invalid argv");
        }
        argc++;
    }
    return argc;
}

int parse_function_prototype(type_t *t, bool is_external) {
    int pos = get_token_pos();

    t = parse_pointer(t);

    char *ident;
    if (!expect_ident(&ident)) {
        debug("parse_function: not fucton def");
        set_token_pos(pos);
        return 0;
    }
    
    if (!expect(T_LPAREN)) {
        debug("parse_function failed: no '('");
        set_token_pos(pos);
        return 0;
    }

    reset_var_max_offset();
    enter_var_frame();

    parse_funcion_prototype_arg_seq();

    if (!expect(T_RPAREN)) {
        error("parse_function: no ')'");
    }

    if (!expect(T_SEMICOLON)) {
        exit_var_frame();
        debug("not function prototype");
        set_token_pos(pos);
        return 0;
    }

    frame_t *frame = get_top_frame();
    add_function(ident, t, is_external, frame->num_vars, frame->vars);

    exit_var_frame();
    return 1;
}

int parse_func_args() {
    int argc = 0;

    if (!parse_func_arg_declare()) {
        return 0;
    }
    argc++;

    while (expect(T_COMMA)) {
        if (!parse_func_arg_declare()) {
            error("Invalid argv");
        }
        argc++;
    }
    return argc;
}

int parse_function_definition(type_t *t) {
    int pos = get_token_pos();

    t = parse_pointer(t);

    char *ident;
    if (!expect_ident(&ident)) {
        debug("parse_function: not fuctionn definition");
        set_token_pos(pos);
        return 0;
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
    func *f = add_function(ident, t, FALSE, frame->num_vars, frame->vars);

    int body_pos = parse_block();
    if (!body_pos) {
        error_s("No body for function: ", ident);
    }

    func_set_body(f, frame->num_vars, frame->vars, body_pos, var_max_offset());

    exit_var_frame();
    return 1;
}

int parse_global_declaration() {
    bool is_external = 0;
    if (expect(T_EXTERN)) {
        is_external = 1;
    }
    type_t *t = parse_type_declaration();
    if (!t) {
        return 0;
    }
    if (expect(T_SEMICOLON)) {
        return 1;
    }
    int pos;
    pos = parse_function_prototype(t, is_external);
    if (pos) {
        return pos;
    }
    pos = parse_function_definition(t);
    if (pos) {
        return pos;
    }
    pos = parse_global_variable(t, is_external);
    if (pos) {
        if (expect(T_SEMICOLON)) {
            return pos;
        }
        error("no semicolon after variable declaration");
    }
    return 0;
}

void parse() {
    init_types();

    enter_var_frame();
    while (!expect(T_EOF)) {
        int pos;
        pos = parse_global_declaration();
        if (pos) continue;
        error("Invalid declaration");
    }
    exit_var_frame();
}
