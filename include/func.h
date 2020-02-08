typedef struct {
    char *name;
    type_t *ret_type;
    int argc;
    var_t *argv;
    int body_pos;
    int max_offset;
} func;

extern func functions[];

func *find_func_name(char *name);
extern func *add_function(char *, type_t *, int, var_t *);
extern func *func_set_body(func *, int, int);
