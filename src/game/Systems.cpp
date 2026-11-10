#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <random>
#include <vector>

#include "game/Spawn.h"
#include "game/Systems.h"
#include "math/Collision.h"
#include "math/Lerp.h"

namespace game
{
    namespace
    {
        // Pushed a fraction further than the overlap, so floating point cannot
        // leave the ball inside and re-trigger the same contact.
        constexpr float PUSH_OUT_TOLERANCE = 0.01f;

        constexpr float MAX_BOUNCE_ANGLE = std::numbers::pi_v<float> / 3.0f;

        constexpr int POINTS_PER_HIT = 10;

        constexpr float PARTICLE_GRAVITY = 420.0f;

        math::AABB bounds_of(const Transform& transform)
        {
            const math::Vec2 half = transform.size * 0.5f;
            return math::AABB{transform.position - half, transform.position + half};
        }

        void bounce_off_edges(const World& world, Transform& transform, Velocity& velocity, float radius)
        {
            if (transform.position.x - radius < 0.0f)
            {
                transform.position.x = radius + PUSH_OUT_TOLERANCE;
                velocity.value = math::reflect(velocity.value, math::Vec2{1.0f, 0.0f});
            }
            else if (transform.position.x + radius > world.size.x)
            {
                transform.position.x = world.size.x - radius - PUSH_OUT_TOLERANCE;
                velocity.value = math::reflect(velocity.value, math::Vec2{-1.0f, 0.0f});
            }

            if (transform.position.y + radius > world.size.y)
            {
                transform.position.y = world.size.y - radius - PUSH_OUT_TOLERANCE;
                velocity.value = math::reflect(velocity.value, math::Vec2{0.0f, -1.0f});
            }

            // No floor: falling past the bottom costs a life, which is
            // lifecycle_system's job rather than a collision.
        }

        void bounce_off_paddles(World& world, Transform& transform, Velocity& velocity, float radius)
        {
            const math::Circle ball{transform.position, radius};
            const std::vector<core::Entity>& entities = world.paddles.entities();

            for (std::size_t i = 0; i < world.paddles.size(); i++)
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

                transform.position += contact.normal * (contact.penetration + PUSH_OUT_TOLERANCE);

                // Where it hit decides where it leaves - the incoming direction is discarded.
                const float half_width = paddle->size.x * 0.5f;
                const float offset = (transform.position.x - paddle->position.x) / half_width;
                const float speed = math::length(velocity.value);

                velocity.value = math::paddle_bounce_direction(offset, MAX_BOUNCE_ANGLE) * speed;

                world.events.push_back(GameEvent::PaddleBounce);
                return;
            }
        }

        void speed_up(Velocity& velocity)
        {
            const float speed = math::length(velocity.value);
            if (speed <= 0.0f)
            {
                return;
            }

            const float faster = std::min(speed + BALL_SPEED_PER_HIT, MAX_BALL_SPEED);
            velocity.value = math::normalize(velocity.value) * faster;
        }

        void clear_particles(World& world)
        {
            const std::vector<core::Entity>& entities = world.particles.entities();

            for (std::size_t i = 0; i < world.particles.size(); i++)
            {
                world.destroyed.add(entities[i], Destroyed{});
            }
        }

        void clear_capsules(World& world)
        {
            const std::vector<core::Entity>& entities = world.capsules.entities();

            for (std::size_t i = 0; i < world.capsules.size(); i++)
            {
                world.destroyed.add(entities[i], Destroyed{});
            }
        }

        bool overlaps(const math::AABB& a, const math::AABB& b)
        {
            return a.min.x < b.max.x && b.min.x < a.max.x
                && a.min.y < b.max.y && b.min.y < a.max.y;
        }

        // Used after the paddle moves and after it grows, since either can
        // push it past a wall.
        void keep_inside(const World& world, Transform& paddle)
        {
            const float half_width = paddle.size.x * 0.5f;
            paddle.position.x = std::clamp(paddle.position.x, half_width, world.size.x - half_width);
        }

        // True with the given chance. Built on the engine's raw output, which
        // the standard fixes, so a seed gives the same answers on every compiler.
        bool roll(std::mt19937& rng, float chance)
        {
            const double outcomes = static_cast<double>(std::mt19937::max()) + 1.0;
            return static_cast<double>(rng()) < static_cast<double>(chance) * outcomes;
        }

