#include "rstring.h"

void _strcati(char *dst, int i) {
    char buf[100];
    int pos = 0;
    for (;;) {
        buf[pos] = '0' + (i % 10);
        i /= 10;
        if (i==0 || pos >= 100) {
            break;
        }
        pos++;
    }
    dst += strlen(dst);
    for (;pos >= 0; pos--) {
        *dst = buf[pos];
        dst++;
    }
    *dst = 0;
}

void _strcat3(char buf[], char *s1, int i, char *s2) {
    strcat(buf, s1);
    _strcati(buf, i);
    strcat(buf, s2);
}
