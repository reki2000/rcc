
#include "rsys.h"
#include "rstring.h"
#include "types.h"

#include "token.h"
#include "file.h"

typedef struct {
    int g_offset;
    int f_offset;
    char *stack;
    char *reg;
    long num_fp;
} va_list;

void va_start(va_list* va, char *arg1, int size1) {
    va->stack = arg1 + size1 + 8 + 8;
    va->reg = arg1 - (8- size1) - 8 * 6;
    va->g_offset = 8;
    va->f_offset = 48;
    va->num_fp = 0;
}

extern int vsprintf(char *buf, const char *fmt, va_list *v);
extern int sprintf(char *buf, const char *fmt, ...);

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
    sprintf(buf, "%s%s: %s%s\n", tty? color_str[level]:"", level_str[level], message, tty? color_white:"");
    write(2, buf, strlen(buf));
}

void debug(char *fmt, ...) {
    va_list va;
    va_start(&va, (char *)&fmt, sizeof(fmt));

    char buf[RCC_BUF_SIZE];
    vsprintf(buf, fmt, &va);
    _log(DEBUG, buf);
}

void debug_s(char *str, char *val) {
    char buf[RCC_BUF_SIZE] = {0};
    strcat(buf, str);
    strcat(buf, val);
    debug(buf);
}

void error(char *fmt, ...) {
    va_list va;
    va_start(&va, (char *)&fmt, sizeof(fmt));

    char buf[RCC_BUF_SIZE];
    vsprintf(buf, fmt, &va);
    _log(ERROR, buf);
    _log(NONE, "");
    dump_tokens();
    exit(1);
}

void error_i(char *str, int val) {
    char buf[RCC_BUF_SIZE] = {0};
    _strcat3(buf, str, val, "");
    error(buf);
}

void error_s(char *str, char *val) {
    char buf[RCC_BUF_SIZE] = {0};
    strcat(buf, str);
    strcat(buf, val);
    error(buf);
}

void warning(char *fmt, ...) {
    va_list va;
    va_start(&va, (char *)&fmt, sizeof(fmt));

    char buf[RCC_BUF_SIZE];
    vsprintf(buf, fmt, &va);
    _log(WARN, buf);
}

void warning_i(char *str, int val) {
    char buf[RCC_BUF_SIZE] = {0};
    _strcat3(buf, str, val, "");
    warning(buf);
}

void warning_s(char *str, char *val) {
    char buf[RCC_BUF_SIZE] = {0};
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