        void maybe_drop_capsule(World& world, math::Vec2 position)
        {
            if (!roll(world.rng, world.capsule_drop_chance))
            {
                return;
            }

            const PowerUp kind = roll(world.rng, 0.5f) ? PowerUp::WidePaddle : PowerUp::MultiBall;
            spawn_capsule(world, position, kind);
        }

        void narrow_paddle(Transform& transform, Paddle& paddle)
        {
            // Shrinks about its centre, so it cannot end up past a wall.
            transform.size.x = PADDLE_WIDTH;
            paddle.wide_time_left = 0.0f;
        }

        void narrow_paddles(World& world)
        {
            const std::vector<core::Entity>& entities = world.paddles.entities();

            for (std::size_t i = 0; i < world.paddles.size(); i++)
            {
                if (Transform* transform = world.transforms.find(entities[i]); transform != nullptr)
                {
                    narrow_paddle(*transform, *world.paddles.find(entities[i]));
                }
            }
        }

        // Past the floor: the whole ball is below it. lifecycle_system takes
        // these out at the end of the step.
        bool has_fallen(const Transform& transform, float radius)
        {
            return transform.position.y + radius < 0.0f;
        }

        bool in_play(const World& world, core::Entity ball)
        {
            const Transform* transform = world.transforms.find(ball);
            return is_alive(world, ball) && transform != nullptr
                && !has_fallen(*transform, world.balls.find(ball)->radius);
        }

        // Two more balls from the first ball still in play, up at
        // MULTI_BALL_ANGLE either side of vertical at its speed, as many as fit
        // under MAX_BALLS. Nothing without a ball in play.
        void add_balls(World& world)
        {
            core::Entity source = core::NULL_ENTITY;
            int balls_in_play = 0;

            const std::vector<core::Entity>& entities = world.balls.entities();
            for (std::size_t i = 0; i < world.balls.size(); i++)
            {
                if (!in_play(world, entities[i]))
                {
                    continue;
                }

                balls_in_play += 1;
                if (source == core::NULL_ENTITY)
                {
                    source = entities[i];
                }
            }

            if (source == core::NULL_ENTITY)
            {
                return;
            }

            // Copied: spawning adds transforms and velocities, which can move
            // the stores these live in.
            const Transform from = *world.transforms.find(source);
            const float speed = math::length(world.velocities.find(source)->value);

            for (const float side : {1.0f, -1.0f})
            {
                if (balls_in_play >= MAX_BALLS)
                {
                    return;
                }

                const core::Entity ball = spawn_ball(world);
                Transform& transform = *world.transforms.find(ball);
                transform.position = from.position;

                // Drawn along the same path as the ball it came from, rather
                // than sliding in from where spawn_ball put it.
                transform.previous_position = from.previous_position;

                const float angle = side * MULTI_BALL_ANGLE;
                world.velocities.find(ball)->value = math::Vec2{std::sin(angle), std::cos(angle)} * speed;

                balls_in_play += 1;
            }
        }

        void widen_paddle(const World& world, Transform& transform, Paddle& paddle)
        {
            // Set rather than scaled, so another catch restarts the time
            // without making it any wider.
            transform.size.x = WIDE_PADDLE_WIDTH;
            paddle.wide_time_left = WIDE_PADDLE_TIME;
            keep_inside(world, transform);
        }

        // Once per step, before anything moves.
        void remember_positions(World& world)
        {
            const std::vector<core::Entity>& entities = world.transforms.entities();

            for (std::size_t i = 0; i < world.transforms.size(); i++)
            {
                Transform& transform = *world.transforms.find(entities[i]);
                transform.previous_position = transform.position;
            }
        }

