#include <gtest/gtest.h>

#include "game/Spawn.h"
#include "game/Systems.h"

namespace
{
    constexpr float DT = 1.0f / 60.0f;
    constexpr float TOLERANCE = 1e-4f;

    game::World make_world()
    {
        game::World world;
        world.size = math::Vec2{800.0f, 600.0f};
        world.state = game::GameState::Playing;
        return world;
    }

    core::Entity add_ball(game::World& world, math::Vec2 position, math::Vec2 velocity, float radius = 8.0f)
    {
        const core::Entity entity = world.create_entity();
        world.transforms.add(entity, game::Transform{position, math::Vec2{radius * 2.0f, radius * 2.0f}, {}});
        world.velocities.add(entity, game::Velocity{velocity});
        world.balls.add(entity, game::Ball{radius});
        return entity;
    }

    core::Entity add_paddle(game::World& world, math::Vec2 position, float speed = 500.0f)
    {
        const core::Entity entity = world.create_entity();
        world.transforms.add(entity, game::Transform{position, math::Vec2{120.0f, 20.0f}, {}});
        world.paddles.add(entity, game::Paddle{speed});
        return entity;
    }

    core::Entity add_brick(game::World& world, math::Vec2 position, int hit_points = 1)
    {
        const core::Entity entity = world.create_entity();
        world.transforms.add(entity, game::Transform{position, math::Vec2{90.0f, 30.0f}, {}});
        world.bricks.add(entity, game::Brick{hit_points});
        return entity;
    }
}

TEST(MovementSystem, IntegratesVelocity)
{
    game::World world = make_world();
    const core::Entity ball = add_ball(world, math::Vec2{100.0f, 100.0f}, math::Vec2{60.0f, 120.0f});

    game::movement_system(world, DT);

    EXPECT_NEAR(world.transforms.find(ball)->position.x, 101.0f, TOLERANCE);
    EXPECT_NEAR(world.transforms.find(ball)->position.y, 102.0f, TOLERANCE);
}

TEST(MovementSystem, LeavesEntitiesWithoutVelocityAlone)
{
    game::World world = make_world();
    const core::Entity brick = add_brick(world, math::Vec2{400.0f, 500.0f});

    game::movement_system(world, DT);

    EXPECT_NEAR(world.transforms.find(brick)->position.x, 400.0f, TOLERANCE);
}

TEST(PaddleSystem, MovesLeftAndRight)
{
    game::World world = make_world();
    const core::Entity paddle = add_paddle(world, math::Vec2{400.0f, 40.0f}, 600.0f);

    game::Input input;
    input.move_right = true;
    game::paddle_system(world, input, DT);
    EXPECT_NEAR(world.transforms.find(paddle)->position.x, 410.0f, TOLERANCE);

    input = game::Input{};
    input.move_left = true;
    game::paddle_system(world, input, DT);
    EXPECT_NEAR(world.transforms.find(paddle)->position.x, 400.0f, TOLERANCE);
}

TEST(PaddleSystem, StaysInsideThePlayArea)
{
    game::World world = make_world();
    const core::Entity paddle = add_paddle(world, math::Vec2{40.0f, 40.0f}, 600.0f);

    game::Input input;
    input.move_left = true;
    for (int i = 0; i < 60; i++)
    {
        game::paddle_system(world, input, DT);
    }

    // Half the paddle width, not zero.
    EXPECT_NEAR(world.transforms.find(paddle)->position.x, 60.0f, TOLERANCE);
}

TEST(CollisionSystem, BouncesOffTheWallsAndCeiling)
{
    game::World world = make_world();

    const core::Entity left = add_ball(world, math::Vec2{4.0f, 300.0f}, math::Vec2{-200.0f, 0.0f});
    const core::Entity right = add_ball(world, math::Vec2{797.0f, 300.0f}, math::Vec2{200.0f, 0.0f});
    const core::Entity top = add_ball(world, math::Vec2{400.0f, 596.0f}, math::Vec2{0.0f, 200.0f});

    game::collision_system(world);

    EXPECT_GT(world.velocities.find(left)->value.x, 0.0f) << "left wall";
    EXPECT_GE(world.transforms.find(left)->position.x, 8.0f) << "left wall did not push out";

    EXPECT_LT(world.velocities.find(right)->value.x, 0.0f) << "right wall";
    EXPECT_LT(world.velocities.find(top)->value.y, 0.0f) << "ceiling";
}

TEST(CollisionSystem, ThePaddleAimsTheBallByWhereItWasHit)
{
    game::World world = make_world();
    add_paddle(world, math::Vec2{400.0f, 40.0f});

    const core::Entity centre = add_ball(world, math::Vec2{400.0f, 55.0f}, math::Vec2{0.0f, -300.0f});
    const core::Entity edge = add_ball(world, math::Vec2{455.0f, 55.0f}, math::Vec2{0.0f, -300.0f});

    game::collision_system(world);

    const math::Vec2 from_centre = world.velocities.find(centre)->value;
    EXPECT_NEAR(from_centre.x, 0.0f, 1.0f);
    EXPECT_GT(from_centre.y, 0.0f);
    EXPECT_NEAR(math::length(from_centre), 300.0f, 1e-2f) << "speed was not preserved";

    const math::Vec2 from_edge = world.velocities.find(edge)->value;
    EXPECT_GT(from_edge.x, 0.0f) << "a hit right of centre must go right";
    EXPECT_GT(from_edge.y, 0.0f) << "and never downward";
}

