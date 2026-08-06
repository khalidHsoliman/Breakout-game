#pragma once

#include <algorithm>
#include <cmath>

#include "math/AABB.h"
#include "math/Circle.h"
#include "math/Vec2.h"

namespace math
{
    struct Contact
    {
        bool hit = false;

        // Unit length, pointing from the box towards the circle.
        Vec2 normal;

        // How far along the normal the circle must move to stop overlapping.
        float penetration = 0.0f;
    };

    inline Contact circle_vs_aabb(Circle circle, AABB box)
    {
        const Vec2 closest{std::clamp(circle.center.x, box.min.x, box.max.x),
                           std::clamp(circle.center.y, box.min.y, box.max.y)};

        const Vec2 to_circle = circle.center - closest;
        const float distance_squared = length_squared(to_circle);

        // The centre is inside the box, so the closest point is the centre
        // itself and there is no direction to push along. Comparing against
        // zero exactly is safe here: clamp returned the centre unchanged, so
        // the subtraction is exact rather than computed.
        if (distance_squared == 0.0f)
        {
            const float to_left = circle.center.x - box.min.x;
            const float to_right = box.max.x - circle.center.x;
            const float to_bottom = circle.center.y - box.min.y;
            const float to_top = box.max.y - circle.center.y;

            float smallest = to_left;
            Vec2 normal{-1.0f, 0.0f};

            if (to_right < smallest)
            {
                smallest = to_right;
                normal = Vec2{1.0f, 0.0f};
            }
            if (to_bottom < smallest)
            {
                smallest = to_bottom;
                normal = Vec2{0.0f, -1.0f};
            }
            if (to_top < smallest)
            {
                smallest = to_top;
                normal = Vec2{0.0f, 1.0f};
            }

            return Contact{true, normal, smallest + circle.radius};
        }

        // Compared squared to avoid a square root on the common miss.
        if (distance_squared >= circle.radius * circle.radius)
        {
            return Contact{};
        }

        const float distance = std::sqrt(distance_squared);
        return Contact{true, to_circle / distance, circle.radius - distance};
    }

    inline Vec2 reflect(Vec2 direction, Vec2 normal)
    {
        return direction - normal * (2.0f * dot(direction, normal));
    }

    // Where the ball hit the paddle decides where it leaves, ignoring the
    // incoming direction entirely - that is what gives the player control.
    //
    // offset is -1 at the paddle's left edge, 0 at its centre, +1 at its right.
    // The result is a unit vector and always points upward, so the ball can
    // never leave through the paddle.
    inline Vec2 paddle_bounce_direction(float offset, float max_angle)
    {
        // The ball's centre can be slightly past the edge when it clips a
        // corner, which would otherwise exceed the maximum angle.
        const float clamped = std::clamp(offset, -1.0f, 1.0f);
        const float angle = clamped * max_angle;

        return Vec2{std::sin(angle), std::cos(angle)};
    }
}
