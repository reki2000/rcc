#include "types.h"
#include "rsys.h"
#include "rstring.h"
#include "devtool.h"
#include "vec.h"

#include "type.h"
#include "var.h"
#include "func.h"
#include "atom.h"
#include "token.h"

#define NUM_ATOMS 100000

atom_t program[NUM_ATOMS];
int atom_pos = 1;

char *atom_name[] = {
    "args", "int", "add", "sub", "mul", "div", "mod", "bit-and","bit-or","bit-xor", "bit-neg", "bit-lshift", "bit-rshift",
    "var_ref", "nop", "expr_stmt", "andthen", "global", "print", "bind",
    "==","!=","<", ">", ">=", "<=", "&&", "||", "!",
    "if", "for", "while", "dowhile", "break", "continue",
    "&(ptr_of)", "*(val_of)", "func", "return", "apply",
    "n++", "n--",
    "str", ".", "->", "gval_ref", "rvalue", "convert", "struct-offset", "array-index",
    "switch", "case", "default", "?:", "cast"
};

bool is_order_operator(int type) {
    switch (type) {
        case TYPE_EQ_EQ:
        case TYPE_EQ_GE:
        case TYPE_EQ_GT:
        case TYPE_EQ_LE:
        case TYPE_EQ_LT:
        case TYPE_EQ_NE:
        return TRUE;
    }
    return FALSE;
}

bool is_arithmetic_operator(int type) {
    switch (type) {
        case TYPE_ADD:
        case TYPE_SUB:
        case TYPE_MUL:
        case TYPE_DIV:
        case TYPE_MOD:
        return TRUE;
    }
    return is_order_operator(type);
}

int alloc_atom(int size) {
    int current;
    current = atom_pos;
    if (atom_pos + size >= NUM_ATOMS) {
        error("Source code too long");
    }
    program[atom_pos].token_pos = get_token_pos();
    atom_pos += size;
    return current;
}

void dump_atom3(char *buf, atom_t *p, int indent, int pos) {
    for (int i=0; i<indent; i++) {
        strcat(buf, " ");
    }

    snprintf(buf+strlen(buf), RCC_BUF_SIZE, "atom#%d,[%s] value:", pos, atom_name[p->type]);
    if (!p) {
        error("null atom for:%s", buf);
    }
    switch (p->type) {
        case TYPE_APPLY:
            strcat(buf, ((func *)(p->ptr_value))->name);
            break;
        case TYPE_VAR_REF:
        default:
            snprintf(buf+strlen(buf), RCC_BUF_SIZE, "%d", p->int_value);
    }

    snprintf(buf+strlen(buf), RCC_BUF_SIZE, "t: %s ", dump_type(p->t));
    dump_token_simple(buf+strlen(buf), p->token_pos);
}

void dump_atom(int pos, int indent) {
    dump_atom2(&program[pos], indent, pos);
}
void dump_atom2(atom_t *p, int indent, int pos) {
    char buf[RCC_BUF_SIZE] = {0};
    dump_atom3(buf, p, indent, pos);
    debug("%s", buf);
}

void dump_atom_all() {
    int i;
    for (i=1; i<atom_pos; i++) {
        dump_atom(i, 0);
    }
}

