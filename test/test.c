int add(int *i1, int i2) {
    return *i1 + i2;
}

int main() {
    int i;
    i = 1;
    return add(&i, 2);
}

