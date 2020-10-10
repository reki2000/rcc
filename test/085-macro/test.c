#define p(a,b,c) \
  puts((a)); \
\
  puts(b);

#define p2 (0)
extern int puts(const char *);
int main() {
  char *a;
  char *b;
  char *c="ABC";
  p((c[0],a="abc",b=a),"xyz","a,\"b,c");
}
