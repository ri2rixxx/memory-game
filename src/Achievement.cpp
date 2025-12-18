#include "Achievement.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <ctime>
#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <map>
#include <cstring>    
#include <chrono>       
#include <errno.h>     

namespace fs = std::filesystem;

AchievementManager::AchievementManager() : totalScore(0), playerName("") {
    saveFilePath = "achievements.dat";
    initializeAchievements();
}

AchievementManager::AchievementManager(const std::string& playerName) 
    : totalScore(0), playerName(playerName), totalPairsFound(0), 
      perfectGamesCount(0), quickMatchesCount(0) {
    
    saveFilePath = "achievements_" + playerName + ".dat";
    initializeAchievements();
    
    // Автоматически загружаем достижения при создании
    loadFromFile(saveFilePath);
    loadAchievementStats();
}

void AchievementManager::setPlayerName(const std::string& name) { 
    playerName = name; 
    saveFilePath = "achievements_" + playerName + ".dat";
    
    // Загружаем достижения для нового пользователя
    loadFromFile(saveFilePath);
}

void AchievementManager::loadAchievementStats() {
    std::string statsFile = "achievement_stats_" + playerName + ".dat";
    std::ifstream file(statsFile);
    
    if (!file.is_open()) {
        return;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string key;
        std::getline(ss, key, ':');
        
        if (key == "THEMES") {
            std::string theme;
            while (std::getline(ss, theme, ',')) {
                if (!theme.empty()) {
                    playedThemes[theme] = true;
                }
            }
        }
        else if (key == "DIFFICULTIES") {
            std::string diff;
            while (std::getline(ss, diff, ',')) {
                if (!diff.empty()) {
                    playedDifficulties[diff] = true;
                }
            }
        }
        else if (key == "TOTAL_PAIRS") {
            ss >> totalPairsFound;
        }
        else if (key == "PERFECT_GAMES") {
            ss >> perfectGamesCount;
        }
        else if (key == "QUICK_MATCHES") {
            ss >> quickMatchesCount;
        }
    }
    
    file.close();
}

void AchievementManager::saveAchievementStats() {
    std::string statsFile = "achievement_stats_" + playerName + ".dat";
    std::ofstream file(statsFile);
    
    if (!file.is_open()) {
        return;
    }
    
    // Сохраняем темы
    file << "THEMES:";
    bool first = true;
    for (const auto& theme : playedThemes) {
        if (!first) file << ",";
        file << theme.first;
        first = false;
    }
    file << "\n";
    
    // Сохраняем сложности
    file << "DIFFICULTIES:";
    first = true;
    for (const auto& diff : playedDifficulties) {
        if (!first) file << ",";
        file << diff.first;
        first = false;
    }
    file << "\n";
    
    // Сохраняем остальную статистику
    file << "TOTAL_PAIRS:" << totalPairsFound << "\n";
    file << "PERFECT_GAMES:" << perfectGamesCount << "\n";
    file << "QUICK_MATCHES:" << quickMatchesCount << "\n";
    
    file.close();
}

void AchievementManager::addPlayedTheme(const std::string& theme) {
    if (theme.empty()) return;
    
    std::cout << "🎨 Adding theme to stats: " << theme << std::endl;
    playedThemes[theme] = true;
    
    saveAchievementStats();
    
    if (playedThemes.size() >= 5) {
        std::cout << "🎯 Theme Collector condition met! " 
                  << playedThemes.size() << "/5 themes" << std::endl;
        
        // Получаем достижение напрямую
        for (auto& achievement : achievements) {
            if (achievement.type == AchievementType::THEME_COLLECTOR) {
                if (!achievement.unlocked) {
                    achievement.unlocked = true;
                    achievement.progress = achievement.requirement;
                    
                    // Добавляем очки
                    switch (achievement.rarity) {
                        case AchievementRarity::RARE: totalScore += 25; break;
                        default: totalScore += 10; break;
                    }
                    
                    std::cout << "🎉 Theme Collector UNLOCKED!" << std::endl;
                    
                    // Сохраняем сразу
                    saveToFile(saveFilePath);
                }
                break;
            }
        }
    }
}

