#include "types.h"
#include "rsys.h"
#include "rstring.h"
#include "devtool.h"
#include "vec.h"

#include "type.h"

VEC_HEADER(type_t, type_vec)
VEC_BODY(type_t, type_vec)

VEC_BODY(member_t, member_vec)

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

char *dump_type(type_t *t) {
    type_t *t_org = t;

    char *buf = calloc(RCC_BUF_SIZE, 1);

    if (!t) {
        strcat(buf, "?");
        return realloc(buf, strlen(buf));
    }
    while (t && t->ptr_to) {
        if (t->array_length > 0) {
            snprintf(buf+strlen(buf), RCC_BUF_SIZE, "[%d]", t->array_length);
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
    snprintf(buf+strlen(buf), RCC_BUF_SIZE, " size:%d", type_size(t_org));

    return realloc(buf, strlen(buf));;
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
    debug("added type:%s", dump_type(t_ptr));

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

VEC_HEADER(struct_t, struct_vec)
VEC_BODY(struct_t, struct_vec)

struct_vec structs = 0;

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
    if (!structs) structs = struct_vec_new();

    type_t *t = (void *)0;
    if (!is_anonymous) {
        t = find_struct_type(name, is_union);
        if (t) {
            return t;
        }
    }
    struct_t s;
    s.name = is_anonymous? "annonymous" : name;
    s.members = member_vec_new();
    s.is_union = is_union;
    s.is_anonymous = is_anonymous;
    s.next_offset = 0;

    t = add_type("$s", 0, 0, -1);
    t->struct_of = struct_vec_push(structs, s);
    return t;
}

type_t *add_union_type(char *name, bool is_anonymous) {
    return add_struct_union_type(name, TRUE, is_anonymous);
}

type_t *add_struct_type(char *name, bool is_anonymous) {
    return add_struct_union_type(name, FALSE, is_anonymous);
}

void copy_union_member_to_struct(type_t *st, type_t *ut) {
    for (int i=0; i<member_vec_len(ut->struct_of->members); i++) {
        member_t *m = member_vec_get(ut->struct_of->members, i);
        add_struct_member(st, m->name, m->t, TRUE);
    }
}

member_t *add_struct_member(type_t *st, char *name, type_t *t, bool is_union) {
    struct_t *s = st->struct_of;
    if (s == 0) {
        error("adding member to non struct type: %s", st->name);
    }
    member_t m;
    m.name = name;
    m.t = t;

    // todo: alignment to 4 bytes
    int t_size = type_size(t);
    if (is_union) {
        m.offset = s->next_offset;
        if (t_size > st->size - s->next_offset) {
            st->size = s->next_offset + t_size;
        }
    } else {
        m.offset = st->size;
        debug("added struct member %s @%d", name, m.offset);
        st->size += t_size;
        s->next_offset += t_size;
    }

    return member_vec_push(s->members, m);
}

member_t *find_struct_member(type_t *t, char *name) {
    struct_t *s = t->struct_of;
    if (s == 0) {
        error("this type is not struct: %s", t->name);
    }
    for (int i=0; i<member_vec_len(s->members); i++) {
        member_t *m = member_vec_get(s->members, i);
        if (!strcmp(name, m->name)) {
            return m;
        }
    }
    return 0;
}

VEC_HEADER(enum_t, enum_vec)
VEC_BODY(enum_t, enum_vec)

enum_vec enums = 0;

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
    if (!enums) enums = enum_vec_new();

    type_t *t = find_enum_type(name);
    if (t) {
        return t;
    }

    enum_t e;
    e.next_value = 0;
    e.name = name;
    debug("added new enum type: ", name);

    t = add_type("$e", 4, 0, -1);
    t->enum_of = enum_vec_push(enums, e);

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
    if (to == type_long && (from == type_int || from == type_char)) return TRUE;
    if (to == type_int && from == type_char) return TRUE;
    return FALSE;
}
