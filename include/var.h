typedef struct {
    char *name;
    int offset;
    type_t *t;
    bool is_global;
    bool is_external;
    bool is_constant;
    bool has_value;
    union {
        int int_value;
        long long_value;
        char *data;
    };
} var_t;

VEC_HEADER(var_t, var_vec)

typedef struct {
    var_vec vars;
    int offset;
    bool is_function_args;
    int num_reg_vars;
    int num_stack_vars;
} frame_t;

extern void enter_var_frame();
extern void enter_function_args_var_frame();
extern void exit_var_frame();

extern frame_t *get_top_frame();
extern frame_t *get_global_frame();
extern var_t *add_var(char *, type_t *);
extern var_t *add_constant_int(char *, type_t *, int value);
extern void add_register_save_area();
extern var_t *find_var(char *);
extern var_t *find_var_in_current_frame(char *name);
extern void var_realloc(var_t *v, type_t *t);

extern void reset_var_max_offset();
extern int var_max_offset();

