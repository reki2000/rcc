extern int isatty(int);

int main(int argc, char** argv) {
    if (isatty(2)) {
        return 0;
    }
    return argc;
}
