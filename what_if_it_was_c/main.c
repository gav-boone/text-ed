/*** includes ***/
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <conio.h>
#include <windows.h>
#include <wincon.h>

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)

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

/*** terminal funcs ***/
void die(const char *s)
{
    DWORD written;
    WriteConsole(E.hStdout, "\x1b[2J", 4, &written, NULL);
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
void editorDrawRows()
{
    int y;
    DWORD written;
    for (y = 0; y < E.screenRows; y++)
    {
        WriteConsole(E.hStdout, "~", 3, &written, NULL);

        if (y < E.screenRows - 1)
            WriteConsole(E.hStdout, "\r\n", 2, &written, NULL);
    }
}

void editorRefreshScreen()
{
    DWORD written;
    WriteConsole(E.hStdout, "\x1b[2J", 4, &written, NULL);
    WriteConsole(E.hStdout, "\x1b[H", 3, &written, NULL);

    editorDrawRows();

    WriteConsole(E.hStdout, "\x1b[H", 3, &written, NULL);
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