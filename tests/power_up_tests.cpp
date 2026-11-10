#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "game/Spawn.h"
#include "game/Systems.h"

namespace
{
    constexpr float DT = 1.0f / 60.0f;
    constexpr float TOLERANCE = 1e-3f;

    // Where the test brick sits, and where a ball just under it heading up
    // starts, so one collision_system call is one hit on it.
    constexpr math::Vec2 BRICK_AT{400.0f, 500.0f};
    constexpr math::Vec2 BALL_UNDER_BRICK{400.0f, 490.0f};

    // Well away from the paddle, so nothing there is caught.
    constexpr float CAPSULE_LANE_X = 700.0f;

    game::World make_world(std::uint32_t seed = 1)
    {
        game::World world;
        world.size = math::Vec2{800.0f, 600.0f};
        world.state = game::GameState::Playing;
        world.rng.seed(seed);
        return world;
    }

    core::Entity add_brick(game::World& world, math::Vec2 position, int hit_points = 1)
    {
        const core::Entity entity = world.create_entity();
        world.transforms.add(entity, game::Transform{position, math::Vec2{60.0f, 24.0f}, position});
        world.colors.add(entity, game::brick_color(hit_points));
        world.bricks.add(entity, game::Brick{hit_points});
        return entity;
    }

    core::Entity add_ball(game::World& world, math::Vec2 position, math::Vec2 velocity)
    {
        const core::Entity entity = world.create_entity();
        world.transforms.add(entity, game::Transform{position, math::Vec2{16.0f, 16.0f}, position});
        world.velocities.add(entity, game::Velocity{velocity});
        world.balls.add(entity, game::Ball{8.0f});
        return entity;
    }

    // One hit from below on the brick at BRICK_AT, by a ball put back under
    // it first.
    void hit_brick(game::World& world, core::Entity ball)
    {
        world.transforms.find(ball)->position = BALL_UNDER_BRICK;
        world.velocities.find(ball)->value = math::Vec2{0.0f, 200.0f};
        game::collision_system(world);
    }

    // Puts a one-hit brick at BRICK_AT, breaks it, and sweeps it away.
    void break_a_brick(game::World& world, core::Entity ball)
    {
        add_brick(world, BRICK_AT, 1);
        hit_brick(world, ball);
        game::sweep_destroyed(world);
    }

    // Playing, with the real paddle, and a tough brick up in the corner so the
    // level is never cleared while a test steps it.
    game::World playing_world()
    {
        game::World world = make_world();
        game::spawn_paddle(world);
        add_brick(world, math::Vec2{100.0f, 560.0f}, 9);
        return world;
    }

    core::Entity paddle_of(const game::World& world)
    {
        return world.paddles.entities()[0];
    }

    float paddle_width(const game::World& world)
    {
        return world.transforms.find(paddle_of(world))->size.x;
    }

    // A capsule dropped right onto the paddle, and one step to catch it.
    void catch_capsule(game::World& world, game::PowerUp kind)
    {
        const math::Vec2 paddle = world.transforms.find(paddle_of(world))->position;
        game::spawn_capsule(world, paddle, kind);
        game::step(world, game::Input{}, DT);
    }

