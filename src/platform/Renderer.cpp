#include <glad/gl.h>

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <numbers>

#include "platform/Font.h"
#include "platform/Viewport.h"
#include "platform/Renderer.h"

// Shader compilation, the vertex array and buffer setup, and the batched draw
// follow the approach taught at https://learnopengl.com

namespace platform
{
    namespace
    {
        const char* vertex_shader_source = R"(
#version 330 core

layout (location = 0) in vec2 a_position;
layout (location = 1) in vec3 a_color;

uniform vec2 u_world_size;

out vec3 v_color;

void main()
{
    vec2 ndc = (a_position / u_world_size) * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
    v_color = a_color;
}
)";

        const char* fragment_shader_source = R"(
#version 330 core

in vec3 v_color;

out vec4 frag_color;

void main()
{
    frag_color = vec4(v_color, 1.0);
}
)";

        unsigned int compile_shader(unsigned int type, const char* source)
        {
            const unsigned int shader = glCreateShader(type);
            glShaderSource(shader, 1, &source, nullptr);
            glCompileShader(shader);

            int success = 0;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (success == GL_FALSE)
            {
                char log[512] = {};
                glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
                std::fprintf(stderr, "Shader compilation failed: %s\n", log);
                glDeleteShader(shader);
                return 0;
            }

            return shader;
        }

        unsigned int link_program(const char* vertex_source, const char* fragment_source)
        {
            const unsigned int vertex = compile_shader(GL_VERTEX_SHADER, vertex_source);
            if (vertex == 0)
            {
                return 0;
            }

            const unsigned int fragment = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
            if (fragment == 0)
            {
                glDeleteShader(vertex);
                return 0;
            }

            const unsigned int program = glCreateProgram();
            glAttachShader(program, vertex);
            glAttachShader(program, fragment);
            glLinkProgram(program);

            // Linking copies the compiled code into the program.
            glDeleteShader(vertex);
            glDeleteShader(fragment);

            int success = 0;
            glGetProgramiv(program, GL_LINK_STATUS, &success);
            if (success == GL_FALSE)
            {
                char log[512] = {};
                glGetProgramInfoLog(program, sizeof(log), nullptr, log);
                std::fprintf(stderr, "Shader link failed: %s\n", log);
                glDeleteProgram(program);
                return 0;
            }

            return program;
        }
    }

    bool Renderer::init(math::Vec2 world_size, math::Vec2 framebuffer_size)
    {
        const unsigned int program = link_program(vertex_shader_source, fragment_shader_source);
        if (program == 0)
        {
            return false;
        }

        glEnable(GL_MULTISAMPLE);

        m_world_size = world_size;
        set_viewport(framebuffer_size);

        unsigned int vao = 0;
        unsigned int vbo = 0;
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        // The vertex array records the format and the buffer it reads from, so
        // both must be bound while the attributes are described.
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              reinterpret_cast<const void*>(offsetof(Vertex, x)));
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              reinterpret_cast<const void*>(offsetof(Vertex, r)));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);

        glUseProgram(program);
        glUniform2f(glGetUniformLocation(program, "u_world_size"), world_size.x, world_size.y);

        m_program = program;
        m_vao = vao;
        m_vbo = vbo;
        return true;
    }

    void Renderer::set_viewport(math::Vec2 framebuffer_size)
    {
        m_framebuffer_size = framebuffer_size;
        m_viewport = fit_viewport(m_world_size, framebuffer_size);

        glViewport(static_cast<int>(m_viewport.x), static_cast<int>(m_viewport.y),
                   static_cast<int>(m_viewport.width), static_cast<int>(m_viewport.height));
    }

    math::Vec2 Renderer::world_from_pixel(math::Vec2 pixel) const
    {
        return platform::world_from_pixel(pixel, m_framebuffer_size, m_viewport, m_world_size);
    }

    Renderer::~Renderer()
    {
        // OpenGL ignores a name of 0, so these are safe even if init failed.
        glDeleteBuffers(1, &m_vbo);
        glDeleteVertexArrays(1, &m_vao);
        glDeleteProgram(m_program);
    }

    void Renderer::begin_frame()
    {
        // Keeps the capacity, so steady-state frames do not allocate.
        m_vertices.clear();
    }

    void Renderer::clear(core::Color color)
    {
        // glClear ignores the viewport, so the bars would otherwise take the
        // play area's colour.
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glEnable(GL_SCISSOR_TEST);
        glScissor(static_cast<int>(m_viewport.x), static_cast<int>(m_viewport.y),
                  static_cast<int>(m_viewport.width), static_cast<int>(m_viewport.height));
        glClearColor(color.r, color.g, color.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);
    }

    void Renderer::draw_quad(math::Vec2 center, math::Vec2 size, core::Color color)
    {
        const math::Vec2 half = size * 0.5f;

        const float left = center.x - half.x;
        const float right = center.x + half.x;
        const float bottom = center.y - half.y;
        const float top = center.y + half.y;

        const Vertex bottom_left{left, bottom, color.r, color.g, color.b};
        const Vertex bottom_right{right, bottom, color.r, color.g, color.b};
        const Vertex top_right{right, top, color.r, color.g, color.b};
        const Vertex top_left{left, top, color.r, color.g, color.b};

        m_vertices.push_back(bottom_left);
        m_vertices.push_back(bottom_right);
        m_vertices.push_back(top_right);

        m_vertices.push_back(top_right);
        m_vertices.push_back(top_left);
        m_vertices.push_back(bottom_left);
    }

    void Renderer::draw_circle(math::Vec2 center, float radius, core::Color color, int segments)
    {
        if (segments < 3)
        {
            segments = 3;
        }

        const Vertex middle{center.x, center.y, color.r, color.g, color.b};
        const float step = 2.0f * std::numbers::pi_v<float> / static_cast<float>(segments);

        for (int i = 0; i < segments; ++i)
        {
            const float from = step * static_cast<float>(i);
            const float to = step * static_cast<float>(i + 1);

            // Wound counter-clockwise, matching draw_quad.
            m_vertices.push_back(middle);
            m_vertices.push_back(Vertex{center.x + std::cos(from) * radius,
                                        center.y + std::sin(from) * radius,
                                        color.r, color.g, color.b});
            m_vertices.push_back(Vertex{center.x + std::cos(to) * radius,
                                        center.y + std::sin(to) * radius,
                                        color.r, color.g, color.b});
        }
    }

    void Renderer::draw_text(std::string_view text, math::Vec2 position, float pixel_size, core::Color color)
    {
        float x = position.x;

        for (const char character : text)
        {
            const Glyph glyph = glyph_for(character);

            for (int row = 0; row < GLYPH_HEIGHT; ++row)
            {
                for (int column = 0; column < GLYPH_WIDTH; ++column)
                {
                    const std::uint8_t mask =
                        static_cast<std::uint8_t>(1u << (GLYPH_WIDTH - 1 - column));

                    if ((glyph[static_cast<std::size_t>(row)] & mask) == 0)
                    {
                        continue;
                    }

                    // Row 0 is the top of the glyph, but world y increases
                    // upward, so rows are drawn from the top down.
                    const math::Vec2 center{
                        x + (static_cast<float>(column) + 0.5f) * pixel_size,
                        position.y + (static_cast<float>(GLYPH_HEIGHT - 1 - row) + 0.5f) * pixel_size};

                    draw_quad(center, math::Vec2{pixel_size, pixel_size}, color);
                }
            }

            x += static_cast<float>(GLYPH_WIDTH + GLYPH_SPACING) * pixel_size;
        }
    }

    void Renderer::end_frame()
    {
        if (m_vertices.empty())
        {
            return;
        }

        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(m_vertices.size() * sizeof(Vertex)),
                     m_vertices.data(),
                     GL_DYNAMIC_DRAW);

        glUseProgram(m_program);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertices.size()));

        glBindVertexArray(0);
    }
}
