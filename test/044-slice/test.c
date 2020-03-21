extern void *malloc(int);
extern void write(int, char *, int);

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

int main() {
    char *s = _slice("hogehoge", 4);
    write(1, s, 4);
    return 0;
}
