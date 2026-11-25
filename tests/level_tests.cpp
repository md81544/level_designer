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
    std::vector<std::vector<levelformat::Segment>> movingObjectLines;
    bool operator==(const Contents&) const = default;
};

Contents contentsOf(const levelformat::LevelData& level)
{
    Contents c { level.title, level.ship, level.exit, level.fuel, normalised(segmentsOf(level.walls)) };
    for (const auto& b : level.breakables) {
        c.breakables.push_back(normalised(segmentsOf(b.lines)));
    }
    for (const auto& m : level.movingObjects) {
        c.movingObjectLines.push_back(normalised(segmentsOf(m.lines)));
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
    for (std::size_t i = 0; i < std::min(saved.movingObjects.size(), original.movingObjects.size()); ++i) {
        auto motion = [](levelformat::MovingObject m) {
            m.lines.clear();
            return m;
        };
        CHECK(motion(saved.movingObjects[i]) == motion(original.movingObjects[i]));
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
          "formatVersion": 1,
          "title": "Features",
          "ship": { "x": 100, "y": 200, "rotation": 45 },
          "exit": { "x": 300, "y": 400, "rotation": 30, "thickness": 4 },
          "fuel": [ { "x": 1, "y": 2, "thickness": 3 } ],
          "walls": [ { "points": [[0, 0], [10, 0], [10, 10]], "closed": true, "thickness": 5 } ],
          "breakables": [
            { "lines": [ { "points": [[700, 550], [700, 650]] }, { "points": [[900, 550], [900, 650]] } ] },
            { "lines": [ { "points": [[1, 1], [2, 2]] } ] }
          ],
          "movingObjects": [
            { "lines": [ { "points": [[5, 5], [6, 6]] } ], "speedX": 0.5, "gravity": 20, "name": "m" }
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
  "formatVersion": 1,
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
