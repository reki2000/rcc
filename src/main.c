#include "devtool.h"
#include "rstring.h"
#include "types.h"

extern void tokenize_file(char *);
extern void add_include_dir(char *);
extern int parse();
extern void emit();

int main(int argc, char *argv[]) {
    int arg_index;
    bool out_asm_source = FALSE;

    for (arg_index = 1;  arg_index < argc; arg_index++) {
        if (strncmp("-I", argv[arg_index], 2) == 0) {
            add_include_dir(&argv[arg_index][2]);
            continue;
        }
        if (strncmp("-S", argv[arg_index], 2) == 0) {
            out_asm_source = TRUE;
            continue;
        }
        break;
    }
    if (arg_index >= argc) {
        error("no source file name");
    }
    if (!out_asm_source) {
        error("need -S option. This copmiler only outputs asm source.");
    }

    tokenize_file(argv[arg_index]);

    int pos = parse();
    if (pos == 0) {
        error("Invalid source code");
    }

    emit();
}
