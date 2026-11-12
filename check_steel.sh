#!/bin/bash
# check_steel.sh - end-to-end check of steel bricks.
#
# Run from the repo root:   bash check_steel.sh
#
# Needs only g++ with C++20. It compiles the game sources (src/game/*.cpp) and a
# small check program into a temp folder, writes real level files there, plays
# through game::step, uses the editor functions, and prints one line per check.
# Nothing is written inside the repo, and the temp folder is removed at the end.
#
# Last line: "RESULT: ALL 11 CHECKS PASSED" (exit 0), or how many failed (exit 1).

set -u

if [ ! -d src/game ] || [ ! -d include/game ]; then
    echo "run this from the repo root (the folder with src/ and include/)"
    exit 2
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

cat > "$work/check.cpp" <<'EOF'
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "game/Editor.h"
#include "game/Level.h"
#include "game/LevelCheck.h"
#include "game/LevelFiles.h"
#include "game/LevelLayout.h"
#include "game/Spawn.h"
#include "game/Systems.h"

namespace fs = std::filesystem;

namespace
{
    constexpr int TOTAL = 11;
    int passed = 0;
    int failed = 0;

    void report(bool ok, const char* name, const std::string& detail)
    {
        if (ok)
        {
            ++passed;
            std::printf("PASS  %s\n", name);
            std::fflush(stdout);
        }
        else
        {
            ++failed;
            std::printf("FAIL  %s  (%s)\n", name, detail.c_str());
            std::fflush(stdout);
        }
    }

    const char* yes(bool value)
    {
        return value ? "yes" : "no";
    }

