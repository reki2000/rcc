typedef struct {
    char *filename;
    int id;
    char *body;
    int pos;
    int len;
    int line;
    int column;

    int prev_line;
    int prev_column;
    int prev_pos;
} src_t;

extern src_t *src;

bool enter_file(char *);
bool exit_file();
char *dump_file();
src_t *file_info(int id);
void dump_src();

int ch();
bool next();
bool is_eof();
