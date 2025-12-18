#include "EmailSender.h"
#include <iostream>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <filesystem>

namespace fs = std::filesystem;

EmailSender::EmailSender() {
   
}

EmailSender::~EmailSender() {
   
}

bool EmailSender::sendFeedback(const std::string& userName, 
                              const std::string& userEmail, 
                              const std::string& message) {
    
    try {
        // Создаём папку если нет
        fs::create_directories("/app/feedback");
        
        // Генерируем имя файла с timestamp
        std::time_t now = std::time(nullptr);
        std::tm* localTime = std::localtime(&now);
        
        char filename[100];
        std::strftime(filename, sizeof(filename), 
                     "feedback_%Y%m%d_%H%M%S.txt", localTime);
        
        std::string filepath = std::string("/app/feedback/") + filename;
        
        // Сохраняем в файл
        std::ofstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "❌ Ошибка создания файла: " << filepath << std::endl;
            return false;
        }
        
        // Формируем содержимое
        file << "=== ОБРАЩЕНИЕ ИЗ MEMORY GAME ===\n\n";
        char timeStr[100];
        std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", localTime);
        file << "Время: " << timeStr << "\n";
        file << "Имя: " << userName << "\n";
        file << "Email: " << userEmail << "\n";
        file << "--- Сообщение ---\n" << message << "\n";
        file << "-----------------\n";
        
        file.close();
        
        // Также добавляем в общий лог
        std::ofstream logfile("/app/feedback/all_feedback.log", std::ios::app);
        if (logfile.is_open()) {
            logfile << filename << " | " << userName << " | " << userEmail << "\n";
            logfile.close();
        }
        
        std::cout << "\n✅ Обращение сохранено!\n";
        std::cout << "📁 Файл: " << filepath << "\n";
        std::cout << "👤 От: " << userName << "\n";
        std::cout << "📧 Email: " << userEmail << "\n";
        std::cout << "💬 Длина сообщения: " << message.length() << " символов\n";
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Ошибка при сохранении обращения: " << e.what() << std::endl;
        return false;
    }
}
