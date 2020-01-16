typedef enum {
    T_RESERVE,
    T_INT,
    T_IDENT,
    T_LPAREN, T_RPAREN,
    T_LBLACE, T_RBLACE,
    T_EQ, T_NE, T_LE, T_LT, T_GE, T_GT,
    T_L_AND, T_L_OR, T_L_NOT,
    T_ADD, T_SUB, T_MUL, T_DIV, T_MOD,
    T_BIND,
    T_SEMICOLON,
    T_TYPE_INT,
    T_PRINTI
} token_id;

typedef union {
    int int_value;
    char *str_value;
} token_val;

typedef struct {
    token_id id;
    token_val value;
} token;

bool expect(token_id id);
bool expect_int(int *value);
bool expect_ident(char **value);

void tokenize();
int get_token_pos();
void set_token_pos(int pos);
bool is_eot();
void init();