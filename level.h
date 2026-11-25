#pragma once

#include <QPointF>
#include <QRectF>
#include <QSizeF>

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

struct Line {
    unsigned x0;
    unsigned y0;
    unsigned x1;
    unsigned y1;
    uint8_t r { 0 };
    uint8_t g { 0 };
    uint8_t b { 0 };
    double thickness { 0 }; // 0 means the default for the line's type
    bool inactive { false }; // lines don't get deleted, just deactivated, avoids index invalidation
    bool breakable { false };
    // Breakable lines with the same group break together (0 for other lines)
    unsigned breakableGroup { 0 };
};

struct StartPosition {
    unsigned x;
    unsigned y;
    unsigned r; // degrees anticlockwise
};

struct ExitPosition {
    unsigned x;
    unsigned y;
    unsigned r { 0 }; // degrees anticlockwise
    double thickness { 0 }; // 0 means the default
};

struct FuelPosition {
    unsigned x;
    unsigned y;
    double thickness { 0 }; // 0 means the default
};

struct MovingObject {
    double xDelta { 0 };
    double xMaxDifference { 0 };
    double yDelta { 0 };
    double yMaxDifference { 0 };
    double rotationDelta { 0 }; // rotation just continues around so no max
    double gravity { 0 };
    std::vector<Line> lines {};
    std::optional<std::string> name {};
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

    // Loads a level file (in either format: see LEVEL_FORMAT.md), adding its
    // contents to this level. If the file doesn't exist, just remembers the
    // name, so it will be created on save. Throws on failure.
    void load(const std::string& filename);
    // Saves to the given file (in JSON), which becomes the level's file name.
    // Throws on failure.
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
    // In EDIT mode, selects the item clicked, if any. extendSelection toggles
    // the item in the selection, rather than replacing the selection;
    // singleLine selects only the line clicked rather than all lines
    // connected to it.
    void leftClicked(QPointF pos, float zoomLevel, bool extendSelection, bool singleLine);
    // Stops drawing the current line (and finishes any moving object being drawn)
    void finishCurrentLine();

    // Selection (in EDIT mode). Any number of lines, moving objects and fuel,
    // the exit and the ship can be selected.
    bool hasSelection() const;
    bool hasSelectedLines() const;
    bool isItemAt(QPointF pos, float zoomLevel) const;
    bool isSelectedItemAt(QPointF pos, float zoomLevel) const;
    // Selects just the item at pos (if any)
    void selectOnlyItemAt(QPointF pos, float zoomLevel, bool singleLine);
    // Selects the items entirely inside the rectangle, adding to the existing
    // selection if extendSelection
    void selectInRect(const QRectF& rect, bool extendSelection);
    void selectAll();
    void clearSelection();
    // The area covered by the selected items, if any
    std::optional<QRectF> selectionBounds() const;

    // Operations on the selection
    void deleteSelection();
    void nudgeSelection(int x, int y);
    // Dragging the selection: call beginTransform() at the start of the drag,
    // then moveSelection() / rotateSelection() as it progresses, with the total
    // movement / rotation since the start, then endTransform(). The drag is
    // undone as one change. The selection is kept within the map.
    void beginTransform();
    void moveSelection(int x, int y);
    // Rotates clockwise (on screen) by degrees about centre
    void rotateSelection(double degrees, QPointF centre);
    void endTransform();
    void convertSelectionToMovingObject();
    // The selected moving object, if exactly one is selected
    std::optional<MovingObject> selectedMovingObject() const;
    // Sets the selected moving object's motion (not its lines) from the
    // supplied object
    void setSelectedMovingObjectMotion(const MovingObject& motion);

    // Each change to the level's contents can be undone separately (except
    // consecutive nudges, which undo together)
    bool canUndo() const;
    bool canRedo() const;
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
    struct Item {
        enum class Type {
            SHIP,
            EXIT,
            FUEL,
            LINE,
            MOVING_OBJECT
        };
        Type type;
        std::size_t index { 0 }; // for FUEL, LINE and MOVING_OBJECT
    };
    std::optional<Item> itemAt(QPointF pos, float zoomLevel) const;
    // Half the size of the box the (rotated) exit fits in
    QSizeF exitHalfSize() const;
    bool isSelected(const Item& item) const;
    void select(const Item& item, bool includeConnectedLines);
    void deselect(const Item& item);
    // Calls fn(x, y) with references to the coordinates of each point of the selected items
    template <typename Fn> void forEachSelectedPoint(Fn fn);
    // Call before changing the contents during a transform
    void transformCheckpoint();
    // Rotates the selection as rotateSelection(), unless that would take it
    // off the map, in which case it returns false without changing anything
    bool applyRotation(double degrees, QPointF centre);
    void highlightGridVertex(QPointF pos);
    void highlightNearestLinePoint(QPointF pos);
    // The level's contents, for undo / redo
    struct Snapshot {
        unsigned long stateId;
        std::string description;
        std::vector<Line> lines;
        std::optional<StartPosition> startPosition;
        std::optional<ExitPosition> exitPosition;
        std::vector<FuelPosition> fuelObjects;
        std::vector<MovingObject> movingObjects;
        MovingObject currentMovingObject;
    };
    enum class Change {
        Other,
        Nudge
    };
    static constexpr std::size_t MAX_UNDO = 1000;
    Snapshot snapshot() const;
    void restore(const Snapshot& snapshot);
    // Copies the contents (but not the state ID) from the snapshot
    void restoreContents(const Snapshot& snapshot);
    // Must be called before each change to the level's contents, so it can be undone
    void checkpoint(Change change = Change::Other);
    void finishCurrentMovingObject();
    void addConnectedLinesToHighlight(const Line& line);
    // Moves the selection, limiting the movement to keep it on the map
    void translateSelection(int x, int y);

    std::string m_levelDescription;
    std::vector<Line> m_lines;
    std::optional<StartPosition> m_startPosition;
    std::optional<ExitPosition> m_exitPosition;
    std::vector<FuelPosition> m_fuelObjects;
    // For assigning a new breakable line's group (see Line::breakableGroup)
    unsigned m_nextBreakableGroup { 1 };
    std::vector<MovingObject> m_movingObjects;

    std::vector<Snapshot> m_undoStack;
    std::vector<Snapshot> m_redoStack;
    // Identifies the state of the level's contents, so we know whether it's
    // the state last saved
    unsigned long m_stateId { 0 };
    unsigned long m_savedStateId { 0 };
    unsigned long m_nextStateId { 1 };
    bool m_lastChangeWasNudge { false };

    // The selection
    std::set<std::size_t> m_highlightedLineIndices;
    std::set<std::size_t> m_selectedMovingObjects;
    std::set<std::size_t> m_selectedFuel;
    bool m_exitSelected { false };
    bool m_shipSelected { false };
    // The contents at the start of a transform (see beginTransform())
    std::optional<Snapshot> m_transformOrigin;
    bool m_transformCheckpointed { false };
    double m_lastValidRotation { 0.0 };

    std::optional<std::tuple<unsigned, unsigned>> m_currentNearestSnapPoint { std::nullopt };
    Line m_currentInsertionLine { 0, 0, 0, 0, 255, 0, 0, 0, true };
    MovingObject m_currentMovingObject;
    Mode m_currentMode { Mode::LINE };
    SnapMode m_snapMode { SnapMode::AUTO };
    std::string m_fileName;
    CurrentPolygon m_currentPolygon;
};

} // namespace mgo
