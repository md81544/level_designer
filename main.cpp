#include "level.h"
#include "configreader.h"
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

        mgo::ConfigReader config("level_designer.cfg");

        // To get around SFML's inability to cope with high-DPI screens (e.g. Apple's "Retina"
        // displays) we just set the window size to whatever the user has in the config file
        unsigned int screenWidth = static_cast<unsigned int>(config.readLong("WindowWidth", 800));
        unsigned int screenHeight = static_cast<unsigned int>(config.readLong("WindowHeight", 800));
        sf::RenderWindow window(sf::VideoMode(screenWidth, screenHeight),
            "Amaze Level Designer",
            sf::Style::Titlebar | sf::Style::Close);
        window.setFramerateLimit(30);

        mgo::Level level(screenWidth, screenHeight);
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
