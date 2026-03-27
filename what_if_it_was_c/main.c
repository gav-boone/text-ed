#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <conio.h>
#include <windows.h>

DWORD originalMode;
HANDLE hStdin;

void die(const char *s) {
    perror(s);
    exit(0);
}

void disableRawMode()
{
    SetConsoleMode(hStdin, originalMode);
}

void enableRawMode()
{
    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hStdin, &originalMode);

    DWORD raw = originalMode;
    raw &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_VIRTUAL_TERMINAL_INPUT);
    SetConsoleMode(hStdin, raw);

    atexit(disableRawMode);
}

int main()
{
    enableRawMode();

    char c;
    DWORD bytesRead;
    while (ReadConsole(hStdin, &c, 1, &bytesRead, NULL) == 1 && c != 'q')
    {
        if (iscntrl(c))
        {
            printf("%d\n", c);
        }
        else
        {
            printf("%d ('%c')\n", c, c);
        }
    };
    return 0;
}