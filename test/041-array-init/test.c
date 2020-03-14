int main() {
    int a = 100;
    int b[3] = {3, a}; // should be compiled as `int b[2]; b[0] = 0; b[1] = a;`
    print(b[0]);
    return b[1];
}
