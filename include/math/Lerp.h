#pragma once

#include "math/Vec2.h"

namespace math
{
    // Somewhere on the line from `from` to `to`. t is 0 at one end and 1 at the
    // other, so a random t is a random point in the range and a decaying t is a
    // fade between two values.
    inline float lerp(float from, float to, float t)
    {
        return from + (to - from) * t;
    }

    inline Vec2 lerp(Vec2 from, Vec2 to, float t)
    {
        return Vec2{lerp(from.x, to.x, t), lerp(from.y, to.y, t)};
    }
}
