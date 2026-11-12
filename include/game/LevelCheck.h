#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "math/Vec2.h"

namespace game
{
    // What is wrong with a level file, and where. Line and column count from 1
    // and are positions in the file itself: blank lines at the top count, a
    // leading byte order mark does not. Both are 0 when the problem is the
    // file as a whole (empty, no bricks, or only steel bricks).
    struct LevelProblem
    {
        int line = 0;
        int column = 0;
        std::string message;

        bool operator==(const LevelProblem&) const = default;
    };

    // The lowest row a brick may sit in, counted from the first non-blank
    // line, is max_level_rows - 1: any lower and it overlaps the ball held on
    // the paddle. Worked out from the same layout the game spawns with.
    int max_level_rows(math::Vec2 world_size);

    // Any more columns and a brick is narrower than one step of the ball at
    // full speed, so the ball could pass through it sideways.
    int max_level_columns(math::Vec2 world_size);

    // A file saved by Notepad may start with a UTF-8 byte order mark.
    // parse_level does not know about it, so text has to go through this
    // before it is parsed.
    std::string_view skip_bom(std::string_view text);

    // The first problem in the raw text of a level file, in reading order, or
    // nothing if the game can play it. Allowed: digits ('0' is empty), '#'
    // (steel, which counts towards the row and column limits but not as a
    // brick that can be broken), '.', spaces, CRLF or LF line ends, rows of
    // different lengths, and a leading byte order mark. Separate from parse_level, which stays tolerant for the
    // editor and the tests.
    std::optional<LevelProblem> check_level_text(std::string_view text, math::Vec2 world_size);
}
