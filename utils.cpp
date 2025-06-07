#include "utils.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

unsigned squaredDistance(unsigned x1, unsigned y1, unsigned x2, unsigned y2)
{
    return (x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1);
}

}

namespace mgo {
namespace utils {

bool doLinesIntersect(long x1, long y1, long x2, long y2, long x3, long y3, long x4, long y4)
{

    long Ax, Bx, Cx, Ay, By, Cy, d, e, f;
    short x1lo, x1hi, y1lo, y1hi;

    Ax = x2 - x1;
    Bx = x3 - x4;

    if (Ax < 0) {
        x1lo = (short)x2;
        x1hi = (short)x1;
    } else {
        x1hi = (short)x2;
        x1lo = (short)x1;
    }
    if (Bx > 0) {
        if (x1hi < (short)x4 || (short)x3 < x1lo) {
            return false;
        }
    } else {
        if (x1hi < (short)x3 || (short)x4 < x1lo) {
            return false;
        }
    }

    Ay = y2 - y1;
    By = y3 - y4;

    if (Ay < 0) {
        y1lo = (short)y2;
        y1hi = (short)y1;
    } else {
        y1hi = (short)y2;
        y1lo = (short)y1;
    }
    if (By > 0) {
        if (y1hi < (short)y4 || (short)y3 < y1lo) {
            return false;
        }
    } else {
        if (y1hi < (short)y3 || (short)y4 < y1lo) {
            return false;
        }
    }

    Cx = x1 - x3;
    Cy = y1 - y3;
    d = By * Cx - Bx * Cy; // alpha numerator
    f = Ay * Bx - Ax * By; // both denominator
    // alpha tests
    if (f > 0) {
        if (d < 0 || d > f) {
            return false;
        }
    } else {
        if (d > 0 || d < f) {
            return false;
        }
    }

    e = Ax * Cy - Ay * Cx; // beta numerator
    // beta tests
    if (f > 0) {
        if (e < 0 || e > f) {
            return false;
        }
    } else {
        if (e > 0 || e < f) {
            return false;
        }
    }

    // if we get here, the lines either intersect or are collinear.
    return true;
}

std::optional<std::pair<unsigned, unsigned>> closestPointOnLine(
    unsigned x0,
    unsigned y0,
    unsigned x1,
    unsigned y1,
    unsigned x,
    unsigned y,
    unsigned d)
{
    const int dx = x1 - x0;
    const int dy = y1 - y0;

    if (dx == 0 && dy == 0) {
        if (squaredDistance(x, y, x0, y0) <= d * d) {
            return { { x0, y0 } };
        } else {
            return std::nullopt;
        }
    }

    int px = x - x0;
    int py = y - y0;

    double t = (px * dx + py * dy) / (double)(dx * dx + dy * dy);
    t = std::max(0.0, std::min(1.0, t)); // Clamping t to the range [0, 1]

    const unsigned nearestX = x0 + t * dx;
    const unsigned nearestY = y0 + t * dy;

    // Check if this nearest point is within the allowed distance `d`
    if (squaredDistance(x, y, nearestX, nearestY) <= d * d) {
        return { { nearestX, nearestY } };
    } else {
        return std::nullopt;
    }
}

} // namespace utils
} // namespace mgo
