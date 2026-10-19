#include "dino.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <limits>
#include <string>
#ifndef _WIN32
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#else
#include <conio.h>
#endif

using dino::Box;

// ---------------------------------------------------------------------------
// Sprites. '#' is a lit pixel. Every row of a sprite has the same width.
// ---------------------------------------------------------------------------
namespace {

using Rows = std::vector<std::string>;

Rows concat(Rows a, const Rows& b) {
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

// Head and body of the standing dinosaur (rows 0-14). Legs are added below.
const Rows kDinoBody = {
    "..........#######.",
    ".........#########",
    ".........##.######",
    ".........#########",
    ".........#########",
    ".........#########",
    ".........######...",
    ".........#####....",
    "#........#####....",
    ".#......######....",
    "##.....#######....",
    "###...##########..",
    "#####.#########...",
    ".#############....",
    "..############....",
};
// Same body, with an X for an eye after crashing.
const Rows kDinoDeadHead = {
    "..........#######.",
    ".........#.#.#####",
    ".........##.######",
    ".........#.#.#####",
};
const Rows kDinoLegsStand = {
    "....###..###......",
    "....###..###......",
    "....####.####.....",
};
const Rows kDinoLegsA = {
    "....###..###......",
    "....###..#........",
    "....####..........",
};
const Rows kDinoLegsB = {
    "....###..###......",
    "....#....###......",
    ".........####.....",
};

const Rows kDuckBody = {
    "................#######.",
    "...............#########",
    "...............##.######",
    ".#.............#########",
    ".##..#########.######...",
    ".###.##############.....",
    "..#################.....",
    "...###############......",
};
const Rows kDuckLegsA = {
    "....###...###...........",
    "....####................",
};
const Rows kDuckLegsB = {
    "....###...###...........",
    "..........####..........",
};

const Rows kSmallCactus = {
    "..###..",
    "..###..",
    "..###.#",
    "..###.#",
    "#.###.#",
    "#.#####",
    "#####..",
    "..###..",
    "..###..",
    "..###..",
    "..###..",
};
const Rows kLargeCactus = {
    "...####...",
    "...####...",
    "...####.##",
    "...####.##",
    "...####.##",
    "##.####.##",
    "##.#######",
    "##.####...",
    "#######...",
    "...####...",
    "...####...",
    "...####...",
    "...####...",
    "...####...",
    "...####...",
};

const Rows kBirdUp = {
    ".........#.....",
    ".........##....",
    "........###....",
    ".......####....",
    "..###.#####....",
    "###############",
    "....#######....",
    "...............",
    "...............",
    "...............",
};
const Rows kBirdDown = {
    "...............",
    "...............",
    "...............",
    "...............",
    "..###.#####....",
    "###############",
    "....#######....",
    ".....#######...",
    "......######...",
    ".......####....",
};

const Rows kCloud = {
    "......####........",
    "..##########......",
    ".################.",
    "..##############..",
};

// Crescent moon, built from two circles so it stays smooth.
Rows makeMoon() {
    Rows rows;
    for (int y = 0; y < 13; ++y) {
        std::string row;
        for (int x = 0; x < 7; ++x) {
            const double dx1 = x - 6.0, dy = y - 6.0;
            const double dx2 = x - 9.0;
            const bool outer = dx1 * dx1 + dy * dy <= 6.4 * 6.4;
            const bool inner = dx2 * dx2 + dy * dy <= 6.4 * 6.4;
            row += (outer && !inner) ? '#' : '.';
        }
        rows.push_back(row);
    }
    return rows;
}

// The circular arrow shown on the game over screen.
Rows makeRestartIcon() {
    constexpr int size = 13;
    Rows rows(size, std::string(size, '.'));
    const double pi = 3.14159265358979;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const double dx = x - 6.0, dy = y - 6.0;
            const double r = std::sqrt(dx * dx + dy * dy);
            // Angle measured clockwise from "up".
            double a = std::atan2(dx, -dy);
            if (a < 0) a += 2 * pi;
            const bool gap = a > 0.30 * pi && a < 0.75 * pi;
            if (r >= 3.6 && r <= 5.7 && !gap) rows[y][x] = '#';
        }
    }
    // Arrow head at the end of the arc, pointing into the gap.
    const int hx = 9, hy = 0;
    const Rows arrow = {
        "#...",
        "###.",
        "####",
        "###.",
        "#...",
    };
    for (size_t j = 0; j < arrow.size(); ++j)
        for (size_t i = 0; i < arrow[j].size(); ++i)
            if (arrow[j][i] == '#') rows[hy + j][hx + i] = '#';
    return rows;
}

