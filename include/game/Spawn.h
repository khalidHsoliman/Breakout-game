#pragma once

#include "core/Entity.h"
#include "game/Level.h"
#include "game/World.h"

namespace game
{
    // Colour carries the remaining toughness, so it has to be re-applied when a
    // brick is damaged - not only when it is spawned.
    core::Color brick_color(int hit_points);

    core::Entity spawn_paddle(World& world);
    core::Entity spawn_ball(World& world);

    // Turns the grid into brick entities. Geometry is derived from the world
    // size and the grid dimensions, so the same level fits any play area.
    void spawn_level(World& world, const Level& level);

    // Puts the ball back above the paddle at its starting speed.
    void reset_ball(World& world, core::Entity ball);
}
