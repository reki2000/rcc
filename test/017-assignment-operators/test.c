int main() {
    int a;
    a=0;
    print(a+=100);
    print(a-=50);
    print(a/=10);
    print(a*=3);
    print(a%=10);

    a=14;
    print(a|=1);
    print(a&=7);
    print(a^=1);

    return a;
}
