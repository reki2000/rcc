
#include "types.h"
#include "rsys.h"
#include "rstring.h"

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
    char *color_str[] = {color_red, color_red, "", color_yellow, ""};
    char *level_str[] = {"WARN ", "ERROR", "DEBUG", "INFO ", ""};

    char buf[RCC_BUF_SIZE];
    bool tty = FALSE; // isatty(2);
    snprintf(buf, RCC_BUF_SIZE, "%s%s: %s%s\n", tty? color_str[level]:"", level_str[level], message, tty? color_white:"");
    write(2, buf, strlen(buf));
}

void debug(char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    char buf[RCC_BUF_SIZE];
    vsnprintf(buf, RCC_BUF_SIZE, fmt, va);
    va_end(va);
    _log(DEBUG, buf);
}

void error(char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    char buf[RCC_BUF_SIZE];
    vsnprintf(buf, RCC_BUF_SIZE, fmt, va);
    va_end(va);
    _log(ERROR, buf);
    _log(NONE, "");
    dump_tokens();
    exit(1);
}

void warning(char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    char buf[RCC_BUF_SIZE];
    vsnprintf(buf, RCC_BUF_SIZE, fmt, va);
    va_end(va);
    _log(WARN, buf);
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
