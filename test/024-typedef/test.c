
typedef struct { 
    int id;
    char name[100];
} person_t;

int main() {
    person_t p;
    p.id = 12;
    return p.id;
}
