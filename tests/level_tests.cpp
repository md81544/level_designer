// Tests that the editor's model (Level) loads and saves levels without
// changing them

#include "level.h"
#include "levelformat.h"
#include "levelformat_test_utils.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <filesystem>
#include <fstream>

using namespace mgo;

namespace {

// A temporary file, deleted when this goes out of scope
class TempFile {
public:
    explicit TempFile(const std::string& name)
        : m_path(std::filesystem::temp_directory_path() / ("level_designer_test_" + name))
    {
    }
    ~TempFile()
    {
        std::filesystem::remove(m_path);
    }
    std::string path() const
    {
        return m_path.string();
    }

private:
    std::filesystem::path m_path;
};

// What matters about a level, ignoring how its lines are split into polylines
struct Contents {
    std::string title;
    std::optional<levelformat::Ship> ship;
    std::optional<levelformat::Exit> exit;
    std::vector<levelformat::Fuel> fuel;
    std::vector<levelformat::Segment> walls;
    std::vector<std::vector<levelformat::Segment>> breakables;
    std::vector<levelformat::Segment> decorations;
    std::vector<std::vector<levelformat::Segment>> specialObjectLines;
    bool operator==(const Contents&) const = default;
};

Contents contentsOf(const levelformat::LevelData& level)
{
    Contents c { level.title, level.ship, level.exit, level.fuel, normalised(segmentsOf(level.walls)) };
    for (const auto& b : level.breakables) {
        c.breakables.push_back(normalised(segmentsOf(b.lines)));
    }
    c.decorations = normalised(segmentsOf(level.decorations));
    for (const auto& m : level.specialObjects) {
        c.specialObjectLines.push_back(normalised(segmentsOf(m.lines)));
    }
    return c;
}

// Loads the level into the editor, saves it, and checks nothing changed; then
// checks that loading and saving that gives exactly the same file
void checkLoadAndSave(const std::string& filename)
{
    INFO(filename);
    const levelformat::LevelData original = levelformat::read(filename);
    TempFile first("first.lvl");
    TempFile second("second.lvl");
    {
        Level level;
        level.load(filename);
        level.save(first.path());
        CHECK_FALSE(level.isDirty());
    }
    const levelformat::LevelData saved = levelformat::read(first.path());
    CHECK(contentsOf(saved) == contentsOf(original));
    for (std::size_t i = 0; i < std::min(saved.specialObjects.size(), original.specialObjects.size()); ++i) {
        auto motion = [](levelformat::SpecialObject m) {
            m.lines.clear();
            return m;
        };
        CHECK(motion(saved.specialObjects[i]) == motion(original.specialObjects[i]));
    }
    {
        Level level;
        level.load(first.path());
        level.save(second.path());
    }
    CHECK(readFile(first.path()) == readFile(second.path()));
}

} // namespace

TEST_CASE("The editor saves what it loads, including rotations and thicknesses")
{
    TempFile file("features.lvl");
    {
        std::ofstream out(file.path());
        out << R"({
          "formatVersion": 2,
          "title": "Features",
          "ship": { "x": 100, "y": 200, "rotation": 45 },
          "exit": { "x": 300, "y": 400, "rotation": 30, "thickness": 4 },
          "fuel": [ { "x": 1, "y": 2, "thickness": 3 } ],
          "walls": [ { "points": [[0, 0], [10, 0], [10, 10]], "closed": true, "thickness": 5 } ],
          "breakables": [
            { "lines": [ { "points": [[700, 550], [700, 650]] }, { "points": [[900, 550], [900, 650]] } ] },
            { "lines": [ { "points": [[1, 1], [2, 2]] } ] }
          ],
          "decorations": [ { "points": [[50, 50], [60, 50], [60, 60]], "thickness": 3 } ],
          "specialObjects": [
            { "lines": [ { "points": [[5, 5], [6, 6]] } ], "speedX": 0.5, "gravity": 20, "name": "m" },
            { "lines": [ { "points": [[5, 5], [6, 6]] } ],
              "path": { "offsets": [[100, -50], [0, 200]], "speed": 0.75, "loop": true }, "spin": 1 }
          ]
        })";
    }
    checkLoadAndSave(file.path());
}

TEST_CASE("The editor loads and saves amaze's levels without changing them")
{
    const std::filesystem::path dir(AMAZE_DATA_DIR);
    if (!std::filesystem::exists(dir)) {
        SKIP("amaze isn't alongside, at " << dir);
    }
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() == ".lvl") {
            checkLoadAndSave(entry.path().string());
        }
    }
}

TEST_CASE("The editor's example level loads and saves without changing")
{
    checkLoadAndSave(SOURCE_DIR "/example.lvl");
}

namespace {

// A level with a closed square of walls, (100, 100) to (300, 300)
const char* SQUARE_LEVEL = R"({
  "formatVersion": 2,
  "title": "Square",
  "walls": [ { "points": [[100, 100], [300, 100], [300, 300], [100, 300]], "closed": true } ]
})";

