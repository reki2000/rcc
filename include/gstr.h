extern char *find_global_string(int);
extern int add_global_string(char *);

extern int alloc_global_array();
extern void add_global_array(int pos, int value);
extern int get_global_array(int pos, int offset);
extern int get_global_array_length(int pos);
