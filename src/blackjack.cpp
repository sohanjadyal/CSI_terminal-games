#include "blackjack.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#ifndef _WIN32
#include <sys/select.h>
#include <termios.h>
#include <thread>
#include <unistd.h>
#else
#include <conio.h>
#include <io.h>
#include <thread>
inline int isatty(int fd) { return _isatty(fd); }
#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif
#endif

namespace {

constexpr const char* kReset = "\033[0m";
constexpr const char* kWhite = "\033[97m";
constexpr const char* kRed = "\033[91m";
constexpr const char* kBlue = "\033[44;97m";
constexpr const char* kChipRed = "\033[91m";
constexpr const char* kChipBlue = "\033[94m";
constexpr const char* kChipCyan = "\033[96m";
constexpr const char* kChipGreen = "\033[92m";
constexpr const char* kYellow = "\033[93m";
constexpr const char* kGreen = "\033[92m";
constexpr const char* kCyan = "\033[96m";
constexpr std::array<int, 4> kChipValues = {10, 25, 50, 100};
constexpr std::array<const char*, 4> kChipColors = {
    kChipRed, kChipBlue, kChipCyan, kChipGreen
};
constexpr std::array<int, 4> kChipColumns = {1, 18, 35, 52};

std::string styled(const std::string& text, const char* color, bool enabled = true) {
    return enabled ? std::string(color) + text + kReset : text;
}

std::string highlighted(std::string text) {
    std::string result = "\033[7m";
    std::size_t position = 0;
    while (true) {
        const std::size_t reset = text.find(kReset, position);
        if (reset == std::string::npos) {
            result += text.substr(position);
            break;
        }
        result += text.substr(position, reset - position);
        result += std::string(kReset) + "\033[7m";
        position = reset + std::char_traits<char>::length(kReset);
    }
    result += kReset;
    return result;
}

std::string border(const std::string& text, bool color) {
    return styled(text, kWhite, color);
}

std::string money(int cents) {
    return "$" + std::to_string(cents / 100) + (cents % 100 < 10 ? ".0" : ".")
           + std::to_string(cents % 100);
}

std::string trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

bool parsePositiveInteger(const std::string& text, int& value) {
    const std::string cleaned = trim(text);
    if (cleaned.empty()) return false;
    int parsed = 0;
    const auto result = std::from_chars(cleaned.data(), cleaned.data() + cleaned.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != cleaned.data() + cleaned.size() || parsed <= 0)
        return false;
    value = parsed;
    return true;
}

void clearScreen() {
    std::cout << "\033[2J\033[H";
}

class MouseTerminal {
public:
    MouseTerminal() = default;

    bool enable(bool alternateScreen = true) {
#ifndef _WIN32
        if (tcgetattr(STDIN_FILENO, &original_) != 0) {
            std::cerr << "Could not read terminal settings for chip selection.\n";
            return false;
        }
        termios input = original_;
        input.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
        input.c_cc[VMIN] = 1;
        input.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSANOW, &input) != 0) {
            std::cerr << "Could not enable mouse chip selection.\n";
            return false;
        }
#endif
        active_ = true;
        alternateScreen_ = alternateScreen;
        if (alternateScreen_) std::cout << "\033[?1049h";
        std::cout << "\033[?25l\033[?1000h\033[?1003h\033[?1006h";
        std::cout.flush();
        return true;
    }

    ~MouseTerminal() {
        if (!active_) return;
        std::cout << "\033[?1006l\033[?1003l\033[?1000l\033[?25h";
        if (alternateScreen_) std::cout << "\033[?1049l";
        std::cout.flush();
#ifndef _WIN32
        tcsetattr(STDIN_FILENO, TCSANOW, &original_);
#endif
    }

    MouseTerminal(const MouseTerminal&) = delete;
    MouseTerminal& operator=(const MouseTerminal&) = delete;

private:
#ifndef _WIN32
    termios original_{};
#endif
    bool active_ = false;
    bool alternateScreen_ = false;
};

struct InputEvent {
    enum class Type { Timeout, Key, Mouse, Error };
    Type type = Type::Timeout;
    char key = '\0';
    int button = 0;
    int column = 0;
    int row = 0;
    bool motion = false;
    bool pressed = false;
};

