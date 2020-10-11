typedef enum {
    FALSE = 0,
    TRUE = 1
} bool;

typedef __builtin_va_list va_list;

#define va_start(va,n) __builtin_va_start(va,n)
#define va_end(va) __builtin_va_end(va)
#define va_arg(va,ret_type) __builtin_va_arg(va,ret_type)
extern void *malloc(long);

#define RCC_BUF_SIZE 1024

#define ABI_NUM_GP 6
#define ABI_NUM_FP 8
#define ABI_REG_SAVE_AREA_SIZE (8*ABI_NUM_GP + 16*ABI_NUM_FP)
#define ALIGN_OF_STACK 8

#define NULL ((void *)0)
