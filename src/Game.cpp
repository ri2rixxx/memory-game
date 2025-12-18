#include "Game.h"
#include <iostream>
#include <algorithm>
#include <random>
#include <sstream>
#include <sys/stat.h>
#include <iomanip>
#include <ctime>
#include <clocale>
#include <fstream>
#include <filesystem>
#include <set>
#include <map>

namespace fs = std::filesystem;

// Вспомогательная функция для проверки Docker
bool Game::isRunningInDockerInternal() {
    std::ifstream dockerEnv("/.dockerenv");
    if (dockerEnv.good()) {
        return true;
    }
    
    std::ifstream cgroup("/proc/self/cgroup");
    if (cgroup.is_open()) {
        std::string line;
        while (std::getline(cgroup, line)) {
            if (line.find("docker") != std::string::npos ||
                line.find("kubepods") != std::string::npos) {
                return true;
            }
        }
    }
    return false;
}

Game::Game() 
    : window(sf::VideoMode(1200, 800), "Memory Game", sf::Style::Titlebar | sf::Style::Close | sf::Style::Resize),
      brightness(1.0f),
      currentVideoMode(1200, 800),
      currentVideoModeIndex(2),
      currentState(GameState::LOGIN_SCREEN),
      previousState(GameState::LOGIN_SCREEN),
      difficulty(Difficulty::MEDIUM),
      currentTheme(CardTheme::ANIMALS),
      rows(4),
      cols(4),
      totalPairs(8),
      matchedPairs(0),
      moves(0),
      isGameActive(false),
      firstCardSelected(false),
      selectedCard1(-1),
      selectedCard2(-1),
      cardFlipTime(0.3f),
      cardFlipProgress(0.0f),
      isFlipping(false),
      firstCard(nullptr),
      secondCard(nullptr),
      isChecking(false),
      hasWon(false),
      consecutiveMatches(0),
      totalGamesPlayed(0),
      winStreak(0),
      activeInputField(InputField::NONE),
      achievementsScrollOffset(0.0f),
      achievementsTotalHeight(0.0f)
{
    std::cout << "=== ИНИЦИАЛИЗАЦИЯ ИГРЫ ===" << std::endl;
    std::cout << "Начинаем с экрана регистрации/логина" << std::endl;
    
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);
    
    // Доступные разрешения
    availableVideoModes = {
        sf::VideoMode(800, 600),
        sf::VideoMode(1024, 768),
        sf::VideoMode(1200, 800),
        sf::VideoMode(1280, 720),
        sf::VideoMode(1366, 768),
        sf::VideoMode(1920, 1080)
    };
    
    // Загрузка ресурсов
    std::cout << "Загрузка ресурсов..." << std::endl;
    loadResources();
    std::cout << "Ресурсы загружены" << std::endl;
    
    // Кнопка сдачи
    surrenderButton = Button(950, 700, 200, 50, "Surrender", mainFont, 
                            [this]() { surrenderGame(); });
    surrenderButton.setColors(sf::Color(220, 20, 60), sf::Color(255, 0, 0), sf::Color(178, 34, 34));
    
        //ИНИЦИАЛИЗАЦИЯ POSTGRESQL БАЗЫ ДАННЫХ
    std::cout << "Initializing PostgreSQL database..." << std::endl;
    
    // Определяем строку подключения
    std::string connStr;
    
    // Проверяем переменную окружения
    char* dbUrl = std::getenv("DATABASE_URL");
    if (dbUrl) {
        connStr = dbUrl;
        std::cout << "Using DATABASE_URL from environment" << std::endl;
    } 
    // Проверяем, в Docker ли мы
    else if (isRunningInDockerInternal()) {
        connStr = "host=postgres dbname=memory_game_db user=game_user password=game_password";
        std::cout << "🐳 Docker environment detected, connecting to PostgreSQL container" << std::endl;
    } 
    // Локальная разработка
    else {
        connStr = "host=localhost dbname=memory_game_db user=game_user password=game_password";
        std::cout << "💻 Local environment, connecting to local PostgreSQL" << std::endl;
    }
    
    std::cout << "Connection string: " << connStr << std::endl;
    
    // Пытаемся подключиться к PostgreSQL
    try {
        database = std::make_unique<Database>(connStr);
        
        if (database->initialize()) {
            std::cout << "✅ PostgreSQL database initialized successfully" << std::endl;
            
            // Тестируем подключение
            auto testRecords = database->getTopScores(1);
            std::cout << "📊 Records in database: " << testRecords.size() << std::endl;
            
        } else {
            std::cout << "⚠ Failed to initialize PostgreSQL database" << std::endl;
            std::cout << "Error: " << database->getLastError() << std::endl;
            database = nullptr;
        }
    } catch (const std::exception& e) {
        std::cerr << "❌ Exception while connecting to PostgreSQL: " << e.what() << std::endl;
        database = nullptr;
    }
    
    // Инициализация UserManager
    userManager = std::make_unique<UserManager>(connStr);  // ИСПРАВЬТЕ ЗДЕСЬ
    if (userManager->initialize()) {
        std::cout << "✅ UserManager initialized with PostgreSQL" << std::endl;
    } else {
        std::cout << "⚠ Failed to initialize UserManager" << std::endl;
    }
    
    // Настройка UI
    setupLoginUI();
    setupRegisterUI();
    setupMainMenu();
    setupGameUI();
    setupPauseMenu();
    setupSetupMenu();
    setupLeaderboardUI();
    setupSettingsMenu();
    setupAchievementsUI();
    setupContactForm();
    
    // Инициализация умных указателей для звука и музыки
    soundManager = std::make_unique<SoundManager>();
    musicPlayer = std::make_unique<MusicPlayer>();
    std::filesystem::create_directories("saves");
    
    std::cout << "=== ИНИЦИАЛИЗАЦИЯ ЗАВЕРШЕНА ===" << std::endl;
    std::cout << "Текущее состояние: LOGIN_SCREEN" << std::endl;
}

Game::~Game() {
    // Сохраняем достижения перед выходом
    if (achievementManager && player) {
        std::string saveDir = "saves/";
        std::filesystem::create_directories(saveDir);
        
        std::string achievementsPath = saveDir + "achievements_" + player->getName() + ".dat";
        std::cout << "\n💾 SAVING achievements on exit..." << std::endl;
        std::cout << "   Path: " << achievementsPath << std::endl;
        
        achievementManager->saveToFile(achievementsPath);
        
        // Также сохраняем в папку database для совместимости
        std::string backupPath = "database/achievements_" + player->getName() + ".dat";
        achievementManager->saveToFile(backupPath);
    }
    
    // Выходим из аккаунта пользователя
    if (userManager) {
        userManager->logout();
    }
    
    std::cout << "Игра завершена." << std::endl;
}

void Game::setupLoginUI() {
    loginButtons.clear();
    
    float centerX = window.getSize().x / 2 - 150;
    float startY = 450.0f;
    float spacing = 70.0f;
    
    usernameInput = "";
    passwordInput = "";
    
    // Кнопка входа
    loginButtons.emplace_back(
        centerX, startY, 300.0f, 60.0f,
        "Login", mainFont,
        [this]() {
            std::cout << "Нажата кнопка Login" << std::endl;
            
            if (usernameInput.empty() || passwordInput.empty()) {
                loginErrorText.setString("Please enter username and password");
                return;
            }
            
            std::string errorMsg;
            if (userManager->login(usernameInput, passwordInput, errorMsg)) {
                std::cout << "✅ Вход успешен!" << std::endl;
                
                // Создаем объект игрока
                player = std::make_unique<Player>(usernameInput);
                
                // Создаем менеджер достижений ДЛЯ ЭТОГО ПОЛЬЗОВАТЕЛЯ
                achievementManager = std::make_unique<AchievementManager>(usernameInput);
                
                // Загружаем достижения пользователя
                std::string achievementsPath;
                if (isRunningInDockerInternal()) {
                    achievementsPath = "/app/database/achievements_" + usernameInput + ".dat";
                } else {
                    achievementsPath = "achievements_" + usernameInput + ".dat";
                }
                achievementManager->loadFromFile(achievementsPath);
                
                // Обновляем таймер ежедневной игры
                achievementManager->recordDailyPlay();
                
                currentState = GameState::MAIN_MENU;
                background.setTexture(menuBackgroundTexture);
                
                usernameInput = "";
                passwordInput = "";
                loginErrorText.setString("");
                
            } else {
                std::cout << "❌ Ошибка входа: " << errorMsg << std::endl;
                loginErrorText.setString(errorMsg);
            }
        }
    );
    
    // Кнопка регистрации
    loginButtons.emplace_back(
        centerX, startY + spacing, 300.0f, 60.0f,
        "Register", mainFont,
        [this]() {
            std::cout << "Переход на экран регистрации" << std::endl;
            currentState = GameState::REGISTER_SCREEN;
            usernameInput = "";
            passwordInput = "";
            emailInput = "";
            confirmPasswordInput = "";
            registerErrorText.setString("");
        }
    );
    
    // Кнопка гостевого режима
    loginButtons.emplace_back(
        centerX, startY + spacing * 2, 300.0f, 60.0f,
        "Play as Guest", mainFont,
        [this]() {
            std::cout << "Запуск гостевого режима" << std::endl;
            player = std::make_unique<Player>("Guest");
            
            // Для гостя тоже создаем achievementManager
            achievementManager = std::make_unique<AchievementManager>("Guest");
            
            currentState = GameState::MAIN_MENU;
            background.setTexture(menuBackgroundTexture);
        }
    );
    
    // Настройка цвета кнопок
    loginButtons[0].setColors(sf::Color(0, 150, 0), sf::Color(0, 200, 0), sf::Color(0, 100, 0));
    loginButtons[1].setColors(sf::Color(70, 130, 180), sf::Color(100, 149, 237), sf::Color(30, 144, 255));
    loginButtons[2].setColors(sf::Color(128, 128, 128), sf::Color(160, 160, 160), sf::Color(96, 96, 96));
    
    // Текст ошибки
    loginErrorText.setFont(mainFont);
    loginErrorText.setCharacterSize(20);
    loginErrorText.setFillColor(sf::Color::Red);
    loginErrorText.setPosition(centerX, startY + spacing * 3);
}

void Game::setupRegisterUI() {
    registerButtons.clear();
    
    float centerX = window.getSize().x / 2 - 150;
    float startY = 550.0f;
    float spacing = 70.0f;
    
    // Кнопка регистрации
    registerButtons.emplace_back(
        centerX, startY, 300.0f, 60.0f,
        "Create Account", mainFont,
        [this]() {
            std::cout << "Нажата кнопка Create Account" << std::endl;
            
            if (usernameInput.empty() || passwordInput.empty() || 
                emailInput.empty() || confirmPasswordInput.empty()) {
                registerErrorText.setString("All fields are required");
                return;
            }
            
            if (passwordInput != confirmPasswordInput) {
                registerErrorText.setString("Passwords do not match");
                return;
            }
            
            if (passwordInput.length() < 4) {
                registerErrorText.setString("Password must be at least 4 characters");
                return;
            }
            
            if (emailInput.find('@') == std::string::npos) {
                registerErrorText.setString("Invalid email address");
                return;
            }
            
            std::string errorMsg;
            if (userManager->registerUser(usernameInput, passwordInput, emailInput, errorMsg)) {
                std::cout << "✅ Регистрация успешна! Автоматический вход..." << std::endl;
                
                // Автоматический вход после регистрации
                if (userManager->login(usernameInput, passwordInput, errorMsg)) {
                    player = std::make_unique<Player>(usernameInput);
                    
                    // Создаем менеджер достижений для нового пользователя
                    achievementManager = std::make_unique<AchievementManager>(usernameInput);
                    
                    // Сразу разблокируем достижение "Первая игра"
                    achievementManager->updateAchievement(AchievementType::FIRST_GAME);
                    
                    // Обновляем таймер ежедневной игры
                    achievementManager->recordDailyPlay();
                    
                    // Сохраняем достижения
                    std::string achievementsPath;
                    if (isRunningInDockerInternal()) {
                        achievementsPath = "/app/database/achievements_" + usernameInput + ".dat";
                    } else {
                        achievementsPath = "achievements_" + usernameInput + ".dat";
                    }
                    achievementManager->saveToFile(achievementsPath);
                    
                    currentState = GameState::MAIN_MENU;
                    background.setTexture(menuBackgroundTexture);
                    
                    usernameInput = "";
                    passwordInput = "";
                    emailInput = "";
                    confirmPasswordInput = "";
                    registerErrorText.setString("");
                }
            } else {
                std::cout << "❌ Ошибка регистрации: " << errorMsg << std::endl;
                registerErrorText.setString(errorMsg);
            }
        }
    );
    
    // Кнопка возврата
    registerButtons.emplace_back(
        centerX, startY + spacing, 300.0f, 60.0f,
        "Back to Login", mainFont,
        [this]() {
            std::cout << "Возврат на экран входа" << std::endl;
            currentState = GameState::LOGIN_SCREEN;
            usernameInput = "";
            passwordInput = "";
            emailInput = "";
            confirmPasswordInput = "";
            registerErrorText.setString("");
        }
    );
    
    // Настройка цвета кнопок
    registerButtons[0].setColors(sf::Color(0, 150, 0), sf::Color(0, 200, 0), sf::Color(0, 100, 0));
    registerButtons[1].setColors(sf::Color(220, 20, 60), sf::Color(255, 0, 0), sf::Color(178, 34, 34));
    
    // Текст ошибки
    registerErrorText.setFont(mainFont);
    registerErrorText.setCharacterSize(20);
    registerErrorText.setFillColor(sf::Color::Red);
    registerErrorText.setPosition(centerX, startY + spacing * 2);
}