bool waitForInput(int milliseconds) {
#ifndef _WIN32
    fd_set input;
    FD_ZERO(&input);
    FD_SET(STDIN_FILENO, &input);
    timeval timeout{};
    timeout.tv_sec = milliseconds / 1000;
    timeout.tv_usec = (milliseconds % 1000) * 1000;
    const int ready = select(STDIN_FILENO + 1, &input, nullptr, nullptr, &timeout);
    if (ready < 0) {
        std::cerr << "Could not wait for terminal input.\n";
        return false;
    }
    return ready > 0;
#else
    for (int i = 0; i < milliseconds / 10; ++i) {
        if (_kbhit()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return _kbhit() != 0;
#endif
}

InputEvent readTerminalEvent() {
    InputEvent event;
#ifndef _WIN32
    if (!waitForInput(100)) {
        event.type = InputEvent::Type::Timeout;
        return event;
    }

    char first = '\0';
    if (read(STDIN_FILENO, &first, 1) != 1) {
        event.type = InputEvent::Type::Error;
        return event;
    }
    if (first != '\033') {
        event.type = InputEvent::Type::Key;
        event.key = first;
        return event;
    }

    std::string sequence;
    bool finished = false;
    for (int count = 0; count < 64 && !finished; ++count) {
        if (!waitForInput(20)) break;
        char next = '\0';
        if (read(STDIN_FILENO, &next, 1) != 1) break;
        sequence.push_back(next);
        finished = next == 'M' || next == 'm';
    }

    if (sequence.size() >= 6 && sequence[0] == '[' && sequence[1] == '<'
        && (sequence.back() == 'M' || sequence.back() == 'm')) {
        int button = 0, column = 0, row = 0;
        if (std::sscanf(sequence.c_str() + 2, "%d;%d;%d", &button, &column, &row) == 3) {
            event.type = InputEvent::Type::Mouse;
            event.button = button;
            event.column = column;
            event.row = row;
            event.motion = (button & 32) != 0;
            event.pressed = sequence.back() == 'M' && !event.motion;
            return event;
        }
    }
    event.type = InputEvent::Type::Timeout;
    return event;
#else
    if (_kbhit()) {
        int ch = _getch();
        event.type = InputEvent::Type::Key;
        event.key = static_cast<char>(ch);
        return event;
    }
    event.type = InputEvent::Type::Timeout;
    return event;
#endif
}

std::array<std::string, 7> cardBack(bool color) {
    const std::array<std::string, 5> pattern = {
        " ◇ ◇ ◇ ◇ ",
        "  ◇ ◇ ◇  ",
        " ◇ ◇ ◇ ◇ ",
        "  ◇ ◇ ◇  ",
        " ◇ ◇ ◇ ◇ "
    };

    std::array<std::string, 7> colored;
    colored.front() = border("┌─────────┐", color);
    colored.back() = border("└─────────┘", color);
    for (std::size_t i = 0; i < pattern.size(); ++i) {
        colored[i + 1] = border("│", color) + styled(pattern[i], kBlue, color)
                     + border("│", color);
    }
    if (!color) {
        for (std::size_t i = 0; i < pattern.size(); ++i)
            colored[i + 1] = "│" + pattern[i] + "│";
    }
    return colored;
}

void renderHand(const std::string& label, const Hand& hand, bool hideHoleCard) {
    std::cout << label;
    if (!hideHoleCard) {
        std::cout << " (" << hand.total() << (hand.isSoft() ? " soft" : "") << ")";
    }
    std::cout << '\n';

    std::vector<std::array<std::string, 7>> cards;
    for (std::size_t i = 0; i < hand.cards().size(); ++i) {
        if (hideHoleCard && i == 1) cards.push_back(Card::renderBack());
        else cards.push_back(hand.cards()[i].render());
    }
    for (std::size_t row = 0; row < 7; ++row) {
        for (std::size_t column = 0; column < cards.size(); ++column) {
            if (column != 0) std::cout << "  ";
            std::cout << cards[column][row];
        }
        std::cout << '\n';
    }
}

void drawTable(const BlackjackGame& game) {
    clearScreen();
    std::cout << styled("  ♠  BLACKJACK  ♣\n", kCyan)
              << "  Bankroll: " << styled(money(game.bankrollCents()), kYellow)
              << "    Wager: " << styled(money(game.wagerCents()), kYellow) << "\n\n";
    if (game.result() == BlackjackResult::NotStarted) {
        std::cout << "Six-deck shoe  •  Dealer stands on every 17\n"
                  << "Natural Blackjack pays 3:2  •  Minimum wager $10\n\n"
                  << "Choose a wager to begin. Press Q to return to the menu.\n";
        return;
    }
    renderHand("DEALER", game.dealerHand(), !game.holeCardRevealed());
    std::cout << '\n';
    renderHand("YOU", game.playerHand(), false);
    std::cout << '\n';
}

const char* resultText(BlackjackResult result) {
    switch (result) {
        case BlackjackResult::Blackjack: return "Blackjack! You win 3:2.";
        case BlackjackResult::DealerBlackjack: return "Dealer has Blackjack. You lose.";
        case BlackjackResult::PlayerWin: return "You win!";
        case BlackjackResult::DealerWin: return "Dealer wins.";
        case BlackjackResult::Push: return "Push — your wager is returned.";
        case BlackjackResult::PlayerBust: return "Bust! You lose.";
        case BlackjackResult::NotStarted:
        case BlackjackResult::InProgress: return "";
    }
    return "";
}

const char* resultColor(BlackjackResult result) {
    switch (result) {
        case BlackjackResult::Blackjack:
        case BlackjackResult::PlayerWin: return kGreen;
        case BlackjackResult::DealerBlackjack:
        case BlackjackResult::DealerWin:
        case BlackjackResult::PlayerBust: return kRed;
        case BlackjackResult::Push: return kCyan;
        case BlackjackResult::NotStarted:
        case BlackjackResult::InProgress: return kWhite;
    }
    return kWhite;
}

bool askForBet(const BlackjackGame& game, int& betDollars) {
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        while (true) {
            std::cout << "Enter your wager in whole dollars ($10–"
                      << game.bankrollCents() / 100 << "), or Q to leave: ";
            std::string line;
            if (!std::getline(std::cin, line)) return false;
            const std::string cleaned = trim(line);
            if (cleaned == "q" || cleaned == "Q") return false;

            int wager = 0;
            if (!parsePositiveInteger(cleaned, wager)) {
                std::cout << styled("Enter a whole-number wager.\n", kRed);
                continue;
            }
            if (wager < BlackjackGame::kMinimumBetDollars) {
                std::cout << styled("The minimum wager is $10.\n", kRed);
                continue;
            }
            if (wager > game.bankrollCents() / 100) {
                std::cout << styled("That wager exceeds your bankroll.\n", kRed);
                continue;
            }
            betDollars = wager;
            return true;
        }
    }

    MouseTerminal terminal;
    if (!terminal.enable()) return false;

    int wager = 0;
    int hovered = -1;
    std::vector<int> selectedChips;
    std::string message = "Hover over a chip to highlight it; click to add it.";

    auto drawBettingScreen = [&]() {
        clearScreen();
        std::cout << styled("  ♠  BLACKJACK  ♣  —  PLACE YOUR BET\n", kCyan)
                  << "  Bankroll: " << styled(money(game.bankrollCents()), kYellow)
                  << "    Bet: " << styled(money(wager * 100), kYellow) << "\n"
                  << "  " << message << "\n"
                  << "  Click chips to add; keys 1-4 also add. Backspace removes, C clears.\n\n"
                  << "  CHIPS\n";

        for (int row = 0; row < 5; ++row) {
            for (int i = 0; i < static_cast<int>(kChipValues.size()); ++i) {
                const bool available = kChipValues[i] <= game.bankrollCents() / 100 - wager;
                std::string line;
                if (row == 0) {
                    line = border("  ┌─────────┐  ", true);
                } else if (row == 4) {
                    line = border("  └─────────┘  ", true);
                } else if (row == 2) {
                    const std::string label = "$" + std::to_string(kChipValues[i]);
                    const std::size_t left = (9 - label.size()) / 2;
                    const std::string inside = std::string(left, ' ') + label
                        + std::string(9 - left - label.size(), ' ');
                    line = border("  │", true) + styled(inside, available ? kWhite : "\033[90m")
                           + border("│  ", true);
                } else {
                    line = border("  │", true) + styled("  • • •  ",
                        available ? kChipColors[i] : "\033[90m") + border("│  ", true);
                }
                if (hovered == i) line = highlighted(std::move(line));
                std::cout << line;
                if (i + 1 < static_cast<int>(kChipValues.size())) std::cout << "  ";
            }
            std::cout << '\n';
        }

        const bool canDeal = wager >= BlackjackGame::kMinimumBetDollars;
        std::cout << '\n' << (canDeal ? styled("  [ DEAL ]", kGreen) : "  [ DEAL ]")
                  << (canDeal ? "  Click Deal or press Enter to start; Q returns to menu.\n"
                              : "  Add at least $10 before dealing; Q returns to menu.\n");
        std::cout.flush();
    };

    auto addChip = [&](int index) {
        const int denomination = kChipValues[index];
        if (denomination > game.bankrollCents() / 100 - wager) {
            message = "That chip exceeds your remaining bankroll.";
            return;
        }
        selectedChips.push_back(denomination);
        wager += denomination;
        message = "Chip added. Click Deal or press Enter when your bet is ready.";
    };

    drawBettingScreen();
    while (true) {
        const bool canDeal = wager >= BlackjackGame::kMinimumBetDollars;
        const InputEvent event = readTerminalEvent();
        if (event.type == InputEvent::Type::Error) return false;
        if (event.type == InputEvent::Type::Timeout) continue;
        bool redraw = false;
        if (event.type == InputEvent::Type::Key) {
            if (event.key == 'q' || event.key == 'Q') return false;
            if (event.key >= '1' && event.key <= '4') {
                addChip(event.key - '1');
                redraw = true;
            } else if (event.key == '\r' || event.key == '\n') {
                if (canDeal) {
                    betDollars = wager;
                    return true;
                }
                message = "Add at least $10 before dealing.";
                redraw = true;
            } else if (event.key == '\x7f' || event.key == '\b') {
                if (!selectedChips.empty()) {
                    wager -= selectedChips.back();
                    selectedChips.pop_back();
                    message = "Last chip removed.";
                    redraw = true;
                }
            } else if (event.key == 'c' || event.key == 'C') {
                wager = 0;
                selectedChips.clear();
                message = "Bet cleared.";
                redraw = true;
            }
        } else if (event.type == InputEvent::Type::Mouse) {
            int chipAtPointer = -1;
            if (event.row >= 7 && event.row <= 11) {
                for (int i = 0; i < static_cast<int>(kChipColumns.size()); ++i) {
                    if (event.column >= kChipColumns[i]
                        && event.column < kChipColumns[i] + 15) {
                        chipAtPointer = i;
                        break;
                    }
                }
            }
            if (hovered != chipAtPointer) {
                hovered = chipAtPointer;
                redraw = true;
            }
            if (event.pressed && (event.button & 3) == 0) {
                if (chipAtPointer >= 0) {
                    addChip(chipAtPointer);
                    redraw = true;
                } else if (event.row == 13 && event.column >= 3 && event.column <= 11) {
                    if (canDeal) {
                        betDollars = wager;
                        return true;
                    }
                    message = "Add at least $10 before dealing.";
                    redraw = true;
                }
            }
        }
        if (redraw) drawBettingScreen();
    }
}

void drawActionButtons(const BlackjackGame& game, int hovered, const std::string& message) {
    const char* colors[] = {kGreen, kCyan, kYellow};
    const char* labels[] = {" [ H ] HIT ", " [ S ] STAND ", " [ D ] DOUBLE DOWN "};
    const int columns[] = {3, 20, 39};
    std::cout << "\033[22;1H\033[2K";
    for (int i = 0; i < 3; ++i) {
        const bool enabled = i != 2 || game.canDoubleDown();
        std::string label = enabled ? styled(labels[i], colors[i])
                                    : styled(labels[i], "\033[90m");
        if (hovered == i && enabled) label = highlighted(std::move(label));
        std::cout << "\033[22;" << columns[i] << 'H' << label;
    }
    std::cout << "\033[23;1H\033[2K"
              << (message.empty() ? "Click an action or press H, S, or D. Q returns to menu."
                                  : message);
    std::cout.flush();
}

bool askForAction(const BlackjackGame& game, char& action) {
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        std::cout << "Choose (H)it, (S)tand, or (D)ouble down: ";
        std::string line;
        if (!std::getline(std::cin, line)) return false;
        const std::string cleaned = trim(line);
        if (cleaned.size() == 1)
            action = static_cast<char>(std::toupper(static_cast<unsigned char>(cleaned[0])));
        return true;
    }

    MouseTerminal terminal;
    if (!terminal.enable(false)) return false;

    int hovered = -1;
    std::string message;
    drawActionButtons(game, hovered, message);
    while (true) {
        const InputEvent event = readTerminalEvent();
        if (event.type == InputEvent::Type::Error) return false;
        if (event.type == InputEvent::Type::Timeout) continue;
        bool redraw = false;
        if (event.type == InputEvent::Type::Key) {
            const char key = static_cast<char>(std::toupper(static_cast<unsigned char>(event.key)));
            if (key == 'Q') return false;
            if (key == 'H' || key == 'S' || key == 'D') {
                if (key == 'D' && !game.canDoubleDown()) {
                    message = "Double Down is unavailable now; choose Hit or Stand.";
                    redraw = true;
                } else {
                    action = key;
                    return true;
                }
            }
        } else if (event.type == InputEvent::Type::Mouse) {
            int actionAtPointer = -1;
            if (event.row == 22) {
                if (event.column >= 3 && event.column <= 14) actionAtPointer = 0;
                else if (event.column >= 20 && event.column <= 33) actionAtPointer = 1;
                else if (event.column >= 39 && event.column <= 58) actionAtPointer = 2;
            }
            if (hovered != actionAtPointer) {
                hovered = actionAtPointer;
                redraw = true;
            }
            if (event.pressed && (event.button & 3) == 0 && actionAtPointer >= 0) {
                if (actionAtPointer == 2 && !game.canDoubleDown()) {
                    message = "Double Down is unavailable now; choose Hit or Stand.";
                    redraw = true;
                } else {
                    action = actionAtPointer == 0 ? 'H' : actionAtPointer == 1 ? 'S' : 'D';
                    return true;
                }
            }
        }
        if (redraw) drawActionButtons(game, hovered, message);
    }
}

}  // namespace

