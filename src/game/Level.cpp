#include <cstddef>

#include "game/Level.h"

namespace game
{
    namespace
    {
        bool is_blank(std::string_view line)
        {
            return line.find_first_not_of(" \t") == std::string_view::npos;
        }

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

                // A file written on Windows leaves the carriage return behind.
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
    }

    int Level::at(int column, int row) const
    {
        if (column < 0 || row < 0 || column >= columns || row >= rows)
        {
            return 0;
        }

        const std::size_t index =
            static_cast<std::size_t>(row) * static_cast<std::size_t>(columns) +
            static_cast<std::size_t>(column);

        return hit_points[index];
    }

    Level parse_level(std::string_view text)
    {
        const std::vector<std::string_view> lines = split_lines(text);

        // Only the outer blank lines go, so a raw string literal can breathe.
        // Blank rows in the middle are real: they are gaps in the layout.
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

        Level level;
        level.rows = static_cast<int>(last - first);

        for (std::size_t i = first; i < last; ++i)
        {
            level.columns = (std::max)(level.columns, static_cast<int>(lines[i].size()));
        }

        if (level.rows == 0 || level.columns == 0)
        {
            return Level{};
        }

        // Short rows keep their trailing zeros, so a file with trailing spaces
        // stripped still loads.
        level.hit_points.assign(
            static_cast<std::size_t>(level.rows) * static_cast<std::size_t>(level.columns), 0);

        for (std::size_t i = first; i < last; ++i)
        {
            const std::string_view line = lines[i];
            const std::size_t row = i - first;

            for (std::size_t column = 0; column < line.size(); ++column)
            {
                const char character = line[column];
                if (character < '1' || character > '9')
                {
                    continue;
                }

                level.hit_points[row * static_cast<std::size_t>(level.columns) + column] =
                    character - '0';
            }
        }

        return level;
    }
}
