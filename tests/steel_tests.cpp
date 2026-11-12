#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "game/LevelCheck.h"
#include "game/Spawn.h"
#include "game/Systems.h"
#include "math/Collision.h"

namespace
{
    constexpr math::Vec2 WORLD{800.0f, 600.0f};
    constexpr float DT = 1.0f / 60.0f;
    constexpr float TOLERANCE = 1e-3f;

    // Where the test steel sits, and where a ball just under it heading up
    // starts, so one collision_system call is one hit on it.
    constexpr math::Vec2 STEEL_AT{400.0f, 500.0f};
    constexpr math::Vec2 BALL_UNDER_STEEL{400.0f, 490.0f};

    game::World make_world(std::uint32_t seed = 1)
    {
        game::World world;
        world.size = WORLD;
        world.state = game::GameState::Playing;
        world.rng.seed(seed);
        return world;
    }

    core::Entity add_ball(game::World& world, math::Vec2 position, math::Vec2 velocity)
    {
        const core::Entity entity = world.create_entity();
        world.transforms.add(entity, game::Transform{position, math::Vec2{16.0f, 16.0f}, position});
        world.velocities.add(entity, game::Velocity{velocity});
        world.balls.add(entity, game::Ball{8.0f});
        return entity;
    }

    core::Entity add_brick(game::World& world, math::Vec2 position, int hit_points = 1)
    {
        const core::Entity entity = world.create_entity();
        world.transforms.add(entity, game::Transform{position, math::Vec2{60.0f, 24.0f}, position});
        world.colors.add(entity, game::brick_color(hit_points));
        world.bricks.add(entity, game::Brick{hit_points});
        return entity;
    }

    // Built the way spawn_level builds steel: a transform, the steel colour and
    // the Steel tag, and no Brick.
    core::Entity add_steel(game::World& world, math::Vec2 position, math::Vec2 size = math::Vec2{60.0f, 24.0f})
    {
        const core::Entity entity = world.create_entity();
        world.transforms.add(entity, game::Transform{position, size, position});
        world.colors.add(entity, game::brick_color(game::STEEL));
        world.steel.add(entity, game::Steel{});
        return entity;
    }

    // One hit from below on the steel at STEEL_AT, by a ball put back under it
    // first. Checks the ball really bounced, so a test of what a hit does not
    // do cannot pass just because the ball went through.
    void hit_steel(game::World& world, core::Entity ball, float speed = 200.0f)
    {
        world.transforms.find(ball)->position = BALL_UNDER_STEEL;
        world.velocities.find(ball)->value = math::Vec2{0.0f, speed};
        game::collision_system(world);
        EXPECT_LT(world.velocities.find(ball)->value.y, 0.0f) << "the ball did not bounce off the steel";
    }

    int count(const game::World& world, game::GameEvent event)
    {
        return static_cast<int>(std::count(world.events.begin(), world.events.end(), event));
    }

    float distance(core::Color a, core::Color b)
    {
        return std::sqrt((a.r - b.r) * (a.r - b.r) + (a.g - b.g) * (a.g - b.g) + (a.b - b.b) * (a.b - b.b));
    }

    game::Input launch_input()
    {
        game::Input input;
        input.launch = true;
        return input;
    }

    // Started and launched, like a game a moment after Space.
    game::World launched_world(std::vector<game::Level> levels)
    {
        game::World world;
        world.size = WORLD;
        world.rng.seed(1);
        game::start_game(world, std::move(levels));
        game::step(world, launch_input(), DT);
        return world;
    }

    // Sends the ball at the only breakable brick from just below it and steps
    // until the level changes or the game is won, at most a second.
    void break_the_only_brick(game::World& world)
    {
        ASSERT_EQ(world.bricks.size(), 1u);
        const int level_before = world.level_index;

        const core::Entity ball = world.balls.entities()[0];
        const math::Vec2 brick = world.transforms.find(world.bricks.entities()[0])->position;
        world.transforms.find(ball)->position = math::Vec2{brick.x, brick.y - 30.0f};
        world.velocities.find(ball)->value = math::Vec2{0.0f, 300.0f};

        for (int i = 0; i < 60 && world.level_index == level_before && world.state == game::GameState::Playing; i++)
        {
            game::step(world, game::Input{}, DT);
        }
    }
}

