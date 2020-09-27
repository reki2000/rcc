int p(int a) {
    print(a);
    return a;
}
int main(int argc, char **argv) {
    int a=0;
    if (0 && p(3)) {
        return 1;
    };
    if (1 || p(4)) {
        return 2;
    };
    return 0;
}
