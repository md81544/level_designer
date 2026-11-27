// Tests for reading and writing level files (see LEVEL_FORMAT.md)
//
// NB this file is copied, unchanged, in both amaze and level_designer: see
// levelformat.h.

#include "levelformat.h"
#include "levelformat_test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace mgo::levelformat;

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
    level.introduction = "<p>Line one</p>\n<p align=\"left\">Line \"two\"</p>";
    level.ship = Ship { 706, 904, 270 };
    level.exit = Exit { 1200, 300, 45, 4 };
    level.fuel = { { 500, 500 }, { 650.5, 820, 4 } };
    level.walls = {
        { { { 650, 600 }, { 1450, 600 }, { 1450, 1350 } }, true },
        { { { 750, 700 }, { 1350, 700 } }, false, 3 },
    };
    level.breakables = { { { { { { 700, 550 }, { 700, 650 } } }, { { { 900, 550 }, { 900, 650 } } } } } };
    level.decorations = { { { { 100, 100 }, { 200, 100 }, { 200, 200 } }, true, 4 } };
    SpecialObject bouncing;
    bouncing.lines = { { { { 1, 2 }, { 3, 4 }, { 5, 6 } }, true } };
    bouncing.speedX = 0.3;
    bouncing.rangeX = 100;
    bouncing.speedY = -1;
    bouncing.rangeY = 200;
    bouncing.spin = 0.5;
    bouncing.gravity = 50;
    bouncing.name = "spinner";
    SpecialObject onPath;
    onPath.lines = { { { { 0, 0 }, { 1, 1 } } } };
    onPath.path = Path { { { 200, -150.5 }, { 0, 300 } }, 0.5, true };
    level.specialObjects = { bouncing, SpecialObject { { { { { 0, 0 }, { 1, 1 } } } } }, onPath };

    CHECK(parseJson(toJson(level)) == level);
    CHECK(parse(toJson(level)) == level);
}

TEST_CASE("JSON is written in the documented layout, leaving out defaults")
{
    LevelData level;
    level.title = "Test";
    level.introduction = "Line one\nLine two";
    level.ship = Ship { 706, 904, 0 };
    level.exit = Exit { 1200, 300 };
    level.fuel = { { 500, 500 }, { 650, 820, 4 } };
    level.walls = { { { { 0, 0 }, { 10, 0 }, { 10, 10 } }, true } };
    level.breakables = { { { { { { 1, 2 }, { 3, 4 } } } } } };
    level.decorations = { { { { 20, 20 }, { 30, 30 } } } };
    SpecialObject bouncing;
    bouncing.lines = { { { { 1, 2 }, { 3, 4 } } } };
    bouncing.speedX = 0.3;
    bouncing.rangeX = 100;
    SpecialObject onPath;
    onPath.lines = { { { { 5, 6 }, { 7, 8 } } } };
    onPath.path = Path { { { 200, -150 } }, 0.5 };
    onPath.spin = 1;
    level.specialObjects = { bouncing, onPath };

    CHECK(toJson(level) == R"({
  "formatVersion": 2,
  "title": "Test",
  "introduction": "Line one\nLine two",
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
  "decorations": [
    { "points": [[20, 20], [30, 30]] }
  ],
  "specialObjects": [
    {
      "lines": [
        { "points": [[1, 2], [3, 4]] }
      ],
      "speedX": 0.3,
      "rangeX": 100
    },
    {
      "lines": [
        { "points": [[5, 6], [7, 8]] }
      ],
      "path": { "offsets": [[200, -150]], "speed": 0.5 },
      "spin": 1
    }
  ]
}
)");
    CHECK(toJson(LevelData {}) == "{\n  \"formatVersion\": 2\n}\n");
}

TEST_CASE("Optional fields take their defaults, and unknown fields are ignored")
{
    const LevelData level = parseJson(R"({
        "formatVersion": 2,
        "someFutureField": { "anything": [1, 2, 3] },
        "ship": { "x": 1, "y": 2, "colour": "red" },
        "specialObjects": [ { "lines": [ { "points": [[0, 0], [1, 1]] } ] } ]
    })");
    LevelData expected;
    expected.ship = Ship { 1, 2, 0 };
    expected.specialObjects = { SpecialObject { { { { { 0, 0 }, { 1, 1 } } } } } };
    CHECK(level == expected);
}

TEST_CASE("Version 1 files' special objects are read from \"movingObjects\"")
{
    const std::string objects = R"([ { "lines": [ { "points": [[0, 0], [1, 1]] } ], "spin": 1 } ])";
    const LevelData v1 = parseJson(R"({ "formatVersion": 1, "movingObjects": )" + objects + " }");
    REQUIRE(v1.specialObjects.size() == 1);
    CHECK(v1.specialObjects[0].spin == 1);
    // From version 2, "movingObjects" is just an unknown field
    CHECK(parseJson(R"({ "formatVersion": 2, "movingObjects": )" + objects + " }").specialObjects.empty());
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
    CHECK(error("{ \"formatVersion\": 2, ").starts_with("Invalid JSON"));
    CHECK(error("{}") == "Missing \"formatVersion\"");
    CHECK(error("{ \"formatVersion\": 3 }").starts_with("This level is in a newer format (version 3)"));
    CHECK(error(R"({ "formatVersion": 2, "ship": { "x": 1 } })") == "ship: missing \"y\"");
    CHECK(
        error(R"({ "formatVersion": 2, "introduction": ["Hello"] })")
        == "\"introduction\" must be a string");
    CHECK(
        error(R"({ "formatVersion": 2, "walls": [ { "points": [[0, 0], [1]] } ] })")
        == "walls[0]: points[1] must be [x, y]");
    CHECK(
        error(R"({ "formatVersion": 2, "walls": [ { "points": [[0, 0]] } ] })")
        == "walls[0]: a polyline needs at least two points");
    CHECK(
        error(R"({ "formatVersion": 2, "specialObjects": [ { "lines": [ { "points": [[0, 0], [1, 1]], "closed": 1 } ] } ] })")
        == "specialObjects[0].lines[0]: \"closed\" must be true or false");
    const std::string line = R"("lines": [ { "points": [[0, 0], [1, 1]] } ])";
    auto specialError = [&](const std::string& fields) {
        return error(R"({ "formatVersion": 2, "specialObjects": [ { )" + line + ", " + fields + " } ] }");
    };
    CHECK(
        specialError(R"("path": { "offsets": [], "speed": 1 })")
        == "specialObjects[0].path: needs at least one offset");
    CHECK(
        specialError(R"("path": { "offsets": [[1, 2, 3]] })")
        == "specialObjects[0].path: offsets[0] must be [x, y]");
    CHECK(
        specialError(R"("path": { "offsets": [[1, 2]], "speed": -1 })")
        == "specialObjects[0].path: \"speed\" can't be negative");
    CHECK(
        specialError(R"("path": { "offsets": [[1, 2]], "loop": 1 })")
        == "specialObjects[0].path: \"loop\" must be true or false");
    CHECK(
        specialError(R"("speedX": 1, "path": { "offsets": [[1, 2]] })")
        == "specialObjects[0]: can't have both a \"path\" and speedX/rangeX/speedY/rangeY");
    CHECK(
        error("!~0~0~1~2~3~Title")
        == "This level file is in the old format, which is no longer supported");
}
