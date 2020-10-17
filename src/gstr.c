#include "types.h"
#include "rsys.h"
#include "rstring.h"
#include "devtool.h"
#include "vec.h"

char_p_vec gstrings = 0;

int add_global_string(char *name) {
    if (!gstrings) gstrings = char_p_vec_new();
    for (int i=0; i<char_p_vec_len(gstrings); i++) {
        if (!strcmp(name, *char_p_vec_get(gstrings,i))) {
            return i;
        }
    }
    char_p_vec_push(gstrings, name);
    return char_p_vec_len(gstrings) - 1;
}

char *find_global_string(int index) {
    if (!gstrings) gstrings = char_p_vec_new();
    char **result = char_p_vec_get(gstrings, index);
    return (result) ? *result : 0;
}

VEC_HEADER(int_vec, int_vec_vec)
VEC_BODY(int_vec, int_vec_vec)

int_vec_vec global_array = 0;

int alloc_global_array() {
    if (!global_array) global_array = int_vec_vec_new();
    
    int_vec_vec_push(global_array, int_vec_new());
    return int_vec_vec_len(global_array) - 1;
}

void add_global_array(int pos, int value) {
    int_vec_push(*int_vec_vec_get(global_array, pos), value);
}

int get_global_array_length(int pos) {
    int_vec *item = int_vec_vec_get(global_array, pos);
    if (!item) {
        error("global array index out of range:%d", pos);
    }
    return int_vec_len(*item);
}

int get_global_array(int pos, int offset) {
    int_vec *item = int_vec_vec_get(global_array, pos);
    if (!item) {
        error("global array pos out of range:%d", pos);
    }
    int *val = int_vec_get(*item, offset);
    if (!val) {
        error("global array offset out of range:%d", offset);
    }
    return *val;
}
