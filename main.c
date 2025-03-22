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

enum editorHighlight {
    HL_NORMAL = 0,
    HL_NUMBER,
    HL_STRING,
    HL_COMMENT,
    HL_MULTILINE_COMMENT,
    HL_KEYWORD1,
    HL_KEYWORD2,
    HL_MATCH
};

#define HL_HIGHLIGHT_NUMBERS (1<<0)
#define HL_HIGHLIGHT_STRINGS (1<<1)

//DATA
struct editorSyntax {
    char* filetype;
    char** filematch;
    char** keywords;
    char* singleLineCommentStart;
    char* multiLineCommentStart;
    char* multiLineCommentEnd;
    int flags;
};


//Structure of a row of text
typedef struct erow {
    int index;
    int size;
    int rsize;
    char* chars;
    char* render;
    unsigned char* highlight;
    int hl_open_comment;
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
    struct editorSyntax* syntax;
    struct termios orig_termios;
};

struct editorConfig E;

//FILETYPES
char* C_HL_extensions[] = {".c", ".h", ".cpp", NULL};
char* C_HL_keywords[] = {
    "switch", "if", "else", "for", "while", "break", "continue", "return",
    "struct", "union", "typedef", "static", "enum", "class", "case",
    "public", "public:", "protected", "protected:", "private", "private:", "do",
    "template", "typename", "try", "catch",

    "int|", "long|", "short|", "float|", "double|", "char|",
    "unsigned|", "signed|", "void|", NULL
};

struct editorSyntax HLDB[] = {
    {
        "c",
        C_HL_extensions,
        C_HL_keywords,
        "//", "/*", "*/",
        HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
    },
};

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

//PROTOTYPES
void setStatusMessage(const char* message, ...);
void refreshScreen();
char* editorPrompt(char* prompt, void (*callback) (char*, int));

//TERMINAL

void die(const char* error) {
    //Exits program on error and displays an error message
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(error);
    exit(EXIT_FAILURE);
}

void disableRawMode() {
    //Disables terminals raw edit mode and returns to it's original settings
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) {
        die("tcsetattr");
    }
}

