#include <cstddef>

#include "game/Editor.h"

namespace game
{
    namespace
    {
        int* mutable_cell(Level& level, Cell cell)
        {
            if (!cell.inside || cell.column < 0 || cell.row < 0 ||
                cell.column >= level.columns || cell.row >= level.rows)
            {
                return nullptr;
            }

            const std::size_t index =
                static_cast<std::size_t>(cell.row) * static_cast<std::size_t>(level.columns) +
                static_cast<std::size_t>(cell.column);

            return &level.hit_points[index];
        }
    }

    void cycle_cell(Level& level, Cell cell)
    {
        int* hit_points = mutable_cell(level, cell);
        if (hit_points == nullptr)
        {
            return;
        }

        *hit_points = (*hit_points >= MAX_EDITABLE_HIT_POINTS) ? 0 : *hit_points + 1;
    }

    void clear_cell(Level& level, Cell cell)
    {
        int* hit_points = mutable_cell(level, cell);
        if (hit_points != nullptr)
        {
            *hit_points = 0;
        }
    }
}
