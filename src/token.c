#include "rsys.h"
#include "rstring.h"
#include "devtool.h"
#include "types.h"
#include "file.h"
#include "macro.h"
#include "token.h"
#include "gstr.h"

#define NUM_TOKENS  (1024*128)

token tokens[NUM_TOKENS];
int token_pos = 0;
int token_len = 0;

void set_src_pos() {
    src->prev_column = src->column;
    src->prev_pos = src->pos;
    src->prev_line = src->line;
}

void skip() {
    int c = ch();
    while (c == ' ' || c == '\t' || c == '\n') {
        next();
        c = ch();
    }
    set_src_pos();
}

bool is_alpha(int ch) {
    return (ch >= 'a' && ch <= 'z')
        || (ch >= 'A' && ch <= 'Z');
}

bool is_digit(int ch) {
    return (ch >= '0' && ch <= '9');
}

bool accept_char(char c) {
    skip();
    if (ch() == c) {
        next();
        return TRUE;
    }
    return FALSE;
}

bool accept_string(char *str) {
    skip();
    int old_pos = src->pos;
    for (;*str != 0; str++) {
        if (ch() != *str) {
            src->pos = old_pos;
            return FALSE;
        }
        next();
    }
    return TRUE;
}

bool accept_ident(char *str) {
    skip();
    int old_pos = src->pos;
    for (;*str != 0; str++) {
        if (ch() != *str) {
            src->pos = old_pos;
            return FALSE;
        }
        next();
    }
    int c = ch();
    if (!is_alpha(c) && !is_digit(c) && c != '_') {
        return TRUE;
    }
    src->pos = old_pos;
    return FALSE;
}

char escape(char escaped_char) {
    char c;
    switch (escaped_char) {
        case 'n': c = '\n'; break;
        case '0': c = '\0'; break;
        case 't': c = '\t'; break;
        case 'r': c = '\r'; break;
        case 'a': c = '\a'; break;
        case 'b': c = '\b'; break;
        case 'e': c = '\e'; break;
        case 'f': c = '\f'; break;
        case '"': c = '"'; break;
        case '\'': c = '\''; break;
        case '\\': c = '\\'; break;
        default: error("Invalid letter after escape");
    }
    return c;
}

bool decode_digit(char c, int *value, char min, char max, int base, int radix) {
    if (c >= min && c <= max) {
        *value *= radix;
        *value += ((char)c - min + base);
        return TRUE;
    }
    return FALSE;
}

bool tokenize_int_hex(int *retval) {
    if (ch() != '0') {
        return FALSE;
    }
    next();
    if (ch() != 'x' && ch() != 'X') {
        src->pos--;
        return FALSE;
    }
    next();

    int count = 0;
    *retval = 0;
    for (;;) {
        int c = ch();
        if (!decode_digit(c, retval, '0', '9', 0, 16)
            && !decode_digit(c, retval, 'A', 'F', 10, 16)
            && !decode_digit(c, retval, 'a', 'f', 10, 16)) {
            break;
        }
        count++;
        next();
    }

    return (count != 0);
}

bool tokenize_int_oct(int *retval) {
    if (ch() != '0') {
        return FALSE;
    }
    next();

    *retval = 0;
    while (decode_digit(ch(), retval, '0', '7', 0, 8)) {
        next();
    }
    return TRUE;
}

bool tokenize_int_decimal(int *retval) {
    int count = 0;
    *retval = 0;
    while (decode_digit(ch(), retval, '0', '9', 0, 10)) {
        count++;
        next();
    }
    return (count != 0);
}

bool tokenize_int(int *retval) {
    skip();
    return tokenize_int_hex(retval)
      || tokenize_int_oct(retval)
      || tokenize_int_decimal(retval);
}

bool tokenize_char(char *retval) {
    skip();
    if (ch() != '\'') {
        return FALSE;
    }
    next();
    char c = ch();
    next();
    if (c == '\\') {
        c = escape(ch());
        next();
    }
    if (ch() != '\'') {
        error_i("invalid char literal", ch());
    }
    next();
    *retval = c;
    return TRUE;
}

