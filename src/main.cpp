#include "Game.h"
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <clocale>

bool isRunningInDocker() {
    std::ifstream dockerEnv("/.dockerenv");
    if (dockerEnv.good()) {
        std::cout << "✅ Запущено в Docker" << std::endl;
        return true;
    }
    
    std::ifstream cgroup("/proc/self/cgroup");
    if (cgroup.is_open()) {
        std::string line;
        while (std::getline(cgroup, line)) {
            if (line.find("docker") != std::string::npos ||
                line.find("kubepods") != std::string::npos) {
                std::cout << "✅ Запущено в Docker (cgroup)" << std::endl;
                return true;
            }
        }
    }
    
    std::cout << "Запущено локально" << std::endl;
    return false;
}

int main() {
    try {

        std::setlocale(LC_ALL, "C.UTF-8");
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "        Memory Game v2.0" << std::endl;
        std::cout << "   Курсовая работка по программированию" << std::endl;
        std::cout << "========================================" << std::endl;
        

        bool inDocker = isRunningInDocker();
        std::cout << "Environment: " << (inDocker ? "Docker" : "Local system") << std::endl;
        

        std::srand(static_cast<unsigned int>(std::time(nullptr)));
        

        std::cout << "Запуск игры..." << std::endl;
        Game game;
        std::cout << "Игра инициализирована" << std::endl;
        
        game.run();
        
    } catch (const std::exception& e) {
        std::cerr << "\n========================================" << std::endl;
        std::cerr << "ERROR: " << e.what() << std::endl;
        std::cerr << "========================================" << std::endl;
        return EXIT_FAILURE;
    }
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "Спасибо за игру!" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return EXIT_SUCCESS;
}
