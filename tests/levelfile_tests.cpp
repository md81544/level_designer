// Tests for reading and writing level files (see LEVEL_FORMAT.md)

#include "levelfile.h"
#include "test_utils.h"
#include "utils.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace mgo::levelfile;

TEST_CASE("Segments which meet end to end are joined into polylines")
{
    // A square, its sides given in a muddled order and directions
    const std::vector<Segment> square {
        { { 0, 0 }, { 10, 0 } },
        { { 10, 10 }, { 0, 10 } },
        { { 10, 10 }, { 10, 0 } },
        { { 0, 0 }, { 0, 10 } },
    };
    auto polylines = toPolylines(square);
    REQUIRE(polylines.size() == 1);
    CHECK(polylines[0].closed);
    CHECK(polylines[0].points.size() == 4);
    CHECK(normalised(toSegments(polylines[0])) == normalised(square));

    // An open path, a separate line, and a line of a different thickness
    // which touches the path
    const std::vector<Segment> segments {
        { { 0, 0 }, { 10, 0 } },
        { { 10, 0 }, { 20, 5 } },
        { { 100, 100 }, { 200, 100 } },
        { { 20, 5 }, { 30, 5 }, 10 },
    };
    polylines = toPolylines(segments);
    REQUIRE(polylines.size() == 3);
    CHECK(polylines[0] == Polyline { { { 0, 0 }, { 10, 0 }, { 20, 5 } } });
    CHECK(polylines[1] == Polyline { { { 100, 100 }, { 200, 100 } } });
    CHECK(polylines[2] == Polyline { { { 20, 5 }, { 30, 5 } }, false, 10 });
}

TEST_CASE("A level survives being written as JSON and read back")
{
    LevelData level;
    level.title = "Quotes \" \\ and caf\xc3\xa9";
    level.ship = Ship { 706, 904, 270 };
    level.exit = Exit { 1200, 300, 45, 4 };
    level.fuel = { { 500, 500 }, { 650.5, 820, 4 } };
    level.walls = {
        { { { 650, 600 }, { 1450, 600 }, { 1450, 1350 } }, true },
        { { { 750, 700 }, { 1350, 700 } }, false, 3 },
    };
    level.breakables = { { { { { { 700, 550 }, { 700, 650 } } }, { { { 900, 550 }, { 900, 650 } } } } } };
    MovingObject moving;
    moving.lines = { { { { 1, 2 }, { 3, 4 }, { 5, 6 } }, true } };
    moving.speedX = 0.3;
    moving.rangeX = 100;
    moving.speedY = -1;
    moving.rangeY = 200;
    moving.spin = 0.5;
    moving.gravity = 50;
    moving.name = "spinner";
    level.movingObjects = { moving, MovingObject { { { { { 0, 0 }, { 1, 1 } } } } } };

    CHECK(parseJson(toJson(level)) == level);
    CHECK(parse(toJson(level)) == level);
}

TEST_CASE("JSON is written in the documented layout, leaving out defaults")
{
    LevelData level;
    level.title = "Test";
    level.ship = Ship { 706, 904, 0 };
    level.exit = Exit { 1200, 300 };
    level.fuel = { { 500, 500 }, { 650, 820, 4 } };
    level.walls = { { { { 0, 0 }, { 10, 0 }, { 10, 10 } }, true } };
    level.breakables = { { { { { { 1, 2 }, { 3, 4 } } } } } };
    MovingObject moving;
    moving.lines = { { { { 1, 2 }, { 3, 4 } } } };
    moving.speedX = 0.3;
    moving.rangeX = 100;
    level.movingObjects = { moving };

    CHECK(toJson(level) == R"({
  "formatVersion": 1,
  "title": "Test",
  "ship": { "x": 706, "y": 904 },
  "exit": { "x": 1200, "y": 300 },
  "fuel": [
    { "x": 500, "y": 500 },
    { "x": 650, "y": 820, "thickness": 4 }
  ],
  "walls": [
    { "points": [[0, 0], [10, 0], [10, 10]], "closed": true }
  ],
  "breakables": [
    { "lines": [{ "points": [[1, 2], [3, 4]] }] }
  ],
  "movingObjects": [
    {
      "lines": [
        { "points": [[1, 2], [3, 4]] }
      ],
      "speedX": 0.3,
      "rangeX": 100
    }
  ]
}
)");
    CHECK(toJson(LevelData {}) == "{\n  \"formatVersion\": 1\n}\n");
}

