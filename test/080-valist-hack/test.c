int add(int num, ...) {
    int result = 0;

    char *_pp = (char *)&num;
    char *_pp2 = _pp + sizeof(num) + 8 + 8;
    int _i = 6 - 1;
    char *_p = (_i > 0)? _pp : _pp2;

    for (int i=0; i<num; i++) {
        _p = (_i>0)? _p-8 : (_i==0)? _pp2 : _p+8; _i--;
        result += *(int *)_p;
    }
    return result;
}

int main(int argc, char **argv) {
    return add(8, 1,2,3,4,5,6,7,8);
}
