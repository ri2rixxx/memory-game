#include "Database.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <fstream>

Database::Database(const std::string& connStr) 
    : connection(nullptr) {
    
    if (connStr.empty()) {
        // Автоматическое определение окружения
        char* dbUrl = std::getenv("DATABASE_URL");
        if (dbUrl) {
            connectionString = dbUrl;
            std::cout << "Using DATABASE_URL from environment" << std::endl;
        } else {
            // Проверяем, в Docker ли мы
            std::ifstream dockerFile("/.dockerenv");
            if (dockerFile.good()) {
                connectionString = "host=postgres dbname=memory_game_db user=game_user password=game_password";
                std::cout << "Docker detected, using PostgreSQL container" << std::endl;
                dockerFile.close();  // Закрываем файл
            } else {
                connectionString = "host=localhost dbname=memory_game_db user=game_user password=game_password";
                std::cout << "Local environment, using localhost PostgreSQL" << std::endl;
            }
        }
    } else {
        connectionString = connStr;
    }
    
    std::cout << "PostgreSQL connection string: " << connectionString << std::endl;
}

Database::~Database() {
    disconnect();
}

bool Database::connect() {
    if (connection) {
        std::cout << "Already connected to database" << std::endl;
        return true;
    }
    
    std::cout << "Connecting to PostgreSQL database..." << std::endl;
    connection = PQconnectdb(connectionString.c_str());
    
    if (PQstatus(connection) != CONNECTION_OK) {
        logError("Connection");
        return false;
    }
    
    std::cout << "✅ Connected to PostgreSQL database: " << PQdb(connection) 
              << " (Server: " << PQhost(connection) << ":" << PQport(connection) << ")" << std::endl;
    return true;
}

bool Database::disconnect() {
    if (connection) {
        PQfinish(connection);
        connection = nullptr;
        std::cout << "Disconnected from PostgreSQL database" << std::endl;
    }
    return true;
}

bool Database::isConnected() const {
    return connection && PQstatus(connection) == CONNECTION_OK;
}

void Database::logError(const std::string& operation) {
    if (connection) {
        std::cerr << "❌ PostgreSQL error during " << operation << ": " 
                  << PQerrorMessage(connection) << std::endl;
    } else {
        std::cerr << "❌ PostgreSQL error: No connection" << std::endl;
    }
}

bool Database::executeQuery(const std::string& query) {
    if (!isConnected()) {
        if (!connect()) return false;
    }
    
    PGresult* result = PQexec(connection, query.c_str());
    if (PQresultStatus(result) != PGRES_COMMAND_OK && 
        PQresultStatus(result) != PGRES_TUPLES_OK) {
        logError("Query execution: " + query);
        PQclear(result);
        return false;
    }
    
    PQclear(result);
    return true;
}

PGresult* Database::executeQueryWithResult(const std::string& query) {
    if (!isConnected()) {
        if (!connect()) return nullptr;
    }
    
    PGresult* result = PQexec(connection, query.c_str());
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        logError("Query with result: " + query);
        PQclear(result);
        return nullptr;
    }
    
    return result;
}

bool Database::initialize() {
    std::cout << "Initializing PostgreSQL database..." << std::endl;
    
    if (!isConnected()) {
        if (!connect()) return false;
    }
    
    // Проверяем существование таблицы games
    const char* checkTableQuery = 
        "SELECT EXISTS ("
        "SELECT FROM information_schema.tables "
        "WHERE table_schema = 'public' "
        "AND table_name = 'games'"
        ");";
    
    PGresult* result = PQexec(connection, checkTableQuery);
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        PQclear(result);
        return false;
    }
    
    bool tableExists = (PQgetvalue(result, 0, 0)[0] == 't');
    PQclear(result);
    
    if (!tableExists) {
        std::cout << "⚠ Tables don't exist, running initialization..." << std::endl;
        
        const char* createTables = 
            "CREATE TABLE IF NOT EXISTS games ("
            "id SERIAL PRIMARY KEY,"
            "player_name VARCHAR(100) NOT NULL,"
            "score INTEGER NOT NULL,"
            "moves INTEGER NOT NULL,"
            "pairs INTEGER NOT NULL,"
            "time DOUBLE PRECISION NOT NULL,"
            "date TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
            "difficulty VARCHAR(20) NOT NULL"
            ");";
        
        if (!executeQuery(createTables)) {
            return false;
        }
    }
    
    std::cout << "✅ PostgreSQL database initialized" << std::endl;
    return true;
}

