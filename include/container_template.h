#define CONTAINER_MODERATE_EXTEND 1024*1024

/*
 * plist is a generic container
 */
#define CONCAT(a,b) a ## b
#define CONTAINER_HEADER(item_t, container_t) \
typedef struct { \
    item_t *items; \
    int len; \
    int cap; \
} container_t; \
\
extern container_t *CONCAT(container_t,_new)(); \
extern item_t CONCAT(container_t, _get)(container_t *p, int index);\
extern item_t CONCAT(container_t, _set)(container_t *p, int index, item_t v);\
extern item_t CONCAT(container_t,_push)(container_t *p, item_t v);\
extern item_t CONCAT(container_t, _pop)(container_t *p);\
extern item_t CONCAT(container_t, _top)(container_t *p);\

#define CONTAINER_BODY(item_t, container_t) \
\
void CONCAT(container_t, _extend)(container_t *p) {\
    p->len++;\
    if (p->len >= p->cap) {\
        p->cap += (p->cap < CONTAINER_MODERATE_EXTEND) ? p->cap : (p->cap >> 2);\
        p->items = realloc(p->items, p->cap * sizeof(item_t));\
    }\
}\
\
container_t *CONCAT(container_t, _new)() {\
    container_t *p = malloc(sizeof(container_t));\
    p->cap = 8;\
    p->len = 0;\
    p->items = malloc(p->cap * sizeof(item_t));\
    return p;\
}\
\
item_t CONCAT(container_t, _get)(container_t *p, int index) {\
    return p->items[index];\
}\
\
item_t CONCAT(container_t, _set)(container_t *p, int index, item_t v) {\
    return p->items[index] = v;\
}\
\
item_t CONCAT(container_t,_push)(container_t *p, item_t v) {\
    CONCAT(container_t, _extend)(p);\
    p->items[p->len-1] = v;\
    return v;\
}\
\
item_t CONCAT(container_t, _pop)(container_t *p) {\
    if (p->len > 0) { item_t x = p->items[p->len-1]; p->len--; return x; } else return (item_t)0;\
}\
\
item_t CONCAT(container_t, _top)(container_t *p) {\
    return p->len > 0 ? p->items[p->len-1] : (item_t)0;\
}\

