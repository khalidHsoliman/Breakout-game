#pragma once

#include <array>
#include <cstddef>

#include "math/Vec2.h"

// Opaque to everything outside Window.cpp - keeps GLFW out of this header, so
// no layer above platform/ can acquire a dependency on it by accident.
struct GLFWwindow;

namespace platform
{
    enum class Key
    {
        Escape,
        ToggleFullscreen,
        MoveLeft,
        MoveRight,
        Launch,
        Pause,
        Restart
    };

    inline constexpr std::size_t KEY_COUNT = 7;

    class Window
    {
    public:
        Window() = default;
        ~Window();

        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;

        // Creates the window and its OpenGL 3.3 context. Reports the reason to
        // stderr and returns false on failure. No other method may be called
        // until this has returned true.
        //
        // When fullscreen is true the window covers the primary monitor at its
        // current video mode; width and height then apply to windowed mode.
        bool init(int width, int height, const char* title, bool fullscreen);

        // Keeps the OpenGL context, so the renderer's objects stay valid. The
        // framebuffer size changes, so the caller must reset the viewport.
        void set_fullscreen(bool fullscreen);
        bool is_fullscreen() const;

        // In pixels, which is not the same as the window size on high-DPI
        // displays. This is what the viewport must be sized against.
        math::Vec2 framebuffer_size() const;

        bool should_close() const;
        void request_close();
        void poll_events();
        void swap_buffers();

        // Held this frame.
        bool is_key_down(Key key) const;

        // Went down this frame - use for one-shot actions, since is_key_down
        // stays true for as long as the key is held.
        bool is_key_pressed(Key key) const;

    private:
        GLFWwindow* m_handle = nullptr;

        std::array<bool, KEY_COUNT> m_previous_key_state{};

        bool m_fullscreen = false;

        // Where to put the window when leaving fullscreen.
        int m_windowed_x = 0;
        int m_windowed_y = 0;
        int m_windowed_width = 0;
        int m_windowed_height = 0;
    };
}
