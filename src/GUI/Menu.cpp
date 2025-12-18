#include "GUI/Menu.h"
#include <iostream>

Menu::Menu() : isVisible(false) {
    background.setFillColor(sf::Color(30, 30, 30, 230)); // Полупрозрачный темный фон
    background.setOutlineThickness(2);
    background.setOutlineColor(sf::Color::White);
}

void Menu::setPosition(float x, float y) {
    background.setPosition(x, y);
    
    // Обновляем позиции кнопок
    float buttonY = y + 80; // Отступ от заголовка
    for (auto& button : buttons) {
        // Вместо getPosition() используем setPosition
        button.setPosition(x + 50, buttonY);
        buttonY += 70; // Интервал между кнопками
    }
    
    // Обновляем позицию заголовка
    if (title.getString().getSize() > 0) {
        sf::FloatRect titleBounds = title.getLocalBounds();
        title.setPosition(x + (background.getSize().x - titleBounds.width) / 2, y + 20);
    }
}

void Menu::setSize(float width, float height) {
    background.setSize(sf::Vector2f(width, height));
}

void Menu::setTitle(const std::string& titleText, const sf::Font& font) {
    title.setFont(font);
    title.setString(titleText);
    title.setCharacterSize(36);
    title.setFillColor(sf::Color::White);
    title.setStyle(sf::Text::Bold);
}

void Menu::addButton(const Button& button) {
    buttons.push_back(button);
    
    // Автоматически позиционируем кнопки
    if (buttons.size() == 1) {
        // Первая кнопка - позиционируем в меню
        float x = background.getPosition().x + 50;
        float y = background.getPosition().y + 80;
        buttons.back().setPosition(x, y);
    } else {
        // Последующие кнопки - под предыдущей
        float x = background.getPosition().x + 50;
        float y = background.getPosition().y + 80 + (buttons.size() - 1) * 70; // Упрощенный расчет
        buttons.back().setPosition(x, y);
    }
}

void Menu::clearButtons() {
    buttons.clear();
}

void Menu::show() {
    isVisible = true;
}

void Menu::hide() {
    isVisible = false;
}

bool Menu::getIsVisible() const {
    return isVisible;
}

void Menu::update(const sf::Vector2f& mousePos) {
    if (!isVisible) return;
    
    for (auto& button : buttons) {
        button.update(mousePos);
    }
}

void Menu::handleEvent(const sf::Event& event, const sf::Vector2f& mousePos) {
    if (!isVisible) return;
    
    for (auto& button : buttons) {
        button.handleEvent(event, mousePos);
    }
}

void Menu::render(sf::RenderWindow& window) {
    if (!isVisible) return;
    
    window.draw(background);
    window.draw(title);
    
    for (auto& button : buttons) {
        button.render(window);
    }
}

void Menu::setBackgroundColor(const sf::Color& color) {
    background.setFillColor(color);
}

void Menu::setTitleColor(const sf::Color& color) {
    title.setFillColor(color);
}
