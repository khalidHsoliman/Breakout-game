#pragma once

#include "math/Vec2.h"

// Opaque to everything outside Window.cpp - keeps GLFW out of this header, so
// no layer above platform/ can acquire a dependency on it by accident.
struct GLFWwindow;

namespace platform
{
    enum class Key
    {
        Escape
    };

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
        // current video mode and width/height are ignored.
        bool init(int width, int height, const char* title, bool fullscreen);

        // In pixels, which is not the same as the window size on high-DPI
        // displays. This is what the viewport must be sized against.
        math::Vec2 framebuffer_size() const;

        bool should_close() const;
        void request_close();
        void poll_events();
        void swap_buffers();
        bool is_key_down(Key key) const;

    private:
        GLFWwindow* m_handle = nullptr;
    };
}