bool Database::saveGame(const GameRecord& record) {
    std::string query = 
        "INSERT INTO games (player_name, score, moves, pairs, time, date, difficulty) "
        "VALUES ($1, $2, $3, $4, $5, $6, $7);";
    
    const char* paramValues[7];
    std::string paramStrings[7];
    
    paramStrings[0] = record.playerName;
    paramStrings[1] = std::to_string(record.score);
    paramStrings[2] = std::to_string(record.moves);
    paramStrings[3] = std::to_string(record.pairs);
    paramStrings[4] = std::to_string(record.time);
    paramStrings[5] = record.date;
    paramStrings[6] = record.difficulty;
    
    for (int i = 0; i < 7; i++) {
        paramValues[i] = paramStrings[i].c_str();
    }
    
    PGresult* result = PQexecParams(connection, query.c_str(), 7, 
                                   nullptr, paramValues, nullptr, nullptr, 0);
    
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        logError("Save game");
        PQclear(result);
        return false;
    }
    
    PQclear(result);
    std::cout << "💾 Game saved to PostgreSQL: " << record.playerName 
              << " - " << record.score << " points" << std::endl;
    return true;
}

std::vector<GameRecord> Database::getTopScores(int limit) {
    std::vector<GameRecord> records;
    
    std::string query = 
        "SELECT id, player_name, score, moves, pairs, time, "
        "TO_CHAR(date, 'YYYY-MM-DD HH24:MI:SS'), difficulty "
        "FROM games "
        "ORDER BY score DESC "
        "LIMIT $1;";
    
    const char* paramValues[1];
    std::string limitStr = std::to_string(limit);
    paramValues[0] = limitStr.c_str();
    
    PGresult* result = PQexecParams(connection, query.c_str(), 1, 
                                   nullptr, paramValues, nullptr, nullptr, 0);
    
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        logError("Get top scores");
        PQclear(result);
        return records;
    }
    
    int rowCount = PQntuples(result);
    for (int i = 0; i < rowCount; i++) {
        GameRecord record;
        record.id = std::stoi(PQgetvalue(result, i, 0));
        record.playerName = PQgetvalue(result, i, 1);
        record.score = std::stoi(PQgetvalue(result, i, 2));
        record.moves = std::stoi(PQgetvalue(result, i, 3));
        record.pairs = std::stoi(PQgetvalue(result, i, 4));
        record.time = std::stod(PQgetvalue(result, i, 5));
        record.date = PQgetvalue(result, i, 6);
        record.difficulty = PQgetvalue(result, i, 7);
        
        records.push_back(record);
    }
    
    PQclear(result);
    return records;
}

std::vector<GameRecord> Database::getPlayerHistory(const std::string& playerName, int limit) {
    std::vector<GameRecord> records;
    
    std::string query = 
        "SELECT id, player_name, score, moves, pairs, time, "
        "TO_CHAR(date, 'YYYY-MM-DD HH24:MI:SS'), difficulty "
        "FROM games "
        "WHERE player_name = $1 "
        "ORDER BY date DESC "
        "LIMIT $2;";
    
    const char* paramValues[2];
    paramValues[0] = playerName.c_str();
    std::string limitStr = std::to_string(limit);
    paramValues[1] = limitStr.c_str();
    
    PGresult* result = PQexecParams(connection, query.c_str(), 2, 
                                   nullptr, paramValues, nullptr, nullptr, 0);
    
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        logError("Get player history");
        PQclear(result);
        return records;
    }
    
    int rowCount = PQntuples(result);
    for (int i = 0; i < rowCount; i++) {
        GameRecord record;
        record.id = std::stoi(PQgetvalue(result, i, 0));
        record.playerName = PQgetvalue(result, i, 1);
        record.score = std::stoi(PQgetvalue(result, i, 2));
        record.moves = std::stoi(PQgetvalue(result, i, 3));
        record.pairs = std::stoi(PQgetvalue(result, i, 4));
        record.time = std::stod(PQgetvalue(result, i, 5));
        record.date = PQgetvalue(result, i, 6);
        record.difficulty = PQgetvalue(result, i, 7);
        
        records.push_back(record);
    }
    
    PQclear(result);
    return records;
}

