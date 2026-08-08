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

    Level editor_canvas(const Level& source)
    {
        Level canvas;
        canvas.columns = EDITOR_COLUMNS;
        canvas.rows = EDITOR_ROWS;
        canvas.hit_points.assign(
            static_cast<std::size_t>(EDITOR_COLUMNS) * static_cast<std::size_t>(EDITOR_ROWS),
            0
        );

        for (int row = 0; row < EDITOR_ROWS; ++row)
        {
            for (int column = 0; column < EDITOR_COLUMNS; ++column)
            {
                canvas.hit_points[static_cast<std::size_t>(row) * EDITOR_COLUMNS + column] = source.at(column, row);
            }
        }

        return canvas;
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