// ---------------------------------------------------------------- spawning

TEST(SteelSpawn, GoesInItsOwnStoreNotWithTheBricks)
{
    game::World world = make_world();
    const game::Level level = game::parse_level("1#\n#2");

    game::spawn_level(world, level);

    EXPECT_EQ(world.bricks.size(), 2u);
    ASSERT_EQ(world.steel.size(), 2u);

    const math::Vec2 size = game::brick_size(level, WORLD);
    const math::Vec2 expected[] = {game::brick_center(level, WORLD, 1, 0), game::brick_center(level, WORLD, 0, 1)};

    for (std::size_t i = 0; i < world.steel.size(); i++)
    {
        const core::Entity steel = world.steel.entities()[i];
        EXPECT_FALSE(world.bricks.has(steel)) << "steel is not a brick that can be cleared";
        ASSERT_TRUE(world.transforms.has(steel));
        ASSERT_TRUE(world.colors.has(steel));

        const game::Transform& transform = *world.transforms.find(steel);
        EXPECT_FLOAT_EQ(transform.position.x, expected[i].x);
        EXPECT_FLOAT_EQ(transform.position.y, expected[i].y);
        EXPECT_FLOAT_EQ(transform.size.x, size.x);
        EXPECT_FLOAT_EQ(transform.size.y, size.y);

        const core::Color color = *world.colors.find(steel);
        const core::Color grey = game::brick_color(game::STEEL);
        EXPECT_TRUE(color.r == grey.r && color.g == grey.g && color.b == grey.b);
    }
}

TEST(SteelSpawn, IsGreyAndLooksLikeNoBrickCapsuleOrPaddle)
{
    const core::Color steel = game::brick_color(game::STEEL);
    EXPECT_FLOAT_EQ(steel.r, 0.50f);
    EXPECT_FLOAT_EQ(steel.g, 0.52f);
    EXPECT_FLOAT_EQ(steel.b, 0.56f);

    for (int hit_points = 1; hit_points <= 9; ++hit_points)
    {
        EXPECT_GT(distance(steel, game::brick_color(hit_points)), 0.15f) << "hit points " << hit_points;
    }
    EXPECT_GT(distance(steel, game::capsule_color(game::PowerUp::WidePaddle)), 0.15f);
    EXPECT_GT(distance(steel, game::capsule_color(game::PowerUp::MultiBall)), 0.15f);

    game::World world = make_world();
    const core::Entity paddle = game::spawn_paddle(world);
    EXPECT_GT(distance(steel, *world.colors.find(paddle)), 0.15f);
}

// The check lets steel down to row 17 because it clears the held ball there.
TEST(SteelSpawn, SteelInTheLowestAllowedRowClearsTheHeldBall)
{
    std::string text;
    for (int row = 0; row < game::max_level_rows(WORLD) - 1; row++)
    {
        text += "1\n";
    }
    text += "#\n";
    ASSERT_EQ(game::check_level_text(text, WORLD), std::nullopt);

    game::World world;
    world.size = WORLD;
    game::start_game(world, game::parse_level(text));
    game::step(world, game::Input{}, DT);   // serve_system parks the ball

    const core::Entity ball = world.balls.entities()[0];
    const math::Circle circle{world.transforms.find(ball)->position, world.balls.find(ball)->radius};

    ASSERT_EQ(world.steel.size(), 1u);
    const game::Transform& steel = *world.transforms.find(world.steel.entities()[0]);
    const math::Vec2 half = steel.size * 0.5f;
    EXPECT_FALSE(math::circle_vs_aabb(circle, math::AABB{steel.position - half, steel.position + half}).hit);
}

// ---------------------------------------------------------------- a hit on steel

TEST(SteelHit, TheBallBouncesOffIt)
{
    game::World world = make_world();
    const core::Entity steel = add_steel(world, STEEL_AT);
    const core::Entity ball = add_ball(world, BALL_UNDER_STEEL, math::Vec2{0.0f, 200.0f});

    game::collision_system(world);

    EXPECT_LT(world.velocities.find(ball)->value.y, 0.0f);

    // Pushed out below it.
    const float steel_bottom = STEEL_AT.y - 12.0f;
    EXPECT_LE(world.transforms.find(ball)->position.y + 8.0f, steel_bottom);
    EXPECT_TRUE(game::is_alive(world, steel));
}

