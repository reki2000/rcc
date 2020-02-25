int main() {
    int i=1;
    do {
        print(i);
        if (i==1) {
            i++;
            continue;
        }
        break;
        i++;
    } while (i<4);

    return 0;
}
