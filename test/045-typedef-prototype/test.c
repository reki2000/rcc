typedef struct p struct_t;

struct p { 
    int id;
    char name[100];
};

int main() {
    struct_t p;
    p.id = 12;
    return p.id;
}