void Game::run() {
    std::cout << "=== НАЧАЛО ИГРОВОГО ЦИКЛА ===" << std::endl;
    sf::Clock clock;
    
    while (window.isOpen()) {
        sf::Time deltaTime = clock.restart();
        
        handleEvents();
        update(deltaTime.asSeconds());
        render();
    }
}

void Game::startNewGame() {
    std::cout << "\n=== НАЧАЛО НОВОЙ ИГРЫ ===" << std::endl;
    currentState = GameState::ENTER_NAME;
    playerNameInput = "";
    isEnteringName = true;
    hasWon = false;
    consecutiveMatches = 0;
    matchedPairs = 0;
    moves = 0;
}

void Game::pauseGame() {
    if (currentState == GameState::PLAYING) {
        currentState = GameState::PAUSED;
        isGameActive = false;
    }
}

void Game::resumeGame() {
    if (currentState == GameState::PAUSED) {
        currentState = GameState::PLAYING;
        isGameActive = true;
        gameClock.restart();
    }
}

void Game::showLeaderboard() {
    currentState = GameState::LEADERBOARD;
}

void Game::showAchievements() {
    std::cout << "=== SHOW ACHIEVEMENTS ===" << std::endl;
    
    // Проверяем, есть ли менеджер достижений
    if (!achievementManager) {
        std::cout << "❌ Achievement manager not initialized!" << std::endl;
        
        // Пытаемся создать менеджер достижений
        if (player) {
            achievementManager = std::make_unique<AchievementManager>(player->getName());
            std::cout << "✅ Created achievement manager for: " << player->getName() << std::endl;
            
            // Загружаем достижения
            std::string achievementsPath;
            if (isRunningInDockerInternal()) {
                achievementsPath = "/app/database/achievements_" + player->getName() + ".dat";
            } else {
                achievementsPath = "achievements_" + player->getName() + ".dat";
            }
            achievementManager->loadFromFile(achievementsPath);
        } else if (userManager && userManager->isUserLoggedIn()) {
            achievementManager = std::make_unique<AchievementManager>(userManager->getCurrentUsername());
            std::cout << "✅ Created achievement manager for: " << userManager->getCurrentUsername() << std::endl;
        } else {
            achievementManager = std::make_unique<AchievementManager>("Guest");
            std::cout << "✅ Created achievement manager for Guest" << std::endl;
        }
    } else {
        // Обновляем данные достижений
        std::cout << "✅ Achievement manager already exists" << std::endl;
        
        // Загружаем свежие данные
        if (player) {
            std::string achievementsPath;
            if (isRunningInDockerInternal()) {
                achievementsPath = "/app/database/achievements_" + player->getName() + ".dat";
            } else {
                achievementsPath = "achievements_" + player->getName() + ".dat";
            }
            achievementManager->loadFromFile(achievementsPath);
            std::cout << "✅ Reloaded achievements from: " << achievementsPath << std::endl;
        }
    }
    
    currentState = GameState::ACHIEVEMENTS;
}

void Game::showSettings() {
    currentState = GameState::SETTINGS;
}

void Game::exitGame() {
    window.close();
}

void Game::surrenderGame() {
    if (!isGameActive) return;
    
    std::cout << "Игрок сдался!" << std::endl;
    
    if (soundManager) {
        soundManager->playGameLose();
    }
    
    isGameActive = false;
    
    if (player) {
        player->finishGame();
        player->calculateScore(totalPairs);
        
        GameRecord record;
        record.playerName = player->getName();
        record.score = player->getScore() / 2;
        record.moves = moves;
        record.pairs = matchedPairs;
        record.time = elapsedTime.asSeconds();
        record.date = getCurrentDate();
        record.difficulty = getDifficultyString();
        
        if (database) {
            database->saveGame(record);
        }
    }
    
    currentState = GameState::GAME_OVER_LOSE;
}

void Game::setDifficulty(Difficulty diff) {
    difficulty = diff;
}

void Game::setTheme(CardTheme theme) {
    currentTheme = theme;
}

void Game::saveGameResult() {
    if (!player || !database) {
        return;
    }
    
    GameRecord record;
    record.playerName = player->getName();
    record.score = player->getScore();
    record.moves = moves;
    record.pairs = matchedPairs;
    record.time = elapsedTime.asSeconds();
    record.date = getCurrentDate();
    record.difficulty = getDifficultyString();
    
    database->saveGame(record);
    std::cout << "💾 Результат сохранен в БД" << std::endl;
}

std::string Game::getCurrentDate() const {
    std::time_t now = std::time(nullptr);
    std::tm* localTime = std::localtime(&now);
    
    char buffer[80];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localTime);
    return std::string(buffer);
}

std::string Game::getDifficultyString() const {
    switch (difficulty) {
        case Difficulty::EASY: return "Easy";
        case Difficulty::MEDIUM: return "Medium";
        case Difficulty::HARD: return "Hard";
        case Difficulty::EXPERT: return "Expert";
        default: return "Unknown";
    }
}

sf::Color Game::getDifficultyColor() const {
    switch (difficulty) {
        case Difficulty::EASY:
            return sf::Color::Green;
        case Difficulty::MEDIUM:
            return sf::Color::Yellow;
        case Difficulty::HARD:
            return sf::Color(255, 165, 0);
        case Difficulty::EXPERT:
            return sf::Color::Red;
        default:
            return sf::Color::White;
    }
}

void Game::getImagePathsForTheme(CardTheme theme, std::vector<std::string>& imagePaths) {
    std::string themeFolder;
    
    switch (theme) {
        case CardTheme::ANIMALS: themeFolder = "animals"; break;
        case CardTheme::FRUITS: themeFolder = "fruits"; break;
        case CardTheme::EMOJI: themeFolder = "emoji"; break;
        case CardTheme::MEMES: themeFolder = "memes"; break;
        case CardTheme::SYMBOLS: themeFolder = "symbols"; break;
        default: themeFolder = "animals"; break;
    }
    
    std::string imageDir = "assets/images/" + themeFolder + "/";
    std::cout << "📁 Поиск изображений в: " << imageDir << std::endl;
    
    imagePaths.clear();
    
    try {
        int foundCount = 0;
        for (const auto& entry : fs::directory_iterator(imageDir)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                
                if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp") {
                    imagePaths.push_back(entry.path().string());
                    foundCount++;
                    
                    if (foundCount <= 5) {
                        std::cout << "   ✅ " << entry.path().filename() << std::endl;
                    }
                }
            }
        }
        
        if (foundCount > 5) {
            std::cout << "   ... и еще " << (foundCount - 5) << " файлов" << std::endl;
        }
        
        std::cout << "Найдено файлов: " << foundCount << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "❌ Ошибка доступа к папке: " << e.what() << std::endl;
        // Создаем тестовые пути
        for (int i = 1; i <= 18; i++) {
            imagePaths.push_back(imageDir + "image" + std::to_string(i) + ".png");
        }
    }
}

void Game::renderMainMenu() {
    window.draw(titleText);
    
    for (auto& button : mainMenuButtons) {
        button.render(window);
    }
}

void Game::renderNameInput() {
    // Заголовок
    sf::Text title;
    title.setFont(mainFont);
    title.setString("Enter your name:");
    title.setCharacterSize(48);
    title.setFillColor(sf::Color::White);
    title.setStyle(sf::Text::Bold);
    
    sf::FloatRect titleBounds = title.getLocalBounds();
    title.setOrigin(titleBounds.left + titleBounds.width / 2.0f,
                    titleBounds.top + titleBounds.height / 2.0f);
    title.setPosition(window.getSize().x / 2, 200);
    window.draw(title);
    
    // Поле ввода
    nameInputBox.setPosition(window.getSize().x / 2 - 200, 300);
    window.draw(nameInputBox);
    
    // Текст ввода
    nameInputText.setString(playerNameInput + "_");
    nameInputText.setPosition(window.getSize().x / 2 - 180, 315);
    window.draw(nameInputText);
    
    // Подсказка
    sf::Text hint;
    hint.setFont(mainFont);
    hint.setString("Press Enter to continue");
    hint.setCharacterSize(24);
    hint.setFillColor(sf::Color(200, 200, 200));
    
    sf::FloatRect hintBounds = hint.getLocalBounds();
    hint.setOrigin(hintBounds.left + hintBounds.width / 2.0f,
                   hintBounds.top + hintBounds.height / 2.0f);
    hint.setPosition(window.getSize().x / 2, 400);
    window.draw(hint);
}

void Game::renderSetupMenu() {
    // Заголовок
    sf::Text setupTitle("Game Setup", mainFont, 48);
    setupTitle.setFillColor(sf::Color::White);
    setupTitle.setStyle(sf::Text::Bold);
    setupTitle.setPosition(window.getSize().x / 2 - 100, 100);
    window.draw(setupTitle);
    
    // Информация о текущих настройках
    std::stringstream settingsInfo;
    settingsInfo << "Current settings:\n";
    settingsInfo << "• Player: " << (player ? player->getName() : "Not set") << "\n";
    settingsInfo << "• Difficulty: " << getDifficultyString() << "\n";
    settingsInfo << "• Theme: ";
    switch (currentTheme) {
        case CardTheme::ANIMALS: settingsInfo << "Animals"; break;
        case CardTheme::FRUITS: settingsInfo << "Fruits"; break;
        case CardTheme::EMOJI: settingsInfo << "Emoji"; break;
        case CardTheme::MEMES: settingsInfo << "Memes"; break;
        case CardTheme::SYMBOLS: settingsInfo << "Symbols"; break;
    }
    
    sf::Text infoText(settingsInfo.str(), mainFont, 24);
    infoText.setFillColor(sf::Color(200, 200, 200));
    infoText.setPosition(window.getSize().x / 2 - 200, 150);
    window.draw(infoText);
    
    // Кнопки
    for (auto& button : setupButtons) button.render(window);
}

void Game::renderGame() {
    // Заголовок и статистика
    window.draw(titleText);
    window.draw(statsText);
    window.draw(timerText);
    window.draw(scoreText);
    window.draw(difficultyText);
    
    // Карточки
    for (auto& card : cards) {
        card->render(window);
    }
    
    // Кнопки
    for (auto& button : gameButtons) {
        button.render(window);
    }
    
    surrenderButton.render(window);
}

void Game::renderPauseMenu() {
    // Полупрозрачный фон
    sf::RectangleShape overlay(sf::Vector2f(window.getSize().x, window.getSize().y));
    overlay.setFillColor(sf::Color(0, 0, 0, 150));
    window.draw(overlay);
    
    sf::Text pauseText("PAUSED", mainFont, 72);
    pauseText.setFillColor(sf::Color::Yellow);
    pauseText.setStyle(sf::Text::Bold);
    sf::FloatRect pauseBounds = pauseText.getLocalBounds();
    pauseText.setOrigin(pauseBounds.left + pauseBounds.width / 2.0f,
                        pauseBounds.top + pauseBounds.height / 2.0f);
    pauseText.setPosition(window.getSize().x / 2, 200);
    window.draw(pauseText);
    
    for (auto& button : pauseButtons) {
        button.render(window);
    }
}

