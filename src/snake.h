#pragma once
#include <deque>
#include <random>
#include <string>
#include <vector>

enum class SnakeDirection { Up, Down, Left, Right };

struct Point {
    int x = 0;
    int y = 0;
    bool operator==(const Point& other) const { return x == other.x && y == other.y; }
    bool operator!=(const Point& other) const { return !(*this == other); }
};

class SnakeGame {
public:
    static constexpr int WIDTH = 20;
    static constexpr int HEIGHT = 10;

    explicit SnakeGame(unsigned seed = std::random_device{}(),
                       int width = WIDTH,
                       int height = HEIGHT,
                       int numFoods = 1);

    // Advance the snake by 1 step in the current direction.
    // Returns false if collision occurs (game over).
    bool step();

    // Change direction (ignored if opposite of current direction).
    void setDirection(SnakeDirection dir);

    SnakeDirection direction() const { return currentDir_; }
    bool isGameOver() const { return gameOver_; }
    bool hasWon() const { return won_; }
    int score() const { return score_; }
    int width() const { return width_; }
    int height() const { return height_; }

    const std::deque<Point>& body() const { return body_; }
    Point food() const { return foods_.empty() ? Point{} : foods_.front(); }
    const std::vector<Point>& foods() const { return foods_; }

    // Helpers used for testing and deterministic setups
    void setFood(Point p) { foods_ = {p}; }
    void setFoods(const std::vector<Point>& foods) { foods_ = foods; }
    void setBody(const std::deque<Point>& b) { body_ = b; }

    void print() const;
    void printArcade(int hiScore, int speedLevel) const;

private:
    void spawnFood();

    int width_ = WIDTH;
    int height_ = HEIGHT;
    int targetFoodCount_ = 1;
    std::deque<Point> body_;  // Index 0 is the head, back is the tail
    SnakeDirection currentDir_ = SnakeDirection::Right;
    std::vector<Point> foods_;
    int score_ = 0;
    bool gameOver_ = false;
    bool won_ = false;
    std::mt19937 rng_;
};

int getSnakeSessionHiScore();
void setSnakeSessionHiScore(int score);

void playSnake();
