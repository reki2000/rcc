int _strlen(char *str) {
    int len = 0;
    while (*str != 0) {
        str++;
        len++;
    }
    return len;
}

int _strcat(char *dst, char *str) {
    while (*dst != 0) {
        dst++;
    }

    int count = 0;
    while (*str != 0) {
        *dst++ = *str++;
        count++;
    }
    *dst = 0;
    return count;
}

void _stritoa(char *dst, int i) {
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
    dst += _strlen(dst);
    for (;pos >= 0; pos--) {
        *dst = buf[pos];
        dst++;
    }
    *dst = 0;
}