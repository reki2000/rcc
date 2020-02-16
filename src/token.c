#include <rsys.h>
#include <rstring.h>
#include <devtool.h>
#include <types.h>
#include <token.h>
#include <gstr.h>

token tokens[1024 * 128];
int token_pos = 0;
int token_len = 0;

char src[1024 * 1024];
int src_pos = 0;
int src_len = 0;
int src_line = 1;
int src_column = 1;

int prev_src_line = 1;
int prev_src_column = 1;
int prev_src_pos = 0;

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
    if (ch() == '\n') {
        src_column = 1;
        src_line++;
    }
    src_pos++;
    src_column++;
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
    skip();
    int old_pos = src_pos;
    for (;*str != 0; str++) {
        if (ch() != *str) {
            src_pos = old_pos;
            return FALSE;
        }
        next();
    }
    int c = ch();
    if (!is_alpha(c) && !is_digit(c)) {
        skip();
        return TRUE;
    }
    src_pos = old_pos;
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

char escape(char escaped_char) {
    char c;
    switch (escaped_char) {
        case 'n': c = '\n'; break;
        case '0': c = '\0'; break;
        case 't': c = '\t'; break;
        case 'r': c = '\r'; break;
        case 'a': c = '\a'; break;
        case 'b': c = '\b'; break;
        case 'f': c = '\f'; break;
        case '"': c = '"'; break;
        case '\'': c = '\''; break;
        case '\\': c = '\\'; break;
        default: error("Invalid letter after escape");
    }
    return c;
}

bool parse_char_token(char *retval) {
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

bool parse_string_token(char **retval) {
    char buf[1024];
    int buf_pos = 0;
    char *str;
    int i;

    skip();
    if (ch() != '"') {
        return FALSE;
    }
    next();
    for (;;) {
        if (buf_pos >= 1024) {
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

    str = _malloc(buf_pos + 1);
    for (i=0; i<buf_pos; i++) {
        str[i] = buf[i];
    }
    str[i] = 0;
    *retval = str;
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
    tokens[token_len].src_line = prev_src_line;
    tokens[token_len].src_column = prev_src_column;
    tokens[token_len].src_pos = prev_src_pos;
    prev_src_column = src_column;
    prev_src_pos = src_pos;
    prev_src_line = src_line;

    token_len++;
}

void add_int_token(int val) {
    add_token(T_INT);
    tokens[token_len - 1].int_value = val;
}

void add_string_token(char *val) {
    add_token(T_STRING);
    tokens[token_len - 1].str_value = val;
}

void add_char_token(char val) {
    add_token(T_CHAR);
    tokens[token_len - 1].char_value = val;
}

void add_ident_token(char *s) {
    add_token(T_IDENT);
    tokens[token_len - 1].str_value = s;
}

void dump_tokens() {
    int i;
    for (i=0; i<token_len; i++) {
        char buf[100];
        buf[0] = 0;
        strcat(buf, (i == token_pos - 1) ? "*" : " ");
        _strcat3(buf, "id:", tokens[i].id, "");
        _strcat3(buf, "(col:", tokens[i].src_column, ",");
        _strcat3(buf, "lin:", tokens[i].src_line, ") ");
        strcat(buf, _slice(&src[tokens[i].src_pos], 10));
        for (char *s = buf; *s != 0; s++) {
            if (*s == '\n') *s = ' ';
        }
        debug_s("token:", buf);
    }
}

void tokenize() {
    while (ch() != -1) {
        if (expect_str("!=")) {
            add_token(T_NE);
        } else if (expect_c('!')) {
            add_token(T_L_NOT);
        } else if (expect_str("==")) {
            add_token(T_EQ);
        } else if (expect_str("&&")) {
            add_token(T_L_AND);
        } else if (expect_str("&=")) {
            add_token(T_AMP_EQUAL);
        } else if (expect_str("||")) {
            add_token(T_L_OR);
        } else if (expect_str("|=")) {
            add_token(T_PIPE_EQUAL);
        } else if (expect_str("^=")) {
            add_token(T_HAT_EQUAL);
        } else if (expect_c('&')) {
            add_token(T_AMP);
        } else if (expect_c('=')) {
            add_token(T_EQUAL);
        } else if (expect_str("<=")) {
            add_token(T_LE);
        } else if (expect_c('<')) {
            add_token(T_LT);
        } else if (expect_str(">=")) {
            add_token(T_GE);
        } else if (expect_c('>')) {
            add_token(T_GT);
        } else if (expect_str("*=")) {
            add_token(T_ASTERISK_EQUAL);
        } else if (expect_c('*')) {
            add_token(T_ASTERISK);
        } else if (expect_str("/=")) {
            add_token(T_SLASH_EQUAL);
        } else if (expect_c('/')) {
            add_token(T_SLASH);
        } else if (expect_str("%=")) {
            add_token(T_PERCENT_EQUAL);
        } else if (expect_c('%')) {
            add_token(T_PERCENT);
        } else if (expect_str("++")) {
            add_token(T_INC);
        } else if (expect_str("+=")) {
            add_token(T_PLUS_EQUAL);
        } else if (expect_c('+')) {
            add_token(T_PLUS);
        } else if (expect_str("--")) {
            add_token(T_DEC);
        } else if (expect_str("-=")) {
            add_token(T_MINUS_EQUAL);
        } else if (expect_str("->")) {
            add_token(T_ALLOW);
        } else if (expect_c('-')) {
            add_token(T_MINUS);
        } else if (expect_c('{')) {
            add_token(T_LBLACE);
        } else if (expect_c('}')) {
            add_token(T_RBLACE);
        } else if (expect_c('[')) {
            add_token(T_LBRACKET);
        } else if (expect_c(']')) {
            add_token(T_RBRACKET);
        } else if (expect_c('(')) {
            add_token(T_LPAREN);
        } else if (expect_c(')')) {
            add_token(T_RPAREN);
        } else if (expect_c(';')) {
            add_token(T_SEMICOLON);
        } else if (expect_c('.')) {
            add_token(T_PERIOD);
        } else if (expect_c(',')) {
            add_token(T_COMMA);
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
        } else if (expect_reserved_str("print")) {
            add_token(T_PRINT);
        } else if (expect_reserved_str("return")) {
            add_token(T_RETURN);
        } else if (expect_reserved_str("sizeof")) {
            add_token(T_SIZEOF);
        } else if (expect_reserved_str("struct")) {
            add_token(T_STRUCT);
        } else if (expect_reserved_str("typedef")) {
            add_token(T_TYPEDEF);
        } else if (expect_reserved_str("union")) {
            add_token(T_UNION);
        } else if (expect_reserved_str("enum")) {
            add_token(T_ENUM);
        } else if (expect_reserved_str("while")) {
            add_token(T_WHILE);
        } else {
            int i;
            char c;
            char *str;
            if (parse_int_token(&i)) {
                add_int_token(i);
            } else if (parse_char_token(&c)) {
                add_char_token(c);
            } else if (parse_string_token(&str)) {
                add_string_token(str);
            } else if (parse_ident_token(&str)) {
                add_ident_token(str);
            } else {
                char buf[100];
                buf[0] = 0;
                strcat(buf, "Invalid token: \n");
                strcat(buf, _slice(&src[(src_pos > 20) ? src_pos - 20 : 0], 20));
                strcat(buf, " --> ");
                strcat(buf, _slice(&src[src_pos], 20));
                // dump_tokens();

                error(buf);
            }
        }
    }
    add_token(T_EOF);
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

void init() {
    src_len = _read(0, src, 1024);
}

