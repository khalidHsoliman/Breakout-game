#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <random>
#include <string>
#include <vector>

#include "game/LevelLayout.h"
#include "game/Spawn.h"
#include "game/Systems.h"

namespace
{
    constexpr float DT = game::FIXED_DT;

    // STUCK_BALL_TIME in steps. 600 steps of 1/60 add up to just over 10 in
    // float, so the ball goes on the 600th.
    constexpr int STUCK_STEPS = 600;

    // Steel at the top left and a brick at the top right, both far above
    // y 300, where the tests keep their balls.
    constexpr const char* STEEL_LEVEL = "#.......1.";
    constexpr const char* PLAIN_LEVEL = "........1.";

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

    void run_steps(game::World& world, int steps)
    {
        for (int i = 0; i < steps; i++)
        {
            game::step(world, game::Input{}, DT);
        }
    }

    // Started and launched, with the ball put where it touches nothing: flat
    // across the middle, from wall to wall for ever.
    game::World flat_ball_world(const char* level, float y = 300.0f)
    {
        game::World world;
        world.size = game::WORLD_SIZE;
        world.rng.seed(1);
        game::start_game(world, game::parse_level(level));
        game::step(world, launch_input(), DT);

        const core::Entity ball = world.balls.entities()[0];
        world.transforms.find(ball)->position = math::Vec2{400.0f, y};
        world.velocities.find(ball)->value = math::Vec2{game::BALL_SPEED, 0.0f};
        return world;
    }

    core::Entity only_ball(const game::World& world)
    {
        return world.balls.entities()[0];
    }

    float clock_of(const game::World& world, core::Entity ball)
    {
        return world.balls.find(ball)->time_since_touch;
    }

    // A second ball, flat at y, with its own clock from zero.
    core::Entity add_flat_ball(game::World& world, float y)
    {
        const core::Entity ball = game::spawn_ball(world);
        world.transforms.find(ball)->position = math::Vec2{200.0f, y};
        world.velocities.find(ball)->value = math::Vec2{-game::BALL_SPEED, 0.0f};
        return ball;
    }

    // ---- the corridor between two rows of steel, where balls got stuck

    constexpr const char* CORRIDOR = "##########\n"
                                     "..........\n"
                                     "####.#####\n"
                                     "........1.\n";

    // Below the lowest row of bricks: out of the corridor by itself.
    constexpr float BELOW_THE_BRICKS = 440.0f;

    struct Start
    {
        math::Vec2 position;
        math::Vec2 velocity;
    };

    // The 11 that were still in the corridor after 3 minutes when the ball
    // starts in the middle of a cell of the middle row, going up and to the
    // right at 5 to 60 degrees from vertical: {column, degrees}.
    std::vector<Start> user_starts()
    {
        const int stuck[][2] = {{2, 11}, {6, 12}, {6, 21}, {6, 24}, {7, 9}, {7, 35},
                                {7, 44}, {9, 7},  {9, 12}, {9, 44}, {9, 55}};
        const game::Level level = game::parse_level(CORRIDOR);

        std::vector<Start> starts;
        for (const auto& [column, degrees] : stuck)
        {
            const float angle = static_cast<float>(degrees) * 3.14159265f / 180.0f;
            starts.push_back({game::brick_center(level, game::WORLD_SIZE, column, 1),
                              math::Vec2{std::sin(angle), std::cos(angle)} * game::BALL_SPEED});
        }
        return starts;
    }

    // The 16 of 560 that were still in after 3 minutes in a sweep of 20 spots
    // across the corridor by 28 directions all the way round:
    // {spot, direction}.
    std::vector<Start> sweep_starts()
    {
        const int stuck[][2] = {{0, 10},  {1, 26},  {2, 1},   {3, 17},  {6, 20},  {9, 1},
                                {11, 2},  {11, 23}, {12, 5},  {12, 6},  {12, 16}, {12, 20},
                                {18, 9},  {18, 12}, {18, 20}, {19, 15}};

        std::vector<Start> starts;
        for (const auto& [spot, direction] : stuck)
        {
            const float x = 30.0f + static_cast<float>(spot) * (740.0f / 19.0f);
            const float y = 511.0f + static_cast<float>(spot % 4) * 4.5f;
            const float degrees = 3.0f + static_cast<float>(direction) * (360.0f / 28.0f);
            const float angle = degrees * 3.14159265f / 180.0f;
            starts.push_back({math::Vec2{x, y}, math::Vec2{std::cos(angle), std::sin(angle)} * game::BALL_SPEED});
        }
        return starts;
    }

