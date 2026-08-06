typedef struct { int a; int b; int c; int d; int e; } big;

void show(big p, big q) {
    print(p.a);
    print(p.e);
    print(q.a);
    print(q.e);
}

int main() {
    big x;
    big y;
    x.a = 1;
    x.e = 5;
    y.a = 11;
    y.e = 15;
    show(x, y);
    return 0;
}
