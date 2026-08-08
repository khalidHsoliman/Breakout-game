#include "game/LevelLayout.h"

namespace game
{
    namespace
    {
        constexpr float CELL_HEIGHT = BRICK_HEIGHT + BRICK_GAP;

        // A cell is wider than the brick it holds: the gap is part of the cell.
        float cell_width(const Level& level, math::Vec2 world_size)
        {
            if (level.columns <= 0)
            {
                return 0.0f;
            }

            return (world_size.x - 2.0f * BRICK_SIDE_MARGIN) / static_cast<float>(level.columns);
        }
    }

    math::Vec2 brick_size(const Level& level, math::Vec2 world_size)
    {
        return math::Vec2{cell_width(level, world_size) - BRICK_GAP, BRICK_HEIGHT};
    }

    math::Vec2 brick_center(const Level& level, math::Vec2 world_size, int column, int row)
    {
        return math::Vec2{
            BRICK_SIDE_MARGIN + (static_cast<float>(column) + 0.5f) * cell_width(level, world_size),
            world_size.y - BRICK_TOP_MARGIN - (static_cast<float>(row) + 0.5f) * CELL_HEIGHT};
    }

    Cell cell_at(const Level& level, math::Vec2 world_size, math::Vec2 position)
    {
        const float width = cell_width(level, world_size);
        if (width <= 0.0f || level.rows <= 0)
        {
            return Cell{};
        }

        const float from_left = position.x - BRICK_SIDE_MARGIN;
        const float from_top = world_size.y - BRICK_TOP_MARGIN - position.y;

        // Checked before the cast, which truncates towards zero and would turn
        // a position just outside the grid into cell 0.
        if (from_left < 0.0f || from_top < 0.0f)
        {
            return Cell{};
        }

        const int column = static_cast<int>(from_left / width);
        const int row = static_cast<int>(from_top / CELL_HEIGHT);

        if (column >= level.columns || row >= level.rows)
        {
            return Cell{};
        }

        return Cell{column, row, true};
    }
}