    // Each start stays in the corridor until the ball has gone 10 seconds
    // without a touch, then is on the paddle, ready to serve, with no life lost.
    void expect_back_on_the_paddle_after_ten_seconds(const std::vector<Start>& starts)
    {
        for (std::size_t i = 0; i < starts.size(); i++)
        {
            SCOPED_TRACE("start " + std::to_string(i));

            game::World world;
            world.size = game::WORLD_SIZE;
            game::start_game(world, game::parse_level(CORRIDOR));
            world.state = game::GameState::Playing;

            const core::Entity ball = only_ball(world);
            world.transforms.find(ball)->position = starts[i].position;
            world.velocities.find(ball)->value = starts[i].velocity;

            for (int step = 1; step < STUCK_STEPS; step++)
            {
                game::step(world, game::Input{}, DT);
                ASSERT_EQ(world.state, game::GameState::Playing) << "step " << step;
                ASSERT_GT(world.transforms.find(ball)->position.y, BELOW_THE_BRICKS)
                    << "came out by itself at step " << step << ", so it was not stuck";
            }

            game::step(world, game::Input{}, DT);
            ASSERT_EQ(world.state, game::GameState::Ready) << "not back after 10 seconds";
            EXPECT_EQ(world.lives, game::STARTING_LIVES);
            ASSERT_EQ(world.balls.size(), 1u);
            EXPECT_EQ(world.velocities.find(ball)->value.x, 0.0f);
            EXPECT_EQ(world.velocities.find(ball)->value.y, 0.0f);

            // The next step parks it on the paddle, like any serve.
            game::step(world, game::Input{}, DT);
            const game::Transform& paddle = *world.transforms.find(world.paddles.entities()[0]);
            EXPECT_FLOAT_EQ(world.transforms.find(ball)->position.x, paddle.position.x);
            EXPECT_GT(world.transforms.find(ball)->position.y, paddle.position.y);
            EXPECT_LT(world.transforms.find(ball)->position.y, paddle.position.y + 30.0f);
        }
    }
}

// ---------------------------------------------------------------- the corridor

TEST(StuckBall, TheElevenStuckStartsInTheCorridorAreBackOnThePaddleInTenSeconds)
{
    expect_back_on_the_paddle_after_ten_seconds(user_starts());
}

TEST(StuckBall, TheSixteenStuckStartsOfTheSweepAreBackOnThePaddleInTenSeconds)
{
    expect_back_on_the_paddle_after_ten_seconds(sweep_starts());
}

// ---------------------------------------------------------------- the rule

TEST(StuckBall, TheOnlyBallGoesBackOntoThePaddleWithNoLifeLost)
{
    game::World world = flat_ball_world(STEEL_LEVEL);
    const core::Entity ball = only_ball(world);
    world.score = 70;

    run_steps(world, STUCK_STEPS - 1);
    EXPECT_EQ(world.state, game::GameState::Playing) << "gone before 10 seconds";

    game::step(world, game::Input{}, DT);
    EXPECT_EQ(world.state, game::GameState::Ready);
    EXPECT_EQ(world.lives, game::STARTING_LIVES);
    EXPECT_EQ(world.score, 70);
    EXPECT_EQ(world.balls.size(), 1u);
    EXPECT_TRUE(world.balls.has(ball));

    // Served again like any held ball.
    game::step(world, launch_input(), DT);
    EXPECT_EQ(world.state, game::GameState::Playing);
    EXPECT_GT(world.velocities.find(ball)->value.y, 0.0f);
}

// What losing the last ball does, apart from the life.
TEST(StuckBall, GoingBackClearsCapsulesAndTheWidePaddle)
{
    game::World world = flat_ball_world(STEEL_LEVEL);
    run_steps(world, STUCK_STEPS - 2);

    const core::Entity paddle = world.paddles.entities()[0];
    world.transforms.find(paddle)->size.x = game::WIDE_PADDLE_WIDTH;
    world.paddles.find(paddle)->wide_time_left = game::WIDE_PADDLE_TIME;
    game::spawn_capsule(world, math::Vec2{700.0f, 400.0f}, game::PowerUp::MultiBall);

    run_steps(world, 2);

    ASSERT_EQ(world.state, game::GameState::Ready);
    EXPECT_EQ(world.capsules.size(), 0u);
    EXPECT_EQ(world.particles.size(), 0u);
    EXPECT_EQ(world.transforms.find(paddle)->size.x, game::PADDLE_WIDTH);
    EXPECT_EQ(world.paddles.find(paddle)->wide_time_left, 0.0f);
}

