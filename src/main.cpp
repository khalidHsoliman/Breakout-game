#include "platform/Window.h"
#include "platform/Renderer.h"

int main()
{
    platform::Window window;
    if (!window.init(800, 600, "Breakout"))
    {
        return 1;
    }

    platform::Renderer renderer;

    while (!window.should_close())
    {
        window.poll_events();

        if (window.is_key_down(platform::Key::Escape))
        {
            window.request_close();
        }

        renderer.clear(0.2f, 0.3f, 0.3f);
        window.swap_buffers();
    }

    return 0;
}
