/*** includes ***/
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <conio.h>
#include <windows.h>

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/
DWORD originalMode;
HANDLE hStdin;
HANDLE hStdout;

/*** terminal funcs ***/
void die(const char *s)
{
    DWORD written;
    WriteConsole(hStdout, "\x1b[2J", 4, &written, NULL);
    WriteConsole(hStdout, "\x1b[H", 3, &written, NULL);
    fprintf(stderr, "%s: error %lu\n", s, GetLastError());
    exit(1);
}

void disableRawMode()
{
    if (!SetConsoleMode(hStdin, originalMode))
        die("SetConsoleMode");
}

void enableRawMode()
{
    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if (!GetConsoleMode(hStdin, &originalMode))
        die("GetConsoleMode");

    DWORD raw = originalMode;
    raw &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_VIRTUAL_TERMINAL_INPUT);

    if (!SetConsoleMode(hStdin, raw))
        die("SetConsoleMode");

    hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD outMode;
    if (!GetConsoleMode(hStdout, &outMode))
        die("GetConsoleMode");
    if (!SetConsoleMode(hStdout, outMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
        die("SetConsoleMode");

    atexit(disableRawMode);
}

char editorReadKey()
{
    char c;
    DWORD bytesRead;
    ReadConsole(hStdin, &c, 1, &bytesRead, NULL);
    return c;
}

/*** input ***/
void editorProcessKeypress()
{
    char c = editorReadKey();

    switch (c)
    {
    case CTRL_KEY('q'):
        DWORD written;
        WriteConsole(hStdout, "\x1b[2J", 4, &written, NULL);
        WriteConsole(hStdout, "\x1b[H", 3, &written, NULL);
        exit(0);
        break;
    }
}

/*** ouptut ***/
void editorRefreshScreen()
{
    DWORD written;
    WriteConsole(hStdout, "\x1b[2J", 4, &written, NULL);
    WriteConsole(hStdout, "\x1b[H", 3, &written, NULL);
}

/*** main ***/
int main()
{
    enableRawMode();

    while (1)
    {
        editorRefreshScreen();
        editorProcessKeypress();
    }

    return 0;
}