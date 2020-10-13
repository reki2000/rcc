typedef struct { int x; int y; int z; } xyz;
int main() {
  xyz a[10];
  a[2].x = 10;
  a[2].y = 20;
  a[2].z = 30;
  xyz b[3];
  b[1] = a[2];
  return b[1].x + b[1].y + b[1].z;
}
