#include "vec.h"

extern int puts(const char *);
extern int printf(const char *, ...);
extern int strcmp(const char *, const char *);
extern void exit(int);

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

int main() {
    char_p_vec *a = char_p_vec_new();
    char_p_vec_push(a, "xyz");
    for (int i=0; i<100; i++) char_p_vec_push(a, "abc");

    assert_eq_int(101, a->len);
    assert_eq_int(128, a->cap);
    assert_eq_str("abc", char_p_vec_top(a));

    a->items[100] = "xyz";
    assert_eq_str("xyz", char_p_vec_top(a));
    assert_eq_str("xyz", a->items[100]);

    assert_eq_str("xyz", char_p_vec_pop(a));
    assert_eq_str("abc", char_p_vec_pop(a));

    for (int i=0; i<98; i++) {
        char *x = char_p_vec_pop(a);
        assert_eq_int(98-i, a->len);
        assert_eq_str("abc", x);
    }

    assert_eq_str("xyz", char_p_vec_top(a));
    assert_eq_int(1, a->len);
}