void dump_atom_tree(int pos, int indent) {
    dump_atom(pos, indent);
    atom_t *a = &program[pos];
    switch (a->type) {
        case TYPE_ANDTHEN:
            dump_atom_tree(a->atom_pos, indent);
            dump_atom_tree((a+1)->atom_pos, indent);
            break;
        case TYPE_ADD:
        case TYPE_MUL:
        case TYPE_EQ_EQ:
        case TYPE_EQ_GE:
        case TYPE_EQ_GT:
        case TYPE_EQ_LE:
        case TYPE_EQ_LT:
        case TYPE_LOG_AND:
        case TYPE_LOG_OR:
        case TYPE_BIND:
        case TYPE_MEMBER_OFFSET:
        case TYPE_CASE:
            dump_atom_tree(a->atom_pos, indent + 1);
            dump_atom_tree((a+1)->atom_pos, indent + 1);
            break;
        case TYPE_EXPR_STATEMENT:
        case TYPE_PTR_DEREF:
        case TYPE_PTR:
        case TYPE_RETURN:
        case TYPE_PRINT:
        case TYPE_RVALUE:
        case TYPE_CONVERT:
        case TYPE_POSTFIX_DEC:
        case TYPE_POSTFIX_INC:
        case TYPE_DEFAULT:
            dump_atom_tree(a->atom_pos, indent + 1);
            break;
        case TYPE_ARRAY_INDEX:
            dump_atom_tree(a->atom_pos, indent + 1);
            dump_atom_tree((a+1)->atom_pos, indent + 1);
            dump_atom2(a+2, indent + 1, pos+2);
            break;
        case TYPE_WHILE:
        case TYPE_DO_WHILE:
            dump_atom_tree(a->atom_pos, indent + 1);
            dump_atom_tree((a+1)->atom_pos, indent + 1);
            break;
        case TYPE_FOR:
            dump_atom_tree(a->atom_pos, indent + 1);
            dump_atom_tree((a+1)->atom_pos, indent + 1);
            dump_atom_tree((a+2)->atom_pos, indent + 1);
            dump_atom_tree((a+3)->atom_pos, indent + 1);
            break;
        case TYPE_IF:
        case TYPE_TERNARY:
            dump_atom_tree(a->atom_pos, indent + 1);
            dump_atom_tree((a+1)->atom_pos, indent + 1);
            dump_atom_tree((a+2)->atom_pos, indent + 1);
            break;
        case TYPE_APPLY:
            dump_atom2(++a, indent + 1, pos);
            for (a++; a->type == TYPE_ARG; a++) {
                dump_atom_tree(a->atom_pos, indent + 1);
            }
            break;
        case TYPE_SWITCH:
            dump_atom_tree(a->atom_pos, indent + 1);
            for (a++; a->type == TYPE_ARG; a++) {
                dump_atom_tree(a->atom_pos, indent + 1);
            }
            break;
    }
}

int atom_set_type(int pos, type_t *t) {
    program[pos].t = t;
    return pos;
}

type_t *atom_type(int pos) {
    if (program[pos].t == 0) {
        error("Null type at atom_t #%d", pos);
    }
    return program[pos].t;
}

void build_int_atom(int pos, int type, int value) {
    atom_t *a = &program[pos];
    a->type = type;
    a->int_value = value;
    a->t = type_int;
}

void build_ptr_atom(int pos, int type, void *ptr) {
    atom_t *a = &program[pos];
    a->type = type;
    a->ptr_value = ptr;
    a->t = program[pos].t;
}

void build_pos_atom(int pos, int type, int target) {
    atom_t *a = &program[pos];
    a->type = type;
    a->atom_pos = target;
    a->t = program[target].t;
}

int alloc_typed_pos_atom(int type, int pos, type_t *t) {
    int new = alloc_atom(1);
    atom_t *a = &program[new];
    a->type = type;
    a->atom_pos = pos;
    a->t = t;
    return new;
}

int alloc_typed_int_atom(int type, int value, type_t *t) {
    int pos = alloc_atom(1);
    atom_t *a = &program[pos];
    a->type = type;
    a->int_value = value;
    a->t = t;
    return pos;
}

int alloc_typed_long_atom(int type, long value, type_t *t) {
    int pos = alloc_atom(1);
    atom_t *a = &program[pos];
    a->type = type;
    a->long_value = value;
    a->t = t;
    return pos;
}

int atom_to_rvalue(int target) {
    atom_t *a = &program[target];
    type_t *t;
    switch (a->type) {
        case TYPE_VAR_REF:
        case TYPE_GLOBAL_VAR_REF:
            t = a->t->ptr_to;
            // in rvalue context, array type should forget its size information
            if (a->t->array_length != -1) { 
                t = add_pointer_type(t);
                return alloc_typed_pos_atom(TYPE_CONVERT, target, t);
            }
            return alloc_typed_pos_atom(TYPE_RVALUE, target, t);

        case TYPE_ARRAY_INDEX:
        case TYPE_MEMBER_OFFSET:
        case TYPE_PTR_DEREF:
            return alloc_typed_pos_atom(TYPE_RVALUE, target, a->t->ptr_to);
    }
    return target;
}