// The (normalised) wall segments of the level, as saved
std::vector<levelformat::Segment> wallsOf(Level& level)
{
    TempFile file("walls.lvl");
    level.save(file.path());
    return normalised(segmentsOf(levelformat::read(file.path()).walls));
}

std::vector<levelformat::Segment> segments(std::vector<std::array<int, 4>> lines)
{
    std::vector<levelformat::Segment> result;
    for (const auto& [x0, y0, x1, y1] : lines) {
        result.push_back({ { double(x0), double(y0) }, { double(x1), double(y1) } });
    }
    return normalised(result);
}

} // namespace

TEST_CASE("Lines in a closed shape can be edited individually")
{
    TempFile file("square.lvl");
    {
        std::ofstream out(file.path());
        out << SQUARE_LEVEL;
    }
    Level level;
    level.load(file.path());
    level.setSnapMode(SnapMode::NONE);
    const float zoom = 1.0f;

    SECTION("Select is the default mode")
    {
        CHECK(level.mode() == Mode::EDIT);
    }

    SECTION("Deleting one line leaves the others")
    {
        level.deleteLineAt({ 200, 101 }, zoom);
        CHECK(wallsOf(level)
              == segments({ { 300, 100, 300, 300 }, { 300, 300, 100, 300 }, { 100, 300, 100, 100 } }));
        level.undo();
        CHECK(wallsOf(level).size() == 4);
    }

    SECTION("Splitting a line adds a vertex, which can be dragged")
    {
        REQUIRE(level.canSplitLineAt({ 200, 102 }, zoom));
        level.splitLineAt({ 200, 102 }, zoom);
        CHECK(wallsOf(level)
              == segments({ { 100, 100, 200, 100 },
                            { 200, 100, 300, 100 },
                            { 300, 100, 300, 300 },
                            { 300, 300, 100, 300 },
                            { 100, 300, 100, 100 } }));
        // Can't split at a vertex
        CHECK_FALSE(level.canSplitLineAt({ 200, 100 }, zoom));

        const auto vertex = level.vertexAt({ 201, 101 }, zoom, false);
        REQUIRE(vertex.has_value());
        CHECK(*vertex == QPointF(200, 100));
        level.beginVertexDrag(*vertex);
        level.moveVertex({ 210, 50 });
        level.moveVertex({ 200, 40 });
        level.endTransform();
        CHECK(wallsOf(level)
              == segments({ { 100, 100, 200, 40 },
                            { 200, 40, 300, 100 },
                            { 300, 100, 300, 300 },
                            { 300, 300, 100, 300 },
                            { 100, 300, 100, 100 } }));
        // The drag undoes in one go
        level.undo();
        CHECK(wallsOf(level).size() == 5);
        CHECK(level.vertexAt({ 200, 100 }, zoom, false).has_value());

        SECTION("Joining the lines at a vertex removes it")
        {
            REQUIRE(level.canJoinLinesAt({ 200, 100 }, zoom));
            level.joinLinesAt({ 200, 100 }, zoom);
            CHECK(wallsOf(level) == segments({ { 100, 100, 300, 100 },
                                               { 300, 100, 300, 300 },
                                               { 300, 300, 100, 300 },
                                               { 100, 300, 100, 100 } }));
        }
    }

    SECTION("Dragging a vertex onto its neighbour removes the line between them")
    {
        level.beginVertexDrag({ 300, 100 });
        level.moveVertex({ 300, 300 });
        level.endTransform();
        CHECK(wallsOf(level)
              == segments({ { 100, 100, 300, 300 }, { 300, 300, 100, 300 }, { 100, 300, 100, 100 } }));
    }

    SECTION("Lines can't be joined where more than two meet")
    {
        level.changeMode(Mode::LINE);
        level.leftClicked({ 100, 100 }, zoom, false, false);
        level.mouseMoved({ 200, 200 });
        level.leftClicked({ 200, 200 }, zoom, false, false);
        level.finishCurrentLine();
        CHECK_FALSE(level.canJoinLinesAt({ 100, 100 }, zoom));
        CHECK(level.canJoinLinesAt({ 300, 300 }, zoom));
    }
}

TEST_CASE("Items can be placed directly, as from the context menu")
{
    Level level;
    level.changeMode(Mode::LINE);
    level.placeShip({ 100, 200 });
    CHECK(level.mode() == Mode::EDIT);
    CHECK(level.selectionBounds()->center() == QPointF(100, 200));
    level.placeExit({ 300, 400 });
    CHECK(level.selectionBounds()->center() == QPointF(300, 400));
    level.addFuel({ 500, 600 });
    level.addFuel({ 500, 600 }); // adds, rather than removing what's there

    TempFile file("placed.lvl");
    level.save(file.path());
    const auto saved = levelformat::read(file.path());
    REQUIRE(saved.ship.has_value());
    CHECK(saved.ship->x == 100);
    CHECK(saved.ship->y == 200);
    REQUIRE(saved.exit.has_value());
    CHECK(saved.exit->x == 300);
    CHECK(saved.exit->y == 400);
    CHECK(saved.fuel.size() == 2);
}

