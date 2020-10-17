#include "types.h"
#include "vec.h"
#include "rsys.h"
#include "rstring.h"
#include "devtool.h"
#include "file.h"

#define NUM_FILES 1000
#define SIZE_FILE_BUF (1024*1024)

src_t *src;

VEC_HEADER(src_t, src_vec)
VEC_BODY(src_t, src_vec)

src_vec srcs;

int src_file_id_stack[NUM_FILES];
int src_file_stack_top = 0;

char_p_vec include_dirs = 0;

void add_include_dir(char *dir) {
    if (include_dirs == 0) include_dirs = char_p_vec_new();
    char_p_vec_push(include_dirs, dir);
}

void dirname(char *out, char*path) {
    int i;
    int last_delimiter_index = 0;
    for (i=0; i<strlen(path); i++) {
        if (path[i] == '/') {
            last_delimiter_index = i;
        }
    }
    for (i=0; i<last_delimiter_index; i++) {
        out[i] = path[i];
    }
    out[last_delimiter_index] = 0;
    return;
}

src_t *get_current_file() {
    if (src_file_stack_top <= 0) {
        return 0;
    }
    return src_vec_get(srcs, src_file_id_stack[src_file_stack_top - 1]);
}

int open_include_file(char *filename) {
    int i;
    for (i=0; i<char_p_vec_len(include_dirs); i++) {
        char path[RCC_BUF_SIZE] = {0};
        strcat(path, *char_p_vec_get(include_dirs, i));
        strcat(path, "/");
        strcat(path, filename);
        int fd = open(path, 0);
        if (fd >= 0) {
            return fd;
        }
        debug("include file not found at:%s", path);
    };
    return -1;
}

char *load_file(char *filename) {
    char *buf = calloc(SIZE_FILE_BUF,1);

    int fd;

    if (srcs == 0) srcs = src_vec_new();
    if (src_vec_len(srcs) == 0) {
        char *b = calloc(1, RCC_BUF_SIZE);
        dirname(b, filename);
        add_include_dir(b);
        fd = open(filename, 0);
    } else {
        fd = open_include_file(filename);
    }

    if (fd == -1) {
        error("cannot open include file: %s", filename);
    }

    int len = read(fd, buf, SIZE_FILE_BUF);
    if (close(fd)) {
        debug("closing fd: %d", fd);
        error("error on closing file: %s", filename);
    }

    buf = realloc(buf, len + 1);
    buf[len] = '\0';

    debug("loaded: %s", filename);
    return buf;
}

bool enter_new_file(char *filename, char *body, int pos, int len, int line, int column) {
    src_t *s = src_vec_extend(srcs, 1);

    s->id = src_vec_len(srcs) - 1;
    s->filename = filename;
    s->body = body;

    s->pos = pos;
    s->len = len;
    s->line = line;
    s->column = column;

    s->prev_line = 1;
    s->prev_column = 1;
    s->prev_pos = 0;

    if (src_file_stack_top >= NUM_FILES) {
        error("too many file stack:%s", filename);
    }
    src_file_id_stack[src_file_stack_top] = s->id;
    src_file_stack_top++;

    src = get_current_file();

    debug("entered to file:%s #%d %x len:%d", src->filename, src->id, src->filename, src->len);
    //dump_file_stack();

    return TRUE;
}

bool enter_file(char *filename) {
    char *buf = load_file(filename);
    return enter_new_file(filename, buf, 0, strlen(buf), 1, 1);
}

bool exit_file() {
    if (src_file_stack_top == 0) {
        error("invalid exit from the root file");
    }
    debug("exiting file #%d:%s %x", src->id, src->filename, src->filename);
    //dump_file_stack();

    src_file_stack_top--;
    src = get_current_file();
    return TRUE;
}

void dump_file_stack() {
    for(int i=0; i<src_file_stack_top; i++) {
        int id = src_file_id_stack[i];
        debug("stack:%d id:%d name:%s %d, %s", i, id, src_vec_get(srcs,id)->filename, src_vec_get(srcs, id), src == src_vec_get(srcs, id) ? "<--" : "");
    }
}

/*
 * display the part of the file in 1-line style, sorrounded by '=>' and '<='
 */
char *dump_file(int id, int start_pos, int end_pos) {
    if (start_pos > end_pos) {
        return "* invalid pos for dump_file *";
    }
    int line_start_pos = start_pos;
    char *body = src_vec_get(srcs, id)->body;

    while (line_start_pos >= 0 && body[line_start_pos] != '\n' && start_pos - line_start_pos < 40) {
        line_start_pos--;
    }
    line_start_pos++;

    int line_end_pos = end_pos;
    while (line_end_pos < src_vec_get(srcs, id)->len && body[line_end_pos] != '\n' && line_end_pos - end_pos < 40) {
        line_end_pos++;
    }

    int line_size = line_end_pos - line_start_pos + 1;

    char *buf = calloc(1, line_size + 4 + 4 + 1 + 10);
    char *p = buf;
    int i = line_start_pos;
    while (i < start_pos) { *p++ = body[i++]; }
    strcat(p, " => "); p+=4;
    while (i <= end_pos && i < line_end_pos) { 
        if (start_pos + 30 < i && i < end_pos - 30) {
            strcat(p, " ... "); p+=5;
            while (i < end_pos - 30) i++;
        } else {
            *p++ = body[i++]; 
        }
    }
    strcat(p, " <= "); p+=4;
    while (i < line_end_pos) { *p++ = body[i++]; }
    *p = '\0';

    return buf;
}

char *file_get_part(int id, int start_pos, int end_pos) {
    char *body = src_vec_get(srcs, id)->body;
    int line_size = end_pos - start_pos + 1;
    char *buf = calloc(1, line_size + 3 + 5 + 5 + 2);
    char *p = buf;
    int i = start_pos;
    while (i <= end_pos) { *p++ = body[i++]; }
    return buf;
}

src_t *file_info(int id) {
    return src_vec_get(srcs,id);
}

bool is_eof() {
    return src->pos >= src->len;
}

int ch() {
    if (is_eof()) {
        return -1;
    }
    return src->body[src->pos];
}

int ch_next() {
    if (src->pos+1 >= src->len) {
        return -1;
    }
    return src->body[src->pos+1];
}

bool next() {
    if (is_eof()) return FALSE;

    if (ch() == '\n') {
        src->column = 1;
        src->line++;
    }
    src->pos++;
    src->column++;

    while (ch() == '\\' && ch_next() == '\n') {
        src->pos+=2;
        src->line++;
        src->column = 1;
    }
    return TRUE;
}
