#include "GUI/CardSprite.h"
#include <iostream>
#include <fstream>
#include <algorithm>

CardSprite::CardSprite(int id, const std::string& symbol, float x, float y, float size)
    : id(id), symbol(symbol), state(CardState::HIDDEN), isClickable(true), hasImage(false) {
    
    shape.setPosition(x, y);
    shape.setSize(sf::Vector2f(size, size));
    shape.setFillColor(sf::Color(25, 25, 112));
    shape.setOutlineThickness(2);
    shape.setOutlineColor(sf::Color::White);
    
    symbolText.setCharacterSize(static_cast<unsigned int>(size * 0.4f));
    symbolText.setFillColor(sf::Color::White);
    symbolText.setStyle(sf::Text::Bold);
    
    centerText();
}

bool CardSprite::loadImage(const std::string& imagePath) {
    if (imageTexture.loadFromFile(imagePath)) {
        hasImage = true;
        
        imageSprite.setTexture(imageTexture);
        
        sf::FloatRect imageBounds = imageSprite.getLocalBounds();
        float scaleX = (shape.getSize().x * 0.8f) / imageBounds.width;
        float scaleY = (shape.getSize().y * 0.8f) / imageBounds.height;
        float scale = std::min(scaleX, scaleY);
        
        imageSprite.setScale(scale, scale);
        centerImage();
        
        return true;
    }
    
    return false;
}

void CardSprite::setSymbol(const std::string& symbol, const sf::Font& mainFont) {
    this->symbol = symbol;
    
    std::string lowerSymbol = symbol;
    std::transform(lowerSymbol.begin(), lowerSymbol.end(), lowerSymbol.begin(), ::tolower);
    
    if (lowerSymbol.find(".png") != std::string::npos || 
        lowerSymbol.find(".jpg") != std::string::npos ||
        lowerSymbol.find(".jpeg") != std::string::npos ||
        lowerSymbol.find(".bmp") != std::string::npos) {
        
        if (loadImage(symbol)) {
            hasImage = true;
            return;
        }
    }
    
    hasImage = false;
    symbolText.setFont(mainFont);
    symbolText.setString(symbol);
    symbolText.setCharacterSize(static_cast<unsigned int>(shape.getSize().x * 0.4f));
    symbolText.setFillColor(sf::Color::White);
    symbolText.setStyle(sf::Text::Bold);
    
    centerText();
}

void CardSprite::setClickable(bool clickable) {
    isClickable = clickable;
}

void CardSprite::centerText() {
    if (symbolText.getFont()) {
        sf::FloatRect textBounds = symbolText.getLocalBounds();
        symbolText.setOrigin(
            textBounds.left + textBounds.width / 2.0f,
            textBounds.top + textBounds.height / 2.0f
        );
        
        symbolText.setPosition(
            shape.getPosition().x + shape.getSize().x / 2.0f,
            shape.getPosition().y + shape.getSize().y / 2.0f
        );
    }
}

void CardSprite::centerImage() {
    if (hasImage) {
        sf::FloatRect spriteBounds = imageSprite.getLocalBounds();
        imageSprite.setOrigin(
            spriteBounds.left + spriteBounds.width / 2.0f,
            spriteBounds.top + spriteBounds.height / 2.0f
        );
        
        imageSprite.setPosition(
            shape.getPosition().x + shape.getSize().x / 2.0f,
            shape.getPosition().y + shape.getSize().y / 2.0f
        );
    }
}

void CardSprite::setPosition(float x, float y) {
    shape.setPosition(x, y);
    centerText();
    centerImage();
}

void CardSprite::setSize(float size) {
    shape.setSize(sf::Vector2f(size, size));
    symbolText.setCharacterSize(static_cast<unsigned int>(size * 0.4f));
    centerText();
    centerImage();
}

void CardSprite::setState(CardState newState) {
    state = newState;
    
    switch (state) {
        case CardState::HIDDEN:
            shape.setFillColor(sf::Color(25, 25, 112));
            shape.setOutlineColor(sf::Color::White);
            isClickable = true;
            break;
        case CardState::REVEALED:
            shape.setFillColor(sf::Color(100, 149, 237));
            shape.setOutlineColor(sf::Color::Yellow);
            isClickable = false;
            break;
        case CardState::MATCHED:
            shape.setFillColor(sf::Color(50, 205, 50));
            shape.setOutlineColor(sf::Color::Green);
            shape.setOutlineThickness(3);
            isClickable = false;
            break;
    }
}

void CardSprite::flip() {
    if (state == CardState::HIDDEN) {
        setState(CardState::REVEALED);
    } else if (state == CardState::REVEALED) {
        setState(CardState::HIDDEN);
    }
}

void CardSprite::reveal() {
    setState(CardState::REVEALED);
}

void CardSprite::hide() {
    setState(CardState::HIDDEN);
}

void CardSprite::markMatched() {
    setState(CardState::MATCHED);
}

bool CardSprite::contains(const sf::Vector2f& point) const {
    return shape.getGlobalBounds().contains(point) && isClickable;
}

void CardSprite::update(float deltaTime) {
    (void)deltaTime;
}

void CardSprite::render(sf::RenderWindow& window) {
    window.draw(shape);
    
    if (state != CardState::HIDDEN) {
        if (hasImage) {
            window.draw(imageSprite);
        } else if (symbolText.getFont()) {
            sf::FloatRect textBounds = symbolText.getLocalBounds();
            if (textBounds.width > 0 && textBounds.height > 0) {
                sf::RectangleShape textBackground(sf::Vector2f(
                    textBounds.width + 20,
                    textBounds.height + 20
                ));
                textBackground.setFillColor(sf::Color(255, 255, 255, 100));
                textBackground.setPosition(
                    symbolText.getPosition().x - textBackground.getSize().x / 2,
                    symbolText.getPosition().y - textBackground.getSize().y / 2
                );
                window.draw(textBackground);
                
                sf::Text shadow = symbolText;
                shadow.setFillColor(sf::Color(0, 0, 0, 100));
                shadow.move(2, 2);
                window.draw(shadow);
            }
            
            window.draw(symbolText);
        }
    }
}
