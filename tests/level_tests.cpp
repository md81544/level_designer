// Tests that the editor's model (Level) loads and saves levels without
// changing them

#include "level.h"
#include "levelfile.h"
#include "test_utils.h"

#include <catch2/catch_test_macros.hpp>

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
    std::optional<levelfile::Ship> ship;
    std::optional<levelfile::Exit> exit;
    std::vector<levelfile::Fuel> fuel;
    std::vector<levelfile::Segment> walls;
    std::vector<std::vector<levelfile::Segment>> breakables;
    std::vector<std::vector<levelfile::Segment>> movingObjectLines;
    bool operator==(const Contents&) const = default;
};

Contents contentsOf(const levelfile::LevelData& level)
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
    const levelfile::LevelData original = levelfile::read(filename);
    TempFile first("first.lvl");
    TempFile second("second.lvl");
    {
        Level level;
        level.load(filename);
        level.save(first.path());
        CHECK_FALSE(level.isDirty());
    }
    const levelfile::LevelData saved = levelfile::read(first.path());
    CHECK(contentsOf(saved) == contentsOf(original));
    for (std::size_t i = 0; i < std::min(saved.movingObjects.size(), original.movingObjects.size()); ++i) {
        auto motion = [](levelfile::MovingObject m) {
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
