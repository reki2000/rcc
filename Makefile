#

CC = gcc
CFLAGS = -g -Wall -Wextra

.PHONY: test

test: bin/main
	test/test.sh

all: bin/main

bin/main: out/main.o
	$(CC) -o bin/main out/main.o

out/main.o: src/main.c
	$(CC) $(CFLAGS) -c -o out/main.o src/main.c

clean:
	$(RM) bin/main out/*.o
