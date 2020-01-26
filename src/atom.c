#include "rsys.h"
#include "rstring.h"
#include "devtool.h"

#include "types.h"
#include "type.h"
#include "var.h"
#include "func.h"
#include "atom.h"

atom program[10000];
int atom_pos = 1;

char *atom_name[] = {
    "args", "int", "add", "sub", "mul", "div", "mod", 
    "var_val", "var_ref", "nop", "expr_stmt", "andthen", "global", "print", "bind",
    "==","!=","<", ">", ">=", "<=", "&&", "||", "!",
    "if", "for", "while", "dowhile", "break", "continue",
    "&(ptr_of)", "*(val_of)", "func", "return", "apply",
    "++n", "--n", "n++", "n--",
    "str"
};

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
    atom *p = &(program[pos]);
    _strcat3(buf, "atom#", pos, ",[");
    strcat(buf, atom_name[p->type]);
    strcat(buf, "] value:");
    switch (p->type) {
        case TYPE_APPLY:
            strcat(buf, ((func *)(p->value.ptr_value))->name);
            break;
        default:
            _strcat3(buf, "", p->value.int_value, "");
    }

    strcat(buf, " t:");
    type_s *t = p->t;
    while(t) {
        if (t->ptr_to) {
            strcat(buf, "*");
            t = t->ptr_to;
        } else {
            strcat(buf, t->name);
            break;
        }
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

void atom_copy_type(int from, int to) {
    atom_set_type(to, program[from].t);
}

type_s *atom_type(int pos) {
    return program[pos].t;
}

int alloc_int_atom(int type, int value) {
    int pos = alloc_atom(1);
    build_int_atom(pos, type, value);
    return pos;
}

int alloc_pos_atom(int type, int value) {
    int pos = alloc_atom(1);
    build_pos_atom(pos, type, value);
    return pos;
}


int alloc_var_atom(var *v) {
    int pos = alloc_atom(1);
    build_int_atom(pos, TYPE_VAR_VAL, v->offset);
    atom_set_type(pos, v->t);
    return pos;
}

int alloc_deref_atom(int target) {
    if (!atom_type(target)->ptr_to) {
        error("target is not pointer type");
    }
    int pos = alloc_atom(1);
    build_int_atom(pos, TYPE_PTR_DEREF, target);
    atom_set_type(pos, atom_type(target)->ptr_to);
    return pos;
}

int alloc_deref_op_atom(int type, int target) {
    if (!atom_type(target)->ptr_to) {
        error("target is not pointer type");
    }
    int pos = alloc_atom(1);
    build_int_atom(pos, type, target);
    atom_set_type(pos, atom_type(target)->ptr_to);
    return pos;
}

int alloc_func_atom(func *f) {
    int pos = alloc_atom(f->argc + 1);
    build_ptr_atom(pos, TYPE_APPLY, (void *)f);
    atom_set_type(pos, f->ret_type);
    return pos;
}

int alloc_ptr_atom(int target) {
    int pos = alloc_atom(1);
    build_int_atom(pos, TYPE_PTR, target);
    atom_set_type(pos, add_pointer_type(atom_type(target)));
    if (program[target].type != TYPE_VAR_VAL) {
        error("cannot get pointer to non var atom");
    }
    program[target].type = TYPE_VAR_REF;
    return pos;
}

int alloc_binop_atom(int type, int lpos, int rpos) {
    int pos = alloc_atom(2);
    build_pos_atom(pos, type, lpos);
    build_pos_atom(pos+1, TYPE_ARG, rpos);
    return pos;
}

int alloc_str_atom(int index) {
    int pos = alloc_atom(1);
    build_int_atom(pos, TYPE_STRING, index);
    atom_set_type(pos, add_pointer_type(find_type("char")));
    return pos;
}

void build_int_atom(int pos, int type, int value) {
    atom *a = &program[pos];
    a->type = type;
    a->value.int_value = value;
    a->t = find_type("int");
}

void build_ptr_atom(int pos, int type, void * value) {
    program[pos].type = type;
    program[pos].value.ptr_value = value;
}

void build_pos_atom(int pos, int type, int value) {
    program[pos].type = type;
    program[pos].value.atom_pos = value;
    atom_copy_type(value, pos);
}

int atom_to_lvalue(int pos) {
    switch (program[pos].type) {
        case TYPE_VAR_VAL:
            program[pos].type = TYPE_VAR_REF;
            return pos;
        case TYPE_PTR_DEREF:
            return program[pos].value.atom_pos;
    }
    return 0;
}

bool atom_same_type(int p1, int p2) {
    
    return (program[p1].t == program[p2].t);
}
