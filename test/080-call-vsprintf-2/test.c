typedef __builtin_va_list va_list;

#define va_start __builtin_va_start
#define va_end __builtin_va_end
#define va_args __builtin_va_arg

extern int vsprintf(char *buf, const char *fmt, va_list *v);
extern int strlen(const char *);
extern int write(int fd, const char *buf, int size);

void x(char *fmt, ...) {
    va_list va;

    va_start(va, fmt);
    char buf[200];
    vsprintf(buf, "%s %d %s %d %d %d %d %d\n", va);
    va_end(va);

    write(1, buf, strlen(buf));
}

int main(int argc, char **argv) {
    x("", "1",2,"3",4,5,6,7,8);
}
