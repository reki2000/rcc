#include "types.h"
#include "rsys.h"
#include "rstring.h"
#include "devtool.h"
#include "vec.h"

#include "type.h"

#define NUM_TYPES 1024
#define NUM_ENUMS 1024
#define NUM_STRUCTS 1024
#define NUM_STRUCT_MEMBERS 100 // todo: this should be same with type.h 

VEC_HEADER(type_t, type_vec)
VEC_BODY(type_t, type_vec)

type_vec types = 0;

type_t *type_int;
type_t *type_void;
type_t *type_char;
type_t *type_long;
type_t *type_void_ptr;
type_t *type_char_ptr;

void init_types() {
    add_type("$*", 8, 0, -1);    // for pointer
    type_void = add_type("void", 0, 0, -1);
    type_int = add_type("int", 4, 0, -1);
    type_char = add_type("char", 1, 0, -1);
    type_long = add_type("long", 8, 0, -1);

    type_void_ptr = add_pointer_type(type_void);
    type_char_ptr = add_pointer_type(type_char);
}

void dump_type(char *buf, type_t *t) {
    type_t *t_org = t;

    if (!t) {
        strcat(buf, "?");
        return;
    }
    while (t && t->ptr_to) {
        if (t->array_length > 0) {
            _strcat3(buf, "[", t->array_length, "]");
        } else if (t->array_length == 0) {
            strcat(buf, "[]");
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
    _strcat3(buf, " size:", type_size(t_org), "");
}

type_t *add_type(char* name, int size, type_t *ptr_to, int array_length) {
    if (!types) types = type_vec_new();

    type_t t;
    t.name = name;
    t.size = size;
    t.ptr_to = ptr_to;
    t.array_length = array_length;
    t.enum_of = (void *)0;
    t.struct_of = (void *)0;
    t.typedef_of = (void *)0;

    type_t *t_ptr = type_vec_push(types, t);

    char buf[RCC_BUF_SIZE] = {0};
    dump_type(buf, t_ptr);
    debug("added type:%s", buf);

    return t_ptr;
}

type_t *add_typedef(char *name, type_t* t) {
    type_t *defined_type = add_type(name, type_size(t), t->ptr_to, t->array_length);
    defined_type->struct_of = t->struct_of;
    defined_type->enum_of = t->enum_of;
    defined_type->typedef_of = t;
    return defined_type;
}

type_t *find_pointer(type_t *ptr_to, int array_length) {
    for (int i=0; i<type_vec_len(types); i++) {
        type_t *t = type_vec_get(types, i);
        if (t->ptr_to == ptr_to && t->array_length == array_length) {
            return t;
        }
    }
    return 0;
}

type_t *add_pointer_type(type_t *t) {
    type_t *p = find_pointer(t, -1);
    if (!p) {
        int size = 8;
        p = add_type("", size, t, -1);
    }
    return p;
}

type_t *add_array_type(type_t *t, int array_length) {
    type_t *p = find_pointer(t, array_length);
    if (!p) {
        int size = array_length * type_size(t);
        p = add_type("", align(size, 4), t, array_length);
    }
    return p;
}

type_t *find_type(char *name) {
    for (int i=0; i<type_vec_len(types); i++) {
        type_t *t = type_vec_get(types, i);
        if (!strcmp(name, t->name)) {
            return t;
        }
    }
    return 0;
}

struct_t structs[NUM_STRUCTS];
int structs_len = 0;

type_t *find_struct_type(char *name, bool is_union) {
    for (int i=0; i<type_vec_len(types); i++) {
        type_t *t = type_vec_get(types, i);
        if (t->struct_of && t->typedef_of == 0) {
            struct_t *s = t->struct_of;
            if (!s->is_anonymous 
                && !strcmp(name, s->name) 
                && s->is_union == is_union) {
                return t;
            }
        }
    }
    return 0;
}

type_t *add_struct_union_type(char *name, bool is_union, bool is_anonymous) {
    type_t *t = (void *)0;
    if (!is_anonymous) {
        t = find_struct_type(name, is_union);
        if (t) {
            return t;
        }
    }
    if (structs_len >= NUM_STRUCTS) {
        error("Too many structs:%s", name);
    }
    struct_t *s = &structs[structs_len++];
    s->name = is_anonymous? "annonymous" : name;
    s->num_members = 0;
    s->is_union = is_union;
    s->is_anonymous = is_anonymous;

    t = add_type("$s", 0, 0, -1);
    t->struct_of = s;
    return t;
}

type_t *add_union_type(char *name, bool is_anonymous) {
    return add_struct_union_type(name, TRUE, is_anonymous);
}

type_t *add_struct_type(char *name, bool is_anonymous) {
    return add_struct_union_type(name, FALSE, is_anonymous);
}

void copy_union_member_to_struct(type_t *st, type_t *ut) {
    for (int i=0; i<ut->struct_of->num_members; i++) {
        member_t *m = &(ut->struct_of->members[i]);
        add_struct_member(st, m->name, m->t, TRUE);
    }
}

member_t *add_struct_member(type_t *st, char *name, type_t *t, bool is_union) {
    struct_t *s = st->struct_of;
    if (s == 0) {
        error("adding member to non struct type: %s", st->name);
    }
    if (s->num_members > NUM_STRUCT_MEMBERS) {
        error("Too many struct members: %s", name);
    }
    member_t *m = &(s->members[s->num_members++]);
    m->name = name;
    m->t = t;

    // todo: alignment to 4 bytes
    int t_size = type_size(t);
    if (is_union) {
        m->offset = s->next_offset;
        if (t_size > st->size - s->next_offset) {
            st->size = s->next_offset + t_size;
        }
    } else {
        m->offset = st->size;
        debug("added struct member %s @%d", name, m->offset);
        st->size += t_size;
        s->next_offset += t_size;
    }

    return m;
}

member_t *find_struct_member(type_t *t, char *name) {
    struct_t *s = t->struct_of;
    if (s == 0) {
        error("this type is not struct: %s", t->name);
    }
    for (int i=0; i<s->num_members; i++) {
        member_t *m = &(s->members[i]);
        if (strcmp(name, m->name) == 0) {
            return m;
        }
    }
    return 0;
}


enum_t enums[NUM_ENUMS];
int enums_len = 0;

type_t *find_enum_type(char *name) {
    // annonymous enum should be different in every occurence
    if (strlen(name) == 0) {
        return 0; 
    }

    for (int i=0; i<type_vec_len(types); i++) {
        type_t *t = type_vec_get(types, i);
        if (t->enum_of && t->typedef_of == 0) {
            if (!strcmp(name, t->enum_of->name)) {
                return t;
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

    if (enums_len >= NUM_ENUMS) {
        error("too much enum definitions");
    }

    enum_t *e = &enums[enums_len++];
    e->next_value = 0;
    e->name = name;
    debug("added new enum type: ", name);

    t = add_type("$e", 4, 0, -1);
    t->enum_of = e;

    return t;
}

type_t *type_unalias(type_t *t) {
    while (t->typedef_of) {
        t = t->typedef_of;
    }
    return t;
}

int type_size(type_t *t) {
    return type_unalias(t)->size;
}

bool type_is_same(type_t *to, type_t *from) {
    if (!to || !from) return FALSE;
    to = type_unalias(to);
    from = type_unalias(from);
    if (to == from) return TRUE;
    if (to->array_length != from->array_length) return FALSE;
    if (to->ptr_to && from->ptr_to && type_is_same(to->ptr_to, from->ptr_to)) return TRUE;
    return FALSE;
}

bool type_is_convertable(type_t *to, type_t *from) {
    if (!to || !from) return FALSE;
    to = type_unalias(to);
    from = type_unalias(from);
    if (to == from) return TRUE;
    if (to->ptr_to && from->ptr_to && type_is_convertable(to->ptr_to, from->ptr_to)) return TRUE;
    return FALSE;
}
