#include <iostream>

#include "game2048.h"
#include "hangman.h"
#include "snake.h"
#include "tictactoe.h"

int main() {
    while (true) {
        std::cout << "\n=== Terminal Games ===\n"
                  << "1. 2048\n"
                  << "2. Tic-tac-toe (vs computer)\n"
                  << "3. Hangman\n"
                  << "4. Snake\n"
                  << "5. Quit\n"
                  << "Pick a game: ";
        int choice = 0;
        std::cin >> choice;
        switch (choice) {
            case 1: play2048(); break;
            case 2: playTicTacToe(); break;
            case 3: playHangman(); break;
            case 4: playSnake(); break;
            case 5: std::cout << "Bye!\n"; return 0;
            default: std::cout << "Please pick 1, 2, 3, 4 or 5.\n";
        }
    }
}
