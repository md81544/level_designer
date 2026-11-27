#pragma once

#include <QLineF>
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
    DECORATION, // lines which are only drawn: the ship passes over them
    EDIT,
    EXIT,
    START,
    FUEL,
    SPECIAL, // special objects (which can move, spin and/or have gravity)
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
    bool decoration { false }; // drawn, but not collided with
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

struct SpecialObject {
    // Bouncing within a box (unused if it has a path)
    double xDelta { 0 };
    double xMaxDifference { 0 };
    double yDelta { 0 };
    double yMaxDifference { 0 };
    double rotationDelta { 0 }; // rotation just continues around so no max
    double gravity { 0 };
    std::vector<Line> lines {};
    std::optional<std::string> name {};
    // Following a path (see LEVEL_FORMAT.md): the offsets from the drawn
    // position of each point along it. Empty if it has no path.
    std::vector<QPointF> pathOffsets {};
    double pathSpeed { 0 };
    bool pathLoops { false };
};

// The centre of a special object's lines (of their bounding box), from which
// its path is drawn
QPointF specialObjectCentre(const SpecialObject& m);

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
    // Stops drawing the current line (and finishes any special object being drawn)
    void finishCurrentLine();
    // Whether a line or polygon is being drawn
    bool isDrawing() const;

    // Placing items, as clicking in the START, EXIT and FUEL modes does
    // (except that adding fuel doesn't remove any already there). Placing the
    // ship or exit switches to EDIT mode and selects it, so it can be moved
    // or rotated.
    void placeShip(QPointF pos);
    void placeExit(QPointF pos);
    void addFuel(QPointF pos);

    // Selection (in EDIT mode). Any number of lines, special objects and fuel,
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
    void convertSelectionToSpecialObject();
    // Changing the type of the selected lines (e.g. to make a polygon a
    // decoration). Whether any selected lines aren't decorations / walls
    // already (breakable lines aren't walls):
    bool canConvertSelectionToDecoration() const;
    bool canConvertSelectionToWalls() const;
    void convertSelectionToDecoration();
    void convertSelectionToWalls();

    // Editing individual lines (walls, breakable lines and decorations, not special
    // objects'), at a position. A vertex is a point where lines meet (or a
    // line's free end).
    //
    // The line at pos, if any
    std::optional<QLineF> lineAt(QPointF pos, float zoomLevel) const;
    // The vertex at pos, if any; if selectedOnly, only the ends of selected lines
    std::optional<QPointF> vertexAt(QPointF pos, float zoomLevel, bool selectedOnly) const;
    // The ends of the selected lines (each once), for drawing handles on
    std::vector<QPointF> selectedLineVertices() const;
    void deleteLineAt(QPointF pos, float zoomLevel);
    // Splits the line at pos into two, with a new vertex at the nearest point
    // on the line to pos. Returns false if there's no line there, or pos is
    // too near either end.
    bool canSplitLineAt(QPointF pos, float zoomLevel) const;
    void splitLineAt(QPointF pos, float zoomLevel);
    // Replaces the two lines meeting at the vertex at pos with one straight
    // line. Only possible if exactly two lines (of the same type) meet there.
    bool canJoinLinesAt(QPointF pos, float zoomLevel) const;
    void joinLinesAt(QPointF pos, float zoomLevel);
    // Selects the line at pos, and if includeConnectedLines, all the lines
    // connected to it (switching to EDIT mode)
    void selectLineAt(QPointF pos, float zoomLevel, bool includeConnectedLines);
    // Dragging a vertex: all the lines meeting at it move with it. Call
    // beginVertexDrag() with the vertex (from vertexAt()), then
    // moveVertex() with the mouse position (which is snapped, as when
    // drawing) as the drag progresses, then endTransform(). Lines reduced to
    // nothing are removed.
    void beginVertexDrag(QPointF vertex);
    void moveVertex(QPointF pos);
    // The selected special object, if exactly one is selected
    std::optional<SpecialObject> selectedSpecialObject() const;
    // Sets the selected special object's motion (not its lines) from the
    // supplied object
    void setSelectedSpecialObjectMotion(const SpecialObject& motion);

    // Editing the selected special object's path (if exactly one is selected)
    //
    // Moves the end of its path (the last point) so the object's centre goes
    // to pos, giving it a path if it doesn't have one (replacing any bouncing
    // within a box)
    void setPathEndAt(QPointF pos);
    // Adds a point to the end of its path, if it has one
    void addPathPointAt(QPointF pos);
    // The point of its path at pos (an index into pathOffsets), if any
    std::optional<std::size_t> pathPointAt(QPointF pos, float zoomLevel) const;
    // Removes a point from its path (removing the path if it's the last one)
    void removePathPoint(std::size_t index);
    // Where the object's centre is at each point of its path, for drawing
    // handles on (empty if it has no path)
    std::vector<QPointF> selectedPathPoints() const;
    // Dragging a point of its path: call beginPathPointDrag() with the index,
    // then movePathPoint() with the mouse position (snapped to the grid
    // according to the snap mode) as the drag progresses, then endTransform()
    void beginPathPointDrag(std::size_t index);
    void movePathPoint(QPointF pos);

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
    void drawSpecialObjectBoundary(QPainter& painter, const SpecialObject& m, size_t idx) const;
    void drawSpecialObjectPath(QPainter& painter, const SpecialObject& m) const;
    // The selected special object, if exactly one is selected
    SpecialObject* singleSelectedSpecialObject();
    const SpecialObject* singleSelectedSpecialObject() const;
    void drawCircle(QPainter& painter, float maxRadius, float centreX, float centreY) const;
    void drawRoundedRect(QPainter& painter, float x, float y, float w, float h, float r) const;
    // Returns the index (into m_Lines) of the first (of potentially several) lines that are *near*
    // the cursor or no value if no lines are nearby.
    std::optional<std::size_t> lineUnderCursor(QPointF pos, float zoomLevel) const;
    std::optional<std::size_t> specialObjectUnderCursor(QPointF pos) const;
    struct Item {
        enum class Type {
            SHIP,
            EXIT,
            FUEL,
            LINE,
            SPECIAL_OBJECT
        };
        Type type;
        std::size_t index { 0 }; // for FUEL, LINE and SPECIAL_OBJECT
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
    // Ignores lines with an end at exclude (if given)
    void highlightNearestLinePoint(
        QPointF pos, std::optional<std::pair<unsigned, unsigned>> exclude = std::nullopt);
    // Sets m_currentNearestSnapPoint to what pos snaps to, according to the snap mode
    void updateSnapPoint(
        QPointF pos, std::optional<std::pair<unsigned, unsigned>> exclude = std::nullopt);
    // Index of the line nearest pos, if within hit testing distance; unlike
    // lineUnderCursor(), this is the nearest rather than the first
    std::optional<std::size_t> nearestLine(QPointF pos, float zoomLevel) const;
    // Indices of the (active) lines with an end at the point
    std::vector<std::size_t> linesWithEndAt(unsigned x, unsigned y) const;
    // The level's contents, for undo / redo
    struct Snapshot {
        unsigned long stateId;
        std::string description;
        std::vector<Line> lines;
        std::optional<StartPosition> startPosition;
        std::optional<ExitPosition> exitPosition;
        std::vector<FuelPosition> fuelObjects;
        std::vector<SpecialObject> specialObjects;
        SpecialObject currentSpecialObject;
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
    void finishCurrentSpecialObject();
    void addConnectedLinesToHighlight(const Line& line);
    // Moves the selection, limiting the movement to keep it on the map
    void translateSelection(int x, int y);

    std::string m_levelDescription;
    // Shown by amaze before the level starts. Not editable here yet, but kept
    // so that saving doesn't lose it.
    std::string m_levelIntroduction;
    std::vector<Line> m_lines;
    std::optional<StartPosition> m_startPosition;
    std::optional<ExitPosition> m_exitPosition;
    std::vector<FuelPosition> m_fuelObjects;
    // For assigning a new breakable line's group (see Line::breakableGroup)
    unsigned m_nextBreakableGroup { 1 };
    std::vector<SpecialObject> m_specialObjects;

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
    std::set<std::size_t> m_selectedSpecialObjects;
    std::set<std::size_t> m_selectedFuel;
    bool m_exitSelected { false };
    bool m_shipSelected { false };
    // The contents at the start of a transform (see beginTransform())
    std::optional<Snapshot> m_transformOrigin;
    bool m_transformCheckpointed { false };
    double m_lastValidRotation { 0.0 };
    // The vertex being dragged (see beginVertexDrag())
    std::optional<std::pair<unsigned, unsigned>> m_draggedVertex;
    // The path point being dragged (see beginPathPointDrag())
    std::optional<std::size_t> m_draggedPathPoint;

    std::optional<std::tuple<unsigned, unsigned>> m_currentNearestSnapPoint { std::nullopt };
    Line m_currentInsertionLine { 0, 0, 0, 0, 255, 0, 0, 0, true };
    SpecialObject m_currentSpecialObject;
    Mode m_currentMode { Mode::EDIT };
    SnapMode m_snapMode { SnapMode::AUTO };
    std::string m_fileName;
    CurrentPolygon m_currentPolygon;
};

} // namespace mgo
