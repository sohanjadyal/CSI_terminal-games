#include "spaceinvaders.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <conio.h>
#include <io.h>
#else
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace SpaceInvaders {

namespace {

const char* const CLR_RESET     = "\033[0m";
const char* const CLR_CRIMSON   = "\033[38;2;255;82;102m";  // #ff5266 Top Alien & HUD
const char* const CLR_GREEN     = "\033[38;2;74;222;128m";  // #4ade80 Mid Alien
const char* const CLR_PURPLE    = "\033[38;2;192;132;252m"; // #c084fc Bottom Alien
const char* const CLR_CYAN      = "\033[38;2;115;232;242m"; // #73e8f2 Bunkers
const char* const CLR_GOLD      = "\033[38;2;255;215;0m";   // #ffd700 Cannon
const char* const CLR_WHITE     = "\033[1;38;2;255;255;255m";
const char* const CLR_ORANGE    = "\033[38;2;251;146;60m";  // Alien Bombs
const char* const CLR_MUTED     = "\033[38;2;113;113;122m";

const int BUNKER_X_POSITIONS[BUNKER_COUNT] = {6, 20, 34, 48};
constexpr int BUNKER_START_Y = 15;

const bool INITIAL_BUNKER_SHAPE[BUNKER_HEIGHT][BUNKER_WIDTH] = {
    {false, false, true,  true,  true,  false, false}, //   ###  
    {false, true,  true,  true,  true,  true,  false}, //  ##### 
    {true,  true,  true,  true,  true,  true,  true }, // #######
    {true,  true,  false, false, false, true,  true }  // ##   ##
};

} // namespace

SpaceInvadersGame::SpaceInvadersGame(unsigned seed) : rng_(seed) {
    initWave();
    initBunkers();
}

void SpaceInvadersGame::initWave() {
    aliens_.clear();
    aliens_.reserve(ALIEN_ROWS * ALIEN_COLS);

    for (int r = 0; r < ALIEN_ROWS; ++r) {
        AlienTier tier = AlienTier::Bottom;
        if (r == 0) tier = AlienTier::Top;
        else if (r == 1 || r == 2) tier = AlienTier::Middle;

        for (int c = 0; c < ALIEN_COLS; ++c) {
            aliens_.push_back({r, c, tier, true});
        }
    }

    fleetX_ = 4;
    fleetY_ = 2;
    fleetDir_ = 1;
    fleetTickCount_ = 0;
    animFrame_ = 0;
    bombDropCooldown_ = 15;
    playerLasers_.clear();
    alienBombs_.clear();
}

void SpaceInvadersGame::initBunkers() {
    for (int b = 0; b < BUNKER_COUNT; ++b) {
        for (int r = 0; r < BUNKER_HEIGHT; ++r) {
            for (int c = 0; c < BUNKER_WIDTH; ++c) {
                bunkers_[b][r][c] = INITIAL_BUNKER_SHAPE[r][c];
            }
        }
    }
}

int SpaceInvadersGame::aliveAlienCount() const {
    int count = 0;
    for (const auto& a : aliens_) {
        if (a.alive) ++count;
    }
    return count;
}

void SpaceInvadersGame::killAlienForTesting(int index) {
    if (index >= 0 && index < static_cast<int>(aliens_.size())) {
        aliens_[index].alive = false;
    }
}

bool SpaceInvadersGame::isBunkerBlockSolid(int bunkerIdx, int row, int col) const {
    if (bunkerIdx < 0 || bunkerIdx >= BUNKER_COUNT ||
        row < 0 || row >= BUNKER_HEIGHT ||
        col < 0 || col >= BUNKER_WIDTH) {
        return false;
    }
    return bunkers_[bunkerIdx][row][col];
}

void SpaceInvadersGame::damageBunkerForTesting(int bunkerIdx, int row, int col) {
    if (bunkerIdx >= 0 && bunkerIdx < BUNKER_COUNT &&
        row >= 0 && row < BUNKER_HEIGHT &&
        col >= 0 && col < BUNKER_WIDTH) {
        bunkers_[bunkerIdx][row][col] = false;
    }
}

void SpaceInvadersGame::addLaserForTesting(int x, int y) {
    playerLasers_.push_back({x, y, true});
}

void SpaceInvadersGame::addBombForTesting(int x, int y) {
    alienBombs_.push_back({x, y, true});
}

void SpaceInvadersGame::movePlayer(int deltaX) {
    cannonX_ = std::clamp(cannonX_ + deltaX, 0, WIDTH - CANNON_WIDTH);
}

bool SpaceInvadersGame::fireLaser() {
    int activeCount = 0;
    for (const auto& l : playerLasers_) {
        if (l.active) ++activeCount;
    }

    if (activeCount < 2) {
        int laserX = cannonX_ + 2; // Center of cannon (the '^' in '/-^-\')
        int laserY = cannonY_ - 1;
        playerLasers_.push_back({laserX, laserY, true});
        return true;
    }
    return false;
}

void SpaceInvadersGame::tick() {
    if (gameOver_ || won_) return;

    updateLasers();
    updateBombs();
    checkCollisions();
    updateAliens();
    spawnAlienBomb();
    checkCollisions();

    // Check level completion
    if (aliveAlienCount() == 0) {
        level_++;
        initWave();
        initBunkers(); // Refresh bunkers for next wave
    }
}

void SpaceInvadersGame::updateLasers() {
    for (auto& l : playerLasers_) {
        if (l.active) {
            l.y--;
            if (l.y < 1) {
                l.active = false;
            }
        }
    }
    playerLasers_.erase(
        std::remove_if(playerLasers_.begin(), playerLasers_.end(),
                       [](const Projectile& p) { return !p.active; }),
        playerLasers_.end());
}

void SpaceInvadersGame::updateBombs() {
    for (auto& b : alienBombs_) {
        if (b.active) {
            b.y++;
            if (b.y >= HEIGHT) {
                b.active = false;
            }
        }
    }
    alienBombs_.erase(
        std::remove_if(alienBombs_.begin(), alienBombs_.end(),
                       [](const Projectile& p) { return !p.active; }),
        alienBombs_.end());
}

void SpaceInvadersGame::updateAliens() {
    int alive = aliveAlienCount();
    if (alive == 0) return;

    // Movement speed accelerates dynamically as aliens are eliminated
    int movePeriod = std::max(2, (alive * 18) / (ALIEN_ROWS * ALIEN_COLS) - (level_ - 1));

    fleetTickCount_++;
    if (fleetTickCount_ < movePeriod) {
        return;
    }
    fleetTickCount_ = 0;
    animFrame_ ^= 1;

    // Find leftmost and rightmost column of living aliens
    int minCol = ALIEN_COLS;
    int maxCol = -1;
    int maxRow = -1;

    for (const auto& a : aliens_) {
        if (a.alive) {
            minCol = std::min(minCol, a.col);
            maxCol = std::max(maxCol, a.col);
            maxRow = std::max(maxRow, a.row);
        }
    }

    if (maxCol == -1) return;

    int leftEdge = fleetX_ + minCol * 4;
    int rightEdge = fleetX_ + maxCol * 4 + (ALIEN_WIDTH - 1);

    if (fleetDir_ == 1) {
        if (rightEdge + 1 >= WIDTH - 1) {
            fleetY_++;
            fleetDir_ = -1;
        } else {
            fleetX_++;
        }
    } else {
        if (leftEdge - 1 <= 1) {
            fleetY_++;
            fleetDir_ = 1;
        } else {
            fleetX_--;
        }
    }

    // Check invasion condition: aliens reach bunker level or cannon
    int lowestAlienY = fleetY_ + maxRow;
    if (lowestAlienY >= BUNKER_START_Y) {
        gameOver_ = true;
    }
}

void SpaceInvadersGame::spawnAlienBomb() {
    if (bombDropCooldown_ > 0) {
        bombDropCooldown_--;
        return;
    }

    if (alienBombs_.size() >= 3) return;

    // Find bottom-most living alien in each column
    std::vector<const Alien*> shooters;
    for (int c = 0; c < ALIEN_COLS; ++c) {
        const Alien* bottomAlien = nullptr;
        for (const auto& a : aliens_) {
            if (a.col == c && a.alive) {
                if (!bottomAlien || a.row > bottomAlien->row) {
                    bottomAlien = &a;
                }
            }
        }
        if (bottomAlien) {
            shooters.push_back(bottomAlien);
        }
    }

    if (!shooters.empty()) {
        std::uniform_int_distribution<size_t> dist(0, shooters.size() - 1);
        const Alien* chosen = shooters[dist(rng_)];
        int bombX = fleetX_ + chosen->col * 4 + 1; // Center of alien
        int bombY = fleetY_ + chosen->row + 1;
        alienBombs_.push_back({bombX, bombY, true});

        // Set next cooldown
        std::uniform_int_distribution<int> cdDist(12, 24);
        bombDropCooldown_ = std::max(6, cdDist(rng_) - (level_ - 1) * 2);
    }
}

void SpaceInvadersGame::checkCollisions() {
    // 1. Lasers hitting bunkers
    for (auto& l : playerLasers_) {
        if (!l.active) continue;
        if (l.y >= BUNKER_START_Y && l.y < BUNKER_START_Y + BUNKER_HEIGHT) {
            for (int b = 0; b < BUNKER_COUNT; ++b) {
                int bx = BUNKER_X_POSITIONS[b];
                if (l.x >= bx && l.x < bx + BUNKER_WIDTH) {
                    int col = l.x - bx;
                    int row = l.y - BUNKER_START_Y;
                    if (bunkers_[b][row][col]) {
                        bunkers_[b][row][col] = false; // Block destroyed
                        l.active = false;
                        break;
                    }
                }
            }
        }
    }

    // 2. Lasers hitting aliens
    for (auto& l : playerLasers_) {
        if (!l.active) continue;
        for (auto& a : aliens_) {
            if (!a.alive) continue;
            int ax = fleetX_ + a.col * 4;
            int ay = fleetY_ + a.row;
            if (l.y == ay && l.x >= ax && l.x < ax + ALIEN_WIDTH) {
                a.alive = false;
                l.active = false;
                if (a.tier == AlienTier::Top) score_ += 30;
                else if (a.tier == AlienTier::Middle) score_ += 20;
                else score_ += 10;
                break;
            }
        }
    }

    // 3. Bombs hitting bunkers
    for (auto& b : alienBombs_) {
        if (!b.active) continue;
        if (b.y >= BUNKER_START_Y && b.y < BUNKER_START_Y + BUNKER_HEIGHT) {
            for (int bi = 0; bi < BUNKER_COUNT; ++bi) {
                int bx = BUNKER_X_POSITIONS[bi];
                if (b.x >= bx && b.x < bx + BUNKER_WIDTH) {
                    int col = b.x - bx;
                    int row = b.y - BUNKER_START_Y;
                    if (bunkers_[bi][row][col]) {
                        bunkers_[bi][row][col] = false; // Block destroyed
                        b.active = false;
                        break;
                    }
                }
            }
        }
    }

    // 4. Bombs hitting player cannon
    for (auto& b : alienBombs_) {
        if (!b.active) continue;
        if (b.y == cannonY_ && b.x >= cannonX_ && b.x < cannonX_ + CANNON_WIDTH) {
            b.active = false;
            lives_--;
            if (lives_ <= 0) {
                gameOver_ = true;
            }
        }
    }

    // Prune inactive projectiles immediately
    playerLasers_.erase(
        std::remove_if(playerLasers_.begin(), playerLasers_.end(),
                       [](const Projectile& p) { return !p.active; }),
        playerLasers_.end());
    alienBombs_.erase(
        std::remove_if(alienBombs_.begin(), alienBombs_.end(),
                       [](const Projectile& p) { return !p.active; }),
        alienBombs_.end());
}

std::string SpaceInvadersGame::render() const {
    // 2D Character & Color Buffer
    std::vector<std::string> chars(HEIGHT, std::string(WIDTH, ' '));
    std::vector<std::vector<const char*>> colors(
        HEIGHT, std::vector<const char*>(WIDTH, CLR_RESET));

    // 1. Draw Aliens
    for (const auto& a : aliens_) {
        if (!a.alive) continue;
        int ax = fleetX_ + a.col * 4;
        int ay = fleetY_ + a.row;

        if (ay >= 0 && ay < HEIGHT) {
            const char* clr = CLR_PURPLE;
            std::string sprite = "o-o";

            if (a.tier == AlienTier::Top) {
                clr = CLR_CRIMSON;
                sprite = (animFrame_ == 0) ? ",^," : "^,^";
            } else if (a.tier == AlienTier::Middle) {
                clr = CLR_GREEN;
                sprite = (animFrame_ == 0) ? "_O-" : "-O_";
            } else {
                clr = CLR_PURPLE;
                sprite = (animFrame_ == 0) ? "o-o" : "-o-";
            }

            for (int i = 0; i < ALIEN_WIDTH; ++i) {
                int px = ax + i;
                if (px >= 0 && px < WIDTH) {
                    chars[ay][px] = sprite[i];
                    colors[ay][px] = clr;
                }
            }
        }
    }

    // 2. Draw Bunkers
    for (int b = 0; b < BUNKER_COUNT; ++b) {
        int bx = BUNKER_X_POSITIONS[b];
        for (int r = 0; r < BUNKER_HEIGHT; ++r) {
            int py = BUNKER_START_Y + r;
            if (py >= HEIGHT) continue;
            for (int c = 0; c < BUNKER_WIDTH; ++c) {
                int px = bx + c;
                if (px >= 0 && px < WIDTH && bunkers_[b][r][c]) {
                    chars[py][px] = '#';
                    colors[py][px] = CLR_CYAN;
                }
            }
        }
    }

    // 3. Draw Player Lasers
    for (const auto& l : playerLasers_) {
        if (l.active && l.y >= 0 && l.y < HEIGHT && l.x >= 0 && l.x < WIDTH) {
            chars[l.y][l.x] = ':';
            colors[l.y][l.x] = CLR_WHITE;
        }
    }

    // 4. Draw Alien Bombs
    for (const auto& b : alienBombs_) {
        if (b.active && b.y >= 0 && b.y < HEIGHT && b.x >= 0 && b.x < WIDTH) {
            chars[b.y][b.x] = '!';
            colors[b.y][b.x] = CLR_ORANGE;
        }
    }

    // 5. Draw Player Cannon
    std::string cannon = "/-^-\\ ";
    for (int i = 0; i < CANNON_WIDTH; ++i) {
        int px = cannonX_ + i;
        if (px >= 0 && px < WIDTH) {
            chars[cannonY_][px] = cannon[i];
            colors[cannonY_][px] = CLR_GOLD;
        }
    }

    // Convert buffer to output string with ANSI color codes
    std::ostringstream out;
    for (int y = 0; y < HEIGHT - 1; ++y) {
        const char* activeColor = CLR_RESET;
        for (int x = 0; x < WIDTH; ++x) {
            if (colors[y][x] != activeColor) {
                out << colors[y][x];
                activeColor = colors[y][x];
            }
            out << chars[y][x];
        }
        if (activeColor != CLR_RESET) out << CLR_RESET;
        out << "\n";
    }

    // 6. Draw HUD line at bottom matching expected output
    // e.g. "Level: 01 Score: 0000000 Lives: /-\\ /-\\"
    std::ostringstream hud;
    hud << "Level: " << std::setfill('0') << std::setw(2) << level_
        << " Score: " << std::setfill('0') << std::setw(7) << score_
        << " Lives:";

    for (int l = 1; l < lives_; ++l) {
        hud << " /-\\ ";
    }

    std::string hudStr = hud.str();
    int pad = std::max(0, (WIDTH - static_cast<int>(hudStr.size())) / 2);

    out << std::string(pad, ' ') << CLR_CRIMSON << hudStr << CLR_RESET << "\n";

    return out.str();
}

} // namespace SpaceInvaders

