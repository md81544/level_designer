#include "helperfunctions.h"

namespace mgo {
namespace helperfunctions {

int sgn(int x)
{
    if (x > 0) {
        return 1;
    }
    if (x < 0) {
        return -1;
    }
    return 0;
}

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

} // namespace helperfunctions
} // namespace mgo
