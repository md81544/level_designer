#pragma once

#include <SFML/Graphics.hpp>
#include <string>

std::string getInputFromDialog(
    sf::RenderWindow& window,
    sf::Font& font,
    const std::string& prompt,
    const std::string& defaultEntry = "");