#include "UserManager.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <algorithm>
#include <functional>

// Простая функция хеширования (в реальном проекте используйте bcrypt)
std::string simpleHash(const std::string& password) {
    unsigned int hash = 5381;
    for (char c : password) {
        hash = ((hash << 5) + hash) + c;
    }
    std::stringstream ss;
    ss << std::hex << hash;
    return ss.str();
}

UserManager::UserManager(const std::string& connStr) 
    : isLoggedIn(false) {
    
    database = std::make_unique<Database>(connStr);
    std::cout << "UserManager created with PostgreSQL backend" << std::endl;
}

UserManager::~UserManager() {
    logout();
    std::cout << "UserManager destroyed" << std::endl;
}

bool UserManager::initialize() {
    if (!database) {
        std::cerr << "❌ Database not initialized" << std::endl;
        return false;
    }
    
    return database->initialize();
}

std::string UserManager::hashPassword(const std::string& password) {
    return simpleHash(password);
}

bool UserManager::verifyPassword(const std::string& password, const std::string& hash) {
    return hashPassword(password) == hash;
}

bool UserManager::registerUser(const std::string& username, const std::string& password, 
                             const std::string& email, std::string& errorMsg) {
    
    std::cout << "Attempting to register user: " << username << std::endl;
    
    // Проверки
    if (username.length() < 3) {
        errorMsg = "Username must be at least 3 characters";
        return false;
    }
    
    if (password.length() < 4) {
        errorMsg = "Password must be at least 4 characters";
        return false;
    }
    
    if (email.find('@') == std::string::npos) {
        errorMsg = "Invalid email address";
        return false;
    }
    
    // Хеширование пароля
    std::string passwordHash = hashPassword(password);
    
    // Используем Database для создания пользователя
    if (!database->createUser(username, passwordHash, email, errorMsg)) {
        return false;
    }
    
    std::cout << "✅ User registered: " << username << std::endl;
    return true;
}

bool UserManager::login(const std::string& username, const std::string& password, 
                       std::string& errorMsg) {
    
    std::cout << "Attempting login: " << username << std::endl;
    
    // Здесь нужна реализация через Database
    // Пока используем упрощенную версию
    
    if (!database->authenticateUser(username, hashPassword(password), errorMsg)) {
        return false;
    }
    
    // Устанавливаем текущего пользователя
    currentUser.username = username;
    currentUser.password = hashPassword(password); // Сохраняем хеш
    isLoggedIn = true;
    
    std::cout << "✅ User logged in: " << username << std::endl;
    return true;
}

bool UserManager::logout() {
    if (isLoggedIn) {
        std::cout << "User logged out: " << currentUser.username << std::endl;
        currentUser = User();
        isLoggedIn = false;
        return true;
    }
    return false;
}

bool UserManager::usernameExists(const std::string& username) {
    // Реализация через запрос к БД
    return false; // Упрощенно
}

bool UserManager::emailExists(const std::string& email) {
    // Реализация через запрос к БД
    return false; // Упрощенно
}

bool UserManager::updateStats(int score, bool won, double playTime) {
    if (!isLoggedIn) return false;
    
    // Обновляем локальные данные
    currentUser.totalScore += score;
    currentUser.gamesPlayed++;
    if (won) currentUser.gamesWon++;
    currentUser.totalPlayTime += playTime;
    
    // Здесь должна быть реализация через Database::updateUserStats
    // Нужен ID пользователя
    
    return true;
}

std::vector<User> UserManager::getLeaderboard(int limit) {
    std::vector<User> leaderboard;
    
    // Здесь должна быть реализация через запрос к БД
    // Пока возвращаем пустой список
    
    return leaderboard;
}
