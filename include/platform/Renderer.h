#pragma once

#include <string_view>
#include <vector>

#include "core/Color.h"
#include "math/Vec2.h"

namespace platform
{
    // Owns every OpenGL call in the project. Needs a current context with its
    // function pointers loaded, so a Window must be initialised first.
    //
    // Quads are batched: draw_quad appends vertices to a CPU-side buffer and
    // end_frame uploads the whole frame and issues a single draw call.
    class Renderer
    {
    public:
        Renderer() = default;
        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        // Compiles the shaders and creates the vertex buffers. Reports the
        // reason to stderr and returns false on failure.
        //
        // The viewport is the largest rectangle with the world's aspect ratio
        // that fits the framebuffer, centred, so the world is never distorted
        // by the shape of the display.
        bool init(math::Vec2 world_size, math::Vec2 framebuffer_size);

        // Call again whenever the framebuffer size changes, such as after
        // toggling fullscreen.
        void set_viewport(math::Vec2 framebuffer_size);

        void begin_frame();
        void clear(core::Color color);
        void draw_quad(math::Vec2 center, math::Vec2 size, core::Color color);
        void draw_circle(math::Vec2 center, float radius, core::Color color, int segments = 24);

        // One quad per lit pixel of a 5x7 glyph. position is the bottom-left of
        // the text, and pixel_size is the world size of one font pixel.
        void draw_text(std::string_view text, math::Vec2 position, float pixel_size, core::Color color);
        void end_frame();

    private:
        struct Vertex
        {
            float x, y;
            float r, g, b;
        };

        std::vector<Vertex> m_vertices;
        math::Vec2 m_world_size;

        // GLuint is unsigned int, so handles can live here without glad.
        unsigned int m_program = 0;
        unsigned int m_vao = 0;
        unsigned int m_vbo = 0;

        int m_viewport_x = 0;
        int m_viewport_y = 0;
        int m_viewport_width = 0;
        int m_viewport_height = 0;
    };
}
