
extern int write(int, const char*, int);

int main() {
    char buf[256] = {'a', 'b'};
    char *d;

    d = buf + 1;
    *d = 'a';

    write(1,buf,2);
    return 0;
}
