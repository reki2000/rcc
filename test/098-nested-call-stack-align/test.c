extern int snprintf(char *buf, long size, char *fmt, int v);
extern long strlen(char *s);

char buf[64];

int val(int n) {
    return n * 2;
}

int main() {
    // a call nested inside another call's arguments must not break
    // the 16-byte %rsp alignment required at the callee's entry
    print(val(1) + snprintf(buf, 64, "%d", val(11)));
    print(snprintf(buf, 64, "%d", val(1) + val(2)));
    print(strlen(buf));
    return 0;
}
