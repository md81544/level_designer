#include "dialog.h"

// Note this has its own event loop so acts as a modal dialog

std::string getInputFromDialog(
    sf::RenderWindow& window,
    sf::View& view,
    sf::Font& font,
    const std::string& prompt,
    const std::string& defaultEntry, /* ="" */
    InputType inputType /* = InputType::string */)
{
    // Save the existing main window's contents so we're not displaying on a black screen
    sf::Texture windowContent(sf::Vector2u(window.getSize().x, window.getSize().y));
    windowContent.update(window);
    auto oldView = window.getView();
    window.setView(view);
    // Text and input box elements
    sf::Text promptText(font, prompt, 20);
    promptText.setFillColor(sf::Color::Green);
    promptText.setPosition({ 20, 30 });

    sf::RectangleShape inputBox(sf::Vector2f(480, 40));
    inputBox.setFillColor(sf::Color::White);
    inputBox.setOutlineColor(sf::Color::Red);
    inputBox.setOutlineThickness(2);
    inputBox.setPosition({ 20, 80 });

    sf::Text inputText(font, "", 20);
    inputText.setFillColor(sf::Color::Black);
    inputText.setPosition({ 25, 85 });
    inputText.setString(defaultEntry + "_");

    std::string input = defaultEntry;
    bool enterPressed = false;
    bool firstKeyPress = true;

    // Dialog loop
    std::optional event = window.pollEvent(); // Clear the keypress that got us here
    while (window.isOpen() && !enterPressed) {
        for (;;) {
            event = window.pollEvent();
            if (!event.has_value()) {
                break;
            }
            if (event->is<sf::Event::Closed>()) {
                window.close();
            } else if (event->is<sf::Event::TextEntered>()) {
                auto unicode = event->getIf<sf::Event::TextEntered>()->unicode;
                if (unicode == '\b') {
                    if (firstKeyPress) {
                        input.clear();
                    } else {
                        if (!input.empty()) {
                            input.pop_back();
                        }
                    }
                } else if (unicode == '\r' || unicode == '\n') {
                    enterPressed = true;
                } else if (unicode < 128) { // ASCII characters
                    auto c = static_cast<char>(unicode);
                    if (inputType == InputType::numeric) {
                        if (std::isdigit(c) || c == '.' || (input.length() == 0 && c == '-')) {
                            firstKeyPress = false;
                            input += c;
                        }
                    } else {
                        firstKeyPress = false;
                        input += c;
                    }
                }
                inputText.setString(input + "_");
            } else if (event->is<sf::Event::KeyPressed>()) {
                if (event->getIf<sf::Event::KeyPressed>()->scancode
                    == sf::Keyboard::Scancode::Escape) { // Escape to cancel
                    return "";
                }
            }
        }

        // Clear and redraw the window
        window.clear(sf::Color::Black);
        sf::Sprite sprite(windowContent);
        sprite.setColor({ 160, 160, 160 }); // dim it a bit
        window.draw(sprite); // draw the static main window content
        window.draw(promptText);
        window.draw(inputBox);
        window.draw(inputText);
        window.display();
    }
    window.setView(oldView);
    return input;
}
