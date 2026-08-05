#pragma once

namespace platform
{
    // Owns every OpenGL call in the project. Needs a current context with its
    // function pointers loaded, so a Window must be initialised first.
    class Renderer
    {
    public:
        void clear(float red, float green, float blue);
    };
}
