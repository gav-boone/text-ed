#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <conio.h>
#include <windows.h>
#include <errno.h>

DWORD originalMode;
HANDLE hStdin;

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
    DWORD fileType = GetFileType(hStdin);
    printf("File type: %lu\n", fileType);
    if (!GetConsoleMode(hStdin, &originalMode))
        die("SetConsoleMode");

    DWORD raw = originalMode;
    raw &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_VIRTUAL_TERMINAL_INPUT);

    if (!SetConsoleMode(hStdin, raw))
        die("SetConsoleMode");

    atexit(disableRawMode);
}

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

        if (c == 'q')
            break;
    };
    return 0;
}