    void run_steps(game::World& world, int steps)
    {
        for (int i = 0; i < steps; i++)
        {
            game::step(world, game::Input{}, DT);
        }
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

    game::World started_world(std::vector<game::Level> levels)
    {
        game::World world;
        world.size = math::Vec2{800.0f, 600.0f};
        world.rng.seed(1);
        game::start_game(world, std::move(levels));
        game::step(world, launch_input(), DT);
        return world;
    }

    // Removes every brick, so the next step sees a cleared level.
    void clear_level(game::World& world)
    {
        const std::vector<core::Entity> bricks = world.bricks.entities();
        for (const core::Entity brick : bricks)
        {
            world.destroy_entity(brick);
        }
        game::step(world, game::Input{}, DT);
    }

    std::vector<game::PowerUp> kinds_dropped(const game::World& world)
    {
        std::vector<game::PowerUp> kinds;
        for (const game::Capsule& capsule : world.capsules.components())
        {
            kinds.push_back(capsule.kind);
        }
        return kinds;
    }

    float distance(core::Color a, core::Color b)
    {
        return std::sqrt((a.r - b.r) * (a.r - b.r) + (a.g - b.g) * (a.g - b.g) + (a.b - b.b) * (a.b - b.b));
    }
}

// ---------------------------------------------------------------- drops

TEST(CapsuleDrop, ADestroyedBrickCanDropACapsuleWhereItWas)
{
    game::World world = make_world();
    world.capsule_drop_chance = 1.0f;
    add_brick(world, BRICK_AT, 1);
    const core::Entity ball = add_ball(world, BALL_UNDER_BRICK, math::Vec2{});

    hit_brick(world, ball);

    ASSERT_EQ(world.capsules.size(), 1u);
    const game::Transform* capsule = world.transforms.find(world.capsules.entities()[0]);
    ASSERT_NE(capsule, nullptr);
    EXPECT_FLOAT_EQ(capsule->position.x, BRICK_AT.x);
    EXPECT_FLOAT_EQ(capsule->position.y, BRICK_AT.y);
}

TEST(CapsuleDrop, WithNoChanceNoBrickDrops)
{
    game::World world = make_world();
    world.capsule_drop_chance = 0.0f;
    const core::Entity ball = add_ball(world, BALL_UNDER_BRICK, math::Vec2{});

    for (int i = 0; i < 200; i++)
    {
        break_a_brick(world, ball);
    }

    EXPECT_EQ(world.capsules.size(), 0u);
}

// Any brick can drop, however tough, but only once it is destroyed.
TEST(CapsuleDrop, OnlyTheHitThatDestroysABrickDrops)
{
    for (const int hit_points : {1, 2, 4, 9})
    {
        game::World world = make_world();
        world.capsule_drop_chance = 1.0f;
        const core::Entity brick = add_brick(world, BRICK_AT, hit_points);
        const core::Entity ball = add_ball(world, BALL_UNDER_BRICK, math::Vec2{});

        for (int hit = 1; hit < hit_points; hit++)
        {
            hit_brick(world, ball);
            ASSERT_TRUE(game::is_alive(world, brick));
            EXPECT_EQ(world.capsules.size(), 0u) << hit_points << " hit points, hit " << hit;
        }

        hit_brick(world, ball);
        ASSERT_FALSE(game::is_alive(world, brick));
        EXPECT_EQ(world.capsules.size(), 1u) << hit_points << " hit points";
    }
}

// 15% of bricks, half of them each kind. The seed is fixed and only the
// engine's raw output is used, so this is the same count on every compiler.
TEST(CapsuleDrop, AboutOneBrickInSevenDropsAndTheKindsAreEven)
{
    ASSERT_FLOAT_EQ(game::CAPSULE_DROP_CHANCE, 0.15f);

    game::World world = make_world(12345);
    ASSERT_FLOAT_EQ(world.capsule_drop_chance, 0.15f);
    const core::Entity ball = add_ball(world, BALL_UNDER_BRICK, math::Vec2{});

    constexpr int BRICKS = 2000;
    for (int i = 0; i < BRICKS; i++)
    {
        break_a_brick(world, ball);
    }

    int wide = 0;
    int multi = 0;
    for (const game::PowerUp kind : kinds_dropped(world))
    {
        (kind == game::PowerUp::WidePaddle ? wide : multi) += 1;
    }

    const int drops = wide + multi;
    EXPECT_GE(drops, 250);   // 300 expected
    EXPECT_LE(drops, 350);
    EXPECT_GE(wide, drops * 2 / 5);
    EXPECT_GE(multi, drops * 2 / 5);
}

TEST(CapsuleDrop, TheSameSeedDropsTheSameCapsules)
{
    auto run = []()
    {
        game::World world = make_world(777);
        const core::Entity ball = add_ball(world, BALL_UNDER_BRICK, math::Vec2{});

        // Which bricks dropped, and what.
        std::vector<int> drops;
        for (int i = 0; i < 300; i++)
        {
            const std::size_t before = world.capsules.size();
            break_a_brick(world, ball);
            if (world.capsules.size() != before)
            {
                const core::Entity capsule = world.capsules.entities().back();
                drops.push_back(i * 2 + static_cast<int>(world.capsules.find(capsule)->kind));
            }
        }
        return drops;
    };

    const std::vector<int> first = run();
    const std::vector<int> second = run();

    EXPECT_FALSE(first.empty());
    EXPECT_EQ(first, second);
}

// R keeps the same generator: restarting neither reseeds it nor starts it over.
TEST(CapsuleDrop, ARestartCarriesOnWithTheSameGenerator)
{
    auto drops_after = [](bool restart_midway)
    {
        game::World world;
        world.size = math::Vec2{800.0f, 600.0f};
        world.rng.seed(4242);
        game::start_game(world, game::parse_level("1"));
        world.state = game::GameState::Playing;

        std::vector<game::PowerUp> kinds;
        for (int i = 0; i < 200; i++)
        {
            if (restart_midway && i == 100)
            {
                game::start_game(world, world.levels);
                world.state = game::GameState::Playing;
            }

            // A fresh ball each time: a restart destroys the old one.
            const core::Entity ball = add_ball(world, BALL_UNDER_BRICK, math::Vec2{});
            const std::size_t before = world.capsules.size();
            break_a_brick(world, ball);
            world.destroy_entity(ball);

            if (world.capsules.size() != before)
            {
                kinds.push_back(world.capsules.find(world.capsules.entities().back())->kind);
            }
        }
        return kinds;
    };

    const std::vector<game::PowerUp> straight = drops_after(false);
    const std::vector<game::PowerUp> restarted = drops_after(true);

    ASSERT_FALSE(straight.empty());
    EXPECT_EQ(straight, restarted);
}

// ---------------------------------------------------------------- capsules

TEST(Capsule, IsFortyByFourteenInTheColourOfItsKind)
{
    game::World world = make_world();

    for (const game::PowerUp kind : {game::PowerUp::WidePaddle, game::PowerUp::MultiBall})
    {
        const core::Entity capsule = game::spawn_capsule(world, math::Vec2{300.0f, 400.0f}, kind);

        const game::Transform* transform = world.transforms.find(capsule);
        ASSERT_NE(transform, nullptr);
        EXPECT_FLOAT_EQ(transform->size.x, 40.0f);
        EXPECT_FLOAT_EQ(transform->size.y, 14.0f);
        EXPECT_FLOAT_EQ(transform->previous_position.y, 400.0f) << "would be drawn sliding in";

        const core::Color* color = world.colors.find(capsule);
        ASSERT_NE(color, nullptr);
        EXPECT_FLOAT_EQ(distance(*color, game::capsule_color(kind)), 0.0f);
        EXPECT_EQ(world.capsules.find(capsule)->kind, kind);
    }
}

TEST(Capsule, FallsStraightDownAtOneHundredAndFiftyUnitsASecond)
{
    game::World world = playing_world();
    const core::Entity capsule =
        game::spawn_capsule(world, math::Vec2{CAPSULE_LANE_X, 400.0f}, game::PowerUp::WidePaddle);

    run_steps(world, 60);

    const game::Transform* transform = world.transforms.find(capsule);
    ASSERT_NE(transform, nullptr);
    EXPECT_FLOAT_EQ(transform->position.x, CAPSULE_LANE_X);
    EXPECT_NEAR(transform->position.y, 400.0f - 150.0f, TOLERANCE);
}

TEST(CapsuleColor, WideIsBlueMultiBallIsGreenAndNeitherLooksLikeABrick)
{
    const core::Color wide = game::capsule_color(game::PowerUp::WidePaddle);
    const core::Color multi = game::capsule_color(game::PowerUp::MultiBall);

    EXPECT_GT(wide.b, wide.r);
    EXPECT_GT(wide.b, wide.g);
    EXPECT_GT(multi.g, multi.r);
    EXPECT_GT(multi.g, multi.b);
    EXPECT_GT(distance(wide, multi), 0.3f);

    // 5 and up share one colour, so 1 to 5 is every brick colour there is.
    for (int hit_points = 1; hit_points <= 5; hit_points++)
    {
        const core::Color brick = game::brick_color(hit_points);
        EXPECT_GT(distance(wide, brick), 0.3f) << "wide paddle and a " << hit_points << "-hit brick";
        EXPECT_GT(distance(multi, brick), 0.3f) << "multi-ball and a " << hit_points << "-hit brick";
    }
}

TEST(Capsule, AMissedCapsuleDisappearsOnceAllOfItIsBelowTheFloor)
{
    game::World world = playing_world();

    // Its top edge just above the floor: still in play.
    const core::Entity capsule =
        game::spawn_capsule(world, math::Vec2{CAPSULE_LANE_X, -6.0f}, game::PowerUp::WidePaddle);
    game::power_up_system(world, DT);
    EXPECT_TRUE(game::is_alive(world, capsule));

    // Just under it: gone, and nothing else changes.
    world.transforms.find(capsule)->position.y = -7.5f;
    game::step(world, game::Input{}, DT);

    EXPECT_FALSE(world.capsules.has(capsule));
    EXPECT_FALSE(world.transforms.has(capsule));
    EXPECT_EQ(world.lives, game::STARTING_LIVES);
    EXPECT_EQ(world.state, game::GameState::Playing);
    EXPECT_FLOAT_EQ(paddle_width(world), game::PADDLE_WIDTH);
}

TEST(Capsule, ACaughtCapsuleIsUsedUpAndScoresNothing)
{
    for (const game::PowerUp kind : {game::PowerUp::WidePaddle, game::PowerUp::MultiBall})
    {
        game::World world = playing_world();

        catch_capsule(world, kind);

        EXPECT_EQ(world.capsules.size(), 0u);
        EXPECT_EQ(world.score, 0);
        EXPECT_TRUE(world.events.empty()) << "no sound for a catch yet";
    }
}

TEST(Capsule, OneJustAboveThePaddleIsNotCaughtYet)
{
    game::World world = playing_world();
    const game::Transform paddle = *world.transforms.find(paddle_of(world));

    // Its bottom edge one unit above the paddle's top edge.
    const float y = paddle.position.y + paddle.size.y * 0.5f + game::CAPSULE_HEIGHT * 0.5f + 1.0f;
    const core::Entity capsule =
        game::spawn_capsule(world, math::Vec2{paddle.position.x, y}, game::PowerUp::WidePaddle);

    game::power_up_system(world, DT);

    EXPECT_TRUE(game::is_alive(world, capsule));
    EXPECT_FLOAT_EQ(paddle_width(world), game::PADDLE_WIDTH);
}

TEST(Capsule, BallsPassThroughCapsules)
{
    game::World world = make_world();
    const core::Entity ball = add_ball(world, math::Vec2{400.0f, 300.0f}, math::Vec2{0.0f, 200.0f});
    const core::Entity capsule =
        game::spawn_capsule(world, math::Vec2{400.0f, 305.0f}, game::PowerUp::WidePaddle);

    game::collision_system(world);

    EXPECT_FLOAT_EQ(world.velocities.find(ball)->value.y, 200.0f);
    EXPECT_TRUE(world.events.empty());
    EXPECT_TRUE(game::is_alive(world, capsule));
}

TEST(Capsule, AWinClearsThem)
{
    game::World world = started_world({game::parse_level("1")});
    const core::Entity capsule =
        game::spawn_capsule(world, math::Vec2{CAPSULE_LANE_X, 300.0f}, game::PowerUp::MultiBall);

    clear_level(world);

    ASSERT_EQ(world.state, game::GameState::Won);
    EXPECT_EQ(world.capsules.size(), 0u);
    EXPECT_FALSE(world.transforms.has(capsule));
}

TEST(Capsule, TheNextLevelClearsThemAndTheWidePaddle)
{
    game::World world = started_world({game::parse_level("1"), game::parse_level("11")});
    catch_capsule(world, game::PowerUp::WidePaddle);
    ASSERT_FLOAT_EQ(paddle_width(world), game::WIDE_PADDLE_WIDTH);
    game::spawn_capsule(world, math::Vec2{CAPSULE_LANE_X, 300.0f}, game::PowerUp::WidePaddle);

    clear_level(world);

    ASSERT_EQ(world.level_index, 1);
    EXPECT_EQ(world.capsules.size(), 0u);
    EXPECT_FLOAT_EQ(paddle_width(world), game::PADDLE_WIDTH);
    EXPECT_FLOAT_EQ(world.paddles.find(paddle_of(world))->wide_time_left, 0.0f);
}

TEST(Capsule, RestartClearsThemAndTheWidePaddle)
{
    game::World world = started_world({game::parse_level("11")});
    catch_capsule(world, game::PowerUp::WidePaddle);
    ASSERT_FLOAT_EQ(paddle_width(world), game::WIDE_PADDLE_WIDTH);
    game::spawn_capsule(world, math::Vec2{CAPSULE_LANE_X, 300.0f}, game::PowerUp::WidePaddle);
    world.state = game::GameState::GameOver;

    game::Input input;
    input.restart = true;
    game::step(world, input, DT);

    ASSERT_EQ(world.state, game::GameState::Ready);
    EXPECT_EQ(world.capsules.size(), 0u);
    EXPECT_FLOAT_EQ(paddle_width(world), game::PADDLE_WIDTH);
    EXPECT_FLOAT_EQ(world.paddles.find(paddle_of(world))->wide_time_left, 0.0f);
}

// ---------------------------------------------------------------- wide paddle

TEST(WidePaddle, CatchingItMakesThePaddleOneAndAHalfTimesWider)
{
    game::World world = playing_world();
    const game::Transform before = *world.transforms.find(paddle_of(world));

    catch_capsule(world, game::PowerUp::WidePaddle);

    const game::Transform& after = *world.transforms.find(paddle_of(world));
    EXPECT_FLOAT_EQ(after.size.x, 180.0f);
    EXPECT_FLOAT_EQ(after.size.x, 1.5f * game::PADDLE_WIDTH);
    EXPECT_FLOAT_EQ(after.size.y, before.size.y);
    EXPECT_FLOAT_EQ(after.position.x, before.position.x) << "it grows about its centre";
}

// Catching uses the width it has now. Beyond its normal half width plus half a
// capsule, only the wide paddle reaches.
TEST(WidePaddle, ItsOuterPartsCatchCapsules)
{
    const float outer = game::PADDLE_WIDTH * 0.5f + game::CAPSULE_WIDTH * 0.5f + 15.0f;

    for (const float side : {-1.0f, 1.0f})
    {
        // The same spot misses a normal paddle, so the test means something.
        game::World normal = playing_world();
        const math::Vec2 normal_paddle = normal.transforms.find(paddle_of(normal))->position;
        const core::Entity missed = game::spawn_capsule(
            normal, normal_paddle + math::Vec2{side * outer, 0.0f}, game::PowerUp::WidePaddle);
        game::step(normal, game::Input{}, DT);
        ASSERT_TRUE(normal.capsules.has(missed)) << "caught by a normal paddle";

        game::World world = playing_world();
        catch_capsule(world, game::PowerUp::WidePaddle);
        run_steps(world, 120);

        const math::Vec2 paddle = world.transforms.find(paddle_of(world))->position;
        const core::Entity capsule = game::spawn_capsule(
            world, paddle + math::Vec2{side * outer, 0.0f}, game::PowerUp::WidePaddle);
        game::step(world, game::Input{}, DT);

        EXPECT_FALSE(world.capsules.has(capsule)) << (side < 0.0f ? "left" : "right") << " end not caught";
        EXPECT_NEAR(world.paddles.find(paddle_of(world))->wide_time_left, game::WIDE_PADDLE_TIME, TOLERANCE)
            << "the catch did not restart the time";
    }
}

// What the width is for: a ball its normal width would have let through.
TEST(WidePaddle, ReturnsABallTheNormalPaddleWouldMiss)
{
    game::World world = playing_world();
    catch_capsule(world, game::PowerUp::WidePaddle);

    const game::Transform& paddle = *world.transforms.find(paddle_of(world));
    const float beyond_normal = paddle.position.x + game::PADDLE_WIDTH * 0.5f + 8.0f + 10.0f;
    const core::Entity ball = add_ball(world, math::Vec2{beyond_normal, 55.0f}, math::Vec2{0.0f, -200.0f});

    game::collision_system(world);

    EXPECT_GT(world.velocities.find(ball)->value.y, 0.0f);
}

TEST(WidePaddle, LastsTenSeconds)
{
    ASSERT_FLOAT_EQ(game::WIDE_PADDLE_TIME, 10.0f);

    game::World world = playing_world();
    catch_capsule(world, game::PowerUp::WidePaddle);

    run_steps(world, 598);
    EXPECT_FLOAT_EQ(paddle_width(world), game::WIDE_PADDLE_WIDTH) << "gone before 10 seconds";

    run_steps(world, 4);
    EXPECT_FLOAT_EQ(paddle_width(world), game::PADDLE_WIDTH) << "still wide after 10 seconds";
    EXPECT_FLOAT_EQ(world.paddles.find(paddle_of(world))->wide_time_left, 0.0f);
}

TEST(WidePaddle, ASecondCatchRestartsTheTimeWithoutWideningFurther)
{
    game::World world = playing_world();
    catch_capsule(world, game::PowerUp::WidePaddle);
    run_steps(world, 360);

    catch_capsule(world, game::PowerUp::WidePaddle);
    EXPECT_FLOAT_EQ(paddle_width(world), game::WIDE_PADDLE_WIDTH);

    // Past the first catch's 10 seconds, inside the second's.
    run_steps(world, 360);
    EXPECT_FLOAT_EQ(paddle_width(world), game::WIDE_PADDLE_WIDTH);

    run_steps(world, 240);
    EXPECT_FLOAT_EQ(paddle_width(world), game::PADDLE_WIDTH);
}

TEST(WidePaddle, StaysInsideTheWalls)
{
    const float half_normal = game::PADDLE_WIDTH * 0.5f;
    const float half_wide = game::WIDE_PADDLE_WIDTH * 0.5f;

    // Caught against each wall, then pushed into it.
    for (const bool left : {true, false})
    {
        game::World world = playing_world();
        game::Transform& paddle = *world.transforms.find(paddle_of(world));
        paddle.position.x = left ? half_normal : world.size.x - half_normal;

        catch_capsule(world, game::PowerUp::WidePaddle);

        const float expected = left ? half_wide : world.size.x - half_wide;
        EXPECT_FLOAT_EQ(world.transforms.find(paddle_of(world))->position.x, expected) << (left ? "left" : "right");

        game::Input input;
        input.move_left = left;
        input.move_right = !left;
        for (int i = 0; i < 60; i++)
        {
            game::step(world, input, DT);
        }
        EXPECT_FLOAT_EQ(world.transforms.find(paddle_of(world))->position.x, expected) << (left ? "left" : "right");
    }
}

TEST(WidePaddle, TheTimeStopsWhilePaused)
{
    game::World world = playing_world();
    catch_capsule(world, game::PowerUp::WidePaddle);
    run_steps(world, 120);

    game::step(world, pause_input(), DT);
    ASSERT_EQ(world.state, game::GameState::Paused);
    const float left_when_paused = world.paddles.find(paddle_of(world))->wide_time_left;

    // Twenty seconds of pause, twice the power-up's whole time.
    run_steps(world, 1200);

    EXPECT_FLOAT_EQ(paddle_width(world), game::WIDE_PADDLE_WIDTH);
    EXPECT_FLOAT_EQ(world.paddles.find(paddle_of(world))->wide_time_left, left_when_paused);

    // And after it, the 8 seconds that were left, no more and no less.
    game::step(world, pause_input(), DT);
    ASSERT_EQ(world.state, game::GameState::Playing);
    run_steps(world, 476);
    EXPECT_FLOAT_EQ(paddle_width(world), game::WIDE_PADDLE_WIDTH);
    run_steps(world, 4);
    EXPECT_FLOAT_EQ(paddle_width(world), game::PADDLE_WIDTH);
}

TEST(WidePaddle, AMultiBallCapsuleDoesNotWidenThePaddle)
{
    game::World world = playing_world();

    catch_capsule(world, game::PowerUp::MultiBall);

    EXPECT_FLOAT_EQ(paddle_width(world), game::PADDLE_WIDTH);
    EXPECT_FLOAT_EQ(world.paddles.find(paddle_of(world))->wide_time_left, 0.0f);
}

// ---------------------------------------------------------------- multi-ball

namespace
{
    // The balls spawned by the power-up: every ball but the one it came from.
    std::vector<core::Entity> other_balls(const game::World& world, core::Entity source)
    {
        std::vector<core::Entity> others;
        for (const core::Entity ball : world.balls.entities())
        {
            if (!(ball == source))
            {
                others.push_back(ball);
            }
        }
        return others;
    }

