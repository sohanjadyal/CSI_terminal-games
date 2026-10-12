#include "intro.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "blackjack.h"
#include "dino.h"
#include "game2048.h"
#include "hangman.h"
#include "snake.h"
#include "tictactoe.h"

#ifdef _WIN32
#include <conio.h>
#include <io.h>
#include <windows.h>
#define ISATTY _isatty
#define FILENO _fileno
#else
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#define ISATTY isatty
#define FILENO fileno
#endif

namespace Borderland {

namespace {

// Authentic palette matching https://git.csispit.site/
const char* const CLR_RESET     = "\033[0m";
const char* const CLR_RED       = "\033[38;2;255;82;102m";  // #ff5266 Signature Crimson
const char* const CLR_CYAN      = "\033[38;2;115;232;242m"; // #73e8f2 Neon Cyan
const char* const CLR_WHITE     = "\033[38;2;244;244;245m"; // Pure Ink White
const char* const CLR_BOLD_WHT  = "\033[1;38;2;255;255;255m";
const char* const CLR_MUTED     = "\033[38;2;113;113;122m"; // Muted Slate
const char* const CLR_GOLD      = "\033[38;2;255;215;0m";   // Gold / Yellow
const char* const CLR_CARD_BDR  = "\033[38;2;161;161;170m"; // Sleek Card Border (#a1a1aa)

bool isInteractive() {
    return ISATTY(FILENO(stdout)) && ISATTY(FILENO(stdin));
}

#ifndef _WIN32
class PosixRawGuard {
public:
    PosixRawGuard() {
        if (isatty(STDIN_FILENO)) {
            if (tcgetattr(STDIN_FILENO, &orig_) == 0) {
                termios raw = orig_;
                // Disable line-buffering (ICANON) and echo so keys are detected immediately
                raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
                raw.c_cc[VMIN] = 0;
                raw.c_cc[VTIME] = 0;
                if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0) {
                    active_ = true;
                }
            }
        }
    }

    ~PosixRawGuard() {
        if (active_) {
            // Discard any residual bytes pressed during the intro animation
            tcflush(STDIN_FILENO, TCIFLUSH);
            char buf[64];
            while (read(STDIN_FILENO, buf, sizeof(buf)) > 0) {}
            tcsetattr(STDIN_FILENO, TCSANOW, &orig_);
        }
    }

private:
    termios orig_{};
    bool active_ = false;
};
#endif

bool checkKeyHit() {
#ifdef _WIN32
    return _kbhit() != 0;
#else
    struct timeval tv = {0L, 0L};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) > 0;
#endif
}

void drainInputBuffer() {
#ifdef _WIN32
    while (_kbhit()) {
        _getch();
    }
#else
    tcflush(STDIN_FILENO, TCIFLUSH);
    char buf[64];
    while (read(STDIN_FILENO, buf, sizeof(buf)) > 0) {}
#endif
}

bool sleepCheck(int ms) {
    if (!isInteractive()) return false;
    const int step = 15;
    for (int elapsed = 0; elapsed < ms; elapsed += step) {
        if (checkKeyHit()) {
            drainInputBuffer();
            return true; // user requested skip
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(step));
    }
    return false;
}

std::string centerText(const std::string& text, int width) {
    int len = static_cast<int>(text.size());
    if (len > width) {
        return text.substr(0, width);
    }
    int pad = width - len;
    int right = pad / 2;
    int left = pad - right;
    return std::string(left, ' ') + text + std::string(right, ' ');
}

// 8 lines per card, 13 chars wide each
std::string renderEmptyCardLine(int /* row */) {
    return "             ";
}

