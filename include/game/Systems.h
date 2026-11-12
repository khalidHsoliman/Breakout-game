#pragma once

#include "game/Input.h"
#include "game/LevelLayout.h"
#include "game/World.h"

namespace game
{

    // Every brick hit makes the ball slightly faster.
    inline constexpr float BALL_SPEED_PER_HIT = 4.0f;
    inline constexpr float FIXED_DT = 1.0f / 60.0f;
    inline constexpr float MAX_BALL_SPEED = 0.5f * BRICK_HEIGHT / FIXED_DT;

    // Gap between the paddle's top and a held ball, so they do not collide
    // the instant it launches.
    inline constexpr float SERVE_GAP = 2.0f;

    // Steel can trap a ball where it bounces for ever. In a level with steel,
    // a ball that goes this long without touching the paddle or a breakable
    // brick is taken out of play.
    inline constexpr float STUCK_BALL_TIME = 10.0f;

    // The ball cannot cross a brick in a single step.
    static_assert(
        MAX_BALL_SPEED * FIXED_DT < BRICK_HEIGHT,
        "the ball can cross a brick in a single step and pass through it"
    );

    void paddle_system(World& world, const Input& input, float dt);
    void serve_system(World& world, const Input& input);
    void movement_system(World& world, float dt);
    void collision_system(World& world);
    void lifecycle_system(World& world);
    void trail_system(World& world);
    void particle_system(World& world, float dt);

    // Runs down the wide paddle's time, gives a caught capsule's power-up to
    // the paddle, and removes capsules that are caught or fall past the floor.
    // Playing only, so the time stands still while paused.
    void power_up_system(World& world, float dt);

    // In a level with steel only, so a level without any plays exactly as
    // before. Runs each ball's clock, and takes a ball that has gone
    // STUCK_BALL_TIME without a touch out of play: removed if another ball is
    // still in play, otherwise back onto the paddle to be served again, as
    // after a lost life but with no life lost.
    void stuck_ball_system(World& world, float dt);

    // Clearing a level moves on to the next, keeping score and lives; clearing
    // the last one wins.
    void win_system(World& world);

    void sweep_destroyed(World& world);

    // Pause, resume and restart. Runs in every state, since it is what gets
    // you out of the ones where nothing else does.
    void transition_system(World& world, const Input& input);

    // Advances the world by exactly dt. The order of the systems is
    // load-bearing, which is why it lives here rather than in main.
    void step(World& world, const Input& input, float dt);
}