int Card::value() const {
    const int rank = static_cast<int>(rank_);
    if (rank == static_cast<int>(Rank::Ace)) return 11;
    return std::min(rank, 10);
}

std::string Card::rankText() const {
    const int rank = static_cast<int>(rank_);
    if (rank <= 10) return std::to_string(rank);
    switch (rank_) {
        case Rank::Jack: return "J";
        case Rank::Queen: return "Q";
        case Rank::King: return "K";
        case Rank::Ace: return "A";
        default: throw std::logic_error("Invalid card rank.");
    }
}

std::string Card::suitText() const {
    switch (suit_) {
        case Suit::Spades: return "♠";
        case Suit::Hearts: return "♥";
        case Suit::Diamonds: return "♦";
        case Suit::Clubs: return "♣";
    }
    throw std::logic_error("Invalid card suit.");
}

std::array<std::string, 7> Card::render(bool color) const {
    const bool red = suit_ == Suit::Hearts || suit_ == Suit::Diamonds;
    const char* ink = red ? kRed : kWhite;
    const std::string rank = rankText();
    const std::string left = rank + std::string(9 - rank.size(), ' ');
    const std::string right = std::string(9 - rank.size(), ' ') + rank;
    const std::string suitLine = std::string(4, ' ') + suitText() + std::string(4, ' ');

    return {
        border("┌─────────┐", color),
        border("│", color) + styled(left, ink, color) + border("│", color),
        border("│", color) + "         " + border("│", color),
        border("│", color) + styled(suitLine, ink, color) + border("│", color),
        border("│", color) + "         " + border("│", color),
        border("│", color) + styled(right, ink, color) + border("│", color),
        border("└─────────┘", color)
    };
}

