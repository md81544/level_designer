#include "level.h"
#include <SFML/Graphics.hpp>
#include <boost/program_options.hpp>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[])
{
    try {

        mgo::Level level;

        namespace po = boost::program_options;
        po::options_description desc("Amaze Level Designer");
        // clang-format off
        desc.add_options()
            ("help,h", "show help message")
            ("load,l", po::value<std::string>(), "load file");
        // clang-format on
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);

        if (vm.count("help")) {
            std::cout << desc << "\n";
            return 1;
        }
        if (vm.count("load")) {
            std::string loadFileName = vm["load"].as<std::string>();
            level.load(loadFileName);
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

        while (window.isOpen()) {
            sf::Event event;
            while (window.pollEvent(event)) {
                level.processEvent(window, event);
            }

            window.clear();
            level.drawGridLines(window);
            level.draw(window);
            level.drawDialog(window);
            level.displayMode(window);
            window.display();
        }
        return 0;
    } catch (const std::exception& e) {
        // anything caught here is a terminal event
        std::cout << e.what() << std::endl;
    }
    return 1;
}