TEST(SteelHit, NeverBreaksHoweverOftenItIsHit)
{
    game::World world = make_world();
    const core::Entity steel = add_steel(world, STEEL_AT);
    const core::Entity ball = add_ball(world, BALL_UNDER_STEEL, math::Vec2{});

    for (int i = 0; i < 100; i++)
    {
        hit_steel(world, ball);
        game::sweep_destroyed(world);
    }

    ASSERT_TRUE(world.steel.has(steel));
    ASSERT_TRUE(world.transforms.has(steel));
    ASSERT_TRUE(world.colors.has(steel));
    EXPECT_TRUE(game::is_alive(world, steel));
    EXPECT_FALSE(world.bricks.has(steel));

    const core::Color color = *world.colors.find(steel);
    const core::Color grey = game::brick_color(game::STEEL);
    EXPECT_TRUE(color.r == grey.r && color.g == grey.g && color.b == grey.b) << "recoloured as if damaged";
}

TEST(SteelHit, ScoresNothing)
{
    game::World world = make_world();
    add_steel(world, STEEL_AT);
    const core::Entity ball = add_ball(world, BALL_UNDER_STEEL, math::Vec2{});

    hit_steel(world, ball);
    hit_steel(world, ball);

    EXPECT_EQ(world.score, 0);
}

TEST(SteelHit, DrawsNothingFromTheGeneratorAndDropsNoCapsule)
{
    game::World world = make_world();
    world.capsule_drop_chance = 1.0f;
    add_steel(world, STEEL_AT);
    const core::Entity ball = add_ball(world, BALL_UNDER_STEEL, math::Vec2{});
    const std::mt19937 before = world.rng;

    for (int i = 0; i < 20; i++)
    {
        hit_steel(world, ball);
    }

    EXPECT_TRUE(world.rng == before) << "the generator moved";
    EXPECT_EQ(world.capsules.size(), 0u);
}

TEST(SteelHit, MakesNoParticles)
{
    game::World world = make_world();
    add_steel(world, STEEL_AT);
    const core::Entity ball = add_ball(world, BALL_UNDER_STEEL, math::Vec2{});

    hit_steel(world, ball);

    EXPECT_EQ(world.particles.size(), 0u);
}

TEST(SteelHit, DoesNotSpeedTheBallUp)
{
    game::World world = make_world();
    add_steel(world, STEEL_AT);
    const core::Entity ball = add_ball(world, BALL_UNDER_STEEL, math::Vec2{});

    for (int i = 0; i < 10; i++)
    {
        hit_steel(world, ball, 200.0f);
        EXPECT_NEAR(math::length(world.velocities.find(ball)->value), 200.0f, TOLERANCE) << "hit " << i;
    }
}

TEST(SteelHit, IsReportedAsSteelNotAsABrick)
{
    game::World world = make_world();
    add_steel(world, STEEL_AT);
    const core::Entity ball = add_ball(world, BALL_UNDER_STEEL, math::Vec2{});

    hit_steel(world, ball);

    EXPECT_EQ(world.events, (std::vector<game::GameEvent>{game::GameEvent::SteelHit}));
}

// ---------------------------------------------------------------- brick and steel in one step

// The ball at x 400 touches both from below: the brick on the left of it, the
// steel on the right. Only the deeper contact is resolved, once. Resolving
// both would reflect the ball twice and send it back up.
TEST(SteelAndBrick, TouchingBothOnlyTheDeeperSteelIsHit)
{
    game::World world = make_world();
    const core::Entity brick = add_brick(world, math::Vec2{370.0f, 500.0f}, 1);   // bottom 488: 2 deep
    const core::Entity steel = add_steel(world, math::Vec2{430.0f, 498.0f});       // bottom 486: 4 deep
    const core::Entity ball = add_ball(world, math::Vec2{400.0f, 482.0f}, math::Vec2{0.0f, 200.0f});

    game::collision_system(world);

    EXPECT_LT(world.velocities.find(ball)->value.y, 0.0f);
    EXPECT_NEAR(math::length(world.velocities.find(ball)->value), 200.0f, TOLERANCE);
    EXPECT_EQ(world.events, (std::vector<game::GameEvent>{game::GameEvent::SteelHit}));
    EXPECT_EQ(world.bricks.find(brick)->hit_points, 1);
    EXPECT_TRUE(game::is_alive(world, brick));
    EXPECT_TRUE(game::is_alive(world, steel));
    EXPECT_EQ(world.score, 0);
}