std::array<std::string, 7> Card::renderBack(bool color) {
    return cardBack(color);
}

Deck::Deck(unsigned seed, std::size_t decks) : rng_(seed), deckCount_(decks) {
    if (decks == 0 || decks > std::numeric_limits<std::size_t>::max() / 52)
        throw std::invalid_argument("A shoe must contain at least one deck.");
    resetAndShuffle();
}

void Deck::resetAndShuffle() {
    cards_.clear();
    cards_.reserve(deckCount_ * 52);
    for (std::size_t deck = 0; deck < deckCount_; ++deck) {
        for (int suit = static_cast<int>(Suit::Spades); suit <= static_cast<int>(Suit::Clubs); ++suit) {
            for (int rank = static_cast<int>(Rank::Two); rank <= static_cast<int>(Rank::Ace); ++rank) {
                cards_.emplace_back(static_cast<Suit>(suit), static_cast<Rank>(rank));
            }
        }
    }
    for (std::size_t i = cards_.size() - 1; i > 0; --i) {
        std::uniform_int_distribution<std::size_t> pick(0, i);
        std::swap(cards_[i], cards_[pick(rng_)]);
    }
    next_ = 0;
}

bool Deck::needsShuffle() const {
    return remaining() * 4 < size();
}

void Deck::prepareForRound() {
    if (needsShuffle()) resetAndShuffle();
}

