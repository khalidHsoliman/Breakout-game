#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "game/Level.h"

namespace game
{
    // "level-1.txt", "level-2.txt", ...
    std::string level_file_name(int number);

    struct LoadedLevels
    {
        // In file order, bad files left out. Never empty: when no file is
        // usable this is the built-in level alone.
        std::vector<Level> levels;

        // One line per file that was skipped, with the file name in front:
        // "level-2.txt:3:5: ..." or "level-3.txt: the file is empty".
        // Nothing is printed here; main decides where these go.
        std::vector<std::string> errors;
    };

    // Reads level-1.txt, level-2.txt and so on from the folder until the first
    // missing number. Each file is checked with check_level_text against
    // WORLD_SIZE, and parsed only if it passes. A missing or empty folder is
    // not an error, it just means the built-in level. Does not throw.
    LoadedLevels load_levels(const std::filesystem::path& folder);

    // The editor's slots are the level files: slot n is level-n.txt.
    inline constexpr int FIRST_SLOT = 1;
    inline constexpr int LAST_SLOT = 9;

    // Moves by step and stops at the ends rather than wrapping round.
    int step_slot(int slot, int step);

    struct SlotResult
    {
        bool ok = false;

        // Short, for the editor's status line: capital letters, digits and
        // spaces only, since that is all the font can draw.
        std::string status;

        // The whole story, for stderr, with the file name in front. Empty when
        // the status says it all.
        std::string error;

        // load_slot only: the canvas to edit, when ok.
        Level level;
    };

    // Writes the canvas to level-<slot>.txt, creating the folder if needed.
    // A canvas without bricks is refused and nothing is written.
    SlotResult save_slot(const std::filesystem::path& folder, int slot, const Level& canvas);

    // Reads level-<slot>.txt into an editor canvas. Refused, leaving nothing
    // to load, when the file is missing, fails check_level_text, or is bigger
    // than the editor's grid, which would cut it down.
    SlotResult load_slot(const std::filesystem::path& folder, int slot);
}
