int main() {
    struct point {
        char name[16];
        int y;
        struct point *p;
    } t;
    t.p = &t;
    t.p->y = 20;
    return t.y;
}
