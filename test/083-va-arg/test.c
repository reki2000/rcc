typedef __builtin_va_list va_list;

#define va_start(va,n) __builtin_va_start(va,n)
#define va_end(va) __builtin_va_end(va)
#define va_arg(va,ret_type) __builtin_va_arg(va,ret_type)

extern void *malloc(long);

int add(int n, ...) {
    va_list va;
    va_start(va, n);
    int result = 0;
    for (int i=0; i<n; i++) {
        result += va_arg(va, int);
    }
    va_end(va);
    return result;
}

int main(int argc, char **argv) {
    return add(8,1,2,3,4,5,6,7,8);
}