void Game::renderGameOverLose() {
    // Game Over текст
    sf::Text gameOverText("GAME OVER", mainFont, 72);
    gameOverText.setFillColor(sf::Color::Red);
    gameOverText.setStyle(sf::Text::Bold);
    
    sf::FloatRect bounds = gameOverText.getLocalBounds();
    gameOverText.setOrigin(bounds.left + bounds.width / 2.0f,
                          bounds.top + bounds.height / 2.0f);
    gameOverText.setPosition(window.getSize().x / 2, 200);
    window.draw(gameOverText);
    
    // Сообщение
    sf::Text messageText("Better luck next time!", mainFont, 36);
    messageText.setFillColor(sf::Color(200, 200, 200));
    
    sf::FloatRect messageBounds = messageText.getLocalBounds();
    messageText.setOrigin(messageBounds.left + messageBounds.width / 2.0f,
                         messageBounds.top + messageBounds.height / 2.0f);
    messageText.setPosition(window.getSize().x / 2, 300);
    window.draw(messageText);
    
    // Статистика
    if (player) {
        std::stringstream stats;
        stats << "Player: " << player->getName() << "\n\n";
        stats << "Final Score: " << player->getScore() << "\n";
        stats << "Progress: " << matchedPairs << "/" << totalPairs << " pairs\n";
        stats << "Time: " << (int)elapsedTime.asSeconds() << " seconds\n";
        stats << "Difficulty: " << getDifficultyString();
        
        sf::Text statsText(stats.str(), mainFont, 32);
        statsText.setFillColor(sf::Color::White);
        statsText.setPosition(window.getSize().x / 2 - 200, 350);
        window.draw(statsText);
    }
    
    // Кнопка продолжения
    sf::RectangleShape continueButton(sf::Vector2f(300, 60));
    continueButton.setPosition(window.getSize().x / 2 - 150, window.getSize().y - 150);
    
    sf::Vector2f mousePos = static_cast<sf::Vector2f>(sf::Mouse::getPosition(window));
    bool isMouseOverButton = continueButton.getGlobalBounds().contains(mousePos);
    
    if (isMouseOverButton) {
        continueButton.setFillColor(sf::Color(70, 130, 180));
        continueButton.setOutlineColor(sf::Color::Yellow);
    } else {
        continueButton.setFillColor(sf::Color(50, 100, 150));
        continueButton.setOutlineColor(sf::Color::White);
    }
    
    continueButton.setOutlineThickness(2);
    window.draw(continueButton);
    
    sf::Text continueText("Return to Menu", mainFont, 28);
    continueText.setFillColor(sf::Color::White);
    sf::FloatRect continueBounds = continueText.getLocalBounds();
    continueText.setOrigin(continueBounds.left + continueBounds.width / 2.0f,
                          continueBounds.top + continueBounds.height / 2.0f);
    continueText.setPosition(window.getSize().x / 2, window.getSize().y - 120);
    
    // Тень
    sf::Text shadowText = continueText;
    shadowText.setFillColor(sf::Color(0, 0, 0, 150));
    shadowText.move(2, 2);
    window.draw(shadowText);
    
    window.draw(continueText);
}

void Game::renderLoginScreen() {
    // Заголовок
    sf::Text title("Memory Game", mainFont, 72);
    title.setFillColor(sf::Color::White);
    title.setStyle(sf::Text::Bold);
    title.setPosition(window.getSize().x / 2 - 150, 50);
    window.draw(title);
    
    sf::Text subtitle("Login to your account", mainFont, 36);
    subtitle.setFillColor(sf::Color(200, 200, 200));
    subtitle.setPosition(window.getSize().x / 2 - 100, 150);
    window.draw(subtitle);
    
    // Поле для имени пользователя
    sf::Text usernameLabel("Username:", mainFont, 28);
    usernameLabel.setFillColor(sf::Color::White);
    usernameLabel.setPosition(400, 250);
    window.draw(usernameLabel);
    
    sf::RectangleShape usernameBox(sf::Vector2f(400, 40));
    usernameBox.setPosition(400, 290);
    usernameBox.setFillColor(sf::Color(50, 50, 50));
    usernameBox.setOutlineThickness(2);
    usernameBox.setOutlineColor(activeInputField == InputField::USERNAME ? 
                                sf::Color::Yellow : sf::Color::White);
    window.draw(usernameBox);
    
    sf::Text usernameText(usernameInput + (activeInputField == InputField::USERNAME ? "_" : ""), 
                         mainFont, 24);
    usernameText.setFillColor(sf::Color::White);
    usernameText.setPosition(410, 295);
    window.draw(usernameText);
    
    // Поле для пароля
    sf::Text passwordLabel("Password:", mainFont, 28);
    passwordLabel.setFillColor(sf::Color::White);
    passwordLabel.setPosition(400, 350);
    window.draw(passwordLabel);
    
    sf::RectangleShape passwordBox(sf::Vector2f(400, 40));
    passwordBox.setPosition(400, 390);
    passwordBox.setFillColor(sf::Color(50, 50, 50));
    passwordBox.setOutlineThickness(2);
    passwordBox.setOutlineColor(activeInputField == InputField::PASSWORD ? 
                                sf::Color::Yellow : sf::Color::White);
    window.draw(passwordBox);
    
    // Отображаем звездочки вместо пароля
    std::string passwordDisplay;
    for (size_t i = 0; i < passwordInput.length(); i++) {
        passwordDisplay += "*";
    }
    if (activeInputField == InputField::PASSWORD) {
        passwordDisplay += "_";
    }
    
    sf::Text passwordText(passwordDisplay, mainFont, 24);
    passwordText.setFillColor(sf::Color::White);
    passwordText.setPosition(410, 395);
    window.draw(passwordText);
    
    // Кнопки
    for (auto& button : loginButtons) {
        button.render(window);
    }
    
    // Текст ошибки
    window.draw(loginErrorText);
    
    // Подсказка
    sf::Text hint("Press TAB to switch fields, ENTER to login", mainFont, 18);
    hint.setFillColor(sf::Color(150, 150, 150));
    hint.setPosition(400, 550);
    window.draw(hint);
}

void Game::renderRegisterScreen() {
    // Заголовок
    sf::Text title("Create Account", mainFont, 72);
    title.setFillColor(sf::Color::White);
    title.setStyle(sf::Text::Bold);
    title.setPosition(window.getSize().x / 2 - 150, 50);
    window.draw(title);
    
    // Поле для имени пользователя
    sf::Text usernameLabel("Username:", mainFont, 28);
    usernameLabel.setFillColor(sf::Color::White);
    usernameLabel.setPosition(400, 150);
    window.draw(usernameLabel);
    
    sf::RectangleShape usernameBox(sf::Vector2f(400, 40));
    usernameBox.setPosition(400, 190);
    usernameBox.setFillColor(sf::Color(50, 50, 50));
    usernameBox.setOutlineThickness(2);
    usernameBox.setOutlineColor(activeInputField == InputField::USERNAME ? 
                                sf::Color::Yellow : sf::Color::White);
    window.draw(usernameBox);
    
    sf::Text usernameText(usernameInput + (activeInputField == InputField::USERNAME ? "_" : ""), 
                         mainFont, 24);
    usernameText.setFillColor(sf::Color::White);
    usernameText.setPosition(410, 195);
    window.draw(usernameText);
    
    // Поле для email
    sf::Text emailLabel("Email:", mainFont, 28);
    emailLabel.setFillColor(sf::Color::White);
    emailLabel.setPosition(400, 250);
    window.draw(emailLabel);
    
    sf::RectangleShape emailBox(sf::Vector2f(400, 40));
    emailBox.setPosition(400, 290);
    emailBox.setFillColor(sf::Color(50, 50, 50));
    emailBox.setOutlineThickness(2);
    emailBox.setOutlineColor(activeInputField == InputField::EMAIL ? 
                             sf::Color::Yellow : sf::Color::White);
    window.draw(emailBox);
    
    sf::Text emailText(emailInput + (activeInputField == InputField::EMAIL ? "_" : ""), 
                       mainFont, 24);
    emailText.setFillColor(sf::Color::White);
    emailText.setPosition(410, 295);
    window.draw(emailText);
    
    // Поле для пароля
    sf::Text passwordLabel("Password:", mainFont, 28);
    passwordLabel.setFillColor(sf::Color::White);
    passwordLabel.setPosition(400, 350);
    window.draw(passwordLabel);
    
    sf::RectangleShape passwordBox(sf::Vector2f(400, 40));
    passwordBox.setPosition(400, 390);
    passwordBox.setFillColor(sf::Color(50, 50, 50));
    passwordBox.setOutlineThickness(2);
    passwordBox.setOutlineColor(activeInputField == InputField::PASSWORD ? 
                                sf::Color::Yellow : sf::Color::White);
    window.draw(passwordBox);
    
    std::string passwordDisplay;
    for (size_t i = 0; i < passwordInput.length(); i++) {
        passwordDisplay += "*";
    }
    if (activeInputField == InputField::PASSWORD) {
        passwordDisplay += "_";
    }
    
    sf::Text passwordText(passwordDisplay, mainFont, 24);
    passwordText.setFillColor(sf::Color::White);
    passwordText.setPosition(410, 395);
    window.draw(passwordText);
    
    // Поле для подтверждения пароля
    sf::Text confirmLabel("Confirm Password:", mainFont, 28);
    confirmLabel.setFillColor(sf::Color::White);
    confirmLabel.setPosition(400, 450);
    window.draw(confirmLabel);
    
    sf::RectangleShape confirmBox(sf::Vector2f(400, 40));
    confirmBox.setPosition(400, 490);
    confirmBox.setFillColor(sf::Color(50, 50, 50));
    confirmBox.setOutlineThickness(2);
    confirmBox.setOutlineColor(activeInputField == InputField::CONFIRM_PASSWORD ? 
                               sf::Color::Yellow : sf::Color::White);
    window.draw(confirmBox);
    
    std::string confirmDisplay;
    for (size_t i = 0; i < confirmPasswordInput.length(); i++) {
        confirmDisplay += "*";
    }
    if (activeInputField == InputField::CONFIRM_PASSWORD) {
        confirmDisplay += "_";
    }
    
    sf::Text confirmText(confirmDisplay, mainFont, 24);
    confirmText.setFillColor(sf::Color::White);
    confirmText.setPosition(410, 495);
    window.draw(confirmText);
    
    // Кнопки
    for (auto& button : registerButtons) {
        button.render(window);
    }
    
    // Текст ошибки
    window.draw(registerErrorText);
    
    // Подсказка
    sf::Text hint("Press TAB to switch fields", mainFont, 18);
    hint.setFillColor(sf::Color(150, 150, 150));
    hint.setPosition(400, 600);
    window.draw(hint);
}

void Game::renderLeaderboard() {
    // Заголовок
    sf::Text title("Leaderboard", mainFont, 64);
    title.setFillColor(sf::Color::White);
    title.setStyle(sf::Text::Bold);
    title.setPosition(window.getSize().x / 2 - 150, 80);
    window.draw(title);
    
    // Получаем лучшие результаты
    auto topPlayers = database ? database->getTopScores(10) : std::vector<GameRecord>();
    
    if (topPlayers.empty()) {
        sf::Text noData("No records in leaderboard yet", mainFont, 32);
        noData.setFillColor(sf::Color(200, 200, 200));
        noData.setPosition(window.getSize().x / 2 - 150, 200);
        window.draw(noData);
    } else {
        // Заголовок таблицы
        sf::Text header("#  Player              Score   Time   Difficulty", mainFont, 28);
        header.setFillColor(sf::Color::Yellow);
        header.setPosition(150, 180);
        window.draw(header);
        
        // Список
        float yPos = 230;
        int rank = 1;
        
        for (const auto& record : topPlayers) {
            std::stringstream line;
            line << std::setw(2) << std::right << rank << ". ";
            line << std::setw(15) << std::left << record.playerName.substr(0, 15) << " ";
            line << std::setw(6) << std::right << record.score << " ";
            line << std::setw(4) << std::right << (int)record.time << "s ";
            line << record.difficulty;
            
            sf::Text playerText(line.str(), mainFont, 24);
            
            if (rank == 1) playerText.setFillColor(sf::Color(255, 215, 0));
            else if (rank == 2) playerText.setFillColor(sf::Color(192, 192, 192));
            else if (rank == 3) playerText.setFillColor(sf::Color(205, 127, 50));
            else playerText.setFillColor(sf::Color::White);
            
            playerText.setPosition(150, yPos);
            window.draw(playerText);
            
            yPos += 40;
            rank++;
            if (rank > 10) break;
        }
    }
    
    // Кнопки
    for (auto& button : leaderboardButtons) {
        button.render(window);
    }
}

void Game::renderSettings() {
    window.draw(settingsTitle);
    
    for (auto& button : settingsButtons) {
        button.render(window);
    }
    
    sf::Text hintText("Changes apply immediately!", mainFont, 20);
    hintText.setFillColor(sf::Color(200, 200, 200));
    hintText.setPosition(window.getSize().x / 2 - 100, 500);
    window.draw(hintText);
}

void Game::renderContactForm() {
    // Полупрозрачный фон
    sf::RectangleShape overlay(sf::Vector2f(window.getSize().x, window.getSize().y));
    overlay.setFillColor(sf::Color(0, 0, 0, 200));
    window.draw(overlay);
    
    contactForm.render(window);
}

