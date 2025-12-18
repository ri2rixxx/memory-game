#include "Audio/SoundManager.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <cmath>

namespace fs = std::filesystem;

SoundManager::SoundManager() : volume(50.0f), soundEnabled(true) {
    std::cout << "\n🎵 === ЗАГРУЗКА ЗВУКОВ ИЗ ФАЙЛОВ ===\n" << std::endl;
    
    // Список звуков для загрузки
    std::vector<std::pair<std::string, std::string>> soundFiles = {
        {"flip", "assets/sounds/flip.wav"},
        {"match", "assets/sounds/match.wav"}, 
        {"mismatch", "assets/sounds/mismatch.wav"},
        {"click", "assets/sounds/click.wav"},
        {"win", "assets/sounds/win.wav"},
        {"lose", "assets/sounds/lose.wav"}
    };
    
    int loadedFromFiles = 0;
    
    for (const auto& [soundName, filePath] : soundFiles) {
        std::cout << "  🔍 " << soundName << " -> " << filePath;
        
        // 1. Проверяем существует ли файл
        std::ifstream testFile(filePath, std::ios::binary);
        if (!testFile.is_open()) {
            std::cout << " ❌ ФАЙЛ НЕ НАЙДЕН" << std::endl;
            createFallbackSound(soundName);
            continue;
        }
        testFile.close();
        
        // 2. Пробуем загрузить через SFML
        sf::SoundBuffer buffer;
        if (buffer.loadFromFile(filePath)) {
            // Успешно загрузили из файла
            soundBuffers[soundName] = buffer;
            
            sf::Sound sound;
            sound.setBuffer(soundBuffers[soundName]);
            sound.setVolume(volume);
            sounds[soundName] = sound;
            
            loadedFromFiles++;
            
            // Выводим информацию о загруженном звуке
            sf::Time duration = buffer.getDuration();
            std::cout << " ✅ ЗАГРУЖЕН (" 
                      << duration.asSeconds() << " сек, "
                      << buffer.getSampleRate() << " Hz)" << std::endl;
        } else {
            std::cout << " ❌ ОШИБКА ЗАГРУЗКИ" << std::endl;
            
            // Пробуем проанализировать файл
            std::ifstream file(filePath, std::ios::binary | std::ios::ate);
            std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);
            
            if (size >= 12) {
                char header[12];
                file.read(header, 12);
                file.close();
                
                std::cout << "    📏 Размер: " << size << " байт" << std::endl;
                std::cout << "    🔧 Заголовок: ";
                for (int i = 0; i < 12; i++) {
                    printf("%02X ", (unsigned char)header[i]);
                }
                std::cout << std::endl;
                
                // Проверяем WAV заголовок
                if (header[0] == 'R' && header[1] == 'I' && 
                    header[2] == 'F' && header[3] == 'F') {
                    std::cout << "    ✅ RIFF заголовок OK" << std::endl;
                } else {
                    std::cout << "    ❌ Неверный RIFF заголовок" << std::endl;
                }
            }
            
            createFallbackSound(soundName);
        }
    }
    
    std::cout << "\n📊 РЕЗУЛЬТАТ: " << loadedFromFiles << " из " 
              << soundFiles.size() << " звуков загружены из файлов\n" << std::endl;
    
    if (loadedFromFiles < soundFiles.size()) {
        std::cout << "⚠ Некоторые звуки не загрузились, используем заглушки\n" << std::endl;
    }
}

SoundManager::~SoundManager() {
    // Автоматическая очистка
}

void SoundManager::createFallbackSound(const std::string& name) {
    // Создаем простой программный звук ТОЛЬКО если не удалось загрузить из файла
    
    float frequency = 440.0f;
    float duration = 0.3f;
    
    if (name == "flip") frequency = 800.0f;
    else if (name == "match") frequency = 600.0f;
    else if (name == "mismatch") frequency = 300.0f;
    else if (name == "click") { frequency = 1000.0f; duration = 0.1f; }
    else if (name == "win") duration = 1.0f;
    else if (name == "lose") frequency = 392.0f;
    
    createProgrammaticSound(name, frequency, duration);
}

void SoundManager::createProgrammaticSound(const std::string& name, float frequency, float duration) {
    unsigned int sampleRate = 44100;
    unsigned int numSamples = static_cast<unsigned int>(sampleRate * duration);
    
    std::vector<sf::Int16> samples(numSamples);
    
    for (unsigned int i = 0; i < numSamples; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        float value = 0.3f * sin(2.0f * 3.14159265f * frequency * t);
        samples[i] = static_cast<sf::Int16>(value * 32767.0f);
    }
    
    sf::SoundBuffer buffer;
    if (buffer.loadFromSamples(samples.data(), samples.size(), 1, sampleRate)) {
        soundBuffers[name] = buffer;
        
        sf::Sound sound;
        sound.setBuffer(soundBuffers[name]);
        sound.setVolume(volume);
        sounds[name] = sound;
        
        std::cout << "    🔊 Создана программная заглушка для " << name 
                  << " (" << frequency << " Hz)" << std::endl;
    }
}

bool SoundManager::loadSound(const std::string& name, const std::string& filepath) {
    sf::SoundBuffer buffer;
    
    if (buffer.loadFromFile(filepath)) {
        soundBuffers[name] = buffer;
        
        sf::Sound sound;
        sound.setBuffer(soundBuffers[name]);
        sound.setVolume(volume);
        sounds[name] = sound;
        
        return true;
    }
    
    return false;
}

void SoundManager::playSound(const std::string& name) {
    if (!soundEnabled) return;
    
    auto it = sounds.find(name);
    if (it != sounds.end()) {
        // Если звук уже играет, перезапускаем
        if (it->second.getStatus() == sf::Sound::Playing) {
            it->second.stop();
        }
        it->second.play();
        std::cout << "[PLAY] " << name << std::endl;
    }
}

void SoundManager::stopSound(const std::string& name) {
    auto it = sounds.find(name);
    if (it != sounds.end()) {
        it->second.stop();
    }
}

void SoundManager::setVolume(float newVolume) {
    volume = std::max(0.0f, std::min(100.0f, newVolume));
    
    for (auto& pair : sounds) {
        pair.second.setVolume(volume);
    }
}

float SoundManager::getVolume() const {
    return volume;
}

void SoundManager::enableSound(bool enable) {
    soundEnabled = enable;
    std::cout << "[SOUND] " << (enable ? "ВКЛЮЧЕН" : "ВЫКЛЮЧЕН") << std::endl;
}

bool SoundManager::isSoundEnabled() const {
    return soundEnabled;
}

void SoundManager::playCardFlip() {
    playSound("flip");
}

void SoundManager::playCardMatch() {
    playSound("match");
}

void SoundManager::playCardMismatch() {
    playSound("mismatch");
}

void SoundManager::playButtonClick() {
    playSound("click");
}

void SoundManager::playGameWin() {
    playSound("win");
}

void SoundManager::playGameLose() {
    playSound("lose");
}

bool SoundManager::isSoundLoaded(const std::string& name) const {
    auto it = sounds.find(name);
    return it != sounds.end() && it->second.getBuffer() != nullptr;
}
