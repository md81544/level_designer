#pragma once

#include <QPointF>

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

class QPainter;

// As this application is a bit quick-and-dirty, this class holds pretty much everything in it

enum class Mode {
    LINE,
    BREAKABLE,
    EDIT,
    EXIT,
    START,
    FUEL,
    MOVING, // objects which have motion
    POLYGON_CENTRE,
    POLYGON_RADIUS
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
    uint8_t thickness { 1 }; // unused currently
    bool inactive { false }; // lines don't get deleted, just deactivated, avoids index invalidation
    bool breakable { false };
};

struct StartPosition {
    unsigned x;
    unsigned y;
    unsigned r;
};

struct MovingObject {
    float xDelta { 0.f };
    float xMaxDifference { 0.f };
    float yDelta { 0.f };
    float yMaxDifference { 0.f };
    float rotationDelta { 0.f }; // rotation just continues around so no max
    float gravity { 0.f };
    std::vector<Line> lines {};
};

struct CurrentPolygon {
    std::optional<float> centreX { std::nullopt };
    std::optional<float> centreY { std::nullopt };
    unsigned sides { 12 };
    std::vector<Line> lines {};
};

// The level being edited, plus the editing state (current mode, selection
// etc). It knows nothing about windows: positions passed in are workspace
// coordinates (0 to MAP_SIZE in each direction), and drawing is done with a
// QPainter already transformed to workspace coordinates.
class Level {
public:
    static constexpr float MAP_SIZE = 2000.f;

    // Loads a level file, adding its contents to this level. If the file
    // doesn't exist, just remembers the name, so it will be created on save.
    // Throws on failure.
    void load(const std::string& filename);
    // Saves to the given file, which becomes the level's file name. Throws on
    // failure.
    void save(const std::string& filename);
    const std::string& fileName() const;
    const std::string& description() const;
    void setDescription(const std::string& description);
    bool isDirty() const;

    Mode mode() const;
    void changeMode(Mode mode);
    SnapMode snapMode() const;
    void setSnapMode(SnapMode snapMode);
    void cycleSnapMode();
    // Number of sides for polygons subsequently inserted (3 to 64)
    unsigned polygonSides() const;
    void setPolygonSides(unsigned sides);

    // Draws everything, including the grid and in-progress edits
    void draw(QPainter& painter) const;

    // Input, in workspace coordinates. zoomLevel is workspace units per pixel.
    void mouseMoved(QPointF pos);
    // extendSelection adds to (rather than replaces) the selection; singleLine
    // selects only the line clicked rather than all lines connected to it
    void leftClicked(QPointF pos, float zoomLevel, bool extendSelection, bool singleLine);
    // Stops drawing the current line (and finishes any moving object being drawn)
    void finishCurrentLine();

    // Operations on the selection
    bool hasSelection() const;
    bool hasSelectedLines() const;
    void deleteSelection();
    void nudgeSelection(int x, int y);
    void convertSelectionToMovingObject();
    // The selected moving object, if any
    std::optional<MovingObject> selectedMovingObject() const;
    // Sets the selected moving object's motion (not its lines) from the
    // supplied object
    void setSelectedMovingObjectMotion(const MovingObject& motion);

    void revert();
    void undo();
    void redo();

private:
    void drawLine(QPainter& painter, const Line& line, std::optional<std::size_t> idx = std::nullopt)
        const;
    void drawGridLines(QPainter& painter) const;
    void drawEdits(QPainter& painter) const;
    void drawObjects(QPainter& painter) const;
    void drawMovingObjectBoundary(QPainter& painter, const MovingObject& m, size_t idx) const;
    void drawCircle(QPainter& painter, float maxRadius, float centreX, float centreY) const;
    void drawRoundedRect(QPainter& painter, float x, float y, float w, float h, float r) const;
    // Returns the index (into m_Lines) of the first (of potentially several) lines that are *near*
    // the cursor or no value if no lines are nearby.
    std::optional<std::size_t> lineUnderCursor(QPointF pos, float zoomLevel) const;
    std::optional<std::size_t> movingObjectUnderCursor(QPointF pos) const;
    void highlightGridVertex(QPointF pos);
    void highlightNearestLinePoint(QPointF pos);
    void replay();
    void addReplayItem(const Action& action);
    void finishCurrentMovingObject();
    void addOrRemoveHighlightedLine(std::optional<size_t>& lineIdx, bool includeConnectedLines);
    void addConnectedLinesToHighlight(const Line& line);
    void moveMovingObject(std::size_t movingObjectIdx, int x, int y);
    void moveLines(int x, int y);

    std::string m_levelDescription;
    std::vector<Line> m_lines;
    std::optional<StartPosition> m_startPosition;
    std::optional<std::pair<unsigned, unsigned>> m_exitPosition;
    std::vector<std::pair<unsigned, unsigned>> m_fuelObjects;
    std::vector<MovingObject> m_movingObjects;

    std::vector<Action> m_replay; // this is used for undo/redo
    long m_replayIndex { 0 };

    std::set<std::size_t> m_highlightedLineIndices;
    std::optional<std::size_t> m_highlightedMovingObjectIdx;
    std::optional<std::tuple<unsigned, unsigned>> m_currentNearestSnapPoint { std::nullopt };
    Line m_currentInsertionLine { 0, 0, 0, 0, 255, 0, 0, 1, true };
    MovingObject m_currentMovingObject;
    Mode m_currentMode { Mode::LINE };
    SnapMode m_snapMode { SnapMode::AUTO };
    std::string m_fileName;
    bool m_dirty { false };
    CurrentPolygon m_currentPolygon;
};

} // namespace mgo
