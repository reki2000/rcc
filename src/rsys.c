#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int _read(int fd, void *buf, int len) {
    return read(fd, buf, len);
}

int _write(int fd, void *buf, int len) {
    return write(fd, buf, len);
}

void __exit(int code) {
    exit(code);
}