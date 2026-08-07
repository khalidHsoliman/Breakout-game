#include <gtest/gtest.h>

#include "game/Spawn.h"
#include "game/Systems.h"

namespace
{
    constexpr float dt = 1.0f / 60.0f;
    constexpr float tolerance = 1e-4f;

    game::World make_world()
    {
        game::World world;
        world.size = math::Vec2{800.0f, 600.0f};
        return world;
    }

    core::Entity add_ball(game::World& world, math::Vec2 position, math::Vec2 velocity, float radius = 8.0f)
    {
        const core::Entity entity = world.create_entity();
        world.transforms.add(entity, game::Transform{position, math::Vec2{radius * 2.0f, radius * 2.0f}});
        world.velocities.add(entity, game::Velocity{velocity});
        world.balls.add(entity, game::Ball{radius});
        return entity;
    }

    core::Entity add_paddle(game::World& world, math::Vec2 position, float speed = 500.0f)
    {
        const core::Entity entity = world.create_entity();
        world.transforms.add(entity, game::Transform{position, math::Vec2{120.0f, 20.0f}});
        world.paddles.add(entity, game::Paddle{speed});
        return entity;
    }

    core::Entity add_brick(game::World& world, math::Vec2 position, int hit_points = 1)
    {
        const core::Entity entity = world.create_entity();
        world.transforms.add(entity, game::Transform{position, math::Vec2{90.0f, 30.0f}});
        world.bricks.add(entity, game::Brick{hit_points});
        return entity;
    }
}

TEST(MovementSystem, IntegratesVelocity)
{
    game::World world = make_world();
    const core::Entity ball = add_ball(world, math::Vec2{100.0f, 100.0f}, math::Vec2{60.0f, 120.0f});

    game::movement_system(world, dt);

    EXPECT_NEAR(world.transforms.find(ball)->position.x, 101.0f, tolerance);
    EXPECT_NEAR(world.transforms.find(ball)->position.y, 102.0f, tolerance);
}

TEST(MovementSystem, LeavesEntitiesWithoutVelocityAlone)
{
    game::World world = make_world();
    const core::Entity brick = add_brick(world, math::Vec2{400.0f, 500.0f});

    game::movement_system(world, dt);

    EXPECT_NEAR(world.transforms.find(brick)->position.x, 400.0f, tolerance);
}

TEST(PaddleSystem, MovesLeftAndRight)
{
    game::World world = make_world();
    const core::Entity paddle = add_paddle(world, math::Vec2{400.0f, 40.0f}, 600.0f);

    game::Input input;
    input.move_right = true;
    game::paddle_system(world, input, dt);
    EXPECT_NEAR(world.transforms.find(paddle)->position.x, 410.0f, tolerance);

    input = game::Input{};
    input.move_left = true;
    game::paddle_system(world, input, dt);
    EXPECT_NEAR(world.transforms.find(paddle)->position.x, 400.0f, tolerance);
}

TEST(PaddleSystem, StaysInsideThePlayArea)
{
    game::World world = make_world();
    const core::Entity paddle = add_paddle(world, math::Vec2{40.0f, 40.0f}, 600.0f);

    game::Input input;
    input.move_left = true;
    for (int i = 0; i < 60; ++i)
    {
        game::paddle_system(world, input, dt);
    }

    // Half the paddle width, not zero.
    EXPECT_NEAR(world.transforms.find(paddle)->position.x, 60.0f, tolerance);
}

TEST(CollisionSystem, BouncesOffTheLeftWall)
{
    game::World world = make_world();
    const core::Entity ball = add_ball(world, math::Vec2{4.0f, 300.0f}, math::Vec2{-200.0f, 0.0f});

    game::collision_system(world);

    EXPECT_GT(world.velocities.find(ball)->value.x, 0.0f);
    EXPECT_GE(world.transforms.find(ball)->position.x, 8.0f);
}

TEST(CollisionSystem, BouncesOffTheRightWall)
{
    game::World world = make_world();
    const core::Entity ball = add_ball(world, math::Vec2{797.0f, 300.0f}, math::Vec2{200.0f, 0.0f});

    game::collision_system(world);

    EXPECT_LT(world.velocities.find(ball)->value.x, 0.0f);
}

TEST(CollisionSystem, BouncesOffTheCeiling)
{
    game::World world = make_world();
    const core::Entity ball = add_ball(world, math::Vec2{400.0f, 596.0f}, math::Vec2{0.0f, 200.0f});

    game::collision_system(world);

    EXPECT_LT(world.velocities.find(ball)->value.y, 0.0f);
}

TEST(CollisionSystem, PaddleCentreSendsTheBallStraightUp)
{
    game::World world = make_world();
    add_paddle(world, math::Vec2{400.0f, 40.0f});
    const core::Entity ball = add_ball(world, math::Vec2{400.0f, 55.0f}, math::Vec2{50.0f, -200.0f});

    game::collision_system(world);

    const math::Vec2 velocity = world.velocities.find(ball)->value;
    EXPECT_NEAR(velocity.x, 0.0f, 1e-3f);
    EXPECT_GT(velocity.y, 0.0f);
}