    void drop_below_the_floor(game::World& world, core::Entity ball)
    {
        world.transforms.find(ball)->position = math::Vec2{400.0f, -100.0f};
        world.velocities.find(ball)->value = math::Vec2{0.0f, -200.0f};
    }
}

TEST(MultiBall, AddsTwoBallsUpAtThirtyDegreesFromABallInPlayAtItsSpeed)
{
    game::World world = playing_world();
    const math::Vec2 velocity{120.0f, 250.0f};
    const core::Entity source = add_ball(world, math::Vec2{300.0f, 300.0f}, velocity);
    const float speed = math::length(velocity);

    catch_capsule(world, game::PowerUp::MultiBall);

    ASSERT_EQ(world.balls.size(), 3u);
    const game::Transform& from = *world.transforms.find(source);
    EXPECT_FLOAT_EQ(world.velocities.find(source)->value.x, velocity.x) << "the source ball was changed";
    EXPECT_FLOAT_EQ(world.velocities.find(source)->value.y, velocity.y) << "the source ball was changed";

    int going_left = 0;
    int going_right = 0;
    for (const core::Entity ball : other_balls(world, source))
    {
        const game::Transform& transform = *world.transforms.find(ball);
        EXPECT_FLOAT_EQ(transform.position.x, from.position.x);
        EXPECT_FLOAT_EQ(transform.position.y, from.position.y);

        // Drawn along the source ball's path, not sliding in from the serve spot.
        EXPECT_FLOAT_EQ(transform.previous_position.x, from.previous_position.x);
        EXPECT_FLOAT_EQ(transform.previous_position.y, from.previous_position.y);

        const math::Vec2 v = world.velocities.find(ball)->value;
        EXPECT_NEAR(math::length(v), speed, TOLERANCE);
        EXPECT_NEAR(v.y / speed, std::cos(30.0f * 3.14159265f / 180.0f), TOLERANCE);
        EXPECT_NEAR(std::abs(v.x) / speed, 0.5f, TOLERANCE);
        (v.x < 0.0f ? going_left : going_right) += 1;

        // A whole ball, drawn like one.
        EXPECT_TRUE(world.circle_shapes.has(ball));
        EXPECT_TRUE(world.colors.has(ball));
        EXPECT_FLOAT_EQ(world.balls.find(ball)->radius, game::BALL_RADIUS);
    }
    EXPECT_EQ(going_left, 1);
    EXPECT_EQ(going_right, 1);
}

TEST(MultiBall, NeverMoreThanFiveBalls)
{
    game::World world = playing_world();
    add_ball(world, math::Vec2{300.0f, 300.0f}, math::Vec2{0.0f, 100.0f});

    catch_capsule(world, game::PowerUp::MultiBall);
    EXPECT_EQ(world.balls.size(), 3u);
    catch_capsule(world, game::PowerUp::MultiBall);
    EXPECT_EQ(world.balls.size(), 5u);
    catch_capsule(world, game::PowerUp::MultiBall);
    EXPECT_EQ(world.balls.size(), 5u);

    // From four, only one fits.
    game::World four = playing_world();
    for (int i = 0; i < 4; i++)
    {
        add_ball(four, math::Vec2{200.0f + 100.0f * static_cast<float>(i), 300.0f}, math::Vec2{0.0f, 100.0f});
    }
    catch_capsule(four, game::PowerUp::MultiBall);
    EXPECT_EQ(four.balls.size(), 5u);
}

TEST(MultiBall, NothingWithoutABallInPlay)
{
    game::World world = playing_world();

    catch_capsule(world, game::PowerUp::MultiBall);

    EXPECT_EQ(world.balls.size(), 0u);
    EXPECT_EQ(world.capsules.size(), 0u) << "caught all the same";
}

TEST(MultiBall, ABallThatHasFallenIsNotCopied)
{
    game::World world = playing_world();

    // First in the store, so a version that takes the first ball takes it.
    add_ball(world, math::Vec2{200.0f, -30.0f}, math::Vec2{0.0f, -200.0f});
    const core::Entity in_play = add_ball(world, math::Vec2{500.0f, 300.0f}, math::Vec2{0.0f, 200.0f});

    catch_capsule(world, game::PowerUp::MultiBall);

    ASSERT_EQ(world.balls.size(), 3u);
    EXPECT_EQ(world.lives, game::STARTING_LIVES);
    const math::Vec2 from = world.transforms.find(in_play)->position;
    for (const core::Entity ball : other_balls(world, in_play))
    {
        EXPECT_FLOAT_EQ(world.transforms.find(ball)->position.x, from.x);
        EXPECT_FLOAT_EQ(world.transforms.find(ball)->position.y, from.y);
    }
}

TEST(MultiBall, ACatchInTheStepTheLastBallFallsAddsNothing)
{
    game::World world = playing_world();
    add_ball(world, math::Vec2{200.0f, -30.0f}, math::Vec2{0.0f, -200.0f});

    catch_capsule(world, game::PowerUp::MultiBall);

    EXPECT_EQ(world.balls.size(), 1u);
    EXPECT_EQ(world.lives, game::STARTING_LIVES - 1);
    EXPECT_EQ(world.state, game::GameState::Ready);
}

TEST(MultiBall, TheNextLevelAndRestartStartWithOneBall)
{
    game::World world = started_world({game::parse_level("1"), game::parse_level("11")});
    catch_capsule(world, game::PowerUp::MultiBall);
    ASSERT_EQ(world.balls.size(), 3u);

    clear_level(world);
    ASSERT_EQ(world.level_index, 1);
    EXPECT_EQ(world.balls.size(), 1u);

    game::step(world, launch_input(), DT);
    catch_capsule(world, game::PowerUp::MultiBall);
    ASSERT_EQ(world.balls.size(), 3u);
    world.state = game::GameState::GameOver;

    game::Input input;
    input.restart = true;
    game::step(world, input, DT);
    EXPECT_EQ(world.balls.size(), 1u);
}

// ---------------------------------------------------------------- lives with several balls

TEST(Lives, ABallFallingWhileAnotherIsInPlayCostsNothing)
{
    game::World world = playing_world();
    const core::Entity falling = add_ball(world, math::Vec2{200.0f, 300.0f}, math::Vec2{0.0f, 100.0f});
    add_ball(world, math::Vec2{300.0f, 300.0f}, math::Vec2{0.0f, 100.0f});
    add_ball(world, math::Vec2{400.0f, 300.0f}, math::Vec2{0.0f, 100.0f});
    drop_below_the_floor(world, falling);

    game::step(world, game::Input{}, DT);

    EXPECT_EQ(world.lives, game::STARTING_LIVES);
    EXPECT_EQ(world.state, game::GameState::Playing);
    EXPECT_EQ(world.balls.size(), 2u);
    EXPECT_FALSE(world.transforms.has(falling)) << "a fallen extra ball must go";
}

TEST(Lives, TheLastBallFallingCostsALife)
{
    game::World world = playing_world();
    const core::Entity first = add_ball(world, math::Vec2{200.0f, 300.0f}, math::Vec2{0.0f, 100.0f});
    const core::Entity last = add_ball(world, math::Vec2{300.0f, 300.0f}, math::Vec2{0.0f, 100.0f});

    drop_below_the_floor(world, first);
    game::step(world, game::Input{}, DT);
    ASSERT_EQ(world.lives, game::STARTING_LIVES);

    drop_below_the_floor(world, last);
    game::step(world, game::Input{}, DT);

    EXPECT_EQ(world.lives, game::STARTING_LIVES - 1);
    EXPECT_EQ(world.state, game::GameState::Ready);
    ASSERT_EQ(world.balls.size(), 1u);
    EXPECT_FLOAT_EQ(math::length(world.velocities.find(last)->value), 0.0f) << "the last ball is kept, held";
}

TEST(Lives, SeveralFallingInOneStepCostOneLife)
{
    game::World world = playing_world();
    for (int i = 0; i < 3; i++)
    {
        drop_below_the_floor(world, add_ball(world, math::Vec2{}, math::Vec2{}));
    }

    game::step(world, game::Input{}, DT);

    EXPECT_EQ(world.lives, game::STARTING_LIVES - 1);
    EXPECT_EQ(world.state, game::GameState::Ready);
    ASSERT_EQ(world.balls.size(), 1u);
    EXPECT_FLOAT_EQ(math::length(world.velocities.find(world.balls.entities()[0])->value), 0.0f);
}

TEST(Lives, OnTheLastLifeSeveralFallingIsGameOverAtZero)
{
    game::World world = playing_world();
    world.lives = 1;
    for (int i = 0; i < 3; i++)
    {
        drop_below_the_floor(world, add_ball(world, math::Vec2{}, math::Vec2{}));
    }

    game::step(world, game::Input{}, DT);

    EXPECT_EQ(world.lives, 0);
    EXPECT_EQ(world.state, game::GameState::GameOver);
}

TEST(Lives, LosingALifeClearsCapsulesAndTheWidePaddle)
{
    game::World world = playing_world();
    const core::Entity ball = add_ball(world, math::Vec2{300.0f, 300.0f}, math::Vec2{0.0f, 100.0f});
    catch_capsule(world, game::PowerUp::WidePaddle);
    ASSERT_FLOAT_EQ(paddle_width(world), game::WIDE_PADDLE_WIDTH);
    game::spawn_capsule(world, math::Vec2{CAPSULE_LANE_X, 300.0f}, game::PowerUp::MultiBall);
    game::spawn_capsule(world, math::Vec2{CAPSULE_LANE_X, 450.0f}, game::PowerUp::WidePaddle);

    drop_below_the_floor(world, ball);
    game::step(world, game::Input{}, DT);

    ASSERT_EQ(world.state, game::GameState::Ready);
    EXPECT_EQ(world.capsules.size(), 0u);
    EXPECT_FLOAT_EQ(paddle_width(world), game::PADDLE_WIDTH);
    EXPECT_FLOAT_EQ(world.paddles.find(paddle_of(world))->wide_time_left, 0.0f);
}

// With several balls on the way out, the serve must still hold exactly one.
TEST(Serve, TheNextServeParksAndLaunchesOnlyOneBall)
{
    game::World world = started_world({game::parse_level("11")});
    catch_capsule(world, game::PowerUp::MultiBall);
    ASSERT_EQ(world.balls.size(), 3u);

    const std::vector<core::Entity> balls = world.balls.entities();
    for (const core::Entity ball : balls)
    {
        drop_below_the_floor(world, ball);
    }
    game::step(world, game::Input{}, DT);
    ASSERT_EQ(world.state, game::GameState::Ready);
    ASSERT_EQ(world.balls.size(), 1u);

    // Parked: it follows the paddle.
    game::Input input;
    input.move_right = true;
    for (int i = 0; i < 30; i++)
    {
        game::step(world, input, DT);
    }
    const core::Entity held = world.balls.entities()[0];
    const game::Transform& paddle = *world.transforms.find(paddle_of(world));
    EXPECT_FLOAT_EQ(world.transforms.find(held)->position.x, paddle.position.x);
    EXPECT_GT(world.transforms.find(held)->position.y, paddle.position.y);
    EXPECT_EQ(world.balls.size(), 1u);

    game::step(world, launch_input(), DT);
    EXPECT_EQ(world.state, game::GameState::Playing);
    EXPECT_EQ(world.balls.size(), 1u);
    EXPECT_GT(world.velocities.find(held)->value.y, 0.0f);
}

// ---------------------------------------------------------------- a long game

namespace
{
    // What happened to the capsules, and to the game, in one long run. Counted
    // by watching the world between steps, not by anything in the game.
    struct Tally
    {
        int dropped = 0;
        int caught_wide = 0;
        int caught_multi = 0;
        int missed = 0;

