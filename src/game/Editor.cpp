#include <cstddef>

#include "game/Editor.h"

namespace game
{
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
                canvas.set(column, row, source.at(column, row));
            }
        }

        return canvas;
    }

    void cycle_cell(Level& level, Cell cell)
    {
        if (!cell.inside)
        {
            return;
        }

        const int hit_points = level.at(cell.column, cell.row);

        // Steel comes after the toughest brick. 5 to 9, from a file, go to
        // empty rather than to steel, as they did before steel existed.
        int next = hit_points + 1;
        if (hit_points == MAX_EDITABLE_HIT_POINTS)
        {
            next = STEEL;
        }
        else if (hit_points == STEEL || hit_points > MAX_EDITABLE_HIT_POINTS)
        {
            next = 0;
        }

        level.set(cell.column, cell.row, next);
    }

    void clear_cell(Level& level, Cell cell)
    {
        if (!cell.inside)
        {
            return;
        }

        level.set(cell.column, cell.row, 0);
    }

    bool has_bricks(const Level& level)
    {
        for (const int hit_points : level.hit_points)
        {
            if (hit_points > 0)
            {
                return true;
            }
        }
        return false;
    }
}
