#pragma once

#include <SFML/Graphics.hpp>
#include <string>

std::string getInputFromDialog(sf::RenderWindow& window, const std::string& prompt, sf::Font& font);