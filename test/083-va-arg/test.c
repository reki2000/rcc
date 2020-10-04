typedef __builtin_va_list va_list;

#define va_start __builtin_va_start
#define va_end __builtin_va_end
#define va_arg __builtin_va_arg

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