void Game::updateBackgrounds() {
    // Применяем яркость
    sf::Color adjustedMenuColor(
        std::min(255, int(menuBackgroundColor.r * brightness)),
        std::min(255, int(menuBackgroundColor.g * brightness)),
        std::min(255, int(menuBackgroundColor.b * brightness))
    );
    
    sf::Color adjustedGameColor(
        std::min(255, int(gameBackgroundColor.r * brightness)),
        std::min(255, int(gameBackgroundColor.g * brightness)),
        std::min(255, int(gameBackgroundColor.b * brightness))
    );
    
    // Создаем текстуры
    sf::Image menuImage;
    menuImage.create(window.getSize().x, window.getSize().y, adjustedMenuColor);
    menuBackgroundTexture.loadFromImage(menuImage);
    
    sf::Image gameImage;
    gameImage.create(window.getSize().x, window.getSize().y, adjustedGameColor);
    gameBackgroundTexture.loadFromImage(gameImage);
    
    background.setTexture(menuBackgroundTexture);
}

void Game::loadResources() {
    // Загрузка шрифта
    std::vector<std::string> fontPaths = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
        "/usr/local/share/memory_game/assets/fonts/gamefont.ttf",
        "assets/fonts/arial.ttf",
        "./arial.ttf"
    };
    
    bool fontLoaded = false;
    for (const auto& path : fontPaths) {
        if (mainFont.loadFromFile(path)) {
            fontLoaded = true;
            std::cout << "Шрифт загружен: " << path << std::endl;
            break;
        }
    }
    
    if (!fontLoaded) {
        throw std::runtime_error("Cannot load any font!");
    }
    
    // Инициализируем цвета фона
    menuBackgroundColor = sf::Color(30, 30, 60);
    gameBackgroundColor = sf::Color(20, 20, 40);
    
    updateBackgrounds();
    
    // Настройка текстовых элементов
    titleText.setFont(mainFont);
    titleText.setString("Memory Game");
    titleText.setCharacterSize(72);
    titleText.setFillColor(sf::Color::White);
    titleText.setStyle(sf::Text::Bold);
    titleText.setOutlineColor(sf::Color::Black);
    titleText.setOutlineThickness(2);
    
    sf::FloatRect titleBounds = titleText.getLocalBounds();
    titleText.setOrigin(titleBounds.left + titleBounds.width / 2.0f,
                        titleBounds.top + titleBounds.height / 2.0f);
    titleText.setPosition(600, 100);
    
    statsText.setFont(mainFont);
    statsText.setCharacterSize(20); 
    statsText.setFillColor(sf::Color::White);
    statsText.setPosition(50, 50);
    statsText.setLineSpacing(1.3f);
    
    timerText.setFont(mainFont);
    timerText.setCharacterSize(24);
    timerText.setFillColor(sf::Color::White);
    timerText.setPosition(50, 180);
    
    scoreText.setFont(mainFont);
    scoreText.setCharacterSize(28);
    scoreText.setFillColor(sf::Color::Yellow);
    scoreText.setPosition(50, 230);
    
    difficultyText.setFont(mainFont);
    difficultyText.setCharacterSize(22);
    difficultyText.setFillColor(sf::Color::White);
    difficultyText.setPosition(50, 280);
    
    settingsTitle.setFont(mainFont);
    settingsTitle.setString("Settings");
    settingsTitle.setCharacterSize(48);
    settingsTitle.setFillColor(sf::Color::White);
    settingsTitle.setStyle(sf::Text::Bold);
    settingsTitle.setPosition(400, 100);
    
    nameInputText.setFont(mainFont);
    nameInputText.setCharacterSize(32);
    nameInputText.setFillColor(sf::Color::White);
    
    nameInputBox.setSize(sf::Vector2f(400, 60));
    nameInputBox.setFillColor(sf::Color(50, 50, 50));
    nameInputBox.setOutlineThickness(2);
    nameInputBox.setOutlineColor(sf::Color::White);
}

void Game::setupAchievementsUI() {
    achievementsButtons.clear();
    
    float buttonWidth = 200.0f;
    float buttonHeight = 50.0f;
    float centerX = window.getSize().x / 2 - buttonWidth / 2;
    float buttonY = window.getSize().y - 100;
    
    achievementsButtons.emplace_back(centerX, buttonY, buttonWidth, buttonHeight, "Back to Menu", mainFont, 
                                   [this]() { 
        currentState = GameState::MAIN_MENU;
    });
    
    achievementsButtons[0].setColors(sf::Color(70, 130, 180), sf::Color(100, 149, 237), sf::Color(30, 144, 255));
}

void Game::renderAchievements() {
    // Заголовок
    sf::Text title("Achievements", mainFont, 64);
    title.setFillColor(sf::Color::White);
    title.setStyle(sf::Text::Bold);
    title.setPosition(window.getSize().x / 2 - 150, 50);
    window.draw(title);
    
    if (!achievementManager) {
        // Если менеджер достижений не создан
        sf::Text noAchievements("Achievements system not initialized", mainFont, 32);
        noAchievements.setFillColor(sf::Color::Red);
        noAchievements.setPosition(window.getSize().x / 2 - 200, 200);
        window.draw(noAchievements);
        
        // Кнопки
        for (auto& button : achievementsButtons) {
            button.render(window);
        }
        return;
    }
    
    // Статистика достижений
    int unlockedCount = achievementManager->getUnlockedCount();
    int totalCount = achievementManager->getTotalCount();
    int totalScore = achievementManager->getTotalScore();
    
    // Заголовок с именем игрока
    std::string playerHeader = "Player: ";
    if (player) {
        playerHeader += player->getName();
    } else if (userManager && userManager->isUserLoggedIn()) {
        playerHeader += userManager->getCurrentUsername();
    } else {
        playerHeader += "Guest";
    }
    
    sf::Text playerText(playerHeader, mainFont, 28);
    playerText.setFillColor(sf::Color::Yellow);
    playerText.setPosition(50, 120);
    window.draw(playerText);
    
    // Статистика
    std::stringstream stats;
    stats << "Progress: " << unlockedCount << "/" << totalCount << " (" 
          << std::fixed << std::setprecision(1) 
          << (totalCount > 0 ? (float)unlockedCount / totalCount * 100 : 0) << "%)\n"
          << "Total Score: " << totalScore << "\n"
          << "Unlocked Achievements:";
    
    sf::Text statsText(stats.str(), mainFont, 24);
    statsText.setFillColor(sf::Color::Yellow);
    statsText.setPosition(50, 160);
    window.draw(statsText);
    
    // Список достижений
    auto allAchievements = achievementManager->getAllAchievements();
    float startY = 250.0f;
    float spacing = 70.0f;
    float panelWidth = 700.0f;
    float panelHeight = 60.0f;
    
    // Создаем область для скроллинга
    sf::RectangleShape scrollArea(sf::Vector2f(800, 400));
    scrollArea.setPosition(40, 240);
    scrollArea.setFillColor(sf::Color(0, 0, 0, 0)); // Прозрачная
    scrollArea.setOutlineColor(sf::Color(100, 100, 100));
    scrollArea.setOutlineThickness(1);
    window.draw(scrollArea);
    
    // Применяем смещение скроллинга
    float renderStartY = startY - achievementsScrollOffset;
    
    // Проверяем, есть ли достижения
    if (allAchievements.empty()) {
        sf::Text noData("No achievements data available", mainFont, 28);
        noData.setFillColor(sf::Color(200, 200, 200));
        noData.setPosition(window.getSize().x / 2 - 150, renderStartY);
        window.draw(noData);
    } else {
        // Прокручиваемый список достижений
        for (size_t i = 0; i < allAchievements.size(); i++) {
            const auto& achievement = allAchievements[i];
            
            float achievementY = renderStartY + i * spacing;
            
            // Пропускаем элементы, которые вне области видимости
            if (achievementY < 240 || achievementY > 640) {
                continue;
            }
            
            // Фон для достижения
            sf::RectangleShape achievementBg(sf::Vector2f(panelWidth, panelHeight));
            achievementBg.setPosition(50, achievementY);
            
            if (achievement.unlocked) {
                achievementBg.setFillColor(sf::Color(60, 60, 60, 200));
                achievementBg.setOutlineColor(sf::Color::Green);
            } else {
                achievementBg.setFillColor(sf::Color(40, 40, 40, 200));
                achievementBg.setOutlineColor(sf::Color(100, 100, 100));
            }
            
            achievementBg.setOutlineThickness(2);
            window.draw(achievementBg);
            
            // Иконка
            if (!achievement.icon.empty()) {
                sf::Text iconText(achievement.icon, mainFont, 30);
                iconText.setPosition(60, achievementY + 15);
                
                if (achievement.unlocked) {
                    iconText.setFillColor(achievement.getRarityColor());
                } else {
                    iconText.setFillColor(sf::Color(100, 100, 100));
                }
                window.draw(iconText);
            }
            
            // Название и описание
            std::string titleStr = achievement.title;
            if (achievement.unlocked) {
                titleStr = "✓ " + titleStr;
            }
            
            sf::Text titleText(titleStr, mainFont, 22);
            if (achievement.unlocked) {
                titleText.setFillColor(sf::Color::White);
            } else {
                titleText.setFillColor(sf::Color(150, 150, 150));
            }
            titleText.setPosition(100, achievementY + 5);
            window.draw(titleText);
            
            sf::Text descText(achievement.description, mainFont, 16);
            if (achievement.unlocked) {
                descText.setFillColor(sf::Color(200, 200, 200));
            } else {
                descText.setFillColor(sf::Color(100, 100, 100));
            }
            descText.setPosition(100, achievementY + 32);
            window.draw(descText);
            
            // Прогресс (только если не разблокировано)
            if (!achievement.unlocked && achievement.requirement > 1) {
                std::string progressStr = std::to_string(achievement.progress) + 
                                         "/" + std::to_string(achievement.requirement);
                sf::Text progressText(progressStr, mainFont, 18);
                progressText.setFillColor(sf::Color::Yellow);
                progressText.setPosition(600, achievementY + 15);
                window.draw(progressText);
                
                // Прогресс-бар
                sf::RectangleShape progressBg(sf::Vector2f(100, 10));
                progressBg.setPosition(650, achievementY + 20);
                progressBg.setFillColor(sf::Color(50, 50, 50));
                window.draw(progressBg);
                
                float progressWidth = (float)achievement.progress / achievement.requirement * 100.0f;
                if (progressWidth > 100) progressWidth = 100;
                
                sf::RectangleShape progressBar(sf::Vector2f(progressWidth, 10));
                progressBar.setPosition(650, achievementY + 20);
                progressBar.setFillColor(achievement.getRarityColor());
                window.draw(progressBar);
            } else if (achievement.unlocked) {
                // Для разблокированных - показываем дату или иконку разблокировки
                sf::Text unlockedText("UNLOCKED", mainFont, 16);
                unlockedText.setFillColor(sf::Color::Green);
                unlockedText.setPosition(650, achievementY + 20);
                window.draw(unlockedText);
            }
            
            // Редкость
            sf::Text rarityText(achievement.getRarityString(), mainFont, 14);
            rarityText.setFillColor(achievement.getRarityColor());
            rarityText.setPosition(750, achievementY + 20);
            window.draw(rarityText);
        }
    }
    
    // Полоса прокрутки
    float totalContentHeight = allAchievements.size() * spacing;
    float visibleHeight = 400;
    
    if (totalContentHeight > visibleHeight) {
        // Рисуем полосу прокрутки
        sf::RectangleShape scrollTrack(sf::Vector2f(10, visibleHeight));
        scrollTrack.setPosition(770, 240);
        scrollTrack.setFillColor(sf::Color(50, 50, 50));
        window.draw(scrollTrack);
        
        // Бегунок
        float thumbHeight = (visibleHeight / totalContentHeight) * visibleHeight;
        float thumbPosition = (achievementsScrollOffset / (totalContentHeight - visibleHeight)) * (visibleHeight - thumbHeight);
        
        sf::RectangleShape scrollThumb(sf::Vector2f(10, thumbHeight));
        scrollThumb.setPosition(770, 240 + thumbPosition);
        scrollThumb.setFillColor(sf::Color(150, 150, 150));
        window.draw(scrollThumb);
    }
    
    // Кнопка назад
    for (auto& button : achievementsButtons) {
        button.render(window);
    }
}

