enum {
    TYPE_ARG = 0,
    TYPE_INT,
    TYPE_ADD,
    TYPE_SUB,
    TYPE_MUL,
    TYPE_DIV,
    TYPE_MOD,
    TYPE_VAR_REF,
    TYPE_NOP,
    TYPE_EXPR_STATEMENT,
    TYPE_ANDTHEN,
    TYPE_GLOBAL_IDENT,
    TYPE_PRINTI,
    TYPE_BIND,
    TYPE_EQ_EQ,
    TYPE_EQ_NE,
    TYPE_EQ_LT,
    TYPE_EQ_GT,
    TYPE_EQ_LE,
    TYPE_EQ_GE,
    TYPE_LOG_AND,
    TYPE_LOG_OR,
    TYPE_LOG_NOT,
    TYPE_IF,
    TYPE_FOR,
    TYPE_WHILE,
    TYPE_DO_WHILE,
    TYPE_BREAK,
    TYPE_CONTINUE
};

extern char* atom_name[];

typedef struct atom_t {
    int type;
    union {
        char *str_value;
        int int_value;
        int atom_pos;
    } value;
} atom;

extern atom program[];

int alloc_atom(int size);

void dump_atom(int pos);
void dump_atom_all();

void build_int_atom(int pos, int type, int value);
void build_string_atom(int pos, int type, char * value);
void build_pos_atom(int pos, int type, int value);
void build_ptr_atom(int pos, int type, void *value);

int alloc_int_atom(int type, int value);
int alloc_pos_atom(int type, int value);
int alloc_binop_atom(int type, int lpos, int rpos);
