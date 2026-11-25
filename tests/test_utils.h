#pragma once

// Helpers shared by the tests

#include "levelfile.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

inline std::string readFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream contents;
    contents << in.rdbuf();
    return contents.str();
}

// Segments in a canonical form (each with its lower point first, sorted), so
// sets of segments can be compared regardless of order and direction
inline std::vector<mgo::levelfile::Segment>
normalised(std::vector<mgo::levelfile::Segment> segments)
{
    auto key = [](const mgo::levelfile::Point& p) { return std::tie(p.x, p.y); };
    for (auto& s : segments) {
        if (key(s.end) < key(s.start)) {
            std::swap(s.start, s.end);
        }
    }
    std::ranges::sort(segments, [&](const auto& a, const auto& b) {
        return std::tuple(a.start.x, a.start.y, a.end.x, a.end.y, a.thickness.value_or(0))
            < std::tuple(b.start.x, b.start.y, b.end.x, b.end.y, b.thickness.value_or(0));
    });
    return segments;
}

inline std::vector<mgo::levelfile::Segment>
segmentsOf(const std::vector<mgo::levelfile::Polyline>& polylines)
{
    std::vector<mgo::levelfile::Segment> segments;
    for (const auto& p : polylines) {
        const auto s = mgo::levelfile::toSegments(p);
        segments.insert(segments.end(), s.begin(), s.end());
    }
    return segments;
}