void Game::checkAchievements() {
    if (!achievementManager) {
        std::cout << "❌ Cannot check achievements: achievement manager not initialized" << std::endl;
        return;
    }
    
    if (!player) {
        std::cout << "❌ Cannot check achievements: player not initialized" << std::endl;
        return;
    }
    
    std::cout << "=== CHECKING ACHIEVEMENTS ===" << std::endl;
    std::cout << "Player: " << player->getName() << std::endl;
    std::cout << "Score: " << player->getScore() << std::endl;
    std::cout << "Moves: " << moves << std::endl;
    std::cout << "Total pairs: " << totalPairs << std::endl;
    std::cout << "Time: " << elapsedTime.asSeconds() << " seconds" << std::endl;
    
    // Обновляем таймер ежедневной игры
    achievementManager->recordDailyPlay();
    
    // Получаем названия темы и сложности
    std::string themeName;
    switch (currentTheme) {
        case CardTheme::ANIMALS: themeName = "Animals"; break;
        case CardTheme::FRUITS: themeName = "Fruits"; break;
        case CardTheme::EMOJI: themeName = "Emoji"; break;
        case CardTheme::MEMES: themeName = "Memes"; break;
        case CardTheme::SYMBOLS: themeName = "Symbols"; break;
    }
    
    std::string difficultyName = getDifficultyString();
    
    // Обновляем статистику тем и сложностей ПЕРЕД проверкой достижений
    achievementManager->addPlayedTheme(themeName);
    achievementManager->addPlayedDifficulty(difficultyName);
    
    // Обновляем общее количество найденных пар
    achievementManager->updateAchievement(AchievementType::MATCH_FANATIC, matchedPairs);
    
    // Проверяем достижения после игры
    achievementManager->checkGameAchievements(
        player->getScore(),
        moves,
        totalPairs,
        elapsedTime.asSeconds(),
        difficultyName,
        themeName
    );
    
    // Проверяем быстрый подбор пары
    if (pairTimer.getElapsedTime().asSeconds() < 3.0) {
        std::cout << "Quick match detected: " << pairTimer.getElapsedTime().asSeconds() << " seconds" << std::endl;
        achievementManager->recordQuickMatch(pairTimer.getElapsedTime().asSeconds());
    }
    
    // Проверяем достижение "Perfect Game" (все пары найдены за минимальное количество ходов)
    if (moves == totalPairs) {
        std::cout << "Perfect game detected! Moves = Pairs" << std::endl;
        achievementManager->updateAchievement(AchievementType::PERFECT_GAME);
    }
    
    // Проверяем достижение "No Mistakes" (игра без ошибок)
    if (moves == totalPairs) {
        achievementManager->updateAchievement(AchievementType::NO_MISTAKES);
    }
    
    // Проверяем достижение "Speed Runner" (менее 60 секунд)
    if (elapsedTime.asSeconds() < 60.0) {
        std::cout << "Speed run detected: " << elapsedTime.asSeconds() << " seconds" << std::endl;
        achievementManager->updateAchievement(AchievementType::SPEED_RUNNER);
    }
    
    // Проверяем достижение "Moves Efficient" (100% эффективность)
    float efficiency = (float)matchedPairs / (moves > 0 ? moves : 1);
    if (efficiency >= 1.0f) {
        std::cout << "100% efficiency detected: " << efficiency << std::endl;
        achievementManager->updateAchievement(AchievementType::MOVES_EFFICIENT);
    }
    
    // Сохраняем достижения в файл пользователя
    std::string achievementsPath;
    if (isRunningInDockerInternal()) {
        achievementsPath = "/app/database/achievements_" + player->getName() + ".dat";
    } else {
        achievementsPath = "achievements_" + player->getName() + ".dat";
    }
    
    std::cout << "💾 Saving achievements to: " << achievementsPath << std::endl;
    achievementManager->saveToFile(achievementsPath);
    
    // Проверяем, что что-то сохранилось
    auto unlockedCount = achievementManager->getUnlockedCount();
    std::cout << "✅ Achievements checked. Unlocked: " << unlockedCount << std::endl;
}

void Game::setupMainMenu() {
    mainMenuButtons.clear();
    
    float buttonWidth = 300.0f;
    float buttonHeight = 60.0f;
    float startY = 300.0f;
    float spacing = 80.0f;
    
    mainMenuButtons.emplace_back(
        450.0f, startY, buttonWidth, buttonHeight, 
        "New Game", mainFont, 
        [this]() { startNewGame(); }
    );
    
    mainMenuButtons.emplace_back(
        450.0f, startY + spacing, buttonWidth, buttonHeight, 
        "Leaderboard", mainFont, 
        [this]() { showLeaderboard(); }
    );
    
    mainMenuButtons.emplace_back(
        450.0f, startY + spacing * 2, buttonWidth, buttonHeight, 
        "Achievements", mainFont,
        [this]() { showAchievements(); }
    );
    
    mainMenuButtons.emplace_back(
        450.0f, startY + spacing * 3, buttonWidth, buttonHeight, 
        "Settings", mainFont, 
        [this]() { showSettings(); }
    );
    
    mainMenuButtons.emplace_back(
        450.0f, startY + spacing * 4, buttonWidth, buttonHeight, 
        "Exit", mainFont, 
        [this]() { exitGame(); }
    );
    
    for (auto& button : mainMenuButtons) {
        button.setColors(sf::Color(70, 130, 180), sf::Color(100, 149, 237), sf::Color(30, 144, 255));
    }
}

void Game::setupGameUI() {
    gameButtons.clear();
    
    float buttonWidth = 150.0f;
    float buttonHeight = 40.0f;
    
    gameButtons.emplace_back(
        window.getSize().x - 200, 50.0f, buttonWidth, buttonHeight,
        "Pause", mainFont,
        [this]() { pauseGame(); }
    );
    
    gameButtons.emplace_back(
        window.getSize().x - 200, 100.0f, buttonWidth, buttonHeight,
        "Menu", mainFont,
        [this]() { 
            currentState = GameState::MAIN_MENU;
            background.setTexture(menuBackgroundTexture);
        }
    );
    
    gameButtons.emplace_back(
        window.getSize().x - 200, 150.0f, buttonWidth, buttonHeight,
        "Restart", mainFont,
        [this]() { startNewGame(); }
    );
    
    for (auto& button : gameButtons) {
        button.setColors(
            sf::Color(50, 205, 50),
            sf::Color(60, 215, 60),
            sf::Color(40, 195, 40)
        );
    }
    
    // Обновляем позицию кнопки сдачи
    surrenderButton.setPosition(window.getSize().x - 250, window.getSize().y - 100);
}

void Game::setupPauseMenu() {
    pauseButtons.clear();
    
    float buttonWidth = 250.0f;
    float buttonHeight = 60.0f;
    float centerX = window.getSize().x / 2 - buttonWidth / 2;
    float startY = 350.0f;
    float spacing = 80.0f;
    
    pauseButtons.emplace_back(centerX, startY, buttonWidth, buttonHeight, "Resume", mainFont, 
                             [this]() { resumeGame(); });
    pauseButtons.emplace_back(centerX, startY + spacing, buttonWidth, buttonHeight, "Restart", mainFont, 
                             [this]() { startNewGame(); });
    pauseButtons.emplace_back(centerX, startY + spacing * 2, buttonWidth, buttonHeight, "Main Menu", mainFont, 
                             [this]() { 
                                 currentState = GameState::MAIN_MENU;
                                 background.setTexture(menuBackgroundTexture); 
                             });
    
    for (auto& button : pauseButtons) {
        button.setColors(sf::Color(255, 165, 0), sf::Color(255, 185, 0), sf::Color(255, 140, 0));
    }
}

void Game::setupSetupMenu() {
    setupButtons.clear();
    
    float buttonWidth = 300.0f;
    float buttonHeight = 60.0f;
    float centerX = 450.0f;
    float startY = 200.0f;
    float spacing = 100.0f;
    
    // Сложность
    setupButtons.emplace_back(centerX, startY, buttonWidth, buttonHeight, "Difficulty: Medium", mainFont, 
                             [this]() { 
        switch (difficulty) {
            case Difficulty::EASY: 
                setDifficulty(Difficulty::MEDIUM); 
                setupButtons[0].setText("Difficulty: Medium"); 
                break;
            case Difficulty::MEDIUM: 
                setDifficulty(Difficulty::HARD); 
                setupButtons[0].setText("Difficulty: Hard"); 
                break;
            case Difficulty::HARD: 
                setDifficulty(Difficulty::EXPERT); 
                setupButtons[0].setText("Difficulty: Expert"); 
                break;
            case Difficulty::EXPERT: 
                setDifficulty(Difficulty::EASY); 
                setupButtons[0].setText("Difficulty: Easy"); 
                break;
        }
    });
    
    // Тема
    setupButtons.emplace_back(centerX, startY + spacing, buttonWidth, buttonHeight, "Theme: Animals", mainFont, 
                             [this]() { 
        switch (currentTheme) {
            case CardTheme::ANIMALS: 
                setTheme(CardTheme::FRUITS); 
                setupButtons[1].setText("Theme: Fruits"); 
                break;
            case CardTheme::FRUITS: 
                setTheme(CardTheme::EMOJI); 
                setupButtons[1].setText("Theme: Emoji"); 
                break;
            case CardTheme::EMOJI: 
                setTheme(CardTheme::MEMES); 
                setupButtons[1].setText("Theme: Memes"); 
                break;
            case CardTheme::MEMES: 
                setTheme(CardTheme::SYMBOLS); 
                setupButtons[1].setText("Theme: Symbols"); 
                break;
            case CardTheme::SYMBOLS: 
                setTheme(CardTheme::ANIMALS); 
                setupButtons[1].setText("Theme: Animals"); 
                break;
        }
    });
    
    // Начать игру
    setupButtons.emplace_back(centerX, startY + spacing * 2, buttonWidth, buttonHeight, "Start Game!", mainFont, 
                             [this]() { 
        if (player) {
            resetGame();
            currentState = GameState::PLAYING;
            background.setTexture(gameBackgroundTexture);
            isGameActive = true;
            gameClock.restart();
            std::cout << "Игра начата! Всего пар: " << totalPairs << std::endl;
        }
    });
    
    // Назад
    setupButtons.emplace_back(centerX, startY + spacing * 3, buttonWidth, buttonHeight, "Back to Menu", mainFont, 
                             [this]() { 
        currentState = GameState::MAIN_MENU;
        titleText.setString("Memory Game");
        titleText.setCharacterSize(72);
        sf::FloatRect titleBounds = titleText.getLocalBounds();
        titleText.setOrigin(titleBounds.left + titleBounds.width / 2.0f,
                           titleBounds.top + titleBounds.height / 2.0f);
        titleText.setPosition(window.getSize().x / 2, 100);
    });
    
    // Цвета
    for (int i = 0; i < 2; i++) {
        setupButtons[i].setColors(sf::Color(138, 43, 226), sf::Color(148, 0, 211), sf::Color(128, 0, 128));
    }
    setupButtons[2].setColors(sf::Color(0, 200, 0), sf::Color(0, 230, 0), sf::Color(0, 170, 0));
    setupButtons[3].setColors(sf::Color(220, 20, 60), sf::Color(255, 0, 0), sf::Color(178, 34, 34));
}

void Game::setupLeaderboardUI() {
    leaderboardButtons.clear();
    
    float buttonWidth = 200.0f;
    float buttonHeight = 50.0f;
    float centerX = window.getSize().x / 2 - buttonWidth / 2;
    float buttonY = window.getSize().y - 100;
    
    leaderboardButtons.emplace_back(centerX, buttonY, buttonWidth, buttonHeight, "Back to Menu", mainFont, 
                                   [this]() { 
        currentState = GameState::MAIN_MENU;
    });
    
    leaderboardButtons[0].setColors(sf::Color(70, 130, 180), sf::Color(100, 149, 237), sf::Color(30, 144, 255));
}

void Game::setupSettingsMenu() {
    settingsButtons.clear();
    
    float buttonWidth = 300.0f;
    float buttonHeight = 60.0f;
    float centerX = 450.0f;
    float startY = 200.0f;
    float spacing = 80.0f;
    
    // Яркость
    settingsButtons.emplace_back(
        centerX, startY, buttonWidth, buttonHeight,
        "Brightness: 100%", mainFont,
        [this]() { 
            brightness += 0.1f;
            if (brightness > 1.5f) brightness = 0.5f;
            
            std::stringstream ss;
            ss << "Brightness: " << int(brightness * 100) << "%";
            settingsButtons[0].setText(ss.str());
            
            updateBackgrounds();
        }
    );
    
    // Разрешение
    settingsButtons.emplace_back(
        centerX, startY + spacing, buttonWidth, buttonHeight,
        "Resolution: 1200x800", mainFont,
        [this]() { 
            currentVideoModeIndex = (currentVideoModeIndex + 1) % availableVideoModes.size();
            currentVideoMode = availableVideoModes[currentVideoModeIndex];
            
            std::stringstream ss;
            ss << "Resolution: " << currentVideoMode.width << "x" << currentVideoMode.height;
            settingsButtons[1].setText(ss.str());
            
            window.create(currentVideoMode, "Memory Game", 
                         sf::Style::Titlebar | sf::Style::Close | sf::Style::Resize);
            window.setFramerateLimit(60);
            
            updateBackgrounds();
        }
    );
    
    // Обратная связь
    settingsButtons.emplace_back(
        centerX, startY + spacing * 2, buttonWidth, buttonHeight,
        "Contact Developer", mainFont,
        [this]() { 
            previousState = currentState;
            currentState = GameState::CONTACT_FORM;
            contactForm.reset();
        }
    );
    
    // Назад
    settingsButtons.emplace_back(
        centerX, startY + spacing * 3, buttonWidth, buttonHeight,
        "Back to Menu", mainFont,
        [this]() { 
            currentState = GameState::MAIN_MENU;
            background.setTexture(menuBackgroundTexture);
        }
    );
    
    // Цвета кнопок
    settingsButtons[0].setColors(sf::Color(138, 43, 226), sf::Color(148, 0, 211), sf::Color(128, 0, 128));
    settingsButtons[1].setColors(sf::Color(138, 43, 226), sf::Color(148, 0, 211), sf::Color(128, 0, 128));
    settingsButtons[2].setColors(sf::Color(70, 130, 180), sf::Color(100, 149, 237), sf::Color(30, 144, 255));
    settingsButtons[3].setColors(sf::Color(220, 20, 60), sf::Color(255, 0, 0), sf::Color(178, 34, 34));
}