void AchievementManager::addPlayedDifficulty(const std::string& difficulty) {
    if (difficulty.empty()) return;
    
    std::cout << "📈 Adding difficulty to stats: " << difficulty << std::endl;
    playedDifficulties[difficulty] = true;
    
    // Сохраняем статистику сразу
    saveAchievementStats();
    
    if (playedDifficulties.size() >= 4) {
        std::cout << "🎯 All Difficulties condition met! " 
                  << playedDifficulties.size() << "/4 difficulties" << std::endl;
        
        for (auto& achievement : achievements) {
            if (achievement.type == AchievementType::ALL_DIFFICULTIES) {
                if (!achievement.unlocked) {
                    achievement.unlocked = true;
                    achievement.progress = achievement.requirement;
                    totalScore += 100; // Легендарное достижение
                    std::cout << "🎉 All Difficulties UNLOCKED!" << std::endl;
                    saveToFile(saveFilePath);
                }
                break;
            }
        }
    }
    
    if (difficulty == "Expert") {
        std::cout << "🎯 Expert difficulty played!" << std::endl;
        
        for (auto& achievement : achievements) {
            if (achievement.type == AchievementType::DIFFICULTY_MASTER) {
                if (!achievement.unlocked) {
                    achievement.unlocked = true;
                    achievement.progress = achievement.requirement;
                    totalScore += 50; // Эпическое достижение
                    std::cout << "🎉 Difficulty Master UNLOCKED!" << std::endl;
                    saveToFile(saveFilePath);
                }
                break;
            }
        }
    }
}

void AchievementManager::initializeAchievements() {
    achievements.clear();
    
    achievements.emplace_back(
        AchievementType::FIRST_GAME, 
        "First Step", 
        "Play your first game",
        AchievementRarity::COMMON, 
        "", 1
    );
    
    achievements.emplace_back(
        AchievementType::PERFECT_GAME, 
        "Perfect Game", 
        "Find all pairs with minimum moves",
        AchievementRarity::RARE, 
        "", 3  // Требуется 3 раза
    );
    
    achievements.emplace_back(
        AchievementType::SPEED_RUNNER, 
        "Speed Runner", 
        "Complete a game in less than 60 seconds",
        AchievementRarity::EPIC, 
        "", 1
    );
    
    achievements.emplace_back(
        AchievementType::COMBO_MASTER, 
        "Combo Master", 
        "Find 3 pairs in a row without mistakes",
        AchievementRarity::RARE, 
        "", 5  // Требуется 5 раз
    );
    
    achievements.emplace_back(
        AchievementType::DIFFICULTY_MASTER, 
        "Difficulty Master", 
        "Complete a game on Expert difficulty",
        AchievementRarity::EPIC, 
        "", 1
    );
    
    achievements.emplace_back(
        AchievementType::THEME_COLLECTOR, 
        "Theme Collector", 
        "Play with all card themes",
        AchievementRarity::RARE, 
        "", 5  // Все 5 тем
    );
    
    achievements.emplace_back(
        AchievementType::SCORE_MILESTONE_1000, 
        "Beginner", 
        "Score 1000 points",
        AchievementRarity::COMMON, 
        "", 1000
    );
    
    achievements.emplace_back(
        AchievementType::SCORE_MILESTONE_5000, 
        "Score Master", 
        "Score 5000 points",
        AchievementRarity::EPIC, 
        "", 5000
    );
    
    achievements.emplace_back(
        AchievementType::MOVES_EFFICIENT, 
        "Efficient Player", 
        "Complete a game with 100% efficiency",
        AchievementRarity::RARE, 
        "", 3  // Требуется 3 раза
    );
    
    achievements.emplace_back(
        AchievementType::STREAK_5, 
        "Hot Streak", 
        "Win 5 games in a row",
        AchievementRarity::EPIC, 
        "", 5
    );
    
    achievements.emplace_back(
        AchievementType::DAILY_PLAYER, 
        "Daily Player", 
        "Play for 7 consecutive days",
        AchievementRarity::RARE, 
        "", 7
    );
    
    achievements.emplace_back(
        AchievementType::MATCH_FANATIC, 
        "Match Fanatic", 
        "Find 100 pairs of cards",
        AchievementRarity::EPIC, 
        "", 100
    );
    
    achievements.emplace_back(
        AchievementType::NO_MISTAKES, 
        "No Mistakes", 
        "Complete a game without wrong moves",
        AchievementRarity::LEGENDARY, 
        "", 1
    );
    
    achievements.emplace_back(
        AchievementType::ALL_DIFFICULTIES, 
        "Difficulty Conqueror", 
        "Complete all difficulty levels",
        AchievementRarity::LEGENDARY, 
        "", 4  // Все 4 уровня сложности
    );
    
    achievements.emplace_back(
        AchievementType::QUICK_THINKER, 
        "Quick Thinker", 
        "Find a pair in less than 3 seconds",
        AchievementRarity::RARE, 
        "", 10  // Требуется 10 раз
    );
}

