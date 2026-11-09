#pragma once

#include <vector>

#include "core/Entity.h"
#include "game/Level.h"
#include "game/World.h"

namespace game
{
    // Public so the level check can work out how much room the bricks have.
    inline constexpr float PADDLE_WIDTH = 120.0f;
    inline constexpr float PADDLE_HEIGHT = 20.0f;
    inline constexpr float PADDLE_Y = 40.0f;
    inline constexpr float PADDLE_SPEED = 520.0f;

    inline constexpr float BALL_RADIUS = 8.0f;
    inline constexpr float BALL_SPEED = 350.0f;
    inline constexpr float BALL_START_Y = 120.0f;

    // Colour carries the remaining toughness, so it has to be re-applied when a
    // brick is damaged - not only when it is spawned.
    core::Color brick_color(int hit_points);

    core::Entity spawn_paddle(World& world);
    core::Entity spawn_ball(World& world);

    // Turns the grid into brick entities. Geometry is derived from the world
    // size and the grid dimensions, so the same level fits any play area.
    void spawn_level(World& world, const Level& level);

    // particles thrown from a brick that just died, in the colour it was.
    void spawn_brick_particles(World& world, math::Vec2 center, math::Vec2 size, core::Color color);

    // A dot dropped where the ball just was.
    void spawn_ball_trail(World& world, math::Vec2 position, core::Color color);

    // Gives a held ball its starting velocity.
    void launch_ball(World& world, core::Entity ball);

    // Destroys every entity, resets score, lives and state, and rebuilds from
    // the first level. Used both to start and to restart. Taken by value, so a
    // restart can pass world.levels back in.
    void start_game(World& world, std::vector<Level> levels);

    // A game with a single level.
    void start_game(World& world, const Level& level);

    // Destroys every entity and rebuilds from levels[level_index], ready to
    // launch. Score and lives are kept; so are this step's events, so the hit
    // on the last brick of a level is still heard.
    void start_level(World& world);
}
