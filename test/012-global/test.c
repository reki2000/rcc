int a;
int b;
int *c;

int sub() {
    a = 0;
    b = 10;
    c = &b;
}

int main() {
    int a;
    a = 30;
    sub();
    return a + *c;
}
