#pragma once

#include "math/Vec2.h"

namespace platform
{
    struct Viewport
    {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
    };

    // The largest rectangle with the world's aspect ratio that fits inside the
    // framebuffer, centred. Whichever axis has room to spare becomes a bar.
    inline Viewport fit_viewport(math::Vec2 world_size, math::Vec2 framebuffer_size)
    {
        if (world_size.y <= 0.0f || framebuffer_size.y <= 0.0f)
        {
            return Viewport{};
        }

        const float world_aspect = world_size.x / world_size.y;
        const float frame_aspect = framebuffer_size.x / framebuffer_size.y;

        float width = framebuffer_size.x;
        float height = framebuffer_size.y;

        if (frame_aspect > world_aspect)
        {
            width = framebuffer_size.y * world_aspect;
        }
        else
        {
            height = framebuffer_size.x / world_aspect;
        }

        return Viewport{(framebuffer_size.x - width) * 0.5f,
                        (framebuffer_size.y - height) * 0.5f,
                        width,
                        height};
    }

    // pixel is measured from the top-left with y increasing downward, which is
    // how a cursor is reported. The world has y increasing upward, so this is
    // where the flip happens.
    inline math::Vec2 world_from_pixel(math::Vec2 pixel,
                                       math::Vec2 framebuffer_size,
                                       Viewport viewport,
                                       math::Vec2 world_size)
    {
        if (viewport.width <= 0.0f || viewport.height <= 0.0f)
        {
            return math::Vec2{};
        }

        const float pixel_from_bottom = framebuffer_size.y - pixel.y;

        // How far along each axis of the viewport, 0 at one edge and 1 at the other.
        const float x_fraction = (pixel.x - viewport.x) / viewport.width;
        const float y_fraction = (pixel_from_bottom - viewport.y) / viewport.height;

        return math::Vec2{x_fraction * world_size.x, y_fraction * world_size.y};
    }
}
