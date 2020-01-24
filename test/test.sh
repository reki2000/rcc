#!/bin/bash

cd $(dirname $BASH_SOURCE)

function run {
    local quiet="/dev/tty"
    if [ "$1" == "-q" ]; then
      quiet="/dev/null"
      shift
    fi
    echo "test: $1 ------------------"
    ( ../bin/rekicc < $1 2>$quiet > out/test.s ) \
    && gcc -o out/test.out out/test.s \
    && ( ( out/test.out > out/result.txt ); echo $? >> out/result.txt )  \
    && diff $2 out/result.txt
    echo "end : $1 ------------------"
}

set -e

function all {
    for t in 0*; do
        run -q $t/test.c $t/expect.txt
    done
}

if [ -z "$1" ]; then
    all
else
    for t in ${1}*; do
        run $t/test.c $t/expect.txt
    done
fi

