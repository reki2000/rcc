typedef struct {
    char *name;
    src_t *src;
    int start_pos;
    int end_pos;
    char_p_vec *vars;
} macro_t;

extern macro_t macros[];

void add_macro(const char *name, int start_pos, int end_pos, char_p_vec *vars);
macro_t *find_macro(const char *name);

bool enter_macro(const char *name);
bool exit_macro();

bool enter_macro_arg(const char *name);
bool exit_macro_arg();
