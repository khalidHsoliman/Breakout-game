#include <gtest/gtest.h>

#include "game/Spawn.h"

namespace
{
    // 6 + 8 + 10 + 8 + 6 = 38 bricks.
    constexpr const char* diamond = R"(
..111111..
.12222221.
1233333321
.12222221.
..111111..
)";

    game::World make_world()
    {
        game::World world;
        world.size = math::Vec2{800.0f, 600.0f};
        return world;
    }
}

TEST(SpawnLevel, CreatesOneBrickPerNonEmptyCell)
{
    game::World world = make_world();

    game::spawn_level(world, game::parse_level(diamond));

    EXPECT_EQ(world.bricks.size(), 38u);
}

TEST(SpawnLevel, BricksCarryTheHitPointsFromTheGrid)
{
    game::World world = make_world();

    game::spawn_level(world, game::parse_level("123"));

    ASSERT_EQ(world.bricks.size(), 3u);

    int total = 0;
    for (const game::Brick& brick : world.bricks.components())
    {
        total += brick.hit_points;
    }
    EXPECT_EQ(total, 6);
}

TEST(SpawnLevel, BricksFitInsideThePlayArea)
{
    game::World world = make_world();

    game::spawn_level(world, game::parse_level(diamond));

    for (const core::Entity entity : world.bricks.entities())
    {
        const game::Transform* transform = world.transforms.find(entity);
        ASSERT_NE(transform, nullptr);

        const math::Vec2 half = transform->size * 0.5f;
        EXPECT_GE(transform->position.x - half.x, 0.0f);
        EXPECT_LE(transform->position.x + half.x, world.size.x);
        EXPECT_GE(transform->position.y - half.y, 0.0f);
        EXPECT_LE(transform->position.y + half.y, world.size.y);
    }
}

// The same level authored once must fit whatever play area it is given.
TEST(SpawnLevel, GeometryScalesWithTheWorldSize)
{
    game::World narrow = make_world();
    narrow.size = math::Vec2{400.0f, 600.0f};
    game::spawn_level(narrow, game::parse_level(diamond));

    game::World wide = make_world();
    game::spawn_level(wide, game::parse_level(diamond));

    ASSERT_EQ(narrow.bricks.size(), wide.bricks.size());

    const float narrow_width = narrow.transforms.components().front().size.x;
    const float wide_width = wide.transforms.components().front().size.x;
    EXPECT_LT(narrow_width, wide_width);
}

TEST(SpawnLevel, AnEmptyLevelSpawnsNothing)
{
    game::World world = make_world();

    game::spawn_level(world, game::parse_level(""));

    EXPECT_EQ(world.bricks.size(), 0u);
}

TEST(SpawnBall, StartsHeldInsideThePlayArea)
{
    game::World world = make_world();

    const core::Entity ball = game::spawn_ball(world);

    const game::Transform* transform = world.transforms.find(ball);
    ASSERT_NE(transform, nullptr);
    EXPECT_GT(transform->position.y, 0.0f);
    EXPECT_LT(transform->position.y, world.size.y);
    EXPECT_TRUE(world.circle_shapes.has(ball));

    // Held until launched, so it must not be moving.
    EXPECT_FLOAT_EQ(math::length(world.velocities.find(ball)->value), 0.0f);
}

TEST(LaunchBall, GivesTheBallAnUpwardVelocity)
{
    game::World world = make_world();
    const core::Entity ball = game::spawn_ball(world);

    game::launch_ball(world, ball);

    EXPECT_GT(world.velocities.find(ball)->value.y, 0.0f);
    EXPECT_GT(math::length(world.velocities.find(ball)->value), 0.0f);
}

TEST(StartGame, RebuildsEverythingFromScratch)
{
    game::World world = make_world();
    game::start_game(world, game::parse_level(diamond));

    const std::size_t bricks_before = world.bricks.size();
    world.score = 500;
    world.lives = 1;
    world.state = game::GameState::GameOver;
    world.destroy_entity(world.bricks.entities()[0]);

    game::start_game(world, world.level);

    EXPECT_EQ(world.bricks.size(), bricks_before);
    EXPECT_EQ(world.score, 0);
    EXPECT_EQ(world.lives, game::starting_lives);
    EXPECT_EQ(world.state, game::GameState::Ready);
    EXPECT_EQ(world.paddles.size(), 1u);
    EXPECT_EQ(world.balls.size(), 1u);
}

// If two toughness levels shared a colour, re-colouring on damage would be
// invisible and the whole scheme would carry no information.
TEST(BrickColor, DiffersBetweenToughnessLevels)
{
    for (int hit_points = 1; hit_points < 4; ++hit_points)
    {
        const core::Color lower = game::brick_color(hit_points);
        const core::Color higher = game::brick_color(hit_points + 1);

        const bool identical = lower.r == higher.r && lower.g == higher.g && lower.b == higher.b;
        EXPECT_FALSE(identical) << "hit points " << hit_points << " and " << hit_points + 1;
    }
}

TEST(SpawnPaddle, IsCentredHorizontally)
{
    game::World world = make_world();

    const core::Entity paddle = game::spawn_paddle(world);

    EXPECT_FLOAT_EQ(world.transforms.find(paddle)->position.x, world.size.x * 0.5f);
    EXPECT_GT(world.paddles.find(paddle)->speed, 0.0f);
}
