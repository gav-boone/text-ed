/*** includes ***/
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <conio.h>
#include <windows.h>
#include <wincon.h>
#include <string.h>

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)
#define TEXT_ED_VERSION "0.0.1"
enum editorKey
{
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    PAGE_UP,
    PAGE_DOWN,
};

/*** data ***/
struct editorConfig
{
    DWORD originalMode;
    HANDLE hStdin;
    HANDLE hStdout;
    int screenRows;
    int screenCols;
    int cx, cy;
};

struct editorConfig E;

/*** appedn buffer ***/
struct abuf
{
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len)
{
    char *new = realloc(ab->b, ab->len + len);

    if (new == NULL)
        return;

    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void abFree(struct abuf *ab)
{
    free(ab->b);
}

/*** terminal funcs ***/
void die(const char *s)
{
    DWORD written;
    abAppend(E.hStdout, "\x1b[2J", 4);
    WriteConsole(E.hStdout, "\x1b[H", 3, &written, NULL);
    fprintf(stderr, "%s: error %lu\n", s, GetLastError());
    exit(1);
}

void disableRawMode()
{
    if (!SetConsoleMode(E.hStdin, E.originalMode))
        die("SetConsoleMode");
}

void enableRawMode()
{
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

int handlePageKeys(char seq[3])
{
    DWORD bytesRead;
    if (!ReadConsole(E.hStdin, &seq[2], 1, &bytesRead, NULL))
        return '\x1b';
    if (seq[2] == '~')
    {
        switch (seq[1])
        {
        case '5':
            return PAGE_UP;
        case '6':
            return PAGE_DOWN;
        }
    }
    return '\x1b';
}

int handleArrowKeys(char seq[3])
{
    switch (seq[1])
    {
    case 'A':
        return ARROW_UP;
    case 'B':
        return ARROW_DOWN;
    case 'C':
        return ARROW_RIGHT;
    case 'D':
        return ARROW_LEFT;
    }
    return '\x1b';
}

int handleEscSeq()
{
    char seq[3];
    DWORD bytesRead;

    if (!ReadConsole(E.hStdin, &seq[0], 1, &bytesRead, NULL))
        return '\x1b';
    if (!ReadConsole(E.hStdin, &seq[1], 1, &bytesRead, NULL))
        return '\x1b';

    if (seq[0] == '[')
    {
        if (seq[1] >= '0' && seq[1] <= '9')
            handlePageKeys(seq);

        handleArrowKeys(seq);
    }
    return '\x1b';
}

int editorReadKey()
{
    char c;
    DWORD bytesRead;
    ReadConsole(E.hStdin, &c, 1, &bytesRead, NULL);

    if (c == '\x1b')
    {
        return handleEscSeq();
    }

    return c;
}

int getWindowSize(int *rows, int *cols)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(E.hStdout, &csbi))
        return -1;
    *cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    *rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    return 0;
}

/*** input ***/
void editorMoveCursor(int key)
{
    switch (key)
    {
    case ARROW_UP:
        if (E.cy != 0)
            E.cy--;
        break;
    case ARROW_DOWN:
        if (E.cy != E.screenRows - 1)
            E.cy++;
        break;
    case ARROW_LEFT:
        if (E.cx != 0)
            E.cx--;
        break;
    case ARROW_RIGHT:
        if (E.cx != E.screenCols - 1)
            E.cx++;
        break;
    }
}

void editorProcessKeypress()
{
    int c = editorReadKey();
    DWORD written;

    switch (c)
    {
    case CTRL_KEY('q'):
        WriteConsole(E.hStdout, "\x1b[2J", 4, &written, NULL);
        WriteConsole(E.hStdout, "\x1b[H", 3, &written, NULL);
        exit(0);
        break;
    case ARROW_UP:
    case ARROW_LEFT:
    case ARROW_DOWN:
    case ARROW_RIGHT:
        editorMoveCursor(c);
        break;
    }
}

/*** ouptut ***/
void editorDrawRows(struct abuf *ab)
{
    int y;
    for (y = 0; y < E.screenRows; y++)
    {
        if (y == E.screenRows / 3)
        {
            char welcome[80];
            int welcomeLen = snprintf(welcome, sizeof(welcome), "Welcome to Text Ed v%s", TEXT_ED_VERSION);
            if (welcomeLen > E.screenCols)
                welcomeLen = E.screenCols;

            int padding = (E.screenCols - welcomeLen) / 2;
            if (padding)
            {
                abAppend(ab, "~", 1);
                padding--;
            }
            while (padding--)
                abAppend(ab, " ", 1);

            abAppend(ab, welcome, welcomeLen);
        }
        else
        {
            abAppend(ab, "~", 1);
        }

        abAppend(ab, "\x1b[K", 3);
        if (y < E.screenRows - 1)
            abAppend(ab, "\r\n", 2);
    }
}

void editorRefreshScreen()
{
    struct abuf ab = ABUF_INIT;
    DWORD written;

    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);

    WriteConsole(E.hStdout, ab.b, ab.len, &written, NULL);
    abFree(&ab);
}

/*** init ***/
void initEditor()
{
    E.cx = 0;
    E.cy = 0;

    if (getWindowSize(&E.screenRows, &E.screenCols) == -1)
        die("getWindowSize");
}

/*** main ***/
int main()
{
    enableRawMode();
    initEditor();

    while (1)
    {
        editorRefreshScreen();
        editorProcessKeypress();
    }

    return 0;
}