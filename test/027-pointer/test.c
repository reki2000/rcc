int add(int *i1, int i2) {
    return *i1 + i2;
}

int main() {
    int i;
    int *j;
    i = 1;
    j = &i;
    *j = 5;
    return add(&i, *j);
}

