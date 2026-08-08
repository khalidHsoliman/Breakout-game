#pragma once

#include "game/Level.h"
#include "game/LevelLayout.h"

namespace game
{
    // The editor only offers what the palette can distinguish. brick_color
    // gives 1 to 4 their own colour and everything above the same steel, so a
    // level built here always reads at a glance.
    inline constexpr int MAX_EDITABLE_HIT_POINTS = 4;

    // Empty -> 1 -> 2 -> ... -> MAX -> empty. Hit points above the cap, which
    // only a hand-authored file can contain, clear rather than continuing up.
    void cycle_cell(Level& level, Cell cell);

    void clear_cell(Level& level, Cell cell);
}
