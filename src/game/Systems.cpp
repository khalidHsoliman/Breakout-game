#include <algorithm>
#include <cstddef>
#include <numbers>
#include <vector>

#include "math/Collision.h"

#include "game/Spawn.h"

#include "game/Systems.h"

namespace game
{
    namespace
    {
        // Pushed a fraction further than the overlap, so floating point cannot
        // leave the ball a hair inside and re-trigger the same contact.
        constexpr float push_out_tolerance = 0.01f;

        constexpr float max_bounce_angle = std::numbers::pi_v<float> / 3.0f;

        constexpr int points_per_hit = 10;

        math::AABB bounds_of(const Transform& transform)
        {
            const math::Vec2 half = transform.size * 0.5f;
            return math::AABB{transform.position - half, transform.position + half};
        }

        void bounce_off_edges(const World& world, Transform& transform, Velocity& velocity, float radius)
        {
            if (transform.position.x - radius < 0.0f)
            {
                transform.position.x = radius + push_out_tolerance;
                velocity.value = math::reflect(velocity.value, math::Vec2{1.0f, 0.0f});
            }
            else if (transform.position.x + radius > world.size.x)
            {
                transform.position.x = world.size.x - radius - push_out_tolerance;
                velocity.value = math::reflect(velocity.value, math::Vec2{-1.0f, 0.0f});
            }

            if (transform.position.y + radius > world.size.y)
            {
                transform.position.y = world.size.y - radius - push_out_tolerance;
                velocity.value = math::reflect(velocity.value, math::Vec2{0.0f, -1.0f});
            }

            // No floor: falling past the bottom costs a life, which is
            // lifecycle_system's job rather than a collision.
        }

        void bounce_off_paddles(const World& world, Transform& transform, Velocity& velocity, float radius)
        {
            const math::Circle ball{transform.position, radius};
            const std::vector<core::Entity>& entities = world.paddles.entities();

            for (std::size_t i = 0; i < world.paddles.size(); ++i)
            {
                const Transform* paddle = world.transforms.find(entities[i]);
                if (paddle == nullptr)
                {
                    continue;
                }

                const math::Contact contact = math::circle_vs_aabb(ball, bounds_of(*paddle));
                if (!contact.hit)
                {
                    continue;
                }

                transform.position += contact.normal * (contact.penetration + push_out_tolerance);

                // Where it hit decides where it leaves - the incoming direction
                // is discarded, which is what gives the player control.
                const float half_width = paddle->size.x * 0.5f;
                const float offset = (transform.position.x - paddle->position.x) / half_width;
                const float speed = math::length(velocity.value);

                velocity.value = math::paddle_bounce_direction(offset, max_bounce_angle) * speed;
                return;
            }
        }

        void bounce_off_bricks(World& world, Transform& transform, Velocity& velocity, float radius)
        {
            const math::Circle ball{transform.position, radius};
            const std::vector<core::Entity>& entities = world.bricks.entities();

            // Resolve only the deepest contact. Reflecting off several bricks in
            // one step can cancel out and send the ball straight through.
            core::Entity hit_entity = core::null_entity;
            math::Contact deepest;

            for (std::size_t i = 0; i < world.bricks.size(); ++i)
            {
                const core::Entity entity = entities[i];
                if (!is_alive(world, entity))
                {
                    continue;
                }

                const Transform* brick = world.transforms.find(entity);
                if (brick == nullptr)
                {
                    continue;
                }

                const math::Contact contact = math::circle_vs_aabb(ball, bounds_of(*brick));
                if (contact.hit && contact.penetration > deepest.penetration)
                {
                    hit_entity = entity;
                    deepest = contact;
                }
            }

            if (hit_entity == core::null_entity)
            {
                return;
            }

            transform.position += deepest.normal * (deepest.penetration + push_out_tolerance);
            velocity.value = math::reflect(velocity.value, deepest.normal);

            Brick* brick = world.bricks.find(hit_entity);
            if (brick != nullptr)
            {
                brick->hit_points -= 1;

                // Per hit rather than per brick, so a tough brick is worth more
                // without needing to remember what it started at.
                world.score += points_per_hit;

                if (brick->hit_points <= 0)
                {
                    world.destroyed.add(hit_entity, Destroyed{});
                }
                else if (core::Color* color = world.colors.find(hit_entity); color != nullptr)
                {
                    // Re-colour to the remaining toughness, so a damaged brick
                    // stops claiming to be as tough as it was.
                    *color = brick_color(brick->hit_points);
                }
            }
        }
    }

    void paddle_system(World& world, const Input& input, float dt)
    {
        float direction = 0.0f;
        if (input.move_left)
        {
            direction -= 1.0f;
        }
        if (input.move_right)
        {
            direction += 1.0f;
        }

        const std::vector<core::Entity>& entities = world.paddles.entities();
        const std::vector<Paddle>& paddles = world.paddles.components();

        for (std::size_t i = 0; i < paddles.size(); ++i)
        {
            Transform* transform = world.transforms.find(entities[i]);
            if (transform == nullptr)
            {
                continue;
            }

            transform->position.x += direction * paddles[i].speed * dt;

            const float half_width = transform->size.x * 0.5f;
            transform->position.x =
                std::clamp(transform->position.x, half_width, world.size.x - half_width);
        }
    }

    void movement_system(World& world, float dt)
    {
        const std::vector<core::Entity>& entities = world.velocities.entities();
        const std::vector<Velocity>& velocities = world.velocities.components();

        for (std::size_t i = 0; i < velocities.size(); ++i)
        {
            Transform* transform = world.transforms.find(entities[i]);
            if (transform == nullptr)
            {
                continue;
            }

            transform->position += velocities[i].value * dt;
        }
    }

    void collision_system(World& world)
    {
        const std::vector<core::Entity>& entities = world.balls.entities();
        const std::vector<Ball>& balls = world.balls.components();

        for (std::size_t i = 0; i < balls.size(); ++i)
        {
            const core::Entity entity = entities[i];

            Transform* transform = world.transforms.find(entity);
            Velocity* velocity = world.velocities.find(entity);
            if (transform == nullptr || velocity == nullptr)
            {
                continue;
            }

            const float radius = balls[i].radius;

            bounce_off_edges(world, *transform, *velocity, radius);
            bounce_off_paddles(world, *transform, *velocity, radius);
            bounce_off_bricks(world, *transform, *velocity, radius);
        }
    }

    void lifecycle_system(World& world)
    {
        const std::vector<core::Entity>& entities = world.balls.entities();
        const std::vector<Ball>& balls = world.balls.components();

        for (std::size_t i = 0; i < balls.size(); ++i)
        {
            const core::Entity entity = entities[i];

            const Transform* transform = world.transforms.find(entity);
            if (transform == nullptr)
            {
                continue;
            }

            if (transform->position.y + balls[i].radius >= 0.0f)
            {
                continue;
            }

            world.lives -= 1;
            if (world.lives <= 0)
            {
                // Temporary: proper game over arrives with the Day 5 state
                // machine. Keeping it playable matters more today.
                world.lives = starting_lives;
            }

            reset_ball(world, entity);
        }
    }

    void sweep_destroyed(World& world)
    {
        // Copied first: destroy_entity mutates the store being read.
        const std::vector<core::Entity> doomed = world.destroyed.entities();

        for (const core::Entity entity : doomed)
        {
            world.destroy_entity(entity);
        }
    }

    void step(World& world, const Input& input, float dt)
    {
        paddle_system(world, input, dt);
        movement_system(world, dt);
        collision_system(world);
        lifecycle_system(world);
        sweep_destroyed(world);
    }
}
