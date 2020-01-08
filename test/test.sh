#!/bin/bash

cd $(dirname $BASH_SOURCE)

../bin/main < test.c > test.s

gcc -o test.out test.s

./test.out

echo $?


