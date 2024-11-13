#pragma once

#include <optional>

namespace mgo {
namespace helperfunctions {

int sgn(int x);
bool doLinesIntersect(long x1, long y1, long x2, long y2, long x3, long y3, long x4, long y4);

std::optional<std::pair<unsigned, unsigned>> closestPointOnLine(
    unsigned x1,
    unsigned y1,
    unsigned x2,
    unsigned y2,
    unsigned x,
    unsigned y,
    unsigned d);

} // namespace helperfunctions
} // namespace mgo
