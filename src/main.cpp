#include <chrono>
#include <cstddef>

#include "game/Systems.h"
#include "platform/Renderer.h"
#include "platform/Window.h"

namespace
{
    // Starting mode only - F11 toggles at runtime.
    constexpr bool start_fullscreen = false;

    // Used for windowed mode, whether that is at startup or after a toggle.
    constexpr int window_width = 800;
    constexpr int window_height = 600;

    // World units. Currently 1:1 with pixels at the default window size, but
    // nothing in the game may rely on that.
    constexpr math::Vec2 world_size{800.0f, 600.0f};

    // The game always advances in these increments, whatever the frame rate.
    constexpr float fixed_dt = 1.0f / 60.0f;

    // A long stall must not queue up seconds of catch-up work: past this many
    // steps the leftover time is dropped instead.
    constexpr int max_steps_per_frame = 5;

    core::Entity spawn(game::World& world,
                       math::Vec2 position,
                       math::Vec2 size,
                       core::Color color)
    {
        const core::Entity entity = world.create_entity();
        world.transforms.add(entity, game::Transform{position, size});
        world.colors.add(entity, color);
        return entity;
    }

    // Temporary scaffolding. Level layout becomes data on Day 4.
    void create_placeholder_scene(game::World& world)
    {
        const core::Entity paddle = spawn(world, math::Vec2{400.0f, 40.0f},
                                          math::Vec2{120.0f, 20.0f},
                                          core::Color{0.90f, 0.90f, 0.90f});
        world.paddles.add(paddle, game::Paddle{520.0f});

        const float ball_radius = 8.0f;
        const core::Entity ball = spawn(world, math::Vec2{400.0f, 120.0f},
                                        math::Vec2{ball_radius * 2.0f, ball_radius * 2.0f},
                                        core::Color{0.95f, 0.65f, 0.25f});
        world.circle_shapes.add(ball, game::CircleShape{});
        world.balls.add(ball, game::Ball{ball_radius});
        world.velocities.add(ball, game::Velocity{math::Vec2{180.0f, 300.0f}});

        for (int row = 0; row < 4; ++row)
        {
            for (int column = 0; column < 8; ++column)
            {
                const float x = 50.0f + static_cast<float>(column) * 100.0f;
                const float y = 560.0f - static_cast<float>(row) * 40.0f;
                const float green = 0.25f + static_cast<float>(column) * 0.07f;

                const core::Entity brick = spawn(world, math::Vec2{x, y},
                                                 math::Vec2{90.0f, 30.0f},
                                                 core::Color{0.85f, green, 0.35f});
                world.bricks.add(brick, game::Brick{1});
            }
        }
    }

    game::Input read_input(const platform::Window& window)
    {
        game::Input input;
        input.move_left = window.is_key_down(platform::Key::MoveLeft);
        input.move_right = window.is_key_down(platform::Key::MoveRight);
        return input;
    }

    void render_world(const game::World& world, platform::Renderer& renderer)
    {
        const std::vector<core::Entity>& entities = world.transforms.entities();
        const std::vector<game::Transform>& transforms = world.transforms.components();

        for (std::size_t i = 0; i < transforms.size(); ++i)
        {
            const core::Entity entity = entities[i];
            const core::Color* color = world.colors.find(entity);
            if (color == nullptr)
            {
                continue;
            }

            if (world.circle_shapes.has(entity))
            {
                renderer.draw_circle(transforms[i].position, transforms[i].size.x * 0.5f, *color);
            }
            else
            {
                renderer.draw_quad(transforms[i].position, transforms[i].size, *color);
            }
        }
    }
}

int main()
{
    platform::Window window;
    if (!window.init(window_width, window_height, "Breakout", start_fullscreen))
    {
        return 1;
    }

    platform::Renderer renderer;
    if (!renderer.init(world_size, window.framebuffer_size()))
    {
        return 1;
    }

    game::World world;
    world.size = world_size;
    create_placeholder_scene(world);

    std::chrono::steady_clock::time_point previous = std::chrono::steady_clock::now();
    float accumulator = 0.0f;

    while (!window.should_close())
    {
        window.poll_events();

        if (window.is_key_down(platform::Key::Escape))
        {
            window.request_close();
        }

        if (window.is_key_pressed(platform::Key::ToggleFullscreen))
        {
            window.set_fullscreen(!window.is_fullscreen());
            renderer.set_viewport(window.framebuffer_size());
        }

        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        accumulator += std::chrono::duration<float>(now - previous).count();
        previous = now;

        const game::Input input = read_input(window);

        int steps = 0;
        while (accumulator >= fixed_dt && steps < max_steps_per_frame)
        {
            game::step(world, input, fixed_dt);
            accumulator -= fixed_dt;
            ++steps;
        }

        if (steps == max_steps_per_frame)
        {
            accumulator = 0.0f;
        }

        renderer.begin_frame();
        renderer.clear(core::Color{0.10f, 0.12f, 0.15f});
        render_world(world, renderer);
        renderer.end_frame();

        window.swap_buffers();
    }

    return 0;
}

