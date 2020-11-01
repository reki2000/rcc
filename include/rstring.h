extern long strlen(const char *);
extern char *strdup(const char *);
extern char *strcat(char *, const char *);
extern int strcmp(char *, const char *);
extern int strncmp(const char *, const char *, long);

extern int vsnprintf(char *buf, long size, const char *fmt, va_list v);
extern int snprintf(char *buf, long size, const char *fmt, ...);

extern bool is_alpha(int c);
extern bool is_digit(int c);
extern bool is_space(int c);

extern char unescape_char(char escaped_char);
extern void escape_string(char *buf, const char *str);
