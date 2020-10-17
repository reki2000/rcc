#include "types.h"
#include "rsys.h"
#include "rstring.h"
#include "devtool.h"
#include "vec.h"

#include "type.h"
#include "var.h"
#include "func.h"
#include "atom.h"

VEC_HEADER(frame_t, frame_vec)
VEC_BODY(frame_t, frame_vec)

VEC_BODY(var_t, var_vec)

frame_vec env = 0;
int max_offset = 0;

void dump_env() {
    debug("env ----");
    for (int pos = frame_vec_len(env) - 1; pos >= 0; pos--) {
        frame_t *f = frame_vec_get(env, pos);
        debug("env[%d] vars:%d offset:%d", pos, var_vec_len(f->vars), f->offset);
        for (int i=0; i<var_vec_len(f->vars); i++) {
            var_t *v = var_vec_get(f->vars, i);
            debug(" %s: @%d", v->name, v->offset);
        }
    }
    debug("--------");
}

void _enter_var_frame(bool is_function_args) {
    if (!env) env = frame_vec_new();

    int env_top = frame_vec_len(env);
    debug("entering frame:%d", env_top);

    frame_t f;
    f.vars = var_vec_new();
    f.offset = (env_top == 0) ? 0 : frame_vec_get(env, env_top - 1)->offset;
    f.is_function_args = is_function_args;
    f.num_reg_vars = 0;
    f.num_stack_vars = 0;
    frame_vec_push(env, f);
}

void enter_function_args_var_frame() {
    _enter_var_frame(TRUE);
}

void enter_var_frame() {
    _enter_var_frame(FALSE);
}

void exit_var_frame() {
    int env_top = frame_vec_len(env) - 1;
    debug("exiting frame:%d", env_top);
    if (env_top < 0) {
        error("Invalid frame_t exit");
    }
    frame_vec_pop(env);
}

int var_max_offset() {
    return max_offset;
}

void reset_var_max_offset() {
    max_offset = 0;
}

frame_t *get_top_frame() {
    if (frame_vec_len(env) == 0) {
        error("empty environment");
    }
    return frame_vec_top(env);
}

frame_t *get_global_frame() {
    if (frame_vec_len(env) == 0) {
        error("empty global environment");
    }
    return frame_vec_get(env, 0);
}


var_t *add_constant_int(char *name, type_t*t, int value) {
    var_t v;
    v.name = name;
    v.t = t;
    v.is_constant = TRUE;
    v.is_global = (frame_vec_len(env) == 1);
    v.has_value = TRUE;
    v.int_value = value;
    debug("add_constant_int: added %d", v.int_value);

    frame_t *f = get_top_frame();
    return var_vec_push(f->vars, v);
}

void var_realloc(var_t *v, type_t *t) {
    if (type_size(v->t) != 0) {
        error("cannot realloc variable: %s", v->name);
    }

    frame_t *f = get_top_frame();
    f->offset += align(type_size(t), 4);
    if (f->offset > max_offset) {
        max_offset = f->offset;
    }
    v->offset = f->offset;
    v->t = t;
}

void add_register_save_area() {
    frame_t *f = get_top_frame();
    f->offset = align(f->offset, ALIGN_OF_STACK) + ABI_REG_SAVE_AREA_SIZE;
    if (f->offset > max_offset) {
        max_offset = f->offset;
    }
}

var_t *add_var(char *name, type_t *t) {
    frame_t *f = get_top_frame();
    var_t v;

    v.name = name;
    v.t = t;
    v.is_global = FALSE;
    v.is_constant = FALSE;
    v.is_external = FALSE;
    v.has_value = FALSE;

    if (frame_vec_len(env) == 1) {
        v.offset = 0;
        v.is_global = TRUE;
    } else if (f->is_function_args && t->struct_of) {
        int size = type_size(t);
        if (size <= 16 && f->num_reg_vars < ABI_NUM_GP - 2) {
            f->offset += align(type_size(t), 8);
            max_offset = max(f->offset, max_offset);
            v.offset = f->offset;
            f->num_reg_vars += (size > 8) ? 2 : 1;
        } else {
            v.offset = -ALIGN_OF_STACK * (2 + f->num_stack_vars); // 2 : return address, %rbp
            f->num_stack_vars += align(size,8) / 8;
        }
    } else if (f->is_function_args && f->num_reg_vars >= ABI_NUM_GP) {
        v.offset = -ALIGN_OF_STACK * (2 + f->num_stack_vars); // 2 : return address, %rbp
        f->num_stack_vars++;
    } else {
        f->offset += align(type_size(t), 4);
        max_offset = max(f->offset, max_offset);
        v.offset = f->offset;
        f->num_reg_vars++;
    }

    var_t *v_ptr = var_vec_push(f->vars, v);
    char buf[RCC_BUF_SIZE] = {0};
    dump_type(buf, t);
    debug("add_var:'%s' frame[%d] offset:%d type:%s", name, frame_vec_len(env)-1, v.offset, buf);

    return v_ptr;
}

var_t *find_var_in_current_frame(char *name) {
    frame_t *f = get_top_frame();
    for (int i=0; i<var_vec_len(f->vars); i++) {
        var_t *v = var_vec_get(f->vars, i);
        if (!strcmp(name, v->name)) {
            return v;
        }
    }
    return 0;
}

var_t *find_var(char *name) {
    for (int env_pos = frame_vec_len(env) - 1; env_pos >= 0; env_pos--) {
        frame_t *f = frame_vec_get(env, env_pos);
        for (int i=0; i<var_vec_len(f->vars); i++) {
            var_t *v = var_vec_get(f->vars, i);
            if (!strcmp(name, v->name)) {
                return v;
            }
        }
    }
    return 0;
}

