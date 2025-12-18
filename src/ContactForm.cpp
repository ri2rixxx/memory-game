#include "ContactForm.h"
#include <iostream>

ContactForm::ContactForm() : activeField(ActiveField::NONE) {
    nameInput = "";
    emailInput = "";
    messageInput = "";
}

bool ContactForm::loadFont(const std::string& fontPath) {
    return font.loadFromFile(fontPath);
}

void ContactForm::setup(float windowWidth, float windowHeight) {
    // Настройка шрифтов
    titleText.setFont(font);
    titleText.setString("Contact Developer");
    titleText.setCharacterSize(48);
    titleText.setFillColor(sf::Color::White);
    titleText.setStyle(sf::Text::Bold);
    titleText.setPosition(windowWidth / 2 - 150, 50);
    
    // Надписи полей
    nameLabel.setFont(font);
    nameLabel.setString("Your Name:");
    nameLabel.setCharacterSize(28);
    nameLabel.setFillColor(sf::Color::White);
    nameLabel.setPosition(100, 150);
    
    emailLabel.setFont(font);
    emailLabel.setString("Your Email:");
    emailLabel.setCharacterSize(28);
    emailLabel.setFillColor(sf::Color::White);
    emailLabel.setPosition(100, 250);
    
    messageLabel.setFont(font);
    messageLabel.setString("Your Message:");
    messageLabel.setCharacterSize(28);
    messageLabel.setFillColor(sf::Color::White);
    messageLabel.setPosition(100, 350);
    
    // Поля ввода
    nameBox.setSize(sf::Vector2f(500, 40));
    nameBox.setFillColor(sf::Color(50, 50, 50));
    nameBox.setOutlineThickness(2);
    nameBox.setOutlineColor(sf::Color::White);
    nameBox.setPosition(300, 150);
    
    emailBox.setSize(sf::Vector2f(500, 40));
    emailBox.setFillColor(sf::Color(50, 50, 50));
    emailBox.setOutlineThickness(2);
    emailBox.setOutlineColor(sf::Color::White);
    emailBox.setPosition(300, 250);
    
    messageBox.setSize(sf::Vector2f(500, 200));
    messageBox.setFillColor(sf::Color(50, 50, 50));
    messageBox.setOutlineThickness(2);
    messageBox.setOutlineColor(sf::Color::White);
    messageBox.setPosition(300, 350);
    
    // Кнопка отправки
    sendButton.setSize(sf::Vector2f(200, 50));
    sendButton.setFillColor(sf::Color(0, 150, 0));
    sendButton.setOutlineThickness(2);
    sendButton.setOutlineColor(sf::Color::White);
    sendButton.setPosition(windowWidth / 2 - 220, 600);
    
    sendButtonText.setFont(font);
    sendButtonText.setString("Save Feedback");
    sendButtonText.setCharacterSize(24);
    sendButtonText.setFillColor(sf::Color::White);
    sendButtonText.setPosition(windowWidth / 2 - 180, 610);
    
    // Кнопка назад
    backButton.setSize(sf::Vector2f(200, 50));
    backButton.setFillColor(sf::Color(150, 0, 0));
    backButton.setOutlineThickness(2);
    backButton.setOutlineColor(sf::Color::White);
    backButton.setPosition(windowWidth / 2 + 20, 600);
    
    backButtonText.setFont(font);
    backButtonText.setString("Back to Menu");
    backButtonText.setCharacterSize(24);
    backButtonText.setFillColor(sf::Color::White);
    backButtonText.setPosition(windowWidth / 2 + 60, 610);
    
    // Статус
    statusText.setFont(font);
    statusText.setString("");
    statusText.setCharacterSize(20);
    statusText.setFillColor(sf::Color::Yellow);
    statusText.setPosition(100, 680);
}

void ContactForm::handleEvent(const sf::Event& event, const sf::Vector2f& mousePos) {
    if (event.type == sf::Event::MouseButtonPressed) {
        if (event.mouseButton.button == sf::Mouse::Left) {
            // Проверяем клик по полям
            if (nameBox.getGlobalBounds().contains(mousePos)) {
                activeField = ActiveField::NAME;
                nameBox.setOutlineColor(sf::Color::Yellow);
                emailBox.setOutlineColor(sf::Color::White);
                messageBox.setOutlineColor(sf::Color::White);
            } else if (emailBox.getGlobalBounds().contains(mousePos)) {
                activeField = ActiveField::EMAIL;
                nameBox.setOutlineColor(sf::Color::White);
                emailBox.setOutlineColor(sf::Color::Yellow);
                messageBox.setOutlineColor(sf::Color::White);
            } else if (messageBox.getGlobalBounds().contains(mousePos)) {
                activeField = ActiveField::MESSAGE;
                nameBox.setOutlineColor(sf::Color::White);
                emailBox.setOutlineColor(sf::Color::White);
                messageBox.setOutlineColor(sf::Color::Yellow);
            } else if (sendButton.getGlobalBounds().contains(mousePos)) {
                sendFeedback();
            } else {
                activeField = ActiveField::NONE;
                nameBox.setOutlineColor(sf::Color::White);
                emailBox.setOutlineColor(sf::Color::White);
                messageBox.setOutlineColor(sf::Color::White);
            }
        }
    }
    
    // Обработка ввода текста
    if (event.type == sf::Event::TextEntered && activeField != ActiveField::NONE) {
        handleTextInput(event.text.unicode);
    }
}

