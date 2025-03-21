#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#define CTRL_KEY(k) ((k) & 0x1f)
#define KEWETEXT_VERSION "0.0.1"
#define TAB_STOP 8
#define QUIT_TIMES 2

enum editorKey {
    BACKSPACE = 127,
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DELETE,
    PAGE_UP,
    PAGE_DOWN,
    HOME,
    END
};

//DATA

//Structure of a row of text
typedef struct erow {
    int size;
    int rsize;
    char* chars;
    char* render;
} erow;

//Stores original terminal settings
struct editorConfig {
    int cursorx;
    int cursory;
    int renderx;
    int rowoff;
    int coloff;
    int screen_rows;
    int screen_cols;
    int num_rows;
    int dirty;
    erow* row;
    char* filename;
    char status[80];
    time_t status_time;
    struct termios orig_termios;
};

struct editorConfig E;

//PROTOTYPES
void setStatusMessage(const char* message, ...);
void refreshScreen();
char* editorPrompt(char* prompt);

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
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) {
                    return '\x1b';
                }
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return HOME;
                        case '3': return DELETE;
                        case '4': return END;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME;
                        case '8': return END;
                    }
                }
            } else {
                switch (seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME;
                    case 'F': return END;
                }
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'H': return HOME;
                case 'F': return END;
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

//ROW OPS

int rowCursorXToRenderX(erow* row, int cursorx) {
    int renderx = 0;
    int k;
    for (k = 0; k < cursorx; k++) {
        if (row->chars[k] == '\t') {
            renderx += (TAB_STOP - 1) - (renderx % TAB_STOP);
        }
        renderx++;
    }
    return renderx;
}

void editorUpdateRow(erow* row) {

    int tabs = 0;
    int j;
    for (j = 0; j < row->size; j++) {
        if (row->chars[j] == '\t') {
            ++tabs;
        }
    }

    free(row->render);
    row->render = malloc(row->size + tabs * (TAB_STOP - 1) + 1);

    int renderIndex = 0;
    for (j = 0; j < row->size; j++) {
        if (row->chars[j] == '\t') {
            row->render[renderIndex++] = ' ';
            while (renderIndex % TAB_STOP != 0) {
                row->render[renderIndex++] = ' ';
            }
        } else {
            row->render[renderIndex++] = row->chars[j];
        }
    }
    row->render[renderIndex] = '\0';
    row->rsize = renderIndex;

}

void editorInsertRow(int rowAt, char* text, size_t len) {

    if (rowAt < 0 || rowAt > E.num_rows) {
        return;
    }

    E.row = realloc(E.row, sizeof(erow) * (E.num_rows + 1));
    memmove(&E.row[rowAt + 1], &E.row[rowAt], sizeof(erow) * (E.num_rows - rowAt));

    E.row[rowAt].size = len;
    E.row[rowAt].chars = malloc(len + 1);
    memcpy(E.row[rowAt].chars, text, len);
    E.row[rowAt].chars[len] = '\0';

    E.row[rowAt].rsize = 0;
    E.row[rowAt].render = NULL;
    editorUpdateRow(&E.row[rowAt]);

    ++(E.num_rows);
    ++(E.dirty);
}

void editorFreeRow(erow* row) {
    free(row->render);
    free(row->chars);
}

void editorDeleteRow(int pos) {
    if (pos < 0 || pos > E.num_rows) {
        return;
    }
    editorFreeRow(&E.row[pos]);
    memmove(&E.row[pos], &E.row[pos + 1], sizeof(erow) * (E.num_rows - pos - 1));
    E.num_rows--;
    E.dirty++;
}

void rowInsertChar(erow* row, int pos, int charToInsert) {
    if (pos < 0 || pos > row->size) {
        pos = row->size;
    }
    row->chars = realloc(row->chars, row->size + 2);
    memmove(&row->chars[pos + 1], &row->chars[pos], row->size - pos + 1);
    row->size++;
    row->chars[pos] = charToInsert;
    editorUpdateRow(row);
    ++(E.dirty);
}

void rowAppendString(erow* row, char* text, size_t length) {
    row->chars = realloc(row->chars, row->size + length + 1);
    memcpy(&row->chars[row->size], text, length);
    row->size += length;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
    E.dirty++;
}

void rowDeleteChar(erow* row, int pos) {
    if (pos < 0 || pos > row->size) {
        return;
    }
    memmove(&row->chars[pos], &row->chars[pos + 1], row->size - pos);
    row->size--;
    editorUpdateRow(row);
    E.dirty++;
}

//EDITOR OPERATIONS

void editorInsertChar(int c) {
    if (E.cursory == E.num_rows) {
        editorInsertRow(E.num_rows, "", 0);
    }
    rowInsertChar(&E.row[E.cursory], E.cursorx, c);
    E.cursorx++;
}

void editorInsertNewLine() {
    if (E.cursorx == 0) {
        editorInsertRow(E.cursory, "", 0);
    } else {
        erow* row= &E.row[E.cursory];
        editorInsertRow(E.cursory + 1, &row->chars[E.cursorx], row->size - E.cursorx);
        row = &E.row[E.cursory];
        row->size = E.cursorx;
        row->chars[row->size] = '\0';
        editorUpdateRow(row);
    }
    E.cursory++;
    E.cursorx = 0;
}

void editorDeleteChar() {
    if (E.cursory == E.num_rows) {
        return;
    }
    if (E.cursorx == 0 && E.cursory == 0) {
        return;
    }
    erow* row = &E.row[E.cursory];
    if (E.cursorx > 0) {
        rowDeleteChar(row, E.cursorx);
        E.cursorx--;
    } else {
        E.cursorx = E.row[E.cursory - 1].size;
        rowAppendString(&E.row[E.cursory - 1], row->chars, row->size);
        editorDeleteRow(E.cursory);
        E.cursory--;
    }
}

//FILE IO

char* editorRowsToString(int* buflen) {
    int totlen = 0;
    int i;
    for (i = 0; i < E.num_rows; i++) {
        totlen += E.row[i].size + 1;
    }
    *buflen = totlen;

    char* combinedStr = malloc(totlen);
    char* rowItr = combinedStr;
    for (i = 0; i < E.num_rows; i++) {
        memcpy(rowItr, E.row[i].chars, E.row[i].size);
        rowItr += E.row[i].size;
        *rowItr = '\n';
        ++rowItr;
    }

    return combinedStr;
}

void editorOpen(char* filename) {

    free(E.filename);
    E.filename = strdup(filename);

    FILE* file = fopen(filename, "r");
    if (!file) {
        die("fopen");
    }
    char* line = NULL;
    size_t lineCap = 0;
    ssize_t lineLen;
    while ((lineLen = getline(&line, &lineCap, file)) != -1) {
        while (lineLen > 0 && (line[lineLen - 1] == '\n' ||
            line[lineLen - 1] == '\r' )) {
            lineLen--;
        }
        editorInsertRow(E.num_rows, line, lineLen);
    }

    free(line);
    fclose(file);
    E.dirty = 0;
}

void editorSaveFile(int newFile) {
    if (E.filename == NULL || newFile) {
        E.filename = editorPrompt("Save file as: %s");
        if (E.filename == NULL) {
            setStatusMessage("Stopped Save");
            return;
        }
    }

    int length;
    char* buffer = editorRowsToString(&length);

    int file = open(E.filename, O_RDWR | O_CREAT, 0644);
    if (file != -1) {
        if (ftruncate(file, length) != -1) {
            if (write(file, buffer, length) == length) {
                close(file);
                free(buffer);
                setStatusMessage("%d bytes written to disk", length);
                E.dirty = 0;
                return;
            }
        }
        close(file);
    }
    free(buffer);
    setStatusMessage("Can't save, IO error: %s", strerror(errno));
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

void scroll() {

    E.renderx = 0;
    if (E.cursory < E.num_rows) {
        E.renderx = rowCursorXToRenderX(&E.row[E.cursory], E.cursorx);
    }

    if (E.cursory < E.rowoff) {
        E.rowoff = E.cursory;
    }
    if (E.cursory >= E.rowoff + E.screen_rows) {
        E.rowoff = E.cursory - E.screen_rows + 1;
    }
    if (E.renderx < E.coloff) {
        E.coloff = E.renderx;
    }
    if (E.renderx >= E.coloff + E.screen_cols) {
        E.coloff = E.renderx - E.screen_cols + 1;
    }
}

void drawRows(struct appendbuf* abuf) {
    int i;
    for (i = 0; i < E.screen_rows; ++i) {
        int fileRow = E.rowoff + i;
        if (fileRow >= E.num_rows) {
            if (E.num_rows == 0 && i == E.screen_rows / 3) {
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
        } else {
            int rowLen = E.row[fileRow].rsize - E.coloff;
            if (rowLen < 0) {
                rowLen = 0;
            }
            if (rowLen > E.screen_cols) {
                rowLen = E.screen_cols;
            }
            appendBufAppend(abuf, &E.row[fileRow].render[E.coloff], rowLen);
        }
        appendBufAppend(abuf, "\x1b[K", 3);

        appendBufAppend(abuf, "\r\n", 2);
    }
}

void drawStatusBar(struct appendbuf* abuf) {
    appendBufAppend(abuf, "\x1b[7m", 4);
    char status[80], rightstatus[80];
    int length = snprintf(status, sizeof(status)," %.20s - %d lines %s",
        E.filename ? E.filename : "[No Name]", E.num_rows,
        E.dirty? "(Modified)" : "");
    int rightlength = snprintf(rightstatus, sizeof(rightstatus), "Line: %d/%d ",
        E.cursory + 1, E.num_rows);
    if (length > E.screen_cols) {
        length = E.screen_cols;
    }
    appendBufAppend(abuf, status, length);

    while (length < E.screen_cols) {
        if (E.screen_cols - length == rightlength) {
            appendBufAppend(abuf, rightstatus, rightlength);
            break;
        } else {
            appendBufAppend(abuf, " ", 1);
            ++length;
        }
    }
    appendBufAppend(abuf, "\x1b[m", 3);
    appendBufAppend(abuf, "\r\n", 2);
}

void drawMessage(struct appendbuf* abuf) {
    appendBufAppend(abuf, "\x1b[K", 3);
    int messageLength = strlen(E.status);
    if (messageLength > E.screen_cols) {
        messageLength = E.screen_cols;
    }
    if (messageLength && time(NULL) - E.status_time < 5) {
        appendBufAppend(abuf, E.status, messageLength);
    }
}

void refreshScreen() {

    scroll();

    //Remember the buff is seen in formating terminal text
    struct appendbuf abuf = APPENDBUF_INIT;

    appendBufAppend(&abuf, "\x1b[?25l", 6);
    appendBufAppend(&abuf, "\x1b[H", 3);

    drawRows(&abuf);
    drawStatusBar(&abuf);
    drawMessage(&abuf);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH",
        (E.cursory - E.rowoff) + 1, (E.renderx - E.coloff) + 1);
    appendBufAppend(&abuf, buf, strlen(buf));

    appendBufAppend(&abuf, "\x1b[?25h", 6);

    write(STDOUT_FILENO, abuf.buffer, abuf.length);
    appendBufFree(&abuf);
}

void setStatusMessage(const char* message, ...) {
    va_list messageArgs;
    va_start(messageArgs, message);
    vsnprintf(E.status, sizeof(E.status), message, messageArgs);
    va_end(messageArgs);
    E.status_time = time(NULL);
}

//INPUT

char* editorPrompt(char* prompt) {

    //Initial memory alloc for answer buffer
    size_t bufferSize = 128;
    char* buffer = malloc(bufferSize);
    size_t bufferLength = 0;
    buffer[0] = '\0';

    while (1) {
        setStatusMessage(prompt, buffer);
        refreshScreen();

        int c = editorReadKey();
        if (c == DELETE || c == BACKSPACE || c == CTRL('h')) {
            if (bufferLength != 0) {
                buffer[--bufferLength] = '\0';
            }
        } else if (c == '\x1b') {
            setStatusMessage("");
            free(buffer);
            return NULL;
        } else if (c == '\r') {
            if (bufferLength != 0) {
                setStatusMessage("");
                return buffer;
            }
        } else if (!iscntrl(c) && c < 128) {
            if (bufferLength == bufferSize - 1) {
                //When we are at the buffer size, double the space and realloc mem
                bufferSize *= 2;
                buffer = realloc(buffer, bufferSize);
            }
            //Set end of buffer to c, increment it, and set very end to null terminator
            buffer[bufferLength++] = c;
            buffer[bufferLength] = '\0';
        }

    }
}

void moveCursor(int key) {
    erow* row = (E.cursory >= E.num_rows) ? NULL : &E.row[E.cursory];


    switch (key) {
        case ARROW_UP:
            if (E.cursory != 0) {
                E.cursory--;
            }
            break;
        case ARROW_LEFT:
            if (E.cursorx != 0) {
                E.cursorx--;
            } else if (E.cursory > 0) {
                E.cursory--;
                E.cursorx = E.row[E.cursory].size;
            }
            break;
        case ARROW_DOWN:
            if (E.cursory < E.num_rows) {
                E.cursory++;
            }
            break;
        case ARROW_RIGHT:
            if (row && E.cursorx < row->size) {
                E.cursorx++;
            } else if (row && E.cursorx == row->size) {
                E.cursory++;
                E.cursorx = 0;
            }
            break;
    }

    row = (E.cursory >= E.num_rows) ? NULL : &E.row[E.cursory];
    int rowlen = row ? row->size : 0;
    if (E.cursorx > rowlen) {
        E.cursorx = rowlen;
    }
}


void processKeyPress() {
    //Reads and performs functions for special control keys
    static int quit_times = QUIT_TIMES;
    int c = editorReadKey();

    switch (c) {
        case '\r':
            editorInsertNewLine();
            break;

        case CTRL_KEY('Q'):
            if (E.dirty && quit_times > 0) {
                setStatusMessage("Unsaved Changes. Press Ctrl-Q %d more times to quit", quit_times);
                quit_times--;
                return;
            }
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(EXIT_SUCCESS);
            break;

        case CTRL_KEY('S'):
            editorSaveFile(0);
            break;

        case CTRL_KEY('N'):
            editorSaveFile(1);
            break;

        case HOME:
            E.cursorx = 0;
            break;
        case END:
            if (E.cursory < E.num_rows) {
                E.cursorx = E.row[E.cursory].size;
            }
            break;

        case BACKSPACE:
        case CTRL_KEY('h'):
        case DELETE:
            if (c == DELETE) {
                moveCursor(ARROW_RIGHT);
            }
            editorDeleteChar();
            break;

        case PAGE_UP:
        case PAGE_DOWN: {
            if (c == PAGE_UP) {
                E.cursory = E.rowoff;
            } else if (c == PAGE_DOWN) {
                E.cursory = E.rowoff + E.screen_rows - 1;
                if (E.cursory >= E.num_rows) {
                    E.cursory = E.num_rows;
                }
            }

            int times = E.screen_rows;
            while (times--) {
                moveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            }
            break;
        }

        case ARROW_UP:
        case ARROW_LEFT:
        case ARROW_DOWN:
        case ARROW_RIGHT:
            moveCursor(c);
            break;

        case CTRL_KEY('l'):
        case '\x1b':
            break;

        default:
            editorInsertChar(c);
            break;
    }
    quit_times = QUIT_TIMES;
}

//MAIN CODE

void startEditor() {

    E.cursorx = 0;
    E.cursory = 0;
    E.renderx = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.num_rows = 0;
    E.dirty = 0;
    E.row = NULL;
    E.filename = NULL;
    E.status[0] = '\0';
    E.status_time = 0;

    if (getWindowSize(&E.screen_rows, &E.screen_cols) == -1) {
        die("getWindowSize");
    }
    E.screen_rows -= 2;
}

int main(int argc, char *argv[]) {

    enableRawMode();
    startEditor();
    if (argc > 1) {
        editorOpen(argv[1]);
    }

    setStatusMessage("HELP: Ctrl-S: Save, Ctrl-N: Save New File, Ctrl-Q: Quit");

    while (1) {
        refreshScreen();
        processKeyPress();
    }

    return EXIT_SUCCESS;
}