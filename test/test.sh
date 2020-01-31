#!/bin/bash

cd $(dirname $BASH_SOURCE)

function clean {
    rm out/*
}

function compile {
    ../bin/rekicc < $1 2>$2 > out/test.s
}
function run {
    local quiet="/dev/tty"
    if [ "$1" == "-q" ]; then
      quiet="out/debug.log"
      shift
    fi
    echo "test: $1 ------------------"
    clean
    compile $1 $quiet \
        && gcc -o out/test.out out/test.s \
        && ( out/test.out > out/result.txt; echo $? >> out/result.txt )  \
        && diff $2 out/result.txt \
        && echo "end : $1 ------------------" \
        || ( echo "ERROR"; cat out/debug.log; false )
}

set -e

function all {
    for t in 0*; do
        run -q $t/test.c $t/expect.txt || exit
    done
}

if [ -z "$1" ]; then
    all
else
    for t in ${1}*; do
        run $t/test.c $t/expect.txt || exit
    done
fi