Card Deck::draw() {
    if (next_ >= cards_.size()) throw std::runtime_error("The card shoe is empty.");
    return cards_[next_++];
}

int Hand::total() const {
    int sum = 0;
    int aces = 0;
    for (const Card& card : cards_) {
        sum += card.value();
        if (card.rank() == Rank::Ace) ++aces;
    }
    while (sum > 21 && aces > 0) {
        sum -= 10;
        --aces;
    }
    return sum;
}

bool Hand::isSoft() const {
    int sum = 0;
    int aces = 0;
    for (const Card& card : cards_) {
        sum += card.value();
        if (card.rank() == Rank::Ace) ++aces;
    }
    while (sum > 21 && aces > 0) {
        sum -= 10;
        --aces;
    }
    return aces > 0;
}

bool Hand::isBlackjack() const {
    return cards_.size() == 2 && total() == 21;
}

BlackjackGame::BlackjackGame(unsigned seed, std::size_t decks) : deck_(seed, decks) {}

int BlackjackGame::returnFor(BlackjackResult result, int wagerCents) {
    switch (result) {
        case BlackjackResult::Blackjack: return wagerCents + wagerCents * 3 / 2;
        case BlackjackResult::PlayerWin: return wagerCents * 2;
        case BlackjackResult::Push: return wagerCents;
        case BlackjackResult::NotStarted:
        case BlackjackResult::InProgress:
        case BlackjackResult::DealerBlackjack:
        case BlackjackResult::DealerWin:
        case BlackjackResult::PlayerBust: return 0;
    }
    return 0;
}

