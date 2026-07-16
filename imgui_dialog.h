#pragma once

// Class to completely contain all imgui-sfml code.
// Imgui-sfml is used to provide menus and other UI widgets.

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Event.hpp>
#include <vector>

namespace mgo {
namespace imgui {

struct Status {
    bool quit { false };
    bool breakable { false };
    bool edit { false };
    bool line { false };
    bool fuel { false };
    bool start { false };
    bool exit { false };
    bool moving { false };
    bool polygon { false };
    bool snapMode { false };
    bool load { false };
    bool save { false };
};

class Dialog {
public:
    explicit Dialog(sf::RenderWindow& window);
    ~Dialog() noexcept;
    // Note: this should be the only place that polls for sf::Events.
    // any events not handled by imgui are then passed back in a vector
    // for the main SFML app to handle.
    std::vector<sf::Event> processEvents();
    Status build();
    void render();

private:
    sf::Clock m_deltaClock;
    sf::RenderWindow& m_window;
    int m_currentListBoxIndex { -1 };
};

} // namespace imgui
} // namespace mgo