bool tokenize_string(char **retval) {
    char buf[RCC_BUF_SIZE];
    int buf_pos = 0;
    char *str;
    int i;

    skip();
    if (ch() != '"') {
        return FALSE;
    }
    next();
    for (;;) {
        if (buf_pos >= RCC_BUF_SIZE) {
            error("String too long");
        }
        int c = ch();
        if (c == '"') {
            next();
            break;
        }
        if (c == '\\') {
            next();
            c = escape(ch());
        }
        buf[buf_pos++] = c;
        next();
    }

    str = malloc(buf_pos + 1);
    for (i=0; i<buf_pos; i++) {
        str[i] = buf[i];
    }
    str[i] = 0;
    *retval = str;
    return TRUE;
}

bool tokenize_ident(char **retval) {
    bool is_first = TRUE;
    char buf[RCC_BUF_SIZE];
    int buf_pos = 0;

    skip();
    for (;;) {
        int c;
        c = ch();
        if (is_alpha(c) || c == '_' || (!is_first && (is_digit(c) || c == '_'))) {
            buf[buf_pos++] = c;
            next();
            is_first = FALSE;
        } else {
            break;
        }
    }
    buf[buf_pos] = 0;

    if (buf_pos == 0) {
        return FALSE;
    }

    *retval = strdup(buf);
    return TRUE;
}

token *add_token(token_id id) {
    if (token_len >= NUM_TOKENS) {
        error("Too much tokens");
    }

    token *t=&tokens[token_len++];
    t->id = id;
    t->src_id = src->id;
    t->src_line = src->prev_line;
    t->src_column = src->prev_column;
    t->src_pos = src->prev_pos;
    t->src_end_pos = src->pos - 1;

    set_src_pos();
    return t;
}

void add_int_token(int val) {
    token *t = add_token(T_INT);
    t->int_value = val;
}

void add_string_token(char *val) {
    token *t = add_token(T_STRING);
    t->str_value = val;
}

void add_char_token(char val) {
    token *t = add_token(T_CHAR);
    t->char_value = val;
}

void add_ident_token(char *s) {
    token *t = add_token(T_IDENT);
    t->str_value = s;
}

void dump_token(int pos, token *t) {
    src_t *s = file_info(t->src_id);

    char buf[RCC_BUF_SIZE] = {0};
    _strcat3(buf, "#:", pos, " ");
    _strcat3(buf, "id:", t->id, " ");
    _strcat3(buf, "src_id:", t->src_id, " ");
    strcat(buf, s->filename);
    _strcat3(buf, ":", t->src_line, "");
    _strcat3(buf, ":", t->src_column, "\n");
    strcat(buf, dump_file2(t->src_id, t->src_pos, t->src_end_pos));
    debug_s("token:", buf);
}

void dump_token_simple(char *buf, int pos) {
    token *t = &tokens[pos];
    src_t *s = file_info(t->src_id);

    strcat(buf, s->filename);
    _strcat3(buf, ":", t->src_line, "");
    _strcat3(buf, ":", t->src_column, " ");
    strcat(buf, dump_file3(t->src_id, t->src_pos, t->src_end_pos));
}

void dump_tokens() {
    for (int i = token_pos - 2; i >= 0 && i <= token_pos; i++) {
        if (i<0 || i>=token_len) {
            continue;
        }
        dump_token(i, &tokens[i]);
    }
}

void tokenize();

void directive_include() {
    if (accept_char('\"')) {
        char filename[RCC_BUF_SIZE];
        int i=0;
        while (ch() != '\"') {
            if (i>=100) {
                error("too long file name");
            }
            filename[i++] = ch();
            next();
        }
        filename[i] = '\0';
        next();

        enter_file(filename);
        tokenize();
        exit_file();
    } else {
        error("no file name for #include");
    }
}

void directive_define() {
    char *name;
    if (tokenize_ident(&name)) {
        skip();
        int start_pos = src->pos;
        char prev_ch = 0;
        for (;;) {
            if (ch() == '\n' && prev_ch != '\\') {
                    break;
            }
            prev_ch = ch();
            next();
        }
        int end_pos = src->pos;
        add_macro(name, start_pos, end_pos);
    } else {
        error("no identifier for define directive");
    }
}