void Game::setupContactForm() {
    std::cout << "Настройка формы обратной связи..." << std::endl;
    
    // Пытаемся загрузить шрифт для формы
    std::vector<std::string> fontPaths = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf"
    };
    
    for (const auto& path : fontPaths) {
        if (contactForm.loadFont(path)) {
            std::cout << "Шрифт для формы загружен: " << path << std::endl;
            break;
        }
    }
    
    // Настраиваем форму
    contactForm.setup(window.getSize().x, window.getSize().y);
}

void Game::initializeCards() {
    gameCards.clear();
    
    // Устанавливаем размеры
    switch (difficulty) {
        case Difficulty::EASY: rows = 3; cols = 4; totalPairs = 6; break;
        case Difficulty::MEDIUM: rows = 4; cols = 4; totalPairs = 8; break;
        case Difficulty::HARD: rows = 4; cols = 6; totalPairs = 12; break;
        case Difficulty::EXPERT: rows = 6; cols = 6; totalPairs = 18; break;
    }
    
    int totalCards = rows * cols;
    std::cout << "\n=== ИНИЦИАЛИЗАЦИЯ КАРТ ===" << std::endl;
    std::cout << "Поле: " << rows << "x" << cols << " = " << totalCards << " карт" << std::endl;
    std::cout << "Нужно пар: " << totalPairs << std::endl;
    
    // 1. Получаем файлы из папки текущей темы
    std::string themeFolder;
    switch (currentTheme) {
        case CardTheme::ANIMALS: themeFolder = "animals"; break;
        case CardTheme::FRUITS: themeFolder = "fruits"; break;
        case CardTheme::EMOJI: themeFolder = "emoji"; break;
        case CardTheme::MEMES: themeFolder = "memes"; break;
        case CardTheme::SYMBOLS: themeFolder = "symbols"; break;
        default: themeFolder = "animals"; break;
    }
    
    std::string imageDir = "assets/images/" + themeFolder + "/";
    std::cout << "Ищем изображения в: " << imageDir << std::endl;
    
    // 2. Собираем список доступных файлов
    std::vector<std::string> availableImages;
    try {
        for (const auto& entry : fs::directory_iterator(imageDir)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                
                if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp") {
                    availableImages.push_back(entry.path().string());
                    std::cout << "  Найдено: " << entry.path().filename() << std::endl;
                }
            }
        }
    } catch (const std::exception& e) {
        std::cout << "Ошибка доступа к папке: " << e.what() << std::endl;
    }
    
    // 3. Если нет файлов, создаем тестовые имена
    if (availableImages.empty()) {
        std::cout << "Файлы не найдены, создаем тестовые..." << std::endl;
        for (int i = 1; i <= totalPairs; i++) {
            availableImages.push_back(imageDir + "image" + std::to_string(i) + ".png");
        }
    }
    
    // 4. ГАРАНТИРУЕМ ПАРНОСТЬ
    std::vector<std::string> pairedImages;
    
    // Берем первые totalPairs уникальных изображений
    int imagesToUse = std::min(totalPairs, (int)availableImages.size());
    for (int i = 0; i < imagesToUse; i++) {
        pairedImages.push_back(availableImages[i % availableImages.size()]);
    }
    
    // Если нужно больше пар, чем есть уникальных изображений - дублируем
    while ((int)pairedImages.size() < totalPairs) {
        pairedImages.push_back(availableImages[pairedImages.size() % availableImages.size()]);
    }
    
    std::cout << "Используем " << pairedImages.size() << " изображений для пар" << std::endl;
    
    // 5. СОЗДАЕМ КАРТЫ ПАРАМИ
    int cardId = 0;
    for (int i = 0; i < totalPairs; i++) {
        std::string imagePath = pairedImages[i];
        
        // Первая карта пары
        gameCards.emplace_back(cardId++, imagePath, currentTheme);
        // Вторая карта пары (ТА ЖЕ САМАЯ!)
        gameCards.emplace_back(cardId++, imagePath, currentTheme);
        
        std::string filename = fs::path(imagePath).filename().string();
        std::cout << "  Пара #" << (i+1) << ": " << filename 
                  << " (ID: " << (cardId-2) << " и " << (cardId-1) << ")" << std::endl;
    }
    
    // 6. Перемешиваем
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(gameCards.begin(), gameCards.end(), g);
    
    // 7. ПРОВЕРКА
    std::cout << "\n📊 ПРОВЕРКА:" << std::endl;
    std::cout << "Всего карт: " << gameCards.size() << std::endl;
    std::cout << "Должно быть: " << totalCards << std::endl;
    
    if (gameCards.size() == static_cast<size_t>(totalCards)) {
        std::cout << "✅ Размер правильный!" << std::endl;
    } else {
        std::cout << "❌ ОШИБКА: неверное количество карт!" << std::endl;
        // Корректируем
        if (gameCards.size() > static_cast<size_t>(totalCards)) {
            gameCards.resize(totalCards);
        } else {
            while (gameCards.size() < static_cast<size_t>(totalCards)) {
                std::string fallback = pairedImages[gameCards.size() % pairedImages.size()];
                gameCards.emplace_back(cardId++, fallback, currentTheme);
            }
        }
    }
    
    std::cout << "=== ИНИЦИАЛИЗАЦИЯ ЗАВЕРШЕНА ===\n" << std::endl;
}

void Game::createCardSprites() {
    cards.clear();
    
    // Размеры карточек
    float cardSize = 80.0f;
    float spacing = 10.0f;
    
    // Центрируем игровое поле
    float totalWidth = cols * cardSize + (cols - 1) * spacing;
    float totalHeight = rows * cardSize + (rows - 1) * spacing;
    float startX = (window.getSize().x - totalWidth) / 2;
    float startY = (window.getSize().y - totalHeight) / 2 + 50;
    
    std::cout << "\n=== СОЗДАНИЕ СПРАЙТОВ КАРТ ===" << std::endl;
    std::cout << "Создание " << (rows * cols) << " спрайтов..." << std::endl;
    
    // Создаем спрайты карточек
    for (int i = 0; i < rows * cols && i < static_cast<int>(gameCards.size()); i++) {
        int row = i / cols;
        int col = i % cols;
        
        float x = startX + col * (cardSize + spacing);
        float y = startY + row * (cardSize + spacing);
        
        const Card& cardData = gameCards[i];
        std::string imagePath = cardData.getSymbol();
        
        auto cardSprite = std::make_unique<CardSprite>(
            cardData.getId(), imagePath, x, y, cardSize
        );
        
        // Пытаемся загрузить изображение
        if (!cardSprite->loadImage(imagePath)) {
            std::cout << "⚠ Не удалось загрузить изображение: " << imagePath << std::endl;
            // Если не удалось загрузить, используем текстовый символ
            std::string fallback = "IMG" + std::to_string((i % totalPairs) + 1);
            cardSprite->setSymbol(fallback, mainFont);
        }
        
        cardSprite->setClickable(true);
        cardSprite->hide();
        
        cards.push_back(std::move(cardSprite));
    }
    
    std::cout << "✅ Создано " << cards.size() << " спрайтов карт" << std::endl;
}

void Game::resetGame() {
    std::cout << "\n=== СБРОС ИГРЫ ===" << std::endl;
    
    // Сбрасываем состояние игры
    matchedPairs = 0;
    moves = 0;
    isGameActive = false;
    firstCardSelected = false;
    selectedCard1 = -1;
    selectedCard2 = -1;
    firstCard = nullptr;
    secondCard = nullptr;
    isChecking = false;
    isFlipping = false;
    cardFlipProgress = 0.0f;
    hasWon = false;
    consecutiveMatches = 0; // Сбрасываем счетчик последовательных совпадений
    
    std::cout << "matchedPairs сброшен на 0" << std::endl;
    std::cout << "hasWon сброшен на false" << std::endl;
    std::cout << "consecutiveMatches сброшен на 0" << std::endl;
    
    // Очищаем существующие карты
    cards.clear();
    gameCards.clear();
    
    std::cout << "Сбрасываем игрока..." << std::endl;
    if (player) {
        player->startGame();
    }
    
    std::cout << "Инициализируем новые карты..." << std::endl;
    initializeCards();
    
    std::cout << "Создаем спрайты карт..." << std::endl;
    createCardSprites();
    
    // Проверяем результат
    std::cout << "Результат инициализации:" << std::endl;
    std::cout << "  Размер поля: " << rows << "x" << cols << " = " << (rows * cols) << " карт" << std::endl;
    std::cout << "  Создано спрайтов: " << cards.size() << std::endl;
    std::cout << "  Всего пар: " << totalPairs << std::endl;
    
    if (cards.size() == static_cast<size_t>(rows * cols)) {
        std::cout << "✅ Инициализация успешна!" << std::endl;
    } else {
        std::cout << "❌ ОШИБКА: Не все спрайты созданы!" << std::endl;
        // Исправляем: создаем недостающие карты
        int neededCards = rows * cols;
        int currentCards = cards.size();
        if (currentCards < neededCards) {
            std::cout << "  Создаем недостающие " << (neededCards - currentCards) << " карт..." << std::endl;
            
            // Получаем пути к изображениям для текущей темы
            std::vector<std::string> imagePaths;
            getImagePathsForTheme(currentTheme, imagePaths);
            
            float cardSize = 80.0f;
            float spacing = 10.0f;
            float totalWidth = cols * cardSize + (cols - 1) * spacing;
            float totalHeight = rows * cardSize + (rows - 1) * spacing;
            float startX = (window.getSize().x - totalWidth) / 2;
            float startY = (window.getSize().y - totalHeight) / 2 + 50;
            
            for (int i = currentCards; i < neededCards; i++) {
                int row = i / cols;
                int col = i % cols;
                
                float x = startX + col * (cardSize + spacing);
                float y = startY + row * (cardSize + spacing);
                
                // Используем первое изображение как fallback
                std::string imagePath = !imagePaths.empty() ? imagePaths[0] : "fallback.png";
                
                auto cardSprite = std::make_unique<CardSprite>(
                    i + 1000, // ID с смещением, чтобы не конфликтовать
                    imagePath, 
                    x, y, 
                    cardSize
                );
                
                if (!cardSprite->loadImage(imagePath)) {
                    std::string fallback = "CARD" + std::to_string((i % totalPairs) + 1);
                    cardSprite->setSymbol(fallback, mainFont);
                }
                
                cardSprite->setClickable(true);
                cardSprite->hide();
                cards.push_back(std::move(cardSprite));
            }
            
            std::cout << "  ✅ Досоздано " << (neededCards - currentCards) << " карт" << std::endl;
        }
    }
    
    // Сбрасываем таймер
    gameClock.restart();
    elapsedTime = sf::Time::Zero;
    
    // Сбрасываем таймер быстрых совпадений
    pairTimer.restart();
    
    std::cout << "=== СБРОС ЗАВЕРШЕН ===\n" << std::endl;
}

void Game::updateStats() {
    std::stringstream statsSS;
    statsSS << "Player: " << (player ? player->getName() : "Guest") << "\n\n"  // Два переноса
            << "Difficulty: " << getDifficultyString() << "\n\n"
            << "Field: " << rows << "x" << cols << " (" << (rows * cols) << " cards)\n\n"
            << "Moves: " << moves << "\n\n"
            << "Pairs found: " << matchedPairs << "/" << totalPairs << "\n\n"
            << "Progress: " << std::fixed << std::setprecision(1) 
            << (totalPairs > 0 ? (matchedPairs * 100.0 / totalPairs) : 0) << "%";
    
    statsText.setString(statsSS.str());
    statsText.setLineSpacing(1.2f); // Добавляем межстрочный интервал
    
    if (player) {
        player->calculateScore(totalPairs);
        scoreText.setString("Score: " + std::to_string(player->getScore()));
    }
    
    difficultyText.setString("Difficulty: " + getDifficultyString());
    difficultyText.setFillColor(getDifficultyColor());
}

