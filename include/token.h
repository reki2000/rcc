typedef enum {
    T_RESERVE,
    T_EOF,
    T_INT,
    T_IDENT,
    T_LPAREN, T_RPAREN,
    T_LBLACE, T_RBLACE,
    T_EQ, T_NE, T_LE, T_LT, T_GE, T_GT,
    T_L_AND, T_L_OR, T_L_NOT,
    T_PLUS, T_MINUS, T_ASTERISK, T_SLASH, T_PERCENT,
    T_EQUAL,
    T_SEMICOLON,
    T_PRINT,
    T_IF, T_ELSE,
    T_FOR, T_WHILE, T_DO, T_BREAK, T_CONTINUE,
    T_AMP,
    T_RETURN,
    T_COMMA
} token_id;

typedef struct {
    token_id id;
    union {
        int int_value;
        char *str_value;
    } value;
    int src_line;
    int src_column;
    int src_pos;
} token;

bool expect(token_id id);
bool expect_int(int *value);
bool expect_ident(char **value);

void tokenize();
int get_token_pos();
void set_token_pos(int pos);
bool is_eot();
void init();

void dump_tokens();