std::string renderFaceDownLine(int row) {
    switch (row) {
        case 0: return std::string(CLR_CARD_BDR) + "┌───────────┐" + CLR_RESET;
        case 1: return std::string(CLR_CARD_BDR) + "│ " + CLR_CYAN + "♠" + CLR_CARD_BDR + "       " + CLR_CYAN + "♣" + CLR_CARD_BDR + " │" + CLR_RESET;
        case 2: return std::string(CLR_CARD_BDR) + "│  " + CLR_MUTED + "· · · ·" + CLR_CARD_BDR + "  │" + CLR_RESET;
        case 3: return std::string(CLR_CARD_BDR) + "│   " + CLR_WHITE + "ENTRY" + CLR_CARD_BDR + "   │" + CLR_RESET;
        case 4: return std::string(CLR_CARD_BDR) + "│   " + CLR_WHITE + "GAME" + CLR_CARD_BDR + "    │" + CLR_RESET;
        case 5: return std::string(CLR_CARD_BDR) + "│  " + CLR_MUTED + "· · · ·" + CLR_CARD_BDR + "  │" + CLR_RESET;
        case 6: return std::string(CLR_CARD_BDR) + "│ " + CLR_RED + "♦" + CLR_CARD_BDR + "       " + CLR_RED + "♥" + CLR_CARD_BDR + " │" + CLR_RESET;
        case 7: return std::string(CLR_CARD_BDR) + "└───────────┘" + CLR_RESET;
        default: return "             ";
    }
}

std::string renderTurningLine(int row, const std::string& accentColor) {
    switch (row) {
        case 0: return "  " + std::string(CLR_CARD_BDR) + "┌───────┐" + CLR_RESET + "  ";
        case 1: return "  " + std::string(CLR_CARD_BDR) + "│ " + accentColor + "♦   ♥" + CLR_CARD_BDR + " │" + CLR_RESET + "  ";
        case 2: return "  " + std::string(CLR_CARD_BDR) + "│ " + CLR_MUTED + "· · ·" + CLR_CARD_BDR + " │" + CLR_RESET + "  ";
        case 3: return "  " + std::string(CLR_CARD_BDR) + "│ " + CLR_BOLD_WHT + "FLIP" + CLR_CARD_BDR + "  │" + CLR_RESET + "  ";
        case 4: return "  " + std::string(CLR_CARD_BDR) + "│ " + CLR_BOLD_WHT + "CARD" + CLR_CARD_BDR + "  │" + CLR_RESET + "  ";
        case 5: return "  " + std::string(CLR_CARD_BDR) + "│ " + CLR_MUTED + "· · ·" + CLR_CARD_BDR + " │" + CLR_RESET + "  ";
        case 6: return "  " + std::string(CLR_CARD_BDR) + "│ " + accentColor + "♠   ♣" + CLR_CARD_BDR + " │" + CLR_RESET + "  ";
        case 7: return "  " + std::string(CLR_CARD_BDR) + "└───────┘" + CLR_RESET + "  ";
        default: return "             ";
    }
}

std::string renderEdgeLine(int /* row */) {
    return "      " + std::string(CLR_BOLD_WHT) + "│" + CLR_RESET + "      ";
}

std::string renderFaceUpLine(const GameTrial& trial, int row) {
    std::string idStr = std::to_string(trial.id);
    int spacesCount = 8 - static_cast<int>(idStr.size());
    if (spacesCount < 1) spacesCount = 1;
    std::string pad(spacesCount, ' ');

    switch (row) {
        case 0: return std::string(CLR_CARD_BDR) + "┌───────────┐" + CLR_RESET;
        case 1: return std::string(CLR_CARD_BDR) + "│ " + trial.color + trial.suit + CLR_CARD_BDR + pad + CLR_GOLD + idStr + CLR_CARD_BDR + " │" + CLR_RESET;
        case 2: return std::string(CLR_CARD_BDR) + "│           │" + CLR_RESET;
        case 3: return std::string(CLR_CARD_BDR) + "│" + CLR_BOLD_WHT + centerText(trial.title, 11) + CLR_CARD_BDR + "│" + CLR_RESET;
        case 4: return std::string(CLR_CARD_BDR) + "│" + CLR_MUTED + centerText("Trial of", 11) + CLR_CARD_BDR + "│" + CLR_RESET;
        case 5: return std::string(CLR_CARD_BDR) + "│" + trial.color + centerText(trial.trialType, 11) + CLR_CARD_BDR + "│" + CLR_RESET;
        case 6: return std::string(CLR_CARD_BDR) + "│ " + CLR_GOLD + idStr + CLR_CARD_BDR + pad + trial.color + trial.suit + CLR_CARD_BDR + " │" + CLR_RESET;
        case 7: return std::string(CLR_CARD_BDR) + "└───────────┘" + CLR_RESET;
        default: return "             ";
    }
}