const Rows& moonSprite() {
    static const Rows moon = makeMoon();
    return moon;
}
const Rows& restartSprite() {
    static const Rows icon = makeRestartIcon();
    return icon;
}

struct DinoFrames {
    Rows stand, runA, runB, dead, duckA, duckB;
};
const DinoFrames& dinoFrames() {
    static const DinoFrames frames = [] {
        DinoFrames f;
        f.stand = concat(kDinoBody, kDinoLegsStand);
        f.runA = concat(kDinoBody, kDinoLegsA);
        f.runB = concat(kDinoBody, kDinoLegsB);
        f.dead = f.stand;
        for (size_t i = 0; i < kDinoDeadHead.size(); ++i) f.dead[i] = kDinoDeadHead[i];
        f.duckA = concat(kDuckBody, kDuckLegsA);
        f.duckB = concat(kDuckBody, kDuckLegsB);
        return f;
    }();
    return frames;
}

bool sizeIs(const Rows& rows, int w, int h) {
    if (static_cast<int>(rows.size()) != h) return false;
    for (const auto& r : rows)
        if (static_cast<int>(r.size()) != w) return false;
    return true;
}

}  // namespace

bool dinoArtIsValid() {
    const auto& f = dinoFrames();
    return sizeIs(f.stand, dino::kDinoW, dino::kDinoH) && sizeIs(f.runA, dino::kDinoW, dino::kDinoH)
           && sizeIs(f.runB, dino::kDinoW, dino::kDinoH) && sizeIs(f.dead, dino::kDinoW, dino::kDinoH)
           && sizeIs(f.duckA, dino::kDuckW, dino::kDuckH) && sizeIs(f.duckB, dino::kDuckW, dino::kDuckH)
           && sizeIs(kSmallCactus, dino::kSmallCactusW, dino::kSmallCactusH)
           && sizeIs(kLargeCactus, dino::kLargeCactusW, dino::kLargeCactusH)
           && sizeIs(kBirdUp, dino::kBirdW, dino::kBirdH) && sizeIs(kBirdDown, dino::kBirdW, dino::kBirdH)
           && sizeIs(kCloud, dino::kCloudW, dino::kCloudH);
}