TEST(StuckBall, WithAnotherBallInPlayItIsJustRemoved)
{
    game::World world = flat_ball_world(STEEL_LEVEL);
    const core::Entity first = only_ball(world);

    run_steps(world, STUCK_STEPS / 2);
    const core::Entity second = add_flat_ball(world, 200.0f);

    run_steps(world, STUCK_STEPS / 2);
    EXPECT_EQ(world.state, game::GameState::Playing);
    EXPECT_FALSE(world.balls.has(first));
    EXPECT_TRUE(world.balls.has(second));
    EXPECT_EQ(world.lives, game::STARTING_LIVES);

    // Each ball keeps its own time: the second goes 10 seconds after it came.
    run_steps(world, STUCK_STEPS / 2 - 1);
    EXPECT_EQ(world.state, game::GameState::Playing);
    game::step(world, game::Input{}, DT);
    EXPECT_EQ(world.state, game::GameState::Ready);
    EXPECT_TRUE(world.balls.has(second));
    EXPECT_EQ(world.lives, game::STARTING_LIVES);
}

TEST(StuckBall, SeveralStuckAtOnceWithNoOtherBallLeaveOneOnThePaddle)
{
    game::World world = flat_ball_world(STEEL_LEVEL);
    add_flat_ball(world, 200.0f);
    add_flat_ball(world, 250.0f);

    run_steps(world, STUCK_STEPS);

    EXPECT_EQ(world.state, game::GameState::Ready);
    EXPECT_EQ(world.balls.size(), 1u);
    EXPECT_EQ(world.lives, game::STARTING_LIVES);
}

TEST(StuckBall, ThePaddleRestartsTheClock)
{
    game::World world = flat_ball_world(STEEL_LEVEL);
    const core::Entity ball = only_ball(world);
    world.balls.find(ball)->time_since_touch = 9.5f;

    // Just above the paddle, coming down onto it.
    const math::Vec2 paddle = world.transforms.find(world.paddles.entities()[0])->position;
    world.transforms.find(ball)->position = math::Vec2{paddle.x, paddle.y + 20.0f};
    world.velocities.find(ball)->value = math::Vec2{0.0f, -game::BALL_SPEED};
    game::step(world, game::Input{}, DT);

    ASSERT_GT(world.velocities.find(ball)->value.y, 0.0f) << "did not bounce off the paddle";
    EXPECT_LE(clock_of(world, ball), DT);
}

TEST(StuckBall, ABreakableBrickRestartsTheClockEvenWhenItDoesNotBreak)
{
    game::World world = flat_ball_world(STEEL_LEVEL);
    const core::Entity ball = only_ball(world);
    world.balls.find(ball)->time_since_touch = 9.5f;

    const core::Entity brick = world.bricks.entities()[0];
    world.bricks.find(brick)->hit_points = 2;
    const math::Vec2 at = world.transforms.find(brick)->position;
    world.transforms.find(ball)->position = math::Vec2{at.x, at.y - 22.0f};
    world.velocities.find(ball)->value = math::Vec2{0.0f, game::BALL_SPEED};
    game::step(world, game::Input{}, DT);

    ASSERT_EQ(world.bricks.find(brick)->hit_points, 1) << "did not hit the brick";
    EXPECT_LE(clock_of(world, ball), DT);
}

TEST(StuckBall, SteelAndWallsDoNotRestartTheClock)
{
    game::World world = flat_ball_world(STEEL_LEVEL);
    const core::Entity ball = only_ball(world);

    // Under the steel, going up into it.
    world.balls.find(ball)->time_since_touch = 5.0f;
    const math::Vec2 steel = world.transforms.find(world.steel.entities()[0])->position;
    world.transforms.find(ball)->position = math::Vec2{steel.x, steel.y - 22.0f};
    world.velocities.find(ball)->value = math::Vec2{0.0f, game::BALL_SPEED};
    world.events.clear();
    game::step(world, game::Input{}, DT);
    ASSERT_EQ(world.events, (std::vector<game::GameEvent>{game::GameEvent::SteelHit}));
    EXPECT_FLOAT_EQ(clock_of(world, ball), 5.0f + DT);

    // At the left wall, going into it.
    world.transforms.find(ball)->position = math::Vec2{10.0f, 300.0f};
    world.velocities.find(ball)->value = math::Vec2{-game::BALL_SPEED, 0.0f};
    game::step(world, game::Input{}, DT);
    ASSERT_GT(world.velocities.find(ball)->value.x, 0.0f) << "did not bounce off the wall";
    EXPECT_FLOAT_EQ(clock_of(world, ball), 5.0f + 2.0f * DT);
}

