
#include "types.h"
#include "rstring.h"
#include "devtool.h"
#include "vec.h"

#include "type.h"
#include "var.h"

#include "func.h"

#define NUM_FUNCTIONS 1000

func functions[NUM_FUNCTIONS];
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

func *find_function(char *name, type_t *ret_type, int argc, var_vec argv) {
    func *f = find_func_name(name);
    if (!f) {
        return 0;
    }
    if (f->ret_type == ret_type && f->argc == argc) {
        int j;
        for (j=0; j<argc; j++) {
            if (var_vec_get(f->argv, j)->t != var_vec_get(argv, j)->t) {
                break;
            }
        }
        if (argc == j) {
            return f;
        }
    }
    error("function is already declared but types are not matched: %s", name);
    return 0;
}

func *add_function(char *name, type_t *ret_type, bool is_external, bool is_variadic, int argc, var_vec argv) {
    if (function_len >= NUM_FUNCTIONS) {
        error("Too many functions");
    }
    func *f = find_function(name, ret_type, argc, argv);
    if (!f) {
        f = &functions[function_len++];
        f->name = name;
        f->ret_type = ret_type;
        f->argc = argc;
        f->argv = argv;
        f->is_external = is_external;
        f->is_variadic = is_variadic;
    }
    debug("added function: %s", f->name);
    return f;
}

func *func_set_body(func *f, int argc, var_vec argv, int pos, int max_offset) {
    f->argc = argc;
    f->argv = argv;
    f->body_pos = pos;
    f->max_offset = max_offset;
    debug("added function body: %s", f->name);
    return f;
}

