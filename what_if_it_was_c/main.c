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

/*** terminal funcs ***/
void die(const char *s)
{
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
        die("SetConsoleMode");

    DWORD raw = originalMode;
    raw &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_VIRTUAL_TERMINAL_INPUT);

    if (!SetConsoleMode(hStdin, raw))
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
        exit(0);
        break;
    }
}

/*** main ***/
int main()
{
    enableRawMode();

    while (1)
    {
        editorProcessKeypress();
    }

    return 0;
}