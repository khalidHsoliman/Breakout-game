#include <gtest/gtest.h>

#include <algorithm>

#include "game/Spawn.h"
#include "game/Systems.h"

namespace
{
    constexpr math::Vec2 WORLD{800.0f, 600.0f};
    constexpr float DT = 1.0f / 60.0f;

    game::World make_world()
    {
        game::World world;
        world.size = WORLD;
        return world;
    }

    core::Entity add_ball(game::World& world, math::Vec2 position, math::Vec2 velocity)
    {
        const core::Entity entity = world.create_entity();
        world.transforms.add(entity, game::Transform{position, math::Vec2{16.0f, 16.0f}, {}});
        world.velocities.add(entity, game::Velocity{velocity});
        world.balls.add(entity, game::Ball{8.0f});
        return entity;
    }

    core::Entity add_brick(game::World& world, math::Vec2 position, int hit_points)
    {
        const core::Entity entity = world.create_entity();
        world.transforms.add(entity, game::Transform{position, math::Vec2{60.0f, 24.0f}, {}});
        world.colors.add(entity, game::brick_color(hit_points));
        world.bricks.add(entity, game::Brick{hit_points});
        return entity;
    }

    core::Entity add_paddle(game::World& world, math::Vec2 position)
    {
        const core::Entity entity = world.create_entity();
        world.transforms.add(entity, game::Transform{position, math::Vec2{120.0f, 20.0f}, {}});
        world.paddles.add(entity, game::Paddle{520.0f});
        return entity;
    }

    int count(const game::World& world, game::GameEvent event)
    {
        return static_cast<int>(std::count(world.events.begin(), world.events.end(), event));
    }
}

TEST(Events, AQuietStepReportsNothing)
{
    game::World world = make_world();
    add_ball(world, math::Vec2{400.0f, 300.0f}, math::Vec2{0.0f, 60.0f});

    game::collision_system(world);

    EXPECT_TRUE(world.events.empty());
}

TEST(Events, HittingABrickIsReported)
{
    game::World world = make_world();
    add_brick(world, math::Vec2{400.0f, 500.0f}, 1);
    add_ball(world, math::Vec2{400.0f, 490.0f}, math::Vec2{0.0f, 200.0f});

    game::collision_system(world);

    EXPECT_EQ(count(world, game::GameEvent::BrickHit), 1);
    EXPECT_EQ(count(world, game::GameEvent::PaddleBounce), 0);
}

TEST(Events, ABrickThatSurvivesIsStillReported)
{
    game::World world = make_world();
    const core::Entity brick = add_brick(world, math::Vec2{400.0f, 500.0f}, 3);
    add_ball(world, math::Vec2{400.0f, 490.0f}, math::Vec2{0.0f, 200.0f});

    game::collision_system(world);

    EXPECT_EQ(count(world, game::GameEvent::BrickHit), 1);
    EXPECT_EQ(world.bricks.find(brick)->hit_points, 2);
    EXPECT_TRUE(game::is_alive(world, brick));
}

TEST(Events, BouncingOffThePaddleIsReported)
{
    game::World world = make_world();
    add_paddle(world, math::Vec2{400.0f, 40.0f});
    add_ball(world, math::Vec2{400.0f, 55.0f}, math::Vec2{0.0f, -200.0f});

    game::collision_system(world);

    EXPECT_EQ(count(world, game::GameEvent::PaddleBounce), 1);
    EXPECT_EQ(count(world, game::GameEvent::BrickHit), 0);
}

TEST(Events, RestartingClearsThem)
{
    game::World world = make_world();
    add_brick(world, math::Vec2{400.0f, 500.0f}, 1);
    add_ball(world, math::Vec2{400.0f, 490.0f}, math::Vec2{0.0f, 200.0f});

    game::collision_system(world);
    ASSERT_FALSE(world.events.empty());

    game::start_game(world, game::parse_level("11"));

    EXPECT_TRUE(world.events.empty());
}