bool Database::createUser(const std::string& username, const std::string& password, 
                         const std::string& email, std::string& errorMsg) {
    
    std::cout << "DEBUG: Creating user: " << username << std::endl;
    
    if (!isConnected()) {
        std::cout << "DEBUG: Database not connected, trying to connect..." << std::endl;
        if (!connect()) {
            errorMsg = "Cannot connect to database";
            std::cout << "DEBUG: Connect failed: " << getLastError() << std::endl;
            return false;
        }
    }
    
    // Проверяем, существует ли таблица users
    std::string checkTable = 
        "SELECT EXISTS (SELECT FROM information_schema.tables "
        "WHERE table_schema = 'public' AND table_name = 'users');";
    
    PGresult* tableResult = PQexec(connection, checkTable.c_str());
    if (PQresultStatus(tableResult) != PGRES_TUPLES_OK) {
        errorMsg = "Cannot check users table: " + std::string(PQerrorMessage(connection));
        PQclear(tableResult);
        return false;
    }
    
    bool tableExists = (PQgetvalue(tableResult, 0, 0)[0] == 't');
    PQclear(tableResult);
    
    if (!tableExists) {
        std::cout << "DEBUG: Users table doesn't exist, creating..." << std::endl;
        
        // Создаем таблицу пользователей
        std::string createUsers = 
            "CREATE TABLE users ("
            "id SERIAL PRIMARY KEY,"
            "username VARCHAR(50) UNIQUE NOT NULL,"
            "password VARCHAR(100) NOT NULL,"
            "email VARCHAR(100) UNIQUE NOT NULL,"
            "registration_date TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
            "last_login TIMESTAMP"
            ");";
        
        if (!executeQuery(createUsers)) {
            errorMsg = "Failed to create users table: " + getLastError();
            return false;
        }
        
        // Создаем таблицу статистики
        std::string createStats = 
            "CREATE TABLE user_stats ("
            "id SERIAL PRIMARY KEY,"
            "user_id INTEGER REFERENCES users(id) ON DELETE CASCADE,"
            "total_score INTEGER DEFAULT 0,"
            "games_played INTEGER DEFAULT 0,"
            "games_won INTEGER DEFAULT 0,"
            "total_play_time DOUBLE PRECISION DEFAULT 0.0"
            ");";
        
        if (!executeQuery(createStats)) {
            errorMsg = "Failed to create user_stats table: " + getLastError();
            return false;
        }
        
        std::cout << "DEBUG: Tables created successfully" << std::endl;
    }
    
    // Проверяем, существует ли пользователь
    std::string checkUser = 
        "SELECT COUNT(*) FROM users WHERE username = $1 OR email = $2;";
    
    const char* checkParams[2];
    checkParams[0] = username.c_str();
    checkParams[1] = email.c_str();
    
    PGresult* checkResult = PQexecParams(connection, checkUser.c_str(), 2, 
                                        nullptr, checkParams, nullptr, nullptr, 0);
    
    if (PQresultStatus(checkResult) != PGRES_TUPLES_OK) {
        errorMsg = "Database error checking user: " + std::string(PQerrorMessage(connection));
        PQclear(checkResult);
        return false;
    }
    
    int existingCount = std::stoi(PQgetvalue(checkResult, 0, 0));
    PQclear(checkResult);
    
    if (existingCount > 0) {
        errorMsg = "Username or email already exists";
        return false;
    }
    
    // Создаем пользователя
    std::string insertUser = 
        "INSERT INTO users (username, password, email) "
        "VALUES ($1, $2, $3) "
        "RETURNING id;";
    
    const char* insertParams[3];
    insertParams[0] = username.c_str();
    insertParams[1] = password.c_str();
    insertParams[2] = email.c_str();
    
    PGresult* insertResult = PQexecParams(connection, insertUser.c_str(), 3, 
                                         nullptr, insertParams, nullptr, nullptr, 0);
    
    if (PQresultStatus(insertResult) != PGRES_TUPLES_OK) {
        errorMsg = "Failed to create user: " + std::string(PQerrorMessage(connection));
        PQclear(insertResult);
        return false;
    }
    
    int userId = std::stoi(PQgetvalue(insertResult, 0, 0));
    PQclear(insertResult);
    
    // Создаем запись статистики
    std::string insertStats = 
        "INSERT INTO user_stats (user_id) VALUES ($1);";
    
    const char* statsParam[1];
    std::string userIdStr = std::to_string(userId);
    statsParam[0] = userIdStr.c_str();
    
    PGresult* statsResult = PQexecParams(connection, insertStats.c_str(), 1, 
                                        nullptr, statsParam, nullptr, nullptr, 0);
    
    if (PQresultStatus(statsResult) != PGRES_COMMAND_OK) {
        errorMsg = "Failed to create user stats: " + std::string(PQerrorMessage(connection));
        PQclear(statsResult);
        return false;
    }
    
    PQclear(statsResult);
    
    std::cout << "DEBUG: User created successfully: " << username << " (ID: " << userId << ")" << std::endl;
    return true;
}

