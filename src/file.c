#include "types.h"
#include "rsys.h"
#include "rstring.h"
#include "devtool.h"
#include "file.h"

#define NUM_FILES 1000
#define NUM_INCLUDE_DIRS 100
#define SIZE_FILE_BUF (1024*1024)

src_t *src;

src_t src_files[NUM_FILES];
int src_file_len = 0;

int src_file_id_stack[NUM_FILES];
int src_file_stack_top = 0;

char *include_dirs[NUM_INCLUDE_DIRS];
int include_dirs_len = 0;

void add_include_dir(char *dir) {
    if (include_dirs_len >= NUM_INCLUDE_DIRS) {
        error("too much include directories");
    }
    include_dirs[include_dirs_len++] = dir;
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
    return &src_files[src_file_id_stack[src_file_stack_top - 1]];
}

int open_include_file(char *filename) {
    int i;
    for (i=0; i<include_dirs_len; i++) {
        char path[RCC_BUF_SIZE] = {0};
        strcat(path, include_dirs[i]);
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

src_t *load_file(char *filename) {
    if (src_file_len >= NUM_FILES) {
        error("too much include files");
    }

    src_t *s = &src_files[src_file_len];

    s->id = src_file_len;
    s->filename = filename;
    s->body = calloc(1, SIZE_FILE_BUF);

    s->pos = 0;
    s->len = 0;
    s->line = 1;
    s->column = 1;

    s->prev_line = 1;
    s->prev_column = 1;
    s->prev_pos = 0;

    int fd;

    if (src_file_len == 0) {
        char *buf = calloc(1, RCC_BUF_SIZE);
        dirname(buf, filename);
        add_include_dir(buf);
        fd = open(filename, 0);
    } else {
        fd = open_include_file(filename);
    }

    if (fd == -1) {
        error("cannot open include file: %s", filename);
    }

    s->len = read(fd, s->body, SIZE_FILE_BUF);
    if (close(fd)) {
        debug("closing fd: %d", fd);
        error("error on closing file: %s", filename);
    }

    src_file_len++;

    debug("loaded: %s", s->filename);
    return s;
}

bool enter_file(char *filename) {
    src_t *s = load_file(filename);

    if (src_file_stack_top >= NUM_FILES) {
        error("too many file stack:%s", filename);
    }
    src_file_id_stack[src_file_stack_top] = s->id;
    src_file_stack_top++;

    src = get_current_file();
    return TRUE;
}

bool exit_file() {
    if (src_file_stack_top == 0) {
        error("invalid exit from the root file");
    }
    src_file_stack_top--;
    src = get_current_file();
    return TRUE;
}

/*
 * display the part of the file in 1-line style, sorrounded by '=>' and '<='
 */
char *dump_file(int id, int start_pos, int end_pos) {
    if (start_pos > end_pos) {
        return "* invalid pos for dump_file *";
    }
    int line_start_pos = start_pos;
    char *body = src_files[id].body;

    while (line_start_pos >= 0 && body[line_start_pos] != '\n') {
        line_start_pos--;
    }
    line_start_pos++;

    int line_end_pos = end_pos;
    while (line_end_pos < src_files[id].len && body[line_end_pos] != '\n') {
        line_end_pos++;
    }

    int line_size = line_end_pos - line_start_pos + 1;

    char *buf = calloc(1, line_size + 3 + 5 + 5 + 2);
    char *p = buf;
    int i = line_start_pos;
    while (i < start_pos) { *p++ = body[i++]; }
    strcat(p, " => "); p+=4;
    while (i <= end_pos) { *p++ = body[i++]; }
    strcat(p, " <= "); p+=4;
    while (i < line_end_pos) { *p++ = body[i++]; }

    return buf;
}

src_t *file_info(int id) {
    if (id < 0 || id >= src_file_len) {
        error("invalid file id:%d", id);
    }
    return &src_files[id];
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

bool next() {
    if (ch() == '\n') {
        src->column = 1;
        src->line++;
    }
    if (is_eof()) {
        return FALSE;
    }
    src->pos++;
    src->column++;
    return TRUE;
}
