#pragma once
#include <array>

using Board = std::array<char, 9>;  // squares 0-8, each 'X', 'O' or ' '

char winner(const Board& board); 
bool isFull(const Board& board);
int state(const Board& board); // calculating the state of the board (3^9 possible states)
int max(int a, int b);
int chooseMove(Board board, char me, char opponent); // minimax algorithm applies here
void resetMemo();
int computerMove(const Board& board, char me, char opponent);
std::array<int, 3> getWinningLine(const Board& board);
void playTicTacToe();

int getTicTacToePlayerWins();
int getTicTacToeAiWins();
int getTicTacToeDraws();
void recordTicTacToeResult(char winner);
void resetTicTacToeScores();
