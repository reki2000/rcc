#include "rsys.h"
#include "rstring.h"
#include "devtool.h"

#define NUM_GLOBAL_STRINGS 1024
#define NUM_GLOBAL_ARRAY (1024*1024)

char *gstrings[NUM_GLOBAL_STRINGS];
int gstrings_len = 0;

int add_global_string(char *name) {
    if (gstrings_len >= NUM_GLOBAL_STRINGS) {
        error("Too many strings!");
    }
    for (int i=0; i<gstrings_len; i++) {
        if (!strcmp(name, gstrings[i])) {
            return i;
        }
    }
    gstrings[gstrings_len] = name;
    return gstrings_len++;
}

char *find_global_string(int index) {
    return gstrings[index];
}

// 'global_array' is implemented as containing multiple lists in a array
// [length] [val0] [val1] ... [length] [val0] [val1] ...


int global_array[NUM_GLOBAL_ARRAY];
int global_array_len = 0;

int alloc_global_array() {
    if (global_array_len >= NUM_GLOBAL_ARRAY) {
        error("too much global array initializations");
    }
    global_array[global_array_len] = 0;
    return global_array_len++;
}

void add_global_array(int pos, int value) {
    int new_pos = pos + global_array[pos] + 1;
    if (global_array_len > new_pos) {
        error_i("size of the global array already fixed: ", pos);
    }
    global_array[pos]++;
    global_array[new_pos] = value;
    global_array_len++;
}

int get_global_array_length(int pos) {
    if (pos >= global_array_len) {
        error_i("global array index out of range: ", pos);
    }
    return global_array[pos];
}

int get_global_array(int pos, int offset) {
    if (pos + offset + 1 >= global_array_len) {
        error_i("global array index out of range: ", pos);
    }
    if (offset >= global_array[pos]) {
        error_i("global array index out of range:" , pos);
    }
    return global_array[pos + offset + 1];
}