TEST(CollisionSystem, HittingABrickReflectsAndDamagesIt)
{
    game::World world = make_world();
    const core::Entity brick = add_brick(world, math::Vec2{400.0f, 500.0f}, 2);
    const core::Entity ball = add_ball(world, math::Vec2{400.0f, 482.0f}, math::Vec2{0.0f, 200.0f});

    game::collision_system(world);

    EXPECT_LT(world.velocities.find(ball)->value.y, 0.0f);
    EXPECT_EQ(world.bricks.find(brick)->hit_points, 1);
    EXPECT_TRUE(game::is_alive(world, brick));
}

TEST(Step, ABrickWithOneHitPointIsGoneAfterTheStep)
{
    game::World world = make_world();
    const core::Entity brick = add_brick(world, math::Vec2{400.0f, 500.0f}, 1);
    add_ball(world, math::Vec2{400.0f, 482.0f}, math::Vec2{0.0f, 200.0f});

    game::step(world, game::Input{}, DT);

    EXPECT_FALSE(world.bricks.has(brick));
    EXPECT_FALSE(world.transforms.has(brick));
}

TEST(Step, IsDeterministic)
{
    auto run = [](int steps) {
        game::World world = make_world();
        add_paddle(world, math::Vec2{400.0f, 40.0f});
        const core::Entity ball = add_ball(world, math::Vec2{400.0f, 300.0f}, math::Vec2{213.0f, -317.0f});

        game::Input input;
        input.move_right = true;
        for (int i = 0; i < steps; i++)
        {
            game::step(world, input, DT);
        }
        return world.transforms.find(ball)->position;
    };

    const math::Vec2 first = run(600);
    const math::Vec2 second = run(600);

    EXPECT_FLOAT_EQ(first.x, second.x);
    EXPECT_FLOAT_EQ(first.y, second.y);
}

TEST(Lifecycle, FallingPastTheFloorCostsALifeAndReturnsToReady)
{
    game::World world = make_world();
    add_paddle(world, math::Vec2{400.0f, 40.0f});
    const core::Entity ball = add_ball(world, math::Vec2{400.0f, -20.0f}, math::Vec2{0.0f, -200.0f});

    const int lives_before = world.lives;
    game::lifecycle_system(world);

    EXPECT_EQ(world.lives, lives_before - 1);
    EXPECT_EQ(world.state, game::GameState::Ready);

    // The ball is held; serve_system parks it back on the paddle.
    EXPECT_FLOAT_EQ(math::length(world.velocities.find(ball)->value), 0.0f);
}

TEST(Lifecycle, LosingTheLastLifeIsGameOver)
{
    game::World world = make_world();
    world.lives = 1;
    add_paddle(world, math::Vec2{400.0f, 40.0f});
    add_ball(world, math::Vec2{400.0f, -20.0f}, math::Vec2{0.0f, -200.0f});

    game::lifecycle_system(world);

    EXPECT_EQ(world.lives, 0);
    EXPECT_EQ(world.state, game::GameState::GameOver);
}

TEST(Lifecycle, ABallStillInPlayCostsNothing)
{
    game::World world = make_world();
    add_ball(world, math::Vec2{400.0f, 300.0f}, math::Vec2{0.0f, -200.0f});

    game::lifecycle_system(world);

    EXPECT_EQ(world.lives, game::STARTING_LIVES);
}

TEST(Scoring, DamagingABrickRecolorsItToTheRemainingToughness)
{
    game::World world = make_world();
    const core::Entity brick = add_brick(world, math::Vec2{400.0f, 500.0f}, 3);
    world.colors.add(brick, game::brick_color(3));
    add_ball(world, math::Vec2{400.0f, 482.0f}, math::Vec2{0.0f, 200.0f});

    game::collision_system(world);

    ASSERT_EQ(world.bricks.find(brick)->hit_points, 2);

    const core::Color expected = game::brick_color(2);
    const core::Color* actual = world.colors.find(brick);
    ASSERT_NE(actual, nullptr);
    EXPECT_FLOAT_EQ(actual->r, expected.r);
    EXPECT_FLOAT_EQ(actual->g, expected.g);
    EXPECT_FLOAT_EQ(actual->b, expected.b);
}

TEST(Scoring, EachHitScores)
{
    game::World world = make_world();
    add_brick(world, math::Vec2{400.0f, 500.0f}, 2);
    add_ball(world, math::Vec2{400.0f, 482.0f}, math::Vec2{0.0f, 200.0f});

    EXPECT_EQ(world.score, 0);
    game::collision_system(world);
    EXPECT_GT(world.score, 0);
}
