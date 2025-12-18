#include "Card.h"
#include <iostream>
#include <algorithm>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

// Конструктор по умолчанию
Card::Card() 
    : id(-1), symbol(""), isFlipped(false), isMatched(false), 
      theme(CardTheme::ANIMALS), isImage(false) {
}

Card::Card(int id, const std::string& symbol, CardTheme theme)
    : id(id), symbol(symbol), isFlipped(false), isMatched(false), theme(theme) {

    std::string lowerSymbol = symbol;
    std::transform(lowerSymbol.begin(), lowerSymbol.end(), lowerSymbol.begin(), ::tolower);
    
    isImage = (lowerSymbol.find(".png") != std::string::npos || 
               lowerSymbol.find(".jpg") != std::string::npos ||
               lowerSymbol.find(".jpeg") != std::string::npos ||
               lowerSymbol.find(".bmp") != std::string::npos ||
               lowerSymbol.find("img") != std::string::npos);
}

std::vector<std::string> Card::getThemeSymbols(CardTheme theme) {
    switch (theme) {
        case CardTheme::ANIMALS:
            return {
                "🐶", "🐱", "🐭", "🐹", "🐰", "🦊",
                "🐻", "🐼", "🐨", "🐯", "🦁", "🐮",
                "🐷", "🐸", "🐵", "🐔", "🐧", "🐦",
                "🐤", "🐺", "🐗", "🐴", "🦄", "🐝",
                "🐛", "🦋", "🐌", "🐞", "🐜", "🦟"
            };
        
        case CardTheme::FRUITS:
            return {
                "🍎", "🍌", "🍒", "🍇", "🍉", "🍊",
                "🍋", "🍍", "🥭", "🍑", "🍈", "🍐",
                "🥝", "🍅", "🥥", "🥑", "🍆", "🥔",
                "🥕", "🌽", "🌶", "🥦", "🥒", "🥬",
                "🌰", "🥜", "🍞", "🥐", "🥖", "🥨"
            };
        
        case CardTheme::EMOJI:
            return {
                "😀", "😂", "🥰", "😎", "🤩", "😍",
                "😜", "🤪", "😇", "🥳", "😏", "😌",
                "😴", "🤤", "😷", "🤕", "🥺", "😡",
                "🤬", "😱", "🤯", "😈", "👻", "💀",
                "👽", "🤖", "🎃", "👾", "👿", "💩"
            };
        
        case CardTheme::MEMES:
            return {
                "M1", "M2", "M3", "M4", "M5", "M6",
                "M7", "M8", "M9", "M10", "M11", "M12",
                "M13", "M14", "M15", "M16", "M17", "M18",
                "M19", "M20", "M21", "M22", "M23", "M24",
                "M25", "M26", "M27", "M28", "M29", "M30"
            };
        
        case CardTheme::SYMBOLS:
            return {
                "★", "❤", "♦", "♣", "♠", "♪",
                "☀", "☁", "☂", "☃", "☄", "♫",
                "✈", "⌚", "⏰", "⭐", "🌈", "🎯",
                "⚽", "🎾", "🏀", "🏈", "⚾", "🎱",
                "🏆", "🎮", "🎲", "🎸", "🎷", "🎺"
            };
        
        default:
            // Запасной вариант - буквы и цифры
            std::vector<std::string> symbols;
            for (char c = 'A'; c <= 'Z'; c++) {
                symbols.push_back(std::string(1, c));
            }
            for (int i = 1; i <= 10; i++) {
                symbols.push_back(std::to_string(i));
            }
            return symbols;
    }
}

std::string Card::getDisplay() const {
    if (isMatched) {
        return "✓";
    } else if (isFlipped) {
        if (isImage) {
            return "[IMG]";
        } else {
            return symbol;
        }
    } else {
        return "?";
    }
}

void Card::flip() {
    if (!isMatched) {
        isFlipped = !isFlipped;
    }
}

void Card::setMatched(bool matched) {
    isMatched = matched;
    if (matched) {
        isFlipped = true;
    }
}

void Card::reset() {
    isFlipped = false;
    isMatched = false;
}
