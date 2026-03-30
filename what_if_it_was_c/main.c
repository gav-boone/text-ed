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

/*** data ***/
struct editorConfig
{
    DWORD originalMode;
    HANDLE hStdin;
    HANDLE hStdout;
    int screenRows;
    int screenCols;
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
    raw &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_VIRTUAL_TERMINAL_INPUT);

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

char editorReadKey()
{
    char c;
    DWORD bytesRead;
    ReadConsole(E.hStdin, &c, 1, &bytesRead, NULL);
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
void editorProcessKeypress()
{
    char c = editorReadKey();
    DWORD written;

    switch (c)
    {
    case CTRL_KEY('q'):
        WriteConsole(E.hStdout, "\x1b[2J", 4, &written, NULL);
        WriteConsole(E.hStdout, "\x1b[H", 3, &written, NULL);
        exit(0);
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
            while (padding--) abAppend(ab, " ", 1);

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

    abAppend(&ab, "\x1b[H", 3);
    abAppend(&ab, "\x1b[?25h", 6);

    WriteConsole(E.hStdout, ab.b, ab.len, &written, NULL);
    abFree(&ab);
}

/*** init ***/
void initEditor()
{
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