// ---------------------------------------------------------------------------
// Game logic
// ---------------------------------------------------------------------------
namespace {

constexpr double kScoreCoefficient = 0.12;   // points per pixel travelled
constexpr double kAcceleration = 0.0002;     // speed gained per tick
constexpr int kClearTicks = 120;             // empty road at the start
constexpr int kNightTicks = 720;
constexpr int kStripLength = 600;
constexpr double kBirdMinSpeed = 1.7;
constexpr double kSmallGroupSpeed = 1.4;     // groups of three small cacti
constexpr double kLargeGroupSpeed = 1.5;     // groups of two large cacti

bool overlaps(const Box& a, const Box& b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

int groupWidth(DinoObstacle::Kind kind, int count) {
    if (kind == DinoObstacle::Kind::Bird) return dino::kBirdW;
    const int w = kind == DinoObstacle::Kind::SmallCactus ? dino::kSmallCactusW : dino::kLargeCactusW;
    return count * (w + 1) - 1;
}

}  // namespace

DinoGame::DinoGame(unsigned seed, int width, int highScore)
    : rng_(seed), width_(width), highScore_(highScore), moonX_(width * 0.7) {
    // The ground: a strip of little bumps that scrolls and repeats.
    strip_.assign(kStripLength, 0);
    for (int x = 0; x < kStripLength;) {
        if (randomInt(0, 99) < 18) {
            const int row = randomInt(0, 2);
            const int len = randomInt(1, 3);
            for (int i = 0; i < len; ++i) strip_[(x + i) % kStripLength] |= static_cast<std::uint8_t>(1 << row);
            x += len + 5;
        } else {
            ++x;
        }
    }
    for (int i = 0; i < 3; ++i) {
        clouds_.push_back({static_cast<double>(randomInt(10, width_ - 20)), randomInt(3, 16)});
    }
    for (int i = 0; i < 12; ++i) {
        stars_.push_back({static_cast<double>(randomInt(0, width_ - 1)), randomInt(3, 26)});
    }
}

int DinoGame::randomInt(int low, int high) {
    return std::uniform_int_distribution<int>(low, high)(rng_);
}

void DinoGame::start() {
    if (state_ == State::Ready) state_ = State::Running;
}

void DinoGame::spawnObstacle(DinoObstacle::Kind kind, double x, int count, int altitude) {
    DinoObstacle o;
    o.kind = kind;
    o.x = x;
    o.count = count;
    o.width = groupWidth(kind, count);
    switch (kind) {
        case DinoObstacle::Kind::SmallCactus: o.height = dino::kSmallCactusH; break;
        case DinoObstacle::Kind::LargeCactus: o.height = dino::kLargeCactusH; break;
        case DinoObstacle::Kind::Bird: o.height = dino::kBirdH; break;
    }
    o.y = dino::kGroundY - o.height - (kind == DinoObstacle::Kind::Bird ? altitude : 0);
    if (kind == DinoObstacle::Kind::Bird) o.speedOffset = randomInt(0, 1) ? 0.16 : -0.16;
    // Leave room to land and jump again; the gap grows with the speed.
    const double base = 0.9 * kJumpTicks * speed_ * (kind == DinoObstacle::Kind::Bird ? 1.2 : 1.0) + o.width;
    o.gap = base * (1.0 + 0.5 * randomInt(0, 1000) / 1000.0);
    obstacles_.push_back(o);
    history_.push_back(kind);
    if (history_.size() > 8) history_.erase(history_.begin());
}

void DinoGame::spawnNext() {
    DinoObstacle::Kind kind;
    for (;;) {
        const int pick = randomInt(0, 2);
        kind = pick == 0 ? DinoObstacle::Kind::SmallCactus
                         : pick == 1 ? DinoObstacle::Kind::LargeCactus : DinoObstacle::Kind::Bird;
        if (kind == DinoObstacle::Kind::Bird && speed_ < kBirdMinSpeed) continue;
        // Never more than two of the same kind in a row.
        const size_t n = history_.size();
        if (n >= 2 && history_[n - 1] == kind && history_[n - 2] == kind) continue;
        break;
    }
    int count = 1;
    if (kind == DinoObstacle::Kind::SmallCactus) {
        count = randomInt(1, speed_ >= kSmallGroupSpeed ? 3 : 2);
    } else if (kind == DinoObstacle::Kind::LargeCactus && speed_ >= kLargeGroupSpeed) {
        count = randomInt(1, 2);
    }
    const int altitudes[] = {dino::kBirdLow, dino::kBirdMid, dino::kBirdHigh};
    spawnObstacle(kind, width_, count, altitudes[randomInt(0, 2)]);
}

std::vector<Box> DinoGame::dinoBoxes() const {
    using namespace dino;
    const int h = heightAboveGround();
    if (ducking_ && h == 0) {
        const int top = kGroundY - kDuckH;
        return {{kDinoX + 15, top, 8, 4}, {kDinoX + 3, top + 4, 16, 6}};
    }
    const int top = kGroundY - h - kDinoH;
    return {{kDinoX + 10, top + 1, 7, 6}, {kDinoX + 3, top + 9, 10, 9}};
}

bool DinoGame::collides() const {
    const auto mine = dinoBoxes();
    for (const auto& o : obstacles_) {
        const int ox = static_cast<int>(std::lround(o.x));
        Box box;
        if (o.kind == DinoObstacle::Kind::Bird) {
            box = {ox + 2, o.y + 3, o.width - 4, 5};
        } else {
            box = {ox + 1, o.y + 1, o.width - 2, o.height - 1};
        }
        for (const auto& b : mine)
            if (overlaps(b, box)) return true;
    }
    return false;
}

void DinoGame::updateScenery() {
    for (auto& c : clouds_) c.x -= 0.17 * speed_;
    clouds_.erase(std::remove_if(clouds_.begin(), clouds_.end(), [](const DinoCloud& c) { return c.x < -dino::kCloudW; }),
                  clouds_.end());
    if (clouds_.size() < 6 && (clouds_.empty() || clouds_.back().x < width_ - cloudGap_)) {
        clouds_.push_back({static_cast<double>(width_), randomInt(3, 16)});
        cloudGap_ = randomInt(40, 150);
    }
    for (auto& s : stars_) {
        s.x -= 0.04 * speed_;
        if (s.x < 0) {
            s.x += width_;
            s.y = randomInt(3, 26);
        }
    }
    moonX_ -= 0.03;
    if (moonX_ < -10) moonX_ = width_ + 5;
}

void DinoGame::step(bool jump, bool duck) {
    ++ticks_;
    if (state_ == State::Crashed) return;
    if (state_ == State::Ready) {
        if (!jump) return;
        state_ = State::Running;
    }
    ++runTicks_;

    // --- the dinosaur ---
    if (!airborne()) {
        ducking_ = duck && !jump;
        dropping_ = false;
        if (jump) {
            const double half = kJumpTicks / 2.0;
            velocity_ = 2.0 * kJumpApex / half;
            height_ = 1e-6;
            ducking_ = false;
        }
    } else {
        ducking_ = false;
        dropping_ = duck;
    }
    if (airborne()) {
        const double gravity = 2.0 * kJumpApex / ((kJumpTicks / 2.0) * (kJumpTicks / 2.0));
        if (dropping_) {
            // Holding down while in the air makes you fall fast.
            if (velocity_ > -1.0) velocity_ = -1.0;
            height_ += velocity_ * 3.0;
        } else {
            height_ += velocity_;
        }
        velocity_ -= gravity;
        if (height_ <= 0.0) {
            height_ = 0.0;
            velocity_ = 0.0;
            dropping_ = false;
        }
    }

    // --- speed and score ---
    speed_ = std::min(kMaxSpeed, speed_ + kAcceleration);
    distance_ += speed_;
    const int before = score_;
    score_ = static_cast<int>(distance_ * kScoreCoefficient);
    if (score_ / 100 > before / 100) flashTicks_ = 90;
    if (flashTicks_ > 0) --flashTicks_;
    if (nightTicks_ > 0) --nightTicks_;
    if (score_ >= nextNightScore_) {
        nightTicks_ = kNightTicks;
        nextNightScore_ += 700;
    }
    groundOffset_ += speed_;
    if (groundOffset_ >= kStripLength) groundOffset_ -= kStripLength;
    updateScenery();

    // --- obstacles ---
    for (auto& o : obstacles_) {
        o.x -= speed_ + o.speedOffset;
        o.frame = static_cast<int>((runTicks_ / 10) % 2);
    }
    obstacles_.erase(std::remove_if(obstacles_.begin(), obstacles_.end(),
                                    [](const DinoObstacle& o) { return o.x + o.width < 0; }),
                     obstacles_.end());
    if (runTicks_ > kClearTicks) {
        if (obstacles_.empty()) {
            spawnNext();
        } else {
            const auto& last = obstacles_.back();
            if (last.x + last.width + last.gap < width_) spawnNext();
        }
    }

    if (collides()) {
        state_ = State::Crashed;
        highScore_ = std::max(highScore_, score_);
    }
}

// ---------------------------------------------------------------------------
// Terminal handling
// ---------------------------------------------------------------------------
namespace {

class TerminalMode {
public:
    bool enable() {
#ifndef _WIN32
        if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
            std::cerr << "Dinosaur Run needs an interactive terminal.\n";
            return false;
        }
        if (tcgetattr(STDIN_FILENO, &original_) != 0) {
            std::cerr << "Could not read terminal settings.\n";
            return false;
        }
        termios raw = original_;
        // No line buffering, no echo, and Ctrl-C arrives as a key so we can clean up.
        raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO | ISIG));
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
            std::cerr << "Could not enable real-time keyboard input.\n";
            return false;
        }
