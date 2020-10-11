#define x 100;
#undef x
#ifndef x 
int x = 10;
#endif

int main() {
  return x;
}
