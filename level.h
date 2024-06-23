#pragma once
#include <string>
#include <tuple>
#include <vector>

// As this application is a bit quick-and-dirty, this class holds pretty much everything in it

namespace mgo {

struct Line {
    unsigned int x;
    unsigned int y;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t thickness;
};

class Level {
public:
    void load(const std::string& filename);
private:
    std::vector<Line> m_lines;
};

} // namespace mgo