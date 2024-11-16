#pragma once
#include <SFML/Graphics.hpp>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

// As this application is a bit quick-and-dirty, this class holds pretty much everything in it

enum class Mode {
    LINE,
    BREAKABLE,
    EDIT,
    EXIT,
    START,
    FUEL
};

enum class SnapMode {
    NONE,
    GRID,
    LINE
};

namespace mgo {

struct Line {
    unsigned int x0;
    unsigned int y0;
    unsigned int x1;
    unsigned int y1;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t thickness; // unused currently
    bool inactive { false }; // lines don't get deleted, just deactivated, avoids index invalidation
    bool breakable { false };
};

class Level {
public:
    Level(unsigned int windowWidth, unsigned int windowHeight);
    void load(const std::string& filename);
    void save();
    void draw(sf::RenderWindow& window);
    void drawDialog(sf::RenderWindow& window);
    void drawLine(sf::RenderWindow& window, const Line& line, std::optional<std::size_t> idx);
    void drawGridLines(sf::RenderWindow& window);
    // Returns the index (into m_Lines) of the first (of potentially several) lines that are *near*
    // the cursor or no value if no lines are nearby.
    std::optional<std::size_t>
    lineUnderCursor(sf::RenderWindow& window, unsigned int mouseX, unsigned int mouseY);
    void processEvent(sf::RenderWindow& window, const sf::Event& event);
    bool msgbox(
        const std::string& title,
        const std::string& message,
        std::function<void(bool, const std::string&)> callback);
    std::string inputbox(const std::string& title, const std::string& message);
    void displayMode(sf::RenderWindow& window);
    void highlightGridVertex(sf::RenderWindow& window, unsigned int mouseX, unsigned int mouseY);
    void
    highlightNearestLinePoint(sf::RenderWindow& window, unsigned int mouseX, unsigned int mouseY);
    void drawObjects(sf::RenderWindow& window);
    sf::View& getView();
    sf::View& getFixedView();
    void processViewport();

private:
    std::vector<Line> m_lines;
    bool m_isDialogActive { false };
    sf::RectangleShape m_dialog;
    sf::Text m_dialogTitle;
    sf::Text m_dialogText;
    std::function<void(bool, std::string)> m_dialogCallback { [](bool, const std::string&) { } };
    sf::Font m_font;
    sf::Text m_editModeText;
    std::optional<std::size_t> m_highlightedLineIdx;
    std::optional<std::tuple<unsigned int, unsigned int>> m_currentNearestGridVertex {
        std::nullopt
    };
    Line m_currentInsertionLine;
    Mode m_currentMode { Mode::LINE };
    SnapMode m_snapMode { SnapMode::LINE };
    void changeMode(bool backwards);
    std::optional<std::pair<unsigned int, unsigned int>> m_startPosition;
    std::optional<std::pair<unsigned int, unsigned int>> m_exitPosition;
    std::vector<std::pair<unsigned int, unsigned int>> m_fuelObjects;
    sf::View m_view;
    sf::View m_fixedView; // for non-moving elements, e.g. dialog
};

} // namespace mgo