
#include "rsys.h"
#include "rstring.h"
#include "types.h"

#include "token.h"

void _log(char *level, char *message) {
    char buf[1024] = {0};
    strcat(buf, level);
    strcat(buf, ": ");
    strcat(buf, message);
    strcat(buf, "\n");
    write(2, buf, strlen(buf));
}

void debug(char *str) {
    _log("DEBUG", str);
}

void debug_i(char *str, int val) {
    char buf[1024] = {0};
    _strcat3(buf, str, val, "");
    debug(buf);
}

void debug_s(char *str, char *val) {
    char buf[1024] = {0};
    strcat(buf, str);
    strcat(buf, val);
    debug(buf);
}

void error(char *str) {
    _log("ERROR", str);
    _log("","");
    dump_tokens();
    exit(1);
}

void error_i(char *str, int val) {
    char buf[1024] = {0};
    _strcat3(buf, str, val, "");
    error(buf);
}

void error_s(char *str, char *val) {
    char buf[1024] = {0};
    strcat(buf, str);
    strcat(buf, val);
    error(buf);
}

char *_slice(char *src, int count) {
    char *ret = malloc(count + 1);
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

int align(int addr, int size) {
    int mod = addr % size;
    if (!mod) {
        return addr;
    }
    return addr + (size - mod);
}
