#!/bin/bash

cd $(dirname $BASH_SOURCE)

function clean {
    rm -f out/* 2>/dev/null
}

function fatal {
    echo -n "ERROR: "
    echo "$1"
    if [ -n "$2" ]; then
        cat $2
    fi
    exit 1
}

function compile {
    $CC -S -I$(dirname $1)/include $1 2>$DEBUG_LOG > $DEBUG_ASM
}

function check_diff {
    diff -B $1 $2 || fatal "result not matched"
}

function check_result {
    if [ ! -f $1 ]; then
      fatal "no result file"
    fi

    $LD -fPIC -o $DEBUG_BIN $DEBUG_ASM \
        || fatal " cannot build test program"

    local result_file=out/result.txt
    $DEBUG_BIN > $result_file
    echo $? >> $result_file

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
    echo "test: $t ------------------"
    clean
    compile $t/test.c && check_result $t/expect.txt || check_error $t/expect-error.txt
    echo "success"
}

set -e

CC=../bin/rcc
LD=cc
DEBUG_LOG=out/debug.log
DEBUG_BIN=out/test.out
DEBUG_ASM=out/test.s

function all {
    for t in 0*; do
        run $t
    done
}

if [ "$1" = "--stage1" ]; then
  CC=../bin/rcc2
  shift
fi

if [ -z "$1" ]; then
    all
else
    for t in ${1}*; do
        run $t
    done
fi