bool BlackjackGame::startRound(int betDollars) {
    if (result_ == BlackjackResult::InProgress || betDollars < kMinimumBetDollars
        || betDollars > bankrollCents_ / 100) {
        return false;
    }

    deck_.prepareForRound();
    player_.clear();
    dealer_.clear();
    wagerCents_ = betDollars * 100;
    bankrollCents_ -= wagerCents_;
    result_ = BlackjackResult::InProgress;
    playerTurn_ = true;
    dealerTurn_ = false;
    holeCardRevealed_ = false;

    player_.add(deck_.draw());
    dealer_.add(deck_.draw());
    player_.add(deck_.draw());
    dealer_.add(deck_.draw());

    const bool playerNatural = player_.isBlackjack();
    const bool dealerNatural = dealer_.isBlackjack();
    if (playerNatural && dealerNatural) {
        holeCardRevealed_ = true;
        settle(BlackjackResult::Push);
    } else if (playerNatural) {
        holeCardRevealed_ = true;
        settle(BlackjackResult::Blackjack);
    } else if (dealerNatural) {
        holeCardRevealed_ = true;
        settle(BlackjackResult::DealerBlackjack);
    }
    return true;
}

bool BlackjackGame::hit() {
    if (!playerTurn_ || result_ != BlackjackResult::InProgress) return false;
    player_.add(deck_.draw());
    if (player_.isBust()) {
        holeCardRevealed_ = true;
        settle(BlackjackResult::PlayerBust);
    } else if (player_.total() == 21) {
        beginDealerTurn();
    }
    return true;
}

bool BlackjackGame::stand() {
    if (!playerTurn_ || result_ != BlackjackResult::InProgress) return false;
    beginDealerTurn();
    return true;
}

