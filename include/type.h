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


extern void init_types();
extern type_t *add_type(char *, int , type_t *, int );
extern type_t *find_type(char *);
extern void dump_type(char *buf, type_t *);

extern type_t *add_pointer_type(type_t *);
extern type_t *add_array_type(type_t *, int);
extern bool type_is_convertable(type_t *, type_t *);
extern bool type_is_same(type_t *, type_t *);
extern type_t *type_unalias(type_t *);

extern type_t *add_struct_type(char *, bool);
extern type_t *add_union_type(char *, bool);
extern member_t *add_struct_member(type_t *, char *, type_t *t, bool);
extern member_t *find_struct_member(type_t *, char *);
void copy_union_member_to_struct(type_t *st, type_t *ut);

extern type_t *add_enum_type(char *);

