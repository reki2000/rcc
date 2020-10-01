typedef struct {
    int g_offset;
    int f_offset;
    char *stack;
    char *reg;
    long num_fp;
} va_list;

void va_start(va_list* va, char *arg1, int size1) {
    va->stack = arg1 + size1 + 8 + 8;
    va->reg = arg1 - (8- size1) - 8 * 6;
    va->g_offset = 8;
    va->f_offset = 48;
    va->num_fp = 0;
}

extern int vsprintf(char *buf, const char *fmt, va_list *v);
extern int strlen(const char *);
extern int write(int fd, const char *buf, int size);

void x(int num, ...) {
    va_list va;

    va_start(&va, (char *)&num, sizeof(num));

    char buf[200];
    vsprintf(buf, "%d %d %d %d %d %d %d %d\n", &va);
    write(1, buf, strlen(buf));
}

int main(int argc, char **argv) {
    x(8, 1,2,3,4,5,6,7,8);
}
