#include "level.h"
#include <SFML/Graphics.hpp>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>

void print_help()
{
    std::cout << "level_designer [-h|--help] <optional file to load>\n";
}

int main(int argc, char* argv[])
{
    try {

        if (argc > 1) {
            if (argv[1][0] == '-') {
                print_help();
                return 1;
            }
        }
        if (argc > 2) {
            print_help();
            return 2;
        }

        unsigned int screenWidth = sf::VideoMode::getDesktopMode().width;
        unsigned int screenHeight = sf::VideoMode::getDesktopMode().height;
        // Hack to determine if we're on a Mac "retina" display:
        if (sf::VideoMode::getFullscreenModes().empty()) {
            screenWidth *= 0.5;
            screenHeight *= 0.5;
        }
        sf::RenderWindow window(sf::VideoMode(screenWidth - 200, screenHeight - 200),
            "Amaze Level Designer",
            sf::Style::Titlebar | sf::Style::Close);
        window.setFramerateLimit(30);

        mgo::Level level(screenWidth - 200, screenHeight - 200);
        if (argc == 2) {
            std::string loadFileName = argv[1];
            level.load(loadFileName);
        }

        while (window.isOpen()) {
            sf::Event event;
            while (window.pollEvent(event)) {
                level.processEvent(window, event);
            }

            window.clear();
            level.drawGridLines(window);
            level.draw(window);
            level.drawObjects(window);
            level.displayMode(window);
            level.drawDialog(window);
            window.display();
        }
        return 0;
    } catch (const std::exception& e) {
        // anything caught here is a terminal event
        std::cout << e.what() << std::endl;
    }
    return 1;
}
