/*
 * vec_template.h 
 * 
 * This is a generic vector-like type generator which supports operations of push(), pop(), top() and automatic size extention.
 * - <vector_type>_push(vec, item) : add an item to the vector
 * - <vector_type>_pop(vec) : returns the top item and remove it from the vector
 * - <vector_type>_top(vec) : return top item
 * - <vector_type>_new() : returns a pointer to allocated vector
 * - vec->len : length of the vector
 * - vec->items[index] : item at the index position
 * 
 * To use, include this header file and write as below:
 * 
 * VEC_HEADER(<target_type>, <vector_type_name>)
 * - Generates the declaration of vector's body struct type and operating functions. Usually written in .h files.
 * 
 * VEC_BODY(<target_type>, <vector_type_name>)" 
 * - Generates the body of operating functions. Usually written in .c files.
 * 
 * Note: <vector_type_name> might be better to named as <target_type>_vec, or <pointed_type>_p_vec if <target_type> is a pointer.
 */
#define VEC_MODERATE_EXTEND 1024*1024

#define VEC_CONCAT(a,b) a ## b
#define VEC_HEADER(item_t, container_t) \
typedef struct { \
    item_t *items; \
    int len; \
    int cap; \
} container_t; \
\
extern container_t *VEC_CONCAT(container_t,_new)(); \
extern item_t VEC_CONCAT(container_t,_push)(container_t *p, item_t v);\
extern item_t VEC_CONCAT(container_t, _pop)(container_t *p);\
extern item_t VEC_CONCAT(container_t, _top)(container_t *p);\

#define VEC_BODY(item_t, container_t) \
\
void VEC_CONCAT(container_t, _extend)(container_t *p) {\
    p->len++;\
    if (p->len >= p->cap) {\
        p->cap += (p->cap < VEC_MODERATE_EXTEND) ? p->cap : (p->cap >> 2);\
        p->items = realloc(p->items, p->cap * sizeof(item_t));\
    }\
}\
\
container_t *VEC_CONCAT(container_t, _new)() {\
    container_t *p = malloc(sizeof(container_t));\
    p->cap = 8;\
    p->len = 0;\
    p->items = malloc(p->cap * sizeof(item_t));\
    return p;\
}\
\
item_t VEC_CONCAT(container_t,_push)(container_t *p, item_t v) {\
    VEC_CONCAT(container_t, _extend)(p);\
    p->items[p->len-1] = v;\
    return v;\
}\
\
item_t VEC_CONCAT(container_t, _pop)(container_t *p) {\
    if (p->len > 0) { item_t x = p->items[p->len-1]; p->len--; return x; } else return (item_t)0;\
}\
\
item_t VEC_CONCAT(container_t, _top)(container_t *p) {\
    return p->len > 0 ? p->items[p->len-1] : (item_t)0;\
}\