#endif
        active_ = true;
        // Alternate screen, hide cursor, no line wrapping.
        std::cout << "\x1b[?1049h\x1b[?25l\x1b[?7l\x1b[2J";
        std::cout.flush();
        return true;
    }

    ~TerminalMode() {
        if (active_) {
#ifndef _WIN32
            tcflush(STDIN_FILENO, TCIFLUSH);
            tcsetattr(STDIN_FILENO, TCSANOW, &original_);
#endif
            std::cout << "\x1b[0m\x1b[?7h\x1b[?25h\x1b[?1049l";
            std::cout.flush();
        }
    }

    TerminalMode() = default;
    TerminalMode(const TerminalMode&) = delete;
    TerminalMode& operator=(const TerminalMode&) = delete;

private:
#ifndef _WIN32
    termios original_{};
#endif
    bool active_ = false;
};

struct Keys {
    bool jump = false;
    bool duck = false;
    bool restart = false;
    bool quit = false;
};

// Reads whatever bytes are waiting (up to `timeout`) and turns them into keys.
bool readKeys(std::chrono::microseconds timeout, Keys& keys) {
#ifndef _WIN32
    fd_set input;
    FD_ZERO(&input);
    FD_SET(STDIN_FILENO, &input);
    timeval tv{};
    const auto us = std::max<long long>(0, timeout.count());
    tv.tv_sec = static_cast<time_t>(us / 1000000);
    tv.tv_usec = static_cast<suseconds_t>(us % 1000000);
    const int ready = select(STDIN_FILENO + 1, &input, nullptr, nullptr, &tv);
    if (ready < 0) return false;
    if (ready == 0) return true;

    char buf[64];
    const ssize_t n = read(STDIN_FILENO, buf, sizeof buf);
    if (n < 0) return false;
    if (n == 0) {
        keys.quit = true;  // stdin closed
        return true;
    }
    for (ssize_t i = 0; i < n; ++i) {
        const char c = buf[i];
        if (c == '\x1b') {
            if (i + 2 < n && (buf[i + 1] == '[' || buf[i + 1] == 'O')) {
                const char arrow = buf[i + 2];
                if (arrow == 'A') keys.jump = true;
                if (arrow == 'B') keys.duck = true;
                i += 2;
            } else if (i + 1 >= n) {
                keys.quit = true;  // a lone Escape
            }
            continue;
        }
        switch (c) {
            case ' ': case 'w': case 'W': case 'j': case 'J': keys.jump = true; break;
            case 's': case 'S': keys.duck = true; break;
            case 'r': case 'R': case '\r': case '\n': keys.restart = true; break;
            case 'q': case 'Q': case 3: keys.quit = true; break;
            default: break;
        }
    }
    return true;
#else
    (void)timeout;
    while (_kbhit()) {
        int ch = _getch();
        if (ch == 0 || ch == 224) {
            int arrow = _getch();
            if (arrow == 72) keys.jump = true;
            else if (arrow == 80) keys.duck = true;
        } else {
            char c = static_cast<char>(ch);
            switch (c) {
                case ' ': case 'w': case 'W': case 'j': case 'J': keys.jump = true; break;
                case 's': case 'S': keys.duck = true; break;
                case 'r': case 'R': case '\r': case '\n': keys.restart = true; break;
                case 'q': case 'Q': case 3: keys.quit = true; break;
                default: break;
            }
        }
    }
    return true;
#endif
}

