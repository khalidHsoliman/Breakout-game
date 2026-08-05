#include <glad/gl.h>

#include "platform/Renderer.h"

namespace platform
{
    void Renderer::clear(float red, float green, float blue)
    {
        glClearColor(red, green, blue, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }
}