        // Gone with a lost life, a cleared level or a win, unless the test can
        // tell they were caught in that same step. A few are, and count here.
        int cleared = 0;

        int steps_adding_balls = 0;
        int most_balls = 0;
        int lives_lost = 0;
        int levels_cleared = 0;
        int restarts = 0;

        bool operator==(const Tally&) const = default;
    };

    struct Game
    {
        game::World world;
        Tally tally;
    };

    // Follows whatever is coming down lowest, a ball or a capsule, and hits the
    // ball a little off centre by an amount that changes with the score and
    // every ten seconds, so it never settles into one path that hits nothing.
    // Every other ten seconds it ignores capsules, and for the last four
    // seconds of every minute it does not move, so capsules are missed and
    // lives lost as well.
    game::Input autopilot(const game::World& world, int step)
    {
        game::Input input;
        input.launch = world.state == game::GameState::Ready;
        input.restart = world.state == game::GameState::GameOver || world.state == game::GameState::Won;

        if (step % 3600 >= 3360)
        {
            return input;
        }
        const bool ignore_capsules = (step / 600) % 2 == 1;

        const game::Transform& paddle = *world.transforms.find(world.paddles.entities()[0]);
        float lowest = world.size.y;
        float target = paddle.position.x;

        for (const core::Entity ball : world.balls.entities())
        {
            const math::Vec2 position = world.transforms.find(ball)->position;
            if (world.velocities.find(ball)->value.y < 0.0f && position.y > paddle.position.y && position.y < lowest)
            {
                lowest = position.y;
                const int aim = (world.score / 10 + step / 600) % 5 - 2;
                target = position.x - static_cast<float>(aim) * 15.0f;
            }
        }

        for (const core::Entity capsule : world.capsules.entities())
        {
            const math::Vec2 position = world.transforms.find(capsule)->position;
            if (!ignore_capsules && position.y > paddle.position.y && position.y < lowest)
            {
                lowest = position.y;
                target = position.x;
            }
        }

        input.move_left = target < paddle.position.x - 4.0f;
        input.move_right = target > paddle.position.x + 4.0f;
        return input;
    }

