#pragma once

#include "math/Vec2.h"

namespace math
{
    // Axis-aligned bounding box, stored as its two opposite corners.
    struct AABB
    {
        Vec2 min;
        Vec2 max;
    };
}
