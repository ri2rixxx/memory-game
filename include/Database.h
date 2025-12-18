#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <vector>
#include <memory>
#include <libpq-fe.h>

struct GameRecord {
    int id;
    std::string playerName;
    int score;
    int moves;
    int pairs;
    double time;
    std::string date;
    std::string difficulty;
};

class Database {
private:
    PGconn* connection;
    std::string connectionString;
    
    bool executeQuery(const std::string& query);
    PGresult* executeQueryWithResult(const std::string& query);
    void logError(const std::string& operation);
    
public:
    Database(const std::string& connStr = "");
    ~Database();
    
    bool connect();
    bool disconnect();
    bool isConnected() const;
    
    // Основные операции
    bool initialize();
    bool saveGame(const GameRecord& record);
    std::vector<GameRecord> getTopScores(int limit = 10);
    std::vector<GameRecord> getPlayerHistory(const std::string& playerName, int limit = 10);
    
    bool createUser(const std::string& username, const std::string& password, 
                    const std::string& email, std::string& errorMsg);
    bool authenticateUser(const std::string& username, const std::string& password, 
                         std::string& errorMsg);
    bool updateUserStats(int userId, int score, bool won, double playTime);
    
    std::string getLastError() const;
    void displayLeaderboard();
    
    std::vector<GameRecord> getTopPlayers(int limit = 10) {
        return getTopScores(limit);
    }
};

#endif
