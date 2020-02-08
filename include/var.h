typedef struct {
    char *name;
    int offset;
    type_t *t;
    bool is_global;
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
extern int find_var_offset(char *);
extern var_t *find_var(char *);

extern void reset_var_max_offset();
extern int var_max_offset();

