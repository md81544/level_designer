#include <SFML/Graphics.hpp>
#include <cmath>
#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[])
{
    try {
        unsigned int screenWidth = sf::VideoMode::getDesktopMode().width;
        unsigned int screenHeight = sf::VideoMode::getDesktopMode().height;
        sf::RenderWindow window(
            sf::VideoMode(screenWidth - 200, screenHeight - 200), "Amaze Level Designer");
        window.setFramerateLimit(30);

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
            window.display();
        }
        return 0;
    } catch (const std::exception& e) {
        // anything caught here is a terminal event
        std::cout << e.what() << std::endl;
    }
    return 1;
}
