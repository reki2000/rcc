extern long strlen(const char *);
extern char *strdup(const char *);
extern char *strcat(char *, const char *);
extern int strcmp(char *, const char *);
extern int strncmp(const char *, const char *, long);

extern void _strcat3(char *buf, char *s1, int i, char *s2);

extern int vsnprintf(char *buf, long size, const char *fmt, va_list v);
extern int snprintf(char *buf, long size, const char *fmt, ...);

