#!/bin/bash

cd $(dirname $BASH_SOURCE)

function clean {
    rm out/* 2>/dev/null
}

function compile {
    ../bin/rekicc $1 2>$2 > out/test.s
}

function header {
    cat <<EOT
extern int printf(const char*, ...);
void print(int i) { printf("%d\n", i);}
EOT
}

function run {
    echo "test: $1 ------------------"
    clean
    compile $1 out/debug.log
    true
}

function check_result {
    gcc -o out/test.out out/test.s \
        && ( out/test.out > out/result.txt; echo $? >> out/result.txt ) \
        || ( echo "ERROR"; cat out/debug.log; false )
    diff -B $1 out/result.txt
}

function check_error {
    grep 'ERROR' out/debug.log > out/error.txt
    diff -B $1 out/error.txt
}

function check {
    if [ -f $2 ]; then
        check_error $2
    else
        check_result $1
    fi \
        || echo "ERROR: result not matched"
    echo "end"
}

set -e

function all {
    for t in 0*; do
        run $t/test.c && check $t/expect.txt $t/expect-error.txt || exit
    done
}

if [ -z "$1" ]; then
    all
else
    for t in ${1}*; do
        run $t/test.c && check $t/expect.txt $t/expect-error.txt || exit
    done
fi