    Game play(std::uint32_t seed, int steps)
    {
        // Particles use std::rand, not the world's generator. They never
        // change the game, but reset it so they can be compared too.
        std::srand(1);

        Game run;
        game::World& world = run.world;
        Tally& tally = run.tally;

        world.size = math::Vec2{800.0f, 600.0f};
        world.rng.seed(seed);
        game::start_game(world, {game::parse_level(game::BUILT_IN_LEVEL), game::parse_level("1212121212\n2121212121\n3333333333\n")});

        struct Seen
        {
            core::Entity entity;
            float y;
            game::PowerUp kind;
        };

        for (int i = 0; i < steps; i++)
        {
            std::vector<Seen> before;
            for (const core::Entity capsule : world.capsules.entities())
            {
                before.push_back({capsule, world.transforms.find(capsule)->position.y, world.capsules.find(capsule)->kind});
            }
            const std::size_t balls_before = world.balls.size();
            const float wide_before = world.paddles.components()[0].wide_time_left;
            const game::GameState state_before = world.state;
            const int lives_before = world.lives;
            const int level_before = world.level_index;

            game::step(world, autopilot(world, i), DT);

            // What main does with them once a frame.
            world.events.clear();

            for (const core::Entity capsule : world.capsules.entities())
            {
                const bool is_new = std::none_of(before.begin(), before.end(),
                    [&](const Seen& seen) { return seen.entity == capsule; });
                tally.dropped += is_new ? 1 : 0;
            }

            for (const Seen& seen : before)
            {
                if (world.capsules.has(seen.entity))
                {
                    continue;
                }

                // Where it got to in this step, before it was removed.
                const float top = seen.y - game::CAPSULE_SPEED * DT + game::CAPSULE_HEIGHT * 0.5f;
                if (world.state != game::GameState::Playing)
                {
                    tally.cleared += 1;
                }
                else if (top < 0.0f)
                {
                    tally.missed += 1;
                }
                else
                {
                    (seen.kind == game::PowerUp::WidePaddle ? tally.caught_wide : tally.caught_multi) += 1;
                }
            }

            if (state_before == game::GameState::Playing && world.state == game::GameState::Playing
                && world.balls.size() > balls_before)
            {
                tally.steps_adding_balls += 1;
            }
            tally.most_balls = std::max(tally.most_balls, static_cast<int>(world.balls.size()));
            tally.lives_lost += world.lives < lives_before ? 1 : 0;
            tally.levels_cleared += (world.level_index > level_before || world.state == game::GameState::Won) && state_before == game::GameState::Playing ? 1 : 0;
            tally.restarts += (state_before == game::GameState::GameOver || state_before == game::GameState::Won)
                && world.state == game::GameState::Ready ? 1 : 0;

            // The rules, at every step of a long game.
            EXPECT_LE(world.balls.size(), 5u) << "step " << i;
            const game::Transform& paddle = *world.transforms.find(world.paddles.entities()[0]);
            EXPECT_GE(paddle.position.x - paddle.size.x * 0.5f, 0.0f) << "step " << i;
            EXPECT_LE(paddle.position.x + paddle.size.x * 0.5f, world.size.x) << "step " << i;
            if (world.state == game::GameState::Ready)
            {
                EXPECT_EQ(world.balls.size(), 1u) << "step " << i;
                EXPECT_EQ(world.capsules.size(), 0u) << "step " << i;
                EXPECT_EQ(paddle.size.x, game::PADDLE_WIDTH) << "step " << i;
            }
            if (world.paddles.components()[0].wide_time_left > wide_before)
            {
                EXPECT_EQ(paddle.size.x, game::WIDE_PADDLE_WIDTH) << "step " << i;
            }
        }

        return run;
    }

