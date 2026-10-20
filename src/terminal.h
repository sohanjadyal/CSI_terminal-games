#pragma once

#include <cctype>
#include <chrono>
#include <iostream>

#ifdef _WIN32
#include <conio.h>
#include <io.h>
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#else
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif

inline bool isTerminalInteractive() {
#ifdef _WIN32
    return _isatty(0) && _isatty(1);
#else
    return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
#endif
}

inline void sleepMs(int ms) {
#ifdef _WIN32
    Sleep(static_cast<DWORD>(ms));
#else
    usleep(static_cast<useconds_t>(ms * 1000));
#endif
}

#ifdef _WIN32
struct ConsoleSetup {
    UINT oldCP = 0;
    DWORD oldMode = 0;
    HANDLE hOut = INVALID_HANDLE_VALUE;
    bool modeSet = false;

    ConsoleSetup() {
        oldCP = GetConsoleOutputCP();
        SetConsoleOutputCP(CP_UTF8);
        hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            if (GetConsoleMode(hOut, &oldMode)) {
                DWORD newMode = oldMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                modeSet = SetConsoleMode(hOut, newMode);
            }
        }
    }

    ~ConsoleSetup() {
        if (modeSet && hOut != INVALID_HANDLE_VALUE) {
            SetConsoleMode(hOut, oldMode);
        }
        if (oldCP != 0) {
            SetConsoleOutputCP(oldCP);
        }
    }
};
#else
struct ConsoleSetup {
    struct termios orig_termios;
    bool active = false;

    ConsoleSetup() {
        if (tcgetattr(STDIN_FILENO, &orig_termios) == 0) {
            struct termios raw = orig_termios;
            raw.c_lflag &= ~(ECHO | ICANON);
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
            active = true;
        }
    }

    ~ConsoleSetup() {
        if (active) {
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        }
    }
};

inline bool kbhit_posix() {
    struct timeval tv = {0L, 0L};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) > 0;
}
#endif

struct CursorGuard {
    CursorGuard() {
        std::cout << "\033[?25l" << std::flush;
    }
    ~CursorGuard() {
        std::cout << "\033[?25h\033[0m" << std::flush;
    }
};

enum class KeyCode {
    None,
    Up,
    Down,
    Left,
    Right,
    Enter,
    Space,
    Escape,
    Char
};

struct RawInput {
    KeyCode code = KeyCode::None;
    char ch = '\0';
};

inline RawInput pollKey() {
    RawInput res;
#ifdef _WIN32
    if (_kbhit()) {
        int c = _getch();
        if (c == 0 || c == 224) {
            int arrow = _getch();
            if (arrow == 72) res.code = KeyCode::Up;
            else if (arrow == 80) res.code = KeyCode::Down;
            else if (arrow == 75) res.code = KeyCode::Left;
            else if (arrow == 77) res.code = KeyCode::Right;
        } else if (c == 13 || c == 10) {
            res.code = KeyCode::Enter;
        } else if (c == 27) {
            res.code = KeyCode::Escape;
        } else if (c == ' ') {
            res.code = KeyCode::Space;
        } else {
            res.code = KeyCode::Char;
            res.ch = static_cast<char>(std::tolower(c));
        }
    }
#else
    if (kbhit_posix()) {
        char c;
        if (read(STDIN_FILENO, &c, 1) == 1) {
            if (c == '\033') {
                if (kbhit_posix()) {
                    char seq[2];
                    if (read(STDIN_FILENO, &seq[0], 1) == 1 && read(STDIN_FILENO, &seq[1], 1) == 1) {
                        if (seq[0] == '[') {
                            if (seq[1] == 'A') res.code = KeyCode::Up;
                            else if (seq[1] == 'B') res.code = KeyCode::Down;
                            else if (seq[1] == 'D') res.code = KeyCode::Left;
                            else if (seq[1] == 'C') res.code = KeyCode::Right;
                        }
                    }
                } else {
                    res.code = KeyCode::Escape;
                }
            } else if (c == '\n' || c == '\r') {
                res.code = KeyCode::Enter;
            } else if (c == ' ') {
                res.code = KeyCode::Space;
            } else {
                res.code = KeyCode::Char;
                res.ch = static_cast<char>(std::tolower(c));
            }
        }
    }
#endif
    return res;
}

inline void flushInputBuffer() {
#ifdef _WIN32
    while (_kbhit()) {
        _getch();
    }
#else
    while (kbhit_posix()) {
        char dummy;
        if (read(STDIN_FILENO, &dummy, 1) <= 0) break;
    }
#endif
}
