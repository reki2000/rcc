#include "types.h"
#include "rsys.h"
#include "rstring.h"
#include "devtool.h"
#include "file.h"

src_t *src;

src_t src_files[100];
int src_file_len = 0;

char *file_body[100];
int file_body_len = 0;

char *include_dirs[100];
int include_dirs_len = 0;

void add_include_dir(char *dir) {
    if (include_dirs_len >= 100) {
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
    if (src_file_len <= 0) {
        return 0;
    }
    return &src_files[src_file_len - 1];
}

int open_include_file(char *filename) {
    int i;
    for (i=0; i<include_dirs_len; i++) {
        char path[200] = {0};
        strcat(path, include_dirs[i]);
        strcat(path, "/");
        strcat(path, filename);
        int fd = open(path, 0);
        if (fd >= 0) {
            return fd;
        }
        debug_s("include file not found at:", path);
    };
    return -1;
}

bool enter_file(char *filename) {
    if (src_file_len >= 100) {
        error("too much include files");
    }

    src_t *s = &src_files[src_file_len];

    s->filename = filename;

    file_body[file_body_len] = calloc(1, 1024 * 1024);
    s->id = file_body_len;
    s->body = file_body[file_body_len];
    file_body_len++;

    s->pos = 0;
    s->len = 0;
    s->line = 1;
    s->column = 1;

    s->prev_line = 1;
    s->prev_column = 1;
    s->prev_pos = 0;

    int fd;

    if (src_file_len == 0) {
        char *buf = calloc(1, 100);
        dirname(buf, filename);
        add_include_dir(buf);
        fd = open(filename, 0);
    } else {
        fd = open_include_file(filename);
    }

    if (fd == -1) {
        error_s("cannot open include file: ", filename);
    }
    s->len = read(fd, s->body, 1024*1024);
    if (close(fd)) {
        debug_i("closing fd:", fd);
        error_s("error on closing file: ", filename);
    }

    src_file_len++;
    src = get_current_file();
    debug_s("loaded: ", s->filename);
    return TRUE;
}

bool exit_file() {
    if (src_file_len == 0) {
        error("invalid exit from the root file");
    }
    src_file_len--;
    src = get_current_file();
    return TRUE;
}

char *dump_file(int id, int pos) {
    return _slice(&file_body[id][pos], 10);
}

src_t *file_info(int id) {
    return &src_files[id];
}

void dump_src() {
    char buf[1000] = {0};
    src_t *s = file_info(0);
    strcat(buf, s->filename);
    debug_s("files:\n", buf);
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
