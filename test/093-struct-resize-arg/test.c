typedef enum { FALSE, TRUE } bool;
typedef struct type_t type_t;

typedef struct member_t {
    char *name;
    int offset;
    struct type_t *t;
} member_t;

typedef struct struct_t {
    char *name;
    bool is_union;
    bool is_anonymous;
    int next_offset;
    int num_members;
    member_t members[100];
} struct_t;

typedef struct enum_t {
    char *name;
    int next_value;
} enum_t;

typedef struct type_t {
    int size;
    char *name;
    type_t *ptr_to;
    int array_length;
    /*
     *  -1: not array
     *  0: array without size specification (ptr_to must be set)
     *  1: array of size >1 (ptr_to must be set)
     */
    struct_t *struct_of;
    enum_t *enum_of;
    type_t *typedef_of;
} type_t;


void show(type_t a) {
  type_t b;
  b = a;
  print(b.size);
  print(b.array_length);
}
int main() {
    type_t p;
    p.name = "test";
    p.size = 10;
    p.ptr_to = (void *)0;
    p.array_length = -1;
    p.enum_of = (void *)0;
    p.struct_of = (void *)0;
    p.typedef_of = (void *)0;
  show(p);
  return 0;
}
