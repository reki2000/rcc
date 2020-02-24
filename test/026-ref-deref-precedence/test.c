typedef struct { 
    int id;
    char buf[500];
} person_t;

int main() {
    person_t p;
    int *pp;
    char *c;

    p.id = 0;
    pp = &p.id;
    *pp = 5;
    print(p.id);

    p.buf[3] = 'A';
    c = &p.buf[3];
    *c = 'B';
    print(p.buf[3]);

    return 0;
}
