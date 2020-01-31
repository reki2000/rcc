int main() {
    int *a[10];
    int j;
    a[3] = &j;
    a[2] = &j;
    *a[3] = 10;
    return *a[3] + *a[2];
}
