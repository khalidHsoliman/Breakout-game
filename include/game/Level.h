#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace game
{
    // Row 0 is the top row, as it reads in the text.
    struct Level
    {
        int columns = 0;
        int rows = 0;
        std::vector<int> hit_points;   // row-major, columns * rows entries

        // Returns 0 outside the grid rather than reading out of bounds.
        int at(int column, int row) const;
        void set(int column, int row, int value);

        bool operator==(const Level&) const = default;
    };

    // The level played when there are no level files. Digits are hit points,
    // '.' is an empty cell.
    inline constexpr const char* BUILT_IN_LEVEL = R"(
..111111..
.12222221.
1233333321
.12222221.
..111111..
)";

    // Tolerates CRLF, blank leading and trailing lines, and ragged rows.
    Level parse_level(std::string_view text);

    // The inverse: one line per row, '.' for an empty cell, a digit otherwise,
    // and a newline after every row including the last.
    std::string to_text(const Level& level);
}
