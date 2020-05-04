int main() {
    struct { int a; } t;
    t.a = 0;

    if(!t.a) {
        return 10;
    }
    return 20;
}
