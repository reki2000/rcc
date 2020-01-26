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
    T_COMMA,
    T_INC, T_DEC,
    T_STRING
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

extern bool expect(token_id id);
extern bool expect_int(int *value);
extern bool expect_ident(char **value);
extern bool expect_string(char **value);

extern void tokenize();
extern int get_token_pos();
extern void set_token_pos(int pos);
extern bool is_eot();
extern void init();

extern void dump_tokens();