TEST(SteelAndBrick, TouchingBothOnlyTheDeeperBrickIsHit)
{
    game::World world = make_world();
    const core::Entity brick = add_brick(world, math::Vec2{370.0f, 498.0f}, 2);   // bottom 486: 4 deep
    const core::Entity steel = add_steel(world, math::Vec2{430.0f, 500.0f});       // bottom 488: 2 deep
    const core::Entity ball = add_ball(world, math::Vec2{400.0f, 482.0f}, math::Vec2{0.0f, 200.0f});

    game::collision_system(world);

    EXPECT_LT(world.velocities.find(ball)->value.y, 0.0f);
    EXPECT_NEAR(math::length(world.velocities.find(ball)->value), 200.0f + game::BALL_SPEED_PER_HIT, TOLERANCE);
    EXPECT_EQ(world.events, (std::vector<game::GameEvent>{game::GameEvent::BrickHit}));
    EXPECT_EQ(world.bricks.find(brick)->hit_points, 1);
    EXPECT_TRUE(game::is_alive(world, steel));
    EXPECT_EQ(world.score, 10);
}

// Exactly as deep: the brick that can be broken is the one hit.
TEST(SteelAndBrick, AnEqualContactGoesToTheBrick)
{
    game::World world = make_world();
    const core::Entity brick = add_brick(world, math::Vec2{370.0f, 500.0f}, 2);
    add_steel(world, math::Vec2{430.0f, 500.0f});
    add_ball(world, math::Vec2{400.0f, 482.0f}, math::Vec2{0.0f, 200.0f});

    game::collision_system(world);

    EXPECT_EQ(world.events, (std::vector<game::GameEvent>{game::GameEvent::BrickHit}));
    EXPECT_EQ(world.bricks.find(brick)->hit_points, 1);
}

// ---------------------------------------------------------------- clearing a level

TEST(SteelLevel, IsNotClearedWhileABreakableBrickIsLeft)
{
    game::World world = launched_world({game::parse_level("#1#")});

    for (int i = 0; i < 10; i++)
    {
        game::step(world, game::Input{}, DT);
    }

    EXPECT_EQ(world.state, game::GameState::Playing);
    EXPECT_EQ(world.bricks.size(), 1u);
    EXPECT_EQ(world.steel.size(), 2u);
}

TEST(SteelLevel, SteelPlusOneBrickIsWonWhenTheBrickGoes)
{
    game::World world = launched_world({game::parse_level("#1#")});

    break_the_only_brick(world);

    EXPECT_EQ(world.state, game::GameState::Won);
    EXPECT_EQ(world.bricks.size(), 0u);
    EXPECT_EQ(world.steel.size(), 2u) << "the steel is still there";
    EXPECT_EQ(world.score, 10);
}

TEST(SteelLevel, SteelPlusOneBrickMovesOnWhenTheBrickGoesAndTheSteelGoesWithIt)
{
    game::World world = launched_world({game::parse_level("#1#"), game::parse_level("11")});
    ASSERT_EQ(world.steel.size(), 2u);

    break_the_only_brick(world);

    EXPECT_EQ(world.level_index, 1);
    EXPECT_EQ(world.state, game::GameState::Ready);
    EXPECT_EQ(world.bricks.size(), 2u);
    EXPECT_EQ(world.steel.size(), 0u) << "steel from level 1 was left behind";
}

TEST(SteelLevel, RestartBuildsTheSteelAgain)
{
    game::World world = launched_world({game::parse_level("#1#")});
    break_the_only_brick(world);
    ASSERT_EQ(world.state, game::GameState::Won);

    game::Input restart;
    restart.restart = true;
    game::step(world, restart, DT);

    EXPECT_EQ(world.state, game::GameState::Ready);
    EXPECT_EQ(world.bricks.size(), 1u);
    EXPECT_EQ(world.steel.size(), 2u);
    EXPECT_EQ(world.transforms.size(), 1u + 2u + 1u + 1u) << "brick, steel, paddle and ball, nothing twice";
}

