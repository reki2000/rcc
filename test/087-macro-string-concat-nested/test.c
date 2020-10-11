#define m1(a) m2(a,_def)
#define m2(a,b) a##b
int abc_def=55;
int main() {
  return m1(abc);
}