int alloc_var_atom(var_t *v) {
    int pos = alloc_atom(1);
    if (v->is_global) {
        build_ptr_atom(pos, TYPE_GLOBAL_VAR_REF, v->name);
    } else {
        build_int_atom(pos, TYPE_VAR_REF, v->offset);
    }

    if (v->t->array_length != -1) {
        atom_set_type(pos, v->t); // array type is already pointer(lvalue)
    } else {
        atom_set_type(pos, add_pointer_type(v->t)); // handle all variables as pointer(lvalue)
    }
    return pos;
}

int alloc_deref_atom(int target) {
    target = atom_to_rvalue(target);
    atom_t *a = &program[target];
    return  alloc_typed_pos_atom(TYPE_PTR_DEREF, target, a->t);
}

int alloc_ptr_atom(int target) {
    atom_t *a = &program[target];
    switch (a->type) {
        case TYPE_RVALUE:
            error("compiler bug - rvalue shouldn't be here");
            return 0;
        case TYPE_ARRAY_INDEX:
        case TYPE_MEMBER_OFFSET:
        case TYPE_VAR_REF:
        case TYPE_GLOBAL_VAR_REF:
            return  alloc_typed_pos_atom(TYPE_PTR, target, a->t);
    }
    dump_atom_tree(target, 1);
    error("cannot get pointer of above atom #%d atom_type:%d", target, a->type);
    return 0;
}

int alloc_postincdec_atom(int type, int target) {
    type_t *t = atom_type(target);
    return alloc_typed_pos_atom(type, target, t->ptr_to);
}

int alloc_func_atom(func *f, int num_args, int *args) {
    int pos = alloc_atom(2+num_args);

    build_ptr_atom(pos, TYPE_APPLY, (void *)f);
    atom_set_type(pos, f->ret_type);

    build_int_atom(pos+1, TYPE_ARG, num_args); 

    for (int i=0; i<num_args; i++) {
        build_pos_atom(pos+2+i, TYPE_ARG, args[i]);
    }
    return pos;
}

int alloc_index_atom(int base_pos, int index_pos) {
    int pos = base_pos;
    type_t *t = atom_type(pos);
    int size = 0;
    if (t->array_length >= 0) { // base is an array
        debug("alloc array index for array: %s", dump_type(t));
        t = t->ptr_to;
        size = type_size(t);
        if (!t->ptr_to || t->array_length < 0) {
            t = add_pointer_type(t);
        }
    } else if (t->ptr_to) { // base is a pointer
        warning("implicit convertion from pointer to array:%s", dump_type(t));
        pos = alloc_deref_atom(pos);
        t = t->ptr_to;
        size = type_size(t->ptr_to);
    } else {
        dump_atom_tree(base_pos, 0);
        dump_atom_tree(index_pos, 0);
        error("index for non-array atom #%d", pos);
    }
    index_pos = atom_to_rvalue(index_pos);
    int pos2 = alloc_typed_pos_atom(TYPE_ARRAY_INDEX, pos, t);
    alloc_typed_pos_atom(TYPE_ARG, index_pos, atom_type(index_pos));
    alloc_typed_int_atom(TYPE_ARG, size, type_int);
    //dump_atom_tree(pos2, 0);
    return pos2;
}

int alloc_offset_atom(int base_pos, type_t *offset_t, int offset) {
    int pos = base_pos;
    type_t *t = atom_type(pos);
    if (!t->ptr_to->struct_of) {
        dump_atom_tree(base_pos, 0);
        error("offset for non-struct atom #%d" , pos);
    }
 
    int pos2 = alloc_binop_atom(TYPE_MEMBER_OFFSET, pos, alloc_typed_int_atom(TYPE_INTEGER, offset, type_int));
    atom_set_type(pos2, add_pointer_type(offset_t));
    return pos2;
}