void AchievementManager::updateAchievement(AchievementType type, int progress) {
    for (auto& achievement : achievements) {
        if (achievement.type == type) {
            achievement.addProgress(progress);
            
            if (achievement.unlocked && 
                std::find(unlockedAchievements.begin(), unlockedAchievements.end(), type) == unlockedAchievements.end()) {
                unlockedAchievements.push_back(type);
                
                // Добавляем очки за достижение
                switch (achievement.rarity) {
                    case AchievementRarity::COMMON: totalScore += 10; break;
                    case AchievementRarity::RARE: totalScore += 25; break;
                    case AchievementRarity::EPIC: totalScore += 50; break;
                    case AchievementRarity::LEGENDARY: totalScore += 100; break;
                }
                
                std::cout << "🎉 Achievement unlocked: " << achievement.title 
                          << " (" << achievement.getRarityString() << ")" << std::endl;
            }
            break;
        }
    }
}

void AchievementManager::unlockAchievement(AchievementType type) {
    updateAchievement(type, 1000);
}

void AchievementManager::checkGameAchievements(int score, int moves, int totalPairs, 
                                              double time, const std::string& difficulty, 
                                              const std::string& theme) {
    std::cout << "\n=== ACHIEVEMENT CHECK START ===" << std::endl;
    std::cout << "Score: " << score << ", Moves: " << moves << ", Pairs: " << totalPairs << std::endl;
    std::cout << "Time: " << time << "s, Difficulty: " << difficulty << ", Theme: " << theme << std::endl;
    
    updateAchievement(AchievementType::FIRST_GAME);
    
    // Набор очков (используем прогресс)
    if (score >= 1000) {
        updateAchievement(AchievementType::SCORE_MILESTONE_1000, score);
    }
    if (score >= 5000) {
        updateAchievement(AchievementType::SCORE_MILESTONE_5000, score);
    }
    
    // Идеальная игра
    if (moves == totalPairs) {
        std::cout << "🎯 Perfect game detected!" << std::endl;
        updateAchievement(AchievementType::PERFECT_GAME);
        updateAchievement(AchievementType::NO_MISTAKES);
    }
    
    // Скоростной проход
    if (time < 60.0) {
        std::cout << "⚡ Speed run detected!" << std::endl;
        updateAchievement(AchievementType::SPEED_RUNNER);
    }
    
    // Эффективность
    float efficiency = (float)totalPairs / std::max(moves, 1);
    if (efficiency >= 1.0f) {
        updateAchievement(AchievementType::MOVES_EFFICIENT);
    }
    
    // Общее количество пар
    updateAchievement(AchievementType::MATCH_FANATIC, totalPairs);
    
    // Сохраняем результат игры
    recordGameResult(score, difficulty, theme);
    
    // Сохраняем достижения
    std::cout << "💾 Saving achievements..." << std::endl;
    saveToFile(saveFilePath);
    
    std::cout << "=== ACHIEVEMENT CHECK COMPLETE ===\n" << std::endl;
}

bool AchievementManager::isAchievementUnlocked(AchievementType type) const {
    for (const auto& achievement : achievements) {
        if (achievement.type == type && achievement.unlocked) {
            return true;
        }
    }
    return false;
}

Achievement* AchievementManager::getAchievement(AchievementType type) {
    for (auto& achievement : achievements) {
        if (achievement.type == type) {
            return &achievement;
        }
    }
    return nullptr;
}

std::vector<Achievement> AchievementManager::getUnlockedAchievements() const {
    std::vector<Achievement> unlocked;
    for (const auto& achievement : achievements) {
        if (achievement.unlocked) {
            unlocked.push_back(achievement);
        }
    }
    return unlocked;
}

int AchievementManager::getUnlockedCount() const {
    int count = 0;
    for (const auto& achievement : achievements) {
        if (achievement.unlocked) {
            count++;
        }
    }
    return count;
}

void AchievementManager::resetProgress() {
    for (auto& achievement : achievements) {
        achievement.unlocked = false;
        achievement.progress = 0;
    }
    unlockedAchievements.clear();
    totalScore = 0;
}

void AchievementManager::saveToFile(const std::string& filename) {
    try {
        std::cout << "💾 SAVING achievements to: " << filename << std::endl;
        
        // Создаем директорию если нужно
        fs::path filePath(filename);
        if (!filePath.parent_path().empty()) {
            fs::create_directories(filePath.parent_path());
        }
        
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "❌ ERROR: Cannot open file for writing: " << filename << std::endl;
            return;
        }
        
        // Сохраняем данные
        file << "MEMORY_GAME_ACHIEVEMENTS_V2\n";
        file << "PLAYER:" << playerName << "\n";
        file << "TOTAL_SCORE:" << totalScore << "\n";
        file << "SAVE_DATE:" << getCurrentDateTime() << "\n";
        
        // Сохраняем статистику
        file << "STATS:";
        file << "THEMES=" << playedThemes.size() << ",";
        file << "DIFFICULTIES=" << playedDifficulties.size() << ",";
        file << "TOTAL_PAIRS=" << totalPairsFound << ",";
        file << "QUICK_MATCHES=" << quickMatchesCount << "\n";
        
        for (const auto& achievement : achievements) {
            file << "ACH:" 
                 << static_cast<int>(achievement.type) << ","
                 << (achievement.unlocked ? "1" : "0") << ","
                 << achievement.progress << ","
                 << achievement.requirement << "\n";
        }
        
        file << "END\n";
        file.close();
        
        // Также сохраняем отдельную статистику
        saveAchievementStats();
        
        std::cout << "✅ Achievements saved to: " << filename << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ EXCEPTION saving achievements: " << e.what() << std::endl;
    }
}