std::string renderCardRow(const GameTrial& trial, int state, int row) {
    if (state == 0) return renderEmptyCardLine(row);
    if (state == 1) return renderFaceDownLine(row);
    if (state == 2) return renderTurningLine(row, trial.color);
    if (state == 3) return renderEdgeLine(row);
    return renderFaceUpLine(trial, row);
}

void printHeader(std::ostream& out = std::cout) {
    std::string h = "\n";
    h += "   " + std::string(CLR_CYAN) + "♠ " + CLR_MUTED + "· · · · · · · · · · · · · · · · · · · · · · · · · · · · · " + CLR_CYAN + "♣\n" + CLR_RESET;
    h += "                 " + std::string(CLR_BOLD_WHT) + "C S I   S P I T" + CLR_RESET + "  ·  " + CLR_RED + "S O U R C E   S T A R T\n" + CLR_RESET;
    h += "        " + std::string(CLR_WHITE) + "Welcome to the Borderland. Your first commit is your entry game.\n" + CLR_RESET;
    h += "   " + std::string(CLR_RED) + "♦ " + CLR_MUTED + "· · · · · · · · · · · · · · · · · · · · · · · · · · · · · " + CLR_RED + "♥\n\n" + CLR_RESET;
    out << h;
}

// Groups card indices into balanced rows (up to 4 cards per row)
std::vector<std::vector<int>> getRowChunks(int count) {
    std::vector<std::vector<int>> rows;
    if (count <= 4) {
        std::vector<int> r;
        for (int i = 0; i < count; ++i) r.push_back(i);
        rows.push_back(r);
    } else if (count == 5) {
        rows.push_back({0, 1, 2});
        rows.push_back({3, 4});
    } else if (count == 6) {
        rows.push_back({0, 1, 2});
        rows.push_back({3, 4, 5});
    } else if (count == 7) {
        rows.push_back({0, 1, 2, 3});
        rows.push_back({4, 5, 6});
    } else if (count == 8) {
        rows.push_back({0, 1, 2, 3});
        rows.push_back({4, 5, 6, 7});
    } else {
        for (int i = 0; i < count; i += 4) {
            std::vector<int> r;
            for (int j = i; j < i + 4 && j < count; ++j) {
                r.push_back(j);
            }
            rows.push_back(r);
        }
    }
    return rows;
}

void renderScene(const std::vector<GameTrial>& trials,
                 const std::vector<int>& cardStates,
                 const std::string& statusMessage) {
    std::cout << "\033[H";
    printHeader(std::cout);

    auto rowChunks = getRowChunks(static_cast<int>(cardStates.size()));

    for (size_t chunkIdx = 0; chunkIdx < rowChunks.size(); ++chunkIdx) {
        const auto& chunk = rowChunks[chunkIdx];
        int k = static_cast<int>(chunk.size());
        int totalWidth = k * 13 + (k - 1) * 3;
        int leftPad = std::max(0, (76 - totalWidth) / 2);
        std::string padStr(leftPad, ' ');

        for (int row = 0; row < 8; ++row) {
            std::string line = padStr;
            for (size_t i = 0; i < chunk.size(); ++i) {
                int cIdx = chunk[i];
                line += renderCardRow(trials[cIdx], cardStates[cIdx], row);
                if (i + 1 < chunk.size()) line += "   ";
            }
            std::cout << line << "\n";
        }
        if (chunkIdx + 1 < rowChunks.size()) {
            std::cout << "\n";
        }
    }

    std::cout << "\n" << statusMessage << "\n" << std::flush;
}

} // namespace

void initConsole() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
    SetConsoleOutputCP(CP_UTF8);
#endif
}

