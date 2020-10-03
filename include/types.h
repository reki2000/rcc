typedef enum {
    FALSE = 0,
    TRUE = 1
} bool;


#define RCC_BUF_SIZE 1024

#define ABI_NUM_REGISTER_PASS 6
#define ABI_NUM_FP_REGISTER_PASS 8
#define ABI_SZ_REGISTER_AREA (8*(6+8))
#define ALIGN_OF_STACK 8
