#include <gtest/gtest.h>

#include "game/Spawn.h"
#include "game/Systems.h"

namespace
{
    constexpr float dt = 1.0f / 60.0f;

    constexpr const char* one_brick = R"(
1
)";

    game::World started_world()
    {
        game::World world;
        world.size = math::Vec2{800.0f, 600.0f};
        game::start_game(world, game::parse_level(one_brick));
        return world;
    }

    game::Input launch_input()
    {
        game::Input input;
        input.launch = true;
        return input;
    }

    game::Input pause_input()
    {
        game::Input input;
        input.toggle_pause = true;
        return input;
    }
}

TEST(StateMachine, StartsReady)
{
    const game::World world = started_world();

    EXPECT_EQ(world.state, game::GameState::Ready);
}

// Moving the paddle while held is how the player aims the shot.
TEST(StateMachine, TheHeldBallFollowsThePaddle)
{
    game::World world = started_world();

    game::Input input;
    input.move_right = true;
    for (int i = 0; i < 30; ++i)
    {
        game::step(world, input, dt);
    }

    const core::Entity ball = world.balls.entities()[0];
    const core::Entity paddle = world.paddles.entities()[0];

    EXPECT_FLOAT_EQ(world.transforms.find(ball)->position.x,
                    world.transforms.find(paddle)->position.x);
    EXPECT_GT(world.transforms.find(paddle)->position.x, 400.0f);
}

TEST(StateMachine, LaunchingStartsPlay)
{
    game::World world = started_world();

    game::step(world, launch_input(), dt);

    EXPECT_EQ(world.state, game::GameState::Playing);
    EXPECT_GT(world.velocities.find(world.balls.entities()[0])->value.y, 0.0f);
}

TEST(StateMachine, NothingMovesWhilePaused)
{
    game::World world = started_world();
    game::step(world, launch_input(), dt);

    const math::Vec2 before = world.transforms.find(world.balls.entities()[0])->position;
    game::step(world, pause_input(), dt);
    ASSERT_EQ(world.state, game::GameState::Paused);

    for (int i = 0; i < 60; ++i)
    {
        game::step(world, game::Input{}, dt);
    }

    const math::Vec2 after = world.transforms.find(world.balls.entities()[0])->position;
    EXPECT_FLOAT_EQ(before.x, after.x);
    EXPECT_FLOAT_EQ(before.y, after.y);
}

TEST(StateMachine, PauseTogglesBackToPlaying)
{
    game::World world = started_world();
    game::step(world, launch_input(), dt);

    game::step(world, pause_input(), dt);
    ASSERT_EQ(world.state, game::GameState::Paused);

    game::step(world, pause_input(), dt);
    EXPECT_EQ(world.state, game::GameState::Playing);
}

// The bug the input latch exists to prevent: one press must not toggle twice
// just because the frame ran two steps.
TEST(StateMachine, PauseAppliedTwiceInOneFrameWouldToggleTwice)
{
    game::World world = started_world();
    game::step(world, launch_input(), dt);

    const game::Input held = pause_input();
    game::step(world, held, dt);
    game::step(world, held, dt);

    // Back where it started - which is why main clears one-shots after the
    // first step that consumes them.
    EXPECT_EQ(world.state, game::GameState::Playing);
}

TEST(StateMachine, ClearingOneShotsMakesTheSecondStepHarmless)
{
    game::World world = started_world();
    game::step(world, launch_input(), dt);

    game::Input input = pause_input();
    game::step(world, input, dt);
    input.clear_one_shots();
    game::step(world, input, dt);

    EXPECT_EQ(world.state, game::GameState::Paused);
}

TEST(StateMachine, ClearingTheLastBrickWins)
{
    game::World world = started_world();
    game::step(world, launch_input(), dt);

    for (int i = 0; i < 600 && world.state == game::GameState::Playing; ++i)
    {
        game::step(world, game::Input{}, dt);
    }

    EXPECT_EQ(world.state, game::GameState::Won);
    EXPECT_EQ(world.bricks.size(), 0u);
}

TEST(StateMachine, RestartFromGameOverRebuildsTheLevel)
{
    game::World world = started_world();
    world.state = game::GameState::GameOver;
    world.lives = 0;
    world.score = 250;

    game::Input input;
    input.restart = true;
    game::step(world, input, dt);

    EXPECT_EQ(world.state, game::GameState::Ready);
    EXPECT_EQ(world.lives, game::starting_lives);
    EXPECT_EQ(world.score, 0);
    EXPECT_EQ(world.bricks.size(), 1u);
}

TEST(StateMachine, RestartDoesNothingWhilePlaying)
{
    game::World world = started_world();
    game::step(world, launch_input(), dt);
    world.score = 250;

    game::Input input;
    input.restart = true;
    game::step(world, input, dt);

    EXPECT_EQ(world.state, game::GameState::Playing);
    EXPECT_EQ(world.score, 250);
}
