#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <errno.h>

#define CTRL_KEY(k) ((k) & 0x1f)

//DATA

//Stores original terminal settings
struct editorConfig {
    int screen_rows;
    int screen_cols;
    struct termios orig_termios;
};

struct editorConfig E;

//TERMINAL

void die(const char* error) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(error);
    exit(EXIT_FAILURE);
}

void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) {
        die("tcsetattr");
    }
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) {
        die("tcgetattr");
    }
    atexit(disableRawMode);

    struct termios raw = E.orig_termios;
    //Input control
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    //Output control
    raw.c_oflag &= ~(OPOST);
    //Bitmask
    raw.c_cflag |= (CS8);
    //Terminal functions
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        die("tcsetattr");
    }
}

char editorReadKey() {
    //Reads keys from the terminal
    int readnum;
    char c;
    while ((readnum = read(STDIN_FILENO, &c, 1)) != 1) {
        if (readnum == -1 && errno != EAGAIN) {
            die("read");
        }
    }
    return c;
}

int getCursorPosition(int* rows, int* cols) {
    //Gets the cursor position in the terminal setting the rows and cols
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) {
        return -1;
    }

    //Read in info about rows and cols into buf untill character 'R' ([28;100R format)
    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) {
            break;
        }
        if (buf[i] == 'R') {
            break;
        }
        ++i;
    }
    buf[i] = '\0';
    //Parse response into rows and cols
    if (buf[0] != '\x1b' || buf[1] != '[') {
        return -1;
    }
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) {
        return -1;
    }

    return 0;
}

int getWindowSize(int* rows, int* cols) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        //Fallback method to get the window size, start by moving cursor to bottom right
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) {
            return -1;
        }
        return getCursorPosition(rows, cols);
    } else {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
        return 0;
    }
}

//OUTPUT

void drawRows() {
    int i;
    for (i = 0; i < E.screen_rows; ++i) {
        write(STDOUT_FILENO, "-)", 2);

        if (i < E.screen_rows - 1) {
            write(STDOUT_FILENO, "\r\n", 2);
        }
    }
}

void refreshScreen() {
    //Remember the buff is seen in formating terminal text
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    drawRows();
    write(STDOUT_FILENO, "\x1b[H", 3);
}

//INPUT
void processKeyPress() {
    //Reads and performs functions for special control keys
    char c = editorReadKey();

    switch (c) {
        case CTRL_KEY('Q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(EXIT_SUCCESS);
            break;
    }
}

//MAIN CODE

void startEditor() {
    if (getWindowSize(&E.screen_rows, &E.screen_cols) == -1) {
        die("getWindowSize");
    }
}

int main(void) {

    enableRawMode();
    startEditor();

    while (1) {
        refreshScreen();
        processKeyPress();
    }

    return EXIT_SUCCESS;
}