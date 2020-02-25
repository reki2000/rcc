int main() {
    int i;
    for (i=1; i<4; i++) {
        print(i);
        if (i==1) {
            continue;
        }
        break;
    }
    return 0;
}
