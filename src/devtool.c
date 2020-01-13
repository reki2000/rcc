
#include "rsys.h"
#include "rstring.h"

void debug(char *str) {
    char buf[1024];
    buf[0] = 0;
    _strcat(buf, str);
    _strcat(buf, "\n");
    _write(2, buf, _strlen(buf));
}

void debug_i(char *str, int val) {
    char buf[1024];
    buf[0] = 0;
    _strcat(buf, str);
    _stritoa(buf, val);
    _strcat(buf, "\n");
    _write(2, buf, _strlen(buf));
}

void debug_s(char *str, char *val) {
    char buf[1024];
    buf[0] = 0;
    _strcat(buf, str);
    _strcat(buf, val);
    _strcat(buf, "\n");
    _write(2, buf, _strlen(buf));
}

void error(char *str) {
    char buf[1024];
    buf[0] = 0;
    _strcat(buf, str);
    _strcat(buf, "\n");
    _write(2, buf, _strlen(buf));
    __exit(1);
}
