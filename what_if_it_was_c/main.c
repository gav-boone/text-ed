/* includes */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <conio.h>
#include <windows.h>
#include <wincon.h>
#include <string.h>
#include <sys/types.h>
#include <stdarg.h>
#include <fcntl.h>
#include <time.h>

/* defines */
#define CTRL_KEY(k) ((k) & 0x1f)
#define TEXT_ED_VERSION "0.0.1"
#define TEXT_ED_TAB_STOP 4
#define TEXT_ED_QUIT_TIMES 1
enum editorKey {
    BACKSPACE = 127,
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
    char* filename;
    char statusmsg[80];
    time_t statusmsg_time;
    int dirty;
};

struct editorConfig E;

/* prototypes */
void editorSetStatusMessage(const char* fmt, ...);

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
    E.dirty++;
}

void editorFreeRow(erow* row) {
    free(row->render);
    free(row->chars);
}

void editorDelRow(int at) {
    if (at < 0 || at >= E.numRows) return;

    editorFreeRow(&E.row[at]);
    memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numRows - at - 1));
    E.numRows--;
    E.dirty++;
}

void editorRowDelChar(erow* row, int at) {
    if (at < 0 || at >= row->size) return;

    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;
    editorUpdateRow(row);
    E.dirty++;
}

void editorRowInsertChar(erow* row, int at, int c) {
    if (at < 0 || at > row->size)
        at = row->size;

    row->chars = realloc(row->chars, row->size + 2);
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    row->chars[at] = c;
    row->size++;
    editorUpdateRow(row);
    E.dirty++;
}

