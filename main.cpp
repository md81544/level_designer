#include "configreader.h"
#include "level.h"

#include <SFML/Graphics.hpp>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[])
{
    std::string loadFileName;
    try {

        if (argc != 2) {
            std::cout << "Usage: level_designer <filename>\n\n";
            std::cout << "If the file doesn't exist it will be created on save\n\n";
            return 1;
        }

        mgo::ConfigReader config("level_designer.cfg");

        unsigned screenWidth = static_cast<unsigned>(config.readLong("WindowWidth", 800));
        unsigned screenHeight = static_cast<unsigned>(config.readLong("WindowHeight", 800));
        const bool fullscreen = config.readBool("Fullscreen", false);
        const auto mode = sf::VideoMode::getDesktopMode();
        if (fullscreen) {
            // If fullscreen, we use the desktop resolution
            screenWidth = mode.size.x;
            screenHeight = mode.size.y;
        }
        sf::RenderWindow window(
            fullscreen ? mode : sf::VideoMode({ screenWidth, screenHeight }),
            argv[1],
            sf::Style::Titlebar | sf::Style::Close,
            fullscreen ? sf::State::Fullscreen : sf::State::Windowed);
        window.setFramerateLimit(24);

        mgo::Level level(window, screenWidth, screenHeight);

        level.load(argv[1]);

        while (window.isOpen()) {
            for (;;) {
                const auto event = window.pollEvent();
                if (!event.has_value()) {
                    break;
                }
                level.processEvent(window, *event);
            }

            level.clampViewport();
            // Draw the floating view items:
            // Note that .setView() changes whether we're writing to the
            // scrolling or fixed "canvas".
            window.setView(level.getView());
            window.clear();
            level.drawGridLines(window);
            level.draw(window);
            level.drawObjects(window);
            // Draw items to the FIXED view:
            // See note above re .setView()
            window.setView(level.getFixedView());
            level.drawModes(window);
            level.drawDialog(window);
            // Set view back otherwise mouse coords appear to be
            // reported in the wrong position
            window.setView(level.getView());
            window.display();
        }
        return 0;
    } catch (const std::exception& e) {
        // anything caught here is a terminal event
        std::cout << e.what() << std::endl;
    }
    return 1;
}