TEST_CASE("Optional fields take their defaults, and unknown fields are ignored")
{
    const LevelData level = parseJson(R"({
        "formatVersion": 1,
        "someFutureField": { "anything": [1, 2, 3] },
        "ship": { "x": 1, "y": 2, "colour": "red" },
        "movingObjects": [ { "lines": [ { "points": [[0, 0], [1, 1]] } ] } ]
    })");
    LevelData expected;
    expected.ship = Ship { 1, 2, 0 };
    expected.movingObjects = { MovingObject { { { { { 0, 0 }, { 1, 1 } } } } } };
    CHECK(level == expected);
}

TEST_CASE("Invalid level files are rejected with a helpful message")
{
    auto error = [](const std::string& contents) {
        try {
            parse(contents);
        } catch (const std::runtime_error& e) {
            return std::string(e.what());
        }
        return std::string("(no error)");
    };
    CHECK(error("") == "This isn't a level file (or it's empty)");
    CHECK(error("hello") == "This isn't a level file (or it's empty)");
    CHECK(error("{ \"formatVersion\": 1, ").starts_with("Invalid JSON"));
    CHECK(error("{}") == "Missing \"formatVersion\"");
    CHECK(error("{ \"formatVersion\": 2 }").starts_with("This level is in a newer format (version 2)"));
    CHECK(error(R"({ "formatVersion": 1, "ship": { "x": 1 } })") == "ship: missing \"y\"");
    CHECK(
        error(R"({ "formatVersion": 1, "walls": [ { "points": [[0, 0], [1]] } ] })")
        == "walls[0]: points[1] must be [x, y]");
    CHECK(
        error(R"({ "formatVersion": 1, "walls": [ { "points": [[0, 0]] } ] })")
        == "walls[0]: a polyline needs at least two points");
    CHECK(
        error(R"({ "formatVersion": 1, "movingObjects": [ { "lines": [ { "points": [[0, 0], [1, 1]], "closed": 1 } ] } ] })")
        == "movingObjects[0].lines[0]: \"closed\" must be true or false");
    CHECK(error("!~0~0~1~2~x~Title") == "line 1: \"x\" isn't a number");
}

