#include <SFML/Graphics.hpp>
#include <boost/program_options.hpp>
#include <cmath>
#include <iostream>
#include <stdexcept>

#include "level.h"

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
            level.load(vm["load"].as<std::string>());
        }

        unsigned int screenWidth = sf::VideoMode::getDesktopMode().width;
        unsigned int screenHeight = sf::VideoMode::getDesktopMode().height;
        sf::RenderWindow window(
            sf::VideoMode(screenWidth - 200, screenHeight - 200), "Amaze Level Designer");
        window.setFramerateLimit(30);

        float zoomLevel = 0.4;
        while (window.isOpen()) {
            sf::Event event;
            while (window.pollEvent(event)) {
                if (event.type == sf::Event::Closed) {
                    window.close();
                }
                if (event.type == sf::Event::MouseMoved) {
                    std::cout << event.mouseMove.x << ", " << event.mouseMove.y << std::endl;
                }
                if (event.type == sf::Event::MouseButtonPressed) {
                    if (event.mouseButton.button == sf::Mouse::Left) {
                        std::cout << "The left button was pressed" << std::endl;
                        std::cout << "  mouse x: " << event.mouseButton.x << std::endl;
                        std::cout << "  mouse y: " << event.mouseButton.y << std::endl;
                    }
                }
                if (event.type == sf::Event::MouseButtonReleased) {
                    if (event.mouseButton.button == sf::Mouse::Left) {
                        std::cout << "The left button was released" << std::endl;
                        std::cout << "  mouse x: " << event.mouseButton.x << std::endl;
                        std::cout << "  mouse y: " << event.mouseButton.y << std::endl;
                    }
                }
                if (event.mouseWheelScroll.wheel == sf::Mouse::VerticalWheel) {
                    float amt = event.mouseWheelScroll.delta;
                    if (std::abs(amt) > 0.1 && std::abs(amt) < 10.f) {
                        std::cout << "Wheel type: vertical" << std::endl;
                        std::cout << "  wheel movement: " << event.mouseWheelScroll.delta
                                  << std::endl;
                        std::cout << "  mouse x: " << event.mouseWheelScroll.x << std::endl;
                        std::cout << "  mouse y: " << event.mouseWheelScroll.y << std::endl;
                    }
                }
            }

            window.clear();
            level.draw(window, zoomLevel);
            window.display();
        }
        return 0;
    } catch (const std::exception& e) {
        // anything caught here is a terminal event
        std::cout << e.what() << std::endl;
    }
    return 1;
}