void AchievementManager::loadFromFile(const std::string& filename) {
    std::cout << "📂 LOADING achievements from: " << filename << std::endl;
    
    if (!std::filesystem::exists(filename)) {
        std::cout << "   File does not exist, starting fresh" << std::endl;
        return;
    }
    
    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "❌ ERROR: Cannot open file: " << filename << std::endl;
            return;
        }
        
        std::string line;
        std::getline(file, line);
        
        if (line != "MEMORY_GAME_ACHIEVEMENTS_V1") {
            std::cout << "Warning: Invalid achievement file format" << std::endl;
            file.close();
            return;
        }
        
        while (std::getline(file, line)) {
            if (line == "END") break;
            
            if (line.find("PLAYER:") == 0) {
                std::string filePlayer = line.substr(7);
                if (filePlayer != playerName) {
                    std::cout << "Warning: File belongs to different player: " 
                              << filePlayer << " (expected: " << playerName << ")" << std::endl;
                }
            } else if (line.find("TOTAL_SCORE:") == 0) {
                totalScore = std::stoi(line.substr(12));
            } else if (line.find("ACH:") == 0) {
                std::string data = line.substr(4);
                std::stringstream ss(data);
                std::string token;
                std::vector<std::string> tokens;
                
                while (std::getline(ss, token, ',')) {
                    tokens.push_back(token);
                }
                
                if (tokens.size() >= 4) {
                    AchievementType type = static_cast<AchievementType>(std::stoi(tokens[0]));
                    bool unlocked = (tokens[1] == "1");
                    int progress = std::stoi(tokens[2]);
                    int requirement = std::stoi(tokens[3]);
                    
                    // Находим и обновляем достижение
                    for (auto& achievement : achievements) {
                        if (achievement.type == type) {
                            achievement.unlocked = unlocked;
                            achievement.progress = progress;
                            achievement.requirement = requirement;
                            
                            if (unlocked) {
                                // Добавляем в список разблокированных если еще нет
                                if (std::find(unlockedAchievements.begin(), 
                                             unlockedAchievements.end(), 
                                             type) == unlockedAchievements.end()) {
                                    unlockedAchievements.push_back(type);
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }
        
        file.close();
        
        std::cout << "✅ Achievements loaded successfully" << std::endl;
        std::cout << "   Unlocked: " << getUnlockedCount() << "/" << achievements.size() << std::endl;
        std::cout << "   Total score: " << totalScore << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ EXCEPTION loading achievements: " << e.what() << std::endl;
    }
}

std::string AchievementManager::getCurrentDateTime() const {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

void AchievementManager::recordGameResult(int score, const std::string& difficulty, const std::string& theme) {
    static int winStreak = 0;
    
    // Добавляем тему и сложность
    addPlayedTheme(theme);
    addPlayedDifficulty(difficulty);
    
    // Для серии побед
    if (score > 0) { // Победа
        winStreak++;
        if (winStreak >= 5) {
            updateAchievement(AchievementType::STREAK_5);
        }
    } else {
        winStreak = 0;
    }
    
    // Сохраняем статистику
    saveAchievementStats();
}

void AchievementManager::recordDailyPlay() {
    static std::string lastPlayDate = "";
    static int consecutiveDays = 0;
    
    std::time_t now = std::time(nullptr);
    std::tm* localTime = std::localtime(&now);
    char currentDate[11];
    std::strftime(currentDate, sizeof(currentDate), "%Y-%m-%d", localTime);
    
    if (lastPlayDate != currentDate) {
        if (lastPlayDate.empty()) {
            consecutiveDays = 1;
        } else {
            consecutiveDays++;
        }
        lastPlayDate = currentDate;
        
        updateAchievement(AchievementType::DAILY_PLAYER, 1);
    }
}

void AchievementManager::recordPerfectMatch() {
    static int perfectMatchesInRow = 0;
    perfectMatchesInRow++;
    
    if (perfectMatchesInRow >= 3) {
        updateAchievement(AchievementType::COMBO_MASTER);
    }
}

void AchievementManager::recordQuickMatch(double time) {
    if (time < 3.0) { // Меньше 3 секунд
        quickMatchesCount++;
        updateAchievement(AchievementType::QUICK_THINKER);
        saveAchievementStats();
    }
}
