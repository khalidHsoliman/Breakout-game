#pragma once

#include "game/Input.h"
#include "game/World.h"

namespace game
{
    void paddle_system(World& world, const Input& input, float dt);
    void movement_system(World& world, float dt);
    void collision_system(World& world);
    void lifecycle_system(World& world);
    void sweep_destroyed(World& world);

    // Advances the world by exactly dt. The order of the systems is
    // load-bearing, which is why it lives here rather than in main.
    void step(World& world, const Input& input, float dt);
}
