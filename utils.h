#pragma once

#include <optional>
#include <sstream>

namespace mgo {
namespace utils {

template <typename T> std::string to_string_with_precision(const T a_value, const int n = 6)
{
    std::ostringstream out;
    out.precision(n);
    out << std::fixed << a_value;
    return std::move(out).str();
}

bool doLinesIntersect(long x1, long y1, long x2, long y2, long x3, long y3, long x4, long y4);

std::optional<std::pair<unsigned, unsigned>> closestPointOnLine(
    unsigned x1,
    unsigned y1,
    unsigned x2,
    unsigned y2,
    unsigned x,
    unsigned y,
    unsigned d);

} // namespace utils
} // namespace mgo
