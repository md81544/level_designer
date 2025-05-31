#pragma once

#include <SFML/Graphics.hpp>
#include <string>

enum class InputType {
    string,
    numeric
};

std::string getInputFromDialog(
    sf::RenderWindow& window,
    sf::View& view,
    sf::Font& font,
    const std::string& prompt,
    const std::string& defaultEntry = "",
    InputType inputType = InputType::string
);