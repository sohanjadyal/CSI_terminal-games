#include "snake.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <iostream>
#include <string>
#include <vector>

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

namespace {

// Authentic retro arcade palette matching nsnake
const char* const CLR_RESET      = "\033[0m";
const char* const CLR_FRAME      = "\033[38;2;148;163;184m";   // Muted slate borders (#94a3b8)
const char* const CLR_TITLE      = "\033[38;2;192;132;252m";   // Arcade purple title (#c084fc)
const char* const CLR_MODE       = "\033[38;2;115;232;242m";   // Neon cyan mode label (#73e8f2)
const char* const CLR_WALL       = "\033[38;2;156;163;175m";   // Shaded wall blocks (#9ca3af)
const char* const CLR_HEAD       = "\033[1;38;2;74;222;128m";  // Bright green head (@)
const char* const CLR_BODY       = "\033[38;2;34;197;94m";     // Emerald green body (o)
const char* const CLR_BODY_FADE  = "\033[38;2;22;163;74m";     // Faded green for cascade rain
const char* const CLR_FRUIT      = "\033[1;38;2;248;113;113m"; // Vibrant salmon red fruit ($)
const char* const CLR_HUD        = "\033[38;2;56;189;248m";    // HUD cyan values (#38bdf8)
const char* const CLR_GOLD       = "\033[38;2;250;204;21m";    // Gold / yellow accents
const char* const CLR_WHITE      = "\033[38;2;244;244;245m";   // Bright white
const char* const CLR_MUTED      = "\033[38;2;113;113;122m";   // Muted slate

bool isTerminalInteractive() {
#ifdef _WIN32
    return _isatty(0) && _isatty(1);
#else
    return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
#endif
}

void sleepMs(int ms) {
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

bool kbhit_posix() {
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

RawInput pollKey() {
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

void flushInputBuffer() {
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

// Global session high-score persistence
int gSessionHiScore = 0;

// Falling snake particle for title screen cascade
struct CascadeSnake {
    int col = 0;
    float y = 0.0f;
    float speed = 0.4f;
    int length = 4;
};

void renderHelpModal() {
    std::cout << "\033[H\n";
    std::string out;
    out += "  " + std::string(CLR_FRAME) + "┌─────────────────────────────────────────────────────────────┐\n";
    out += "  │                                                             │\n";
    out += "  │   " + std::string(CLR_MODE) + "╭──────────────── CONTROLS & GAMEPLAY GUIDE ────────────────╮" + CLR_FRAME + "   │\n";
    out += "  │   │                                                           │   │\n";
    out += "  │   │   " + std::string(CLR_GOLD) + "[W] / [↑]" + CLR_WHITE + "       Steer Snake Up                     " + CLR_MODE + "│" + CLR_FRAME + "   │\n";
    out += "  │   │   " + std::string(CLR_GOLD) + "[A] / [←]" + CLR_WHITE + "       Steer Snake Left                   " + CLR_MODE + "│" + CLR_FRAME + "   │\n";
    out += "  │   │   " + std::string(CLR_GOLD) + "[S] / [↓]" + CLR_WHITE + "       Steer Snake Down                   " + CLR_MODE + "│" + CLR_FRAME + "   │\n";
    out += "  │   │   " + std::string(CLR_GOLD) + "[D] / [→]" + CLR_WHITE + "       Steer Snake Right                  " + CLR_MODE + "│" + CLR_FRAME + "   │\n";
    out += "  │   │                                                           │   │\n";
    out += "  │   │   " + std::string(CLR_GOLD) + "[P]" + CLR_WHITE + "             Pause / Resume ongoing match       " + CLR_MODE + "│" + CLR_FRAME + "   │\n";
    out += "  │   │   " + std::string(CLR_GOLD) + "[Q] / [ESC]" + CLR_WHITE + "     Quit match or return to main menu  " + CLR_MODE + "│" + CLR_FRAME + "   │\n";
    out += "  │   │                                                           │   │\n";
    out += "  │   │   " + std::string(CLR_TITLE) + "◆ ARCADE MODE RULES ◆" + CLR_WHITE + "                                   " + CLR_MODE + "│" + CLR_FRAME + "   │\n";
    out += "  │   │   · Eat " + std::string(CLR_FRUIT) + "$" + CLR_WHITE + " fruits (+10 pts each) to grow your snake     " + CLR_MODE + "│" + CLR_FRAME + "   │\n";
    out += "  │   │   · Dynamic speed progression (Level 1 -> 9)          " + std::string(CLR_MODE) + "│" + std::string(CLR_FRAME) + "   │\n";
    out += "  │   │   · Boundary wrap-around active on all 4 walls        " + std::string(CLR_MODE) + "│" + std::string(CLR_FRAME) + "   │\n";
    out += "  │   │   · Avoid running into your own body segments         " + std::string(CLR_MODE) + "│" + std::string(CLR_FRAME) + "   │\n";
    out += "  │   │                                                           │   │\n";
    out += "  │   │               " + std::string(CLR_MUTED) + "Press any key to return..." + std::string(CLR_MODE) + "                │" + std::string(CLR_FRAME) + "   │\n";
    out += "  │   ╰───────────────────────────────────────────────────────────╯   │\n";
    out += "  │                                                             │\n";
    out += "  └─────────────────────────────────────────────────────────────┘\n" + std::string(CLR_RESET);
    std::cout << out << std::flush;

    while (true) {
        auto input = pollKey();
        if (input.code != KeyCode::None) {
            flushInputBuffer();
            break;
        }
        sleepMs(30);
    }
}

void renderTitleScreen(int selectedOption, const std::vector<CascadeSnake>& snakes) {
    const int W = 68;
    const int H = 20;

    std::vector<std::vector<std::string>> grid(H, std::vector<std::string>(W, " "));
    std::vector<std::vector<std::string>> colors(H, std::vector<std::string>(W, CLR_RESET));

    // Outer frame
    for (int x = 0; x < W; ++x) {
        grid[0][x] = "─";
        grid[H - 1][x] = "─";
        colors[0][x] = CLR_FRAME;
        colors[H - 1][x] = CLR_FRAME;
    }
    for (int y = 0; y < H; ++y) {
        grid[y][0] = "│";
        grid[y][W - 1] = "│";
        colors[y][0] = CLR_FRAME;
        colors[y][W - 1] = CLR_FRAME;
    }
    grid[0][0] = "┌";
    grid[0][W - 1] = "┐";
    grid[H - 1][0] = "└";
    grid[H - 1][W - 1] = "┘";

    // Top border title
    std::string topTitle = "Snake";
    std::string topMenu = "Main Menu";
    for (size_t i = 0; i < topTitle.size() && (i + 1) < static_cast<size_t>(W); ++i) {
        grid[0][i + 1] = std::string(1, topTitle[i]);
        colors[0][i + 1] = CLR_TITLE;
    }
    int menuStart = W - static_cast<int>(topMenu.size()) - 2;
    for (size_t i = 0; i < topMenu.size() && (menuStart + static_cast<int>(i)) < W; ++i) {
        grid[0][menuStart + i] = std::string(1, topMenu[i]);
        colors[0][menuStart + i] = CLR_MODE;
    }

    // Big Retro ASCII Logo: SNAKE
    const std::vector<std::string> logo = {
        " ____  _  _   __   _  _  ____ ",
        "/ ___|| \\| | / _\\ | |/ /|  __|",
        "\\___ \\|    |/    \\|   < |  __|",
        "|____/|_|\\_/_/\\_/\\|_|\\_\\|____|"
    };

    for (size_t r = 0; r < logo.size(); ++r) {
        for (size_t c = 0; c < logo[r].size() && (c + 2) < static_cast<size_t>(W); ++c) {
            grid[r + 2][c + 2] = std::string(1, logo[r][c]);
            colors[r + 2][c + 2] = CLR_HEAD;
        }
    }

    // Side Menu Panel (Main Menu options)
    const std::vector<std::string> options = {
        "Arcade Mode",
        "Controls & Help",
        "Quit Arena"
    };

    int panelX = 42;
    int panelY = 2;
    int panelW = 23;

    // Side panel top
    grid[panelY][panelX] = "┌";
    colors[panelY][panelX] = CLR_MODE;
    std::string panelHeader = "Main Menu";
    for (size_t i = 0; i < panelHeader.size(); ++i) {
        grid[panelY][panelX + 1 + i] = std::string(1, panelHeader[i]);
        colors[panelY][panelX + 1 + i] = CLR_MODE;
    }
    for (int i = static_cast<int>(panelHeader.size()) + 1; i < panelW - 1; ++i) {
        grid[panelY][panelX + i] = "─";
        colors[panelY][panelX + i] = CLR_FRAME;
    }
    grid[panelY][panelX + panelW - 1] = "┐";
    colors[panelY][panelX + panelW - 1] = CLR_FRAME;

    for (int opt = 0; opt < 3; ++opt) {
        int row = panelY + 1 + opt;
        grid[row][panelX] = "│";
        colors[row][panelX] = CLR_FRAME;
        grid[row][panelX + panelW - 1] = "│";
        colors[row][panelX + panelW - 1] = CLR_FRAME;

        bool isSelected = (opt == selectedOption);
        std::string prefix = isSelected ? "> " : "  ";
        std::string optText = prefix + options[opt];
        std::string col = isSelected ? CLR_MODE : CLR_WHITE;

        for (size_t c = 0; c < optText.size() && (panelX + 1 + static_cast<int>(c)) < (panelX + panelW - 1); ++c) {
            grid[row][panelX + 1 + c] = std::string(1, optText[c]);
            colors[row][panelX + 1 + c] = col;
        }
    }

    // Blank rows in panel
    for (int r = panelY + 4; r <= panelY + 14; ++r) {
        grid[r][panelX] = "│";
        colors[r][panelX] = CLR_FRAME;
        grid[r][panelX + panelW - 1] = "│";
        colors[r][panelX + panelW - 1] = CLR_FRAME;
    }

    // Nav hint in panel
    std::string hint1 = " [W/S] Navigate";
    std::string hint2 = " [Enter] Select";
    for (size_t c = 0; c < hint1.size() && (panelX + 1 + static_cast<int>(c)) < (panelX + panelW - 1); ++c) {
        grid[panelY + 11][panelX + 1 + c] = std::string(1, hint1[c]);
        colors[panelY + 11][panelX + 1 + c] = CLR_MUTED;
    }
    for (size_t c = 0; c < hint2.size() && (panelX + 1 + static_cast<int>(c)) < (panelX + panelW - 1); ++c) {
        grid[panelY + 12][panelX + 1 + c] = std::string(1, hint2[c]);
        colors[panelY + 12][panelX + 1 + c] = CLR_MUTED;
    }

    // Side panel bottom
    grid[panelY + 15][panelX] = "└";
    colors[panelY + 15][panelX] = CLR_FRAME;
    for (int i = 1; i < panelW - 1; ++i) {
        grid[panelY + 15][panelX + i] = "─";
        colors[panelY + 15][panelX + i] = CLR_FRAME;
    }
    grid[panelY + 15][panelX + panelW - 1] = "┘";
    colors[panelY + 15][panelX + panelW - 1] = CLR_FRAME;

    // Draw Falling Snake Cascade Matrix
    for (const auto& s : snakes) {
        int headY = static_cast<int>(s.y);
        for (int seg = 0; seg < s.length; ++seg) {
            int y = headY - seg;
            if (y >= 7 && y < (H - 1) && s.col >= 2 && s.col < (panelX - 1)) {
                if (grid[y][s.col] == " ") {
                    if (seg == 0) {
                        grid[y][s.col] = "@";
                        colors[y][s.col] = CLR_HEAD;
                    } else {
                        grid[y][s.col] = "o";
                        colors[y][s.col] = (seg < 3) ? CLR_BODY : CLR_BODY_FADE;
                    }
                }
            }
        }
    }

    // Render screen buffer
    std::string out;
    out.reserve(2500);
    out += "\033[H\n";
    for (int y = 0; y < H; ++y) {
        out += "  ";
        std::string lastCol = "";
        for (int x = 0; x < W; ++x) {
            if (colors[y][x] != lastCol) {
                out += colors[y][x];
                lastCol = colors[y][x];
            }
            out += grid[y][x];
        }
        out += CLR_RESET;
        out += "\n";
    }

    std::cout << out << std::flush;
}

} // namespace

SnakeGame::SnakeGame(unsigned seed, int width, int height, int numFoods)
    : width_(width), height_(height), targetFoodCount_(numFoods), rng_(seed) {
    int startX = width_ / 2;
    int startY = height_ / 2;
    body_.push_back({startX, startY});
    body_.push_back({startX - 1, startY});
    body_.push_back({startX - 2, startY});

    for (int i = 0; i < targetFoodCount_; ++i) {
        spawnFood();
    }
}

void SnakeGame::setDirection(SnakeDirection dir) {
    if (currentDir_ == SnakeDirection::Up && dir == SnakeDirection::Down) return;
    if (currentDir_ == SnakeDirection::Down && dir == SnakeDirection::Up) return;
    if (currentDir_ == SnakeDirection::Left && dir == SnakeDirection::Right) return;
    if (currentDir_ == SnakeDirection::Right && dir == SnakeDirection::Left) return;
    currentDir_ = dir;
}

bool SnakeGame::step() {
    if (gameOver_) return false;

    Point newHead = body_.front();
    switch (currentDir_) {
        case SnakeDirection::Up:    newHead.y--; break;
        case SnakeDirection::Down:  newHead.y++; break;
        case SnakeDirection::Left:  newHead.x--; break;
        case SnakeDirection::Right: newHead.x++; break;
    }

    // Wrap around boundaries on all 4 sides
    if (newHead.x < 1) newHead.x = width_ - 2;
    else if (newHead.x > width_ - 2) newHead.x = 1;

    if (newHead.y < 1) newHead.y = height_ - 2;
    else if (newHead.y > height_ - 2) newHead.y = 1;

    // Check if newHead lands on any food
    auto foodIt = std::find(foods_.begin(), foods_.end(), newHead);
    bool ateFood = (foodIt != foods_.end());

    // Self-collision (tail will vacate unless snake is eating food)
    size_t checkCount = body_.size() - (ateFood ? 0 : 1);
    for (size_t i = 0; i < checkCount; ++i) {
        if (body_[i] == newHead) {
            gameOver_ = true;
            return false;
        }
    }

    if (ateFood) {
        body_.push_front(newHead);
        score_ += 10;
        foods_.erase(foodIt);
        if (score_ > gSessionHiScore) {
            gSessionHiScore = score_;
        }
        if (body_.size() == static_cast<size_t>((width_ - 2) * (height_ - 2))) {
            won_ = true;
            return true;
        }
        spawnFood();
    } else {
        body_.push_front(newHead);
        body_.pop_back();
    }

    return true;
}

void SnakeGame::spawnFood() {
    std::vector<Point> empty;
    empty.reserve(static_cast<size_t>((width_ - 2) * (height_ - 2)));
    for (int y = 1; y < height_ - 1; ++y) {
        for (int x = 1; x < width_ - 1; ++x) {
            Point p{x, y};
            bool occupied = false;
            for (const auto& seg : body_) {
                if (seg == p) {
                    occupied = true;
                    break;
                }
            }
            if (!occupied) {
                for (const auto& f : foods_) {
                    if (f == p) {
                        occupied = true;
                        break;
                    }
                }
            }
            if (!occupied) empty.push_back(p);
        }
    }

    if (empty.empty()) {
        won_ = true;
        return;
    }

    std::uniform_int_distribution<size_t> dist(0, empty.size() - 1);
    foods_.push_back(empty[dist(rng_)]);
}

void SnakeGame::print() const {
    std::string out;
    out.reserve(512);
    out += "\nScore: " + std::to_string(score_) + "\n+";
    for (int x = 1; x < width_ - 1; ++x) out += '-';
    out += "+\n";

    for (int y = 1; y < height_ - 1; ++y) {
        out += '|';
        for (int x = 1; x < width_ - 1; ++x) {
            Point p{x, y};
            if (p == body_.front()) {
                out += '@';
            } else {
                bool isBody = false;
                for (size_t i = 1; i < body_.size(); ++i) {
                    if (body_[i] == p) {
                        isBody = true;
                        break;
                    }
                }
                if (isBody) {
                    out += 'o';
                } else if (!foods_.empty() && p == foods_.front()) {
                    out += '*';
                } else {
                    out += ' ';
                }
            }
        }
        out += "|\n";
    }

    out += '+';
    for (int x = 1; x < width_ - 1; ++x) out += '-';
    out += "+\n";

    std::cout << out;
}

void SnakeGame::printArcade(int hiScore, int speedLevel) const {
    const int totalW = width_ + 2;

    std::string out;
    out.reserve(2048);

    // Top border: ┌Snake────────────────────────────Arcade Mode┐
    std::string titleStr = "Snake";
    std::string modeStr = "Arcade Mode";
    int middleDashes = totalW - 2 - static_cast<int>(titleStr.size()) - static_cast<int>(modeStr.size());
    if (middleDashes < 2) middleDashes = 2;

    out += CLR_FRAME;
    out += "┌";
    out += CLR_TITLE;
    out += titleStr;
    out += CLR_FRAME;
    for (int i = 0; i < middleDashes; ++i) out += "─";
    out += CLR_MODE;
    out += modeStr;
    out += CLR_FRAME;
    out += "┐\n";

    // Top inner shaded wall
    out += "│" + std::string(CLR_WALL);
    for (int i = 0; i < width_; ++i) out += "░";
    out += std::string(CLR_FRAME) + "│\n";

    // Field rows: x from 1 to width_ - 2 is playable area
    for (int y = 1; y < height_ - 1; ++y) {
        out += std::string(CLR_FRAME) + "│" + std::string(CLR_WALL) + "░" + CLR_RESET;
        for (int x = 1; x < width_ - 1; ++x) {
            Point p{x, y};
            if (p == body_.front()) {
                out += std::string(CLR_HEAD) + "@" + CLR_RESET;
            } else {
                bool isBody = false;
                for (size_t i = 1; i < body_.size(); ++i) {
                    if (body_[i] == p) {
                        isBody = true;
                        break;
                    }
                }
                if (isBody) {
                    out += std::string(CLR_BODY) + "o" + CLR_RESET;
                } else {
                    bool isFood = false;
                    for (const auto& f : foods_) {
                        if (f == p) {
                            isFood = true;
                            break;
                        }
                    }
                    if (isFood) {
                        out += std::string(CLR_FRUIT) + "$" + CLR_RESET;
                    } else {
                        out += " ";
                    }
                }
            }
        }
        out += std::string(CLR_WALL) + "░" + std::string(CLR_FRAME) + "│\n";
    }

    // Bottom inner shaded wall
    out += "│" + std::string(CLR_WALL);
    for (int i = 0; i < width_; ++i) out += "░";
    out += std::string(CLR_FRAME) + "│\n";

    // Status HUD bar: |Hi-Score 0              Score 0              Speed 1|
    std::string hiStr = "Hi-Score " + std::to_string(hiScore);
    std::string scStr = "Score " + std::to_string(score_);
    std::string spStr = "Speed " + std::to_string(speedLevel);

    int segWidth = (totalW - 2) / 3;
    auto padTo = [](const std::string& s, int w) {
        if (static_cast<int>(s.size()) >= w) return s;
        int left = 1;
        int right = w - static_cast<int>(s.size()) - left;
        if (right < 0) right = 0;
        return std::string(left, ' ') + s + std::string(right, ' ');
    };

    int remaining = (totalW - 2) - (segWidth * 2);

    out += std::string(CLR_FRAME) + "│" + CLR_HUD + padTo(hiStr, segWidth)
        + padTo(scStr, segWidth)
        + padTo(spStr, remaining)
        + CLR_FRAME + "│\n";

    // Bottom border
    out += "└";
    for (int i = 0; i < totalW - 2; ++i) out += "─";
    out += "┘\n" + std::string(CLR_RESET);

    std::cout << out << std::flush;
}

namespace {

// Runs an Arcade match
void runArcadeGame(int numFoods = 5) {
    const int arenaW = 62;
    const int arenaH = 16;
    SnakeGame game(std::random_device{}(), arenaW, arenaH, numFoods);

    std::cout << "\033[2J\033[H" << std::flush;

    std::deque<SnakeDirection> inputQueue;
    auto pushDir = [&](SnakeDirection newDir) {
        SnakeDirection lastDir = inputQueue.empty() ? game.direction() : inputQueue.back();
        bool isOpposite = (lastDir == SnakeDirection::Up && newDir == SnakeDirection::Down) ||
                          (lastDir == SnakeDirection::Down && newDir == SnakeDirection::Up) ||
                          (lastDir == SnakeDirection::Left && newDir == SnakeDirection::Right) ||
                          (lastDir == SnakeDirection::Right && newDir == SnakeDirection::Left);
        if (!isOpposite && lastDir != newDir && inputQueue.size() < 2) {
            inputQueue.push_back(newDir);
        }
    };

    bool paused = false;

    while (!game.isGameOver() && !game.hasWon()) {
        int speedLevel = std::min(9, 1 + game.score() / 40);
        int sleepTickMs = std::max(60, 175 - (speedLevel - 1) * 14);

        std::cout << "\033[H";
        game.printArcade(gSessionHiScore, speedLevel);

        if (paused) {
            std::cout << "  " << CLR_GOLD << "▶ [ PAUSED ]" << CLR_WHITE << "  Press 'P' to resume, 'Q' to quit arena" << CLR_RESET << "          \n";
            while (paused) {
                auto input = pollKey();
                if (input.code == KeyCode::Char) {
                    if (input.ch == 'p') {
                        paused = false;
                        break;
                    }
                    if (input.ch == 'q') return;
                } else if (input.code == KeyCode::Escape) {
                    return;
                }
                sleepMs(30);
            }
            continue;
        } else {
            std::cout << "  " << CLR_MUTED << "Controls: Arrow Keys or W A S D to steer | P: Pause | Q: Quit" << CLR_RESET << "  \n";
        }

        // Process inputs
        auto startTick = std::chrono::steady_clock::now();
        while (true) {
            auto input = pollKey();
            if (input.code == KeyCode::Up) pushDir(SnakeDirection::Up);
            else if (input.code == KeyCode::Down) pushDir(SnakeDirection::Down);
            else if (input.code == KeyCode::Left) pushDir(SnakeDirection::Left);
            else if (input.code == KeyCode::Right) pushDir(SnakeDirection::Right);
            else if (input.code == KeyCode::Char) {
                if (input.ch == 'w') pushDir(SnakeDirection::Up);
                else if (input.ch == 's') pushDir(SnakeDirection::Down);
                else if (input.ch == 'a') pushDir(SnakeDirection::Left);
                else if (input.ch == 'd') pushDir(SnakeDirection::Right);
                else if (input.ch == 'p') {
                    paused = true;
                    break;
                } else if (input.ch == 'q') {
                    return;
                }
            } else if (input.code == KeyCode::Escape) {
                return;
            }

            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTick).count();
            if (elapsed >= sleepTickMs) break;
            sleepMs(10);
        }

        if (paused) continue;

        if (!inputQueue.empty()) {
            game.setDirection(inputQueue.front());
            inputQueue.pop_front();
        }

        game.step();
    }

    // Final frame
    std::cout << "\033[H";
    int finalSpeed = std::min(9, 1 + game.score() / 40);
    game.printArcade(gSessionHiScore, finalSpeed);

    if (game.hasWon()) {
        std::cout << "\n  " << CLR_GOLD << "★ ARENA CLEARED! PERFECT RUN! Final Score: " << game.score() << " ★" << CLR_RESET << "\n";
    } else {
        std::cout << "\n  " << CLR_FRUIT << "✖ GAME OVER! Final Score: " << game.score()
                  << "  |  Hi-Score: " << gSessionHiScore << CLR_RESET << "\n";
    }
    std::cout << "  " << CLR_MUTED << "Press any key to return to menu..." << CLR_RESET << "\n";

    flushInputBuffer();
    while (true) {
        auto in = pollKey();
        if (in.code != KeyCode::None) break;
        sleepMs(30);
    }
}

} // namespace

int getSnakeSessionHiScore() {
    return gSessionHiScore;
}

void setSnakeSessionHiScore(int score) {
    gSessionHiScore = score;
}

void playSnake() {
    if (!isTerminalInteractive()) {
        SnakeGame game;
        game.print();
        return;
    }

    ConsoleSetup consoleSetup;
    CursorGuard cursorGuard;

    std::cout << "\033[2J\033[H" << std::flush;

    // Initialize cascade snake particles
    std::vector<CascadeSnake> cascadeSnakes = {
        {6,  8.0f,  0.30f, 4},
        {12, 12.0f, 0.40f, 5},
        {16, 5.0f,  0.35f, 3},
        {20, 14.0f, 0.45f, 6},
        {24, 7.0f,  0.32f, 4},
        {28, 11.0f, 0.38f, 5},
        {34, 4.0f,  0.42f, 4},
        {38, 9.0f,  0.30f, 3}
    };

    int selectedOption = 0; // 0: Arcade Mode, 1: Controls & Help, 2: Quit

    while (true) {
        renderTitleScreen(selectedOption, cascadeSnakes);

        // Update cascade particles
        for (auto& s : cascadeSnakes) {
            s.y += s.speed;
            if (s.y - s.length > 20.0f) {
                s.y = 7.0f;
            }
        }

        // Check for inputs
        auto startTick = std::chrono::steady_clock::now();
        while (true) {
            auto input = pollKey();
            if (input.code == KeyCode::Up) {
                selectedOption = (selectedOption + 2) % 3;
                break;
            } else if (input.code == KeyCode::Down) {
                selectedOption = (selectedOption + 1) % 3;
                break;
            } else if (input.code == KeyCode::Enter || input.code == KeyCode::Space) {
                if (selectedOption == 0) {
                    runArcadeGame(5);
                    std::cout << "\033[2J\033[H" << std::flush;
                } else if (selectedOption == 1) {
                    renderHelpModal();
                    std::cout << "\033[2J\033[H" << std::flush;
                } else if (selectedOption == 2) {
                    return; // Quit to main menu
                }
                break;
            } else if (input.code == KeyCode::Char) {
                if (input.ch == 'w') {
                    selectedOption = (selectedOption + 2) % 3;
                    break;
                } else if (input.ch == 's') {
                    selectedOption = (selectedOption + 1) % 3;
                    break;
                } else if (input.ch == '1') {
                    runArcadeGame(5);
                    std::cout << "\033[2J\033[H" << std::flush;
                    break;
                } else if (input.ch == '2') {
                    renderHelpModal();
                    std::cout << "\033[2J\033[H" << std::flush;
                    break;
                } else if (input.ch == '3' || input.ch == 'q') {
                    return;
                }
            } else if (input.code == KeyCode::Escape) {
                return;
            }

            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTick).count();
            if (elapsed >= 35) break;
            sleepMs(10);
        }
    }
}
