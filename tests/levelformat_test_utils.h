#pragma once

// Helpers for the level format tests
//
// NB this file is copied, unchanged, in both amaze and level_designer: see
// levelformat.h.

#include "levelformat.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

inline std::vector<std::string> splitFields(const std::string& s, char delimiter = '~')
{
    std::vector<std::string> fields;
    std::string field;
    std::istringstream in(s);
    while (std::getline(in, field, delimiter)) {
        fields.push_back(field);
    }
    if (!s.empty() && s.back() == delimiter) {
        fields.push_back("");
    }
    return fields;
}

inline std::string readFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream contents;
    contents << in.rdbuf();
    return contents.str();
}

// Segments in a canonical form (each with its lower point first, sorted), so
// sets of segments can be compared regardless of order and direction
inline std::vector<mgo::levelformat::Segment>
normalised(std::vector<mgo::levelformat::Segment> segments)
{
    auto key = [](const mgo::levelformat::Point& p) { return std::tie(p.x, p.y); };
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

inline std::vector<mgo::levelformat::Segment>
segmentsOf(const std::vector<mgo::levelformat::Polyline>& polylines)
{
    std::vector<mgo::levelformat::Segment> segments;
    for (const auto& p : polylines) {
        const auto s = mgo::levelformat::toSegments(p);
        segments.insert(segments.end(), s.begin(), s.end());
    }
    return segments;
}
