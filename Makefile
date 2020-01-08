#

CC = gcc
CFLAGS = -g -Wall -Wextra

.PHONY: test

test: bin/main
	test/test.sh

all: bin/main

bin/main: out/main.o out/iolib.o
	$(CC) -o bin/main out/main.o out/iolib.o

out/main.o: src/main.c
	$(CC) $(CFLAGS) -c -o out/main.o src/main.c

out/iolib.o: src/iolib.c
	$(CC) $(CFLAGS) -c -o out/iolib.o src/iolib.c

clean:
	$(RM) bin/main out/*.o
