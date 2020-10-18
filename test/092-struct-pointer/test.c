typedef struct xyz_s xyz_t;

typedef struct xyz_s { struct xyz_s *p; char* a; char * b; char* c; int x; int y; int z; } xyz;


void show(xyz *a, int b) {
  print(a->x);
  print(a->y);
  print(a->z);
  print(b);
}
int main() {
  xyz a;
  a.x = 10;
  a.y = 20;
  a.z = 30;
  show(&a,40);
  return 0;
}
