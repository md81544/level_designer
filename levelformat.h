#pragma once

// Reading and writing level files: see LEVEL_FORMAT.md, which defines the
// (JSON) format, and how the old line-based format converts to it.
//
// NB this file and levelformat.cpp are copied, unchanged, in both amaze and
// level_designer, so the two programs read and write levels the same way.
// Keep the copies identical: make any change in both repos (see
// LEVEL_FORMAT.md). The same goes for tests/levelformat_tests.cpp and
// tests/levelformat_test_utils.h.
//
// The structures here mirror the format, and are separate from each
// program's own model of a level, which it converts to / from them.

#include <optional>
#include <string>
#include <vector>

namespace mgo {
namespace levelformat {

constexpr int FORMAT_VERSION = 1;

// Default thicknesses (as in the game); fields left unset take these
constexpr double DEFAULT_WALL_THICKNESS = 2;
constexpr double DEFAULT_BREAKABLE_THICKNESS = 6;
constexpr double DEFAULT_MOVING_OBJECT_THICKNESS = 6;
constexpr double DEFAULT_EXIT_THICKNESS = 6;
constexpr double DEFAULT_FUEL_THICKNESS = 12;

struct Point {
    double x { 0 };
    double y { 0 };
    bool operator==(const Point&) const = default;
};

struct Polyline {
    std::vector<Point> points;
    bool closed { false };
    std::optional<double> thickness; // unset means the default for its use
    bool operator==(const Polyline&) const = default;
};

struct Ship {
    double x { 0 };
    double y { 0 };
    double rotation { 0 }; // degrees anticlockwise
    bool operator==(const Ship&) const = default;
};

struct Exit {
    double x { 0 };
    double y { 0 };
    double rotation { 0 }; // degrees anticlockwise
    std::optional<double> thickness;
    bool operator==(const Exit&) const = default;
};

struct Fuel {
    double x { 0 };
    double y { 0 };
    std::optional<double> thickness;
    bool operator==(const Fuel&) const = default;
};

struct Breakable {
    std::vector<Polyline> lines;
    bool operator==(const Breakable&) const = default;
};

// A moving object's path: it moves from where it's drawn through each offset
// in turn, then back again (or if loop, straight back to the start, and round
// again)
struct Path {
    std::vector<Point> offsets; // from the drawn position; at least one
    double speed { 0 }; // distance along the path per tick
    bool loop { false };
    bool operator==(const Path&) const = default;
};

struct MovingObject {
    std::vector<Polyline> lines;
    // Bouncing within a box (each axis independently). Unused if there's a path.
    double speedX { 0 };
    double rangeX { 0 };
    double speedY { 0 };
    double rangeY { 0 };
    double spin { 0 };
    double gravity { 0 };
    std::optional<std::string> name;
    std::optional<Path> path;
    bool operator==(const MovingObject&) const = default;
};

struct LevelData {
    std::string title;
    std::optional<Ship> ship;
    std::optional<Exit> exit;
    std::vector<Fuel> fuel;
    std::vector<Polyline> walls;
    std::vector<Breakable> breakables;
    std::vector<MovingObject> movingObjects;
    bool operator==(const LevelData&) const = default;
};

// Reads a level file in either format. Throws std::runtime_error on failure,
// with a message suitable for the user.
LevelData read(const std::string& filename);
// Writes a level file (in JSON). Throws std::runtime_error on failure.
void write(const std::string& filename, const LevelData& level);

// Parses a level file's contents, in either format
LevelData parse(const std::string& contents);
LevelData parseJson(const std::string& contents);
LevelData parseOldFormat(const std::string& contents);
// The JSON for a level, laid out as LEVEL_FORMAT.md recommends
std::string toJson(const LevelData& level);

// A single straight line, as used by the old format and the editor
struct Segment {
    Point start;
    Point end;
    std::optional<double> thickness;
    bool operator==(const Segment&) const = default;
};
// Joins segments which meet end to end into polylines (only joining segments
// of the same thickness); a polyline which ends where it starts is closed
std::vector<Polyline> toPolylines(const std::vector<Segment>& segments);
std::vector<Segment> toSegments(const Polyline& polyline);

} // namespace levelformat
} // namespace mgo
