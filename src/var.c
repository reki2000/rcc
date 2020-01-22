#include "rsys.h"
#include "rstring.h"
#include "devtool.h"

#include "types.h"
#include "type.h"
#include "var.h"
#include "func.h"
#include "atom.h"

frame env[100];
int env_top = 0;
int max_offset = 0;

void enter_var_frame() {
    env_top++;
    debug_i("entering frame:", env_top);
    if (env_top >= 100) {
        error("Too many frames");
    }
    env[env_top].vars = _calloc(sizeof(var), 100);
    env[env_top].offset = env[env_top - 1].offset;
    env[env_top].num_vars = 0;
}

void exit_var_frame() {
    debug_i("exiting frame:", env_top);
    if (env_top == 0) {
        error("Invalid frame exit");
    }
    env_top--;
}

int var_max_offset() {
    return max_offset;
}

void reset_var_max_offset() {
    max_offset = 0;
}

frame *get_top_frame() {
    return &env[env_top];
}

void add_var(char *name, type_s *t) {
    var *v;
    frame *f = &env[env_top];

    if (f->num_vars >= 100) {
        error("Too many variables");
    }
    v = &(f->vars[f->num_vars]);
    f->num_vars++;
    f->offset += t->size;
    if (f->offset > max_offset) {
        max_offset = f->offset;
    }

    v->name = name;
    v->size = t->size;
    v->offset = f->offset;
    v->t = t;

    debug_i("add_var: added @", f->offset);
}

int find_var_offset(char *name) {
    var *v;
    v = find_var(name);
    if (v != 0) {
        return v->offset;
    }
    return 0;
}

var *find_var(char *name) {
    int env_pos;
    for (env_pos = env_top; env_pos > 0; env_pos--) {
        var *var_ptr;
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
    var *var_ptr;
    for (pos = env_top; pos > 0; pos--) {
        char buf[100];
        buf[0] = 0;
        _strcat3(buf, "env[", pos, "] ");
        _strcat3(buf, "vars:", env[pos].num_vars, "");
        _strcat3(buf, " offset:", env[pos].offset, "\n");
        _write(2, buf, strlen(buf));
        for (var_ptr = env[pos].vars; var_ptr->name != 0; var_ptr++) {
            char buf[100];
            buf[0] = 0;
            strcat(buf, var_ptr->name);
            _strcat3(buf, "=", var_ptr->offset, "\n");
            _write(2, buf, strlen(buf));
        }
    }
}

