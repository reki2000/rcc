#include "rsys.h"
#include "rstring.h"
#include "devtool.h"

#include "types.h"
#include "type.h"
#include "var.h"
#include "func.h"
#include "atom.h"

frame_t env[100];
int env_top = -1;
int max_offset = 0;

void enter_var_frame() {
    env_top++;
    debug_i("entering frame:", env_top);
    if (env_top >= 100) {
        error("Too many frames");
    }
    env[env_top].vars = calloc(sizeof(var_t), 100);
    env[env_top].offset = env[env_top - 1].offset;
    env[env_top].num_vars = 0;
}

void exit_var_frame() {
    debug_i("exiting frame:", env_top);
    if (env_top < 0) {
        error("Invalid frame_t exit");
    }
    env_top--;
}

int var_max_offset() {
    return max_offset;
}

void reset_var_max_offset() {
    max_offset = 0;
}

frame_t *get_top_frame() {
    return &env[env_top];
}

var_t *add_constant_int(char *name, type_t*t, int value) {
    var_t *v;
    frame_t *f = &env[env_top];

    if (f->num_vars >= 100) {
        error("Too many variables");
    }
    v = &(f->vars[f->num_vars]);
    f->num_vars++;

    v->name = name;
    v->t = t;
    v->is_constant = TRUE;
    v->is_global = (env_top == 0);
    v->has_value = TRUE;
    v->int_value = value;

    debug_i("add_constant_int: added ", v->int_value);
    return v;

}

var_t *add_var(char *name, type_t *t) {
    var_t *v;
    frame_t *f = &env[env_top];

    if (f->num_vars >= 100) {
        error("Too many variables");
    }
    v = &(f->vars[f->num_vars]);
    f->num_vars++;

    f->offset += align(t->size, 4);
    if (f->offset > max_offset) {
        max_offset = f->offset;
    }

    v->name = name;
    v->offset = f->offset;
    v->t = t;
    v->is_global = (env_top == 0);

    char buf[100] = {0};
    strcat(buf, "'");
    strcat(buf, name);
    _strcat3(buf, "' frame[", env_top, "] ");
    _strcat3(buf, "offset:", f->offset, " type:");
    dump_type(buf, t);
    debug_s("add_var:", buf);

    return v;
}

int find_var_offset(char *name) {
    var_t *v;
    v = find_var(name);
    if (v != 0) {
        return v->offset;
    }
    return 0;
}

var_t *find_var_in_current_frame(char *name) {
    if (env_top < 0) {
        error("cannot find variable in frame < 0");
    }
    var_t *var_ptr;
    for (var_ptr = env[env_top].vars; var_ptr->name != 0; var_ptr++) {
        if (strcmp(name, var_ptr->name) == 0) {
            return var_ptr;
        }
    }
    return 0;
}

var_t *find_var(char *name) {
    int env_pos;
    for (env_pos = env_top; env_pos >= 0; env_pos--) {
        var_t *var_ptr;
        for (var_ptr = env[env_pos].vars; var_ptr->name != 0; var_ptr++) {
            if (strcmp(name, var_ptr->name) == 0) {
                return var_ptr;
            }
        }
    }
    return 0;
}

void dump_env() {
    int pos;
    var_t *var_ptr;
    for (pos = env_top; pos >= 0; pos--) {
        char buf[100];
        buf[0] = 0;
        _strcat3(buf, "env[", pos, "] ");
        _strcat3(buf, "vars:", env[pos].num_vars, "");
        _strcat3(buf, " offset:", env[pos].offset, "");
        debug_s("", buf);
        for (var_ptr = env[pos].vars; var_ptr->name != 0; var_ptr++) {
            char buf[100];
            buf[0] = 0;
            strcat(buf, var_ptr->name);
            _strcat3(buf, "=", var_ptr->offset, "\n");
            debug_s("", buf);
        }
    }
}

