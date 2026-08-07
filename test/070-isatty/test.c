extern int isatty(int);

int main(int argc, char** argv) {
    // isatty() on an invalid fd is always false, so this test does not
    // depend on whether the test runner is attached to a terminal
    if (isatty(-1)) {
        return argc;
    }
    return 0;
}
