#pragma once

#include <algorithm>

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

    // The same 0 to 1, taken slowly at both ends and quickly in the middle.
    // lerp(from, to, smoothstep(t)) is a smoothstepped interpolation,
    // and smoothstep(t) on its own is a fade.
    // https://solhsa.com/interpolation/
    inline float smoothstep(float t)
    {
        const float clamped = std::clamp(t, 0.0f, 1.0f);
        return clamped * clamped * (3.0f - 2.0f * clamped);
    }
}
