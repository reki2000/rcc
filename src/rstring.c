#include "rstring.h"
#include "types.h"

void _strcati(char *dst, int i) {
    char buf[100];
    int pos = 0;
    int val = i;
    bool is_negative = FALSE;
    if (val == -2147483648) {
        // INT_MIN cannot be negated 
        strcat(dst, "-2147483648");
        return;
    }
    if (val < 0) {
        val = -val;
        is_negative = TRUE;
    }

    do {
        int mod = val % 10;
        buf[pos++] = '0' + mod;
        val /= 10;
    } while (val > 0);

    dst += strlen(dst);
    if (is_negative) {
        *dst++ = '-';
    }

    for (pos--; pos >= 0; pos--) {
        *dst++ = buf[pos];
    }
    *dst = 0;
}

void _strcat3(char *buf, char *s1, int i, char *s2) {
    strcat(buf, s1);
    _strcati(buf, i);
    strcat(buf, s2);
}
