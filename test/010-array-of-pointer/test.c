int main() {
    int *a[10];
    int j;
    a[2] = &j;
    *a[2] = 20;
    return *a[2];
}
