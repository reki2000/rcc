struct type_t;

typedef struct member_t {
    char *name;
    int offset;
    struct type_t *t;
} member_s;

typedef struct struct_t {
    int num_members;
    member_s members[100];
} struct_s;

typedef struct type_t {
    int type_id;
    int size;
    char *name;
    struct type_t *ptr_to;
    struct_s *struct_of;
    struct type_t *alias_of;
} type_s;

extern void init_types();
extern type_s *add_type(char *name, int size, type_s *);
extern type_s *find_type(char *name);

extern type_s *add_pointer_type(type_s *);

extern type_s *add_struct_type(char *name);
extern type_s *find_struct_type(char *name);
extern member_s *add_struct_member(type_s *, char *name, type_s *t, int);
extern member_s *find_struct_member(type_s *, char *name);
