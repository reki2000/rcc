
#include "rsys.h"
#include "rstring.h"
#include "types.h"

#include "type.h"
#include "token.h"
#include "var.h"
#include "func.h"
#include "atom.h"

void debug(char *str) {
    char buf[1024];
    buf[0] = 0;
    strcat(buf, str);
    strcat(buf, "\n");
    _write(2, buf, strlen(buf));
}

void debug_i(char *str, int val) {
    char buf[1024];
    buf[0] = 0;
    _strcat3(buf, str, val, "\n");
    _write(2, buf, strlen(buf));
}

void debug_s(char *str, char *val) {
    char buf[1024];
    buf[0] = 0;
    strcat(buf, str);
    strcat(buf, val);
    strcat(buf, "\n");
    _write(2, buf, strlen(buf));
}

void error(char *str) {
    debug(str);
    dump_tokens();
    dump_atom_all();
    __exit(1);
}

void error_i(char *str, int val) {
    debug_i(str, val);
    dump_tokens();
    dump_atom_all();
    __exit(1);
}

void error_s(char *str, char *val) {
    debug_s(str, val);
    dump_tokens();
    dump_atom_all();
    __exit(1);
}

char *_slice(char *src, int count) {
    char *ret = _malloc(count + 1);
    char *d = ret;
    for (;count>0;count--) {
        if (*src == 0) {
            break;
        }
        *d++ = *src++;
    }
    *d = 0;
    return ret;
}
