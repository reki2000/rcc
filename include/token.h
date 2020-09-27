typedef enum {
    T_RESERVE,
    T_EOF,
    T_INT,
    T_CHAR,
    T_STRING,
    T_IDENT,
    T_LPAREN, T_RPAREN,
    T_LBLACE, T_RBLACE,
    T_LBRACKET, T_RBRACKET,
    T_EQ, T_NE, T_LE, T_LT, T_GE, T_GT,
    T_L_AND, T_L_OR, T_L_NOT,
    T_PLUS, T_MINUS, T_ASTERISK, T_SLASH, T_PERCENT,
    T_EQUAL,
    T_INC, T_DEC,
    T_SIZEOF,
    T_SEMICOLON,
    T_PRINT,
    T_IF, T_ELSE,
    T_FOR, T_WHILE, T_DO, T_BREAK, T_CONTINUE,
    T_RETURN,
    T_AMP, T_ALLOW,
    T_COMMA, T_PERIOD, 
    T_STRUCT, T_UNION, T_ENUM, T_TYPEDEF,
    T_PLUS_EQUAL, T_MINUS_EQUAL, T_ASTERISK_EQUAL, T_SLASH_EQUAL, T_PERCENT_EQUAL,
    T_PIPE_EQUAL, T_AMP_EQUAL, T_HAT_EQUAL,
    T_LSHIFT, T_LSHIFT_EQUAL, T_RSHIFT, T_RSHIFT_EQUAL, T_TILDE, T_TILDE_EQUAL,
    T_PIPE, T_HAT,
    T_SWITCH, T_CASE, T_COLON, T_DEFAULT,
    T_QUESTION,
    T_EXTERN, T_CONST
} token_id;

typedef struct {
    token_id id;
    union {
        int int_value;
        char *str_value;
        char char_value;
        long long_value;
    };
    int src_id;
    int src_line;
    int src_column;
    int src_pos;
    int src_end_pos;
} token;

extern bool expect(token_id id);
extern bool expect_int(int *value);
extern bool expect_char(char *value);
extern bool expect_ident(char **value);
extern bool expect_string(char **value);

extern void tokenize_file(char *);
extern int get_token_pos();
extern void set_token_pos(int pos);
extern bool is_eot();

extern void dump_tokens();
extern void dump_token_simple(char *buf, int pos);
