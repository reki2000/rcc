int main() {
    int i = 1;
    while (i<4) {
        print(i);
        if (i==1) {
            i++;
            continue;
        }
        break;
        i++;
    }
    return 0;
}
