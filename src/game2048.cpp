#include "game2048.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "terminal.h"

namespace {

// ============================================================================
// TrueColor Terminal Palette (Cyber Amber & Indigo Puzzle Theme)
// ============================================================================
const char* const CLR_RESET       = "\033[0m";
const char* const CLR_FRAME       = "\033[38;2;148;163;184m";    // Slate frame (#94a3b8)
const char* const CLR_FRAME_DIM   = "\033[38;2;71;85;105m";     // Dark slate grid (#475569)
const char* const CLR_TITLE       = "\033[1;38;2;251;191;36m";   // Radiant amber gold (#fbbf24)
const char* const CLR_TITLE_CYAN  = "\033[1;38;2;56;189;248m";   // Electric cyan (#38bdf8)
const char* const CLR_MODE        = "\033[38;2;129;140;248m";    // Cyber indigo (#818cf8)
const char* const CLR_WHITE       = "\033[38;2;248;250;252m";    // Pure white (#f8fafc)
const char* const CLR_MUTED       = "\033[38;2;100;116;139m";    // Slate muted (#64748b)
const char* const CLR_DOT         = "\033[38;2;60;75;95m";       // Subtle cell dot (#3c4b5f)
const char* const CLR_SUCCESS     = "\033[1;38;2;52;211;153m";   // Emerald (#34d399)
const char* const CLR_ALERT       = "\033[1;38;2;244;63;94m";    // Rose alert (#f43f5e)
const char* const CLR_GOLD        = "\033[1;38;2;250;204;21m";   // Yellow gold (#facc15)

// Persistent session high score & best tile
int gSessionHiScore = 0;
int gSessionMaxTile = 0;

std::string formatNumber(int n) {
    std::string s = std::to_string(n);
    int insertPosition = static_cast<int>(s.length()) - 3;
    while (insertPosition > 0) {
        s.insert(insertPosition, ",");
        insertPosition -= 3;
    }
    return s;
}

std::string getTileColor(int val) {
    switch (val) {
        case 2:    return "\033[38;2;241;245;249m";           // Slate 50
        case 4:    return "\033[1;38;2;254;240;138m";         // Yellow 200
        case 8:    return "\033[1;38;2;251;146;60m";          // Orange 400
        case 16:   return "\033[1;38;2;249;115;22m";          // Orange 500
        case 32:   return "\033[1;38;2;251;113;133m";         // Rose 400
        case 64:   return "\033[1;38;2;244;63;94m";           // Rose 500
        case 128:  return "\033[1;38;2;250;204;21m";          // Amber 400
        case 256:  return "\033[1;38;2;234;179;8m";           // Gold 500
        case 512:  return "\033[1;38;2;52;211;153m";          // Emerald 400
        case 1024: return "\033[1;38;2;56;189;248m";          // Sky cyan 400
        case 2048: return "\033[1;38;2;254;240;138;48;2;147;51;234m"; // Gold text on Purple (#9333ea)
        case 4096: return "\033[1;38;2;255;255;255;48;2;79;70;229m";  // White on Indigo (#4f46e5)
        default:
            if (val > 4096) return "\033[1;38;2;255;255;255;48;2;225;29;72m"; // Crimson badge
            return CLR_DOT;
    }
}

// Helper to write plain ASCII text into the terminal grid buffer safely
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

// ============================================================================
// Controls & Strategy Guide Modal
// ============================================================================
void renderHelpModal() {
    std::cout << "\033[H\n";
    std::string out;
    out.reserve(2500);
    out += "  " + std::string(CLR_FRAME) + "┌─────────────────────────────────────────────────────────────┐\n";
    out += "  │                                                             │\n";
    out += "  │   " + std::string(CLR_TITLE) + "╭───────────── 2048 CONTROLS & STRATEGY GUIDE ────────────╮" + CLR_FRAME + "   │\n";
    out += "  │   │                                                         │   │\n";
    out += "  │   │   " + std::string(CLR_GOLD) + "[W] / [↑]" + CLR_WHITE + "       Slide All Tiles Up                 " + CLR_TITLE + "│" + CLR_FRAME + "   │\n";
    out += "  │   │   " + std::string(CLR_GOLD) + "[A] / [←]" + CLR_WHITE + "       Slide All Tiles Left               " + CLR_TITLE + "│" + CLR_FRAME + "   │\n";
    out += "  │   │   " + std::string(CLR_GOLD) + "[S] / [↓]" + CLR_WHITE + "       Slide All Tiles Down               " + CLR_TITLE + "│" + CLR_FRAME + "   │\n";
    out += "  │   │   " + std::string(CLR_GOLD) + "[D] / [→]" + CLR_WHITE + "       Slide All Tiles Right              " + CLR_TITLE + "│" + CLR_FRAME + "   │\n";
    out += "  │   │                                                         │   │\n";
    out += "  │   │   " + std::string(CLR_GOLD) + "[U]" + CLR_WHITE + "             Undo Last Move (Single-step)       " + CLR_TITLE + "│" + CLR_FRAME + "   │\n";
    out += "  │   │   " + std::string(CLR_GOLD) + "[R]" + CLR_WHITE + "             Restart Game Session               " + CLR_TITLE + "│" + CLR_FRAME + "   │\n";
    out += "  │   │   " + std::string(CLR_GOLD) + "[Q] / [ESC]" + CLR_WHITE + "     Return to 2048 Menu / Hub          " + CLR_TITLE + "│" + CLR_FRAME + "   │\n";
    out += "  │   │                                                         │   │\n";
    out += "  │   │   " + std::string(CLR_MODE) + "◆ GRANDMASTER STRATEGY TIPS ◆" + CLR_WHITE + "                          " + CLR_TITLE + "│" + CLR_FRAME + "   │\n";
    out += "  │   │   · " + std::string(CLR_SUCCESS) + "Corner Anchor" + CLR_WHITE + ": Keep your largest tile pinned in a   " + CLR_TITLE + "│" + CLR_FRAME + "   │\n";
    out += "  │   │     single corner (e.g., bottom-right) at all times.    " + std::string(CLR_TITLE) + "│" + std::string(CLR_FRAME) + "   │\n";
    out += "  │   │   · " + std::string(CLR_SUCCESS) + "Snake Chaining" + CLR_WHITE + ": Build descending chains of tiles into  " + CLR_TITLE + "│" + CLR_FRAME + "   │\n";
    out += "  │   │     that corner: (e.g., 1024 > 512 > 256 > 128).        " + std::string(CLR_TITLE) + "│" + std::string(CLR_FRAME) + "   │\n";
    out += "  │   │   · " + std::string(CLR_SUCCESS) + "Safe Sliding" + CLR_WHITE + ": Only slide in 2 or 3 directions so    " + CLR_TITLE + "│" + CLR_FRAME + "   │\n";
    out += "  │   │     your corner tile never gets dislodged!              " + std::string(CLR_TITLE) + "│" + std::string(CLR_FRAME) + "   │\n";
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
// Title Screen Rendering (Clean, grounded, with perfectly aligned borders)
// ============================================================================
void renderTitleScreen(int selectedOption, const Game2048& demoGame) {
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
    std::string topTitle = "2048";
    std::string topSub = "Logic Puzzle";
    drawAscii(grid, colors, 0, 2, W, topTitle, CLR_TITLE);
    int subStart = W - static_cast<int>(topSub.size()) - 3;
    drawAscii(grid, colors, 0, subStart, W, topSub, CLR_MODE);

    // Retro Cyber 2048 ASCII Logo
    const std::vector<std::string> logo = {
        "  ____    ___    _  _    ___  ",
        " |___ \\  / _ \\  | || |  / _ \\ ",
        "  __) | | | | | | || |_| (_) |",
        " / __/  | |_| | |__   _> _ <  ",
        "|_____|  \\___/     |_| \\___/  "
    };

    for (size_t r = 0; r < logo.size(); ++r) {
        for (size_t c = 0; c < logo[r].size() && (c + 4) < static_cast<size_t>(W); ++c) {
            grid[r + 2][c + 4] = std::string(1, logo[r][c]);
            colors[r + 2][c + 4] = (r < 2) ? CLR_TITLE : CLR_TITLE_CYAN;
        }
    }

    // Subtitle tagline
    std::string tagline = "Join matching numbers to reach 2048!";
    drawAscii(grid, colors, 7, 4, 40, tagline, CLR_MUTED);

    // Mini Animated Demo Board (Lines 9 to 17, centered in left column: demoX = 10)
    int demoX = 10;
    int demoY = 9;
    const auto& demoGrid = demoGame.grid();

    // Top border of mini board
    grid[demoY][demoX] = "┌"; colors[demoY][demoX] = CLR_FRAME_DIM;
    for (int c = 0; c < 4; ++c) {
        for (int i = 0; i < 3; ++i) {
            grid[demoY][demoX + 1 + c * 4 + i] = "─";
            colors[demoY][demoX + 1 + c * 4 + i] = CLR_FRAME_DIM;
        }
        grid[demoY][demoX + 1 + c * 4 + 3] = (c == 3) ? "┐" : "┬";
        colors[demoY][demoX + 1 + c * 4 + 3] = CLR_FRAME_DIM;
    }

    for (int r = 0; r < 4; ++r) {
        int rowY = demoY + 1 + r * 2;
        grid[rowY][demoX] = "│"; colors[rowY][demoX] = CLR_FRAME_DIM;
        for (int c = 0; c < 4; ++c) {
            int val = demoGrid[r][c];
            std::string cellStr = " . ";
            std::string cellCol = CLR_DOT;
            if (val > 0) {
                if (val < 10) cellStr = " " + std::to_string(val) + " ";
                else if (val < 100) cellStr = std::to_string(val) + " ";
                else if (val < 1000) cellStr = std::to_string(val);
                else cellStr = " 1k";
                cellCol = getTileColor(val);
            }

            for (int i = 0; i < 3; ++i) {
                grid[rowY][demoX + 1 + c * 4 + i] = std::string(1, cellStr[i]);
                colors[rowY][demoX + 1 + c * 4 + i] = cellCol;
            }
            grid[rowY][demoX + 1 + c * 4 + 3] = "│";
            colors[rowY][demoX + 1 + c * 4 + 3] = CLR_FRAME_DIM;
        }

        // Inner separator / bottom border
        int sepY = rowY + 1;
        if (r < 3) {
            grid[sepY][demoX] = "├"; colors[sepY][demoX] = CLR_FRAME_DIM;
            for (int c = 0; c < 4; ++c) {
                for (int i = 0; i < 3; ++i) {
                    grid[sepY][demoX + 1 + c * 4 + i] = "─";
                    colors[sepY][demoX + 1 + c * 4 + i] = CLR_FRAME_DIM;
                }
                grid[sepY][demoX + 1 + c * 4 + 3] = (c == 3) ? "┤" : "┼";
                colors[sepY][demoX + 1 + c * 4 + 3] = CLR_FRAME_DIM;
            }
        } else {
            grid[sepY][demoX] = "└"; colors[sepY][demoX] = CLR_FRAME_DIM;
            for (int c = 0; c < 4; ++c) {
                for (int i = 0; i < 3; ++i) {
                    grid[sepY][demoX + 1 + c * 4 + i] = "─";
                    colors[sepY][demoX + 1 + c * 4 + i] = CLR_FRAME_DIM;
                }
                grid[sepY][demoX + 1 + c * 4 + 3] = (c == 3) ? "┘" : "┴";
                colors[sepY][demoX + 1 + c * 4 + 3] = CLR_FRAME_DIM;
            }
        }
    }

    std::string demoLabel = "[ Live Demo Preview ]";
    drawAscii(grid, colors, demoY + 9, demoX - 2, 40, demoLabel, CLR_MUTED);

    // Side Menu Panel (cols 42 to 64, width 23)
    int panelX = 42;
    int panelY = 2;
    int panelW = 23;
    int panelH = 16;

    // Top border of panel
    grid[panelY][panelX] = "┌"; colors[panelY][panelX] = CLR_FRAME;
    for (int i = 1; i < panelW - 1; ++i) {
        grid[panelY][panelX + i] = "─"; colors[panelY][panelX + i] = CLR_FRAME;
    }
    grid[panelY][panelX + panelW - 1] = "┐"; colors[panelY][panelX + panelW - 1] = CLR_FRAME;
    drawAscii(grid, colors, panelY, panelX + 2, panelX + panelW - 1, " Game Menu ", CLR_MODE);

    // Vertical borders on all internal rows
    for (int r = panelY + 1; r < panelY + panelH - 1; ++r) {
        grid[r][panelX] = "│"; colors[r][panelX] = CLR_FRAME;
        grid[r][panelX + panelW - 1] = "│"; colors[r][panelX + panelW - 1] = CLR_FRAME;
    }

    // Options
    const std::vector<std::string> options = {
        "Classic 2048",
        "How to Play & Tips",
        "Return to Hub"
    };

    for (int opt = 0; opt < 3; ++opt) {
        int row = panelY + 1 + opt;
        bool isSelected = (opt == selectedOption);
        std::string prefix = isSelected ? "> " : "  ";
        std::string optText = prefix + options[opt];
        std::string col = isSelected ? CLR_TITLE : CLR_WHITE;
        drawAscii(grid, colors, row, panelX + 1, panelX + panelW - 1, optText, col);
    }

    // Sub-card: Session Records
    int recY = panelY + 5;
    grid[recY][panelX] = "├"; colors[recY][panelX] = CLR_FRAME;
    for (int i = 1; i < panelW - 1; ++i) {
        grid[recY][panelX + i] = "─"; colors[recY][panelX + i] = CLR_FRAME;
    }
    grid[recY][panelX + panelW - 1] = "┤"; colors[recY][panelX + panelW - 1] = CLR_FRAME;
    drawAscii(grid, colors, recY, panelX + 3, panelX + panelW - 1, " Session Best ", CLR_MODE);

    std::string hiStr = " High: " + (gSessionHiScore > 0 ? formatNumber(gSessionHiScore) : "0");
    std::string maxStr = " Tile: " + (gSessionMaxTile > 0 ? std::to_string(gSessionMaxTile) : "None");
    drawAscii(grid, colors, recY + 1, panelX + 1, panelX + panelW - 1, hiStr, CLR_GOLD);
    drawAscii(grid, colors, recY + 2, panelX + 1, panelX + panelW - 1, maxStr, CLR_SUCCESS);

    // Another divider before nav hints
    int div2Y = recY + 4;
    grid[div2Y][panelX] = "├"; colors[div2Y][panelX] = CLR_FRAME;
    for (int i = 1; i < panelW - 1; ++i) {
        grid[div2Y][panelX + i] = "─"; colors[div2Y][panelX + i] = CLR_FRAME;
    }
    grid[div2Y][panelX + panelW - 1] = "┤"; colors[div2Y][panelX + panelW - 1] = CLR_FRAME;

    drawAscii(grid, colors, div2Y + 2, panelX + 2, panelX + panelW - 1, "[W/S] Navigate", CLR_MUTED);
    drawAscii(grid, colors, div2Y + 3, panelX + 2, panelX + panelW - 1, "[Enter] Select", CLR_MUTED);

    // Panel bottom border
    grid[panelY + panelH - 1][panelX] = "└"; colors[panelY + panelH - 1][panelX] = CLR_FRAME;
    for (int i = 1; i < panelW - 1; ++i) {
        grid[panelY + panelH - 1][panelX + i] = "─"; colors[panelY + panelH - 1][panelX + i] = CLR_FRAME;
    }
    grid[panelY + panelH - 1][panelX + panelW - 1] = "┘"; colors[panelY + panelH - 1][panelX + panelW - 1] = CLR_FRAME;

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
// In-Game Arcade Board & HUD Rendering
// ============================================================================
void renderArcadeBoard(const Game2048& game,
                       int moveCount,
                       int lastDelta,
                       const std::string& statusMessage) {
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
    drawAscii(grid, colors, 0, 2, W, "2048 PUZZLE", CLR_TITLE);
    drawAscii(grid, colors, 0, W - 14, W, "CLASSIC 4x4", CLR_MODE);

    // 4x4 Large Game Board (Left Column: cols 2 to 34, lines 1 to 17)
    // Each cell: width 7, height 3
    int boardX = 2;
    int boardY = 1;
    const auto& board = game.grid();

    // Top border of 4x4 board
    grid[boardY][boardX] = "┌"; colors[boardY][boardX] = CLR_FRAME;
    for (int c = 0; c < 4; ++c) {
        for (int i = 0; i < 7; ++i) {
            grid[boardY][boardX + 1 + c * 8 + i] = "─";
            colors[boardY][boardX + 1 + c * 8 + i] = CLR_FRAME;
        }
        grid[boardY][boardX + 1 + c * 8 + 7] = (c == 3) ? "┐" : "┬";
        colors[boardY][boardX + 1 + c * 8 + 7] = CLR_FRAME;
    }

    for (int r = 0; r < 4; ++r) {
        int rowTopY = boardY + 1 + r * 4;
        int rowMidY = rowTopY + 1;
        int rowBotY = rowTopY + 2;
        int rowSepY = rowTopY + 3;

        // Leftmost vertical border for each line of the row
        grid[rowTopY][boardX] = "│"; colors[rowTopY][boardX] = CLR_FRAME;
        grid[rowMidY][boardX] = "│"; colors[rowMidY][boardX] = CLR_FRAME;
        grid[rowBotY][boardX] = "│"; colors[rowBotY][boardX] = CLR_FRAME;

        for (int c = 0; c < 4; ++c) {
            int val = board[r][c];
            int cellLeftX = boardX + 1 + c * 8;

            std::string midStr = "       ";
            std::string cellColor = CLR_RESET;
            if (val > 0) {
                std::string numStr = std::to_string(val);
                int padLeft = (7 - static_cast<int>(numStr.size())) / 2;
                if (padLeft < 0) padLeft = 0;
                int padRight = 7 - static_cast<int>(numStr.size()) - padLeft;
                if (padRight < 0) padRight = 0;
                midStr = std::string(padLeft, ' ') + numStr + std::string(padRight, ' ');
                cellColor = getTileColor(val);
            }

            for (int i = 0; i < 7; ++i) {
                grid[rowTopY][cellLeftX + i] = " ";
                colors[rowTopY][cellLeftX + i] = CLR_RESET;
                grid[rowMidY][cellLeftX + i] = std::string(1, midStr[i]);
                colors[rowMidY][cellLeftX + i] = cellColor;
                grid[rowBotY][cellLeftX + i] = " ";
                colors[rowBotY][cellLeftX + i] = CLR_RESET;
            }

            grid[rowTopY][cellLeftX + 7] = "│"; colors[rowTopY][cellLeftX + 7] = CLR_FRAME;
            grid[rowMidY][cellLeftX + 7] = "│"; colors[rowMidY][cellLeftX + 7] = CLR_FRAME;
            grid[rowBotY][cellLeftX + 7] = "│"; colors[rowBotY][cellLeftX + 7] = CLR_FRAME;
        }

        // Horizontal separator line
        if (r < 3) {
            grid[rowSepY][boardX] = "├"; colors[rowSepY][boardX] = CLR_FRAME;
            for (int c = 0; c < 4; ++c) {
                for (int i = 0; i < 7; ++i) {
                    grid[rowSepY][boardX + 1 + c * 8 + i] = "─";
                    colors[rowSepY][boardX + 1 + c * 8 + i] = CLR_FRAME;
                }
                grid[rowSepY][boardX + 1 + c * 8 + 7] = (c == 3) ? "┤" : "┼";
                colors[rowSepY][boardX + 1 + c * 8 + 7] = CLR_FRAME;
            }
        } else {
            grid[rowSepY][boardX] = "└"; colors[rowSepY][boardX] = CLR_FRAME;
            for (int c = 0; c < 4; ++c) {
                for (int i = 0; i < 7; ++i) {
                    grid[rowSepY][boardX + 1 + c * 8 + i] = "─";
                    colors[rowSepY][boardX + 1 + c * 8 + i] = CLR_FRAME;
                }
                grid[rowSepY][boardX + 1 + c * 8 + 7] = (c == 3) ? "┘" : "┴";
                colors[rowSepY][boardX + 1 + c * 8 + 7] = CLR_FRAME;
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

    // Card 1: Score & Best
    std::string scText = formatNumber(game.score());
    if (lastDelta > 0) {
        scText += " (+" + formatNumber(lastDelta) + ")";
    }
    std::string bestText = formatNumber(std::max(gSessionHiScore, game.score()));

    drawSubCard(1, "SCORE & RECORD", {
        {"Score: " + scText, CLR_TITLE},
        {"Best:  " + bestText, CLR_GOLD}
    });

    // Card 2: Game Status
    int currentMax = game.maxTile();
    std::string maxStr = (currentMax > 0 ? std::to_string(currentMax) : "0");
    std::string undoStr = game.canUndo() ? "AVAILABLE" : "EMPTY";
    std::string undoCol = game.canUndo() ? CLR_SUCCESS : CLR_MUTED;

    drawSubCard(5, "STATUS", {
        {"Max Tile:   " + maxStr, getTileColor(currentMax)},
        {"Move Count: " + std::to_string(moveCount), CLR_WHITE},
        {"Undo:       " + undoStr, undoCol}
    });

    // Card 3: Controls Guide
    drawSubCard(10, "CONTROLS", {
        {"[W/A/S/D] or [Arrows] Move", CLR_WHITE},
        {"[U] Undo Move   [R] Restart", CLR_WHITE},
        {"[Q/ESC] Return to Menu", CLR_MUTED}
    });

    // Bottom Status Line (Line 18)
    drawAscii(grid, colors, 18, boardX, W - 2, statusMessage, CLR_TITLE_CYAN);

    // Render screen
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
// Interactive Arcade Game Match Runner
// ============================================================================
void runArcade2048() {
    Game2048 game;
    int moveCount = 0;
    int lastDelta = 0;
    bool continuedAfterWin = false;
    std::string statusMsg = "Use Arrow keys or WASD to slide tiles together!";

    std::cout << "\033[2J\033[H" << std::flush;

    while (true) {
        // Update session high score
        if (game.score() > gSessionHiScore) {
            gSessionHiScore = game.score();
        }
        if (game.maxTile() > gSessionMaxTile) {
            gSessionMaxTile = game.maxTile();
        }

        renderArcadeBoard(game, moveCount, lastDelta, statusMsg);

        // Check Victory
        if (!continuedAfterWin && game.hasWon()) {
            std::cout << "\n  " << CLR_TITLE << "★ YOU CREATED 2048! VICTORY ACHIEVED! ★" << CLR_RESET << "\n";
            std::cout << "  " << CLR_WHITE << "[C] Continue Playing for higher tiles (4096+) | [R] Restart | [Q] Menu: " << CLR_RESET;
            while (true) {
                auto in = pollKey();
                if (in.code == KeyCode::Char) {
                    if (in.ch == 'c') {
                        continuedAfterWin = true;
                        statusMsg = "★ 2048 Achieved! Continuing endless puzzle mode!";
                        break;
                    }
                    if (in.ch == 'r') {
                        game.reset();
                        moveCount = 0;
                        lastDelta = 0;
                        continuedAfterWin = false;
                        statusMsg = "Game restarted. Good luck!";
                        break;
                    }
                    if (in.ch == 'q') return;
                } else if (in.code == KeyCode::Escape) {
                    return;
                } else if (in.code == KeyCode::Space || in.code == KeyCode::Enter) {
                    continuedAfterWin = true;
                    statusMsg = "★ 2048 Achieved! Continuing endless puzzle mode!";
                    break;
                }
                sleepMs(20);
            }
            std::cout << "\033[2J\033[H" << std::flush;
            continue;
        }

        // Check Game Over
        if (!game.canMove()) {
            std::cout << "\n  " << CLR_ALERT << "✖ GAME OVER! NO MORE LEGAL MOVES REMAIN ✖" << CLR_RESET << "\n";
            std::cout << "  " << CLR_WHITE << "Final Score: " << CLR_GOLD << formatNumber(game.score())
                      << CLR_WHITE << "  |  Max Tile: " << CLR_TITLE << game.maxTile()
                      << CLR_WHITE << "  |  Session Best: " << CLR_GOLD << formatNumber(gSessionHiScore) << CLR_RESET << "\n";
            std::cout << "  " << CLR_MUTED << "[R] Try Again    [Q] Return to Menu" << CLR_RESET << "\n";

            while (true) {
                auto in = pollKey();
                if (in.code == KeyCode::Char) {
                    if (in.ch == 'r') {
                        game.reset();
                        moveCount = 0;
                        lastDelta = 0;
                        continuedAfterWin = false;
                        statusMsg = "New game started! Aim for 2048!";
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

        // Wait for player input
        auto input = pollKey();
        if (input.code == KeyCode::None) {
            sleepMs(25);
            continue;
        }

        Direction dir;
        bool hasDir = false;

        if (input.code == KeyCode::Up) { dir = Direction::Up; hasDir = true; }
        else if (input.code == KeyCode::Down) { dir = Direction::Down; hasDir = true; }
        else if (input.code == KeyCode::Left) { dir = Direction::Left; hasDir = true; }
        else if (input.code == KeyCode::Right) { dir = Direction::Right; hasDir = true; }
        else if (input.code == KeyCode::Char) {
            if (input.ch == 'w') { dir = Direction::Up; hasDir = true; }
            else if (input.ch == 's') { dir = Direction::Down; hasDir = true; }
            else if (input.ch == 'a') { dir = Direction::Left; hasDir = true; }
            else if (input.ch == 'd') { dir = Direction::Right; hasDir = true; }
            else if (input.ch == 'u') {
                if (game.undo()) {
                    moveCount = std::max(0, moveCount - 1);
                    lastDelta = 0;
                    statusMsg = "Move undone! Restored previous board state.";
                } else {
                    statusMsg = "No previous move available to undo.";
                }
            } else if (input.ch == 'r') {
                game.reset();
                moveCount = 0;
                lastDelta = 0;
                continuedAfterWin = false;
                statusMsg = "Game reset! Ready to play.";
            } else if (input.ch == 'q') {
                return;
            }
        } else if (input.code == KeyCode::Escape) {
            return;
        }

        if (hasDir) {
            int scoreBefore = game.score();
            bool moved = game.move(dir);
            if (moved) {
                moveCount++;
                lastDelta = game.score() - scoreBefore;
                if (lastDelta > 0) {
                    statusMsg = "Tiles merged! (+" + formatNumber(lastDelta) + " pts)";
                } else {
                    statusMsg = "Tiles shifted across the grid.";
                }
            } else {
                lastDelta = 0;
                statusMsg = "No tiles could move in that direction.";
            }
        }
    }
}

} // namespace

// ============================================================================
// Core Game2048 Implementation
// ============================================================================
Game2048::Game2048(unsigned seed) : rng_(seed) {
    addRandomTile();
    addRandomTile();
}

Row Game2048::slideRow(Row row, int& score) {
    // Push all non-zero tiles to the left, keeping their order.
    Row packed{};
    int count = 0;
    for (int value : row) {
        if (value != 0) packed[count++] = value;
    }

    // Merge adjacent equal tiles once per move.
    Row result{};
    int out = 0;
    for (int i = 0; i < count; i++) {
        if (i + 1 < count && packed[i] == packed[i + 1]) {
            int merged = packed[i] * 2;
            result[out++] = merged;
            score += merged;
            i++;  // Skip the second tile so it only merges once per move
        } else {
            result[out++] = packed[i];
        }
    }
    return result;
}

bool Game2048::move(Direction direction) {
    State stateBeforeMove{grid_, score_, rng_};
    bool moved = false;
    for (int line = 0; line < 4; line++) {
        // Read the line so that "forwards" is always towards index 0.
        Row row;
        for (int i = 0; i < 4; i++) {
            switch (direction) {
                case Direction::Left:  row[i] = grid_[line][i]; break;
                case Direction::Right: row[i] = grid_[line][3 - i]; break;
                case Direction::Up:    row[i] = grid_[i][line]; break;
                case Direction::Down:  row[i] = grid_[3 - i][line]; break;
            }
        }
        Row slid = slideRow(row, score_);
        if (slid != row) moved = true;
        for (int i = 0; i < 4; i++) {
            switch (direction) {
                case Direction::Left:  grid_[line][i] = slid[i]; break;
                case Direction::Right: grid_[line][3 - i] = slid[i]; break;
                case Direction::Up:    grid_[i][line] = slid[i]; break;
                case Direction::Down:  grid_[3 - i][line] = slid[i]; break;
            }
        }
    }
    if (moved) {
        previousState_ = stateBeforeMove;
        canUndo_ = true;
        addRandomTile();
    }
    return moved;
}

bool Game2048::undo() {
    if (!canUndo_) return false;
    grid_ = previousState_.grid;
    score_ = previousState_.score;
    rng_ = previousState_.rng;
    canUndo_ = false;
    return true;
}

bool Game2048::canMove() const {
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (grid_[r][c] == 0) return true;
            if (c < 3 && grid_[r][c] == grid_[r][c + 1]) return true;
            if (r < 3 && grid_[r][c] == grid_[r + 1][c]) return true;
        }
    }
    return false;
}

bool Game2048::hasWon() const {
    for (const Row& row : grid_)
        for (int value : row)
            if (value >= 2048) return true;
    return false;
}

int Game2048::maxTile() const {
    int maxV = 0;
    for (const auto& row : grid_) {
        for (int v : row) {
            if (v > maxV) maxV = v;
        }
    }
    return maxV;
}

void Game2048::reset() {
    grid_ = {};
    score_ = 0;
    canUndo_ = false;
    previousState_ = State();
    addRandomTile();
    addRandomTile();
}

void Game2048::addRandomTile() {
    std::vector<std::pair<int, int>> empty;
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            if (grid_[r][c] == 0) empty.push_back({r, c});
    if (empty.empty()) return;
    auto pt = empty[std::uniform_int_distribution<size_t>(0, empty.size() - 1)(rng_)];
    int r = pt.first;
    int c = pt.second;
    grid_[r][c] = std::uniform_int_distribution<int>(1, 10)(rng_) == 10 ? 4 : 2;
}

void Game2048::print() const {
    std::cout << "\nScore: " << score_ << "\n+------+------+------+------+\n";
    for (const Row& row : grid_) {
        std::cout << "|";
        for (int value : row) {
            if (value == 0) std::cout << "      |";
            else std::cout << std::setw(5) << value << " |";
        }
        std::cout << "\n+------+------+------+------+\n";
    }
}

// Session Accessors
int get2048SessionHiScore() { return gSessionHiScore; }
void set2048SessionHiScore(int score) { gSessionHiScore = score; }
int get2048SessionMaxTile() { return gSessionMaxTile; }
void set2048SessionMaxTile(int tile) { gSessionMaxTile = tile; }

// ============================================================================
// Main Entrypoint
// ============================================================================
void play2048() {
    if (!isTerminalInteractive()) {
        Game2048 game;
        std::cout << "\nJoin the tiles to get to 2048! Moves: w a s d, u to undo, q to quit.\n";
        while (true) {
            game.print();
            if (game.hasWon()) { std::cout << "You made 2048!\n"; return; }
            if (!game.canMove()) { std::cout << "No moves left. Game over!\n"; return; }
            std::cout << "Move: ";
            char key;
            if (!(std::cin >> key)) return;
            switch (key) {
                case 'a': game.move(Direction::Left); break;
                case 'd': game.move(Direction::Right); break;
                case 'w': game.move(Direction::Up); break;
                case 's': game.move(Direction::Down); break;
                case 'u':
                    if (!game.undo()) std::cout << "Nothing to undo.\n";
                    break;
                case 'q': return;
                default: std::cout << "Use w, a, s, d or u.\n";
            }
        }
        return;
    }

    ConsoleSetup consoleSetup;
    CursorGuard cursorGuard;

    std::cout << "\033[2J\033[H" << std::flush;

    // Mini board simulation for title screen
    Game2048 demoGame;
    auto lastDemoTick = std::chrono::steady_clock::now();

    int selectedOption = 0; // 0: Classic, 1: Help, 2: Return

    while (true) {
        // Update demo game simulation every ~750ms
        auto now = std::chrono::steady_clock::now();
        auto demoElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastDemoTick).count();
        if (demoElapsed >= 750) {
            lastDemoTick = now;
            std::vector<Direction> dirs = {Direction::Left, Direction::Down, Direction::Right, Direction::Up};
            std::random_device rd;
            std::shuffle(dirs.begin(), dirs.end(), rd);
            bool moved = false;
            for (auto d : dirs) {
                if (demoGame.move(d)) {
                    moved = true;
                    break;
                }
            }
            if (!moved || !demoGame.canMove()) {
                demoGame.reset();
            }
        }

        renderTitleScreen(selectedOption, demoGame);

        // Process title menu inputs
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
                    runArcade2048();
                    std::cout << "\033[2J\033[H" << std::flush;
                } else if (selectedOption == 1) {
                    renderHelpModal();
                    std::cout << "\033[2J\033[H" << std::flush;
                } else if (selectedOption == 2) {
                    return; // Return to hub
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
                    runArcade2048();
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
            if (elapsed >= 40) break;
            sleepMs(10);
        }
    }
}
