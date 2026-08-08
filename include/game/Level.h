#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace game
{
    // A grid of brick toughness: 0 is an empty cell, 1 to 9 are hit points.
    // Row 0 is the top row, as it reads in the text.
    struct Level
    {
        int columns = 0;
        int rows = 0;
        std::vector<int> hit_points;   // row-major, columns * rows entries

        // Returns 0 outside the grid rather than reading out of bounds.
        int at(int column, int row) const;

        bool operator==(const Level&) const = default;
    };

    // '1' to '9' set hit points; every other character is an empty cell.
    // Tolerates CRLF, blank leading and trailing lines, and ragged rows.
    Level parse_level(std::string_view text);

    // The inverse: one line per row, '.' for an empty cell, a digit otherwise,
    // and a newline after every row including the last.
    std::string to_text(const Level& level);
}