// ---------------------------------------------------------------------------
// Rendering. Pixels are drawn into a buffer, then folded two-to-a-cell with
// half-block characters, using the 256-colour palette so it works everywhere.
// ---------------------------------------------------------------------------
enum Pixel : std::uint8_t { kEmpty = 0, kInk = 1, kLight = 2 };

struct Palette {
    int bg, ink, light, dim;
};
constexpr Palette kDay{255, 240, 252, 245};
constexpr Palette kNight{233, 252, 240, 244};

struct Canvas {
    int w, h;
    std::vector<std::uint8_t> px;
    Canvas(int width, int height) : w(width), h(height), px(static_cast<size_t>(width) * height, kEmpty) {}

    void set(int x, int y, std::uint8_t v) {
        if (x >= 0 && x < w && y >= 0 && y < h) px[static_cast<size_t>(y) * w + x] = v;
    }
    std::uint8_t get(int x, int y) const { return px[static_cast<size_t>(y) * w + x]; }

    void blit(const Rows& sprite, int x, int y, std::uint8_t color = kInk) {
        for (size_t j = 0; j < sprite.size(); ++j)
            for (size_t i = 0; i < sprite[j].size(); ++i)
                if (sprite[j][i] == '#') set(x + static_cast<int>(i), y + static_cast<int>(j), color);
    }
};

