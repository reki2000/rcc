#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

extern int errno;

int _read(int fd, void *buf, int len) {
    return read(fd, buf, len);
}

int _write(int fd, void *buf, int len) {
    return write(fd, buf, len);
}

void __exit(int code) {
    exit(code);
}

void *_calloc(int size, int count) {
    return calloc(size, count);
}

void *_malloc(int size) {
    return malloc(size);
}
