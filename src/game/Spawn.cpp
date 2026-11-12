#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <utility>
#include <vector>

#include "game/LevelLayout.h"
#include "math/Lerp.h"
#include "game/Spawn.h"

namespace game
{
    namespace
    {
        // Particles
        constexpr float TRAIL_SIZE = 7.0f;
        constexpr float TRAIL_LIFE = 0.15f;

        constexpr int PARTICLES_PER_BRICK = 10;
        constexpr float PARTICLE_MIN_SIZE = 1.0f;
        constexpr float PARTICLE_MAX_SIZE = 4.0f;
        constexpr float PARTICLE_MIN_SPEED = 40.0f;
        constexpr float PARTICLE_MAX_SPEED = 190.0f;
        constexpr float PARTICLE_MIN_LIFE = 0.35f;
        constexpr float PARTICLE_MAX_LIFE = 0.85f;

        float random_range(float low, float high)
        {
            return math::lerp(low, high, static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX));
        }
    }

    // Colour carries information here rather than decoration: the player can
    // read a level at a glance and see which bricks are tough.
    core::Color brick_color(int hit_points)
    {
        switch (hit_points)
        {
        case STEEL:
            return core::Color{0.50f, 0.52f, 0.56f};
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

    core::Color capsule_color(PowerUp kind)
    {
        switch (kind)
        {
        case PowerUp::WidePaddle:
            return core::Color{0.15f, 0.40f, 0.95f};
        case PowerUp::MultiBall:
            return core::Color{0.25f, 0.85f, 0.35f};
        }
        return core::Color{};
    }

    core::Entity spawn_paddle(World& world)
    {
        const core::Entity entity = world.create_entity();
        const math::Vec2 position{world.size.x * 0.5f, PADDLE_Y};

        world.transforms.add(
            entity,
            Transform{position, math::Vec2{PADDLE_WIDTH, PADDLE_HEIGHT}, position}
        );
        world.colors.add(entity, core::Color{0.90f, 0.90f, 0.90f});
        world.paddles.add(entity, Paddle{PADDLE_SPEED});

        return entity;
    }

    core::Entity spawn_ball(World& world)
    {
        const core::Entity entity = world.create_entity();
        const math::Vec2 position{world.size.x * 0.5f, BALL_START_Y};

        world.transforms.add(
            entity,
            Transform{position, math::Vec2{BALL_RADIUS * 2.0f, BALL_RADIUS * 2.0f}, position}
        );
        world.colors.add(entity, core::Color{0.95f, 0.65f, 0.25f});
        world.circle_shapes.add(entity, CircleShape{});
        world.balls.add(entity, Ball{BALL_RADIUS});

        // Starts held, with no velocity. serve_system parks it on the paddle
        // until the player launches.
        world.velocities.add(entity, Velocity{});

        return entity;
    }

    core::Entity spawn_capsule(World& world, math::Vec2 position, PowerUp kind)
    {
        const core::Entity entity = world.create_entity();

        world.transforms.add(
            entity,
            Transform{position, math::Vec2{CAPSULE_WIDTH, CAPSULE_HEIGHT}, position}
        );
        world.colors.add(entity, capsule_color(kind));
        world.velocities.add(entity, Velocity{math::Vec2{0.0f, -CAPSULE_SPEED}});
        world.capsules.add(entity, Capsule{kind});

        return entity;
    }

    void spawn_brick_particles(World& world, math::Vec2 center, math::Vec2 size, core::Color color)
    {
        for (int i = 0; i < PARTICLES_PER_BRICK; i++)
        {
            const core::Entity entity = world.create_entity();

            const math::Vec2 position{
                center.x + random_range(-0.5f, 0.5f) * size.x,
                center.y + random_range(-0.5f, 0.5f) * size.y
            };

            const float p_angle = random_range(0.0f, 2.0f * std::numbers::pi_v<float>);
            const float p_size = random_range(PARTICLE_MIN_SIZE, PARTICLE_MAX_SIZE);
            const float p_speed = random_range(PARTICLE_MIN_SPEED, PARTICLE_MAX_SPEED);
            const float p_life = random_range(PARTICLE_MIN_LIFE, PARTICLE_MAX_LIFE);

            world.transforms.add(entity, Transform{position, math::Vec2{p_size, p_size}, position});
            world.velocities.add(entity, Velocity{math::Vec2{std::cos(p_angle), std::sin(p_angle)} * p_speed});
            world.colors.add(entity, color);
            world.particles.add(entity, Particle{p_life, p_life});
        }
    }

    void spawn_ball_trail(World& world, math::Vec2 position, core::Color color)
    {
        const core::Entity entity = world.create_entity();

        world.transforms.add(entity, Transform{position, math::Vec2{TRAIL_SIZE, TRAIL_SIZE}, position});
        world.colors.add(entity, color);
        world.particles.add(entity, Particle{TRAIL_LIFE, TRAIL_LIFE});

        // Still gets one, particle_system updates it
        world.velocities.add(entity, Velocity{});
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

    void start_game(World& world, std::vector<Level> levels)
    {
        world.levels = std::move(levels);
        world.level_index = 0;
        world.score = 0;
        world.lives = STARTING_LIVES;
        world.events.clear();

        start_level(world);
    }

    void start_game(World& world, const Level& level)
    {
        start_game(world, std::vector<Level>{level});
    }

    void start_level(World& world)
    {
        // Copied first: destroy_entity mutates the store being read. Every
        // entity in this game has a Transform, so this reaches all of them.
        const std::vector<core::Entity> all = world.transforms.entities();
        for (const core::Entity entity : all)
        {
            world.destroy_entity(entity);
        }

        const bool in_range =
            world.level_index >= 0 && world.level_index < static_cast<int>(world.levels.size());
        world.level = in_range ? world.levels[static_cast<std::size_t>(world.level_index)] : Level{};
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

        const math::Vec2 size = brick_size(level, world.size);

        for (int row = 0; row < level.rows; ++row)
        {
            for (int column = 0; column < level.columns; ++column)
            {
                const int hit_points = level.at(column, row);
                const bool steel = hit_points == STEEL;
                if (hit_points <= 0 && !steel)
                {
                    continue;
                }

                const core::Entity entity = world.create_entity();
                const math::Vec2 center = brick_center(level, world.size, column, row);

                world.transforms.add(entity, Transform{center, size, center});
                world.colors.add(entity, brick_color(hit_points));

                if (steel)
                {
                    world.steel.add(entity, Steel{});
                }
                else
                {
                    world.bricks.add(entity, Brick{hit_points});
                }
            }
        }
    }
}