const std::vector<GameTrial>& getGameTrials() {
    static const std::vector<GameTrial> registry = {
        {1, "2048",      "2048",         "♦", "Logic",    CLR_RED,  play2048},
        {2, "TIC-TAC",   "Tic-tac-toe",  "♥", "Strategy", CLR_RED,  playTicTacToe},
        {3, "HANGMAN",   "Hangman",      "♣", "Words",    CLR_CYAN, playHangman},
        {4, "SNAKE",     "Snake",        "♠", "Reflexes", CLR_CYAN, playSnake},
        {5, "DINO",      "Dinosaur Run", "♦", "Survival", CLR_RED,  playDino},
        {6, "BLACKJACK", "Blackjack",    "♥", "Tactics",  CLR_RED,  playBlackjack},
    };
    return registry;
}

int getPageCount(size_t numTrials, int pageSize) {
    if (numTrials == 0) return 1;
    if (pageSize <= 0) pageSize = 8;
    return static_cast<int>((numTrials + pageSize - 1) / pageSize);
}

void playIntro(const std::vector<GameTrial>& trials, bool force) {
    if (!force && !isInteractive()) {
        return;
    }

#ifndef _WIN32
    PosixRawGuard rawGuard;
#endif

    // Hide cursor & clear screen once
    std::cout << "\033[?25l\033[2J\033[H" << std::flush;

    // Intro previews up to 8 cards
    int count = std::min(static_cast<int>(trials.size()), 8);
    std::vector<int> states(count, 0);

    // Step 1: Initial empty table
    renderScene(trials, states, "         " + std::string(CLR_MUTED) + "CONNECTING TO BORDERLAND ARENA..." + CLR_RESET);
    if (sleepCheck(160)) {
        std::cout << "\033[?25h\033[0m" << std::flush;
        return;
    }

    // Step 2: Deal cards face-down onto the table
    for (int c = 0; c < count; ++c) {
        states[c] = 1; // Face-down
        std::string msg = "         " + std::string(CLR_MUTED) + "DEALING TRIAL CARDS [";
        for (int i = 0; i < count; ++i) {
            if (i <= c) {
                msg += (trials[i].color) + " █" + CLR_RESET;
            } else {
                msg += std::string(CLR_MUTED) + " ·" + CLR_RESET;
            }
        }
        msg += std::string(CLR_MUTED) + " ]" + CLR_RESET;
        renderScene(trials, states, msg);
        if (sleepCheck(90)) {
            std::cout << "\033[?25h\033[0m" << std::flush;
            return;
        }
    }

    if (sleepCheck(140)) {
        std::cout << "\033[?25h\033[0m" << std::flush;
        return;
    }

    // Step 3: Flip cards one by one (3D rotation effect)
    for (int c = 0; c < count; ++c) {
        // Turning frame
        states[c] = 2;
        renderScene(trials, states, "         " + std::string(CLR_GOLD) + "REVEALING TRIAL " + std::to_string(c + 1) + " OF " + std::to_string(count) + "..." + CLR_RESET);
        if (sleepCheck(60)) {
            std::cout << "\033[?25h\033[0m" << std::flush;
            return;
        }

        // Edge frame
        states[c] = 3;
        renderScene(trials, states, "         " + std::string(CLR_BOLD_WHT) + "REVEALING TRIAL " + std::to_string(c + 1) + " OF " + std::to_string(count) + "..." + CLR_RESET);
        if (sleepCheck(50)) {
            std::cout << "\033[?25h\033[0m" << std::flush;
            return;
        }

        // Turning frame expanding back
        states[c] = 2;
        renderScene(trials, states, "         " + std::string(CLR_GOLD) + "REVEALING TRIAL " + std::to_string(c + 1) + " OF " + std::to_string(count) + "..." + CLR_RESET);
        if (sleepCheck(50)) {
            std::cout << "\033[?25h\033[0m" << std::flush;
            return;
        }

        // Fully revealed face-up
        states[c] = 4;
        renderScene(trials, states, "         " + trials[c].color + "CARD " + std::to_string(c + 1) + " UNLOCKED!" + CLR_RESET);
        if (sleepCheck(80)) {
            std::cout << "\033[?25h\033[0m" << std::flush;
            return;
        }
    }

    // Final reveal frame
    renderScene(trials, states, "         " + std::string(CLR_BOLD_WHT) + "◆ ALL " + std::to_string(count) + " BORDERLAND TRIALS UNLOCKED ◆" + CLR_RESET);
    sleepCheck(280);

    // Restore cursor
    std::cout << "\033[?25h\033[0m" << std::flush;
}

