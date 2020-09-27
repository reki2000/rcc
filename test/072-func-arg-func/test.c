int add(int a, int b) {
    return a + b;
}
int main(int argc, char **argv) {
    print(add(1,add(2,3)));
    return 0;
}
