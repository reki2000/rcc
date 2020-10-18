#define __builtin_va_start(va,first_arg) {\
    __builtin_va_list v = malloc(sizeof(__builtin_va_list)); \
    v->gp_offset = 8; \
    v->fp_offset = 48; \
    v->overflow_arg_area = (char*)&first_arg + sizeof(first_arg) + 8*2, /* 8*2 - pushed %rsp and callar address */ \
    v->reg_save_area = (char *)&first_arg - (8-sizeof(first_arg)) - 8*6 - 16*8, /* 8*6 - save area for 6 general regs + 8 fp regs  */ \
    va = v; \
    }

#define __builtin_va_end(va) 0

#define __builtin_va_arg(va, ret_type) \
    (*(ret_type *)((((va->gp_offset+=8) < (48+8)) ? (va->reg_save_area + va->gp_offset - 8) : (va->overflow_arg_area + va->gp_offset - 8 - 48))))

typedef struct {
    int gp_offset;
    int fp_offset;
    char *overflow_arg_area;
    char *reg_save_area;
} * __builtin_va_list;
