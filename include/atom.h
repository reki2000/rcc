enum {
    TYPE_ARG = 0,
    TYPE_INT,
    TYPE_ADD,
    TYPE_SUB,
    TYPE_MUL,
    TYPE_DIV,
    TYPE_MOD,
    TYPE_AND,
    TYPE_OR,
    TYPE_XOR,
    TYPE_VAR_VAL, TYPE_VAR_REF,
    TYPE_NOP,
    TYPE_EXPR_STATEMENT,
    TYPE_ANDTHEN,
    TYPE_GLOBAL_IDENT,
    TYPE_PRINT,
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
    TYPE_CONTINUE,
    TYPE_PTR,
    TYPE_PTR_DEREF,
    TYPE_FUNC,
    TYPE_RETURN,
    TYPE_APPLY,
    TYPE_POSTFIX_INC,
    TYPE_POSTFIX_DEC,
    TYPE_STRING,
    TYPE_OFFSET,
    TYPE_GLOBAL_VAR_VAL,
    TYPE_GLOBAL_VAR_REF,
    TYPE_GLOBAL_INT,
};

extern char* atom_name[];

typedef struct {
    int type;
    type_t *t;
    union {
        char *ptr_value;
        int int_value;
        char char_value;
        long long_value;
        int atom_pos;
    };
} atom_t;

extern atom_t program[];

int alloc_atom(int size);

void dump_atom(int pos, int);
void dump_atom2(atom_t *a, int, int);
void dump_atom_all();
void dump_atom_tree(int, int);

void build_pos_atom(int pos, int type, int value);

int atom_to_lvalue(int);
bool atom_same_type(int, int);

int alloc_binop_atom(int type, int lpos, int rpos);

int alloc_typed_pos_atom(int , int, type_t *);
int alloc_typed_int_atom(int, int, type_t *);

int alloc_var_atom(var_t *);
int alloc_deref_atom(int);
int alloc_postincdec_atom(int, int);
int alloc_assign_op_atom(int, int, int);
int alloc_ptr_atom(int);
int alloc_func_atom(func *);
int alloc_offset_atom(int, type_t *, int);
int alloc_index_atom(int, int);

