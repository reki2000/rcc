int main() {
    char a = 1;

    print(2|1);
    print(3&1);
    print(7^1);
    print(~a);
    print(1<<10);
    print(256>>4);
    print(-2>>10);

    a <<= 5;
    print(a);
    print(a>>=3);

    return 0;
}
