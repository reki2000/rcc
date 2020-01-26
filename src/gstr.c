#include "rsys.h"
#include "rstring.h"
#include "devtool.h"

char *gstrings[1024];
int gstrings_len = 0;

int add_global_string(char *name) {
    if (gstrings_len >= 1024) {
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
