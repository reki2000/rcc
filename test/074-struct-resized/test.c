typedef struct t t;
struct t { int a; int b; };

int main(int argc, char **argv) {
    t a[10];
    t* p0;
    t* p1;
    int i=0;
    p0 = &a[i++];
    p1 = &a[i++];
    if (p1 == p0) {
        return 1;
    }
    return 0;
}
