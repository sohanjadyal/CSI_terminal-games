#ifndef INTRO_H
#define INTRO_H

#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace Borderland {

struct GameTrial {
    int id;
    std::string title;       // Card header, e.g. "2048", "TIC-TAC", "HANGMAN", "SNAKE", "DINO"
    std::string displayName; // Full display name, e.g. "2048", "Tic-tac-toe", "Hangman", "Snake", "Dinosaur Run"
    std::string suit;        // "♦", "♥", "♣", "♠"
    std::string trialType;   // "Logic", "Strategy", "Words", "Reflexes", "Survival"
    std::string color;       // ANSI color code
    std::function<void()> play;
};

// Initializes console for UTF-8 and ANSI sequences
void initConsole();

// Returns the global registry of games
const std::vector<GameTrial>& getGameTrials();

// Computes the total number of pages given trial count and page size (default 8)
int getPageCount(size_t numTrials, int pageSize = 8);

// Plays the card deal and 3D flip animation for any registered list of games
void playIntro(const std::vector<GameTrial>& trials, bool force = false);

// Renders the responsive card grid menu (auto-wrapping rows, pagination if > 8)
void printMenu(const std::vector<GameTrial>& trials, int page = 0, std::ostream& out = std::cout);

// Displays the themed exit message
void printExit(std::ostream& out = std::cout);

// Displays an invalid choice error message matching README spec
void printError(size_t numGames, std::ostream& out = std::cout);

}  // namespace Borderland

#endif  // INTRO_H
