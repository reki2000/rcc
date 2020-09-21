typedef enum {
    FALSE = 0,
    TRUE = 1
} bool;

#define RCC_BUF_SIZE 1024

extern char *strcat(char *, const char *);
extern long strlen(const char *);

extern int write(int, char *, int);
extern int isatty(int);

typedef enum {
    WARN,
    ERROR,
    DEBUG,
    INFO,
    NONE
} level_e;

char *color_red = "\e[31m";
char *color_green = "\e[32m";
char *color_yellow = "\e[33m";
char *color_blue = "\e[34m";
char *color_magenta = "\e[35m";
char *color_cyan = "\e[36m";
char *color_white = "\e[37m";

void _log(level_e level, char *message) {
    char *color_str[] = {color_red, color_red, "", color_yellow, ""};
    char *level_str[] = {"WARN ", "ERROR", "DEBUG", "INFO ", ""};

    char buf[RCC_BUF_SIZE] = {0};
    bool tty = FALSE;
    if (tty) { strcat(buf, color_str[level]); }
    strcat(buf, level_str[level]);
    strcat(buf, ": ");
    strcat(buf, message);
    strcat(buf, "\n");
    if (tty && color_str[level] != color_white) {
        strcat(buf, color_white);
    }
    write(1, buf, strlen(buf));
}

int main(int argc, char **argv) {
    _log(DEBUG, "test");
    return 0;
}