TEST_CASE("A special object's path can be set where clicked, dragged, and turns with the object")
{
    TempFile square("path_square.lvl");
    {
        // A 100 x 100 square centred on (250, 250)
        std::ofstream out(square.path());
        out << R"({ "formatVersion": 2,
                    "walls": [ { "points": [[200, 200], [300, 200], [300, 300], [200, 300]], "closed": true } ] })";
    }
    Level level;
    level.load(square.path());
    level.changeMode(Mode::EDIT);
    level.selectAll();
    level.convertSelectionToSpecialObject();
    REQUIRE(level.selectedSpecialObject().has_value());

    // Bouncing within a box is replaced by the path
    SpecialObject motion = *level.selectedSpecialObject();
    motion.xDelta = 1;
    motion.xMaxDifference = 50;
    level.setSelectedSpecialObjectMotion(motion);
    level.setPathEndAt({ 450, 251.4 });
    auto m = *level.selectedSpecialObject();
    CHECK(m.pathOffsets == std::vector<QPointF> { { 200, 1 } });
    CHECK(m.pathSpeed > 0);
    CHECK(m.xDelta == 0);
    CHECK(m.xMaxDifference == 0);
    CHECK(level.selectedPathPoints() == std::vector<QPointF> { { 450, 251 } });

    level.addPathPointAt({ 250, 450 });
    CHECK(level.pathPointAt({ 452, 250 }, 1) == 0);
    CHECK(level.pathPointAt({ 250, 448 }, 1) == 1);
    CHECK_FALSE(level.pathPointAt({ 350, 350 }, 1).has_value());

    // Dragging snaps to whole grid squares
    level.setSnapMode(SnapMode::GRID);
    level.beginPathPointDrag(0);
    level.movePathPoint({ 380, 170 });
    level.endTransform();
    CHECK(level.selectedSpecialObject()->pathOffsets == std::vector<QPointF> { { 150, -100 }, { 0, 200 } });

    // Moving the object leaves its path the same relative to it; turning it turns the path
    level.nudgeSelection(10, 0);
    CHECK(level.selectedPathPoints()[0] == QPointF(410, 150));
    level.beginTransform();
    level.rotateSelection(90, level.selectionBounds()->center());
    level.endTransform();
    CHECK(level.selectedSpecialObject()->pathOffsets == std::vector<QPointF> { { 100, 150 }, { -200, 0 } });

    TempFile file("path.lvl");
    level.save(file.path());
    const auto saved = levelformat::read(file.path());
    REQUIRE(saved.specialObjects.size() == 1);
    REQUIRE(saved.specialObjects[0].path.has_value());
    CHECK(saved.specialObjects[0].path->offsets == std::vector<levelformat::Point> { { 100, 150 }, { -200, 0 } });
    CHECK(saved.specialObjects[0].speedX == 0);

    level.removePathPoint(1);
    level.removePathPoint(0);
    CHECK(level.selectedSpecialObject()->pathOffsets.empty());
    level.undo();
    level.selectAll(); // undo clears the selection
    REQUIRE(level.selectedSpecialObject().has_value());
    CHECK(level.selectedSpecialObject()->pathOffsets.size() == 1);
}

TEST_CASE("Selected lines can be made decorations and back again")
{
    TempFile square("convert_square.lvl");
    {
        std::ofstream out(square.path());
        out << R"({ "formatVersion": 2,
                    "walls": [ { "points": [[200, 200], [300, 200], [300, 300], [200, 300]], "closed": true } ],
                    "breakables": [ { "lines": [ { "points": [[500, 500], [600, 500]] } ] } ] })";
    }
    Level level;
    level.load(square.path());
    level.changeMode(Mode::EDIT);
    level.selectOnlyItemAt({ 250, 200 }, 1, false); // the whole square
    CHECK(level.canConvertSelectionToDecoration());
    CHECK_FALSE(level.canConvertSelectionToWalls());

    level.convertSelectionToDecoration();
    CHECK_FALSE(level.canConvertSelectionToDecoration());
    CHECK(level.canConvertSelectionToWalls());
    TempFile file("converted.lvl");
    level.save(file.path());
    auto saved = levelformat::read(file.path());
    CHECK(saved.walls.empty());
    REQUIRE(saved.decorations.size() == 1);
    CHECK(saved.decorations[0].closed);
    CHECK(saved.decorations[0].points.size() == 4);
    CHECK(saved.breakables.size() == 1);

    // Back to walls, along with the breakable line
    level.selectAll();
    level.convertSelectionToWalls();
    level.save(file.path());
    saved = levelformat::read(file.path());
    CHECK(saved.decorations.empty());
    CHECK(saved.breakables.empty());
    CHECK(saved.walls.size() == 2);

    level.undo();
    level.save(file.path());
    saved = levelformat::read(file.path());
    CHECK(saved.decorations.size() == 1);
    CHECK(saved.breakables.size() == 1);
    CHECK(saved.walls.empty());
}