void preprocess() {
    if (accept_ident("include")) {
        directive_include();
    } else if (accept_ident("define")) {
        directive_define();
    } else {
        error("unknown directive");
    }
    set_src_pos();
}

void tokenize() {
    while (!is_eof()) {
        if (accept_string("\\\n")) {
        } else if (accept_char('#')) {
            preprocess();
        } else if (accept_string("!=")) {
            add_token(T_NE);
        } else if (accept_char('!')) {
            add_token(T_L_NOT);
        } else if (accept_char('?')) {
            add_token(T_QUESTION);
        } else if (accept_string("==")) {
            add_token(T_EQ);
        } else if (accept_string("&&")) {
            add_token(T_L_AND);
        } else if (accept_string("&=")) {
            add_token(T_AMP_EQUAL);
        } else if (accept_char('&')) {
            add_token(T_AMP);
        } else if (accept_string("||")) {
            add_token(T_L_OR);
        } else if (accept_string("|=")) {
            add_token(T_PIPE_EQUAL);
        } else if (accept_char('|')) {
            add_token(T_PIPE);
        } else if (accept_string("^=")) {
            add_token(T_HAT_EQUAL);
        } else if (accept_char('^')) {
            add_token(T_HAT);
        } else if (accept_string("~=")) {
            add_token(T_TILDE_EQUAL);
        } else if (accept_char('~')) {
            add_token(T_TILDE);
        } else if (accept_char('=')) {
            add_token(T_EQUAL);
        } else if (accept_string("<=")) {
            add_token(T_LE);
        } else if (accept_string("<<=")) {
            add_token(T_LSHIFT_EQUAL);
        } else if (accept_string("<<")) {
            add_token(T_LSHIFT);
        } else if (accept_char('<')) {
            add_token(T_LT);
        } else if (accept_string(">=")) {
            add_token(T_GE);
        } else if (accept_string(">>=")) {
            add_token(T_RSHIFT_EQUAL);
        } else if (accept_string(">>")) {
            add_token(T_RSHIFT);
        } else if (accept_char('>')) {
            add_token(T_GT);
        } else if (accept_string("*=")) {
            add_token(T_ASTERISK_EQUAL);
        } else if (accept_char('*')) {
            add_token(T_ASTERISK);
        } else if (accept_string("//")) {
            debug("scanning //...");
            while (ch() != '\n') {
                if (!next()) {
                    break;
                }
            }
            set_src_pos();
        } else if (accept_string("/*")) {
            debug("scanning /*...");
            while (!accept_string("*/")) {
                if (!next()) {
                    error("invalid eof in comment block");
                }
            }
            set_src_pos();
        } else if (accept_string("/=")) {
            add_token(T_SLASH_EQUAL);
        } else if (accept_char('/')) {
            add_token(T_SLASH);
        } else if (accept_string("%=")) {
            add_token(T_PERCENT_EQUAL);
        } else if (accept_char('%')) {
            add_token(T_PERCENT);
        } else if (accept_string("++")) {
            add_token(T_INC);
        } else if (accept_string("+=")) {
            add_token(T_PLUS_EQUAL);
        } else if (accept_char('+')) {
            add_token(T_PLUS);
        } else if (accept_string("--")) {
            add_token(T_DEC);
        } else if (accept_string("-=")) {
            add_token(T_MINUS_EQUAL);
        } else if (accept_string("->")) {
            add_token(T_ALLOW);
        } else if (accept_char('-')) {
            add_token(T_MINUS);
        } else if (accept_char('{')) {
            add_token(T_LBLACE);
        } else if (accept_char('}')) {
            add_token(T_RBLACE);
        } else if (accept_char('[')) {
            add_token(T_LBRACKET);
        } else if (accept_char(']')) {
            add_token(T_RBRACKET);
        } else if (accept_char('(')) {
            add_token(T_LPAREN);
        } else if (accept_char(')')) {
            add_token(T_RPAREN);
        } else if (accept_char(':')) {
            add_token(T_COLON);
        } else if (accept_char(';')) {
            add_token(T_SEMICOLON);
        } else if (accept_string("...")) {
            add_token(T_3DOT);
        } else if (accept_char('.')) {
            add_token(T_PERIOD);
        } else if (accept_char(',')) {
            add_token(T_COMMA);
        } else if (accept_ident("break")) {
            add_token(T_BREAK);
        } else if (accept_ident("case")) {
            add_token(T_CASE);
        } else if (accept_ident("const")) {
            add_token(T_CONST);
        } else if (accept_ident("continue")) {
            add_token(T_CONTINUE);
        } else if (accept_ident("default")) {
            add_token(T_DEFAULT);
        } else if (accept_ident("do")) {
            add_token(T_DO);
        } else if (accept_ident("else")) {
            add_token(T_ELSE);
        } else if (accept_ident("extern")) {
            add_token(T_EXTERN);
        } else if (accept_ident("for")) {
            add_token(T_FOR);
        } else if (accept_ident("if")) {
            add_token(T_IF);
        } else if (accept_ident("print")) {
            add_token(T_PRINT);
        } else if (accept_ident("return")) {
            add_token(T_RETURN);
        } else if (accept_ident("sizeof")) {
            add_token(T_SIZEOF);
        } else if (accept_ident("struct")) {
            add_token(T_STRUCT);
        } else if (accept_ident("switch")) {
            add_token(T_SWITCH);
        } else if (accept_ident("typedef")) {
            add_token(T_TYPEDEF);
        } else if (accept_ident("union")) {
            add_token(T_UNION);
        } else if (accept_ident("enum")) {
            add_token(T_ENUM);
        } else if (accept_ident("while")) {
            add_token(T_WHILE);
        } else {
            int i;
            char c;
            char *str;
            if (tokenize_int(&i)) {
                add_int_token(i);
            } else if (tokenize_char(&c)) {
                add_char_token(c);
            } else if (tokenize_string(&str)) {
                add_string_token(str);
            } else if (tokenize_ident(&str)) {
                if (enter_macro(str)) {
                    tokenize();
                    exit_macro();
                } else {
                    add_ident_token(str);
                }
            } else {
                char buf[RCC_BUF_SIZE] = {0};
                strcat(buf, "Invalid token: ");
                strcat(buf, src->filename);
                _strcat3(buf, ":", src->line, "");
                _strcat3(buf, ":", src->column, " ");
                _strcat3(buf, "[", ch(), "]\n");
                strcat(buf, _slice(&(src->body[(src->pos > 20) ? src->pos - 20 : 0]), 20));
                strcat(buf, " --> ");
                strcat(buf, _slice(&(src->body[src->pos]), 20));
                error(buf);
            }
        }
        skip();
    }
}

