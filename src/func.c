
#include "types.h"
#include "rstring.h"
#include "devtool.h"
#include "rsys.h"
#include "vec.h"

#include "type.h"
#include "var.h"

#include "func.h"

VEC_BODY(func, func_vec)

func_vec functions = 0;

func *find_func_name(char *name) {
    for (int i=0; i<func_vec_len(functions); i++) {
        func *f = func_vec_get(functions, i);
        if (!strcmp(f->name, name)) {
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
    if (!functions) functions = func_vec_new();
    func *f = find_function(name, ret_type, argc, argv);
    if (!f) {
        func fn;
        fn.name = name;
        fn.ret_type = ret_type;
        fn.argc = argc;
        fn.argv = argv;
        fn.is_external = is_external;
        fn.is_variadic = is_variadic;
        fn.max_offset = 0;
        fn.body_pos = 0;
        f = func_vec_push(functions, fn);
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

