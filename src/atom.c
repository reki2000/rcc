#include "rsys.h"
#include "rstring.h"
#include "devtool.h"

#include "types.h"
#include "type.h"
#include "var.h"
#include "func.h"
#include "atom.h"

atom_t program[10000];
int atom_pos = 1;

char *atom_name[] = {
    "args", "int", "add", "sub", "mul", "div", "mod", 
    "var_val", "var_ref", "nop", "expr_stmt", "andthen", "global", "print", "bind",
    "==","!=","<", ">", ">=", "<=", "&&", "||", "!",
    "if", "for", "while", "dowhile", "break", "continue",
    "&(ptr_of)", "*(val_of)", "func", "return", "apply",
    "n++", "n--",
    "str", ".", "->", "gvar_val", "gval_ref"
};

int alloc_atom(int size) {
    int current;
    current = atom_pos;
    if (atom_pos + size >= 10000) {
        error("Source code too long");
    }
    atom_pos += size;
    return current;
}

void dump_atom(int pos) {
    char buf[1024];
    buf[0] = 0;

    atom_t *p = &(program[pos]);
    _strcat3(buf, "atom#", pos, ",[");
    strcat(buf, atom_name[p->type]);
    strcat(buf, "] value:");
    switch (p->type) {
        case TYPE_APPLY:
            strcat(buf, ((func *)(p->ptr_value))->name);
            break;
        default:
            _strcat3(buf, "", p->int_value, "");
    }

    strcat(buf, " t:");
    type_s *t = p->t;

    if (t) {
        while (t->ptr_to) {
            strcat(buf, "*");
            t = t->ptr_to;
        }

        if (t->struct_of) {
            strcat(buf, "struct ");
        }
        strcat(buf, t->name);
    } else {
        strcat(buf, "?");
    }

    strcat(buf, "\n");
    _write(2, buf, strlen(buf));
}

void dump_atom_all() {
    int i;
    for (i=1; i<atom_pos; i++) {
        dump_atom(i);
    }
}

int atom_set_type(int pos, type_s *t) {
    program[pos].t = t;
    return pos;
}

type_s *atom_type(int pos) {
    if (program[pos].t == 0) {
        error_i("Null type at atom_t #", pos);
    }
    return program[pos].t;
}

void build_int_atom(int pos, int type, int value) {
    atom_t *a = &program[pos];
    a->type = type;
    a->int_value = value;
    a->t = find_type("int");
}

void build_ptr_atom(int pos, int type, void *ptr) {
    atom_t *a = &program[pos];
    a->type = type;
    a->ptr_value = ptr;
}

void build_pos_atom(int pos, int type, int target) {
    atom_t *a = &program[pos];
    a->type = type;
    a->atom_pos = target;
    a->t = program[target].t;
}

int alloc_typed_pos_atom(int type, int pos, type_s *t) {
    int new = alloc_atom(1);
    atom_t *a = &program[new];
    a->type = type;
    a->atom_pos = pos;
    a->t = t;
    return new;
}

int alloc_typed_int_atom(int type, int value, type_s *t) {
    int pos = alloc_atom(1);
    atom_t *a = &program[pos];
    a->type = type;
    a->int_value = value;
    a->t = t;
    return pos;
}

int alloc_var_atom(var *v) {
    int pos = alloc_atom(1);
    if (v->is_array) {
        if (v->is_global) {
            build_ptr_atom(pos, TYPE_GLOBAL_VAR_REF, v->name);
        } else {
            build_int_atom(pos, TYPE_VAR_REF, v->offset);
        }
    } else {
        if (v->is_global) {
            build_ptr_atom(pos, TYPE_GLOBAL_VAR_VAL, v->name);
        } else {
            build_int_atom(pos, TYPE_VAR_VAL, v->offset);
        }
    }
    atom_set_type(pos, v->t);
    return pos;
}

int alloc_deref_atom(int target) {
    atom_t *a = &program[target];
    if (!a->t->ptr_to) {
        error("target is not pointer type");
    }
    return alloc_typed_pos_atom(TYPE_PTR_DEREF, target, a->t->ptr_to);
}

int alloc_ptr_atom(int target) {
    atom_t *a = &program[target];
    if (a->type == TYPE_VAR_VAL) {
        a->type = TYPE_VAR_REF;
    } else if (a->type == TYPE_GLOBAL_VAR_VAL) {
        a->type = TYPE_GLOBAL_VAR_REF;
    } else {
        error("cannot get pointer to non var atom");
    }
    return alloc_typed_pos_atom(TYPE_PTR, target, add_pointer_type(a->t));
}

int alloc_postincdec_atom(int type, int target) {
    type_s *t = atom_type(target);
    target = atom_to_lvalue(target);
    if (!target) {
        error("postincdec target is not lvalue");
    }
    return alloc_typed_pos_atom(type, target, t);
}

int alloc_func_atom(func *f) {
    int pos = alloc_atom(f->argc + 1);
    build_ptr_atom(pos, TYPE_APPLY, (void *)f);
    atom_set_type(pos, f->ret_type);
    return pos;
}

int alloc_offset_atom(int base_pos, type_s *t, int offset) {
    int pos = alloc_atom(2);
    build_int_atom(pos, TYPE_ADD, alloc_typed_int_atom(TYPE_INT, offset, find_type("int")));
    atom_set_type(pos, add_pointer_type(t));
    build_pos_atom(pos+1, TYPE_ARG, base_pos);
    return alloc_deref_atom(pos);
}

int alloc_binop_atom(int type, int lpos, int rpos) {
    int pos = alloc_atom(2);
    build_pos_atom(pos, type, lpos);

    type_s *lpos_t = atom_type(lpos);
    if (lpos_t->ptr_to != 0 && (type == TYPE_ADD || type == TYPE_SUB)) {
        if (atom_type(rpos)->ptr_to != 0) {
            error("Cannot + or - between pointers");
        }
        int size = alloc_typed_int_atom(TYPE_INT, lpos_t->ptr_to->size, find_type("int"));
        rpos = alloc_binop_atom(TYPE_MUL, rpos, size);
    }

    build_pos_atom(pos+1, TYPE_ARG, rpos);
    return pos;
}

int alloc_assign_op_atom(int type, int lval, int rval) {
    if (!atom_same_type(lval, rval)) {
        error("not same type");
    }
    rval = alloc_binop_atom(type, lval, rval);
    lval = atom_to_lvalue(lval);
    if (!lval) {
        error("cannot bind - not lvalue");
    }
    return alloc_binop_atom(TYPE_BIND, rval, lval);
}

int atom_to_lvalue(int pos) {
    atom_t *a = &program[pos];
    atom_t *a2;
    switch (a->type) {
        case TYPE_VAR_VAL:
            pos = alloc_atom(1);
            a2 = &program[pos];
            a2->type = TYPE_VAR_REF;
            a2->t = add_pointer_type(a->t);
            a2->int_value = a->int_value;
            return pos;
        case TYPE_GLOBAL_VAR_VAL:
            pos = alloc_atom(1);
            a2 = &program[pos];
            a2->type = TYPE_GLOBAL_VAR_REF;
            a2->t = add_pointer_type(a->t);
            a2->int_value = a->int_value;
            return pos;
        case TYPE_PTR_DEREF:
            return a->atom_pos;
    }
    return 0;
}

bool atom_same_type(int p1, int p2) {
    return (program[p1].t == program[p2].t);
}