// ---------------------------------------------------------------- multi-ball

// Three balls under one slab of steel, all hitting it in the same step: each
// bounces, each is reported, and nothing else happens.
TEST(SteelMultiBall, SeveralBallsHitSteelInTheSameStep)
{
    game::World world = make_world();
    world.capsule_drop_chance = 1.0f;
    const core::Entity steel = add_steel(world, math::Vec2{400.0f, 500.0f}, math::Vec2{600.0f, 24.0f});
    const core::Entity balls[] = {
        add_ball(world, math::Vec2{200.0f, 482.0f}, math::Vec2{-100.0f, 200.0f}),
        add_ball(world, math::Vec2{400.0f, 482.0f}, math::Vec2{0.0f, 250.0f}),
        add_ball(world, math::Vec2{600.0f, 482.0f}, math::Vec2{100.0f, 200.0f}),
    };
    std::vector<float> speeds;
    for (const core::Entity ball : balls)
    {
        speeds.push_back(math::length(world.velocities.find(ball)->value));
    }
    const std::mt19937 before = world.rng;

    game::collision_system(world);

    EXPECT_EQ(count(world, game::GameEvent::SteelHit), 3);
    EXPECT_EQ(world.events.size(), 3u);
    for (std::size_t i = 0; i < 3; i++)
    {
        const math::Vec2 v = world.velocities.find(balls[i])->value;
        EXPECT_LT(v.y, 0.0f) << "ball " << i;
        EXPECT_NEAR(math::length(v), speeds[i], TOLERANCE) << "ball " << i;
    }
    EXPECT_TRUE(game::is_alive(world, steel));
    EXPECT_EQ(world.score, 0);
    EXPECT_EQ(world.capsules.size(), 0u);
    EXPECT_EQ(world.particles.size(), 0u);
    EXPECT_TRUE(world.rng == before);
}

// A multi-ball catch under a steel ceiling: the two new balls bounce off the
// steel like the first, none gets through it, and the level goes on.
TEST(SteelMultiBall, NewBallsBounceOffSteelAndNoneGetsThrough)
{
    game::World world = make_world();
    game::spawn_paddle(world);
    add_brick(world, math::Vec2{100.0f, 560.0f}, 9);   // above the steel, so the level is never cleared
    const core::Entity steel = add_steel(world, math::Vec2{400.0f, 400.0f}, math::Vec2{760.0f, 24.0f});
    const float steel_bottom = 400.0f - 12.0f;

    add_ball(world, math::Vec2{400.0f, 300.0f}, math::Vec2{0.0f, 250.0f});

    const math::Vec2 paddle = world.transforms.find(world.paddles.entities()[0])->position;
    game::spawn_capsule(world, paddle, game::PowerUp::MultiBall);
    game::step(world, game::Input{}, DT);
    ASSERT_EQ(world.balls.size(), 3u);
    world.events.clear();
    const std::mt19937 before = world.rng;

    for (int i = 0; i < 40; i++)
    {
        game::step(world, game::Input{}, DT);

        for (const core::Entity ball : world.balls.entities())
        {
            EXPECT_LE(world.transforms.find(ball)->position.y + 8.0f, steel_bottom + TOLERANCE)
                << "a ball got into the steel at step " << i;
        }
    }

    ASSERT_EQ(world.balls.size(), 3u);
    for (const core::Entity ball : world.balls.entities())
    {
        const math::Vec2 v = world.velocities.find(ball)->value;
        EXPECT_LT(v.y, 0.0f) << "every ball came back down";
        EXPECT_NEAR(math::length(v), 250.0f, TOLERANCE) << "steel sped a ball up";
    }
    EXPECT_EQ(count(world, game::GameEvent::SteelHit), 3);
    EXPECT_EQ(count(world, game::GameEvent::BrickHit), 0);
    EXPECT_TRUE(world.steel.has(steel));
    EXPECT_EQ(world.state, game::GameState::Playing);
    EXPECT_EQ(world.score, 0);
    EXPECT_EQ(world.capsules.size(), 0u);
    EXPECT_TRUE(world.rng == before);
}
