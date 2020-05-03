
#include "rsys.h"
#include "rstring.h"
#include "types.h"

#include "token.h"
#include "file.h"

typedef enum {
    WARN,
    ERROR,
    DEBUG,
    INFO,
    NONE
} level_e;

char *color_red = "\e[31m";
char *color_green = "\e[32m";
char *color_yellow = "\e[33m";
char *color_blue = "\e[34m";
char *color_magenta = "\e[35m";
char *color_cyan = "\e[36m";
char *color_white = "\e[37m";

void _log(level_e level, char *message) {
    char *color_str[5] = {color_red, color_red, "", color_yellow, ""};
    char *level_str[5] = {"WARN ", "ERROR", "DEBUG", "INFO ", ""};

    char buf[1024] = {0};
    strcat(buf, color_str[level]);
    strcat(buf, level_str[level]);
    strcat(buf, ": ");
    strcat(buf, message);
    strcat(buf, "\n");
    if (color_str[level] != color_white) {
        strcat(buf, color_white);
    }
    write(2, buf, strlen(buf));
}

void debug(char *str) {
    _log(DEBUG, str);
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
    _log(ERROR, str);
    _log(NONE, "");
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

void warning(char *str) {
    _log(WARN, str);
}

void warning_i(char *str, int val) {
    char buf[1024] = {0};
    _strcat3(buf, str, val, "");
    warning(buf);
}

void warning_s(char *str, char *val) {
    char buf[1024] = {0};
    strcat(buf, str);
    strcat(buf, val);
    warning(buf);
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
