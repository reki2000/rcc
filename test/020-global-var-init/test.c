int x = 100;
int y = 10;
char *s = "abc\n";
int z[100];

int sub(int a) {
    return a+y;
}

extern int write(int, char *, int);

int main() {
    int a = 20;
    char *b = " hogehoge";
    int c = sub(a);
    write(1, s, 4);
    return c;
}
