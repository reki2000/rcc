int main() {
    char buf[100];
    char *d;
    char *s;

    s = "ABCDEF\n";
    d = buf;
    while(*s) {
        *d++ = *s++;
    }
    return 0;
}