bool BlackjackGame::canDoubleDown() const {
    return playerTurn_ && result_ == BlackjackResult::InProgress
           && player_.cards().size() == 2 && bankrollCents_ >= wagerCents_;
}

bool BlackjackGame::doubleDown() {
    if (!canDoubleDown()) return false;
    bankrollCents_ -= wagerCents_;
    wagerCents_ *= 2;
    player_.add(deck_.draw());
    if (player_.isBust()) {
        holeCardRevealed_ = true;
        settle(BlackjackResult::PlayerBust);
    } else {
        beginDealerTurn();
    }
    return true;
}

void BlackjackGame::beginDealerTurn() {
    playerTurn_ = false;
    dealerTurn_ = true;
    holeCardRevealed_ = true;
}

bool BlackjackGame::dealerShouldHit() const {
    return dealerTurn_ && dealer_.total() <= 16;
}

bool BlackjackGame::dealerStep() {
    if (!dealerTurn_) return false;
    if (dealerShouldHit()) {
        dealer_.add(deck_.draw());
        return true;
    }
    settleDealerHand();
    return false;
}

void BlackjackGame::settleDealerHand() {
    if (dealer_.isBust()) {
        settle(BlackjackResult::PlayerWin);
    } else if (player_.total() > dealer_.total()) {
        settle(BlackjackResult::PlayerWin);
    } else if (player_.total() < dealer_.total()) {
        settle(BlackjackResult::DealerWin);
    } else {
        settle(BlackjackResult::Push);
    }
}

void BlackjackGame::settle(BlackjackResult result) {
    result_ = result;
    playerTurn_ = false;
    dealerTurn_ = false;
    bankrollCents_ += returnFor(result, wagerCents_);
}

void playBlackjack() {
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    BlackjackGame game;

    while (true) {
        if (game.bankrollCents() < BlackjackGame::kMinimumBetDollars * 100) {
            clearScreen();
            std::cout << styled("╔══════════════════════════╗\n"
                                "║        GAME OVER         ║\n"
                                "╚══════════════════════════╝\n", kRed)
                      << "\nFinal bankroll: " << styled(money(game.bankrollCents()), kYellow) << '\n';
            if (game.bankrollCents() == 0) {
                std::cout << "Your bankroll is empty.\n";
            } else {
                std::cout << "You don't have enough for the $10 minimum wager.\n";
            }
            return;
        }

        drawTable(game);
        int betDollars = 0;
        if (!askForBet(game, betDollars)) return;
        game.startRound(betDollars);
        drawTable(game);

        while (game.playerTurn()) {
            char action = '\0';
            if (!askForAction(game, action)) return;
            if (action == 'H') {
                game.hit();
                drawTable(game);
                if (game.result() == BlackjackResult::InProgress) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(220));
                }
            } else if (action == 'S') {
                game.stand();
                drawTable(game);
            } else if (action == 'D') {
                if (!game.doubleDown()) {
                    std::cout << styled("Double down is only available on your first two cards "
                                        "and requires enough chips to match the wager.\n", kRed);
                    continue;
                }
                drawTable(game);
            } else {
                std::cout << styled("Choose H to hit, S to stand, or D to double down.\n", kRed);
            }
        }

        while (game.dealerTurn()) {
            if (game.dealerShouldHit()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(700));
                game.dealerStep();
                drawTable(game);
            } else {
                game.dealerStep();
            }
        }

        drawTable(game);
        std::cout << styled(resultText(game.result()), resultColor(game.result())) << '\n';
        if (game.bankrollCents() == 0) {
            std::cout << styled("\n╔══════════════════════════╗\n"
                                "║        GAME OVER         ║\n"
                                "╚══════════════════════════╝\n"
                                "Your bankroll is empty.\n", kRed);
            return;
        }
        if (game.bankrollCents() < BlackjackGame::kMinimumBetDollars * 100) {
            std::cout << styled("\nGame over: you are below the $10 minimum wager.\n", kRed);
            return;
        }

        std::cout << "\nPress Enter for another round, or Q to return to the menu: ";
        std::string line;
        if (!std::getline(std::cin, line)) return;
        if (trim(line) == "q" || trim(line) == "Q") return;
    }
}