bool Database::authenticateUser(const std::string& username, const std::string& password, 
                               std::string& errorMsg) {
    std::string query = 
        "SELECT id, password FROM users WHERE username = $1;";
    
    const char* paramValues[1];
    paramValues[0] = username.c_str();
    
    PGresult* result = PQexecParams(connection, query.c_str(), 1, 
                                   nullptr, paramValues, nullptr, nullptr, 0);
    
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        errorMsg = "Database error";
        PQclear(result);
        return false;
    }
    
    if (PQntuples(result) == 0) {
        errorMsg = "User not found";
        PQclear(result);
        return false;
    }
    
    std::string dbPassword = PQgetvalue(result, 0, 1);
    bool authenticated = (dbPassword == password);
    
    if (!authenticated) {
        errorMsg = "Invalid password";
    }
    
    PQclear(result);
    return authenticated;
}

bool Database::updateUserStats(int userId, int score, bool won, double playTime) {
    std::string query = 
        "UPDATE user_stats SET "
        "total_score = total_score + $1, "
        "games_played = games_played + 1, "
        "games_won = games_won + $2, "
        "total_play_time = total_play_time + $3 "
        "WHERE user_id = $4;";
    
    const char* paramValues[4];
    std::string scoreStr = std::to_string(score);
    std::string wonStr = won ? "1" : "0";
    std::string timeStr = std::to_string(playTime);
    std::string userIdStr = std::to_string(userId);
    
    paramValues[0] = scoreStr.c_str();
    paramValues[1] = wonStr.c_str();
    paramValues[2] = timeStr.c_str();
    paramValues[3] = userIdStr.c_str();
    
    PGresult* result = PQexecParams(connection, query.c_str(), 4, 
                                   nullptr, paramValues, nullptr, nullptr, 0);
    
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        logError("Update user stats");
        PQclear(result);
        return false;
    }
    
    PQclear(result);
    return true;
}

void Database::displayLeaderboard() {
    auto records = getTopScores(10);
    
    std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                    🏆 LEADERBOARD (PostgreSQL) 🏆             ║\n";
    std::cout << "╠════╦═══════════════╦═══════╦═══════╦═══════╦═════════════════╣\n";
    std::cout << "║ №  ║ Player         ║ Score ║ Moves ║ Time  ║ Difficulty      ║\n";
    std::cout << "╠════╬═══════════════╬═══════╬═══════╬═══════╬═════════════════╣\n";
    
    for (size_t i = 0; i < records.size(); i++) {
        std::cout << "║ " << std::setw(2) << (i + 1) << " ║ "
                  << std::setw(13) << std::left << records[i].playerName.substr(0, 13) << " ║ "
                  << std::setw(5) << std::right << records[i].score << " ║ "
                  << std::setw(5) << records[i].moves << " ║ "
                  << std::setw(5) << (int)records[i].time << " ║ "
                  << std::setw(15) << std::left << records[i].difficulty << " ║\n";
    }
    
    std::cout << "╚════╩═══════════════╩═══════╩═══════╩═══════╩═════════════════╝\n";
}

std::string Database::getLastError() const {
    if (connection) {
        return PQerrorMessage(connection);
    }
    return "No database connection";
}
