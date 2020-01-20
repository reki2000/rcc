typedef struct {
    char *name;
    type_s *ret_type;
    int argc;
    var **argv;
    int body_pos;
    int max_offset;
} func;

extern func functions[];

extern func *add_function(char *, type_s *, int, var **);
extern func *func_set_body(func *, int, int);
