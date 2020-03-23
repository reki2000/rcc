char a[10] = { 'a', 65 };

extern void write(int, char *, int);

int main() {
    write(1, a, 2);
    return a[3];
}