struct Cell {
    std::string glyph = " ";
    int fg = 0;
    int bg = 0;
};

void putText(std::vector<Cell>& cells, int cols, int row, int col, const std::string& text, int fg, int bg) {
    for (size_t i = 0; i < text.size(); ++i) {
        const int c = col + static_cast<int>(i);
        if (c < 0 || c >= cols) continue;
        Cell& cell = cells[static_cast<size_t>(row) * cols + c];
        cell.glyph = std::string(1, text[i]);
        cell.fg = fg;
        cell.bg = bg;
    }
}

void putCentered(std::vector<Cell>& cells, int cols, int row, const std::string& text, int fg, int bg) {
    putText(cells, cols, row, (cols - static_cast<int>(text.size())) / 2, text, fg, bg);
}

std::string spaced(const std::string& text) {
    std::string out;
    for (size_t i = 0; i < text.size(); ++i) {
        if (i) out += ' ';
        out += text[i];
    }
    return out;
}

std::string fiveDigits(int n) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%05d", std::min(n, 99999));
    return buf;
}

const Rows& dinoSprite(const DinoGame& game) {
    const auto& f = dinoFrames();
    if (game.crashed()) return game.ducking() && !game.airborne() ? f.duckA : f.dead;
    if (game.ducking() && !game.airborne()) return game.runFrame() ? f.duckB : f.duckA;
    if (game.airborne() || game.ready()) return f.stand;
    return game.runFrame() ? f.runB : f.runA;
}

