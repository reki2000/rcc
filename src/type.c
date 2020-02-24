#include "rstring.h"
#include "devtool.h"
#include "types.h"
#include "type.h"

type_t types[1024];
int types_pos = 0;

void init_types() {
    add_type("$*", 8, 0, 0);    // for pointer
    add_type("void", 0, 0, 0);
    add_type("int", 4, 0, 0);
    add_type("char", 1, 0, 0);
    add_type("long", 8, 0, 0);
}

void dump_type(char buf[], type_t *t) {
    type_t *t_org = t;

    if (t) {
        while (t->ptr_to) {
            if (t->array_length > 0) {
                _strcat3(buf, "[", t->array_length, "]");
            } else {
                strcat(buf, "*");
            }
            t = t->ptr_to;
        }

        if (t->struct_of) {
            if (t->struct_of->is_union) {
                strcat(buf, "union ");
            } else {
                strcat(buf, "struct ");
            }
            strcat(buf, t->struct_of->name);
        } else if (t->enum_of) {
            strcat(buf, "enum ");
            strcat(buf, t->enum_of->name);
        } else {
            strcat(buf, t->name);
        }
       _strcat3(buf, " size:", t_org->size, "");
    } else {
        strcat(buf, "?");
    }
}

type_t *add_type(char* name, int size, type_t *prt_to, int array_length) {
    type_t *p = &types[types_pos++];
    p->name = name;
    p->size = size;
    p->ptr_to = prt_to;
    p->array_length = array_length;
    p->enum_of = 0;
    p->struct_of = 0;
    p->typedef_of = 0;

    char buf[100] = {0};
    strcat(buf, "added type:");
    dump_type(buf, p);
    debug(buf);

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
        if (strcmp(name, types[i].name) == 0) {
            return &types[i];
        }
    }
    return 0;
}

struct_t structs[1024];
int structs_len = 0;

type_t *find_struct_type(char *name, bool is_union) {
    for (int i=0; i<types_pos; i++) {
        type_t *t = &types[i];
        if (t->struct_of && t->typedef_of == 0) {
            if (strcmp(name, t->struct_of->name) == 0 
                && t->struct_of->is_union == is_union) {
                return &types[i];
            }
        }
    }
    return 0;
}

type_t *add_struct_union_type(char *name, bool is_union) {
    type_t *t = find_struct_type(name, is_union);
    if (t) {
        return t;
    }
    if (structs_len > 1024) {
        error_s("Too many structs:", name);
    }
    struct_t *s = &structs[structs_len++];
    s->name = name;
    s->num_members = 0;
    s->is_union = is_union;

    t = add_type("$s", 0, 0, 0);
    t->struct_of = s;
    return t;
}

type_t *add_union_type(char *name) {
    return add_struct_union_type(name, TRUE);
}

type_t *add_struct_type(char *name) {
    return add_struct_union_type(name, FALSE);
}

member_t *add_struct_member(type_t *st, char *name, type_t *t) {
    struct_t *s = st->struct_of;
    if (s == 0) {
        error_s("adding member to non struct type: ", st->name);
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
        error_s("this type is not struct: ", t->name);
    }
    for (int i=0; i<s->num_members; i++) {
        member_t *m = &(s->members[i]);
        if (strcmp(name, m->name) == 0) {
            return m;
        }
    }
    return 0;
}


enum_t enums[1024];
int enums_len = 0;

type_t *find_enum_type(char *name) {
    for (int i=0; i<types_pos; i++) {
        type_t *t = &types[i];
        if (t->enum_of && t->typedef_of == 0) {
            if (strcmp(name, t->enum_of->name) == 0) {
                return &types[i];
            }
        }
    }
    return 0;
}

type_t *add_enum_type(char *name) {
    type_t *t = find_enum_type(name);
    if (t) {
        return t;
    }

    if (enums_len >= 1024) {
        error("too much enum definitions");
    }

    enum_t *e = &enums[enums_len++];
    e->next_value = 0;
    e->name = name;

    t = add_type("$e", 4, 0, 0);
    t->enum_of = e;

    return t;
}

bool type_is_same(type_t *to, type_t *from) {
    if (!to || !from) return FALSE;
    while (to->typedef_of) {
        to = to->typedef_of;
    }
    while (from->typedef_of) {
        from = from->typedef_of;
    }
    if (to == from) return TRUE;
    if (to->array_length != from->array_length) return FALSE;
    if (to->ptr_to && from->ptr_to && type_is_same(to->ptr_to, from->ptr_to)) return TRUE;
    return FALSE;
}

bool type_is_convertable(type_t *to, type_t *from) {
    if (!to || !from) return FALSE;
    while (to->typedef_of) {
        to = to->typedef_of;
    }
    while (from->typedef_of) {
        from = from->typedef_of;
    }
    if (to == from) return TRUE;
    if (to->ptr_to && from->ptr_to && type_is_convertable(to->ptr_to, from->ptr_to)) return TRUE;
    return FALSE;
}
