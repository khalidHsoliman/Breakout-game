#pragma once

#include "math/Vec2.h"

namespace game
{
    // Centre position and full size, matching Renderer::draw_quad.
    struct Transform
    {
        math::Vec2 position;
        math::Vec2 size;

        // Where it was when the step began. Rendering draws between this and
        // position, so motion stays smooth on a display refreshing faster than
        // the game steps.
        math::Vec2 previous_position;
    };

    // Tag: draw this entity as a circle rather than a rectangle, using half of
    // the transform's width as the radius. Named CircleShape rather than Circle
    // so it does not read like the collision shape in math/.
    struct CircleShape {};

    struct Velocity
    {
        math::Vec2 value;
    };

    struct Ball
    {
        float radius = 0.0f;
    };

    struct Paddle
    {
        float speed = 0.0f;
    };

    struct Brick
    {
        int hit_points = 1;
    };

    struct Particle
    {
        float lifetime = 0.0f;
        float max_life = 1.0f;
    };

    // Tag: marked for removal at the end of the step. Systems must skip these,
    // or a dead brick still collides for the rest of the frame.
    struct Destroyed {};
}
