#include "rstring.h"
#include "types.h"

void _strcati(char *dst, int i) {
    char buf[RCC_BUF_SIZE];
    int pos = 0;
    bool is_negative = FALSE;
    if (i < 0) {
        is_negative = TRUE;
        i = -i;
    }
    for (;;) {
        buf[pos] = '0' + (i % 10);
        i /= 10;
        if (i==0 || pos >= 100) {
            break;
        }
        pos++;
    }
    dst += strlen(dst);
    if (is_negative) {
        *dst++ = '-';
        *dst = 0;
    }
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
