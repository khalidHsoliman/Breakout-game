#pragma once

#include "game/Input.h"
#include "game/World.h"

namespace game
{
    void paddle_system(World& world, const Input& input, float dt);
    void serve_system(World& world, const Input& input);
    void movement_system(World& world, float dt);
    void collision_system(World& world);
    void lifecycle_system(World& world);
    void win_system(World& world);
    void sweep_destroyed(World& world);

    // Pause, resume and restart. Runs in every state, since it is what gets
    // you out of the ones where nothing else does.
    void transition_system(World& world, const Input& input);

    // Advances the world by exactly dt. The order of the systems is
    // load-bearing, which is why it lives here rather than in main.
    void step(World& world, const Input& input, float dt);
}