    std::string num(float value)
    {
        char text[32];
        std::snprintf(text, sizeof(text), "%.2f", value);
        return text;
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

    void step(game::World& world, const game::Input& input = game::Input{})
    {
        game::step(world, input, game::FIXED_DT);
    }

    game::World new_world(const std::vector<game::Level>& levels)
    {
        game::World world;
        world.size = game::WORLD_SIZE;
        world.rng.seed(5);
        game::start_game(world, levels);
        return world;
    }

    void launch(game::World& world)
    {
        game::Input input;
        input.launch = true;
        step(world, input);
    }

    core::Entity first_ball(const game::World& world)
    {
        return world.balls.entities()[0];
    }

    bool same_color(core::Color a, core::Color b)
    {
        return a.r == b.r && a.g == b.g && a.b == b.b;
    }

    // Puts the ball just under the given entity, moving straight up.
    void aim_at(game::World& world, core::Entity target, float speed = 350.0f)
    {
        const math::Vec2 at = world.transforms.find(target)->position;
        const core::Entity ball = first_ball(world);
        world.transforms.find(ball)->position = at - math::Vec2{0.0f, 20.0f};
        world.transforms.find(ball)->previous_position = at - math::Vec2{0.0f, 20.0f};
        world.velocities.find(ball)->value = math::Vec2{0.0f, speed};
    }

    bool has_event(const game::World& world, game::GameEvent event)
    {
        for (const game::GameEvent seen : world.events)
        {
            if (seen == event)
            {
                return true;
            }
        }
        return false;
    }

    // The corridor level: a steel ceiling, an open row, steel with one gap.
    const char* CORRIDOR = "##########\n..........\n####.#####\n........1.\n";
}

int main(int, char** argv)
{
    const fs::path root = argv[1];
    const math::Vec2 size = game::WORLD_SIZE;

    // 1
    {
        const game::Level level = game::parse_level("1#1\n##.\n");
        const bool parsed = level.at(1, 0) == game::STEEL && level.at(0, 1) == game::STEEL && level.at(1, 1) == game::STEEL &&
                            level.at(0, 0) == 1 && level.at(2, 1) == 0;
        const std::string text = game::to_text(level);
        report(parsed && text == "1#1\n##.\n" && game::parse_level(text) == level,
               "1   '#' is read as steel and written back as '#'",
               std::string("parsed right=") + yes(parsed) + ", written back as '" + text + "'");
    }

    // 2
    {
        const bool allowed = !game::check_level_text("1#1\n", size).has_value();
        const auto only_steel = game::check_level_text("###\n#.#\n", size);
        const auto only_dots = game::check_level_text("...\n", size);
        const bool steel_named = only_steel.has_value() && only_steel->message.find("steel") != std::string::npos;
        const bool dots_differ = only_dots.has_value() && only_steel.has_value() && only_dots->message != only_steel->message;
        report(allowed && steel_named && dots_differ,
               "2   '#' is allowed; a file with only steel is refused, with its own message",
               std::string("1#1 allowed=") + yes(allowed) + ", only steel: '" + (only_steel ? only_steel->message : "accepted") +
                   "', only dots: '" + (only_dots ? only_dots->message : "accepted") + "'");
    }

    // 3
    {
        const bool row_17 = !game::check_level_text("1\n" + rows(15, ".") + "#\n", size).has_value();
        const auto row_18 = game::check_level_text("1\n" + rows(16, ".") + "#\n", size);
        const bool cols_47 = !game::check_level_text("1" + std::string(46, '#') + "\n", size).has_value();
        const bool cols_48 = game::check_level_text("1" + std::string(47, '#') + "\n", size).has_value();
        const bool row_18_refused = row_18.has_value() && row_18->line == 18;
        report(row_17 && row_18_refused && cols_47 && cols_48,
               "3   steel counts for the 17 row and 47 column limits",
               std::string("steel in row 17 ok=") + yes(row_17) + ", row 18 refused at line 18=" + yes(row_18_refused) +
                   ", 47 wide ok=" + yes(cols_47) + ", 48 wide refused=" + yes(cols_48));
    }

    // 4
    {
        const fs::path folder = root / "levels";
        fs::create_directories(folder);
        write(folder / "level-1.txt", "#1#\n");
        write(folder / "level-2.txt", "###\n");
        write(folder / "level-3.txt", "1\n");
        const game::LoadedLevels loaded = game::load_levels(folder);
        const bool levels = loaded.levels.size() == 2 && loaded.levels[0] == game::parse_level("#1#") &&
                            loaded.levels[1] == game::parse_level("1");
        const bool error = loaded.errors.size() == 1 && loaded.errors[0].rfind("level-2.txt", 0) == 0;
        report(levels && error,
               "4   a levels folder loads steel, and skips a steel-only file with an error",
               "loaded " + std::to_string(loaded.levels.size()) + " levels, " + std::to_string(loaded.errors.size()) +
                   " errors" + (loaded.errors.empty() ? std::string() : ": " + loaded.errors[0]));
    }

    // 5
    {
        game::World world = new_world({game::parse_level("#1#")});
        bool grey = world.steel.size() == 2;
        for (const core::Entity steel : world.steel.entities())
        {
            const core::Color color = *world.colors.find(steel);
            for (int hit_points = 1; hit_points <= 9; ++hit_points)
            {
                grey = grey && !same_color(color, game::brick_color(hit_points));
            }
            grey = grey && !world.bricks.has(steel);
        }
        report(world.steel.size() == 2 && world.bricks.size() == 1 && grey,
               "5   steel spawns in its own store, not as a brick, in a colour no brick has",
               std::to_string(world.steel.size()) + " steel, " + std::to_string(world.bricks.size()) + " bricks, own colour=" +
                   yes(grey));
    }

    // 6
    {
        game::World world = new_world({game::parse_level("#1#")});
        world.capsule_drop_chance = 1.0f;
        launch(world);
        if (world.steel.size() == 0 || world.balls.size() == 0)
        {
            report(false, "6   a steel hit bounces the ball and does nothing else: no score, capsule, random draw or speed-up",
                   "no steel was spawned for \"#1#\"");
        }
        else
        {
        const core::Entity steel = world.steel.entities()[0];
        aim_at(world, steel);
        const std::mt19937 rng_before = world.rng;
        world.events.clear();
        bool bounced = false;
        for (int i = 0; i < 10 && !bounced; ++i)
        {
            step(world);
            bounced = world.velocities.find(first_ball(world))->value.y < 0.0f;
        }
        const math::Vec2 v = world.velocities.find(first_ball(world))->value;
        const float speed = std::sqrt(v.x * v.x + v.y * v.y);
        const bool nothing = world.score == 0 && world.capsules.size() == 0 && world.rng == rng_before &&
                             std::abs(speed - 350.0f) < 0.01f && world.steel.has(steel);
        const bool events = has_event(world, game::GameEvent::SteelHit) && !has_event(world, game::GameEvent::BrickHit);
        report(bounced && nothing && events,
               "6   a steel hit bounces the ball and does nothing else: no score, capsule, random draw or speed-up",
               std::string("bounced=") + yes(bounced) + ", score " + std::to_string(world.score) + ", capsules " +
                   std::to_string(world.capsules.size()) + ", speed " + num(speed) + ", SteelHit and no BrickHit=" + yes(events));
        }
    }

    // 7
    {
        game::World one = new_world({game::parse_level("#1#")});
        launch(one);
        for (const core::Entity brick : std::vector<core::Entity>(one.bricks.entities()))
        {
            one.destroy_entity(brick);
        }
        step(one);
        const bool won = one.state == game::GameState::Won && one.steel.size() == 2;
        game::Input restart;
        restart.restart = true;
        step(one, restart);
        const bool rebuilt = one.state == game::GameState::Ready && one.steel.size() == 2 && one.bricks.size() == 1;

        game::World two = new_world({game::parse_level("#1#"), game::parse_level("#.1")});
        launch(two);
        for (const core::Entity brick : std::vector<core::Entity>(two.bricks.entities()))
        {
            two.destroy_entity(brick);
        }
        step(two);
        const bool moved_on = two.level_index == 1 && two.steel.size() == 1 && two.bricks.size() == 1;
        report(won && rebuilt && moved_on,
               "7   a level is cleared when only steel is left; the next level and R bring the right steel",
               std::string("won with steel left=") + yes(won) + ", R rebuilds it=" + yes(rebuilt) +
                   ", next level has its own steel=" + yes(moved_on));
    }

    // 8
    {
        game::Level canvas = game::editor_canvas(game::Level{});
        const game::Cell cell{0, 0, true};
        std::vector<int> seen;
        for (int click = 0; click < 6; ++click)
        {
            game::cycle_cell(canvas, cell);
            seen.push_back(canvas.at(0, 0));
        }
        canvas.set(1, 0, 7);
        game::cycle_cell(canvas, game::Cell{1, 0, true});
        const std::vector<int> expected{1, 2, 3, 4, game::STEEL, 0};
        std::string got;
        for (const int value : seen)
        {
            got += std::to_string(value) + " ";
        }
        report(seen == expected && canvas.at(1, 0) == 0,
               "8   an editor click cycles empty, 1, 2, 3, 4, steel, empty, and a 7 goes to empty",
               "clicks gave " + got + "(steel is " + std::to_string(game::STEEL) + "), a 7 went to " + std::to_string(canvas.at(1, 0)));
    }

    // 9
    {
        const fs::path slots = root / "editor" / "levels";
        game::Level mixed = game::editor_canvas(game::Level{});
        mixed.set(0, 0, game::STEEL);
        mixed.set(1, 0, 2);
        const game::SlotResult saved = game::save_slot(slots, 1, mixed);
        const game::SlotResult back = game::load_slot(slots, 1);
        const bool hash_in_file = read(slots / "level-1.txt").rfind("#2", 0) == 0;

        game::Level only_steel = game::editor_canvas(game::Level{});
        only_steel.set(3, 3, game::STEEL);
        const std::string before = read(slots / "level-1.txt");
        const game::SlotResult refused = game::save_slot(slots, 1, only_steel);
        const bool untouched = read(slots / "level-1.txt") == before;
        report(saved.ok && back.ok && back.level == mixed && hash_in_file && !game::has_bricks(only_steel) && !refused.ok &&
                   refused.status == "NOTHING TO SAVE" && untouched,
               "9   a slot keeps steel as '#'; a canvas with only steel is not saved and the file stays as it was",
               std::string("saved and loaded back=") + yes(saved.ok && back.ok && back.level == mixed) + ", '#' in file=" +
                   yes(hash_in_file) + ", steel-only status '" + refused.status + "', file untouched=" + yes(untouched));
    }

    // 10
    {
        const game::Level level = game::parse_level(CORRIDOR);
        game::World world = new_world({level});
        launch(world);
        const core::Entity ball = first_ball(world);
        const math::Vec2 inside = game::brick_center(level, size, 2, 1);
        const float angle = 11.0f * 3.14159265f / 180.0f;
        world.transforms.find(ball)->position = inside;
        world.transforms.find(ball)->previous_position = inside;
        world.velocities.find(ball)->value = math::Vec2{std::sin(angle), std::cos(angle)} * 350.0f;

        const int lives = world.lives;
        int steps = 0;
        while (steps < 60 * 15 && world.state == game::GameState::Playing)
        {
            step(world);
            ++steps;
        }
        const bool held = world.state == game::GameState::Ready && world.balls.size() == 1;
        report(held && world.lives == lives && steps <= 60 * 10 + 2,
               "10  a ball stuck between steel goes back onto the paddle within 10 seconds, with no life lost",
               std::string("back on the paddle=") + yes(held) + " after " + num(steps / 60.0f) + " s, lives " +
                   std::to_string(lives) + " -> " + std::to_string(world.lives));
    }

    // 11
    {
        game::World world = new_world({game::parse_level("1\n")});
        launch(world);
        const core::Entity ball = first_ball(world);
        world.transforms.find(ball)->position = math::Vec2{400.0f, 300.0f};
        world.transforms.find(ball)->previous_position = math::Vec2{400.0f, 300.0f};
        world.velocities.find(ball)->value = math::Vec2{350.0f, 0.5f};
        for (int i = 0; i < 60 * 12; ++i)
        {
            step(world);
        }
        const math::Vec2 v = world.balls.size() > 0 ? world.velocities.find(first_ball(world))->value : math::Vec2{0.0f, 99.0f};
        report(world.state == game::GameState::Playing && world.lives == game::STARTING_LIVES && std::abs(v.y) < 1.0f,
               "11  in a level without steel nothing changes: a flat ball still crosses wall to wall after 12 s",
               std::string("still playing=") + yes(world.state == game::GameState::Playing) + ", lives " +
                   std::to_string(world.lives) + ", vertical speed " + num(v.y));
    }

    if (failed == 0)
    {
        std::printf("RESULT: ALL %d CHECKS PASSED\n", passed);
        return 0;
    }

    std::printf("RESULT: %d OF %d CHECKS FAILED\n", failed, TOTAL);
    return 1;
}
EOF

echo "building the game sources and the check program..."
if ! g++ -std=c++20 -Iinclude "$work/check.cpp" src/game/*.cpp -o "$work/check" 2> "$work/build.log"; then
    echo "the check program does not build against this code. First errors:"
    grep -m 3 "error" "$work/build.log"
    echo "RESULT: 0 OF 11 CHECKS PASSED (nothing could run)"
    exit 1
fi

"$work/check" "$work"
code=$?
if [ $code -gt 128 ]; then
    echo "RESULT: THE CHECK PROGRAM CRASHED (signal $((code - 128))), so not every check ran"
    exit 1
fi
exit $code
