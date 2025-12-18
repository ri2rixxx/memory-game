#include "GUI/Button.h"
#include <iostream>

Button::Button(float x, float y, float width, float height, 
               const std::string& textStr, 
               const sf::Font& font,
               std::function<void()> onClickFunc)
    : fontPtr(&font),
      idleColor(sf::Color(70, 130, 180)),
      hoverColor(sf::Color(100, 149, 237)),
      activeColor(sf::Color(30, 144, 255)),
      onClick(onClickFunc)
{
    shape.setPosition(x, y);
    shape.setSize(sf::Vector2f(width, height));
    shape.setFillColor(idleColor);
    shape.setOutlineThickness(2);
    shape.setOutlineColor(sf::Color::White);
    
    if (fontPtr) {
        text.setFont(*fontPtr);
    }
    
    text.setString(textStr);
    text.setCharacterSize(24);
    text.setFillColor(sf::Color::White);
    
    sf::FloatRect textBounds = text.getLocalBounds();
    text.setOrigin(textBounds.left + textBounds.width / 2.0f,
                   textBounds.top + textBounds.height / 2.0f);
    text.setPosition(x + width / 2.0f, y + height / 2.0f);
}

void Button::update(const sf::Vector2f& mousePos) {
    if (shape.getGlobalBounds().contains(mousePos)) {
        shape.setFillColor(hoverColor);
        shape.setOutlineColor(sf::Color::Yellow);
    } else {
        shape.setFillColor(idleColor);
        shape.setOutlineColor(sf::Color::White);
    }
}

void Button::render(sf::RenderWindow& window) {
    window.draw(shape);
    
    if (fontPtr) {
        sf::Text shadow = text;
        shadow.setFillColor(sf::Color(0, 0, 0, 150));
        shadow.move(2, 2);
        window.draw(shadow);
    }
    
    window.draw(text);
}

void Button::handleEvent(const sf::Event& event, const sf::Vector2f& mousePos) {
    if (event.type == sf::Event::MouseButtonPressed) {
        if (event.mouseButton.button == sf::Mouse::Left) {
            if (shape.getGlobalBounds().contains(mousePos)) {
                shape.setFillColor(activeColor);
                if (onClick) {
                    onClick();
                }
            }
        }
    }
}

void Button::setPosition(float x, float y) {
    shape.setPosition(x, y);
    
    sf::FloatRect textBounds = text.getLocalBounds();
    text.setOrigin(textBounds.left + textBounds.width / 2.0f,
                   textBounds.top + textBounds.height / 2.0f);
    text.setPosition(x + shape.getSize().x / 2.0f, 
                     y + shape.getSize().y / 2.0f);
}

sf::Vector2f Button::getPosition() const {
    return shape.getPosition();
}

void Button::setColors(const sf::Color& idle, const sf::Color& hover, const sf::Color& active) {
    idleColor = idle;
    hoverColor = hover;
    activeColor = active;
    shape.setFillColor(idleColor);
}

void Button::setText(const std::string& textStr) {
    text.setString(textStr);
    
    sf::FloatRect textBounds = text.getLocalBounds();
    text.setOrigin(textBounds.left + textBounds.width / 2.0f,
                   textBounds.top + textBounds.height / 2.0f);
    
    sf::Vector2f shapePos = shape.getPosition();
    text.setPosition(shapePos.x + shape.getSize().x / 2.0f,
                     shapePos.y + shape.getSize().y / 2.0f);
}

void Button::setFont(const sf::Font& font) {
    fontPtr = &font;
    text.setFont(font);
}
