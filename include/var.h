typedef struct {
    char *name;
    int offset;
    int size;
    type_s *t;
    bool is_ptr;
} var;

typedef struct {
    var *vars;
    int num_vars;
    int offset;
} frame;

extern frame env[];
extern int env_top;

extern void enter_var_frame();
extern void exit_var_frame();
extern frame *get_top_frame();
extern void add_var(char *, type_s *);
extern int find_var_offset(char *);
extern var *find_var(char *);

extern void reset_var_max_offset();
extern int var_max_offset();

