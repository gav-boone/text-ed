/* includes */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <conio.h>
#include <windows.h>
#include <wincon.h>
#include <string.h>
#include <sys/types.h>

/* defines */
#define CTRL_KEY(k) ((k) & 0x1f)
#define TEXT_ED_VERSION "0.0.1"
#define TEXT_ED_TAB_STOP 4
enum editorKey {
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL,
    HOME,
    END,
    PAGE_UP,
    PAGE_DOWN,

};

/* data */
typedef struct erow {
    int size;
    int rsize;
    char* chars;
    char* render;
} erow;

struct editorConfig {
    DWORD originalMode;
    HANDLE hStdin;
    HANDLE hStdout;
    int screenRows;
    int screenCols;
    int cx, cy;
    int rx;
    int numRows;
    int rowoff;
    int coloff;
    erow* row;
};

struct editorConfig E;

/* append buffer */
struct abuf {
    char* b;
    int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf* ab, const char* s, int len) {
    char* new = realloc(ab->b, ab->len + len);

    if (new == NULL)
        return;

    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void abFree(struct abuf* ab) {
    free(ab->b);
}

/* terminal funcs */
void die(const char* s) {
    DWORD written;
    abAppend(E.hStdout, "\x1b[2J", 4);
    WriteConsole(E.hStdout, "\x1b[H", 3, &written, NULL);
    fprintf(stderr, "%s: error %lu\n", s, GetLastError());
    exit(1);
}

void disableRawMode() {
    if (!SetConsoleMode(E.hStdin, E.originalMode))
        die("SetConsoleMode");
}

void enableRawMode() {
    E.hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if (!GetConsoleMode(E.hStdin, &E.originalMode))
        die("GetConsoleMode");

    DWORD raw = E.originalMode;
    raw &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);
    raw |= ENABLE_VIRTUAL_TERMINAL_INPUT;

    if (!SetConsoleMode(E.hStdin, raw))
        die("SetConsoleMode");

    E.hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD outMode;
    if (!GetConsoleMode(E.hStdout, &outMode))
        die("GetConsoleMode");
    if (!SetConsoleMode(E.hStdout, outMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
        die("SetConsoleMode");

    atexit(disableRawMode);
}

int handleEscSeq() {
    char seq[3];
    DWORD bytesRead;

    if (!ReadConsole(E.hStdin, &seq[0], 1, &bytesRead, NULL))
        return '\x1b';
    if (!ReadConsole(E.hStdin, &seq[1], 1, &bytesRead, NULL))
        return '\x1b';

    if (seq[0] == '[') {
        if (seq[1] >= '0' && seq[1] <= '9') {
            if (!ReadConsole(E.hStdin, &seq[2], 1, &bytesRead, NULL))
                return '\x1b';
            if (seq[2] == '~') {
                switch (seq[1]) {
                case '1':
                case '7':
                    return HOME;
                case '5':
                    return PAGE_UP;
                case '6':
                    return PAGE_DOWN;
                case '8':
                case '4':
                    return END;
                case '3':
                    return DEL;
                }
            }
            return '\x1b';
        }

        switch (seq[1]) {
        case 'A':
            return ARROW_UP;
        case 'B':
            return ARROW_DOWN;
        case 'C':
            return ARROW_RIGHT;
        case 'D':
            return ARROW_LEFT;
        case 'H':
            return HOME;
        case 'F':
            return END;
        }
        return '\x1b';
    }
    else if (seq[0] == 'O') {
        switch (seq[1]) {
        case 'H':
            return HOME;
        case 'F':
            return END;
        }
    }
    return '\x1b';
}

int editorReadKey() {
    char c;
    DWORD bytesRead;
    ReadConsole(E.hStdin, &c, 1, &bytesRead, NULL);

    if (c == '\x1b') {
        return handleEscSeq();
    }

    return c;
}

int getWindowSize(int* rows, int* cols) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(E.hStdout, &csbi))
        return -1;
    *cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    *rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    return 0;
}

/* row ops */
void editorUpdateRow(erow* row) {
    int tabs = 0;
    int j;
    for (j = 0; j < row->size; j++)
        if (row->chars[j] == '\t') tabs++;

    free(row->render);
    row->render = malloc(row->size + tabs * (TEXT_ED_TAB_STOP - 1) + 1);

    int idx = 0;
    for (j = 0; j < row->size; j++) {
        if (row->chars[j] == '\t') {
            row->render[idx++] = ' ';
            while (idx % TEXT_ED_TAB_STOP != 0) row->render[idx++] = ' ';
        }
        else {
            row->render[idx++] = row->chars[j];
        }
    }
    row->render[idx] = '\0';
    row->rsize = idx;
}

void editorAppendRow(char* s, size_t len) {
    E.row = realloc(E.row, sizeof(erow) * (E.numRows + 1));

    int at = E.numRows;
    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';

    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    editorUpdateRow(&E.row[at]);

    E.numRows++;
}

int editorCxtoRx(erow* row, int cx) {
    int rx = 0;
    int j;
    for (j = 0; j < cx; j++) {
        if (row->chars[j] == '\t')
            rx += (TEXT_ED_TAB_STOP - 1) - (rx % TEXT_ED_TAB_STOP);
        rx++;
    }
    return rx;
}