int calculate_if_constant_ops(int type, int lpos, int rpos) {
    if (program[lpos].type == TYPE_INTEGER && program[rpos].type == TYPE_INTEGER) {
        int l_int = program[lpos].int_value;
        int r_int = program[rpos].int_value;
        switch (type) {
            case TYPE_ADD: l_int += r_int; break;
            case TYPE_SUB: l_int -= r_int; break;
            case TYPE_MUL: l_int *= r_int; break;
            case TYPE_DIV: l_int /= r_int; break;
            case TYPE_MOD: l_int %= r_int; break;
            case TYPE_LSHIFT: l_int <<= r_int; break;
            case TYPE_RSHIFT: l_int >>= r_int; break;
            case TYPE_AND: l_int &= r_int; break;
            case TYPE_OR: l_int |= r_int; break;
            case TYPE_XOR: l_int ^= r_int; break;
            case TYPE_LOG_AND: l_int = l_int && r_int; break;
            case TYPE_LOG_OR: l_int = l_int || r_int; break;
            case TYPE_EQ_EQ: l_int = l_int == r_int; break;
            case TYPE_EQ_GE: l_int = l_int >= r_int; break;
            case TYPE_EQ_GT: l_int = l_int > r_int; break;
            case TYPE_EQ_LE: l_int = l_int <= r_int; break;
            case TYPE_EQ_LT: l_int = l_int < r_int; break;
        }
        return alloc_typed_int_atom(TYPE_INTEGER, l_int, type_int);
    }
    return 0;
}

int alloc_binop_atom(int type, int lpos, int rpos) {
    int const_pos = calculate_if_constant_ops(type, lpos, rpos);
    if (const_pos) return const_pos;

    type_t *lpos_t = atom_type(lpos);
    type_t *rpos_t = atom_type(rpos);
    if (lpos_t->ptr_to && (type == TYPE_ADD || type == TYPE_SUB)) {
        if (rpos_t->ptr_to) {
            error("Cannot + or - between pointers");
        }
        int size = alloc_typed_int_atom(TYPE_INTEGER, type_size(lpos_t->ptr_to), type_int);
        rpos = alloc_binop_atom(TYPE_MUL, rpos, size);
    } else if (!lpos_t->ptr_to && !rpos_t->ptr_to && is_arithmetic_operator(type)) {
        if (lpos_t->size > rpos_t->size) {
            rpos = atom_convert_type(lpos, rpos);
        } else {
            lpos = atom_convert_type(rpos, lpos);
        }
    }
    int pos = alloc_atom(2);
    build_pos_atom(pos, type, lpos);
    build_pos_atom(pos+1, TYPE_ARG, rpos);
    return pos;
}

int alloc_assign_op_atom(int type, int lval, int rval) {
    int lval_deref = atom_to_rvalue(lval);
    rval = alloc_binop_atom(type, lval_deref, atom_to_rvalue(rval));
    return alloc_binop_atom(TYPE_BIND, rval, lval);
}

// make p2's type to p1's type
int atom_convert_type(int p1, int p2) {
    type_t *t1 = type_unalias(program[p1].t);
    type_t *t2 = type_unalias(program[p2].t);

    if (type_is_same(t1, t2)) {
        return p2;
    }
    if (type_is_convertable(t1, t2)) {
        return alloc_typed_pos_atom(TYPE_CONVERT, p2, t1);
    }
    if (!t1->ptr_to && !t1->struct_of && !t1->enum_of && !t2->ptr_to && !t2->struct_of && !t2->enum_of) {
        return alloc_typed_pos_atom(TYPE_CONVERT, p2, t1);
    }
    if (t1->ptr_to && t2->ptr_to) {
        warning("implicit pointer conversion: %s -> %s", dump_type(t2), dump_type(t1));
        return alloc_typed_pos_atom(TYPE_CONVERT, p2, t1);
    }
    if ((t1->enum_of && t2 == type_int) || (t1 == type_int && t2->enum_of)) {
        return alloc_typed_pos_atom(TYPE_CONVERT, p2, t1);
    }
    dump_atom_tree(p1, 1);
    dump_atom_tree(p2, 1);
    error("not compatible type");
    return 0;
}

int NOP_ATOM = 0;

int alloc_nop_atom() {
    if (!NOP_ATOM) {
        NOP_ATOM = alloc_typed_pos_atom(TYPE_NOP, 0, type_void);
    }
    return NOP_ATOM;
}
