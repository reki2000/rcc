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
        if (strcmp(name, types[i].name) == 0) {
            return &types[i];
        }
    }
    return 0;
}
