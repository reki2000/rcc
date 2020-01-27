int main() {
    int i0;
    int i1;
    int *ip;
    i0 = 100;
    i1 = 200;
    ip = &i1;
    print(*ip);
    print(*(ip+1));
    return 0;
}
