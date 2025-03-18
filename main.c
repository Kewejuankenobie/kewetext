#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>

#define CTRL_KEY(k) ((k) & 0x1f)
#define KEWETEXT_VERSION "0.0.1"

enum editorKey {
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN
};

//DATA

//Stores original terminal settings
struct editorConfig {
    int cursorx;
    int cursory;
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

int editorReadKey() {
    //Reads keys from the terminal
    int readnum;
    char c;
    while ((readnum = read(STDIN_FILENO, &c, 1)) != 1) {
        if (readnum == -1 && errno != EAGAIN) {
            die("read");
        }
    }

    if (c == '\x1b') {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) {
            return '\x1b';
        }
        if (read(STDIN_FILENO, &seq[1], 1) != 1) {
            return '\x1b';
        }
        if (seq[0] == '[') {
            switch (seq[1]) {
                case 'A': return ARROW_UP;
                case 'B': return ARROW_DOWN;
                case 'C': return ARROW_RIGHT;
                case 'D': return ARROW_LEFT;
            }
        }
        return '\x1b';
    } else {
        return c;
    }
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

//APPEND BUFFER
struct appendbuf {
    char* buffer;
    int length;
};

#define APPENDBUF_INIT { NULL, 0 }

void appendBufAppend(struct appendbuf* b, const char* str, int length) {
    char* newBuffer = realloc(b->buffer, b->length + length);
    if (newBuffer == NULL) {
        return;
    }

    memcpy(&newBuffer[b->length], str, length);
    b->buffer = newBuffer;
    b->length += length;
}

void appendBufFree(struct appendbuf* b) {
    free(b->buffer);
}

//OUTPUT

void drawRows(struct appendbuf* abuf) {
    int i;
    for (i = 0; i < E.screen_rows; ++i) {
        if (i == E.screen_rows / 3) {
            char welcome[80];
            int welcomLength = snprintf(welcome, sizeof(welcome),
                "Kewetext Editor -- version %s", KEWETEXT_VERSION);
            if (welcomLength > E.screen_cols) {
                welcomLength = E.screen_cols;
            }
            int padding = (E.screen_cols - welcomLength) / 2;
            if (padding) {
                appendBufAppend(abuf, "-)", 2);
                padding -= 2;
            }
            while (padding--) {
                appendBufAppend(abuf, " ", 1);
            }

            appendBufAppend(abuf, welcome, welcomLength);
        } else {
            appendBufAppend(abuf, "-)", 2);
        }
        appendBufAppend(abuf, "\x1b[K", 3);

        if (i < E.screen_rows - 1) {
            appendBufAppend(abuf, "\r\n", 2);
        }
    }
}

void refreshScreen() {
    //Remember the buff is seen in formating terminal text
    struct appendbuf abuf = APPENDBUF_INIT;

    appendBufAppend(&abuf, "\x1b[?25l", 6);
    appendBufAppend(&abuf, "\x1b[H", 3);

    drawRows(&abuf);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cursory + 1, E.cursorx + 1);
    appendBufAppend(&abuf, buf, strlen(buf));

    appendBufAppend(&abuf, "\x1b[?25h", 6);

    write(STDOUT_FILENO, abuf.buffer, abuf.length);
    appendBufFree(&abuf);
}

//INPUT
void moveCursor(int key) {
    switch (key) {
        case ARROW_UP:
            if (E.cursory != 0) {
                E.cursory--;
            }
            break;
        case ARROW_LEFT:
            if (E.cursorx != 0) {
                E.cursorx--;
            }
            break;
        case ARROW_DOWN:
            if (E.cursory != E.screen_rows - 1) {
                E.cursory++;
            }
            break;
        case ARROW_RIGHT:
            if (E.cursorx != E.screen_cols - 1) {
                E.cursorx++;
            }
            break;
    }
}


void processKeyPress() {
    //Reads and performs functions for special control keys
    int c = editorReadKey();

    switch (c) {
        case CTRL_KEY('Q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(EXIT_SUCCESS);
            break;
        case ARROW_UP:
        case ARROW_LEFT:
        case ARROW_DOWN:
        case ARROW_RIGHT:
            moveCursor(c);
            break;
    }
}

//MAIN CODE

void startEditor() {

    E.cursorx = 0;
    E.cursory = 0;

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