void tokenize_file(char *filename) {
    enter_file(filename);
    tokenize();
    add_token(T_EOF);
    exit_file();
    debug_i("tokens:", token_len);
}

bool expect(token_id id) {
    if (tokens[token_pos].id == id) {
        token_pos++;
        return TRUE;
    }
    return FALSE;
}

bool expect_int(int *value) {
    if (tokens[token_pos].id == T_INT) {
        *value = tokens[token_pos].int_value;
        token_pos++;
        return TRUE;
    }
    return FALSE;
}

bool expect_ident(char **value) {
    if (tokens[token_pos].id == T_IDENT) {
        *value = tokens[token_pos].str_value;
        token_pos++;
        return TRUE;
    }
    return FALSE;
}

bool expect_string(char **value) {
    if (tokens[token_pos].id == T_STRING) {
        *value = tokens[token_pos].str_value;
        token_pos++;
        return TRUE;
    }
    return FALSE;
}

bool expect_char(char *value) {
    if (tokens[token_pos].id == T_CHAR) {
        *value = tokens[token_pos].char_value;
        token_pos++;
        return TRUE;
    }
    return FALSE;
}

int get_token_pos() {
    return token_pos;
}

void set_token_pos(int pos) {
    token_pos = pos;
}

bool is_eot() {
    return (token_pos >= token_len);
}
