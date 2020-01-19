#include "rstring.h"
#include "devtool.h"
#include "types.h"
#include "type.h"

type_s types[1024];
int types_pos = 0;

void init_types() {
    add_type("", 8, 0);    // for pointer
    add_type("int", 4, 0);
}

void dump_type(type_s *t) {
    char buf[100];
    buf[0] = 0;
    _strcat(buf, "type ");
    _strcat(buf, t->name);
    _strcat_s_i_s(buf, " size:", t->size, "");
    _strcat_s_i_s(buf, " ptr_to:", (int)(t->ptr_to), "");
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
        if (_strcmp(name, types[i].name) == 0) {
            return &types[i];
        }
    }
    return 0;
}
