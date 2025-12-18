#include "Player.h"
#include <iostream>
#include <iomanip>

Player::Player(const std::string& name)
    : name(name), score(0), moves(0), matchedPairs(0), gameFinished(false) {}

void Player::incrementMoves() {
    moves++;
}

void Player::incrementMatchedPairs() {
    matchedPairs++;
}

void Player::calculateScore(int totalPairs) {
    double timeInSeconds = getElapsedTime();
    
    // Формула подсчета очков:
    // Базовые очки за пары * коэффициент эффективности - штраф за время
    int baseScore = matchedPairs * 100;
    double efficiency = (double)matchedPairs / (moves > 0 ? moves : 1);
    int timePenalty = (int)(timeInSeconds * 0.5);
    
    score = (int)(baseScore * efficiency * 2) - timePenalty;
    if (score < 0) score = 0;
    
    // Бонус за идеальную игру
    if (moves == totalPairs) {
        score += 500;
    }
}

void Player::startGame() {
    startTime = std::chrono::steady_clock::now();
    moves = 0;
    matchedPairs = 0;
    score = 0;
    gameFinished = false;
}

void Player::finishGame() {
    endTime = std::chrono::steady_clock::now();
    gameFinished = true;
}

double Player::getElapsedTime() const {
    if (gameFinished) {
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime);
        return duration.count();
    } else {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - startTime);
        return duration.count();
    }
}

void Player::displayStats() const {
    std::cout << "\n╔════════════════════════════════════╗\n";
    std::cout << "║      📊 СТАТИСТИКА ИГРОКА         ║\n";
    std::cout << "╠════════════════════════════════════╣\n";
    std::cout << "║ Игрок: " << std::setw(27) << std::left << name << "║\n";
    std::cout << "║ Ходов: " << std::setw(27) << std::left << moves << "║\n";
    std::cout << "║ Найдено пар: " << std::setw(20) << std::left << matchedPairs << "║\n";
    std::cout << "║ Время: " << std::setw(20) << std::left << (int)getElapsedTime() << " сек   ║\n";
    std::cout << "║ Очки: " << std::setw(27) << std::left << score << "║\n";
    std::cout << "╚════════════════════════════════════╝\n";
}
