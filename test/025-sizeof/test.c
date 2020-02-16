
typedef struct { 
    int id;
    char name[100];
} person_t;

int main() {
    person_t p;
    print(sizeof p);
    print(sizeof (person_t));
    print(sizeof &p);
    print(sizeof p.name);
    print(sizeof (1+1));
    print(sizeof 1+1);
    print(sizeof (int));
    print(sizeof (char *));
    return 0;
}
