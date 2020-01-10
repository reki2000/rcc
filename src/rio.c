#include <stdio.h>

int _read(int fd, void *buf, int len) {
    return read(fd, buf, len);
}

int _write(int fd, void *buf, int len) {
    return write(fd, buf, len);
}
