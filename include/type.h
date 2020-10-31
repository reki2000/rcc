typedef struct type_t type_t;

typedef struct member_t {
    char *name;
    int offset;
    struct type_t *t;
} member_t;

VEC_HEADER(member_t, member_vec)

typedef struct struct_t {
    char *name;
    bool is_union;
    bool is_anonymous;
    int next_offset;
    member_vec members;
} struct_t;

typedef struct enum_t {
    char *name;
    int next_value;
} enum_t;

typedef struct type_t {
    int size;
    int array_length;
    char *name;
    type_t *ptr_to;
    /*
     *  -1: not array
     *  0: array without size specification (ptr_to must be set)
     *  1: array of size >1 (ptr_to must be set)
     */
    struct_t *struct_of;
    enum_t *enum_of;
    type_t *typedef_of;
} type_t;

extern type_t *type_int;
extern type_t *type_void;
extern type_t *type_char;
extern type_t *type_long;
extern type_t *type_void_ptr;
extern type_t *type_char_ptr;

extern void init_types();
extern type_t *add_type(char *, int , type_t *, int );
extern type_t *find_type(char *);
extern void dump_type(char *buf, type_t *);
extern char *dump_type2(type_t *);

extern type_t *add_typedef(char *, type_t *t);
extern type_t *add_pointer_type(type_t *);
extern type_t *add_array_type(type_t *, int);
extern bool type_is_convertable(type_t *, type_t *);
extern bool type_is_same(type_t *, type_t *);
extern type_t *type_unalias(type_t *);

extern int type_size(type_t *t);

extern type_t *add_struct_type(char *, bool);
extern type_t *add_union_type(char *, bool);
extern member_t *add_struct_member(type_t *, char *, type_t *t, bool);
extern member_t *find_struct_member(type_t *, char *);
void copy_union_member_to_struct(type_t *st, type_t *ut);

extern type_t *add_enum_type(char *);

