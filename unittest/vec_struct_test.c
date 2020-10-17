#include "vec.h"

extern int puts(const char *);
extern int printf(const char *, ...);
extern int strcmp(const char *, const char *);
extern void exit(int);

typedef struct {
    int a[16];
    int b;
} xyz;

void assert_eq_str(const char *a, const char *b) {
    if (strcmp(a,b) != 0) {
        printf("expected:%s actual:%s\n", a, b);
        exit(-1);
    }
}

void assert_eq_int(int a, int b) {
    if (a != b) {
        printf("expected:%d actual:%d\n", a, b);
        exit(-1);
    }
}

extern void *calloc(long, long);
extern void *malloc(long);
extern void *realloc(void *, long);

VEC_HEADER(xyz, xyz_vec)
VEC_BODY(xyz, xyz_vec)

void sub(xyz_vec v) {
    xyz *y = xyz_vec_top(v);
    for (int i=0; i<10; i++) { assert_eq_int(y->a[i], i); }
    assert_eq_int(y->b, 100);
}

int main() {
    xyz_vec v = xyz_vec_new();
    xyz x;
    for (int i=0; i<10; i++) { x.a[i] = i; }
    x.b = 100;
    xyz_vec_push(v, x);

    sub(v);
    
    return 0;
}
