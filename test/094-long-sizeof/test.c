int main() {
    print(sizeof(1)); // 4 (char)
    print(sizeof(0x7f)); // 4 (char)
    print(sizeof(0xff)); // 4 (char)

    print(sizeof(1L)); // 8 (long)
    print(sizeof(01L)); // 8 (long)
    print(sizeof(0x1L)); // 8 (long)

    print(sizeof(0x7fffffff)); // 4 (int max)
    print(sizeof(0x80000000)); // 4 (int max + 1)
    print(sizeof(0xffffffff)); // 4 (unsigned int max)
    print(sizeof(0x100000000)); // 8 (unsigned int max + 1)
    print(sizeof(0x7fffffffffffffff)); // 8 (long max)
    print(sizeof(0x8000000000000000)); // 8 (long max + 1)
    print(sizeof(0xffffffffffffffff)); // 8 (unsigned long max)

    print(sizeof(2147483647)); // 4 (int max)
    print(sizeof(2147483648)); // 8 (int max + 1)
    print(sizeof(4294967295)); // 8 (unsigned int max)
    print(sizeof(4294967296)); // 8 (unsigned int max + 1)
    print(sizeof(9223372036854775807)); // 8 (long - long max)
    // print(sizeof(9223372036854775808)); // 16 (long - long max + 1)
    // print(sizeof(18446744073709551615)); // 16 (long - unsigned long max)

    // print(sizeof(18446744073709551616)); // 4 (long - unsigned long max + 1)
}
