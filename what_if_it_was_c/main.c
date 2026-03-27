#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <conio.h>
#include <windows.h>

DWORD originalMode;
HANDLE hStdin;

void disableRawMode() {
    SetConsoleMode(hStdin, originalMode);
}

void enableRawMode() {
    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hStdin, &originalMode);

    DWORD raw = originalMode;
    raw &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
    SetConsoleMode(hStdin, raw);

    atexit(disableRawMode);
}



int main() {
    enableRawMode();

    char c;
    while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q') {
        if (iscntrl(c)) {
            printf("%d\n", c);
        } else {
            printf("%d ('%c')\n", c, c);
        }
    };
    return 0;
}