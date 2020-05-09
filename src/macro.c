#include "types.h"
#include "devtool.h"
#include "rstring.h"
#include "file.h"
#include "macro.h"

#define NUM_MACROS 1000

macro_t macros[NUM_MACROS];
int macro_len = 0;

void add_macro(const char *name, int start_pos, int end_pos) {
    if (macro_len >= NUM_MACROS) {
        error("too many macros");
    }
    macro_t *m = &macros[macro_len++];
    m->name = strdup(name);
    m->src = src;
    m->start_pos = start_pos;
    m->end_pos = end_pos;

    debug_s("added macro:\n" , dump_file2(src->id, start_pos, end_pos));
}

macro_t *find_macro(const char *name) {
    for (int i=0; i<macro_len; i++) {
        if (strcmp(macros[i].name, name) == 0) {
            return &macros[i];
        }
    }
    return 0;
}

extern int src_file_len;
extern src_t src_files[];
extern src_t *get_current_file();

bool enter_macro(const char *name) {
    macro_t *m = find_macro(name);
    if (!m) {
        return FALSE;
    }

    if (src_file_len >= 100) {
        error("too much include files");
    }

    src_t *s = &src_files[src_file_len];

    s->filename = m->name;

    s->id = m->src->id;
    s->body = m->src->body;

    s->pos = m->start_pos;
    s->len = m->end_pos + 1;
    s->line = 1;
    s->column = 1;

    s->prev_line = s->line;
    s->prev_column = s->column;
    s->prev_pos = s->pos;

    src_file_len++;
    src = get_current_file();
    debug_s("entering macro: ", src->filename);

    return TRUE;
}

bool exit_macro() {
    if (src_file_len == 0) {
        error("invalid exit from the root file");
    }
    debug_s("exiting macro: ", src->filename);
    src_file_len--;
    src = get_current_file();
    return TRUE;
}