void editorRowAppendString(erow* row, char* s, size_t len) {
    row->chars = realloc(row->chars, row->size + len + 1);
    memcpy(&row->chars[row->size], s, len);
    row->size += len;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
    E.dirty++;
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

/* editor ops */
void editorInsertChar(int c) {
    if (E.cy == E.numRows)
        editorAppendRow("", 0);

    editorRowInsertChar(&E.row[E.cy], E.cx, c);
    E.cx++;
}

void editorDelChar() {
    if (E.cy == E.numRows) return;
    if (E.cx == 0 && E.cy == 0) return;

    erow* row = &E.row[E.cy];
    if (E.cx > 0) {
        editorRowDelChar(row, E.cx - 1);
        E.cx--;
    }
    else {
        E.cx = E.row[E.cy - 1].size;
        editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);
        editorDelRow(E.cy);
        E.cy--;
    }
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

char* editorRowsToString(int* buflen) {
    int totlen = 0;
    int j;
    for (j = 0; j < E.numRows; j++)
        totlen += E.row[j].size + 1;
    *buflen = totlen;

    char* buf = malloc(totlen);
    char* p = buf;
    for (j = 0; j < E.numRows; j++) {
        memcpy(p, E.row[j].chars, E.row[j].size);
        p += E.row[j].size;
        *p = '\n';
        p++;
    }

    return buf;
}

void editorSave() {
    if (!E.filename) return;

    int len;
    int len_written;
    char* buf = editorRowsToString(&len);

    FILE* fp = fopen(E.filename, "w");
    if (fp) {
        len_written = fwrite(buf, 1, len, fp);
        fclose(fp);
        editorSetStatusMessage("%d bytes written to disk", len_written);
        E.dirty = 0;
    }
    else {
        editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
    }

    free(buf);
}

void editorOpen(char* filename) {
    free(E.filename);
    E.filename = strdup(filename);

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
    E.dirty = 0;
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
            if (row && E.cx >= TEXT_ED_TAB_STOP && E.cx % TEXT_ED_TAB_STOP == 0) {
                int i;
                for (i = 1; i <= TEXT_ED_TAB_STOP; i++)
                    if (row->chars[E.cx - i] != ' ') break;
                if (i > TEXT_ED_TAB_STOP) { E.cx -= TEXT_ED_TAB_STOP; break; }
            }
            E.cx--;
        }
        else if (E.cy > 0) {
            E.cy--;
            E.cx = E.row[E.cy].size;
        }
        break;
    case ARROW_RIGHT:
        if (row && E.cx < row->size) {
            if (E.cx + TEXT_ED_TAB_STOP <= row->size && E.cx % TEXT_ED_TAB_STOP == 0) {
                int i;
                for (i = 0; i < TEXT_ED_TAB_STOP; i++)
                    if (row->chars[E.cx + i] != ' ') break;
                if (i == TEXT_ED_TAB_STOP) { E.cx += TEXT_ED_TAB_STOP; break; }
            }
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
    static int quit_times = TEXT_ED_QUIT_TIMES;

    int c = editorReadKey();
    DWORD written;

    switch (c) {
    case '\r':
        /* TODO */
        break;

    case CTRL_KEY('q'):
        if (E.dirty && quit_times > 0) {
            editorSetStatusMessage(
                "WARNING!!! File has unsaved changes. \nPress Ctrl+Q %d more times to quit.",
                quit_times
            );
            quit_times--;
            return;
        }
        WriteConsole(E.hStdout, "\x1b[2J", 4, &written, NULL);
        WriteConsole(E.hStdout, "\x1b[H", 3, &written, NULL);
        exit(0);
        break;

    case CTRL_KEY('s'):
        editorSave();
        break;

    case HOME:
        E.cx = 0;
        break;
    case END:
        if (E.cy < E.numRows)
            E.cx = E.row[E.cy].size;
        break;

    case BACKSPACE:
    case CTRL_KEY('h'):
    case DEL:
        if (c == DEL) editorMoveCursor(ARROW_RIGHT);
        editorDelChar();
        break;

    case PAGE_UP:
    case PAGE_DOWN: {
        if (c == PAGE_UP) {
            E.cy = E.rowoff;
        }
        else if (c == PAGE_DOWN) {
            E.cy = E.rowoff + E.screenRows - 1;
            if (E.cy > E.numRows) E.cy = E.numRows;
        }

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

    case CTRL_KEY('l'):
    case '\x1b':
        // intentionally ignore these keys
        break;

    default:
        editorInsertChar(c);
        break;
    }

    quit_times = TEXT_ED_QUIT_TIMES;
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

void editorDrawStatusBar(struct abuf* ab) {
    abAppend(ab, "\x1b[7m", 4);
    char status[80], rstatus[80];
    int len = snprintf(
        status,
        sizeof(status),
        "%.20s - %d lines %s",
        E.filename ? E.filename : "[No Name]",
        E.numRows,
        E.dirty ? "(unsaved changes)" : ""
    );
    int rlen = snprintf(
        rstatus,
        sizeof(rstatus),
        "%d/%d",
        E.cy + 1,
        E.numRows
    );
    if (len > E.screenCols) len = E.screenCols;
    abAppend(ab, status, len);
    while (len < E.screenCols) {
        if (E.screenCols - len == rlen) {
            abAppend(ab, rstatus, rlen);
            break;
        }
        else {
            abAppend(ab, " ", 1);
            len++;
        }
    }
    abAppend(ab, "\x1b[m", 3);
    abAppend(ab, "\r\n", 2);
}

void editorSetStatusMessage(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
    E.statusmsg_time = time(NULL);
}

void editorDrawMessageBar(struct abuf* ab) {
    abAppend(ab, "\x1b[K", 3);
    int msglen = strlen(E.statusmsg);
    if (msglen > E.screenCols) msglen = E.screenCols;
    if (msglen && time(NULL) - E.statusmsg_time < 5)
        abAppend(ab, E.statusmsg, msglen);
}

void editorRefreshScreen() {
    editorScroll();

    struct abuf ab = ABUF_INIT;
    DWORD written;

    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);

    getWindowSize(&E.screenRows, &E.screenCols);
    E.screenRows -= 2;
    editorDrawRows(&ab);
    editorDrawStatusBar(&ab);
    editorDrawMessageBar(&ab);

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
    E.filename = NULL;
    E.statusmsg[0] = '\0';
    E.statusmsg_time = 0;
    E.dirty = 0;

    if (getWindowSize(&E.screenRows, &E.screenCols) == -1)
        die("getWindowSize");

    E.screenRows -= 2;
}

/* main */
int main(int argc, char* argv[]) {
    enableRawMode();
    initEditor();
    if (argc >= 2) {
        editorOpen(argv[1]);
    }

    editorSetStatusMessage("HELP: Ctrl+S = save | Ctrl+Q = quit");

    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }

    return 0;
}
