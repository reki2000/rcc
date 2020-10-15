typedef struct { int x; int y; int z; } xyz;
void show(xyz a, int b) {
  print(a.x);
  print(a.y);
  print(a.z);
  print(b);
}
void show2(int a0, int a1, int a2, int a3, int a4, xyz a, int a5) {
  print(a.x);
  print(a.y);
  print(a.z);
  print(a5);
}
int main() {
  xyz a;
  a.x = 10;
  a.y = 20;
  a.z = 30;
  show(a,40);
  show2(0,1,2,3,4,a,40);
  return 0;
}
