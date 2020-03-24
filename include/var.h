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

typedef struct {
    var_t *vars;
    int num_vars;
    int offset;
} frame_t;

extern frame_t env[];
extern int env_top;

extern void enter_var_frame();
extern void exit_var_frame();
extern frame_t *get_top_frame();
extern var_t *add_var(char *, type_t *);
extern var_t *add_constant_int(char *, type_t *, int value);
extern int find_var_offset(char *);
extern var_t *find_var(char *);
extern var_t *find_var_in_current_frame(char *name);
extern void var_realloc(var_t *v, type_t *t);


extern void reset_var_max_offset();
extern int var_max_offset();

