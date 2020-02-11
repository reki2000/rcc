int main() {
    union v {
        int int_value;
        char char_value;
    } x;

    x.int_value = 1024;
    x.char_value = 'A';
    print(x.int_value);
    return x.char_value;
}