TEST(StuckBall, TheClockStandsStillWhilePaused)
{
    game::World world = flat_ball_world(STEEL_LEVEL);
    const core::Entity ball = only_ball(world);
    run_steps(world, STUCK_STEPS - 10);
    const float before = clock_of(world, ball);
    ASSERT_GT(before, 9.0f) << "the clock did not run before the pause";

    game::step(world, pause_input(), DT);
    ASSERT_EQ(world.state, game::GameState::Paused);
    run_steps(world, 10 * STUCK_STEPS);
    EXPECT_EQ(clock_of(world, ball), before);

    game::step(world, pause_input(), DT);
    EXPECT_EQ(world.state, game::GameState::Playing) << "taken out of play by the pause";
}

// Held on the paddle counts as a touch, so a ball goes 10 seconds from its
// launch, however long it was in play before it went back.
TEST(StuckBall, BeingHeldCountsAsATouch)
{
    game::World world = flat_ball_world(STEEL_LEVEL);
    const core::Entity ball = only_ball(world);
    run_steps(world, STUCK_STEPS);
    ASSERT_EQ(world.state, game::GameState::Ready);

    world.balls.find(ball)->time_since_touch = 9.9f;
    run_steps(world, 120);   // held for two seconds
    EXPECT_EQ(clock_of(world, ball), 0.0f);

    game::step(world, launch_input(), DT);
    world.transforms.find(ball)->position = math::Vec2{400.0f, 300.0f};
    world.velocities.find(ball)->value = math::Vec2{game::BALL_SPEED, 0.0f};
    run_steps(world, STUCK_STEPS - 1);
    EXPECT_EQ(world.state, game::GameState::Playing);
    game::step(world, game::Input{}, DT);
    EXPECT_EQ(world.state, game::GameState::Ready);
}

TEST(StuckBall, NewMultiBallBallsStartAtZero)
{
    game::World world = flat_ball_world(STEEL_LEVEL);
    const core::Entity source = only_ball(world);
    run_steps(world, STUCK_STEPS - 60);

    const math::Vec2 paddle = world.transforms.find(world.paddles.entities()[0])->position;
    game::spawn_capsule(world, paddle, game::PowerUp::MultiBall);
    game::step(world, game::Input{}, DT);
    ASSERT_EQ(world.balls.size(), 3u);

    for (const core::Entity ball : world.balls.entities())
    {
        if (ball != source)
        {
            EXPECT_LE(clock_of(world, ball), DT);
        }
    }

    // A second later the source has had its 10 seconds and the new ones have not.
    run_steps(world, 59);
    EXPECT_FALSE(world.balls.has(source));
    EXPECT_EQ(world.balls.size(), 2u);
    EXPECT_EQ(world.state, game::GameState::Playing);
}

TEST(StuckBall, DrawsNothingFromTheGenerator)
{
    game::World world = flat_ball_world(STEEL_LEVEL);
    world.capsule_drop_chance = 1.0f;
    const std::mt19937 before = world.rng;

    run_steps(world, STUCK_STEPS + 60);

    ASSERT_EQ(world.state, game::GameState::Ready);
    EXPECT_TRUE(world.rng == before);
}

// Without steel nothing is taken out of play, and no clock even runs.
TEST(StuckBall, ALevelWithoutSteelNeverTakesABallOutOfPlay)
{
    game::World world = flat_ball_world(PLAIN_LEVEL);
    const core::Entity ball = only_ball(world);

    run_steps(world, 3 * STUCK_STEPS);

    EXPECT_EQ(world.state, game::GameState::Playing);
    EXPECT_TRUE(world.balls.has(ball));
    EXPECT_EQ(std::abs(world.velocities.find(ball)->value.x), game::BALL_SPEED);
    EXPECT_EQ(clock_of(world, ball), 0.0f);
}
