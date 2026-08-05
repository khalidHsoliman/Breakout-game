#include <glad/gl.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cstdio>

#include "platform/Window.h"

namespace platform
{
    namespace
    {
        // GLFW reports the reason for a failure here rather than in a return
        // value, so without this a failed init tells you nothing useful.
        void error_callback(int code, const char* description)
        {
            std::fprintf(stderr, "GLFW error %d: %s\n", code, description);
        }

        int to_glfw_key(Key key)
        {
            switch (key)
            {
            case Key::Escape:
                return GLFW_KEY_ESCAPE;
            }
            return GLFW_KEY_UNKNOWN;
        }
    }

    bool Window::init(int width, int height, const char* title)
    {
        glfwSetErrorCallback(error_callback);

        if (glfwInit() == GLFW_FALSE)
        {
            std::fprintf(stderr, "Failed to initialise GLFW.\n");
            return false;
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        GLFWwindow* handle = glfwCreateWindow(width, height, title, nullptr, nullptr);
        if (handle == nullptr)
        {
            std::fprintf(stderr, "Failed to create window.\n");
            glfwTerminate();
            return false;
        }

        // glad can only resolve function pointers once a context is current.
        glfwMakeContextCurrent(handle);

        if (gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress)) == 0)
        {
            std::fprintf(stderr, "Failed to load OpenGL 3.3 functions.\n");
            glfwDestroyWindow(handle);
            glfwTerminate();
            return false;
        }

        glfwSwapInterval(1);

        // Assigned last, so a non-null m_handle means fully initialised.
        m_handle = handle;
        return true;
    }

    Window::~Window()
    {
        if (m_handle != nullptr)
        {
            glfwDestroyWindow(m_handle);
            glfwTerminate();
        }
    }

    bool Window::should_close() const
    {
        return glfwWindowShouldClose(m_handle) != 0;
    }

    void Window::request_close()
    {
        glfwSetWindowShouldClose(m_handle, GLFW_TRUE);
    }

    void Window::poll_events()
    {
        glfwPollEvents();
    }

    void Window::swap_buffers()
    {
        glfwSwapBuffers(m_handle);
    }

    bool Window::is_key_down(Key key) const
    {
        return glfwGetKey(m_handle, to_glfw_key(key)) == GLFW_PRESS;
    }
}
