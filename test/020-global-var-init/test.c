int x = 100;
int y = 10;
int z[100];

int sub(int a) {
    return a+y;
}

int main() {
    int a = 20;
    char *b = " hogehoge";
    int c = sub(a);
    return c;
}
