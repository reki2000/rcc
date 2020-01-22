int fib(int n) {
    if (n == 1) {
        return 1;
    } else if (n == 2) {
        return 1;
    }
    return fib(n - 1) + fib(n - 2);
}

int main() {
    int i;
    for (i=1; i<10; i=i+1) {
        print(fib(i));
    }
    return 5;
}
