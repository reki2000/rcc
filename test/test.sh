#!/bin/bash

cd $(dirname $BASH_SOURCE)

../bin/main < test.c > out/test.s

gcc -o out/test.out out/test.s

out/test.out

echo $?


