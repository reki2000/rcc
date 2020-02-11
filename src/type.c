#include "rstring.h"
#include "devtool.h"
#include "types.h"
#include "type.h"

type_t types[1024];
int types_pos = 0;

void init_types() {
    add_type("", 8, 0, 0);    // for pointer
    add_type("void", 0, 0, 0);
    add_type("int", 4, 0, 0);
    add_type("char", 1, 0, 0);
    add_type("long", 8, 0, 0);
}

void dump_type(type_t *t) {
    char buf[100];
    buf[0] = 0;
    strcat(buf, "type ");
    if (t->struct_of) {
        if (t->struct_of->is_union) {
            strcat(buf, "union ");
        } else {
            strcat(buf, "struct ");
        }
    }
    strcat(buf, t->name);
    _strcat3(buf, " size:", t->size, "");
    strcat(buf, " ptr_to:");
    strcat(buf, (t->ptr_to == 0) ? "" : t->ptr_to->name);
    if (t->array_length) {
        _strcat3(buf, " [", t->array_length, "] ");
    }
    debug(buf);
}

type_t *add_type(char* name, int size, type_t *prt_to, int array_length) {
    type_t *p = &types[types_pos++];
    p->name = name;
    p->size = size;
    p->ptr_to = prt_to;
    p->array_length = array_length;
    dump_type(p);
    return p;
}

type_t *find_pointer(type_t *ptr_to, int array_length) {
    for (int i=0; i<types_pos; i++) {
        type_t *t = &types[i];
        if (t->ptr_to == ptr_to && t->array_length == array_length) {
            return t;
        }
    }
    return 0;
}

type_t *add_pointer_type(type_t *t) {
    type_t *p = find_pointer(t, 0);
    if (!p) {
        int size = 8;
        p = add_type("", size, t, 0);
    }
    return p;
}

type_t *add_array_type(type_t *t, int array_length) {
    type_t *p = find_pointer(t, array_length);
    if (!p) {
        int size = array_length * t->size;
        p = add_type("", align(size, 4), t, array_length);
    }
    return p;
}

type_t *find_type(char *name) {
    for (int i=0; i<types_pos; i++) {
        if (strcmp(name, types[i].name) == 0 && types[i].struct_of == 0) {
            return &types[i];
        }
    }
    return 0;
}


struct_t structs[1024];
int structs_len = 0;

type_t *add_struct_union_type(char *name, bool is_union) {
    type_t *t = find_struct_type(name, is_union);
    if (t) {
        return t;
    }
    if (structs_len > 1024) {
        error_s("Too many structs:", name);
    }
    struct_t *s = &structs[structs_len++];
    s->num_members = 0;
    s->is_union = is_union;

    t = add_type(name, 0, 0, 0);
    t->struct_of = s;
    return t;
}

type_t *add_union_type(char *name) {
    return add_struct_union_type(name, TRUE);
}

type_t *add_struct_type(char *name) {
    return add_struct_union_type(name, FALSE);
}

type_t *find_struct_type(char *name, bool is_union) {
    for (int i=0; i<types_pos; i++) {
        if (strcmp(name, types[i].name) == 0 && types[i].struct_of != 0) {
            if (types[i].struct_of->is_union == is_union) {
                return &types[i];
            }
        }
    }
    return 0;
}

member_t *add_struct_member(type_t *st, char *name, type_t *t) {
    struct_t *s = st->struct_of;
    if (s == 0) {
        error_s("not struct type: ", st->name);
    }
    if (s->num_members > 100) {
        error_s("Too many struct members: ", name);
    }
    member_t *m = &(s->members[s->num_members++]);
    m->name = name;
    m->t = t;

    // todo: alignment to 4 bytes
    if (s->is_union) {
        m->offset = 0;
        if (st->size < t->size) {
            st->size = t->size;
        }
    } else {
        m->offset = st->size;
        debug_i("added struct member @", m->offset);
        st->size += t->size;
    }

    return m;
}

member_t *find_struct_member(type_t *t, char *name) {
    struct_t *s = t->struct_of;
    if (s == 0) {
        error_s("not struct type: ", t->name);
    }
    for (int i=0; i<s->num_members; i++) {
        member_t *m = &(s->members[i]);
        if (strcmp(name, m->name) == 0) {
            return m;
        }
    }
    return 0;
}

bool is_convertable(type_t *to, type_t *from) {
    if (to == from) return TRUE;
    if (!to || !from) return FALSE;
    if (is_convertable(to->ptr_to, from->ptr_to)) return TRUE;
    return FALSE;
}
