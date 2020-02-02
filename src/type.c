#include "rstring.h"
#include "devtool.h"
#include "types.h"
#include "type.h"

type_s types[1024];
int types_pos = 0;

void init_types() {
    add_type("", 8, 0);    // for pointer
    add_type("void", 0, 0);
    add_type("int", 4, 0);
    add_type("char", 1, 0);
    add_type("long", 8, 0);
}

void dump_type(type_s *t) {
    char buf[100];
    buf[0] = 0;
    strcat(buf, "type ");
    strcat(buf, t->name);
    _strcat3(buf, " size:", t->size, "");
    strcat(buf, " ptr_to:");
    strcat(buf, (t->ptr_to == 0) ? "" : t->ptr_to->name);
    debug(buf);
}

type_s *add_type(char* name, int size, type_s *t) {
    type_s *p = &types[types_pos++];
    p->name = name;
    p->size = size;
    p->ptr_to = t;
    dump_type(p);
    return p;
}

type_s *find_pointer(type_s *t) {
    for (int i=0; i<types_pos; i++) {
        if (types[i].ptr_to == t) {
            return &types[i];
        }
    }
    return 0;
}

type_s *add_pointer_type(type_s *t) {
    type_s *p = find_pointer(t);
    if (!p) {
        p = add_type("", 8, t);
    }
    return p;
}

type_s *find_type(char *name) {
    for (int i=0; i<types_pos; i++) {
        if (strcmp(name, types[i].name) == 0 && types[i].struct_of == 0) {
            return &types[i];
        }
    }
    return 0;
}


struct_s structs[1024];
int structs_len = 0;

type_s *add_struct_type(char *name) {
    type_s *t = find_struct_type(name);
    if (t) {
        return t;
    }
    if (structs_len > 1024) {
        error_s("Too many structs:", name);
    }
    struct_s *s = &structs[structs_len++];
    s->num_members = 0;

    t = add_type(name, 0, 0);
    t->struct_of = s;
    return t;
}

type_s *find_struct_type(char *name) {
    for (int i=0; i<types_pos; i++) {
        if (strcmp(name, types[i].name) == 0 && types[i].struct_of != 0) {
            return &types[i];
        }
    }
    return 0;
}

member_s *add_struct_member(type_s *st, char *name, type_s *t, int array_size) {
    struct_s *s = st->struct_of;
    if (s == 0) {
        error_s("not struct type: ", st->name);
    }
    if (s->num_members > 100) {
        error_s("Too many struct members: ", name);
    }
    member_s *m = &(s->members[s->num_members++]);
    m->name = name;
    m->t = t;

    // todo: alignment to 4 bytes
    m->offset = st->size;
    debug_i("added struct member @", m->offset);

    if (array_size >= 0) {
        st->size += t->ptr_to->size * array_size;
    } else {
        st->size += t->size;
    }
    debug_i("struct size: @", st->size);

    return m;
}

member_s *find_struct_member(type_s *t, char *name) {
    struct_s *s = t->struct_of;
    if (s == 0) {
        error_s("not struct type: ", t->name);
    }
    for (int i=0; i<s->num_members; i++) {
        member_s *m = &(s->members[i]);
        if (strcmp(name, m->name) == 0) {
            return m;
        }
    }
    return 0;
}