TEST_CASE("Old format levels are converted as LEVEL_FORMAT.md describes")
{
    const LevelData level = parse("!~300~100~706~904~90~Old Level\n"
                                  "N~OBSTRUCTION~obstruction\n"
                                  // a square, and a separate line
                                  "L~0~0~10~0~255~0~0~2\n"
                                  "L~10~0~10~10~255~0~0~2\n"
                                  "L~0~10~10~10~255~0~0~2\n"
                                  "L~0~10~0~0~255~0~0~2\n"
                                  "L~50~50~60~60~255~0~0~2\n"
                                  // a breakable of two separate lines
                                  "N~BREAKABLE~breakable\n"
                                  "L~700~550~700~650~255~150~50~6\n"
                                  "L~900~550~900~650~255~150~50~6\n"
                                  // a breakable with a non-standard thickness
                                  "N~BREAKABLE~breakable\n"
                                  "L~1~1~2~2~255~150~50~10\n"
                                  "N~EXIT~exit\n"
                                  "T~EXIT~52~213~235\n"
                                  "P~1200~300\n"
                                  "N~FUEL~fuel\n"
                                  "T~*~255~255~0~12\n"
                                  "P~500~500\n"
                                  "N~FUEL~fuel\n"
                                  "T~*~255~255~0~4\n"
                                  "P~600~600\n"
                                  "N~PRISONER~unused\n"
                                  "L~5~5~6~6~1~2~3~4\n"
                                  "N~MOVING~moving_0~0.3~100~0~0~0.5~20\n"
                                  "L~1~2~3~4~255~172~163~6\n");
    LevelData expected;
    expected.title = "Old Level";
    expected.ship = Ship { 706, 904, 90 };
    expected.exit = Exit { 1200, 300 }; // thickness 6 (the default) when not given
    expected.fuel = { { 500, 500 }, { 600, 600, 4 } };
    expected.walls = {
        { { { 0, 0 }, { 10, 0 }, { 10, 10 }, { 0, 10 } }, true },
        { { { 50, 50 }, { 60, 60 } } },
    };
    expected.breakables = {
        { { { { { 700, 550 }, { 700, 650 } } }, { { { 900, 550 }, { 900, 650 } } } } },
        { { { { { 1, 1 }, { 2, 2 } }, false, 10 } } },
    };
    MovingObject moving;
    moving.lines = { { { { 1, 2 }, { 3, 4 } } } };
    moving.speedX = 0.3;
    moving.rangeX = 100;
    moving.spin = 0.5;
    moving.gravity = 20;
    expected.movingObjects = { moving };
    CHECK(level == expected);
}

TEST_CASE("All of amaze's levels convert from the old format without losing anything")
{
    const std::filesystem::path dir(AMAZE_DATA_DIR);
    if (!std::filesystem::exists(dir)) {
        SKIP("amaze isn't alongside, at " << dir);
    }
    int count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() != ".lvl") {
            continue;
        }
        const std::string contents = readFile(entry.path());
        if (!contents.starts_with('!')) {
            continue; // already converted
        }
        INFO(entry.path().filename());
        ++count;
        const LevelData level = parse(contents);

        // Compare the lines with those in the file, read independently here
        std::vector<Segment> walls;
        std::vector<Segment> breakableSegments;
        std::vector<Segment> movingSegments;
        int breakables = 0;
        int movingObjects = 0;
        std::string type;
        std::istringstream in(contents);
        std::string line;
        while (std::getline(in, line)) {
            const auto f = mgo::utils::split(line, '~');
            if (line.starts_with("N~")) {
                type = f[1];
                breakables += type == "BREAKABLE";
                movingObjects += type == "MOVING";
            } else if (line.starts_with("L~")) {
                const double thickness = f.size() > 8 ? std::stod(f[8]) : 1;
                const double usual = type == "OBSTRUCTION" ? 2 : 6;
                const Segment s { { std::stod(f[1]), std::stod(f[2]) },
                                  { std::stod(f[3]), std::stod(f[4]) },
                                  thickness == usual ? std::nullopt : std::optional(thickness) };
                (type == "OBSTRUCTION"   ? walls
                     : type == "BREAKABLE" ? breakableSegments
                                           : movingSegments)
                    .push_back(s);
            }
        }
        CHECK(normalised(segmentsOf(level.walls)) == normalised(walls));
        std::vector<Segment> converted;
        for (const auto& b : level.breakables) {
            auto s = segmentsOf(b.lines);
            converted.insert(converted.end(), s.begin(), s.end());
        }
        CHECK(normalised(converted) == normalised(breakableSegments));
        CHECK(static_cast<int>(level.breakables.size()) == breakables);
        converted.clear();
        for (const auto& m : level.movingObjects) {
            auto s = segmentsOf(m.lines);
            converted.insert(converted.end(), s.begin(), s.end());
        }
        CHECK(normalised(converted) == normalised(movingSegments));
        CHECK(static_cast<int>(level.movingObjects.size()) == movingObjects);

        // And the JSON reads back exactly
        CHECK(parseJson(toJson(level)) == level);
    }
    CHECK(count > 0);
}
