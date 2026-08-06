int id(int n) {
    return n;
}

int main() {
    int x;
    int b;
    b = 24;

    // the result register of / and % may be %rdx, which the emitter also
    // saves around idiv - the saved value must not overwrite the result
    x = 0;
    x += b / 8;
    print(x);            // 3

    x = 0;
    x += id(24) / 8;
    print(x);            // 3

    x = 100;
    x -= id(24) / 8;
    print(x);            // 97

    x = 0;
    x += id(24) % 7;
    print(x);            // 3

    x = 1;
    x += b * 2;
    print(x);            // 49

    int a[4];
    a[0] = 7;
    a[1] = 9;
    x = 0;
    x += a[1];
    print(x);            // 9

    return 0;
}