void Game::handleLoginInput(sf::Event event) {
    if (event.type == sf::Event::TextEntered) {
        if (event.text.unicode == '\b') { // Backspace
            if (activeInputField == InputField::USERNAME && !usernameInput.empty()) {
                usernameInput.pop_back();
            } else if (activeInputField == InputField::PASSWORD && !passwordInput.empty()) {
                passwordInput.pop_back();
            }
        } else if (event.text.unicode == '\t') { // Tab
            if (activeInputField == InputField::USERNAME) {
                activeInputField = InputField::PASSWORD;
            } else {
                activeInputField = InputField::USERNAME;
            }
        } else if (event.text.unicode == '\r') { // Enter
            // Автоматический вход при нажатии Enter
            if (!usernameInput.empty() && !passwordInput.empty()) {
                std::string errorMsg;
                if (userManager->login(usernameInput, passwordInput, errorMsg)) {
                    player = std::make_unique<Player>(usernameInput);
                    
                    if (achievementManager) {
                        achievementManager->setPlayerName(usernameInput);
                    }
                    
                    currentState = GameState::MAIN_MENU;
                    background.setTexture(menuBackgroundTexture);
                    
                    usernameInput = "";
                    passwordInput = "";
                    loginErrorText.setString("");
                } else {
                    loginErrorText.setString(errorMsg);
                }
            }
        } else if (event.text.unicode >= 32 && event.text.unicode < 128) {
            char c = static_cast<char>(event.text.unicode);
            
            if (activeInputField == InputField::USERNAME && usernameInput.length() < 20) {
                usernameInput += c;
            } else if (activeInputField == InputField::PASSWORD && passwordInput.length() < 30) {
                passwordInput += c;
            }
        }
    }
}

void Game::handleRegisterInput(sf::Event event) {
    if (event.type == sf::Event::TextEntered) {
        if (event.text.unicode == '\b') { // Backspace
            if (activeInputField == InputField::USERNAME && !usernameInput.empty()) {
                usernameInput.pop_back();
            } else if (activeInputField == InputField::EMAIL && !emailInput.empty()) {
                emailInput.pop_back();
            } else if (activeInputField == InputField::PASSWORD && !passwordInput.empty()) {
                passwordInput.pop_back();
            } else if (activeInputField == InputField::CONFIRM_PASSWORD && !confirmPasswordInput.empty()) {
                confirmPasswordInput.pop_back();
            }
        } else if (event.text.unicode == '\t') { // Tab
            switch (activeInputField) {
                case InputField::USERNAME:
                    activeInputField = InputField::EMAIL;
                    break;
                case InputField::EMAIL:
                    activeInputField = InputField::PASSWORD;
                    break;
                case InputField::PASSWORD:
                    activeInputField = InputField::CONFIRM_PASSWORD;
                    break;
                default:
                    activeInputField = InputField::USERNAME;
                    break;
            }
        } else if (event.text.unicode >= 32 && event.text.unicode < 128) {
            char c = static_cast<char>(event.text.unicode);
            
            if (activeInputField == InputField::USERNAME && usernameInput.length() < 20) {
                usernameInput += c;
            } else if (activeInputField == InputField::EMAIL && emailInput.length() < 50) {
                emailInput += c;
            } else if (activeInputField == InputField::PASSWORD && passwordInput.length() < 30) {
                passwordInput += c;
            } else if (activeInputField == InputField::CONFIRM_PASSWORD && confirmPasswordInput.length() < 30) {
                confirmPasswordInput += c;
            }
        }
    }
}

void Game::handleEvents() {
    sf::Event event;
    sf::Vector2f mousePos = static_cast<sf::Vector2f>(sf::Mouse::getPosition(window));
    
    while (window.pollEvent(event)) {
        if (event.type == sf::Event::Closed) {
            window.close();
        }
        
        if (event.type == sf::Event::Resized) {
            sf::FloatRect visibleArea(0, 0, event.size.width, event.size.height);
            window.setView(sf::View(visibleArea));
            updateBackgrounds();
        }
        
        // Обработка колесика мыши для скроллинга достижений
        if (currentState == GameState::ACHIEVEMENTS && event.type == sf::Event::MouseWheelScrolled) {
            if (event.mouseWheelScroll.wheel == sf::Mouse::VerticalWheel) {
                auto allAchievements = achievementManager ? achievementManager->getAllAchievements() : std::vector<Achievement>();
                float totalContentHeight = allAchievements.size() * 70.0f;
                float visibleHeight = 400;
                
                if (totalContentHeight > visibleHeight) {
                    achievementsScrollOffset -= event.mouseWheelScroll.delta * 20.0f; // Скорость прокрутки
                    
                    // Ограничиваем смещение
                    float maxScroll = totalContentHeight - visibleHeight;
                    achievementsScrollOffset = std::max(0.0f, std::min(maxScroll, achievementsScrollOffset));
                }
            }
        }
        
        switch (currentState) {
            case GameState::LOGIN_SCREEN:
                handleLoginInput(event);
                for (auto& button : loginButtons) {
                    button.handleEvent(event, mousePos);
                }
                
                // Обработка кликов по полям ввода
                if (event.type == sf::Event::MouseButtonPressed) {
                    if (event.mouseButton.button == sf::Mouse::Left) {
                        sf::FloatRect usernameBounds(400, 290, 400, 40);
                        sf::FloatRect passwordBounds(400, 390, 400, 40);
                        
                        if (usernameBounds.contains(mousePos)) {
                            activeInputField = InputField::USERNAME;
                        } else if (passwordBounds.contains(mousePos)) {
                            activeInputField = InputField::PASSWORD;
                        } else {
                            activeInputField = InputField::NONE;
                        }
                    }
                }
                break;
                
            case GameState::REGISTER_SCREEN:
                handleRegisterInput(event);
                for (auto& button : registerButtons) {
                    button.handleEvent(event, mousePos);
                }
                
                // Обработка кликов по полям ввода
                if (event.type == sf::Event::MouseButtonPressed) {
                    if (event.mouseButton.button == sf::Mouse::Left) {
                        sf::FloatRect usernameBounds(400, 190, 400, 40);
                        sf::FloatRect emailBounds(400, 290, 400, 40);
                        sf::FloatRect passwordBounds(400, 390, 400, 40);
                        sf::FloatRect confirmBounds(400, 490, 400, 40);
                        
                        if (usernameBounds.contains(mousePos)) {
                            activeInputField = InputField::USERNAME;
                        } else if (emailBounds.contains(mousePos)) {
                            activeInputField = InputField::EMAIL;
                        } else if (passwordBounds.contains(mousePos)) {
                            activeInputField = InputField::PASSWORD;
                        } else if (confirmBounds.contains(mousePos)) {
                            activeInputField = InputField::CONFIRM_PASSWORD;
                        } else {
                            activeInputField = InputField::NONE;
                        }
                    }
                }
                break;
                
            case GameState::MAIN_MENU:
                for (auto& button : mainMenuButtons) {
                    button.handleEvent(event, mousePos);
                }
                break;
                
            case GameState::ENTER_NAME:
                if (event.type == sf::Event::TextEntered) {
                    if (event.text.unicode == '\b') {
                        if (!playerNameInput.empty()) {
                            playerNameInput.pop_back();
                        }
                    } else if (event.text.unicode == '\r') {
                        if (!playerNameInput.empty()) {
                            player = std::make_unique<Player>(playerNameInput);
                            if (achievementManager) {
                                achievementManager->setPlayerName(playerNameInput);
                            }
                            currentState = GameState::SETUP;
                            isEnteringName = false;
                            std::cout << "Игрок создан: " << playerNameInput << std::endl;
                        }
                    } else if (event.text.unicode >= 32 && event.text.unicode < 128) {
                        if (playerNameInput.length() < 20) {
                            playerNameInput += static_cast<char>(event.text.unicode);
                        }
                    }
                }
                break;
                
            case GameState::SETUP:
                for (auto& button : setupButtons) {
                    button.handleEvent(event, mousePos);
                }
                break;
                
            case GameState::PLAYING:
                if (event.type == sf::Event::MouseButtonPressed) {
                    if (event.mouseButton.button == sf::Mouse::Left) {
                        for (size_t i = 0; i < cards.size(); i++) {
                            if (cards[i]->contains(mousePos) && 
                                cards[i]->getState() == CardState::HIDDEN &&
                                cards[i]->getIsClickable() &&
                                !isFlipping && !isChecking) {
                                handleCardClick(i);
                                break;
                            }
                        }
                        
                        surrenderButton.handleEvent(event, mousePos);
                    }
                }
                for (auto& button : gameButtons) {
                    button.handleEvent(event, mousePos);
                }
                break;
                
            case GameState::PAUSED:
                for (auto& button : pauseButtons) {
                    button.handleEvent(event, mousePos);
                }
                break;
                
            case GameState::GAME_OVER_WIN:
            case GameState::GAME_OVER_LOSE:
                if (event.type == sf::Event::MouseButtonPressed) {
                    if (event.mouseButton.button == sf::Mouse::Left) {
                        if (mousePos.x >= window.getSize().x / 2 - 150 && 
                            mousePos.x <= window.getSize().x / 2 + 150 &&
                            mousePos.y >= window.getSize().y - 150 && 
                            mousePos.y <= window.getSize().y - 90) {
                            currentState = GameState::MAIN_MENU;
                            background.setTexture(menuBackgroundTexture);
                        }
                    }
                }
                break;
                
            case GameState::LEADERBOARD:
                for (auto& button : leaderboardButtons) {
                    button.handleEvent(event, mousePos);
                }
                break;
                
            case GameState::ACHIEVEMENTS:
                for (auto& button : achievementsButtons) {
                    button.handleEvent(event, mousePos);
                }
                break;
                
            case GameState::SETTINGS:
                for (auto& button : settingsButtons) {
                    button.handleEvent(event, mousePos);
                }
                break;
                
            case GameState::CONTACT_FORM:
                contactForm.handleEvent(event, mousePos);
                
                if (event.type == sf::Event::MouseButtonPressed) {
                    if (event.mouseButton.button == sf::Mouse::Left) {
                        if (contactForm.isMouseOverBackButton(mousePos)) {
                            currentState = previousState;
                            background.setTexture(menuBackgroundTexture);
                        }
                    }
                }
                break;
                
            case GameState::EXIT:
                window.close();
                break;
        }
    }
}

void Game::update(float deltaTime) {
    sf::Vector2f mousePos = static_cast<sf::Vector2f>(sf::Mouse::getPosition(window));
    
    switch (currentState) {
        case GameState::MAIN_MENU:
            for (auto& button : mainMenuButtons) button.update(mousePos);
            break;
            
        case GameState::ENTER_NAME:
            break;
            
        case GameState::SETUP:
            for (auto& button : setupButtons) button.update(mousePos);
            break;
            
        case GameState::PLAYING:
            if (isGameActive) {
                elapsedTime = gameClock.getElapsedTime();
                
                int seconds = static_cast<int>(elapsedTime.asSeconds());
                int minutes = seconds / 60;
                seconds %= 60;
                std::stringstream timeSS;
                timeSS << std::setfill('0') << std::setw(2) << minutes << ":"
                       << std::setfill('0') << std::setw(2) << seconds;
                timerText.setString("Time: " + timeSS.str());
                
                updateStats();
            }
            
            for (auto& button : gameButtons) button.update(mousePos);
            surrenderButton.update(mousePos);
            
            if (isFlipping) {
                cardFlipProgress += deltaTime;
                if (cardFlipProgress >= cardFlipTime) {
                    cardFlipProgress = 0.0f;
                    isFlipping = false;
                    
                    if (firstCard && secondCard) {
                        isChecking = true;
                        processCardMatch();
                    }
                }
            }
            break;
            
        case GameState::PAUSED:
            for (auto& button : pauseButtons) button.update(mousePos);
            break;
            
        case GameState::GAME_OVER_WIN:
            break;
            
        case GameState::GAME_OVER_LOSE:
            break;
            
        case GameState::LEADERBOARD:
            for (auto& button : leaderboardButtons) button.update(mousePos);
            break;
            
        case GameState::ACHIEVEMENTS:
            for (auto& button : achievementsButtons) button.update(mousePos);
            break;
            
        case GameState::SETTINGS:
            for (auto& button : settingsButtons) button.update(mousePos);
            break;
            
        case GameState::CONTACT_FORM:
            contactForm.update(mousePos);
            break;
            
        case GameState::EXIT:
            break;
    }
    
    for (auto& card : cards) {
        card->update(deltaTime);
    }
}

