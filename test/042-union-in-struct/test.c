int main() {
    struct {
        int val0;
        union {
            int val1;
            int val2;
        };
        int val3;
    } x;

    x.val2 = 1024;
    x.val0 = 10;
    x.val3 = 20;

    print(x.val0);
    print(x.val1);
    print(x.val2);
    print(x.val3);
    return 0;
}