#ifndef _WIN32
namespace {
struct TermGuard {
    struct termios orig{};
    bool active = false;

    TermGuard() {
        if (isatty(STDIN_FILENO)) {
            if (tcgetattr(STDIN_FILENO, &orig) == 0) {
                struct termios raw = orig;
                raw.c_lflag &= ~(ECHO | ICANON);
                raw.c_cc[VMIN] = 0;
                raw.c_cc[VTIME] = 0;
                if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0) {
                    active = true;
                }
            }
        }
    }

    ~TermGuard() {
        if (active) {
            tcsetattr(STDIN_FILENO, TCSANOW, &orig);
        }
    }
};

bool checkKey_posix() {
    struct timeval tv = {0L, 0L};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) > 0;
}
} // namespace
#endif

namespace {
bool isInteractive() {
#ifdef _WIN32
    return _isatty(_fileno(stdin)) != 0 && _isatty(_fileno(stdout)) != 0;
#else
    return isatty(STDIN_FILENO) != 0 && isatty(STDOUT_FILENO) != 0;
#endif
}
} // namespace

void playSpaceInvaders() {
    using namespace SpaceInvaders;

    if (!isInteractive()) {
        return;
    }

    SpaceInvadersGame game;

    // Clear screen once and hide cursor
    std::cout << "\033[2J\033[?25l" << std::flush;

#ifndef _WIN32
    TermGuard termGuard;
#endif

    while (!game.isGameOver()) {
        // Render scene home
        std::cout << "\033[H" << game.render() << std::flush;

        // Process keyboard inputs
#ifdef _WIN32
        while (_kbhit()) {
            int ch = _getch();
            if (ch == 0 || ch == 224) {
                int arrow = _getch();
                if (arrow == 75) game.movePlayer(-2);      // Left
                else if (arrow == 77) game.movePlayer(2); // Right
                else if (arrow == 72) game.fireLaser();    // Up
            } else {
                char k = static_cast<char>(std::tolower(ch));
                if (k == 'q' || ch == 3 || ch == 26) { // 'q', Ctrl-C, Ctrl-Z (EOF)
                    std::cout << "\033[?25h\033[0m\nExiting Space Invaders...\n" << std::flush;
                    return;
                }
                if (k == 'a') game.movePlayer(-2);
                else if (k == 'd') game.movePlayer(2);
                else if (k == ' ' || k == 'w') game.fireLaser();
            }
        }
#else
        while (checkKey_posix()) {
            char ch = 0;
            ssize_t n = read(STDIN_FILENO, &ch, 1);
            if (n <= 0) {
                // EOF or closed stream - clean exit
                std::cout << "\033[?25h\033[0m" << std::flush;
                return;
            }
            if (ch == '\033') { // Escape sequence for arrow keys
                char seq[2];
                if (read(STDIN_FILENO, &seq[0], 1) <= 0) {
                    std::cout << "\033[?25h\033[0m" << std::flush;
                    return;
                }
                if (seq[0] == '[') {
                    if (read(STDIN_FILENO, &seq[1], 1) <= 0) {
                        std::cout << "\033[?25h\033[0m" << std::flush;
                        return;
                    }
                    if (seq[1] == 'D') game.movePlayer(-2);      // Left
                    else if (seq[1] == 'C') game.movePlayer(2); // Right
                    else if (seq[1] == 'A') game.fireLaser();    // Up
                }
            } else {
                char k = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                if (k == 'q' || ch == 3 || ch == 4) { // 'q', Ctrl-C, Ctrl-D (EOF)
                    std::cout << "\033[?25h\033[0m\nExiting Space Invaders...\n" << std::flush;
                    return;
                }
                if (k == 'a') game.movePlayer(-2);
                else if (k == 'd') game.movePlayer(2);
                else if (k == ' ' || k == 'w') game.fireLaser();
            }
        }
#endif

        game.tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(35));
    }

    // Final render on Game Over
    std::cout << "\033[H" << game.render() << "\n";
    std::cout << "          \033[1;31m◆ GAME OVER ◆\033[0m Final Score: "
              << game.score() << "\n\n" << std::flush;
    std::cout << "\033[?25h\033[0m" << std::flush;
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
}
