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

    struct Velocity
    {
        math::Vec2 value;
    };

    struct Ball
    {
        float radius = 0.0f;

        // Seconds since it last touched the paddle or a breakable brick, or
        // was held on the paddle. Steel and walls do not count.
        float time_since_touch = 0.0f;
    };

    struct Paddle
    {
        float speed = 0.0f;

        // Seconds of wide paddle left; 0 when it is its normal width.
        float wide_time_left = 0.0f;
    };

    struct Brick
    {
        int hit_points = 1;
    };

    // Tag: a brick that cannot be broken. The ball bounces off it, but it is
    // never damaged, scores nothing and drops nothing. It is not a Brick, so a
    // level is cleared when the last Brick goes, however much steel is left.
    struct Steel {};

    enum class PowerUp
    {
        WidePaddle,
        MultiBall
    };

    // Dropped by a destroyed brick, falls straight down, and gives its power-up
    // to the paddle that catches it.
    struct Capsule
    {
        PowerUp kind = PowerUp::WidePaddle;
    };

    struct Particle
    {
        float lifetime = 0.0f;
        float max_life = 1.0f;
    };

    // Tag: draw this entity as a circle rather than a rectangle, using half of
    // the transform's width as the radius.
    struct CircleShape {};

    // Tag: marked for removal at the end of the step.
    struct Destroyed {};
}
