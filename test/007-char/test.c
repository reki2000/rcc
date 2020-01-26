int main() {
    char *s;
    char *d;
    char *p;
    int b;
    int c;
    char a;
    b = 0;
    c = 0;
    s = "ABCDEF";
    d = &a;
    p = d;
    while(*s) {
        *d++ = *s++;
    }
    while(*p) {
        print(*p++);
    }
    return 0;
}
