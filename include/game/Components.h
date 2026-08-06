#pragma once

#include "math/Vec2.h"

namespace game
{
    // Centre position and full size, matching Renderer::draw_quad.
    struct Transform
    {
        math::Vec2 position;
        math::Vec2 size;
    };

    // Tag: draw this entity as a circle rather than a rectangle, using half of
    // the transform's width as the radius. Named CircleShape rather than Circle
    // so it does not read like the collision shape in math/.
    struct CircleShape {};
}
