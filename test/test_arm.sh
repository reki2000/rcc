#!/bin/bash

cd $(dirname $BASH_SOURCE)

function exec_arm_gcc {
    docker run -v $(pwd):/test --rm -ti arm64 /bin/bash -c \
        "cd test; aarch64-linux-gnu-gcc-8 -static $*"    
}

function exec_arm_test {
    docker run -v $(pwd):/test --rm -ti arm64 /bin/bash -c \
        "cd test; (qemu-aarch64-static $1; echo $?;) >> $2"
}

function clean {
    rm -f out/* 2>/dev/null
}

function fatal {
    echo -n "ERROR: "
    echo "$1"
    if [ -n "$2" ]; then
        cat $2
    fi
    [ "$EXIT_ON_ERROR" = "1" ] && exit 2
    true
}

function compile {
    $CC -S -I$(dirname $1)/include -I../include -o $DEBUG_ASM $1 2>$DEBUG_LOG \
    && ( $GCC -o $DEBUG_BIN $DEBUG_ASM print.c || fatal " cannot build test program in $CC" )
}

function check_diff {
    diff -B $1 $2 || fatal "result not matched"
}

function check_result {
    if [ ! -f $1 ]; then
      fatal "no result file"
    fi

    local result_file=out/result.txt
    exec_arm_test $DEBUG_BIN $result_file

    check_diff $1 $result_file
}

function check_error {
    if [ ! -f "$1" ]; then
      fatal "unexpected compiler error" $DEBUG_LOG
    fi

    local err_file=out/error.txt
    grep 'ERROR' $DEBUG_LOG | sed 's/\x1b\[[0-9;]*m//g' > $err_file
    check_diff $1 $err_file
}

function run {
    local t=$1
    echo -n "test $t: "
    clean
    compile $t/test.c && check_result $t/expect.txt || check_error $t/expect-error.txt
    echo "OK"
}

set -e

CC=../bin/rcc_arm64
GCC=exec_arm_gcc
DEBUG_LOG=out/debug.log
DEBUG_BIN=out/t
DEBUG_OBJ=out/test.o
DEBUG_ASM=out/test.s

EXIT_ON_ERROR=0

function all {
    for t in 0*; do
        run $t
    done
}

if [ "$1" = "--gen2" ]; then
  CC=../bin/rcc2_arm64
  shift
fi

if [ "$1" = "--gen3" ]; then
  CC=../bin/rcc3_arm64
  shift
fi

if [ "$1" = "--gcc" ]; then
  CC=$GCC
  shift
fi

if [ "$1" = "--exit-on-error" ]; then
  EXIT_ON_ERROR=1
  shift
fi

if [ -z "$1" ]; then
    all
else
    for t in ${1}*; do
        run $t
    done
fi