void Game::render() {
    window.clear();
    
    // Устанавливаем правильный фон в зависимости от состояния
    if (currentState == GameState::LOGIN_SCREEN || 
        currentState == GameState::REGISTER_SCREEN ||
        currentState == GameState::MAIN_MENU || 
        currentState == GameState::SETUP ||
        currentState == GameState::LEADERBOARD ||
        currentState == GameState::ENTER_NAME ||
        currentState == GameState::SETTINGS ||
        currentState == GameState::ACHIEVEMENTS) {
        background.setTexture(menuBackgroundTexture);
    } else {
        background.setTexture(gameBackgroundTexture);
    }
    
    window.draw(background);
    
    switch (currentState) {
        case GameState::LOGIN_SCREEN:
            renderLoginScreen();
            break;
            
        case GameState::REGISTER_SCREEN:
            renderRegisterScreen();
            break;
            
        case GameState::MAIN_MENU:
            renderMainMenu();
            break;
            
        case GameState::ENTER_NAME:
            renderNameInput();
            break;
            
        case GameState::SETUP:
            renderSetupMenu();
            break;
            
        case GameState::PLAYING:
            renderGame();
            break;
            
        case GameState::PAUSED:
            renderPauseMenu();
            break;
            
        case GameState::GAME_OVER_WIN:
            renderGameOverWin();
            break;
            
        case GameState::GAME_OVER_LOSE:
            renderGameOverLose();
            break;
            
        case GameState::LEADERBOARD:
            renderLeaderboard();
            break;
            
        case GameState::ACHIEVEMENTS:
            renderAchievements();
            break;
            
        case GameState::SETTINGS:
            renderSettings();
            break;
            
        case GameState::CONTACT_FORM:
            renderContactForm();
            break;
            
        default:
            // Резервный рендеринг
            sf::Text defaultText("Game State: " + std::to_string(static_cast<int>(currentState)), mainFont, 32);
            defaultText.setFillColor(sf::Color::White);
            defaultText.setPosition(100, 100);
            window.draw(defaultText);
            break;
    }
    
    window.display();
}

void Game::renderGameOverWin() {
    std::cout << "=== ОТРИСОВКА ЭКРАНА ПОБЕДЫ ===" << std::endl;
    std::cout << "Статистика: " << matchedPairs << "/" << totalPairs << " пар" << std::endl;
    
    // Поздравление с победой
    sf::Text victoryText("VICTORY!", mainFont, 72);
    victoryText.setFillColor(sf::Color(255, 215, 0));
    victoryText.setStyle(sf::Text::Bold);
    
    sf::FloatRect bounds = victoryText.getLocalBounds();
    victoryText.setOrigin(bounds.left + bounds.width / 2.0f,
                         bounds.top + bounds.height / 2.0f);
    victoryText.setPosition(window.getSize().x / 2, 150);
    window.draw(victoryText);
    
    // Проверяем полученные достижения
    if (achievementManager) {
        auto unlockedAchievements = achievementManager->getUnlockedAchievements();
        
        // Показываем только недавно полученные достижения (первые 3)
        int recentAchievements = 0;
        for (const auto& achievement : unlockedAchievements) {
            if (recentAchievements < 3) {
                // Иконка достижения
                sf::Text achievementIcon(achievement.icon, mainFont, 36);
                sf::Color rarityColor = achievement.getRarityColor();
                
                achievementIcon.setFillColor(rarityColor);
                achievementIcon.setPosition(100 + recentAchievements * 150, 400);
                window.draw(achievementIcon);
                
                // Название достижения
                sf::Text achievementName(achievement.title, mainFont, 18);
                achievementName.setFillColor(sf::Color::White);
                achievementName.setPosition(100 + recentAchievements * 150, 450);
                window.draw(achievementName);
                
                recentAchievements++;
            }
        }
        
        if (recentAchievements > 0) {
            sf::Text newAchievementsText("New Achievements Unlocked!", mainFont, 24);
            newAchievementsText.setFillColor(sf::Color::Green);
            newAchievementsText.setPosition(window.getSize().x / 2 - 150, 350);
            window.draw(newAchievementsText);
        }
    }
    
    // Статистика
    if (player) {
        std::stringstream stats;
        stats << "Player: " << player->getName() << "\n\n";
        stats << "Final Score: " << player->getScore() << "\n";
        stats << "Moves: " << moves << "\n";
        stats << "Perfect Match: " << (moves == totalPairs ? "YES!" : "No") << "\n";
        stats << "Time: " << (int)elapsedTime.asSeconds() << " seconds\n";
        stats << "Difficulty: " << getDifficultyString();
        
        sf::Text statsText(stats.str(), mainFont, 32);
        statsText.setFillColor(sf::Color::White);
        statsText.setPosition(window.getSize().x / 2 - 200, 200);
        window.draw(statsText);
    }
    
    // Кнопка продолжения
    sf::RectangleShape continueButton(sf::Vector2f(300, 60));
    continueButton.setPosition(window.getSize().x / 2 - 150, window.getSize().y - 150);
    
    sf::Vector2f mousePos = static_cast<sf::Vector2f>(sf::Mouse::getPosition(window));
    bool isMouseOverButton = continueButton.getGlobalBounds().contains(mousePos);
    
    if (isMouseOverButton) {
        continueButton.setFillColor(sf::Color(50, 205, 50));
        continueButton.setOutlineColor(sf::Color::Yellow);
    } else {
        continueButton.setFillColor(sf::Color(0, 200, 0));
        continueButton.setOutlineColor(sf::Color::White);
    }
    
    continueButton.setOutlineThickness(2);
    window.draw(continueButton);
    
    sf::Text continueText("Continue to Menu", mainFont, 28);
    continueText.setFillColor(sf::Color::White);
    sf::FloatRect continueBounds = continueText.getLocalBounds();
    continueText.setOrigin(continueBounds.left + continueBounds.width / 2.0f,
                          continueBounds.top + continueBounds.height / 2.0f);
    continueText.setPosition(window.getSize().x / 2, window.getSize().y - 120);
    
    // Тень
    sf::Text shadowText = continueText;
    shadowText.setFillColor(sf::Color(0, 0, 0, 150));
    shadowText.move(2, 2);
    window.draw(shadowText);
    
    window.draw(continueText);
    
    std::cout << "✅ Экран победы отрисован" << std::endl;
}

void Game::handleCardClick(int cardIndex) {
    if (!isGameActive || isFlipping || isChecking) {
        return;
    }
    
    if (cardIndex < 0 || cardIndex >= static_cast<int>(cards.size())) {
        return;
    }
    
    if (!cards[cardIndex]) {
        return;
    }
    
    if (cards[cardIndex]->getState() == CardState::HIDDEN && !cards[cardIndex]->getIsClickable()) {
        return;
    }
    
    // Звук переворота
    if (soundManager) {
        soundManager->playCardFlip();
    }
    
    // Запускаем таймер для быстрого нахождения пары
    if (!firstCardSelected) {
        pairTimer.restart();
    }
    
    // Переворачиваем карту
    cards[cardIndex]->reveal();
    cards[cardIndex]->setClickable(false);
    
    if (!firstCardSelected) {
        // Первая карта
        firstCardSelected = true;
        selectedCard1 = cardIndex;
        firstCard = cards[cardIndex].get();
        isFlipping = true;
        cardFlipProgress = 0.0f;
    } else {
        // Вторая карта
        selectedCard2 = cardIndex;
        secondCard = cards[cardIndex].get();
        isFlipping = true;
        cardFlipProgress = 0.0f;
        
        // Увеличиваем счетчик ходов
        moves++;
        if (player) {
            player->incrementMoves();
        }
        
        firstCardSelected = false;
    }
}

void Game::processCardMatch() {
    std::cout << "=== ПРОВЕРКА СОВПАДЕНИЯ КАРТ ===" << std::endl;
    std::cout << "Найдено пар: " << matchedPairs << "/" << totalPairs << std::endl;
    
    if (!firstCard || !secondCard || !isChecking) {
        std::cout << "Ошибка: карты не инициализированы" << std::endl;
        return;
    }
    
    // Проверяем совпадение
    bool match = (firstCard->getSymbol() == secondCard->getSymbol());
    std::cout << "Символ 1: '" << firstCard->getSymbol() << "'" << std::endl;
    std::cout << "Символ 2: '" << secondCard->getSymbol() << "'" << std::endl;
    std::cout << "Совпадение: " << (match ? "ДА" : "НЕТ") << std::endl;
    
    if (match) {
        // Совпадение
        if (soundManager) {
            soundManager->playCardMatch();
        }
        
        // Обновляем статистику для достижений
        consecutiveMatches++;
        
        // Проверяем достижение "Combo Master" (3 совпадения подряд без ошибок)
        if (consecutiveMatches >= 3 && achievementManager) {
            std::cout << "🎯 3 matches in a row! Combo Master progress" << std::endl;
            achievementManager->recordPerfectMatch();
            achievementManager->updateAchievement(AchievementType::COMBO_MASTER);
        }
        
        // Помечаем как совпавшие
        firstCard->markMatched();
        secondCard->markMatched();
        
        // Увеличиваем счетчик совпавших пар
        matchedPairs++;
        std::cout << "✅ НОВАЯ ПАРА НАЙДЕНА! Всего: " << matchedPairs << "/" << totalPairs << std::endl;
        
        if (player) {
            player->incrementMatchedPairs();
        }
        
        // Обновляем статистику для достижений
        if (achievementManager) {
            // Обновляем общее количество найденных пар
            achievementManager->updateAchievement(AchievementType::MATCH_FANATIC, 1);
            
            // Проверяем быстрый подбор пары
            double pairTime = pairTimer.getElapsedTime().asSeconds();
            if (pairTime < 3.0) {
                std::cout << "⚡ Quick match: " << pairTime << " seconds" << std::endl;
                achievementManager->recordQuickMatch(pairTime);
                achievementManager->updateAchievement(AchievementType::QUICK_THINKER);
            }
        }
        
        // Обновляем счет
        if (player) {
            player->calculateScore(totalPairs);
        }
        
        // ПРОВЕРКА ПОБЕДЫ 
        if (matchedPairs >= totalPairs && !hasWon) {
            std::cout << "🎉🎉🎉 ПОБЕДА! ВСЕ ПАРЫ НАЙДЕНЫ! 🎉🎉🎉" << std::endl;
            std::cout << "Условие: " << matchedPairs << " >= " << totalPairs << std::endl;
            
            hasWon = true;
            isGameActive = false;
            
            if (player) {
                player->finishGame();
                player->calculateScore(totalPairs);
                saveGameResult();
                
                // Проверяем и сохраняем достижения
                checkAchievements();
                
                // Сохраняем достижения сразу после проверки
                if (achievementManager) {
                    std::string achievementsPath;
                    if (isRunningInDockerInternal()) {
                        achievementsPath = "/app/database/achievements_" + player->getName() + ".dat";
                    } else {
                        achievementsPath = "achievements_" + player->getName() + ".dat";
                    }
                    std::cout << "💾 Сохраняем достижения после победы: " << achievementsPath << std::endl;
                    achievementManager->saveToFile(achievementsPath);
                }
            }
            
            if (soundManager) {
                soundManager->playGameWin();
            }
            
            currentState = GameState::GAME_OVER_WIN;
            std::cout << "Состояние изменено на GAME_OVER_WIN" << std::endl;
            return;
        }
    } else {
        // Несовпадение
        if (soundManager) {
            soundManager->playCardMismatch();
        }
        
        // Сбрасываем счетчик последовательных совпадений
        consecutiveMatches = 0;
        
        isChecking = false;
        
        // Небольшая задержка
        sf::Clock delayClock;
        while (delayClock.getElapsedTime().asSeconds() < 0.8f) {
            // Ждем
        }
        
        firstCard->hide();
        secondCard->hide();
        firstCard->setClickable(true);
        secondCard->setClickable(true);
        std::cout << "❌ Карты не совпали, переворачиваем обратно" << std::endl;
    }
    
    // Сбрасываем выбор только если не победили
    if (currentState != GameState::GAME_OVER_WIN) {
        firstCard = nullptr;
        secondCard = nullptr;
        selectedCard1 = -1;
        selectedCard2 = -1;
        isChecking = false;
    }
    
    std::cout << "=== ПРОВЕРКА ЗАВЕРШЕНА ===\n" << std::endl;
}

void Game::logoutUser() {
    if (userManager && userManager->isUserLoggedIn()) {
        // Сохраняем достижения перед выходом
        if (achievementManager && player) {
            std::string achievementsPath;
            if (isRunningInDockerInternal()) {
                achievementsPath = "/app/database/achievements_" + player->getName() + ".dat";
            } else {
                achievementsPath = "achievements_" + player->getName() + ".dat";
            }
            
            std::cout << "💾 Сохраняем достижения перед выходом из аккаунта..." << std::endl;
            achievementManager->saveToFile(achievementsPath);
        }
        
        userManager->logout();
        achievementManager.reset(); // Сбрасываем менеджер достижений
        player.reset(); // Сбрасываем игрока
        
        // Возвращаемся на экран входа
        currentState = GameState::LOGIN_SCREEN;
        background.setTexture(menuBackgroundTexture);
    }
}
