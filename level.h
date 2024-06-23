#pragma once
#include <SFML/Graphics.hpp>
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
    void draw(sf::RenderWindow& window, float zoomLevel);
private:
    std::vector<Line> m_lines;
};

} // namespace mgo