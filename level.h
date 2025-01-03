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
    FUEL,
    MOVING // objects which have motion
};

enum class SnapMode {
    NONE,
    GRID,
    LINE,
    AUTO
};

namespace mgo {

struct Action {
    Mode actionType;
    std::size_t index;
    unsigned x0 { 0 };
    unsigned y0 { 0 };
    unsigned x1 { 0 };
    unsigned y1 { 0 };
    unsigned rotation { 0 };
    bool erased { false };
};

struct Line {
    unsigned x0;
    unsigned y0;
    unsigned x1;
    unsigned y1;
    uint8_t r { 0 };
    uint8_t g { 0 };
    uint8_t b { 0 };
    uint8_t thickness; // unused currently
    bool inactive { false }; // lines don't get deleted, just deactivated, avoids index invalidation
    bool breakable { false };
};

struct StartPosition {
    unsigned x;
    unsigned y;
    unsigned r;
};

struct MovingObject {
    float xDelta;
    float xMaxDifference;
    float yDelta;
    float yMaxDifference;
    float rotationDelta; // rotation just continues around so no max
    std::vector<Line> lines;
};

class Level {
public:
    Level(unsigned windowWidth, unsigned windowHeight);
    void load(const std::string& filename);
    void save();
    void draw(sf::RenderWindow& window);
    void drawDialog(sf::RenderWindow& window);
    void drawLine(sf::RenderWindow& window, const Line& line, std::optional<std::size_t> idx);
    void drawGridLines(sf::RenderWindow& window);
    // Returns the index (into m_Lines) of the first (of potentially several) lines that are *near*
    // the cursor or no value if no lines are nearby.
    std::optional<std::size_t>
    lineUnderCursor(sf::RenderWindow& window, unsigned mouseX, unsigned mouseY);
    std::optional<std::size_t>
    movingObjectUnderCursor(sf::RenderWindow& window, unsigned mouseX, unsigned mouseY);
    void processEvent(sf::RenderWindow& window, const sf::Event& event);
    void quit(sf::RenderWindow& window);
    void zoomOut();
    void zoomIn();
    bool msgbox(
        const std::string& title,
        const std::string& message,
        std::function<void(bool, const std::string&)> callback);
    void drawModes(sf::RenderWindow& window);
    void highlightGridVertex(sf::RenderWindow& window, unsigned mouseX, unsigned mouseY);
    void highlightNearestLinePoint(sf::RenderWindow& window, unsigned mouseX, unsigned mouseY);
    void drawObjects(sf::RenderWindow& window);
    sf::View& getView();
    sf::View& getFixedView();
    void processViewport();
    void revert();
    void undo();
    void replay();
    void redo();
    void addReplayItem(const Action& action);

private:
    std::vector<Line> m_lines;
    std::optional<StartPosition> m_startPosition;
    std::optional<std::pair<unsigned, unsigned>> m_exitPosition;
    std::vector<std::pair<unsigned, unsigned>> m_fuelObjects;
    std::vector<MovingObject> m_movingObjects;

    std::vector<Action> m_replay; // this is used for undo/redo
    long m_replayIndex { 0 };

    bool m_isDialogActive { false };
    sf::RectangleShape m_dialog;
    sf::Text m_dialogTitle;
    sf::Text m_dialogText;
    std::function<void(bool, std::string)> m_dialogCallback { [](bool, const std::string&) { } };
    sf::Font m_font;
    sf::Text m_editModeText;
    std::optional<std::size_t> m_highlightedLineIdx;
    std::optional<std::size_t> m_highlightedMovingObjectIdx;
    std::optional<std::tuple<unsigned, unsigned>> m_currentNearestSnapPoint { std::nullopt };
    Line m_currentInsertionLine;
    MovingObject m_currentMovingObject;
    Mode m_currentMode { Mode::LINE };
    SnapMode m_snapMode { SnapMode::AUTO };
    void changeMode(Mode mode);
    void cycleMode(bool backwards);
    void changeSnapMode();
    sf::View m_view;
    float m_viewZoomLevel { 1.f };
    sf::View m_fixedView; // for non-moving elements, e.g. dialog
    std::string m_fileName;
    bool m_dirty { false };
    std::optional<int> m_oldMouseX;
    std::optional<int> m_oldMouseY;
};

} // namespace mgo