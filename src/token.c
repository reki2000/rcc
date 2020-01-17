#include <rsys.h>
#include <devtool.h>
#include <types.h>
#include <token.h>

token tokens[1024 * 128];
int token_pos = 0;
int token_len = 0;

char src[1024 * 1024];
int src_pos = 0;
int src_len = 0;

bool is_eof() {
    return src_pos >= src_len;
}

int ch() {
    if (is_eof()) {
        return -1;
    }
    return src[src_pos];
}

void next() {
    src_pos++;
}

void skip() {
    int c;
    for (;;) {
        c = ch();
        if (c != ' ' && c != '\t' && c != '\n') {
            break;
        }
        next();
    }
}

bool is_alpha(int ch) {
    return (ch >= 'a' && ch <= 'z')
        || (ch >= 'A' && ch <= 'Z');
}

bool is_digit(int ch) {
    return (ch >= '0' && ch <= '9');
}

bool expect_c(char c) {
    skip();
    if (ch() == c) {
        next();
        skip();
        return TRUE;
    }
    return FALSE;
}

bool expect_str(char *str) {
    skip();
    int old_pos = src_pos;
    for (;*str != 0; str++) {
        if (ch() != *str) {
            src_pos = old_pos;
            return FALSE;
        }
        next();
    }
    skip();
    return TRUE;
}

bool expect_reserved_str(char *str) {
    if (expect_str(str)) {
        int c = ch();
        if (!is_alpha(c) && !is_digit(c)) {
            return TRUE;
        }
    }
    return FALSE;
}

bool parse_int_token(int *retval) {
    int value = 0;
    int count = 0;

    for (;;) {
        int c = ch();
        if (c >= '0' && c <= '9') {
            value *= 10;
            value += ((char)c - '0');
        } else {
            break;
        }
        count++;
        next();
    }

    if (count == 0) {
        return FALSE;
    }

    *retval = value;
    return TRUE;
}

bool parse_ident_token(char **retval) {
    bool is_first = TRUE;
    char buf[100];
    int buf_pos = 0;
    char *str;
    int i;

    skip();
    for (;;) {
        int c;
        c = ch();
        if (!is_alpha(c) && c != '_' && (is_first || !is_digit(c))) {
            break;
        }
        buf[buf_pos++] = c;
        next();
        is_first = FALSE;
    }

    if (buf_pos == 0) {
        return FALSE;
    }

    str = _malloc(buf_pos + 1);
    for (i=0; i<buf_pos; i++) {
        str[i] = buf[i];
    }
    str[i] = 0;
    *retval = str;
    return TRUE;
}

void add_token(token_id id) {
    if (token_len >= 1024 * 128) {
        error("Too much tokens");
    }
    tokens[token_len].id = id;
    token_len++;
}

void add_int_token(int val) {
    add_token(T_INT);
    tokens[token_len - 1].value.int_value = val;
}

void add_ident_token(char *s) {
    add_token(T_IDENT);
    tokens[token_len - 1].value.str_value = s;
}

void dump_tokens() {
    int i;
    for (i=0; i<token_len; i++) {
        debug_i("token:", tokens[i].id);
    }
}

void tokenize() {
    for (;;) {
        if (ch() == -1) {
            break;
        }
        if (expect_str("!=")) {
            add_token(T_NE);
        } else if (expect_c('!')) {
            add_token(T_L_NOT);
        } else if (expect_str("==")) {
            add_token(T_EQ);
        } else if (expect_c('=')) {
            add_token(T_BIND);
        } else if (expect_str("<=")) {
            add_token(T_LE);
        } else if (expect_c('<')) {
            add_token(T_LT);
        } else if (expect_str(">=")) {
            add_token(T_GE);
        } else if (expect_c('>')) {
            add_token(T_GT);
        } else if (expect_c('*')) {
            add_token(T_MUL);
        } else if (expect_c('/')) {
            add_token(T_DIV);
        } else if (expect_c('%')) {
            add_token(T_MOD);
        } else if (expect_c('+')) {
            add_token(T_ADD);
        } else if (expect_c('-')) {
            add_token(T_SUB);
        } else if (expect_c('{')) {
            add_token(T_LBLACE);
        } else if (expect_c('}')) {
            add_token(T_RBLACE);
        } else if (expect_c('(')) {
            add_token(T_LPAREN);
        } else if (expect_c(')')) {
            add_token(T_RPAREN);
        } else if (expect_c(';')) {
            add_token(T_SEMICOLON);
        } else if (expect_reserved_str("break")) {
            add_token(T_BREAK);
        } else if (expect_reserved_str("continue")) {
            add_token(T_CONTINUE);
        } else if (expect_reserved_str("do")) {
            add_token(T_DO);
        } else if (expect_reserved_str("else")) {
            add_token(T_ELSE);
        } else if (expect_reserved_str("for")) {
            add_token(T_FOR);
        } else if (expect_reserved_str("if")) {
            add_token(T_IF);
        } else if (expect_reserved_str("int")) {
            add_token(T_TYPE_INT);
        } else if (expect_reserved_str("printi")) {
            add_token(T_PRINTI);
        } else if (expect_reserved_str("while")) {
            add_token(T_WHILE);
        } else {
            int i;
            char *str;
            if (parse_int_token(&i)) {
                add_int_token(i);
            } else if (parse_ident_token(&str)) {
                add_ident_token(str);
            } else {
                dump_tokens();
                error_i("invalid token @", src_pos);
            }
        }
    }
    dump_tokens();
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
        *value = tokens[token_pos].value.int_value;
        token_pos++;
        return TRUE;
    }
    return FALSE;
}

bool expect_ident(char **value) {
    if (tokens[token_pos].id == T_IDENT) {
        *value = tokens[token_pos].value.str_value;
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

void init() {
    src_len = _read(0, src, 1024);
}

