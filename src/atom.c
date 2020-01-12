#include "rsys.h"
#include "rstring.h"
#include "devtool.h"

#include "types.h"
#include "atom.h"

atom program[10000];
int atom_pos = 1;

int alloc_atom(int size) {
    int current;
    current = atom_pos;
    if (atom_pos + size >= 100) {
        error("Source code too long");
    }
    atom_pos += size;
    return current;
}

void dump_atom(int pos) {
    char buf[1024];
    buf[0] = 0;
    _strcat(buf, "[");
    _stritoa(buf, pos);
    _strcat(buf, "] type:");
    _stritoa(buf, program[pos].type);
    _strcat(buf, " value:");
    if (program[pos].type == TYPE_IDENT) {
        _strcat(buf, program[pos].value.str_value);
    } else {
        _stritoa(buf, program[pos].value.int_value);
    }
    _strcat(buf, "\n");
    _write(2, buf, _strlen(buf));
}

void dump_atom_all() {
    int i;
    for (i=1; i<atom_pos; i++) {
        dump_atom(i);
    }
}

void build_int_atom(int pos, int type, int value) {
    program[pos].type = type;
    program[pos].value.int_value = value;
}

void build_string_atom(int pos, int type, char * value) {
    program[pos].type = type;
    program[pos].value.str_value = value;
}

void build_pos_atom(int pos, int type, int value) {
    program[pos].type = type;
    program[pos].value.atom_pos = value;
}
