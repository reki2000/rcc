struct type_t;

typedef struct member_t {
    char *name;
    int offset;
    struct type_t *t;
} member_t;

typedef struct struct_t {
    int num_members;
    bool is_union;
    member_t members[100];
} struct_t;

typedef struct type_t {
    int type_id;
    int size;
    char *name;
    struct type_t *ptr_to;
    struct_t *struct_of;
    int array_length;
} type_t;

extern void init_types();
extern type_t *add_type(char *name, int size, type_t *ptr_to, int array_length);
extern type_t *find_type(char *name);

extern type_t *add_pointer_type(type_t *);
extern type_t *add_array_type(type_t *, int);
extern bool is_convertable(type_t *, type_t *);

extern type_t *add_struct_type(char *name);
extern type_t *add_union_type(char *name);
extern member_t *add_struct_member(type_t *, char *name, type_t *t);
extern member_t *find_struct_member(type_t *, char *name);