void printMenu(const std::vector<GameTrial>& trials, int page, std::ostream& out) {
    // Clear screen and home cursor
    out << "\033[2J\033[H" << std::flush;

    const int pageSize = 8;
    int totalGames = static_cast<int>(trials.size());
    int totalPages = getPageCount(trials.size(), pageSize);
    if (page < 0) page = 0;
    if (page >= totalPages) page = totalPages - 1;

    int startIndex = page * pageSize;
    int endIndex = std::min(startIndex + pageSize, totalGames);
    int cardsOnPage = std::max(0, endIndex - startIndex);

    std::vector<GameTrial> pageTrials;
    for (int i = startIndex; i < endIndex; ++i) {
        pageTrials.push_back(trials[i]);
    }

    std::vector<int> revealedStates(cardsOnPage, 4);

    printHeader(out);

    auto rowChunks = getRowChunks(cardsOnPage);

    for (size_t chunkIdx = 0; chunkIdx < rowChunks.size(); ++chunkIdx) {
        const auto& chunk = rowChunks[chunkIdx];
        int k = static_cast<int>(chunk.size());
        int totalWidth = k * 13 + (k - 1) * 3;
        int leftPad = std::max(0, (76 - totalWidth) / 2);
        std::string padStr(leftPad, ' ');

        for (int row = 0; row < 8; ++row) {
            std::string line = padStr;
            for (size_t i = 0; i < chunk.size(); ++i) {
                int cIdx = chunk[i];
                line += renderCardRow(pageTrials[cIdx], revealedStates[cIdx], row);
                if (i + 1 < chunk.size()) line += "   ";
            }
            out << line << "\n";
        }
        if (chunkIdx + 1 < rowChunks.size()) {
            out << "\n";
        }
    }

    out << "\n";
    out << "                    " << CLR_CYAN << "╭─────────────────────────────╮\n"
        << "                    " << CLR_CYAN << "│" << CLR_BOLD_WHT << "   ◆  THE GAME HAS BEGUN  ◆   " << CLR_CYAN << "│\n"
        << "                    " << CLR_CYAN << "╰─────────────────────────────╯" << CLR_RESET << "\n\n";

    // Options line
    out << "     ";
    for (size_t i = 0; i < trials.size(); ++i) {
        out << CLR_GOLD << "[" << (i + 1) << "] "
            << trials[i].color << trials[i].suit << " "
            << CLR_WHITE << trials[i].displayName << "     ";
        if ((i + 1) % 4 == 0 && (i + 1) < trials.size()) {
            out << "\n     ";
        }
    }

    out << "\n                             "
        << CLR_GOLD << "[" << (trials.size() + 1) << "] "
        << CLR_MUTED << "✖ Quit Arena" << CLR_RESET << "\n\n";

    if (totalPages > 1) {
        out << "       " << CLR_MUTED << "[ Page " << (page + 1) << " of " << totalPages
            << " · Enter 0 to toggle page ]" << CLR_RESET << "\n\n";
    }

    out << CLR_CYAN << "Pick a trial card (1-" << (trials.size() + 1) << "): " << CLR_RESET << std::flush;
}

void printExit(std::ostream& out) {
    out << "\n"
        << CLR_CYAN << "♠ " << CLR_RED << "♥ " << CLR_BOLD_WHT
        << "Leaving the Borderland. Until your next commit... "
        << CLR_RED << "♦ " << CLR_CYAN << "♣" << CLR_RESET << "\n\n";
}

void printError(size_t numGames, std::ostream& out) {
    out << CLR_RED << "Please pick ";
    for (size_t i = 1; i <= numGames; ++i) {
        if (i == 1) {
            out << i;
        } else {
            out << ", " << i;
        }
    }
    out << " or " << (numGames + 1) << "." << CLR_RESET << "\n";
}

} // namespace Borderland
