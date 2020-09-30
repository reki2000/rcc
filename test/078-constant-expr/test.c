enum { FALSE, TRUE };

int a = (1 << 2 * 3 ^ 5) % 10 | 4 & ~~15; // 13
int b = !(TRUE && FALSE || TRUE); // FALSE 0

int main(int argc, char **argv) {
    print(a);
    print(b);
    return 0;
}
