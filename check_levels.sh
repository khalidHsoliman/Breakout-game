#!/bin/bash
# check_levels.sh - end-to-end check of the levels feature.
#
# Run from the repo root:   bash check_levels.sh
#
# Needs only g++ with C++20. It compiles the game sources (src/game/*.cpp) and a
# small check program into a temp folder, writes real level files there, plays
# through them, and prints one line per check. Nothing is written inside the
# repo, and the temp folder is removed at the end.
#
# Last line: "RESULT: ALL 12 CHECKS PASSED" (exit 0), or how many failed (exit 1).

set -u

if [ ! -d src/game ] || [ ! -d include/game ]; then
    echo "run this from the repo root (the folder with src/ and include/)"
    exit 2
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

cat > "$work/check.cpp" <<'EOF'
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "game/Editor.h"
#include "game/LevelCheck.h"
#include "game/LevelFiles.h"
#include "game/Spawn.h"
#include "game/Systems.h"

namespace fs = std::filesystem;

namespace
{
    int passed = 0;
    int failed = 0;

    void report(bool ok, const char* name, const std::string& detail)
    {
        if (ok)
        {
            ++passed;
            std::printf("PASS  %s\n", name);
        }
        else
        {
            ++failed;
            std::printf("FAIL  %s  (%s)\n", name, detail.c_str());
        }
    }

    void write(const fs::path& path, const std::string& text)
    {
        std::ofstream file(path, std::ios::binary);
        file << text;
    }

