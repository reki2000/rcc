typedef struct var_t {
    char *name;
    int offset;
    int size;
} var;

typedef struct frame_t {
    var *vars;
    int num_vars;
    int offset;
} frame;

extern frame env[];
extern int env_top;

extern void enter_var_frame();
extern void exit_var_frame();
extern void add_var(char *name);
extern int find_var_offset(char *name);
