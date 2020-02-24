int main() {
    int a[10];
    int i;

    for(i=0;i<10;i++) {
        a[i] = i;
    }
    for(i=0;i<5;i++) {
        print(a[i*2]);
    }
    return 0;
}