TEST(CollisionSystem, PaddleEdgeSendsTheBallSideways)
{
    game::World world = make_world();
    add_paddle(world, math::Vec2{400.0f, 40.0f});
    const core::Entity ball = add_ball(world, math::Vec2{455.0f, 55.0f}, math::Vec2{0.0f, -200.0f});

    game::collision_system(world);

    const math::Vec2 velocity = world.velocities.find(ball)->value;
    EXPECT_GT(velocity.x, 0.0f);
    EXPECT_GT(velocity.y, 0.0f);
}

TEST(CollisionSystem, PaddleBouncePreservesSpeed)
{
    game::World world = make_world();
    add_paddle(world, math::Vec2{400.0f, 40.0f});
    const core::Entity ball = add_ball(world, math::Vec2{430.0f, 55.0f}, math::Vec2{120.0f, -160.0f});

    game::collision_system(world);

    EXPECT_NEAR(math::length(world.velocities.find(ball)->value), 200.0f, 1e-2f);
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

    game::step(world, game::Input{}, dt);

    EXPECT_FALSE(world.bricks.has(brick));
    EXPECT_FALSE(world.transforms.has(brick));
}

TEST(Step, ADestroyedBrickStopsCollidingWithinTheSameStep)
{
    game::World world = make_world();
    const core::Entity brick = add_brick(world, math::Vec2{400.0f, 500.0f}, 1);
    add_ball(world, math::Vec2{400.0f, 482.0f}, math::Vec2{0.0f, 200.0f});

    game::step(world, game::Input{}, dt);
    // A second step must not find the brick again.
    game::step(world, game::Input{}, dt);

    EXPECT_EQ(world.bricks.size(), 0u);
}

// The property the whole fixed-timestep design exists for: the same inputs
// produce the same result, every run.
TEST(Step, IsDeterministic)
{
    auto run = [](int steps) {
        game::World world = make_world();
        add_paddle(world, math::Vec2{400.0f, 40.0f});
        const core::Entity ball = add_ball(world, math::Vec2{400.0f, 300.0f}, math::Vec2{213.0f, -317.0f});

        game::Input input;
        input.move_right = true;
        for (int i = 0; i < steps; ++i)
        {
            game::step(world, input, dt);
        }
        return world.transforms.find(ball)->position;
    };

    const math::Vec2 first = run(600);
    const math::Vec2 second = run(600);

    EXPECT_FLOAT_EQ(first.x, second.x);
    EXPECT_FLOAT_EQ(first.y, second.y);
}

// Ten seconds of play must never let the ball out of the play area, whether it
// is bouncing or being reset after a miss.
TEST(Step, BallNeverEscapesThePlayArea)
{
    game::World world = make_world();
    add_paddle(world, math::Vec2{400.0f, 40.0f});
    const core::Entity ball = add_ball(world, math::Vec2{400.0f, 300.0f}, math::Vec2{213.0f, -317.0f});

    for (int i = 0; i < 600; ++i)
    {
        game::step(world, game::Input{}, dt);

        const math::Vec2 position = world.transforms.find(ball)->position;
        ASSERT_GE(position.x, 0.0f) << "escaped left at step " << i;
        ASSERT_LE(position.x, world.size.x) << "escaped right at step " << i;
        ASSERT_LE(position.y, world.size.y) << "escaped top at step " << i;
    }
}

// Reflection off walls and the ceiling must not let the ball gain or bleed
// energy. Aimed upward so it never reaches the floor and gets reset.
TEST(Step, BouncingOffWallsPreservesSpeed)
{
    game::World world = make_world();
    const core::Entity ball = add_ball(world, math::Vec2{400.0f, 300.0f}, math::Vec2{280.0f, 210.0f});

    const float initial_speed = math::length(world.velocities.find(ball)->value);

    for (int i = 0; i < 120; ++i)
    {
        game::step(world, game::Input{}, dt);
    }

    EXPECT_NEAR(math::length(world.velocities.find(ball)->value), initial_speed, 1e-2f);
}

TEST(Lifecycle, FallingPastTheFloorCostsALifeAndResetsTheBall)
{
    game::World world = make_world();
    add_paddle(world, math::Vec2{400.0f, 40.0f});
    const core::Entity ball = add_ball(world, math::Vec2{400.0f, -20.0f}, math::Vec2{0.0f, -200.0f});

    const int lives_before = world.lives;
    game::lifecycle_system(world);

    EXPECT_EQ(world.lives, lives_before - 1);
    EXPECT_GT(world.transforms.find(ball)->position.y, 0.0f);
    EXPECT_GT(world.velocities.find(ball)->value.y, 0.0f);
}

TEST(Lifecycle, ABallStillInPlayCostsNothing)
{
    game::World world = make_world();
    add_ball(world, math::Vec2{400.0f, 300.0f}, math::Vec2{0.0f, -200.0f});

    game::lifecycle_system(world);

    EXPECT_EQ(world.lives, game::starting_lives);
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
