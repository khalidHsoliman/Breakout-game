#include "game/Spawn.h"

namespace game
{
    namespace
    {
        constexpr float PADDLE_WIDTH = 120.0f;
        constexpr float PADDLE_HEIGHT = 20.0f;
        constexpr float PADDLE_Y = 40.0f;
        constexpr float PADDLE_SPEED = 520.0f;

        constexpr float BALL_RADIUS = 8.0f;
        constexpr float BALL_SPEED = 350.0f;
        constexpr float BALL_START_Y = 120.0f;

        constexpr float BRICK_SIDE_MARGIN = 20.0f;
        constexpr float BRICK_TOP_MARGIN = 40.0f;
        constexpr float BRICK_HEIGHT = 24.0f;
        constexpr float BRICK_GAP = 4.0f;

    }

    // Colour carries information here rather than decoration: the player can
    // read a level at a glance and see which bricks are tough.
    core::Color brick_color(int hit_points)
    {
        switch (hit_points)
        {
        case 1:
            return core::Color{0.95f, 0.82f, 0.30f};
        case 2:
            return core::Color{0.95f, 0.58f, 0.25f};
        case 3:
            return core::Color{0.88f, 0.32f, 0.32f};
        case 4:
            return core::Color{0.72f, 0.38f, 0.85f};
        default:
            return core::Color{0.55f, 0.68f, 0.82f};
        }
    }

    core::Entity spawn_paddle(World& world)
    {
        const core::Entity entity = world.create_entity();

        world.transforms.add(entity, Transform{math::Vec2{world.size.x * 0.5f, PADDLE_Y},
                                               math::Vec2{PADDLE_WIDTH, PADDLE_HEIGHT}});
        world.colors.add(entity, core::Color{0.90f, 0.90f, 0.90f});
        world.paddles.add(entity, Paddle{PADDLE_SPEED});

        return entity;
    }

    core::Entity spawn_ball(World& world)
    {
        const core::Entity entity = world.create_entity();

        world.transforms.add(entity, Transform{math::Vec2{},
                                               math::Vec2{BALL_RADIUS * 2.0f, BALL_RADIUS * 2.0f}});
        world.colors.add(entity, core::Color{0.95f, 0.65f, 0.25f});
        world.circle_shapes.add(entity, CircleShape{});
        world.balls.add(entity, Ball{BALL_RADIUS});

        // Starts held, with no velocity. serve_system parks it on the paddle
        // until the player launches.
        world.velocities.add(entity, Velocity{});
        world.transforms.find(entity)->position =
            math::Vec2{world.size.x * 0.5f, BALL_START_Y};

        return entity;
    }

    void launch_ball(World& world, core::Entity ball)
    {
        Velocity* velocity = world.velocities.find(ball);
        if (velocity == nullptr)
        {
            return;
        }

        // Off vertical, so the opening shot is not a straight up-down bounce.
        velocity->value = math::normalize(math::Vec2{0.45f, 1.0f}) * BALL_SPEED;
    }

    void start_game(World& world, const Level& level)
    {
        // Copied first: destroy_entity mutates the store being read. Every
        // entity in this game has a Transform, so this reaches all of them.
        const std::vector<core::Entity> all = world.transforms.entities();
        for (const core::Entity entity : all)
        {
            world.destroy_entity(entity);
        }

        world.level = level;
        world.score = 0;
        world.lives = STARTING_LIVES;
        world.state = GameState::Ready;

        spawn_paddle(world);
        spawn_ball(world);
        spawn_level(world, world.level);
    }

    void spawn_level(World& world, const Level& level)
    {
        if (level.columns <= 0 || level.rows <= 0)
        {
            return;
        }

        const float usable_width = world.size.x - 2.0f * BRICK_SIDE_MARGIN;
        const float cell_width = usable_width / static_cast<float>(level.columns);
        const float cell_height = BRICK_HEIGHT + BRICK_GAP;

        for (int row = 0; row < level.rows; ++row)
        {
            for (int column = 0; column < level.columns; ++column)
            {
                const int hit_points = level.at(column, row);
                if (hit_points <= 0)
                {
                    continue;
                }

                const float x = BRICK_SIDE_MARGIN + (static_cast<float>(column) + 0.5f) * cell_width;
                const float y = world.size.y - BRICK_TOP_MARGIN -
                                (static_cast<float>(row) + 0.5f) * cell_height;

                const core::Entity entity = world.create_entity();

                world.transforms.add(entity, Transform{math::Vec2{x, y},
                                                       math::Vec2{cell_width - BRICK_GAP, BRICK_HEIGHT}});
                world.colors.add(entity, brick_color(hit_points));
                world.bricks.add(entity, Brick{hit_points});
            }
        }
    }
}
