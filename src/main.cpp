#include <chrono>
#include <cstddef>

#include "game/Spawn.h"
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

    // Digits are hit points; anything else is an empty cell.
    constexpr const char* first_level = R"(
..111111..
.12222221.
1233333321
.12222221.
..111111..
)";

    // Held state is overwritten each frame. One-shot actions are OR-ed in and
    // survive until a step consumes them, because a frame can run zero steps
    // (the press would be lost) or several (it would fire more than once).
    void read_input(const platform::Window& window, game::Input& input)
    {
        input.move_left = window.is_key_down(platform::Key::MoveLeft);
        input.move_right = window.is_key_down(platform::Key::MoveRight);

        input.launch |= window.is_key_pressed(platform::Key::Launch);
        input.toggle_pause |= window.is_key_pressed(platform::Key::Pause);
        input.restart |= window.is_key_pressed(platform::Key::Restart);
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
    game::start_game(world, game::parse_level(first_level));

    std::chrono::steady_clock::time_point previous = std::chrono::steady_clock::now();
    float accumulator = 0.0f;
    game::Input input;

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

        read_input(window, input);

        int steps = 0;
        while (accumulator >= fixed_dt && steps < max_steps_per_frame)
        {
            game::step(world, input, fixed_dt);

            // Consumed. Held state is refreshed next frame regardless.
            input.clear_one_shots();

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