std::string render(const DinoGame& game, int termCols, int termRows) {
    const int W = game.width();
    const int H = dino::kFieldHeight;
    const Palette pal = game.night() ? kNight : kDay;
    Canvas canvas(W, H);

    if (game.night()) {
        for (const auto& s : game.stars()) canvas.set(static_cast<int>(s.x), s.y, kLight);
        canvas.blit(moonSprite(), static_cast<int>(game.moonX()), 6, kInk);
    }
    for (const auto& c : game.clouds()) {
        canvas.blit(kCloud, static_cast<int>(c.x), c.y, kLight);
    }

    // Ground line and bumps.
    const auto& strip = game.groundStrip();
    const int offset = static_cast<int>(game.groundOffset());
    for (int x = 0; x < W; ++x) {
        canvas.set(x, dino::kGroundY, kInk);
        const std::uint8_t bits = strip[(offset + x) % strip.size()];
        for (int row = 0; row < 3; ++row)
            if (bits & (1 << row)) canvas.set(x, dino::kGroundY + 1 + row, kInk);
    }

    for (const auto& o : game.obstacles()) {
        const int x = static_cast<int>(std::lround(o.x));
        if (o.kind == DinoObstacle::Kind::Bird) {
            canvas.blit(o.frame ? kBirdDown : kBirdUp, x, o.y);
        } else {
            const bool small = o.kind == DinoObstacle::Kind::SmallCactus;
            const int w = small ? dino::kSmallCactusW : dino::kLargeCactusW;
            for (int i = 0; i < o.count; ++i)
                canvas.blit(small ? kSmallCactus : kLargeCactus, x + i * (w + 1), o.y);
        }
    }

    const Rows& dinoRows = dinoSprite(game);
    const int dinoH = static_cast<int>(dinoRows.size());
    const int dinoTop = dino::kGroundY - game.heightAboveGround() - dinoH;
    canvas.blit(dinoRows, dino::kDinoX, dinoTop);

    if (game.crashed()) {
        const Rows& icon = restartSprite();
        canvas.blit(icon, (W - static_cast<int>(icon[0].size())) / 2, 21);
    }

    // Fold pixel pairs into terminal cells.
    const int rows = H / 2;
    std::vector<Cell> cells(static_cast<size_t>(W) * rows);
    auto colorOf = [&](std::uint8_t p) { return p == kInk ? pal.ink : p == kLight ? pal.light : pal.bg; };
    for (int r = 0; r < rows; ++r) {
        for (int x = 0; x < W; ++x) {
            const int top = colorOf(canvas.get(x, 2 * r));
            const int bottom = colorOf(canvas.get(x, 2 * r + 1));
            Cell& cell = cells[static_cast<size_t>(r) * W + x];
            if (top == bottom) {
                cell.glyph = " ";
                cell.bg = top;
                cell.fg = top;
            } else {
                cell.glyph = "\xE2\x96\x80";  // upper half block
                cell.fg = top;
                cell.bg = bottom;
            }
        }
    }

    // Text on top: score, hints, game over.
    std::string score = fiveDigits(game.score());
    std::string hi = "HI " + fiveDigits(game.highScore());
    if (!game.scoreVisible()) score = "     ";
    const std::string line = (game.highScore() > 0 ? hi + "  " : std::string()) + score;
    putText(cells, W, 0, W - static_cast<int>(line.size()) - 1, line, pal.ink, pal.bg);
    if (game.highScore() > 0) {
        putText(cells, W, 0, W - static_cast<int>(line.size()) - 1, hi, pal.dim, pal.bg);
    }
    if (game.ready()) {
        putCentered(cells, W, 13, "SPACE or UP to jump   DOWN to duck   Q to quit", pal.dim, pal.bg);
    }
    if (game.crashed()) {
        putCentered(cells, W, 8, spaced("GAME") + "  " + spaced("OVER"), pal.ink, pal.bg);
        putCentered(cells, W, 18, "SPACE to restart   Q to quit", pal.dim, pal.bg);
    }

    // Emit with as few colour changes as possible.
    const int left = std::max(0, (termCols - W) / 2);
    const int top = std::max(0, (termRows - rows) / 2);
    std::string out;
    out.reserve(static_cast<size_t>(W) * rows * 4);
    int curFg = -1, curBg = -1;
    char buf[48];
    for (int r = 0; r < rows; ++r) {
        std::snprintf(buf, sizeof buf, "\x1b[%d;%dH", top + r + 1, left + 1);
        out += buf;
        for (int x = 0; x < W; ++x) {
            const Cell& cell = cells[static_cast<size_t>(r) * W + x];
            if (cell.glyph == " " && cell.bg != curBg) {
                std::snprintf(buf, sizeof buf, "\x1b[48;5;%dm", cell.bg);
                out += buf;
                curBg = cell.bg;
            } else if (cell.glyph != " ") {
                if (cell.fg != curFg) {
                    std::snprintf(buf, sizeof buf, "\x1b[38;5;%dm", cell.fg);
                    out += buf;
                    curFg = cell.fg;
                }
                if (cell.bg != curBg) {
                    std::snprintf(buf, sizeof buf, "\x1b[48;5;%dm", cell.bg);
                    out += buf;
                    curBg = cell.bg;
                }
            }
            out += cell.glyph;
        }
    }
    out += "\x1b[0m";
    return out;
}