void ContactForm::handleTextInput(sf::Uint32 unicode) {
    if (unicode == '\b') { // Backspace
        switch (activeField) {
            case ActiveField::NAME:
                if (!nameInput.empty()) nameInput.pop_back();
                break;
            case ActiveField::EMAIL:
                if (!emailInput.empty()) emailInput.pop_back();
                break;
            case ActiveField::MESSAGE:
                if (!messageInput.empty()) messageInput.pop_back();
                break;
            default:
                break;
        }
    } else if (unicode == '\r' || unicode == '\n') { // Enter
        // Переключение между полями или отправка
        if (activeField == ActiveField::NAME) {
            activeField = ActiveField::EMAIL;
        } else if (activeField == ActiveField::EMAIL) {
            activeField = ActiveField::MESSAGE;
        }
    } else if (unicode >= 32 && unicode < 128) { // Печатные символы
        switch (activeField) {
            case ActiveField::NAME:
                if (nameInput.length() < 50) nameInput += static_cast<char>(unicode);
                break;
            case ActiveField::EMAIL:
                if (emailInput.length() < 100) emailInput += static_cast<char>(unicode);
                break;
            case ActiveField::MESSAGE:
                if (messageInput.length() < 500) messageInput += static_cast<char>(unicode);
                break;
            default:
                break;
        }
    }
}

void ContactForm::update(const sf::Vector2f& mousePos) {
    // Обновление цвета кнопок при наведении
    if (sendButton.getGlobalBounds().contains(mousePos)) {
        sendButton.setFillColor(sf::Color(0, 200, 0));
    } else {
        sendButton.setFillColor(sf::Color(0, 150, 0));
    }
    
    if (backButton.getGlobalBounds().contains(mousePos)) {
        backButton.setFillColor(sf::Color(200, 0, 0));
    } else {
        backButton.setFillColor(sf::Color(150, 0, 0));
    }
}

void ContactForm::render(sf::RenderWindow& window) {
    window.draw(titleText);
    window.draw(nameLabel);
    window.draw(emailLabel);
    window.draw(messageLabel);
    
    window.draw(nameBox);
    window.draw(emailBox);
    window.draw(messageBox);
    
    window.draw(sendButton);
    window.draw(sendButtonText);
    window.draw(backButton);
    window.draw(backButtonText);
    
    // Отрисовка введенного текста
    sf::Text nameDisplay(nameInput + (activeField == ActiveField::NAME ? "_" : ""), font, 24);
    nameDisplay.setFillColor(sf::Color::White);
    nameDisplay.setPosition(305, 155);
    window.draw(nameDisplay);
    
    sf::Text emailDisplay(emailInput + (activeField == ActiveField::EMAIL ? "_" : ""), font, 24);
    emailDisplay.setFillColor(sf::Color::White);
    emailDisplay.setPosition(305, 255);
    window.draw(emailDisplay);
    
    sf::Text messageDisplay(messageInput + (activeField == ActiveField::MESSAGE ? "_" : ""), font, 24);
    messageDisplay.setFillColor(sf::Color::White);
    messageDisplay.setPosition(305, 355);
    window.draw(messageDisplay);
    
    window.draw(statusText);
}

void ContactForm::sendFeedback() {
    // Проверка заполнения полей
    if (nameInput.empty() || emailInput.empty() || messageInput.empty()) {
        statusText.setString("Please fill all fields!");
        statusText.setFillColor(sf::Color::Red);
        return;
    }
    
    // Проверка email (простая)
    if (emailInput.find('@') == std::string::npos || 
        emailInput.find('.') == std::string::npos) {
        statusText.setString("Please enter valid email!");
        statusText.setFillColor(sf::Color::Red);
        return;
    }
    
    statusText.setString("Saving...");
    statusText.setFillColor(sf::Color::Yellow);
    
    // Сохраняем через EmailSender
    if (emailSender.sendFeedback(nameInput, emailInput, messageInput)) {
        statusText.setString("Feedback saved successfully!");
        statusText.setFillColor(sf::Color::Green);
        
        // Очистка полей после успешного сохранения
        reset();
    } else {
        statusText.setString("Failed to save. Please try again.");
        statusText.setFillColor(sf::Color::Red);
    }
}

void ContactForm::reset() {
    nameInput = "";
    emailInput = "";
    messageInput = "";
    activeField = ActiveField::NONE;
    nameBox.setOutlineColor(sf::Color::White);
    emailBox.setOutlineColor(sf::Color::White);
    messageBox.setOutlineColor(sf::Color::White);
}

bool ContactForm::isMouseOverButton(const sf::Vector2f& mousePos) const {
    return sendButton.getGlobalBounds().contains(mousePos);
}

bool ContactForm::isMouseOverBackButton(const sf::Vector2f& mousePos) const {
    return backButton.getGlobalBounds().contains(mousePos);
}
