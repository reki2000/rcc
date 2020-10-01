#include "rsys.h"
#include "rstring.h"
#include "devtool.h"

#include "types.h"
#include "type.h"
#include "var.h"
#include "func.h"
#include "atom.h"

#define NUM_VARS 10000
#define NUM_FRAMES 100

frame_t env[NUM_FRAMES];
int env_top = -1;
int max_offset = 0;


void _enter_var_frame(bool is_function_args) {
    env_top++;
    debug("entering frame:%d", env_top);
    if (env_top >= NUM_FRAMES) {
        error("Too many frames");
    }
    frame_t *f = &env[env_top];
    f->vars = calloc(sizeof(var_t), NUM_VARS);
    f->offset = (env_top == 0) ? 0 : env[env_top - 1].offset;
    f->num_vars = 0;
    f->is_function_args = is_function_args;
}

void enter_function_args_var_frame() {
    _enter_var_frame(TRUE);
}

void enter_var_frame() {
    _enter_var_frame(FALSE);
}

void exit_var_frame() {
    debug("exiting frame:%d", env_top);
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

    if (f->num_vars >= NUM_VARS) {
        error("Too many constants");
    }
    v = &(f->vars[f->num_vars]);
    f->num_vars++;

    v->name = name;
    v->t = t;
    v->is_constant = TRUE;
    v->is_global = (env_top == 0);
    v->has_value = TRUE;
    v->int_value = value;

    debug("add_constant_int: added %d", v->int_value);
    return v;
}

void var_realloc(var_t *v, type_t *t) {
    if (type_size(v->t) != 0) {
        error_s("cannot realloc variable: ", v->name);
    }

    frame_t *f = &env[env_top];

    f->offset += align(type_size(t), 4);
    if (f->offset > max_offset) {
        max_offset = f->offset;
    }
    v->offset = f->offset;
    v->t = t;
}

var_t *add_var(char *name, type_t *t) {
    var_t *v;
    frame_t *f = &env[env_top];

    if (f->num_vars >= NUM_VARS) {
        error("Too many variables");
    }
    v = &(f->vars[f->num_vars]);
    f->num_vars++;

    if (env_top == 0) {
        v->offset = 0;
        v->is_global = TRUE;
    } else  if (f->is_function_args && f->num_vars > ABI_NUM_REGISTER_PASS) {
        v->offset = -ALIGN_OF_STACK * (2 + f->num_vars - ABI_NUM_REGISTER_PASS - 1); // 2 : return address, %rbp
        v->is_global = FALSE;
    } else {
        f->offset += align(type_size(t), 4);
        if (f->offset > max_offset) {
            max_offset = f->offset;
        }
        v->offset = f->offset;
        v->is_global = FALSE;
    }

    v->name = name;
    v->t = t;

    char buf[RCC_BUF_SIZE] = {0};
    strcat(buf, "'");
    strcat(buf, name);
    _strcat3(buf, "' frame[", env_top, "] ");
    _strcat3(buf, "offset:", v->offset, " type:");
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
        char buf[RCC_BUF_SIZE] = {0};
        _strcat3(buf, "env[", pos, "] ");
        _strcat3(buf, "vars:", env[pos].num_vars, "");
        _strcat3(buf, " offset:", env[pos].offset, "");
        debug_s("", buf);
        for (var_ptr = env[pos].vars; var_ptr->name != 0; var_ptr++) {
            char buf[RCC_BUF_SIZE] = {0};
            strcat(buf, var_ptr->name);
            _strcat3(buf, "=", var_ptr->offset, "\n");
            debug_s("", buf);
        }
    }
}