void enableRawMode() {
    //Enables the terminals raw mode
    //Used to process a keypress on each press plus not display in the console
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
        //Handles the reading of escape characters like the arrow keys and esc
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
        //All non escape characters
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
    //Gets the number of rows and columns in the terminal window of any size
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

//SYNTAX HIGHLIGHTING

int isSeperator(int c) {
    //Determines if a character is a non-highlightable character
    return isspace(c) || c == '\0' || strchr(",.()+-/*-~%<>[];", c) != NULL;
}

void editorUpdateSyntax(erow* row) {
    //Updates the highlighted syntax of the editor based on the editors syntax
    row->highlight = realloc(row->highlight, row->rsize);
    memset(row->highlight, HL_NORMAL, row->rsize);

    if (E.syntax == NULL) {
        return;
    }
    
    //Aliases keywords and comments syntax being used
    char** keywords = E.syntax->keywords;

    char* scs = E.syntax->singleLineCommentStart;
    char* mcs = E.syntax->multiLineCommentStart;
    char* mce = E.syntax->multiLineCommentEnd;

    int scsLength = scs ? strlen(scs) : 0;
    int mcsLength = mcs ? strlen(mcs) : 0;
    int mceLength = mce ? strlen(mce) : 0;

    int prevSep = 1;
    int inString = 0;
    int inComment = (row->index > 0 && E.row[row->index - 1].hl_open_comment);

    int i = 0;
    while (i < row->rsize) {
        char c = row->render[i];
        unsigned char prevHl = (i > 0) ? row->highlight[i - 1] : HL_NORMAL;

        //Single Line Comments
        if (scsLength && !inString && !inComment) {
            if (!strncmp(&row->render[i], scs, scsLength)) {
                memset(&row->highlight[i], HL_COMMENT, row->rsize - i);
                break;
            }
        }

        //Multiline Comments
        if (mcsLength && mceLength && !inString) {
            if (inComment) {
                row->highlight[i] = HL_MULTILINE_COMMENT;
                if (!strncmp(&row->render[i], mce, mceLength)) {
                    memset(&row->highlight[i], HL_MULTILINE_COMMENT, mceLength);
                    i += mceLength;
                    inComment = 0;
                    prevSep = 1;
                    continue;
                } else {
                    i++;
                    continue;
                }
            } else if (!strncmp(&row->render[i], mcs, mcsLength)) {
                memset(&row->highlight[i], HL_MULTILINE_COMMENT, mcsLength);
                i += mcsLength;
                inComment = 1;
                continue;
            }
        }

        //String Syntax Checking
        if (E.syntax->flags & HL_HIGHLIGHT_STRINGS) {
            if (inString) {
                row->highlight[i] = HL_STRING;
                if (c == '\\' && i + 1 < row->rsize) {
                    row->highlight[i + 1] = HL_STRING;
                    i += 2;
                    continue;
                }
                if (c == inString) {
                    inString = 0;
                }
                ++i;
                prevSep = 1;
                continue;
            } else {
                if (c == '"' || c == '\'') {
                    inString = c;
                    row->highlight[i] = HL_STRING;
                    ++i;
                    continue;
                }
            }
        }

        //Number Syntax Checking
        if (E.syntax->flags & HL_HIGHLIGHT_NUMBERS) {
            if ((isdigit(c) && (prevSep || prevHl == HL_NUMBER)) ||
                (c == '.' && prevHl == HL_NUMBER)) {
                row->highlight[i] = HL_NUMBER;
                ++i;
                prevSep = 0;
                continue;
            }
        }

        //Keywords
        if (prevSep) {
            int j;
            for (j = 0; keywords[j]; j++) {
                int keylen = strlen(keywords[j]);
                int iskw2 = keywords[j][keylen - 1] == '|';
                if (iskw2) {
                    keylen--;
                }
                if (!strncmp(&row->render[i], keywords[j], keylen) &&
                    isSeperator(row->render[i + keylen])) {
                    memset(&row->highlight[i], iskw2 ? HL_KEYWORD2 : HL_KEYWORD1, keylen);
                    i += keylen;
                    break;
                }
            }
            if (keywords[j] != NULL) {
                prevSep = 0;
                continue;
            }
        }

        prevSep = isSeperator(c);
        ++i;
    }
    //Update hl_open_comment
    int changed = (row->hl_open_comment != inComment);
    row->hl_open_comment = inComment;
    if (changed && row->index + 1 < E.num_rows) {
        editorUpdateSyntax(&E.row[row->index + 1]);
    }
}

int syntaxToColor(int hl) {
    //Returns the syntax color for terminal escape sequences
    switch (hl) {
        case HL_NUMBER: return 31;
        case HL_KEYWORD1: return 35;
        case HL_KEYWORD2: return 32;
        case HL_STRING: return 33;
        case HL_COMMENT:
        case HL_MULTILINE_COMMENT: return 36;
        case HL_MATCH: return 34;
        default: return 37;
    }
}

void editorSelectSyntaxHighlight() {
    //Given the file, selects what format of syntax highlighting to use
    E.syntax = NULL;
    if (E.filename == NULL) {
        return;
    }

    char* ext = strrchr(E.filename, '.');
    unsigned int j;
    for (j = 0; j < HLDB_ENTRIES; ++j) {
        struct editorSyntax* s = &HLDB[j];
        unsigned int i = 0;
        while (s->filematch[i]) {
            int isExt = (s->filematch[i][0] == '.');
            if ((isExt && ext && !strcmp(ext, s->filematch[i])) ||
                (!isExt && strstr(E.filename, s->filematch[i]))) {
                E.syntax = s;

                int filerow;
                for (filerow = 0; filerow < E.num_rows; ++filerow) {
                    editorUpdateSyntax(&E.row[filerow]);
                }
                return;
            }
            ++i;
        }
    }
}

//ROW OPS

int rowCursorXToRenderX(erow* row, int cursorx) {
    //Converts cursor x to render x
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

int rowRenderXToCursorX(erow* row, int renderx) {
    //Converts render x to cursor x
    int cur_renderx = 0;
    int k;
    for (k = 0; k < renderx; k++) {
        if (row->chars[k] == '\t') {
            cur_renderx += (TAB_STOP - 1) - (cur_renderx % TAB_STOP);
        }
        cur_renderx++;
        if (cur_renderx > renderx) {
            return k;
        }
    }
    return k;
}

void editorUpdateRow(erow* row) {
    
    //Updates a row of text
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
        //Adds characters to render index and converts tabs to a number of spaces to add
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

    editorUpdateSyntax(row);
}

void editorInsertRow(int rowAt, char* text, size_t len) {

    //Inserts a row at a position and updates other row indecies
    if (rowAt < 0 || rowAt > E.num_rows) {
        return;
    }

    E.row = realloc(E.row, sizeof(erow) * (E.num_rows + 1));
    memmove(&E.row[rowAt + 1], &E.row[rowAt], sizeof(erow) * (E.num_rows - rowAt));

    for (int j = rowAt + 1; j < E.num_rows; j++) {
        E.row[j].index++;
    }

    E.row[rowAt].index = rowAt;

    E.row[rowAt].size = len;
    E.row[rowAt].chars = malloc(len + 1);
    memcpy(E.row[rowAt].chars, text, len);
    E.row[rowAt].chars[len] = '\0';

    E.row[rowAt].rsize = 0;
    E.row[rowAt].render = NULL;
    E.row[rowAt].highlight = NULL;
    E.row[rowAt].hl_open_comment = 0;
    editorUpdateRow(&E.row[rowAt]);

    ++(E.num_rows);
    ++(E.dirty);
}

void editorFreeRow(erow* row) {
    //Frees heap memory used by a row
    free(row->render);
    free(row->chars);
    free(row->highlight);
}

void editorDeleteRow(int pos) {
    //Removes a row and updates other row indecies
    if (pos < 0 || pos >= E.num_rows) {
        return;
    }
    editorFreeRow(&E.row[pos]);
    memmove(&E.row[pos], &E.row[pos + 1], sizeof(erow) * (E.num_rows - pos - 1));
    for (int j = pos; j < E.num_rows; j++) {
        E.row[j].index--;
    }
    E.num_rows--;
    E.dirty++;
}

void rowInsertChar(erow* row, int pos, int charToInsert) {
    //Inserts a character at a position in a row
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
    //Adds a string of characters to the end of a row
    row->chars = realloc(row->chars, row->size + length + 1);
    memcpy(&row->chars[row->size], text, length);
    row->size += length;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
    E.dirty++;
}

void rowDeleteChar(erow* row, int pos) {
    //Deletes a character at a position in a row
    if (pos < 0 || pos >= row->size) {
        return;
    }
    memmove(&row->chars[pos], &row->chars[pos + 1], row->size - pos);
    row->size--;
    editorUpdateRow(row);
    E.dirty++;
}

//EDITOR OPERATIONS

void editorInsertChar(int c) {
    //Inserts a character in the editor and increments cursor
    if (E.cursory == E.num_rows) {
        editorInsertRow(E.num_rows, "", 0);
    }
    rowInsertChar(&E.row[E.cursory], E.cursorx, c);
    E.cursorx++;
}

void editorInsertNewLine() {
    //Inserts a new line in the editor based on cursor position
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
    //Deletes a character in the editor at the cursor position
    if (E.cursory == E.num_rows) {
        return;
    }
    if (E.cursorx == 0 && E.cursory == 0) {
        return;
    }
    erow* row = &E.row[E.cursory];
    if (E.cursorx > 0) {
        rowDeleteChar(row, E.cursorx - 1);
        E.cursorx--;
    } else {
        E.cursorx = E.row[E.cursory - 1].size;
        rowAppendString(&E.row[E.cursory - 1], row->chars, row->size);
        editorDeleteRow(E.cursory);
        E.cursory--;
    }
}

//FIND

void editorFindCallback(char* query, int key) {
    //Finds the query in the file, uses key to navigate all occurances (Case sensitive)
    static int last_match = -1;
    static int direction = 1;

    static int saved_hl_line;
    static char* saved_hl = NULL;

    if (saved_hl) {
        memcpy(E.row[saved_hl_line].highlight, saved_hl, E.row[saved_hl_line].rsize);
        free(saved_hl);
        saved_hl = NULL;
    }

    //Determines direction to move from key press
    if (key == '\r' || key == '\x1b') {
        last_match = -1;
        direction = 1;
        return;
    } else if (key == ARROW_RIGHT || key == ARROW_DOWN) {
        direction = 1;
    } else if (key == ARROW_LEFT || key == ARROW_UP) {
        direction = -1;
    } else {
        last_match = -1;
        direction = 1;
    }

    if (last_match == -1) {
        direction = 1;
    }
    int current = last_match;

    int r;
    for (r = 0; r < E.num_rows; ++r) {
        //Based on current, find next occuring query, Move current to start or end if necissary
        current += direction;
        if (current == -1) {
            current = E.num_rows - 1;
        } else if (current == E.num_rows) {
            current = 0;
        }
        erow* row = &E.row[current];
        char* match = strstr(row->render, query);
        if (match) {
            last_match = current;
            E.cursory = current;
            E.cursorx = rowRenderXToCursorX(row, match - row->render);
            E.rowoff = E.num_rows;

            saved_hl_line = current;
            saved_hl = malloc(row->rsize);
            memcpy(saved_hl, row->highlight, row->rsize);
            memset(&row->highlight[match - row->render], HL_MATCH, strlen(query));
            break;
        }
    }
}

void editorFind() {
    //Enteres mode to find strings in a file
    int save_cursorx = E.cursorx;
    int save_cursory = E.cursory;
    int save_rowoff = E.rowoff;
    int save_coloff = E.coloff;

    char* query = editorPrompt("Search: %s (Left/Right Arrows to Navigate, ESC to quit)", editorFindCallback);

    if (query) {
        free(query);
    } else {
        E.cursorx = save_cursorx;
        E.cursory = save_cursory;
        E.rowoff = save_rowoff;
        E.coloff = save_coloff;
    }
}

//FILE IO

char* editorRowsToString(int* buflen) {
    //Convertes all rows text to a string
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
    //Opens a file in the editor if argument added
    free(E.filename);
    E.filename = strdup(filename);

    editorSelectSyntaxHighlight();

    FILE* file = fopen(filename, "r");
    if (!file) {
        die("fopen");
    }
    char* line = NULL;
    size_t lineCap = 0;
    ssize_t lineLen;
    while ((lineLen = getline(&line, &lineCap, file)) != -1) {
        //Adds all lines as text for a rows
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
    //Saves the current file or as a new file
    if (E.filename == NULL || newFile) {
        E.filename = editorPrompt("Save file as: %s", NULL);
        if (E.filename == NULL) {
            setStatusMessage("Stopped Save");
            return;
        }
        editorSelectSyntaxHighlight();
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
    //Buffer of chars to add to a row
    char* buffer;
    int length;
};

#define APPENDBUF_INIT { NULL, 0 }

void appendBufAppend(struct appendbuf* b, const char* str, int length) {
    //Adds chars to the append buffer
    char* newBuffer = realloc(b->buffer, b->length + length);
    if (newBuffer == NULL) {
        return;
    }

    memcpy(&newBuffer[b->length], str, length);
    b->buffer = newBuffer;
    b->length += length;
}

void appendBufFree(struct appendbuf* b) {
    //Frees heap memory used in the append buffer
    free(b->buffer);
}

//OUTPUT

void scroll() {
    //Handles scrolling based on the cursor position
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
    //Draws all rows in the editor
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
            
            char* c = &E.row[fileRow].render[E.coloff];
            unsigned char* hl = &E.row[fileRow].highlight[E.coloff];
            int currentColor = -1;
            int j;
            for (j = 0; j < rowLen; ++j) {
                if (iscntrl(c[j])) {
                    //Adds special control characters to the screen
                    char sym = (c[j] <= 26) ? '@' + c[j] : '?';
                    appendBufAppend(abuf, "\x1b[7m", 4);
                    appendBufAppend(abuf, &sym, 1);
                    appendBufAppend(abuf, "\x1b[m", 3);
                    if (currentColor != -1) {
                        char buf[16];
                        int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", currentColor);
                        appendBufAppend(abuf, buf, clen);
                    }
                } else if (hl[j] == HL_NORMAL) {
                    //Adds normal text
                    if (currentColor != -1) {
                        appendBufAppend(abuf, "\x1b[39m", 5);
                        currentColor = -1;
                    }
                    appendBufAppend(abuf, &c[j], 1);
                } else {
                    //Adds colored text based on the highlight array
                    int color = syntaxToColor(hl[j]);
                    if (color != currentColor) {
                        currentColor = color;
                        char buf[16];
                        int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
                        appendBufAppend(abuf, buf, clen);
                    }
                    appendBufAppend(abuf, &c[j], 1);
                }
            }
            appendBufAppend(abuf, "\x1b[39m", 5);
        }
        appendBufAppend(abuf, "\x1b[K", 3);
        appendBufAppend(abuf, "\r\n", 2);
    }
}

void drawStatusBar(struct appendbuf* abuf) {
    //Draws the status bar
    appendBufAppend(abuf, "\x1b[7m", 4);
    char status[80], rightstatus[80];
    int length = snprintf(status, sizeof(status)," %.20s - %d lines %s",
        E.filename ? E.filename : "[No Name]", E.num_rows,
        E.dirty? "(Modified)" : "");
    int rightlength = snprintf(rightstatus, sizeof(rightstatus), "%s | Line: %d/%d ",
        E.syntax ? E.syntax->filetype : "no ft", E.cursory + 1, E.num_rows);
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
    //Draws the message containing prompts and help
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
    //Refreshes all drawn elements, cursor position, prints out append buffer 
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
    //Sets the editors status message
    va_list messageArgs;
    va_start(messageArgs, message);
    vsnprintf(E.status, sizeof(E.status), message, messageArgs);
    va_end(messageArgs);
    E.status_time = time(NULL);
}

//INPUT

char* editorPrompt(char* prompt, void (*callback) (char*, int)) {
    //Prompts the user for a response to a command
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
            if (callback) {
                callback(buffer, c);
            }
            free(buffer);
            return NULL;
        } else if (c == '\r') {
            if (bufferLength != 0) {
                setStatusMessage("");
                if (callback) {
                    callback(buffer, c);
                }
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
        if (callback) {
            callback(buffer, c);
        }

    }
}

void moveCursor(int key) {
    //Moves the cursor based on user input
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
    //Processes key presses and performs their functions
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

        case CTRL_KEY('F'):
            editorFind();
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
    //Starts the text editor and initializes all editor variables
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
    E.syntax = NULL;

    if (getWindowSize(&E.screen_rows, &E.screen_cols) == -1) {
        die("getWindowSize");
    }
    E.screen_rows -= 2;
}


int main(int argc, char *argv[]) {
    //kewetext main code starts, and loops through editor functions
    enableRawMode();
    startEditor();
    if (argc > 1) {
        editorOpen(argv[1]);
    }

    setStatusMessage("HELP: Ctrl-S: Save, Ctrl-N: Save New File, CTRL-F: Find, Ctrl-Q: Quit");

    while (1) {
        refreshScreen();
        processKeyPress();
    }

    return EXIT_SUCCESS;
}
