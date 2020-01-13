#include "rsys.h"
#include "rstring.h"
#include "devtool.h"

#include "atom.h"
#include "var.h"

frame env[100];
int env_top = 0;

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

void add_var(char *name) {
    var *var_ptr;
    frame *f = &env[env_top];

    if (f->num_vars >= 100) {
        error("Too many variables");
    }
    var_ptr = &(f->vars[f->num_vars]);
    f->num_vars++;
    f->offset += 8;

    var_ptr->name = name;
    var_ptr->size = 8;
    var_ptr->offset = f->offset;

    debug_i("add_var: added @", f->offset);
}

int find_var_offset(char *name) {
    int env_pos;
    for (env_pos = env_top; env_pos > 0; env_pos--) {
        var *var_ptr;
        for (var_ptr = env[env_pos].vars; var_ptr->name != 0; var_ptr++) {
            if (_strcmp(name, var_ptr->name) == 0) {
                return var_ptr->offset;
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
        _strcat(buf, "env[");
        _stritoa(buf, pos);
        _strcat(buf, "] vars:");
        _stritoa(buf, env[pos].num_vars);
        _strcat(buf, " offset:");
        _stritoa(buf, env[pos].offset);
        _strcat(buf, "\n");
        _write(2, buf, _strlen(buf));
        for (var_ptr = env[pos].vars; var_ptr->name != 0; var_ptr++) {
            char buf[100];
            buf[0] = 0;
            _strcat(buf, var_ptr->name);
            _strcat(buf, "=");
            _stritoa(buf, var_ptr->offset);
            _strcat(buf, "\n");
            _write(2, buf, _strlen(buf));
        }
    }
}