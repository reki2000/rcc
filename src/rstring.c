#include "types.h"
#include "rstring.h"

bool is_alpha(int ch) {
    return (ch >= 'a' && ch <= 'z')
        || (ch >= 'A' && ch <= 'Z');
}

bool is_digit(int ch) {
    return (ch >= '0' && ch <= '9');
}

bool is_space(int c) {
    return (c == ' ' || c == '\t' || c == '\n');
}

char unescape_char(char escaped_char) {
    char c;
    switch (escaped_char) {
        case 'n': c = '\n'; break;
        case '0': c = '\0'; break;
        case 't': c = '\t'; break;
        case 'r': c = '\r'; break;
        case 'a': c = '\a'; break;
        case 'b': c = '\b'; break;
        case 'e': c = '\e'; break;
        case 'f': c = '\f'; break;
        case '"': c = '"'; break;
        case '\'': c = '\''; break;
        case '\\': c = '\\'; break;
        default: return -1;
    }
    return c;
}

void escape_string(char *buf, const char *str) {
    char *d = buf;
    while (*str) {
        switch(*str) {
            case '\n': *d++='\\'; *d = 'n'; break;
            case '\r': *d++='\\'; *d = 'r'; break;
            case '\t': *d++='\\'; *d = 't'; break;
            case '\f': *d++='\\'; *d = 'f'; break;
            case '\a': *d++='\\'; *d = 'a'; break;
            case '\b': *d++='\\'; *d = 'b'; break;
            case '\e': *d++='\\'; *d = 'e'; break;
            case '\"': *d++='\\'; *d = '"'; break;
            case '\'': *d++='\\'; *d = '\''; break;
            case '\\': *d++='\\'; *d = '\\'; break;
            default: *d = *str;
        }
        str++;
        d++;
    }
    *d = '\0';
}

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
