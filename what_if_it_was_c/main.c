/*** includes ***/
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <conio.h>
#include <windows.h>
#include <errno.h>

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

/*** main ***/
int main()
{
    enableRawMode();

    DWORD bytesRead;
    while (1)
    {
        char c = '\0';
        if (!ReadConsole(hStdin, &c, 1, &bytesRead, NULL) && errno != EAGAIN)
            die("ReadConsole");

        if (iscntrl(c))
            printf("%d\n", c);
        else
            printf("%d ('%c')\n", c, c);

        if (c == CTRL_KEY('q'))
            break;
    };
    return 0;
}