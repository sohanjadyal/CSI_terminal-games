#include "tictactoe.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include "terminal.h"

int dp[20000];

namespace {

const int LINES[][3] = {
    {0, 1, 2}, {3, 4, 5}, {6, 7, 8},  // rows
    {0, 3, 6}, {1, 4, 7}, {2, 5, 8},  // columns
    {0, 4, 8}, {2, 4, 6},              // diagonals
};

// ============================================================================
// TrueColor Terminal Palette (Neon Cyber Duel Theme)
// ============================================================================
const char* const CLR_RESET       = "\033[0m";
const char* const CLR_FRAME       = "\033[38;2;148;163;184m";    // Slate frame (#94a3b8)
const char* const CLR_FRAME_DIM   = "\033[38;2;71;85;105m";     // Dark slate grid (#475569)
const char* const CLR_TITLE       = "\033[1;38;2;251;191;36m";   // Radiant amber gold (#fbbf24)
const char* const CLR_MODE        = "\033[38;2;129;140;248m";    // Cyber indigo (#818cf8)
const char* const CLR_WHITE       = "\033[38;2;248;250;252m";    // Pure white (#f8fafc)
const char* const CLR_MUTED       = "\033[38;2;100;116;139m";    // Slate muted (#64748b)
const char* const CLR_X           = "\033[1;38;2;244;63;94m";    // Neon coral/crimson (#f43f5e)
const char* const CLR_O           = "\033[1;38;2;56;189;248m";   // Electric sky cyan (#38bdf8)
const char* const CLR_CURSOR      = "\033[1;38;2;250;204;21m";   // Sunflower gold (#facc15)
const char* const CLR_WIN         = "\033[1;38;2;251;191;36m";   // Radiant gold (#fbbf24)
const char* const CLR_SUCCESS     = "\033[1;38;2;52;211;153m";   // Emerald (#34d399)
const char* const CLR_ALERT       = "\033[1;38;2;244;63;94m";    // Rose alert (#f43f5e)

// Global session scoreboard
int gPlayerWins = 0;
int gAiWins = 0;
int gDraws = 0;

void drawAscii(std::vector<std::vector<std::string>>& grid,
               std::vector<std::vector<std::string>>& colors,
               int y, int startX, int maxW,
               const std::string& text,
               const std::string& color) {
    for (size_t i = 0; i < text.size() && (startX + static_cast<int>(i)) < maxW; ++i) {
        grid[y][startX + i] = std::string(1, text[i]);
        colors[y][startX + i] = color;
    }
}


int winningSquare(const Board& board, char player) {
    for (int square = 0; square < 9; square++) {
        if (board[square] != ' ') continue;
        Board next = board;
        next[square] = player;
        if (winner(next) == player) return square;
    }
    return -1;
}

void show(const Board& board) {
    for (int row = 0; row < 3; row++) {
        std::cout << " " << board[row * 3] << " | " << board[row * 3 + 1] << " | " << board[row * 3 + 2] << "\n";
        if (row < 2) std::cout << "---+---+---\n";
    }
}

// ============================================================================
// Controls & Strategy Guide Modal
// ============================================================================
void renderHelpModal() {
    std::cout << "\033[H\n";
    std::string out;
    out.reserve(2500);
    out += "  " + std::string(CLR_FRAME) + "┌─────────────────────────────────────────────────────────────┐\n";
    out += "  │                                                             │\n";
    out += "  │   " + std::string(CLR_TITLE) + "╭─────────── TIC-TAC-TOE CONTROLS & STRATEGY ─────────────╮" + CLR_FRAME + "   │\n";
    out += "  │   │                                                         │   │\n";
    out += "  │   │   " + std::string(CLR_CURSOR) + "[W/A/S/D] or [Arrows]" + CLR_WHITE + "  Navigate cursor across 3x3 grid    " + CLR_TITLE + "│" + CLR_FRAME + "   │\n";
    out += "  │   │   " + std::string(CLR_CURSOR) + "[Enter] or [Space]" + CLR_WHITE + "     Place your mark on selected square " + CLR_TITLE + "│" + CLR_FRAME + "   │\n";
    out += "  │   │   " + std::string(CLR_CURSOR) + "[1 - 9]" + CLR_WHITE + "                 Quick-place mark using number keys " + CLR_TITLE + "│" + CLR_FRAME + "   │\n";
    out += "  │   │                                                         │   │\n";
    out += "  │   │   " + std::string(CLR_CURSOR) + "[R]" + CLR_WHITE + "                     Restart match session              " + CLR_TITLE + "│" + CLR_FRAME + "   │\n";
    out += "  │   │   " + std::string(CLR_CURSOR) + "[Q] / [ESC]" + CLR_WHITE + "             Return to Game Menu / Hub          " + CLR_TITLE + "│" + CLR_FRAME + "   │\n";
    out += "  │   │                                                         │   │\n";
    out += "  │   │   " + std::string(CLR_MODE) + "◆ MASTER STRATEGY GUIDELINES ◆" + CLR_WHITE + "                         " + CLR_TITLE + "│" + CLR_FRAME + "   │\n";
    out += "  │   │   · " + std::string(CLR_SUCCESS) + "Center Command" + CLR_WHITE + ": The center square is part of 4       " + CLR_TITLE + "│" + CLR_FRAME + "   │\n";
    out += "  │   │     winning lines. Taking it establishes maximum control" + std::string(CLR_TITLE) + "│" + std::string(CLR_FRAME) + "   │\n";
    out += "  │   │   · " + std::string(CLR_SUCCESS) + "Corner Fortress" + CLR_WHITE + ": Corner squares control 3 lines each. " + CLR_TITLE + "│" + CLR_FRAME + "   │\n";
    out += "  │   │     Opening with a corner forces defensive replies.     " + std::string(CLR_TITLE) + "│" + std::string(CLR_FRAME) + "   │\n";
    out += "  │   │   · " + std::string(CLR_SUCCESS) + "Fork Threats" + CLR_WHITE + ": Create two simultaneous threats so the   " + CLR_TITLE + "│" + CLR_FRAME + "   │\n";
    out += "  │   │     opponent can only block one, securing your win!     " + std::string(CLR_TITLE) + "│" + std::string(CLR_FRAME) + "   │\n";
    out += "  │   │                                                         │   │\n";
    out += "  │   │              " + std::string(CLR_MUTED) + "Press any key to return..." + std::string(CLR_TITLE) + "                 │" + std::string(CLR_FRAME) + "   │\n";
    out += "  │   ╰─────────────────────────────────────────────────────────╯   │\n";
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

// ============================================================================
// Title Screen Rendering
// ============================================================================
void renderTitleScreen(int selectedOption, const Board& demoBoard) {
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

    // Top Header Bar
    drawAscii(grid, colors, 0, 2, W, "TIC-TAC-TOE", CLR_TITLE);
    int subStart = W - 16;
    drawAscii(grid, colors, 0, subStart, W, "Strategy Duel", CLR_MODE);

    // Retro ASCII Logo: TIC TAC TOE (34 chars wide)
    const std::vector<std::string> logo = {
        " _____ ___ ____   _____ _    ____ ",
        "|_   _|_ _/ ___| |_   _/ \\  / ___|",
        "  | |  | | |       | |/ _ \\| |    ",
        "  | |  | | |___    | / ___ \\ |___ ",
        "  |_| |___\\____|   |/_/   \\_\\____|"
    };

    for (size_t r = 0; r < logo.size(); ++r) {
        for (size_t c = 0; c < logo[r].size() && (c + 4) < static_cast<size_t>(W); ++c) {
            grid[r + 2][c + 4] = std::string(1, logo[r][c]);
            colors[r + 2][c + 4] = (c < 17) ? CLR_X : CLR_O;
        }
    }
    drawAscii(grid, colors, 7, 13, 38, "--- TOE ---", CLR_TITLE);

    // Tagline
    drawAscii(grid, colors, 8, 4, 40, "Outthink the AI or challenge a friend!", CLR_MUTED);

    // Mini Animated Demo Board (Lines 10 to 16, centered: demoX = 13)
    int demoX = 13;
    int demoY = 10;

    grid[demoY][demoX] = "┌"; colors[demoY][demoX] = CLR_FRAME_DIM;
    for (int c = 0; c < 3; ++c) {
        for (int i = 0; i < 3; ++i) {
            grid[demoY][demoX + 1 + c * 4 + i] = "─";
            colors[demoY][demoX + 1 + c * 4 + i] = CLR_FRAME_DIM;
        }
        grid[demoY][demoX + 1 + c * 4 + 3] = (c == 2) ? "┐" : "┬";
        colors[demoY][demoX + 1 + c * 4 + 3] = CLR_FRAME_DIM;
    }

    for (int r = 0; r < 3; ++r) {
        int rowY = demoY + 1 + r * 2;
        grid[rowY][demoX] = "│"; colors[rowY][demoX] = CLR_FRAME_DIM;
        for (int c = 0; c < 3; ++c) {
            char mark = demoBoard[r * 3 + c];
            std::string cellStr = "   ";
            std::string cellCol = CLR_RESET;
            if (mark == 'X') { cellStr = " X "; cellCol = CLR_X; }
            else if (mark == 'O') { cellStr = " O "; cellCol = CLR_O; }
            else { cellStr = " . "; cellCol = CLR_MUTED; }

            for (int i = 0; i < 3; ++i) {
                grid[rowY][demoX + 1 + c * 4 + i] = std::string(1, cellStr[i]);
                colors[rowY][demoX + 1 + c * 4 + i] = cellCol;
            }
            grid[rowY][demoX + 1 + c * 4 + 3] = "│";
            colors[rowY][demoX + 1 + c * 4 + 3] = CLR_FRAME_DIM;
        }

        int sepY = rowY + 1;
        if (r < 2) {
            grid[sepY][demoX] = "├"; colors[sepY][demoX] = CLR_FRAME_DIM;
            for (int c = 0; c < 3; ++c) {
                for (int i = 0; i < 3; ++i) {
                    grid[sepY][demoX + 1 + c * 4 + i] = "─";
                    colors[sepY][demoX + 1 + c * 4 + i] = CLR_FRAME_DIM;
                }
                grid[sepY][demoX + 1 + c * 4 + 3] = (c == 2) ? "┤" : "┼";
                colors[sepY][demoX + 1 + c * 4 + 3] = CLR_FRAME_DIM;
            }
        } else {
            grid[sepY][demoX] = "└"; colors[sepY][demoX] = CLR_FRAME_DIM;
            for (int c = 0; c < 3; ++c) {
                for (int i = 0; i < 3; ++i) {
                    grid[sepY][demoX + 1 + c * 4 + i] = "─";
                    colors[sepY][demoX + 1 + c * 4 + i] = CLR_FRAME_DIM;
                }
                grid[sepY][demoX + 1 + c * 4 + 3] = (c == 2) ? "┘" : "┴";
                colors[sepY][demoX + 1 + c * 4 + 3] = CLR_FRAME_DIM;
            }
        }
    }

    drawAscii(grid, colors, demoY + 7, demoX - 3, 40, "[ Live Duel Preview ]", CLR_MUTED);

    // Side Menu Panel (cols 42 to 64, width 23)
    int panelX = 42;
    int panelY = 2;
    int panelW = 23;
    int panelH = 16;

    // Top border
    grid[panelY][panelX] = "┌"; colors[panelY][panelX] = CLR_FRAME;
    for (int i = 1; i < panelW - 1; ++i) {
        grid[panelY][panelX + i] = "─"; colors[panelY][panelX + i] = CLR_FRAME;
    }
    grid[panelY][panelX + panelW - 1] = "┐"; colors[panelY][panelX + panelW - 1] = CLR_FRAME;
    drawAscii(grid, colors, panelY, panelX + 2, panelX + panelW - 1, " Game Menu ", CLR_MODE);

    // Vertical borders on all rows
    for (int r = panelY + 1; r < panelY + panelH - 1; ++r) {
        grid[r][panelX] = "│"; colors[r][panelX] = CLR_FRAME;
        grid[r][panelX + panelW - 1] = "│"; colors[r][panelX + panelW - 1] = CLR_FRAME;
    }

    // Options
    const std::vector<std::string> options = {
        "VS Computer (AI)",
        "2-Player Local",
        "How to Play & Tips",
        "Return to Hub"
    };

    for (int opt = 0; opt < 4; ++opt) {
        int row = panelY + 1 + opt;
        bool isSelected = (opt == selectedOption);
        std::string prefix = isSelected ? "> " : "  ";
        std::string optText = prefix + options[opt];
        std::string col = isSelected ? CLR_TITLE : CLR_WHITE;
        drawAscii(grid, colors, row, panelX + 1, panelX + panelW - 1, optText, col);
    }

    // Divider: Scoreboard
    int recY = panelY + 6;
    grid[recY][panelX] = "├"; colors[recY][panelX] = CLR_FRAME;
    for (int i = 1; i < panelW - 1; ++i) {
        grid[recY][panelX + i] = "─"; colors[recY][panelX + i] = CLR_FRAME;
    }
    grid[recY][panelX + panelW - 1] = "┤"; colors[recY][panelX + panelW - 1] = CLR_FRAME;
    drawAscii(grid, colors, recY, panelX + 3, panelX + panelW - 1, " Scoreboard ", CLR_MODE);

    drawAscii(grid, colors, recY + 1, panelX + 2, panelX + panelW - 1, "Player X: " + std::to_string(gPlayerWins) + " Wins", CLR_X);
    drawAscii(grid, colors, recY + 2, panelX + 2, panelX + panelW - 1, "AI / O:   " + std::to_string(gAiWins) + " Wins", CLR_O);
    drawAscii(grid, colors, recY + 3, panelX + 2, panelX + panelW - 1, "Draws:    " + std::to_string(gDraws) + " Ties", CLR_WHITE);

    // Another divider before nav hints
    int div2Y = recY + 4;
    grid[div2Y][panelX] = "├"; colors[div2Y][panelX] = CLR_FRAME;
    for (int i = 1; i < panelW - 1; ++i) {
        grid[div2Y][panelX + i] = "─"; colors[div2Y][panelX + i] = CLR_FRAME;
    }
    grid[div2Y][panelX + panelW - 1] = "┤"; colors[div2Y][panelX + panelW - 1] = CLR_FRAME;

    drawAscii(grid, colors, div2Y + 1, panelX + 2, panelX + panelW - 1, "[W/S] Navigate", CLR_MUTED);
    drawAscii(grid, colors, div2Y + 2, panelX + 2, panelX + panelW - 1, "[Enter] Select", CLR_MUTED);

    // Panel bottom border
    grid[panelY + panelH - 1][panelX] = "└"; colors[panelY + panelH - 1][panelX] = CLR_FRAME;
    for (int i = 1; i < panelW - 1; ++i) {
        grid[panelY + panelH - 1][panelX + i] = "─"; colors[panelY + panelH - 1][panelX + i] = CLR_FRAME;
    }
    grid[panelY + panelH - 1][panelX + panelW - 1] = "┘"; colors[panelY + panelH - 1][panelX + panelW - 1] = CLR_FRAME;

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

// ============================================================================
// In-Game Arcade Board & HUD Rendering
// ============================================================================
void renderArcadeBoard(const Board& board,
                       int cursorIndex,
                       bool isAiMode,
                       char currentTurn,
                       const std::string& statusMessage,
                       const std::array<int, 3>& winningLine) {
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

    // Header Title
    drawAscii(grid, colors, 0, 2, W, "TIC-TAC-TOE", CLR_TITLE);
    std::string modeTag = isAiMode ? "VS COMPUTER" : "2-PLAYER PVP";
    drawAscii(grid, colors, 0, W - static_cast<int>(modeTag.size()) - 3, W, modeTag, CLR_MODE);

    // 3x3 Large Board (cols 3 to 34, lines 2 to 14)
    // Each cell: width 9, height 3
    int boardX = 3;
    int boardY = 2;

    // Top border of 3x3 board
    grid[boardY][boardX] = "┌"; colors[boardY][boardX] = CLR_FRAME;
    for (int c = 0; c < 3; ++c) {
        for (int i = 0; i < 9; ++i) {
            grid[boardY][boardX + 1 + c * 10 + i] = "─";
            colors[boardY][boardX + 1 + c * 10 + i] = CLR_FRAME;
        }
        grid[boardY][boardX + 1 + c * 10 + 9] = (c == 2) ? "┐" : "┬";
        colors[boardY][boardX + 1 + c * 10 + 9] = CLR_FRAME;
    }

    auto isWinSquare = [&](int sq) {
        return sq == winningLine[0] || sq == winningLine[1] || sq == winningLine[2];
    };

    for (int r = 0; r < 3; ++r) {
        int rowTopY = boardY + 1 + r * 4;
        int rowMidY = rowTopY + 1;
        int rowBotY = rowTopY + 2;
        int rowSepY = rowTopY + 3;

        grid[rowTopY][boardX] = "│"; colors[rowTopY][boardX] = CLR_FRAME;
        grid[rowMidY][boardX] = "│"; colors[rowMidY][boardX] = CLR_FRAME;
        grid[rowBotY][boardX] = "│"; colors[rowBotY][boardX] = CLR_FRAME;

        for (int c = 0; c < 3; ++c) {
            int sq = r * 3 + c;
            char mark = board[sq];
            int cellLeftX = boardX + 1 + c * 10;
            bool isCursor = (sq == cursorIndex);
            bool isWinner = isWinSquare(sq);

            // Clear cell interior (3 rows of 9 chars)
            for (int i = 0; i < 9; ++i) {
                grid[rowTopY][cellLeftX + i] = " ";
                colors[rowTopY][cellLeftX + i] = CLR_RESET;
                grid[rowMidY][cellLeftX + i] = " ";
                colors[rowMidY][cellLeftX + i] = CLR_RESET;
                grid[rowBotY][cellLeftX + i] = " ";
                colors[rowBotY][cellLeftX + i] = CLR_RESET;
            }

            char displayChar = (mark != ' ') ? mark : static_cast<char>('1' + sq);
            std::string markColor;
            if (isWinner) {
                markColor = CLR_WIN;
            } else if (mark == 'X') {
                markColor = CLR_X;
            } else if (mark == 'O') {
                markColor = CLR_O;
            } else {
                markColor = isCursor ? CLR_CURSOR : CLR_MUTED;
            }

            if (isCursor) {
                // The active cursor ALWAYS shows bright yellow brackets [ ] around it
                grid[rowMidY][cellLeftX + 2] = "[";
                colors[rowMidY][cellLeftX + 2] = CLR_CURSOR;

                grid[rowMidY][cellLeftX + 4] = std::string(1, displayChar);
                colors[rowMidY][cellLeftX + 4] = markColor;

                grid[rowMidY][cellLeftX + 6] = "]";
                colors[rowMidY][cellLeftX + 6] = CLR_CURSOR;
            } else {
                grid[rowMidY][cellLeftX + 4] = std::string(1, displayChar);
                colors[rowMidY][cellLeftX + 4] = markColor;
            }

            grid[rowTopY][cellLeftX + 9] = "│"; colors[rowTopY][cellLeftX + 9] = CLR_FRAME;
            grid[rowMidY][cellLeftX + 9] = "│"; colors[rowMidY][cellLeftX + 9] = CLR_FRAME;
            grid[rowBotY][cellLeftX + 9] = "│"; colors[rowBotY][cellLeftX + 9] = CLR_FRAME;
        }

        // Horizontal separator
        if (r < 2) {
            grid[rowSepY][boardX] = "├"; colors[rowSepY][boardX] = CLR_FRAME;
            for (int c = 0; c < 3; ++c) {
                for (int i = 0; i < 9; ++i) {
                    grid[rowSepY][boardX + 1 + c * 10 + i] = "─";
                    colors[rowSepY][boardX + 1 + c * 10 + i] = CLR_FRAME;
                }
                grid[rowSepY][boardX + 1 + c * 10 + 9] = (c == 2) ? "┤" : "┼";
                colors[rowSepY][boardX + 1 + c * 10 + 9] = CLR_FRAME;
            }
        } else {
            grid[rowSepY][boardX] = "└"; colors[rowSepY][boardX] = CLR_FRAME;
            for (int c = 0; c < 3; ++c) {
                for (int i = 0; i < 9; ++i) {
                    grid[rowSepY][boardX + 1 + c * 10 + i] = "─";
                    colors[rowSepY][boardX + 1 + c * 10 + i] = CLR_FRAME;
                }
                grid[rowSepY][boardX + 1 + c * 10 + 9] = (c == 2) ? "┘" : "┴";
                colors[rowSepY][boardX + 1 + c * 10 + 9] = CLR_FRAME;
            }
        }
    }

    // Right Dashboard Cards (cols 36 to 65, width 30)
    int dashX = 36;
    int dashW = 30;

    auto drawSubCard = [&](int y, const std::string& title, const std::vector<std::pair<std::string, std::string>>& lines) {
        grid[y][dashX] = "┌"; colors[y][dashX] = CLR_FRAME;
        for (int i = 1; i < dashW - 1; ++i) {
            grid[y][dashX + i] = "─"; colors[y][dashX + i] = CLR_FRAME;
        }
        grid[y][dashX + dashW - 1] = "┐"; colors[y][dashX + dashW - 1] = CLR_FRAME;
        drawAscii(grid, colors, y, dashX + 2, dashX + dashW - 1, " " + title + " ", CLR_MODE);

        for (size_t row = 0; row < lines.size(); ++row) {
            int lineY = y + 1 + static_cast<int>(row);
            grid[lineY][dashX] = "│"; colors[lineY][dashX] = CLR_FRAME;
            for (int i = 1; i < dashW - 1; ++i) {
                grid[lineY][dashX + i] = " ";
                colors[lineY][dashX + i] = CLR_RESET;
            }
            grid[lineY][dashX + dashW - 1] = "│"; colors[lineY][dashX + dashW - 1] = CLR_FRAME;

            const std::string& text = lines[row].first;
            const std::string& col = lines[row].second;
            drawAscii(grid, colors, lineY, dashX + 2, dashX + dashW - 1, text, col);
        }

        int botY = y + 1 + static_cast<int>(lines.size());
        grid[botY][dashX] = "└"; colors[botY][dashX] = CLR_FRAME;
        for (int i = 1; i < dashW - 1; ++i) {
            grid[botY][dashX + i] = "─"; colors[botY][dashX + i] = CLR_FRAME;
        }
        grid[botY][dashX + dashW - 1] = "┘"; colors[botY][dashX + dashW - 1] = CLR_FRAME;
    };

    // Card 1: Turn Indicator
    std::string turnStr;
    std::string turnCol;
    if (currentTurn == 'X') {
        turnStr = isAiMode ? "Turn: Player X (You)" : "Turn: Player X";
        turnCol = CLR_X;
    } else {
        turnStr = isAiMode ? "Turn: Computer (O)" : "Turn: Player O";
        turnCol = CLR_O;
    }

    drawSubCard(1, "MATCH STATUS", {
        {turnStr, turnCol},
        {isAiMode ? "Mode: Human vs AI" : "Mode: Local PvP Pass & Play", CLR_WHITE}
    });

    // Card 2: Scoreboard
    drawSubCard(5, "SCOREBOARD", {
        {"Player X: " + std::to_string(gPlayerWins) + " Wins", CLR_X},
        {(isAiMode ? "AI (O):   " : "Player O: ") + std::to_string(gAiWins) + " Wins", CLR_O},
        {"Draws:    " + std::to_string(gDraws) + " Ties", CLR_WHITE}
    });

    // Card 3: Controls
    drawSubCard(10, "CONTROLS", {
        {"[W/A/S/D] or [Arrows] Move", CLR_WHITE},
        {"[Enter/Space] Place Mark", CLR_CURSOR},
        {"[1-9] Direct Square Key", CLR_MUTED},
        {"[R] Restart  [Q/ESC] Quit", CLR_MUTED}
    });

    // Status message at line 17
    drawAscii(grid, colors, 17, boardX, W - 2, statusMessage, CLR_TITLE);

    // Output buffered screen
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

// ============================================================================
// Interactive Arcade Match Loop
// ============================================================================
void runArcadeTicTacToe(bool isAiMode) {
    Board board;
    board.fill(' ');
    resetMemo();

    int cursorIndex = 4; // Start at center square
    char currentTurn = 'X';
    std::string statusMsg = "Player X's turn! Select square with Arrows + Enter or press 1-9.";
    std::array<int, 3> winningLine = {-1, -1, -1};

    std::cout << "\033[2J\033[H" << std::flush;

    while (true) {
        renderArcadeBoard(board, cursorIndex, isAiMode, currentTurn, statusMsg, winningLine);

        // Check if game reached end state
        char win = winner(board);
        if (win != ' ' || isFull(board)) {
            winningLine = getWinningLine(board);
            renderArcadeBoard(board, cursorIndex, isAiMode, currentTurn, statusMsg, winningLine);

            if (win == 'X') {
                recordTicTacToeResult('X');
                std::cout << "\n  " << CLR_WIN << "★ PLAYER X WINS THE MATCH! ★" << CLR_RESET << "\n";
            } else if (win == 'O') {
                recordTicTacToeResult('O');
                std::cout << "\n  " << (isAiMode ? CLR_ALERT : CLR_WIN)
                          << (isAiMode ? "✖ COMPUTER (O) WINS THE DUEL! ✖" : "★ PLAYER O WINS THE MATCH! ★")
                          << CLR_RESET << "\n";
            } else {
                recordTicTacToeResult(' ');
                std::cout << "\n  " << CLR_WHITE << "■ CAT'S GAME - IT'S A DRAW! ■" << CLR_RESET << "\n";
            }

            std::cout << "  " << CLR_MUTED << "[R] Next Round    [Q] Return to Menu: " << CLR_RESET;

            while (true) {
                auto in = pollKey();
                if (in.code == KeyCode::Char) {
                    if (in.ch == 'r') {
                        board.fill(' ');
                        resetMemo();
                        cursorIndex = 4;
                        currentTurn = 'X';
                        winningLine = {-1, -1, -1};
                        statusMsg = "New round started! Player X, make your move.";
                        std::cout << "\033[2J\033[H" << std::flush;
                        break;
                    }
                    if (in.ch == 'q') return;
                } else if (in.code == KeyCode::Escape) {
                    return;
                }
                sleepMs(20);
            }
            continue;
        }

        // If AI's turn
        if (isAiMode && currentTurn == 'O') {
            statusMsg = "Computer is calculating optimal response...";
            renderArcadeBoard(board, cursorIndex, isAiMode, currentTurn, statusMsg, winningLine);
            sleepMs(300); // Natural arcade think delay

            int m = computerMove(board, 'O', 'X');
            if (m >= 0 && m < 9 && board[m] == ' ') {
                board[m] = 'O';
            }
            currentTurn = 'X';
            statusMsg = "Computer played square " + std::to_string(m + 1) + ". Your turn, Player X!";
            continue;
        }

        // Player's turn: poll input
        auto input = pollKey();
        if (input.code == KeyCode::None) {
            sleepMs(25);
            continue;
        }

        int curRow = cursorIndex / 3;
        int curCol = cursorIndex % 3;

        if (input.code == KeyCode::Up) {
            curRow = (curRow + 2) % 3;
            cursorIndex = curRow * 3 + curCol;
        } else if (input.code == KeyCode::Down) {
            curRow = (curRow + 1) % 3;
            cursorIndex = curRow * 3 + curCol;
        } else if (input.code == KeyCode::Left) {
            curCol = (curCol + 2) % 3;
            cursorIndex = curRow * 3 + curCol;
        } else if (input.code == KeyCode::Right) {
            curCol = (curCol + 1) % 3;
            cursorIndex = curRow * 3 + curCol;
        } else if (input.code == KeyCode::Enter || input.code == KeyCode::Space) {
            if (board[cursorIndex] == ' ') {
                board[cursorIndex] = currentTurn;
                currentTurn = (currentTurn == 'X') ? 'O' : 'X';
                statusMsg = std::string("Mark placed at square ") + std::to_string(cursorIndex + 1) + ".";
            } else {
                statusMsg = "Square " + std::to_string(cursorIndex + 1) + " is occupied! Use Arrows/WASD to pick an empty square.";
            }
        } else if (input.code == KeyCode::Char) {
            if (input.ch == 'w') {
                curRow = (curRow + 2) % 3;
                cursorIndex = curRow * 3 + curCol;
            } else if (input.ch == 's') {
                curRow = (curRow + 1) % 3;
                cursorIndex = curRow * 3 + curCol;
            } else if (input.ch == 'a') {
                curCol = (curCol + 2) % 3;
                cursorIndex = curRow * 3 + curCol;
            } else if (input.ch == 'd') {
                curCol = (curCol + 1) % 3;
                cursorIndex = curRow * 3 + curCol;
            } else if (input.ch >= '1' && input.ch <= '9') {
                int sq = (input.ch - '1');
                if (board[sq] == ' ') {
                    cursorIndex = sq;
                    board[sq] = currentTurn;
                    currentTurn = (currentTurn == 'X') ? 'O' : 'X';
                    statusMsg = std::string("Mark placed at square ") + std::to_string(sq + 1) + ".";
                } else {
                    statusMsg = "Square " + std::to_string(sq + 1) + " is already occupied!";
                }
            } else if (input.ch == 'r') {
                board.fill(' ');
                resetMemo();
                cursorIndex = 4;
                currentTurn = 'X';
                winningLine = {-1, -1, -1};
                statusMsg = "Game reset! Player X, make your move.";
            } else if (input.ch == 'q') {
                return;
            }
        } else if (input.code == KeyCode::Escape) {
            return;
        }
    }
}

} // namespace

// ============================================================================
// Core Minimax Implementation
// ============================================================================
char winner(const Board& board) {
    for (const auto& line : LINES) {
        char a = board[line[0]];
        if (a != ' ' && a == board[line[1]] && a == board[line[2]]) return a;
    }
    return ' ';
}

std::array<int, 3> getWinningLine(const Board& board) {
    for (const auto& line : LINES) {
        char a = board[line[0]];
        if (a != ' ' && a == board[line[1]] && a == board[line[2]]) {
            return {line[0], line[1], line[2]};
        }
    }
    return {-1, -1, -1};
}

bool isFull(const Board& board) {
    for (char c : board)
        if (c == ' ') return false;
    return true;
}

int state(const Board& board) {
    int ans = 0;
    int mul = 1;
    for (int i = 0; i < 9; i++) {
        int val = 0;
        if (board[i] == ' ') val = 0;
        else if (board[i] == 'O') val = 1;
        else if (board[i] == 'X') val = 2;
        ans += val * mul;
        mul *= 3;
    }
    return ans;
}

int max(int a, int b) {
    return (a > b) ? a : b;
}

int chooseMove(Board board, char me, char opponent) {
    if (dp[state(board)] != -2) return dp[state(board)];
    if (winner(board) == me) {
        dp[state(board)] = 1;
        return 1;
    } else if (winner(board) == opponent) {
        dp[state(board)] = -1;
        return -1;
    } else if (isFull(board)) {
        dp[state(board)] = 0;
        return 0;
    }
    int best_score = -2;
    for (int i = 0; i < 9; i++) {
        if (board[i] == ' ') {
            Board next = board;
            next[i] = me;
            int ret = -1 * chooseMove(next, opponent, me);
            best_score = max(best_score, ret);
        }
    }
    dp[state(board)] = best_score;
    return best_score;
}

void resetMemo() {
    for (int i = 0; i < 20000; i++) dp[i] = -2;
}

int computerMove(const Board& board, char me, char opponent) {
    int winning_move = winningSquare(board, me);
    if (winning_move != -1) return winning_move;

    static const int ORDER[9] = {4, 0, 2, 6, 8, 1, 3, 5, 7};
    int move = -1;
    int best_score = -2;
    for (int i : ORDER) {
        if (board[i] != ' ') continue;
        Board next = board;
        next[i] = me;
        int score = -chooseMove(next, opponent, me);
        if (score > best_score) {
            best_score = score;
            move = i;
        }
    }
    return move;
}

// Session Accessors
int getTicTacToePlayerWins() { return gPlayerWins; }
int getTicTacToeAiWins() { return gAiWins; }
int getTicTacToeDraws() { return gDraws; }
void recordTicTacToeResult(char winner) {
    if (winner == 'X') {
        gPlayerWins++;
    } else if (winner == 'O') {
        gAiWins++;
    } else {
        gDraws++;
    }
}
void resetTicTacToeScores() {
    gPlayerWins = 0;
    gAiWins = 0;
    gDraws = 0;
}

// ============================================================================
// Main Entrypoint
// ============================================================================
void playTicTacToe() {
    if (!isTerminalInteractive()) {
        resetMemo();
        Board board;
        board.fill(' ');
        std::cout << "\nYou are X, the computer is O. Squares are numbered 1-9.\n\n";
        while (true) {
            show(board);
            int square;
            std::cout << "\nYour move (1-9): ";

            if (!(std::cin >> square)) {
                if (std::cin.eof()) return;
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "Please enter a number from 1 to 9.\n";
                continue;
            }

            if (square < 1 || square > 9 || board[square - 1] != ' ') {
                std::cout << "Pick an empty square from 1 to 9.\n";
                continue;
            }

            board[square - 1] = 'X';
            if (winner(board) == 'X') { show(board); std::cout << "\nYou win!\n"; return; }
            if (isFull(board)) { show(board); std::cout << "\nIt's a draw!\n"; return; }

            board[computerMove(board, 'O', 'X')] = 'O';
            if (winner(board) == 'O') { show(board); std::cout << "\nThe computer wins!\n"; return; }
        }
        return;
    }

    ConsoleSetup consoleSetup;
    CursorGuard cursorGuard;

    std::cout << "\033[2J\033[H" << std::flush;

    // Mini preview simulation on title screen
    Board demoBoard;
    demoBoard.fill(' ');
    char demoTurn = 'X';
    auto lastDemoTick = std::chrono::steady_clock::now();

    int selectedOption = 0; // 0: VS AI, 1: 2-Player PvP, 2: Help, 3: Return

    while (true) {
        // Step demo simulation every ~700ms
        auto now = std::chrono::steady_clock::now();
        auto demoElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastDemoTick).count();
        if (demoElapsed >= 700) {
            lastDemoTick = now;
            if (winner(demoBoard) != ' ' || isFull(demoBoard)) {
                demoBoard.fill(' ');
                demoTurn = 'X';
            } else {
                std::vector<int> emptySquares;
                for (int i = 0; i < 9; ++i) {
                    if (demoBoard[i] == ' ') emptySquares.push_back(i);
                }
                if (!emptySquares.empty()) {
                    std::random_device rd;
                    std::uniform_int_distribution<size_t> dist(0, emptySquares.size() - 1);
                    demoBoard[emptySquares[dist(rd)]] = demoTurn;
                    demoTurn = (demoTurn == 'X') ? 'O' : 'X';
                }
            }
        }

        renderTitleScreen(selectedOption, demoBoard);

        // Menu navigation
        auto startTick = std::chrono::steady_clock::now();
        while (true) {
            auto input = pollKey();
            if (input.code == KeyCode::Up) {
                selectedOption = (selectedOption + 3) % 4;
                break;
            } else if (input.code == KeyCode::Down) {
                selectedOption = (selectedOption + 1) % 4;
                break;
            } else if (input.code == KeyCode::Enter || input.code == KeyCode::Space) {
                if (selectedOption == 0) {
                    runArcadeTicTacToe(true); // VS AI
                    std::cout << "\033[2J\033[H" << std::flush;
                } else if (selectedOption == 1) {
                    runArcadeTicTacToe(false); // 2-Player PvP
                    std::cout << "\033[2J\033[H" << std::flush;
                } else if (selectedOption == 2) {
                    renderHelpModal();
                    std::cout << "\033[2J\033[H" << std::flush;
                } else if (selectedOption == 3) {
                    return; // Return to hub
                }
                break;
            } else if (input.code == KeyCode::Char) {
                if (input.ch == 'w') {
                    selectedOption = (selectedOption + 3) % 4;
                    break;
                } else if (input.ch == 's') {
                    selectedOption = (selectedOption + 1) % 4;
                    break;
                } else if (input.ch == '1') {
                    runArcadeTicTacToe(true);
                    std::cout << "\033[2J\033[H" << std::flush;
                    break;
                } else if (input.ch == '2') {
                    runArcadeTicTacToe(false);
                    std::cout << "\033[2J\033[H" << std::flush;
                    break;
                } else if (input.ch == '3') {
                    renderHelpModal();
                    std::cout << "\033[2J\033[H" << std::flush;
                    break;
                } else if (input.ch == '4' || input.ch == 'q') {
                    return;
                }
            } else if (input.code == KeyCode::Escape) {
                return;
            }

            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTick).count();
            if (elapsed >= 40) break;
            sleepMs(10);
        }
    }
}
