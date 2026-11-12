#include <cstddef>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "game/Level.h"
#include "game/LevelCheck.h"
#include "game/LevelLayout.h"
#include "game/Spawn.h"
#include "game/Systems.h"

namespace game
{
    namespace
    {
        constexpr std::string_view BOM = "\xEF\xBB\xBF";

        // Far beyond any real play area; only stops the search on a world
        // size that makes no sense.
        constexpr int SEARCH_LIMIT = 1000;

        constexpr const char* ALLOWED = "a level may only contain digits, '#', '.' and spaces";

        // The same lines parse_level sees: split on LF, one trailing CR
        // removed. It has to stay that way, or the rows counted here would not
        // be the rows the game spawns.
        std::vector<std::string_view> split_lines(std::string_view text)
        {
            std::vector<std::string_view> lines;
            std::size_t start = 0;

            while (true)
            {
                const std::size_t end = text.find('\n', start);
                std::string_view line = (end == std::string_view::npos)
                                            ? text.substr(start)
                                            : text.substr(start, end - start);

                if (!line.empty() && line.back() == '\r')
                {
                    line.remove_suffix(1);
                }

                lines.push_back(line);

                if (end == std::string_view::npos)
                {
                    break;
                }
                start = end + 1;
            }

            return lines;
        }

        // parse_level's definition. A tab is reported before this matters.
        bool is_blank(std::string_view line)
        {
            return line.find_first_not_of(" \t") == std::string_view::npos;
        }

        bool is_allowed(char character)
        {
            return character == '.' || character == ' ' || character == '#' ||
                   (character >= '0' && character <= '9');
        }

        std::string describe(char character)
        {
            if (character == '\t')
            {
                return "a tab";
            }
            if (character == '\r')
            {
                return "a carriage return";
            }

            const auto byte = static_cast<unsigned char>(character);
            if (byte >= 0x21 && byte <= 0x7E)
            {
                return std::string("'") + character + "'";
            }

            // Control characters and anything outside ASCII, one byte at a time.
            char hex[8];
            std::snprintf(hex, sizeof(hex), "0x%02X", static_cast<unsigned>(byte));
            return std::string("byte ") + hex;
        }

        LevelProblem problem_at(std::size_t line_index, std::size_t column_index, std::string message)
        {
            return LevelProblem{
                static_cast<int>(line_index) + 1,
                static_cast<int>(column_index) + 1,
                std::move(message)
            };
        }
    }

    int max_level_rows(math::Vec2 world_size)
    {
        // Where serve_system parks the ball: on top of the paddle, a gap above.
        const float held_ball_top = PADDLE_Y + PADDLE_HEIGHT * 0.5f + SERVE_GAP + 2.0f * BALL_RADIUS;

        // Row heights do not depend on the number of columns; one will do.
        Level probe;
        probe.columns = 1;
        const float half_height = brick_size(probe, world_size).y * 0.5f;

        int rows = 0;
        while (rows < SEARCH_LIMIT)
        {
            const float bottom = brick_center(probe, world_size, 0, rows).y - half_height;
            if (bottom <= held_ball_top)
            {
                break;
            }
            ++rows;
        }

        return rows;
    }

    int max_level_columns(math::Vec2 world_size)
    {
        // The same reasoning as the static_assert on BRICK_HEIGHT in Systems.h,
        // turned sideways.
        const float longest_step = MAX_BALL_SPEED * FIXED_DT;

        Level probe;
        int columns = 0;
        while (columns < SEARCH_LIMIT)
        {
            probe.columns = columns + 1;
            if (brick_size(probe, world_size).x <= longest_step)
            {
                break;
            }
            ++columns;
        }

        return columns;
    }

    std::string_view skip_bom(std::string_view text)
    {
        if (text.substr(0, BOM.size()) == BOM)
        {
            text.remove_prefix(BOM.size());
        }

        return text;
    }

    std::optional<LevelProblem> check_level_text(std::string_view text, math::Vec2 world_size)
    {
        const std::vector<std::string_view> lines = split_lines(skip_bom(text));

        // Blank lines above and below the grid are not rows, exactly as in
        // parse_level. Only lines in between count towards rows and width.
        std::size_t first = 0;
        while (first < lines.size() && is_blank(lines[first]))
        {
            ++first;
        }

        std::size_t last = lines.size();
        while (last > first && is_blank(lines[last - 1]))
        {
            --last;
        }

        const std::size_t max_rows = static_cast<std::size_t>(max_level_rows(world_size));
        const std::size_t max_columns = static_cast<std::size_t>(max_level_columns(world_size));

        // Steel counts as a brick for the limits, but only a brick that can
        // be broken makes a level that can be cleared.
        bool has_bricks = false;
        bool has_breakable_bricks = false;

        // One pass in reading order, so the problem reported is the first one
        // a person reading the file would reach.
        for (std::size_t line_index = 0; line_index < lines.size(); ++line_index)
        {
            const std::string_view line = lines[line_index];
            const bool in_grid = line_index >= first && line_index < last;

            for (std::size_t column = 0; column < line.size(); ++column)
            {
                const char character = line[column];

                if (!is_allowed(character))
                {
                    return problem_at(line_index, column,
                                      describe(character) + " is not allowed: " + ALLOWED);
                }

                if (in_grid && column >= max_columns)
                {
                    return problem_at(line_index, column,
                                      "the row is " + std::to_string(line.size()) +
                                      " columns wide, but at most " + std::to_string(max_columns) +
                                      " fit (trailing spaces count too)");
                }

                const bool breakable = character >= '1' && character <= '9';
                if (breakable || character == '#')
                {
                    has_bricks = true;
                    has_breakable_bricks = has_breakable_bricks || breakable;

                    // A brick is never on a blank line, so in_grid holds here.
                    const std::size_t row = line_index - first;
                    if (row >= max_rows)
                    {
                        return problem_at(line_index, column,
                                          "a brick in row " + std::to_string(row + 1) +
                                          " is too low: at most " + std::to_string(max_rows) +
                                          " rows fit above the paddle");
                    }
                }
            }
        }

        if (first == last)
        {
            return LevelProblem{0, 0, "the file is empty"};
        }

        if (!has_bricks)
        {
            return LevelProblem{0, 0, "the level has no bricks"};
        }

        if (!has_breakable_bricks)
        {
            return LevelProblem{0, 0, "the level has only steel bricks, which cannot be broken"};
        }

        return std::nullopt;
    }
}
