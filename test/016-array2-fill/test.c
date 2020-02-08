int main() {
    int a[1][3][5];
    int i;
    int j;
    int val;
    int ***p;

    val = 0;
    for (i=0; i<3; i++) {
        for (j=0; j<5; j++) {
            a[0][i][j] = val++;
        }
    }

    for (i=0; i<3; i++) {
        for (j=0; j<5; j++) {
            print(a[0][i][j]);
        }
    }
    return 0;
}
