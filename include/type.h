typedef struct type_t {
    int type_id;
    int size;
    char *name;
    struct type_t *ptr_to;
} type_s;

extern void init_types();
extern type_s *add_type(char *name, int size, type_s *);
extern type_s *find_type(char *name);
extern type_s *add_pointer_type(type_s *);
