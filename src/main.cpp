#include <iostream>
#include <limits>
#include <string>

#include "intro.h"

int main() {
    Borderland::initConsole();
    const auto& trials = Borderland::getGameTrials();
    Borderland::playIntro(trials);

    int currentPage = 0;
    const int quitOption = static_cast<int>(trials.size()) + 1;
    const int totalPages = Borderland::getPageCount(trials.size());

    while (true) {
        Borderland::printMenu(trials, currentPage);
        int choice = 0;

        if (!(std::cin >> choice)) {
            if (std::cin.eof()) {
                return 0;
            }

            std::cin.clear();
            std::cin.ignore(
                std::numeric_limits<std::streamsize>::max(), '\n');

            Borderland::printError(trials.size());
            continue;
        }

        if (choice == 0 && totalPages > 1) {
            currentPage = (currentPage + 1) % totalPages;
            continue;
        }

        if (choice >= 1 && choice <= static_cast<int>(trials.size())) {
            trials[choice - 1].play();
        } else if (choice == quitOption) {
            Borderland::printExit();
            return 0;
        } else {
            Borderland::printError(trials.size());
        }
    }
}