    std::string read(const fs::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        return std::string{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }

    std::string rows(int count, const std::string& row)
    {
        std::string text;
        for (int i = 0; i < count; ++i)
        {
            text += row + "\n";
        }
        return text;
    }

    std::string state_text(const game::World& world)
    {
        return "level " + std::to_string(world.level_index + 1) + ", score " + std::to_string(world.score) +
               ", lives " + std::to_string(world.lives) + ", state " + std::to_string(static_cast<int>(world.state));
    }

    void launch(game::World& world)
    {
        if (world.state == game::GameState::Ready)
        {
            game::Input input;
            input.launch = true;
            game::step(world, input, game::FIXED_DT);
        }
    }

    // Removes every brick, then one step lets the game notice the level is clear.
    void clear_level(game::World& world)
    {
        launch(world);
        const std::vector<core::Entity> bricks = world.bricks.entities();
        for (const core::Entity brick : bricks)
        {
            world.destroy_entity(brick);
        }
        game::step(world, game::Input{}, game::FIXED_DT);
    }

    void lose_a_life(game::World& world)
    {
        launch(world);
        const core::Entity ball = world.balls.entities()[0];
        world.transforms.find(ball)->position = math::Vec2{400.0f, -100.0f};
        game::step(world, game::Input{}, game::FIXED_DT);
    }

    void press_r(game::World& world)
    {
        game::Input input;
        input.restart = true;
        game::step(world, input, game::FIXED_DT);
    }

    game::World new_world(const std::vector<game::Level>& levels)
    {
        game::World world;
        world.size = game::WORLD_SIZE;
        game::start_game(world, levels);
        return world;
    }
}

int main(int, char** argv)
{
    const fs::path root = argv[1];

    // A levels folder with good files, bad files and a gap:
    //   1 good, 2 bad character, 3 good with BOM and CRLF, 4 empty, 5 good,
    //   6 missing, 7 good but after the gap.
    const fs::path folder = root / "levels";
    fs::create_directories(folder);
    write(folder / "level-1.txt", "1.1\n");
    write(folder / "level-2.txt", "11\n1x\n");
    write(folder / "level-3.txt", "\xEF\xBB\xBF" "22\r\n.2\r\n");
    write(folder / "level-4.txt", "");
    write(folder / "level-5.txt", "333\n");
    write(folder / "level-7.txt", "1111\n");

    const game::LoadedLevels loaded = game::load_levels(folder);
    const std::size_t count = loaded.levels.size();

    std::string errors;
    for (const std::string& error : loaded.errors)
    {
        errors += "[" + error + "] ";
    }

    // 1
    report(count == 3 && loaded.levels[0] == game::parse_level("1.1") && loaded.levels[2] == game::parse_level("333"),
           "1   good files load in order, bad files are left out",
           "loaded " + std::to_string(count) + " levels, expected 3: level-1, level-3, level-5");

    // 2
    report(loaded.errors.size() == 2 && loaded.errors[0].rfind("level-2.txt:2:2: ", 0) == 0 &&
               loaded.errors[1] == "level-4.txt: the file is empty",
           "2   each bad file comes back as one error with file name, line and column",
           "errors: " + errors);

    // 3
    report(count >= 2 && loaded.levels[1] == game::parse_level("22\n.2"),
           "3   a file with BOM and CRLF loads like the same level without them",
           "level-3 did not load as the clean 2x2 level");

    // 4
    bool after_gap = false;
    for (const game::Level& level : loaded.levels)
    {
        after_gap = after_gap || level == game::parse_level("1111");
    }
    report(!after_gap && errors.find("level-7") == std::string::npos && errors.find("level-6") == std::string::npos,
           "4   reading stops at the first missing number (level-7 is ignored)",
           "level-7 was loaded or reported");

    // 5
    const game::LoadedLevels none = game::load_levels(root / "no-such-folder");
    report(none.levels.size() == 1 && none.levels[0] == game::parse_level(game::BUILT_IN_LEVEL) && none.errors.empty(),
           "5   a missing folder gives the built-in level and no errors",
           "got " + std::to_string(none.levels.size()) + " levels, " + std::to_string(none.errors.size()) + " errors");

    // 6
    {
        game::World world = new_world(loaded.levels);
        launch(world);
        world.score = 50;
        world.lives = 2;
        clear_level(world);
        const bool on_two = world.level_index == 1 && world.score == 50 && world.lives == 2 &&
                            world.state == game::GameState::Ready;
        const std::string after_first = state_text(world);
        clear_level(world);
        const bool on_three = world.level_index == 2;
        clear_level(world);
        report(on_two && on_three && world.state == game::GameState::Won,
               "6   clearing a level moves on with score and lives kept, the last one wins",
               "after level 1: " + after_first + "; at the end: " + state_text(world));
    }

    // 7
    {
        game::World world = new_world(loaded.levels);
        clear_level(world);
        clear_level(world);
        world.score = 300;
        clear_level(world);
        const bool won = world.state == game::GameState::Won;
        press_r(world);
        report(won && world.level_index == 0 && world.score == 0 && world.lives == game::STARTING_LIVES &&
                   world.state == game::GameState::Ready && world.levels.size() == 3 && world.bricks.size() == 2,
               "7   R after a win starts again from level 1 with score 0",
               "after R: " + state_text(world) + ", levels " + std::to_string(world.levels.size()) +
                   ", bricks " + std::to_string(world.bricks.size()));
    }

    // 8
    {
        game::World world = new_world(loaded.levels);
        clear_level(world);
        world.score = 90;
        for (int life = 0; life < game::STARTING_LIVES; ++life)
        {
            lose_a_life(world);
        }
        const bool over_on_two = world.state == game::GameState::GameOver && world.level_index == 1;
        press_r(world);
        report(over_on_two && world.level_index == 0 && world.score == 0 && world.lives == game::STARTING_LIVES &&
                   world.levels.size() == 3 && world.bricks.size() == 2,
               "8   R after a game over on level 2 starts again from level 1 with score 0",
               "after R: " + state_text(world) + ", levels " + std::to_string(world.levels.size()) +
                   ", bricks " + std::to_string(world.bricks.size()));
    }

    // 9
    {
        const math::Vec2 size = game::WORLD_SIZE;
        const bool rows_17 = !game::check_level_text(rows(17, "1"), size).has_value();
        const bool rows_18 = game::check_level_text(rows(18, "1"), size).has_value();
        const bool dots_18 = !game::check_level_text(rows(17, "1") + "....\n", size).has_value();
        const bool cols_47 = !game::check_level_text(std::string(47, '1'), size).has_value();
        const bool cols_48 = game::check_level_text(std::string(48, '1'), size).has_value();
        report(rows_17 && rows_18 && dots_18 && cols_47 && cols_48,
               "9   17 rows and 47 columns pass, 18 rows and 48 columns do not",
               std::string("17 rows ok=") + (rows_17 ? "yes" : "no") + ", 18 rows refused=" + (rows_18 ? "yes" : "no") +
                   ", 18th row of dots ok=" + (dots_18 ? "yes" : "no") + ", 47 ok=" + (cols_47 ? "yes" : "no") +
                   ", 48 refused=" + (cols_48 ? "yes" : "no"));
    }

    // 10
    {
        const math::Vec2 size = game::WORLD_SIZE;
        const bool empty = game::check_level_text("", size).has_value();
        const bool no_bricks = game::check_level_text("...\n. .\n", size).has_value();
        const bool tab = game::check_level_text("1\t1\n", size).has_value();
        report(empty && no_bricks && tab,
               "10  an empty file, a file with no bricks and a tab are all refused",
               std::string("empty refused=") + (empty ? "yes" : "no") + ", no bricks refused=" + (no_bricks ? "yes" : "no") +
                   ", tab refused=" + (tab ? "yes" : "no"));
    }

    // 11
    {
        const fs::path slots = root / "editor" / "levels";
        game::Level canvas = game::editor_canvas(game::Level{});
        canvas.set(0, 0, 1);
        canvas.set(game::EDITOR_COLUMNS - 1, game::EDITOR_ROWS - 1, 4);

        const game::SlotResult saved = game::save_slot(slots, 2, canvas);
        const bool file_made = fs::is_regular_file(slots / "level-2.txt");
        const game::SlotResult back = game::load_slot(slots, 2);
        const std::string before = read(slots / "level-2.txt");
        const game::SlotResult empty = game::save_slot(slots, 2, game::editor_canvas(game::Level{}));
        const bool untouched = read(slots / "level-2.txt") == before;

        report(saved.ok && file_made && back.ok && back.level == canvas && !empty.ok && untouched,
               "11  the editor saves a slot (making the folder) and loads it back; an empty canvas is not saved",
               "save ok=" + std::string(saved.ok ? "yes" : "no") + ", file made=" + (file_made ? "yes" : "no") +
                   ", load ok=" + (back.ok ? "yes" : "no") + ", same canvas=" + (back.level == canvas ? "yes" : "no") +
                   ", empty refused=" + (!empty.ok ? "yes" : "no") + ", file untouched=" + (untouched ? "yes" : "no"));
    }

    // 12
    {
        const fs::path slots = root / "editor" / "levels";
        write(slots / "level-3.txt", "11111111111\n");
        const game::SlotResult big = game::load_slot(slots, 3);
        const bool ends = game::step_slot(1, -1) == 1 && game::step_slot(9, 1) == 9 && game::step_slot(4, 1) == 5;
        report(!big.ok && big.status == "TOO BIG TO EDIT" && ends,
               "12  a file bigger than 10x6 is refused with TOO BIG TO EDIT; slots stop at 1 and 9",
               "load status '" + big.status + "', slot ends ok=" + (ends ? "yes" : "no"));
    }

    if (failed == 0)
    {
        std::printf("RESULT: ALL %d CHECKS PASSED\n", passed);
        return 0;
    }

    std::printf("RESULT: %d OF %d CHECKS FAILED\n", failed, passed + failed);
    return 1;
}
EOF

echo "building the game sources and the check program..."
if ! g++ -std=c++20 -Iinclude "$work/check.cpp" src/game/*.cpp -o "$work/check" 2> "$work/build.log"; then
    echo "the check program does not build against this code. First errors:"
    grep -m 3 "error" "$work/build.log"
    echo "RESULT: 0 OF 12 CHECKS PASSED (nothing could run)"
    exit 1
fi

"$work/check" "$work"
