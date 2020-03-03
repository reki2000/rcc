int sub(int i) {
    switch (i) {
        case 1:
        case 2:
            print(100);
            break;
        case 3:
            print(101);
            break;
        default:
            print(102);
    }
}

int main() {
    sub(1);
    sub(2);
    sub(3);
    sub(4);
    return 0;
}
