#include "Audio/MusicPlayer.h"
#include <iostream>

MusicPlayer::MusicPlayer() : volume(50.0f), isPlaying(false) {
    // Инициализация путей к музыке
    musicFiles[MusicTheme::MENU] = "assets/music/menu.ogg";
    musicFiles[MusicTheme::GAMEPLAY_EASY] = "assets/music/gameplay_easy.ogg";
    musicFiles[MusicTheme::GAMEPLAY_MEDIUM] = "assets/music/gameplay_medium.ogg";
    musicFiles[MusicTheme::GAMEPLAY_HARD] = "assets/music/gameplay_hard.ogg";
    musicFiles[MusicTheme::GAME_OVER] = "assets/music/game_over.ogg";
}

MusicPlayer::~MusicPlayer() {
    stop();
}

bool MusicPlayer::loadMusic(MusicTheme theme, const std::string& filepath) {
    musicFiles[theme] = filepath;
    return true; // Фактическая загрузка произойдет при play()
}

void MusicPlayer::play(MusicTheme theme) {
    auto it = musicFiles.find(theme);
    if (it != musicFiles.end()) {
        // Останавливаем текущую музыку
        if (currentMusic.getStatus() == sf::Music::Playing) {
            currentMusic.stop();
        }
        
        // Загружаем и играем новую
        if (currentMusic.openFromFile(it->second)) {
            currentMusic.setVolume(volume);
            currentMusic.setLoop(true);
            currentMusic.play();
            isPlaying = true;
        } else {
            std::cerr << "Не удалось загрузить музыку: " << it->second << std::endl;
            isPlaying = false;
        }
    }
}

void MusicPlayer::pause() {
    if (currentMusic.getStatus() == sf::Music::Playing) {
        currentMusic.pause();
        isPlaying = false;
    }
}

void MusicPlayer::resume() {
    if (currentMusic.getStatus() == sf::Music::Paused) {
        currentMusic.play();
        isPlaying = true;
    }
}

void MusicPlayer::stop() {
    currentMusic.stop();
    isPlaying = false;
}

void MusicPlayer::setVolume(float newVolume) {
    volume = std::max(0.0f, std::min(100.0f, newVolume));
    currentMusic.setVolume(volume);
}

float MusicPlayer::getVolume() const {
    return volume;
}

bool MusicPlayer::getIsPlaying() const {
    return isPlaying;
}

void MusicPlayer::setLoop(bool loop) {
    currentMusic.setLoop(loop);
}