        void bounce_off_bricks(World& world, Transform& transform, Velocity& velocity, float radius)
        {
            const math::Circle ball{transform.position, radius};
            const std::vector<core::Entity>& entities = world.bricks.entities();

            // Resolve only the deepest contact. Reflecting off several bricks in
            // one step can cancel out and send the ball straight through.
            core::Entity hit_entity = core::NULL_ENTITY;
            math::Contact deepest;

            for (std::size_t i = 0; i < world.bricks.size(); i++)
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

            if (hit_entity == core::NULL_ENTITY)
            {
                return;
            }

            transform.position += deepest.normal * (deepest.penetration + PUSH_OUT_TOLERANCE);
            velocity.value = math::reflect(velocity.value, deepest.normal);

            Brick* brick = world.bricks.find(hit_entity);
            if (brick != nullptr)
            {
                const int brick_hit_points = brick->hit_points;
                brick->hit_points -= 1;

                world.events.push_back(GameEvent::BrickHit);

                // Per brick hit
                speed_up(velocity);

                // Per hit rather than per brick, so a tough brick is worth more
                // without needing to remember what it started at.
                world.score += POINTS_PER_HIT;

                if (brick->hit_points <= 0)
                {
                    if (const Transform* dead = world.transforms.find(hit_entity); dead != nullptr)
                    {
                        // Copied: spawning adds transforms and can move the store.
                        const math::Vec2 position = dead->position;
                        spawn_brick_particles(world, position, dead->size, brick_color(brick_hit_points));
                        maybe_drop_capsule(world, position);
                    }

                    world.destroyed.add(hit_entity, Destroyed{});
                }
                else if (core::Color* color = world.colors.find(hit_entity); color != nullptr)
                {
                    // Re-colour to the remaining toughness.
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

        for (std::size_t i = 0; i < paddles.size(); i++)
        {
            Transform* transform = world.transforms.find(entities[i]);
            if (transform == nullptr)
            {
                continue;
            }

            transform->position.x += direction * paddles[i].speed * dt;
            keep_inside(world, *transform);
        }
    }

    void movement_system(World& world, float dt)
    {
        const std::vector<core::Entity>& entities = world.velocities.entities();
        const std::vector<Velocity>& velocities = world.velocities.components();

        for (std::size_t i = 0; i < velocities.size(); i++)
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

        for (std::size_t i = 0; i < balls.size(); i++)
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

        std::vector<core::Entity> fallen;
        bool any_in_play = false;

        for (std::size_t i = 0; i < balls.size(); i++)
        {
            const core::Entity entity = entities[i];

            const Transform* transform = world.transforms.find(entity);
            if (!is_alive(world, entity) || transform == nullptr)
            {
                continue;
            }

            if (has_fallen(*transform, balls[i].radius))
            {
                fallen.push_back(entity);
            }
            else
            {
                any_in_play = true;
            }
        }

        if (fallen.empty())
        {
            return;
        }

        // While another ball is in play, a fallen one is just gone. Otherwise
        // one is kept to be served again and the rest go, so the next serve
        // has exactly one ball.
        const std::size_t first_to_remove = any_in_play ? 0 : 1;
        for (std::size_t i = first_to_remove; i < fallen.size(); i++)
        {
            world.destroyed.add(fallen[i], Destroyed{});
        }

        if (any_in_play)
        {
            return;
        }

        // The last ball: one life, however many fell in this step.
        world.lives -= 1;

        Velocity* velocity = world.velocities.find(fallen[0]);
        if (velocity != nullptr)
        {
            velocity->value = math::Vec2{};
        }

        clear_particles(world);
        clear_capsules(world);
        narrow_paddles(world);
        world.state = (world.lives <= 0) ? GameState::GameOver : GameState::Ready;
    }

    void trail_system(World& world)
    {
        const std::vector<core::Entity>& entities = world.balls.entities();

        for (std::size_t i = 0; i < world.balls.size(); i++)
        {
            const core::Entity entity = entities[i];

            const Transform* transform = world.transforms.find(entity);
            const core::Color* color = world.colors.find(entity);
            if (transform == nullptr || color == nullptr)
            {
                continue;
            }

            const math::Vec2 position = transform->position;
            const core::Color trail_color = *color;

            spawn_ball_trail(world, position, trail_color);
        }
    }

    void particle_system(World& world, float dt)
    {
        const std::vector<core::Entity>& entities = world.particles.entities();

        for (std::size_t i = 0; i < world.particles.size(); i++)
        {
            const core::Entity entity = entities[i];
            Particle& particle = *world.particles.find(entity);

            particle.lifetime -= dt;

            if (particle.lifetime <= 0.0f)
            {
                world.destroyed.add(entity, Destroyed{});
                continue;
            }

            // What is left of its life is what is left of its opacity.
            world.colors.find(entity)->a = math::smoothstep(particle.lifetime / particle.max_life);
            world.velocities.find(entity)->value.y -= PARTICLE_GRAVITY * dt;
        }
    }

    void power_up_system(World& world, float dt)
    {
        const std::vector<core::Entity>& paddle_entities = world.paddles.entities();

        // First, so a paddle widened in this step keeps its full time.
        for (std::size_t i = 0; i < world.paddles.size(); i++)
        {
            Paddle& paddle = *world.paddles.find(paddle_entities[i]);
            if (paddle.wide_time_left <= 0.0f)
            {
                continue;
            }

            paddle.wide_time_left -= dt;
            if (paddle.wide_time_left > 0.0f)
            {
                continue;
            }

            paddle.wide_time_left = 0.0f;
            if (Transform* transform = world.transforms.find(paddle_entities[i]); transform != nullptr)
            {
                narrow_paddle(*transform, paddle);
            }
        }

        const std::vector<core::Entity>& capsule_entities = world.capsules.entities();

        for (std::size_t i = 0; i < world.capsules.size(); i++)
        {
            const core::Entity entity = capsule_entities[i];
            const Transform* transform = world.transforms.find(entity);
            if (!is_alive(world, entity) || transform == nullptr)
            {
                continue;
            }

            const math::AABB capsule = bounds_of(*transform);

            bool caught = false;
            for (std::size_t p = 0; p < world.paddles.size() && !caught; p++)
            {
                Transform* paddle_transform = world.transforms.find(paddle_entities[p]);
                if (paddle_transform == nullptr || !overlaps(capsule, bounds_of(*paddle_transform)))
                {
                    continue;
                }

                caught = true;

                switch (world.capsules.find(entity)->kind)
                {
                case PowerUp::WidePaddle:
                    widen_paddle(world, *paddle_transform, *world.paddles.find(paddle_entities[p]));
                    break;
                case PowerUp::MultiBall:
                    // Adds transforms, so no pointer taken above is used after this.
                    add_balls(world);
                    break;
                }
            }

            // Missed once all of it is below the floor.
            if (caught || capsule.max.y < 0.0f)
            {
                world.destroyed.add(entity, Destroyed{});
            }
        }
    }

    void win_system(World& world)
    {
        if (world.bricks.size() != 0)
        {
            return;
        }

        if (world.level_index + 1 < static_cast<int>(world.levels.size()))
        {
            // start_level clears the particles along with everything else.
            world.level_index += 1;
            start_level(world);
            return;
        }

        clear_particles(world);
        clear_capsules(world);
        world.state = GameState::Won;
    }

    void serve_system(World& world, const Input& input)
    {
        const Transform* paddle = nullptr;
        if (world.paddles.size() > 0)
        {
            paddle = world.transforms.find(world.paddles.entities()[0]);
        }

        const std::vector<core::Entity>& entities = world.balls.entities();
        const std::vector<Ball>& balls = world.balls.components();

        if (paddle != nullptr)
        {
            for (std::size_t i = 0; i < balls.size(); i++)
            {
                Transform* transform = world.transforms.find(entities[i]);
                if (transform == nullptr)
                {
                    continue;
                }

                // Parked on the paddle, so moving the paddle aims the shot.
                transform->position = math::Vec2{
                    paddle->position.x,
                    paddle->position.y + paddle->size.y * 0.5f + balls[i].radius + SERVE_GAP};
            }
        }

        if (!input.launch)
        {
            return;
        }

        for (std::size_t i = 0; i < balls.size(); i++)
        {
            launch_ball(world, entities[i]);
        }
        world.state = GameState::Playing;
    }

    void transition_system(World& world, const Input& input)
    {
        if (input.restart && (world.state == GameState::GameOver || world.state == GameState::Won))
        {
            // From the first level, not the one that was lost on.
            start_game(world, world.levels);
            return;
        }

        if (!input.toggle_pause)
        {
            return;
        }

        if (world.state == GameState::Playing)
        {
            world.state = GameState::Paused;
        }
        else if (world.state == GameState::Paused)
        {
            world.state = GameState::Playing;
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
        remember_positions(world);

        // Runs first and in every state
        transition_system(world, input);

        switch (world.state)
        {
        case GameState::Ready:
            paddle_system(world, input, dt);
            serve_system(world, input);
            break;

        case GameState::Playing:
            paddle_system(world, input, dt);
            movement_system(world, dt);
            collision_system(world);
            power_up_system(world, dt);
            trail_system(world);
            particle_system(world, dt);
            lifecycle_system(world);
            win_system(world);
            sweep_destroyed(world);
            break;

        case GameState::Paused:
        case GameState::GameOver:
        case GameState::Won:
            break;
        }
    }
}
