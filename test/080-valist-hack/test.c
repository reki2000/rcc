int add(int num, ...) {

    char *_p = (char *)&num - (8 - sizeof(num));
    char *_pp2 = (char *)&num + sizeof(num) + 8 + 8; // stack args
    int _i = 6 - 1;

    int result = 0;
    for (int i=0; i<num; i++) {
        _p = (_i>0)? _p-8 : (_i==0)? _pp2 : _p+8; _i--;
        result += *(int *)_p;
    }
    return result;
}

int main(int argc, char **argv) {
    return add(8, 1,2,3,4,5,6,7,8);
}
