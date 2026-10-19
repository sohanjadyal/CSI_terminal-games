#pragma once

#include <array>
#include <random>
#include <string>
#include <vector>

namespace SpaceInvaders {

constexpr int WIDTH = 60;
constexpr int HEIGHT = 22;

constexpr int ALIEN_ROWS = 5;
constexpr int ALIEN_COLS = 10;
constexpr int ALIEN_WIDTH = 3;
constexpr int BUNKER_COUNT = 4;
constexpr int BUNKER_WIDTH = 7;
constexpr int BUNKER_HEIGHT = 4;
constexpr int CANNON_WIDTH = 5;

enum class AlienTier {
    Top,     // Crimson, 30 pts
    Middle,  // Green, 20 pts
    Bottom   // Purple, 10 pts
};

struct Alien {
    int row = 0;
    int col = 0;
    AlienTier tier = AlienTier::Bottom;
    bool alive = true;
};

struct Projectile {
    int x = 0;
    int y = 0;
    bool active = false;
};

class SpaceInvadersGame {
public:
    explicit SpaceInvadersGame(unsigned seed = 1337);

    // Moves player cannon by deltaX (-1 or +1)
    void movePlayer(int deltaX);

    // Fires player laser from cannon center
    bool fireLaser();

    // Advances the game simulation by one tick
    void tick();

    // State queries
    bool isGameOver() const { return gameOver_; }
    bool isWon() const { return won_; }
    int score() const { return score_; }
    int lives() const { return lives_; }
    int level() const { return level_; }
    int cannonX() const { return cannonX_; }
    int alienFleetX() const { return fleetX_; }
    int alienFleetY() const { return fleetY_; }
    int alienDirection() const { return fleetDir_; }
    int aliveAlienCount() const;

    // Direct access for testing
    const std::vector<Alien>& aliens() const { return aliens_; }
    const std::vector<Projectile>& playerLasers() const { return playerLasers_; }
    const std::vector<Projectile>& alienBombs() const { return alienBombs_; }
    bool isBunkerBlockSolid(int bunkerIdx, int row, int col) const;
    void killAlienForTesting(int index);
    void setCannonXForTesting(int x) { cannonX_ = x; }
    void setFleetPosForTesting(int x, int y, int dir) { fleetX_ = x; fleetY_ = y; fleetDir_ = dir; }
    void addLaserForTesting(int x, int y);
    void addBombForTesting(int x, int y);
    void damageBunkerForTesting(int bunkerIdx, int row, int col);
    void setLivesForTesting(int l) { lives_ = l; }

    // Renders the board into an ASCII string buffer with ANSI color codes
    std::string render() const;

private:
    void initWave();
    void initBunkers();
    void updateAliens();
    void updateLasers();
    void updateBombs();
    void spawnAlienBomb();
    void checkCollisions();

    int cannonX_ = (WIDTH - CANNON_WIDTH) / 2;
    int cannonY_ = 20;
    int lives_ = 3;
    int score_ = 0;
    int level_ = 1;
    bool gameOver_ = false;
    bool won_ = false;

    // Alien fleet state
    int fleetX_ = 4;
    int fleetY_ = 2;
    int fleetDir_ = 1;
    int fleetTickCount_ = 0;
    int animFrame_ = 0;
    std::vector<Alien> aliens_;

    // Projectiles
    std::vector<Projectile> playerLasers_;
    std::vector<Projectile> alienBombs_;

    // 4 Bunkers, each 4 rows x 7 cols
    // Bunker shape:
    //   ###  
    //  ##### 
    // #######
    // ##   ##
    std::array<std::array<std::array<bool, BUNKER_WIDTH>, BUNKER_HEIGHT>, BUNKER_COUNT> bunkers_{};

    int bombDropCooldown_ = 0;
    std::mt19937 rng_;
};

}  // namespace SpaceInvaders

void playSpaceInvaders();
