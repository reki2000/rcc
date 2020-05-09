int main() {
    char *fmt = "abcdefg\n";
    fmt += 2;
    write(1, fmt, 6);
}
