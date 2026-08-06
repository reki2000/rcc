#include "types.h"
#include "devtool.h"
#include "rstring.h"

extern int open(const char*, int, int);
extern int close(int);
#define O_CREAT 0x40
#define O_TRUNC 0x200
#define O_WRONLY 1

extern void tokenize_file(char *);
extern void add_include_dir(char *);
extern int parse();
extern void compile_file(int fd);
extern void ssa_compile_file(int fd);

extern bool opt_use_ssa;
extern bool opt_dump_ssa;

int main(int argc, char **argv) {
    int arg_index;
    bool out_asm_source = FALSE;
    int output_fd = 1;

    for (arg_index = 1;  arg_index < argc; arg_index++) {
        if (strncmp("-I", argv[arg_index], 2) == 0) {
            add_include_dir(&argv[arg_index][2]);
            continue;
        }
        if (strncmp("-S", argv[arg_index], 2) == 0) {
            out_asm_source = TRUE;
            continue;
        }
        if (strcmp("-fno-ssa", argv[arg_index]) == 0) {
            opt_use_ssa = FALSE;    // fall back to the direct AST emitter
            continue;
        }
        if (strcmp("-fdump-ssa", argv[arg_index]) == 0) {
            opt_dump_ssa = TRUE;
            continue;
        }
        if (strncmp("-o", argv[arg_index], 2) == 0) {
            arg_index++;
            if (arg_index < argc) {
                output_fd = open(argv[arg_index], O_CREAT | O_TRUNC | O_WRONLY, 0644);
                if (output_fd == -1) {
                    error("cannot open output file: %s", argv[arg_index]);
                }
            } else {
                error("specified -o option without file name");
            }
            debug("write output to file:%s", argv[arg_index]);
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

    parse();

    if (opt_use_ssa) {
        ssa_compile_file(output_fd);
    } else {
        compile_file(output_fd);
    }

    if (output_fd != 1) {
        close(output_fd);
    }
}
