typedef __builtin_va_list va_list;

#define va_start(va,n) __builtin_va_start(va,n)
#define va_end(va) __builtin_va_end(va)
#define va_arg(va,ret_type) __builtin_va_arg(va,ret_type)

extern int vsprintf(char *, const char *fmt, va_list *v);
extern int puts(const char *);
extern int strlen(const char *);
extern void *malloc(long);

void x(char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    char buf[100]; 
    vsprintf(buf, fmt, va);
    va_end(va);
    puts(buf);
}

int main(int argc, char **argv) {
    x("1");
}
