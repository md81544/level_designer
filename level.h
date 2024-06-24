#pragma once
#include <SFML/Graphics.hpp>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

// As this application is a bit quick-and-dirty, this class holds pretty much everything in it

namespace mgo {

struct Line {
    bool isFirstLine;
    unsigned int x0;
    unsigned int y0;
    unsigned int x1;
    unsigned int y1;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t thickness;
};

class Level {
public:
    Level();
    void load(const std::string& filename);
    void save(const std::string& filename);
    void draw(sf::RenderWindow& window);
    void drawDialog(sf::RenderWindow& window);
    void drawLine(sf::RenderWindow& window, const Line& line);
    void drawGridLines(sf::RenderWindow& window);
    // Returns the index (into m_Lines) of the first (of potentially several) lines that are *near*
    // the cursor or no value if no lines are nearby.
    std::optional<std::size_t> lineUnderCursor(unsigned int mouseX, unsigned int mouseY);
    void highlightLine(std::size_t idx);
    void processEvent(sf::RenderWindow& window, const sf::Event& event);
    bool msgbox(const std::string& title, const std::string& message);
    std::string inputbox(const std::string& title, const std::string& message);

private:
    std::vector<Line> m_lines;
    bool m_isDialogActive { false };
    float m_zoomLevel { 0.4 };
    std::string m_saveFilename;
    int m_originX { 0 };
    int m_originY { 0 };
    sf::RectangleShape m_dialog;
    sf::Text m_dialogTitle;
    sf::Text m_dialogText;
    sf::Font m_font;
    std::variant<bool, std::string> m_dialogResult; // true = OK, false = Cancel, string = input
};

} // namespace mgo