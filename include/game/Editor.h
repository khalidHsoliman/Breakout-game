#pragma once

#include "game/Level.h"
#include "game/LevelLayout.h"

namespace game
{
    inline constexpr int MAX_EDITABLE_HIT_POINTS = 4;
    inline constexpr int EDITOR_COLUMNS = 10;
    inline constexpr int EDITOR_ROWS = 6;

    Level editor_canvas(const Level& source);

    // Empty -> 1 -> 2 -> ... -> MAX -> steel -> empty. Above MAX -> empty.
    void cycle_cell(Level& level, Cell cell);

    void clear_cell(Level& level, Cell cell);

    // A canvas without a brick that can be broken is neither saved nor
    // played: clearing nothing would be an instant win. Steel does not count,
    // since STEEL is below zero.
    bool has_bricks(const Level& level);
}
