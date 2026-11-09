#include <exception>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

#include "game/Editor.h"
#include "game/LevelCheck.h"
#include "game/LevelFiles.h"
#include "game/World.h"

namespace game
{
    namespace
    {
        // The contents, or the reason they could not be had.
        struct ReadResult
        {
            std::string contents;
            std::string error;
        };

        ReadResult read_file(const std::filesystem::path& path)
        {
            std::error_code code;
            if (!std::filesystem::is_regular_file(path, code))
            {
                // A folder with the file's name, say. Reading one with
                // ifstream opens fine and then throws on the first read.
                return ReadResult{{}, "it is not a file that can be read"};
            }

            std::ifstream file(path, std::ios::binary);
            if (!file)
            {
                return ReadResult{{}, "the file could not be opened"};
            }

            try
            {
                std::string contents{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
                if (file.bad())
                {
                    return ReadResult{{}, "the file could not be read"};
                }
                return ReadResult{std::move(contents), {}};
            }
            catch (const std::exception&)
            {
                return ReadResult{{}, "the file could not be read"};
            }
        }

        std::string describe(const std::string& file_name, const LevelProblem& problem)
        {
            // Problems with the whole file have no position to point at.
            if (problem.line == 0)
            {
                return file_name + ": " + problem.message;
            }

            return file_name + ":" + std::to_string(problem.line) + ":" +
                   std::to_string(problem.column) + ": " + problem.message;
        }
    }

    std::string level_file_name(int number)
    {
        return "level-" + std::to_string(number) + ".txt";
    }

    LoadedLevels load_levels(const std::filesystem::path& folder)
    {
        LoadedLevels loaded;

        for (int number = 1;; ++number)
        {
            const std::string file_name = level_file_name(number);
            const std::filesystem::path path = folder / file_name;

            // A missing folder is the same as an empty one: level-1.txt is
            // simply not there.
            std::error_code code;
            const bool exists = std::filesystem::exists(path, code);
            if (code)
            {
                // No permission to look, say. Reported, then treated like a
                // missing number, since nothing after it can be found either.
                loaded.errors.push_back(file_name + ": could not look for the file: " + code.message());
                break;
            }
            if (!exists)
            {
                break;
            }

            const ReadResult read = read_file(path);
            if (!read.error.empty())
            {
                loaded.errors.push_back(file_name + ": " + read.error);
                continue;
            }

            if (const std::optional<LevelProblem> problem = check_level_text(read.contents, WORLD_SIZE))
            {
                loaded.errors.push_back(describe(file_name, *problem));
                continue;
            }

            loaded.levels.push_back(parse_level(skip_bom(read.contents)));
        }

        if (loaded.levels.empty())
        {
            loaded.levels.push_back(parse_level(BUILT_IN_LEVEL));
        }

        return loaded;
    }

    int step_slot(int slot, int step)
    {
        const int moved = slot + step;
        if (moved < FIRST_SLOT)
        {
            return FIRST_SLOT;
        }
        if (moved > LAST_SLOT)
        {
            return LAST_SLOT;
        }
        return moved;
    }

    SlotResult save_slot(const std::filesystem::path& folder, int slot, const Level& canvas)
    {
        if (!has_bricks(canvas))
        {
            return SlotResult{false, "NOTHING TO SAVE", {}, {}};
        }

        const std::string file_name = level_file_name(slot);

        std::error_code code;
        std::filesystem::create_directories(folder, code);
        if (code)
        {
            return SlotResult{false, "COULD NOT SAVE",
                              file_name + ": could not create the levels folder: " + code.message(), {}};
        }

        // Binary, so the bytes on disk are exactly the bytes given.
        const std::string text = to_text(canvas);
        std::ofstream file(folder / file_name, std::ios::binary | std::ios::trunc);
        if (!file)
        {
            return SlotResult{false, "COULD NOT SAVE", file_name + ": the file could not be opened for writing", {}};
        }

        file.write(text.data(), static_cast<std::streamsize>(text.size()));
        file.close();
        if (!file)
        {
            return SlotResult{false, "COULD NOT SAVE", file_name + ": the file could not be written", {}};
        }

        return SlotResult{true, "SAVED TO SLOT " + std::to_string(slot), {}, {}};
    }

    SlotResult load_slot(const std::filesystem::path& folder, int slot)
    {
        const std::string file_name = level_file_name(slot);
        const std::filesystem::path path = folder / file_name;

        std::error_code code;
        const bool exists = std::filesystem::exists(path, code);
        if (code)
        {
            return SlotResult{false, "COULD NOT LOAD", file_name + ": could not look for the file: " + code.message(), {}};
        }
        if (!exists)
        {
            return SlotResult{false, "NOTHING SAVED IN SLOT " + std::to_string(slot), {}, {}};
        }

        const ReadResult read = read_file(path);
        if (!read.error.empty())
        {
            return SlotResult{false, "COULD NOT LOAD", file_name + ": " + read.error, {}};
        }

        // The same rules as the game, so a file the editor opens is one the
        // game would play.
        if (const std::optional<LevelProblem> problem = check_level_text(read.contents, WORLD_SIZE))
        {
            return SlotResult{false, "BAD LEVEL FILE", describe(file_name, *problem), {}};
        }

        const Level level = parse_level(skip_bom(read.contents));
        if (level.columns > EDITOR_COLUMNS || level.rows > EDITOR_ROWS)
        {
            return SlotResult{false, "TOO BIG TO EDIT",
                              file_name + ": the level is " + std::to_string(level.columns) + " by " +
                              std::to_string(level.rows) + " cells (columns by rows), but the editor holds at most " +
                              std::to_string(EDITOR_COLUMNS) + " by " + std::to_string(EDITOR_ROWS),
                              {}};
        }

        return SlotResult{true, "LOADED SLOT " + std::to_string(slot), {}, editor_canvas(level)};
    }
}