/* file io */
ssize_t getline(char** lineptr, size_t* n, FILE* stream) {
    if (!lineptr || !n || !stream) return -1;
    size_t pos = 0;
    int c;
    if (!*lineptr) {
        *n = 128;
        *lineptr = malloc(*n);
        if (!*lineptr) return -1;
    }
    while ((c = fgetc(stream)) != EOF) {
        if (pos + 1 >= *n) {
            *n *= 2;
            char* tmp = realloc(*lineptr, *n);
            if (!tmp) return -1;
            *lineptr = tmp;
        }
        (*lineptr)[pos++] = c;
        if (c == '\n') break;
    }
    if (pos == 0 && c == EOF) return -1;
    (*lineptr)[pos] = '\0';
    return pos;
}

void editorOpen(char* filename) {
    FILE* fp = fopen(filename, "r");
    if (!fp) die("fopen");

    char* line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while ((linelen = getline(&line, &linecap, fp)) != -1) {
        while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
            linelen--;

        editorAppendRow(line, linelen);
    }
    free(line);
    fclose(fp);
}

/* input */
void editorMoveCursor(int key) {
    erow* row = (E.cy >= E.numRows) ? NULL : &E.row[E.cy];

    switch (key) {
    case ARROW_UP:
        if (E.cy != 0)
            E.cy--;
        break;
    case ARROW_DOWN:
        if (E.cy < E.numRows)
            E.cy++;
        break;
    case ARROW_LEFT:
        if (E.cx != 0) {
            E.cx--;
        }
        else if (E.cy > 0) {
            E.cy--;
            E.cx = E.row[E.cy].size;
        }
        break;
    case ARROW_RIGHT:
        if (row && E.cx < row->size) {
            E.cx++;
        }
        else if (row && E.cx == row->size) {
            E.cy++;
            E.cx = 0;
        }
        break;
    }

    row = (E.cy >= E.numRows) ? NULL : &E.row[E.cy];
    int rowlen = row ? row->size : 0;
    if (E.cx > rowlen) {
        E.cx = rowlen;
    }
}

void editorProcessKeypress() {
    int c = editorReadKey();
    DWORD written;

    switch (c) {
    case CTRL_KEY('q'):
        WriteConsole(E.hStdout, "\x1b[2J", 4, &written, NULL);
        WriteConsole(E.hStdout, "\x1b[H", 3, &written, NULL);
        exit(0);
        break;

    case HOME:
        E.cx = 0;
        break;
    case END:
        E.cx = E.screenCols - 1;
        break;

    case PAGE_UP:
    case PAGE_DOWN: {
        int times = E.screenRows;
        while (times--) {
            editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
        }
        break;
    }

    case ARROW_UP:
    case ARROW_LEFT:
    case ARROW_DOWN:
    case ARROW_RIGHT:
        editorMoveCursor(c);
        break;
    }
}

/* ouptut */
void editorDrawRows(struct abuf* ab) {
    int y;
    for (y = 0; y < E.screenRows; y++) {
        int filerow = y + E.rowoff;
        if (filerow >= E.numRows) {
            if (E.numRows == 0 && y == E.screenRows / 3) {
                char welcome[80];
                int welcomeLen = snprintf(welcome, sizeof(welcome), "Welcome to Text Ed v%s", TEXT_ED_VERSION);
                if (welcomeLen > E.screenCols)
                    welcomeLen = E.screenCols;

                int padding = (E.screenCols - welcomeLen) / 2;
                if (padding) {
                    abAppend(ab, "~", 1);
                    padding--;
                }
                while (padding--)
                    abAppend(ab, " ", 1);

                abAppend(ab, welcome, welcomeLen);
            }
            else {
                abAppend(ab, "~", 1);
            }
        }
        else {
            int len = E.row[filerow].rsize - E.coloff;
            if (len < 0) len = 0;
            if (len > E.screenCols) len = E.screenCols;
            abAppend(ab, &E.row[filerow].render[E.coloff], len);
        }

        abAppend(ab, "\x1b[K", 3);
        if (y < E.screenRows - 1)
            abAppend(ab, "\r\n", 2);
    }
}

void editorScroll() {
    E.rx = 0;
    if (E.cy < E.numRows) {
        E.rx = editorCxtoRx(&E.row[E.cy], E.cx);
    }

    if (E.cy < E.rowoff) {
        E.rowoff = E.cy;
    }
    if (E.cy >= E.rowoff + E.screenRows) {
        E.rowoff = E.cy - E.screenRows + 1;
    }
    if (E.rx < E.coloff) {
        E.coloff = E.rx;
    }
    if (E.rx >= E.coloff + E.screenCols) {
        E.coloff = E.rx = E.screenCols + 1;
    }
}

void editorRefreshScreen() {
    editorScroll();

    struct abuf ab = ABUF_INIT;
    DWORD written;

    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);

    getWindowSize(&E.screenRows, &E.screenCols);
    editorDrawRows(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, (E.rx - E.coloff) + 1);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);

    WriteConsole(E.hStdout, ab.b, ab.len, &written, NULL);
    abFree(&ab);
}

/* init */
void initEditor() {
    E.cx = 0;
    E.cy = 0;
    E.rx = 0;
    E.numRows = 0;
    E.row = NULL;
    E.rowoff = 0;
    E.coloff = 0;

    if (getWindowSize(&E.screenRows, &E.screenCols) == -1)
        die("getWindowSize");
}

/* main */
int main(int argc, char* argv[]) {
    enableRawMode();
    initEditor();
    if (argc >= 2) {
        editorOpen(argv[1]);
    }

    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }

    return 0;
}