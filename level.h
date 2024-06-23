#pragma once
#include <SFML/Graphics.hpp>
#include <optional>
#include <string>
#include <tuple>
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
    void load(const std::string& filename);
    void save(const std::string& filename);
    void draw(sf::RenderWindow& window, float zoomLevel, int originX, int originY);
    void
    drawLine(sf::RenderWindow& window, const Line& line, float zoomLevel, int originX, int originY);
    void drawGridLines(sf::RenderWindow& window, float zoomLevel, int originX, int originY);
    // Returns the index (into m_Lines) of the first (of potentially several) lines that are *near*
    // the cursor or no value if no lines are nearby.
    std::optional<std::size_t> lineUnderCursor(unsigned int mouseX,
        unsigned int mouseY,
        float zoomLevel,
        int originX,
        int originY);
    void highlightLine(std::size_t idx);

private:
    std::vector<Line> m_lines;
};

} // namespace mgo