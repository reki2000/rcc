char *a[3] = { "ABC", "DEF" };

extern void write(int, char *, int);

int main() {
    write(1, a[1], 3);
    return 0;
}
