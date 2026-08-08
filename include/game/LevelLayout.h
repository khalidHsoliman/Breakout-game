#pragma once

#include "game/Level.h"
#include "math/Vec2.h"

namespace game
{
    // both the spawned level and the editor use the same layout
    inline constexpr float BRICK_SIDE_MARGIN = 20.0f;
    inline constexpr float BRICK_TOP_MARGIN = 40.0f;
    inline constexpr float BRICK_HEIGHT = 24.0f;
    inline constexpr float BRICK_GAP = 4.0f;

    struct Cell
    {
        int column = 0;
        int row = 0;

        // False when the position was outside the grid.
        bool inside = false;
    };

    math::Vec2 brick_size(const Level& level, math::Vec2 world_size);
    math::Vec2 brick_center(const Level& level, math::Vec2 world_size, int column, int row);

    // The inverse of brick_center.
    Cell cell_at(const Level& level, math::Vec2 world_size, math::Vec2 position);
}
