typedef struct {
    char *name;
    type_t *ret_type;
    int argc;
    var_vec argv;
    int body_pos;
    int max_offset;
    bool is_external;
    bool is_variadic;
} func;

extern func functions[];

func *find_func_name(char *name);
extern func *add_function(char *, type_t *, bool, bool, int, var_vec);
extern func *func_set_body(func *, int, var_vec, int, int);