    template <typename T, typename Same>
    bool same_store(const core::ComponentStore<T>& a, const core::ComponentStore<T>& b, Same same)
    {
        if (a.entities() != b.entities())
        {
            return false;
        }
        for (std::size_t i = 0; i < a.size(); i++)
        {
            if (!same(a.components()[i], b.components()[i]))
            {
                return false;
            }
        }
        return true;
    }

    bool same_vec(math::Vec2 a, math::Vec2 b)
    {
        return a.x == b.x && a.y == b.y;
    }

    // Every field of the two worlds, compared exactly: no tolerance.
    void expect_identical(const game::World& a, const game::World& b)
    {
        EXPECT_EQ(a.score, b.score);
        EXPECT_EQ(a.lives, b.lives);
        EXPECT_EQ(a.state, b.state);
        EXPECT_EQ(a.level_index, b.level_index);
        EXPECT_TRUE(a.level == b.level);
        EXPECT_TRUE(a.rng == b.rng) << "the generators have drawn different numbers";

        EXPECT_TRUE(same_store(a.transforms, b.transforms, [](const game::Transform& x, const game::Transform& y)
            { return same_vec(x.position, y.position) && same_vec(x.size, y.size) && same_vec(x.previous_position, y.previous_position); }))
            << "transforms";
        EXPECT_TRUE(same_store(a.velocities, b.velocities, [](const game::Velocity& x, const game::Velocity& y)
            { return same_vec(x.value, y.value); })) << "velocities";
        EXPECT_TRUE(same_store(a.colors, b.colors, [](const core::Color& x, const core::Color& y)
            { return x.r == y.r && x.g == y.g && x.b == y.b && x.a == y.a; })) << "colours";
        EXPECT_TRUE(same_store(a.balls, b.balls, [](const game::Ball& x, const game::Ball& y)
            { return x.radius == y.radius; })) << "balls";
        EXPECT_TRUE(same_store(a.paddles, b.paddles, [](const game::Paddle& x, const game::Paddle& y)
            { return x.speed == y.speed && x.wide_time_left == y.wide_time_left; })) << "paddles";
        EXPECT_TRUE(same_store(a.bricks, b.bricks, [](const game::Brick& x, const game::Brick& y)
            { return x.hit_points == y.hit_points; })) << "bricks";
        EXPECT_TRUE(same_store(a.capsules, b.capsules, [](const game::Capsule& x, const game::Capsule& y)
            { return x.kind == y.kind; })) << "capsules";
        EXPECT_TRUE(same_store(a.particles, b.particles, [](const game::Particle& x, const game::Particle& y)
            { return x.lifetime == y.lifetime && x.max_life == y.max_life; })) << "particles";
        EXPECT_TRUE(same_store(a.circle_shapes, b.circle_shapes, [](auto, auto) { return true; })) << "circle shapes";
        EXPECT_TRUE(same_store(a.destroyed, b.destroyed, [](auto, auto) { return true; })) << "destroyed";
    }
}

// The README's promise, with power-ups in it: the same seed and the same input
// give the same game, step for step, for fifteen minutes of play.
//
// Another compiler's sin and cos can differ in the last bit, which makes a
// different game, as deterministic as this one. So what is asserted to have
// happened below is what happened in every one of 60 seeds tried here, not
// just in this one; clearing a level is printed but not required, since 5 of
// those 60 games never managed it.
TEST(LongGame, TheSameSeedPlaysTheSameGameWithPowerUps)
{
    constexpr int FIFTEEN_MINUTES = 15 * 60 * 60;

    const Game first = play(2024, FIFTEEN_MINUTES);
    const Game second = play(2024, FIFTEEN_MINUTES);

    const Tally& t = first.tally;
    std::printf("[ tally    ] %d capsules dropped: %d caught (%d wide, %d multi-ball), %d missed, %d cleared, %d still falling\n"
                "[ tally    ] %d steps added balls, most balls %d, %d lives lost, %d levels cleared, %d restarts, score %d\n",
        t.dropped, t.caught_wide + t.caught_multi, t.caught_wide, t.caught_multi, t.missed, t.cleared,
        static_cast<int>(first.world.capsules.size()), t.steps_adding_balls, t.most_balls, t.lives_lost,
        t.levels_cleared, t.restarts, first.world.score);

    // It really was a game with power-ups in it.
    EXPECT_GT(t.dropped, 0);
    EXPECT_GT(t.caught_wide, 0);
    EXPECT_GT(t.caught_multi, 0);
    EXPECT_GT(t.missed, 0);
    EXPECT_GT(t.steps_adding_balls, 0);
    EXPECT_GE(t.most_balls, 3);
    EXPECT_GT(t.lives_lost, 0);
    EXPECT_EQ(t.dropped, t.caught_wide + t.caught_multi + t.missed + t.cleared + static_cast<int>(first.world.capsules.size()))
        << "a capsule went unaccounted for";

    EXPECT_TRUE(first.tally == second.tally);
    expect_identical(first.world, second.world);
}
