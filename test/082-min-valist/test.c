typedef __builtin_va_list va_list;

#define va_start __builtin_va_start
#define va_end __builtin_va_end
#define va_args __builtin_va_arg

extern int vsprintf(char *, const char *fmt, va_list *v);
extern int puts(const char *);
extern int strlen(const char *);

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
