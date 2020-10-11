#include "types.h"
#include "vec.h"
#include "rsys.h"
#include "rstring.h"
#include "devtool.h"

#include "file.h"
#include "macro.h"
#include "token.h"
#include "gstr.h"


#define NUM_IFDEF 1000
bool _ifdef_skip[NUM_IFDEF];
int ifdef_top = -1;

void ifdef_start(bool skip) {
    if (ifdef_top >= NUM_IFDEF) {
        error("too many #ifdef nest");
    }
    ifdef_top++;
    _ifdef_skip[ifdef_top] = skip;
}

void ifdef_else() {
    if (ifdef_top < 0) {
        error("found #else without #ifdef/ifndef");
    }
    _ifdef_skip[ifdef_top] = !_ifdef_skip[ifdef_top];
}

bool ifdef_skip() {
    if (ifdef_top < 0) return FALSE;
    return _ifdef_skip[ifdef_top];
}

void ifdef_end() {
    if (ifdef_top < 0) {
        error("found #endif without #ifdef/ifndef");
    }
    ifdef_top--;
}


#define NUM_TOKENS  (1024*128)

token tokens[NUM_TOKENS];
int token_pos = 0;
int token_len = 0;

void set_src_pos() {
    src->prev_column = src->column;
    src->prev_pos = src->pos;
    src->prev_line = src->line;
}

void to_eol() {
    while (ch() != '\n' && !is_eof()) next();
}

void skip() {
    while (!is_eof()) {
        int c = ch();
        if (c == '/') { // scan comment
            int pos = src->pos;
            next();
            if (ch() == '/') {
                debug("scanning //...");
                to_eol();
            } else if (ch() == '*') {
                next();
                debug("scanning /*...");
                while (!is_eof()) {
                    if (ch() == '*') {
                        next();
                        if (ch() == '/') {
                            break;
                        }
                    }
                    next();
                }
            } else {
                src->pos = pos;
                break;
            }
        } else if (!is_space(c)) {
            break;
        }
        next();
    }
    set_src_pos();
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
        error("invalid char literal:%d", ch());
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
    debug("token:#%d: id:%d src_id:%d %s:%d:%d |%s|", pos, t->id, t->src_id, s->filename, t->src_line, t->src_column, dump_file(t->src_id, t->src_pos, t->src_end_pos));
}

void dump_token_simple(char *buf, int pos) {
    token *t = &tokens[pos];
    src_t *s = file_info(t->src_id);

    snprintf(buf, RCC_BUF_SIZE, "%s:%d:%d |%s|", s->filename, t->src_line, t->src_column, dump_file(t->src_id, t->src_pos, t->src_end_pos));
}

void dump_tokens() {
    int i = token_pos;
    if (i<0 || i>=token_len) {
        return;
    }
    dump_token(i, &tokens[i]);
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
    if (!tokenize_ident(&name)) error("no identifier for define directive");

    char_p_vec vars = char_p_vec_new();
    if (ch() == ('(')) {
        next();
        while (vars->len == 0 || accept_char(',')) {
            char *var_name;
            if (!tokenize_ident(&var_name)) error("invalid macro arg declaration");
            char_p_vec_push(vars, var_name);
        }
        if (!accept_char(')')) error("stray macro args end");
    }

    skip();
    int start_pos = src->pos;
    to_eol();
    int end_pos = src->pos - 1;
    add_macro(name, start_pos, end_pos, vars);
}

void directive_undef() {
    char *name;
    if (!tokenize_ident(&name)) error("no identifier for define directive");

    delete_macro(name);
}

void preprocess() {
    if (accept_ident("ifdef")) {
        bool skip = FALSE;
        if (!ifdef_skip()) {
            char *name;
            if (!tokenize_ident(&name)) error("#ifdef needs a identifier");
            skip = !find_macro(name);
        }
        ifdef_start(skip);
    } else if (accept_ident("ifndef")) {
        bool skip = FALSE;
        if (!ifdef_skip()) {
            char *name;
            if (!tokenize_ident(&name)) error("#ifndef needs a identifier");
            skip = (bool)(find_macro(name) != NULL);
        }
        ifdef_start(skip);
    } else if (accept_ident("else")) {
        ifdef_else();
    } else if (accept_ident("endif")) {
        ifdef_end();
    } else if (!ifdef_skip()) {
        if (accept_ident("include")) {
            directive_include();
        } else if (accept_ident("define")) {
            directive_define();
        } else if (accept_ident("undef")) {
            directive_undef();
        } else {
            error("unknown directive");
        }
    }
    set_src_pos();
}

void tokenize() {
    int concat_start_token_pos = -1;

    while (!is_eof()) {
        if (accept_char('#')) {
            preprocess();

        } else if (ifdef_skip()) {
            to_eol();
            set_src_pos();

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
                    exit_file();
                } else {
                    add_ident_token(str);
                }
            } else {
                error("invalid_token: %s:%d:%d [%d] %s => %s", 
                    src->filename, src->line, src->column, ch(), 
                    _slice(&(src->body[max(src->pos - 20, 0)]), 20),
                    _slice(&(src->body[src->pos]), 20));
            }
        }

        // do macro string concatination
        if (accept_string("##")) {
            if (concat_start_token_pos == -1) {
                concat_start_token_pos = token_len - 1;
            }
            token *t = &tokens[token_len - 1];
            debug("found ## @ start_token_pos:%d [%s]", concat_start_token_pos, file_get_part(t->src_id, t->src_pos, t->src_end_pos));
        } else if (concat_start_token_pos != -1) {
            char *buf = calloc(RCC_BUF_SIZE, 1);
            for (int i = concat_start_token_pos; i < token_len; i++) {
                token *t = &tokens[i];
                strcat(buf, file_get_part(t->src_id, t->src_pos, t->src_end_pos));
                // debug("contatinating buf: %s, added %s, token:%d", buf, file_get_part(t->src_id, t->src_pos, t->src_end_pos), i);
            }
            buf = realloc(buf, strlen(buf)+1);

            token_len = concat_start_token_pos; // reset concatinated tokens!
            concat_start_token_pos = -1;

            enter_new_file(src->filename, buf, 0, strlen(buf), 1, 1);
            tokenize();
            exit_file();
        }
        skip();
    }

    // debug("start dump token %s ---------------", src->filename);
    // for(int i=0; i<token_len; i++) {
    //     token *t = &tokens[i];
    //     dump_token(i, t);
    // }
    // debug("end dump token %s ---------------", src->filename);

}

void tokenize_file(char *filename) {
    enter_file(filename);

    enter_file("rcc/args.h");  // defines __builtin_va_* macros
    tokenize();
    exit_file();

    tokenize();
    add_token(T_EOF);
    exit_file();
    debug("tokens:%d", token_len);
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
