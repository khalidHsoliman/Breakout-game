#include <gtest/gtest.h>

#include "game/Spawn.h"
#include "game/Systems.h"
#include "math/Collision.h"

namespace
{
    constexpr math::Vec2 WORLD{800.0f, 600.0f};
    constexpr float TOLERANCE = 1e-3f;

    game::World make_world()
    {
        game::World world;
        world.size = WORLD;
        return world;
    }

    core::Entity add_ball(game::World& world, math::Vec2 position, math::Vec2 velocity)
    {
        const core::Entity entity = world.create_entity();
        world.transforms.add(entity, game::Transform{position, math::Vec2{16.0f, 16.0f}});
        world.velocities.add(entity, game::Velocity{velocity});
        world.balls.add(entity, game::Ball{8.0f});
        return entity;
    }

    core::Entity add_brick(game::World& world, math::Vec2 position, int hit_points)
    {
        const core::Entity entity = world.create_entity();
        world.transforms.add(entity, game::Transform{position, math::Vec2{60.0f, 24.0f}});
        world.colors.add(entity, game::brick_color(hit_points));
        world.bricks.add(entity, game::Brick{hit_points});
        return entity;
    }

    float speed_of(const game::World& world, core::Entity ball)
    {
        return math::length(world.velocities.find(ball)->value);
    }
}

TEST(BallSpeed, RisesOnABrickHit)
{
    game::World world = make_world();
    add_brick(world, math::Vec2{400.0f, 500.0f}, 1);
    const core::Entity ball = add_ball(world, math::Vec2{400.0f, 490.0f}, math::Vec2{0.0f, 200.0f});

    game::collision_system(world);

    EXPECT_NEAR(speed_of(world, ball), 200.0f + game::BALL_SPEED_PER_HIT, TOLERANCE);
}

TEST(BallSpeed, ThePaddleDoesNotSpeedItUp)
{
    game::World world = make_world();

    const core::Entity paddle = world.create_entity();
    world.transforms.add(paddle, game::Transform{math::Vec2{400.0f, 40.0f}, math::Vec2{120.0f, 20.0f}});
    world.paddles.add(paddle, game::Paddle{520.0f});

    const core::Entity ball = add_ball(world, math::Vec2{400.0f, 55.0f}, math::Vec2{0.0f, -200.0f});

    game::collision_system(world);

    EXPECT_NEAR(speed_of(world, ball), 200.0f, TOLERANCE);
}

TEST(BallSpeed, IsCappedHoweverManyBricksAreHit)
{
    game::World world = make_world();
    const core::Entity ball = add_ball(world, math::Vec2{400.0f, 490.0f}, math::Vec2{0.0f, 200.0f});

    for (int hit = 0; hit < 500; ++hit)
    {
        // A fresh brick under the ball every time, so the loop is 500 hits
        // rather than 500 attempts at the same dead brick.
        add_brick(world, world.transforms.find(ball)->position + math::Vec2{0.0f, 10.0f}, 9);
        game::collision_system(world);
        game::sweep_destroyed(world);

        ASSERT_LE(speed_of(world, ball), game::MAX_BALL_SPEED + TOLERANCE) << "hit " << hit;
    }

    EXPECT_NEAR(speed_of(world, ball), game::MAX_BALL_SPEED, TOLERANCE) << "the cap was never reached";
}

TEST(BallSpeed, TheCapCannotCrossABrickInOneStep)
{
    EXPECT_LT(game::MAX_BALL_SPEED * game::FIXED_DT, game::BRICK_HEIGHT);
}

TEST(BallSpeed, StartsOverAfterARestart)
{
    game::World world = make_world();
    game::start_game(world, game::parse_level("1111111111"));

    const core::Entity ball = world.balls.entities()[0];
    world.velocities.find(ball)->value = math::Vec2{0.0f, game::MAX_BALL_SPEED};

    game::start_game(world, game::parse_level("1111111111"));

    const core::Entity fresh = world.balls.entities()[0];
    game::launch_ball(world, fresh);

    EXPECT_LT(speed_of(world, fresh), game::MAX_BALL_SPEED);
}
