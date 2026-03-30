#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <conio.h>
#include <windows.h>
#include <errno.h>

DWORD originalMode;
HANDLE hStdin;

// TODO: implement die in RawMode controllers
void die(const char *s)
{
    perror(s);
    exit(1);
}

void disableRawMode()
{
    if (SetConsoleMode(hStdin, originalMode) == -1)
        die("SetConsoleMode");
}

void enableRawMode()
{
    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if (GetConsoleMode(hStdin, &originalMode) == -1)
        die("SetConsoleMode");

    DWORD raw = originalMode;
    raw &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_VIRTUAL_TERMINAL_INPUT);

    if (SetConsoleMode(hStdin, raw) == -1)
        die("SetConsoleMode");

    atexit(disableRawMode);
}

int main()
{
    enableRawMode();

    char c;
    DWORD bytesRead;
    while (1)
    {
        char c = '\0';
        if (ReadConsole(hStdin, &c, 1, &bytesRead, NULL) == -1 && errno != EAGAIN)
            die("ReadConsole");

        if (iscntrl(c))
            printf("%d\n", c);
        else
            printf("%d ('%c')\n", c, c);

        if (c == 'q')
            break;
    };
    return 0;
}