#define Z 1

#ifdef Y
#define X "Y"
#else

#ifndef Z
#define X "!Y and !Z"
#else
#define X "!Y and Z"
#endif // ifndef Z

#endif // ifdef Y

extern int puts(const char *);
int main() {
    puts(X);
    return 0;
}