void terminalSize(int& cols, int& rows) {
#ifndef _WIN32
    winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        cols = ws.ws_col;
        rows = ws.ws_row;
        return;
    }
#endif
    cols = 100;
    rows = 24;
}

int g_highScore = 0;  // best score this session, kept across visits to the menu

}  // namespace

void playDino() {
    // The menu leaves the rest of the line (the Enter key) unread.
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    int cols = 0, rows = 0;
    terminalSize(cols, rows);
    constexpr int kRows = dino::kFieldHeight / 2;
    constexpr int kMinCols = 60;
    constexpr int kMaxCols = 100;
    if (cols < kMinCols || rows < kRows) {
        std::cerr << "Dinosaur Run needs a terminal of at least " << kMinCols << "x" << kRows
                  << " characters (yours is " << cols << "x" << rows << ").\n";
        return;
    }
    const int width = std::min(cols, kMaxCols);

    TerminalMode terminal;
    if (!terminal.enable()) return;

    using Clock = std::chrono::steady_clock;
    const auto tick = std::chrono::microseconds(1000000 / dino::kTicksPerSecond);
    const auto duckHold = std::chrono::milliseconds(550);  // terminals send no key-up events
    const auto restartDelay = std::chrono::milliseconds(600);

    std::random_device device;
    DinoGame game(device(), width, g_highScore);
    auto nextTick = Clock::now() + tick;
    auto duckUntil = Clock::now();
    auto crashedAt = Clock::now();
    bool wasCrashed = false;
    bool jumpQueued = false;

    std::cout << render(game, cols, rows);
    std::cout.flush();

    while (true) {
        Keys keys;
        const auto now = Clock::now();
        const auto wait = std::chrono::duration_cast<std::chrono::microseconds>(nextTick - now);
        if (!readKeys(wait, keys)) break;
        if (keys.quit) break;

        const auto afterRead = Clock::now();
        if (keys.duck) duckUntil = afterRead + duckHold;
        if (keys.jump) {
            jumpQueued = true;
            duckUntil = afterRead;  // jumping cancels a duck
        }

        if (game.crashed()) {
            if (!wasCrashed) {
                wasCrashed = true;
                crashedAt = afterRead;
                g_highScore = game.highScore();
            }
            if ((keys.jump || keys.restart) && afterRead - crashedAt > restartDelay) {
                game = DinoGame(device(), width, g_highScore);
                game.start();
                wasCrashed = false;
                jumpQueued = false;
                nextTick = afterRead + tick;
            }
        } else if (game.ready() && keys.restart) {
            jumpQueued = true;
        }

        if (afterRead >= nextTick) {
            game.step(jumpQueued, afterRead < duckUntil);
            jumpQueued = false;
            nextTick += tick;
            if (afterRead - nextTick > 5 * tick) nextTick = afterRead + tick;  // fell behind
            std::cout << render(game, cols, rows);
            std::cout.flush();
        }
    }
    g_highScore = std::max(g_highScore, game.highScore());
}
