#include <cstddef>

#include "game/World.h"
#include "platform/Renderer.h"
#include "platform/Window.h"

namespace
{
    constexpr bool fullscreen = true;

    // Ignored when fullscreen.
    constexpr int window_width = 800;
    constexpr int window_height = 600;

    // World units. Currently 1:1 with pixels at the default window size, but
    // nothing in the game may rely on that.
    constexpr math::Vec2 world_size{800.0f, 600.0f};

    core::Entity create_quad(game::World& world,
                             math::Vec2 position,
                             math::Vec2 size,
                             core::Color color)
    {
        const core::Entity entity = world.create_entity();
        world.transforms.add(entity, game::Transform{position, size});
        world.colors.add(entity, color);
        return entity;
    }

    // Temporary scaffolding. Level layout moves into game/ once bricks have
    // rules rather than just a position.
    void create_placeholder_scene(game::World& world)
    {
        create_quad(world, math::Vec2{400.0f, 40.0f}, math::Vec2{120.0f, 20.0f},
                    core::Color{0.9f, 0.9f, 0.9f});

        create_quad(world, math::Vec2{400.0f, 90.0f}, math::Vec2{16.0f, 16.0f},
                    core::Color{0.95f, 0.65f, 0.25f});

        for (int column = 0; column < 8; ++column)
        {
            const float x = 50.0f + static_cast<float>(column) * 100.0f;
            const float green = 0.30f + static_cast<float>(column) * 0.07f;

            create_quad(world, math::Vec2{x, 520.0f}, math::Vec2{90.0f, 30.0f},
                        core::Color{0.85f, green, 0.35f});
        }
    }

    void render_world(const game::World& world, platform::Renderer& renderer)
    {
        const std::vector<core::Entity>& entities = world.transforms.entities();
        const std::vector<game::Transform>& transforms = world.transforms.components();

        for (std::size_t i = 0; i < transforms.size(); ++i)
        {
            const core::Color* color = world.colors.find(entities[i]);
            if (color == nullptr)
            {
                continue;
            }

            renderer.draw_quad(transforms[i].position, transforms[i].size, *color);
        }
    }
}

int main()
{
    platform::Window window;
    if (!window.init(window_width, window_height, "Breakout", fullscreen))
    {
        return 1;
    }

    platform::Renderer renderer;
    if (!renderer.init(world_size, window.framebuffer_size()))
    {
        return 1;
    }

    game::World world;
    create_placeholder_scene(world);

    while (!window.should_close())
    {
        window.poll_events();

        if (window.is_key_down(platform::Key::Escape))
        {
            window.request_close();
        }

        renderer.begin_frame();
        renderer.clear(core::Color{0.10f, 0.12f, 0.15f});
        render_world(world, renderer);
        renderer.end_frame();

        window.swap_buffers();
    }

    return 0;
}
