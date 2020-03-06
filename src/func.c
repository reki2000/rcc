
#include "rstring.h"
#include "devtool.h"

#include "types.h"
#include "type.h"
#include "var.h"

#include "func.h"

func functions[1000];
int function_pos = 0;
int function_len = 0;

func *find_func_name(char *name) {
    for (int i=0; i<function_len; i++) {
        func *f = &functions[i];
        if (strcmp(f->name, name) == 0) {
            return f;
        }
    }
    return 0;
}

func *find_function(char *name, type_t *ret_type, int argc, var_t *argv) {
    func *f = find_func_name(name);
    if (!f) {
        return 0;
    }
    if (f->ret_type == ret_type && f->argc == argc) {
        int j;
        for (j=0; j<argc; j++) {
            if (f->argv[j].t != argv[j].t) {
                break;
            }
        }
        if (argc == j) {
            return f;
        }
    }
    error_s("function is already declared but types are not matched: ", name);
    return 0;
}

func *add_function(char *name, type_t *ret_type, int argc, var_t *argv) {
    if (function_len >= 1000) {
        error("Too many functions");
    }
    func *f = find_function(name, ret_type, argc, argv);
    if (!f) {
        f = &functions[function_len++];
        f->name = name;
        f->ret_type = ret_type;
        f->argc = argc;
        f->argv = argv;
    }
    debug_s("added function: ", f->name);
    return f;
}

func *func_set_body(func *f, int argc, var_t *argv, int pos, int max_offset) {
    f->argc = argc;
    f->argv = argv;
    f->body_pos = pos;
    f->max_offset = max_offset;
    debug_s("added function body: ", f->name);
    return f;
}

