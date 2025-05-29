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

        // To get around SFML's inability to cope with high-DPI screens (e.g. Apple's "Retina"
        // displays) we just set the window size to whatever the user has in the config file - the
        // user (I) can adjust to whatever is best for the machine being used
        unsigned screenWidth = static_cast<unsigned>(config.readLong("WindowWidth", 800));
        unsigned screenHeight = static_cast<unsigned>(config.readLong("WindowHeight", 800));
        sf::RenderWindow window(
            sf::VideoMode({ screenWidth, screenHeight }),
            "Amaze Level Designer",
            sf::Style::Titlebar | sf::Style::Close);
        window.setFramerateLimit(24);

        mgo::Level level(screenWidth, screenHeight);

        level.load(argv[1]);

        while (window.isOpen()) {
            for (;;) {
                auto event = window.pollEvent();
                if (!event.has_value()) {
                    break;
                }
                    level.processEvent(window, *event);
                }

                level.processViewport();

                window.setView(level.getView());
                window.clear();
                level.drawGridLines(window);
                level.draw(window);
                level.drawObjects(window);
                window.setView(level.getFixedView());
                level.drawModes(window);
                level.drawDialog(window);
                // Set view back otherwise mouse coords appear to be
                // reported in the wrong position
                window.setView(level.getView());
                window.display();
            }
            return 0;
        }
        catch (const std::exception& e)
        {
            // anything caught here is a terminal event
            std::cout << e.what() << std::endl;
        }
        return 1;
    }
