typedef __builtin_va_list va_list;

#define va_start(va,n) __builtin_va_start(va,n)
#define va_end(va) __builtin_va_end(va)
#define va_arg(va,ret_type) __builtin_va_arg(va,ret_type)

extern int vsprintf(char *buf, const char *fmt, va_list *v);
extern int strlen(const char *);
extern int write(int fd, const char *buf, int size);
extern void *malloc(long);

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
