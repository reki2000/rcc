#include "rstring.h"
#include "types.h"

void _strcati(char *dst, int i) {
    char buf[100];
    int pos = 0;
    int val = i;
    for (;;) {
        int mod = val % 10;
        buf[pos++] = '0' +((mod >= 0) ? mod : -mod);
        val /= 10;
        if (val == 0) {
            break;
        }
    }

    dst += strlen(dst);
    if (i < 0) {
        *dst++ = '-';
    }

    for (pos--; pos >= 0; pos--) {
        *dst++ = buf[pos];
    }
    *dst = 0;
}

void _strcat3(char buf[], char *s1, int i, char *s2) {
    strcat(buf, s1);
    _strcati(buf, i);
    strcat(buf, s2);
}
