enum {
    TYPE_ARG = 0,
    TYPE_INT,
    TYPE_ADD,
    TYPE_MUL,
    TYPE_IDENT
};

typedef union value_t {
    char *str_value;
    int int_value;
    int atom_pos;
} value;

typedef struct atom_t {
    int type;
    value value;
} atom;

extern atom program[];

int alloc_atom(int size);

void dump_atom(int pos);
void dump_atom_all();

void build_int_atom(int pos, int type, int value);
void build_string_atom(int pos, int type, char * value);
void build_pos_atom(int pos, int type, int value);