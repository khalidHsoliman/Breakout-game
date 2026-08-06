#include "platform/Window.h"
#include "platform/Renderer.h"

namespace
{
    constexpr int window_width = 800;
    constexpr int window_height = 600;

    // World units. Currently 1:1 with pixels at the default window size, but
    // nothing in the game may rely on that.
    constexpr math::Vec2 world_size{800.0f, 600.0f};
}

int main()
{
    platform::Window window;
    if (!window.init(window_width, window_height, "Breakout"))
    {
        return 1;
    }

    platform::Renderer renderer;
    if (!renderer.init(world_size))
    {
        return 1;
    }

    while (!window.should_close())
    {
        window.poll_events();

        if (window.is_key_down(platform::Key::Escape))
        {
            window.request_close();
        }

        renderer.begin_frame();
        renderer.clear(core::Color{0.2f, 0.3f, 0.3f});
        renderer.draw_quad(math::Vec2{400.0f, 40.0f},
                           math::Vec2{120.0f, 20.0f},
                           core::Color{0.9f, 0.9f, 0.9f});
        renderer.end_frame();

        window.swap_buffers();
    }

    return 0;
}
