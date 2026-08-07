#include "game/Spawn.h"

namespace game
{
    namespace
    {
        constexpr float paddle_width = 120.0f;
        constexpr float paddle_height = 20.0f;
        constexpr float paddle_y = 40.0f;
        constexpr float paddle_speed = 520.0f;

        constexpr float ball_radius = 8.0f;
        constexpr float ball_speed = 350.0f;
        constexpr float ball_start_y = 120.0f;

        constexpr float brick_side_margin = 20.0f;
        constexpr float brick_top_margin = 40.0f;
        constexpr float brick_height = 24.0f;
        constexpr float brick_gap = 4.0f;

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

        world.transforms.add(entity, Transform{math::Vec2{world.size.x * 0.5f, paddle_y},
                                               math::Vec2{paddle_width, paddle_height}});
        world.colors.add(entity, core::Color{0.90f, 0.90f, 0.90f});
        world.paddles.add(entity, Paddle{paddle_speed});

        return entity;
    }

    core::Entity spawn_ball(World& world)
    {
        const core::Entity entity = world.create_entity();

        world.transforms.add(entity, Transform{math::Vec2{},
                                               math::Vec2{ball_radius * 2.0f, ball_radius * 2.0f}});
        world.colors.add(entity, core::Color{0.95f, 0.65f, 0.25f});
        world.circle_shapes.add(entity, CircleShape{});
        world.balls.add(entity, Ball{ball_radius});

        // Starts held, with no velocity. serve_system parks it on the paddle
        // until the player launches.
        world.velocities.add(entity, Velocity{});
        world.transforms.find(entity)->position =
            math::Vec2{world.size.x * 0.5f, ball_start_y};

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
        velocity->value = math::normalize(math::Vec2{0.45f, 1.0f}) * ball_speed;
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
        world.lives = starting_lives;
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

        const float usable_width = world.size.x - 2.0f * brick_side_margin;
        const float cell_width = usable_width / static_cast<float>(level.columns);
        const float cell_height = brick_height + brick_gap;

        for (int row = 0; row < level.rows; ++row)
        {
            for (int column = 0; column < level.columns; ++column)
            {
                const int hit_points = level.at(column, row);
                if (hit_points <= 0)
                {
                    continue;
                }

                const float x = brick_side_margin + (static_cast<float>(column) + 0.5f) * cell_width;
                const float y = world.size.y - brick_top_margin -
                                (static_cast<float>(row) + 0.5f) * cell_height;

                const core::Entity entity = world.create_entity();

                world.transforms.add(entity, Transform{math::Vec2{x, y},
                                                       math::Vec2{cell_width - brick_gap, brick_height}});
                world.colors.add(entity, brick_color(hit_points));
                world.bricks.add(entity, Brick{hit_points});
            }
        }
    }
}
