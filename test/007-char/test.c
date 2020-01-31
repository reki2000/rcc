int main() {
    char *s;
    char buf[100];
    char *d;
    char *p;

    s = "ABCDEF";
    p = d = buf;
    while(*s) {
        *d++ = *s++;
    }
    while(*p) {
        print(*p++);
    }
    return 0;
}
