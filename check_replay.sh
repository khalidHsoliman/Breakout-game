#!/bin/bash
# check_replay.sh - end-to-end check of replays.
#
# Run from the repo root:   bash check_replay.sh
#
# Needs only g++ with C++20. It compiles the game sources (src/game/*.cpp), the
# breakout_replay program (src/breakout_replay.cpp) and a small check program
# into a temp folder. The check program plays seeded games through the
# recorder, saves and loads them, plays them back, and runs breakout_replay on
# good, edited and broken files. It prints one line per check. Nothing is
# written inside the repo, and the temp folder is removed at the end.
#
# Last line: "RESULT: ALL 14 CHECKS PASSED" (exit 0), or how many failed (exit 1).

set -u

if [ ! -d src/game ] || [ ! -d include/game ]; then
    echo "run this from the repo root (the folder with src/ and include/)"
    exit 2
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

cat > "$work/check.cpp" <<'EOF'
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <vector>

#include "game/Editor.h"
#include "game/Level.h"
#include "game/Replay.h"
#include "game/Spawn.h"
#include "game/Systems.h"

namespace fs = std::filesystem;

namespace
{
    int passed = 0;
    int failed = 0;
    fs::path root;
    std::string program;

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
        std::fflush(stdout);
    }

    std::string read(const fs::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        return std::string{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }

    void write(const fs::path& path, const std::string& text)
    {
        std::ofstream file(path, std::ios::binary);
        file << text;
    }

    std::vector<std::string> lines_of(const std::string& text)
    {
        std::vector<std::string> lines;
        std::istringstream in(text);
        for (std::string line; std::getline(in, line);)
        {
            lines.push_back(line);
        }
        return lines;
    }

    std::string join(const std::vector<std::string>& lines)
    {
        std::string text;
        for (const std::string& line : lines)
        {
            text += line + "\n";
        }
        return text;
    }

    std::string last_line(const std::string& text)
    {
        const std::vector<std::string> lines = lines_of(text);
        return lines.empty() ? "" : lines.back();
    }

    bool starts_with(const std::string& text, const std::string& start)
    {
        return text.rfind(start, 0) == 0;
    }

    bool has_line(const std::string& text, const std::string& line)
    {
        for (const std::string& each : lines_of(text))
        {
            if (each == line)
            {
                return true;
            }
        }
        return false;
    }

    std::string one_line(std::string text)
    {
        for (char& c : text)
        {
            if (c == '\n')
            {
                c = '|';
            }
        }
        return text;
    }

    struct Run
    {
        int exit = -1;
        std::string out;
        std::string err;
    };

    // Runs breakout_replay with these arguments (already quoted).
    Run run_program(const std::string& arguments)
    {
        const fs::path out = root / "out.txt";
        const fs::path err = root / "err.txt";
        const std::string command = "\"" + program + "\" " + arguments + " > \"" + out.string() + "\" 2> \"" +
                                    err.string() + "\"";
        const int status = std::system(command.c_str());
        Run run;
        run.exit = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        run.out = read(out);
        run.err = read(err);
        return run;
    }

    Run run_on(const fs::path& file)
    {
        return run_program("\"" + file.string() + "\"");
    }

    std::string hex(float value)
    {
        char buffer[64];
        std::snprintf(buffer, sizeof buffer, "%a", static_cast<double>(value));
        return buffer;
    }

    std::string at(const game::World& world, core::Entity entity)
    {
        const game::Transform* t = world.transforms.find(entity);
        if (t == nullptr)
        {
            return " (no transform)";
        }
        return " " + hex(t->position.x) + " " + hex(t->position.y) + " size " + hex(t->size.x) + " " + hex(t->size.y);
    }

    // Everything that decides how a game goes on, in store order, with exact
    // floats. Entity ids are left out: they keep counting after R.
    std::string describe(const game::World& world)
    {
        std::string text = "score " + std::to_string(world.score) + " lives " + std::to_string(world.lives) +
                           " level " + std::to_string(world.level_index) + " state " +
                           std::to_string(static_cast<int>(world.state)) + " draws " +
                           std::to_string(world.rng_draws) + "\n";
        for (const core::Entity e : world.balls.entities())
        {
            const game::Velocity* v = world.velocities.find(e);
            text += "ball" + at(world, e) + " v " + (v ? hex(v->value.x) + " " + hex(v->value.y) : "none") + "\n";
        }
        for (const core::Entity e : world.paddles.entities())
        {
            text += "paddle" + at(world, e) + "\n";
        }
        for (const core::Entity e : world.bricks.entities())
        {
            text += "brick" + at(world, e) + " hp " + std::to_string(world.bricks.find(e)->hit_points) + "\n";
        }
        for (const core::Entity e : world.steel.entities())
        {
            text += "steel" + at(world, e) + "\n";
        }
        for (const core::Entity e : world.capsules.entities())
        {
            text += "capsule" + at(world, e) + " kind " +
                    std::to_string(static_cast<int>(world.capsules.find(e)->kind)) + "\n";
        }
        return text;
    }

    bool same_world(const game::World& a, const game::World& b)
    {
        return describe(a) == describe(b) && a.rng == b.rng;
    }

    std::string first_difference(const game::World& a, const game::World& b)
    {
        const std::vector<std::string> x = lines_of(describe(a));
        const std::vector<std::string> y = lines_of(describe(b));
        for (std::size_t i = 0; i < x.size() || i < y.size(); ++i)
        {
            const std::string p = i < x.size() ? x[i] : "(nothing)";
            const std::string q = i < y.size() ? y[i] : "(nothing)";
            if (p != q)
            {
                return "live: " + p + " / replay: " + q;
            }
        }
        return a.rng == b.rng ? "no difference" : "the generators differ";
    }

    // The paddle stays on the left and the ball is launched whenever it waits,
    // so the game is lost quickly. One pause of 60 steps, once the ball is in
    // play. Returns true when the recorder says the game ended.
    struct Pilot
    {
        int steps = 0;
        int paused_steps = 0;
        int pause_started = -1;
        bool paused_once = false;

        game::Input next(const game::World& world)
        {
            game::Input input;
            input.move_left = true;
            input.launch = world.state == game::GameState::Ready;
            if (!paused_once && steps > 100 && world.state == game::GameState::Playing)
            {
                input.toggle_pause = true;
                paused_once = true;
                pause_started = steps;
            }
            if (world.state == game::GameState::Paused && steps - pause_started >= 60)
            {
                input.toggle_pause = true;
            }
            return input;
        }

        bool play_to_end(game::World& world, game::Recorder& recorder, int limit = 30000)
        {
            for (int i = 0; i < limit; ++i)
            {
                const game::Input input = next(world);
                const bool ended = recorder.step(world, input);
                world.events.clear();
                ++steps;
                if (world.state == game::GameState::Paused)
                {
                    ++paused_steps;
                }
                if (ended)
                {
                    return true;
                }
            }
            return false;
        }
    };

    int count_pause_runs(const game::Replay& replay)
    {
        int count = 0;
        for (const game::ReplayRun& run : replay.runs)
        {
            count += run.input.toggle_pause ? 1 : 0;
        }
        return count;
    }

    // Index of the "steps" line and of the end line in a replay's text.
    struct Layout
    {
        std::size_t steps = 0;
        std::size_t end = 0;
    };

    Layout layout_of(const std::vector<std::string>& lines)
    {
        Layout layout;
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            if (lines[i] == "steps")
            {
                layout.steps = i;
            }
            if (starts_with(lines[i], "end steps "))
            {
                layout.end = i;
            }
        }
        return layout;
    }

    // Replaces the number after "<word> " on the end line.
    std::string with_number(const std::string& end_line, const std::string& word, long long value)
    {
        std::vector<std::string> parts;
        std::istringstream in(end_line);
        for (std::string part; in >> part;)
        {
            parts.push_back(part);
        }
        std::string out;
        for (std::size_t i = 0; i < parts.size(); ++i)
        {
            if (i > 0 && parts[i - 1] == word)
            {
                parts[i] = std::to_string(value);
            }
            out += (i > 0 ? " " : "") + parts[i];
        }
        return out;
    }

    long long number_after(const std::string& end_line, const std::string& word)
    {
        std::istringstream in(end_line);
        std::string previous;
        for (std::string part; in >> part;)
        {
            if (previous == word)
            {
                return std::atoll(part.c_str());
            }
            previous = part;
        }
        return -1;
    }
}

int main(int, char** argv)
{
    root = argv[1];
    program = argv[2];

    const game::Level built_in = game::parse_level(game::BUILT_IN_LEVEL);
    const game::Level second = game::parse_level("1111\n");

    // Game 1: seeded, a few numbers already drawn, two levels.
    game::World live;
    live.size = game::WORLD_SIZE;
    live.seed_rng(2024);
    live.next_random();
    live.next_random();
    live.next_random();
    game::Recorder recorder;
    recorder.start(live, {built_in, second});
    Pilot pilot;
    const bool ended_1 = pilot.play_to_end(live, recorder);
    const game::Replay game_1 = recorder.replay();
    const fs::path file_1 = root / "game1.txt";
    const std::string save_error = game::save_replay(file_1, game_1);
    const std::string text_1 = read(file_1);
    const std::vector<std::string> lines_1 = lines_of(text_1);
    const Layout layout_1 = layout_of(lines_1);

    // 1
    {
        const game::LoadedReplay loaded = game::load_replay(file_1);
        const bool header = !lines_1.empty() && lines_1[0] == "breakout replay " + std::to_string(game::REPLAY_VERSION);
        const bool ok = ended_1 && save_error.empty() && loaded.ok && loaded.replay == game_1 && header &&
                        game_1.levels.size() == 2 && game_1.seed == 2024 && game_1.skip == 3;
        report(ok, "1   a recorded game saves, loads back unchanged, with its seed, skip and both levels",
               "ended=" + std::to_string(ended_1) + " save error '" + save_error + "' load ok=" +
                   std::to_string(loaded.ok) + " '" + loaded.error + "' same=" +
                   std::to_string(loaded.replay == game_1) + " first line '" + (lines_1.empty() ? "" : lines_1[0]) +
                   "' levels " + std::to_string(game_1.levels.size()) + " seed " + std::to_string(game_1.seed) +
                   " skip " + std::to_string(game_1.skip));
    }

    // 2
    {
        const game::PlayedReplay played = game::play_replay(game_1);
        report(ended_1 && same_world(live, played.world) && game::replay_matches(game_1, played),
               "2   playing it back gives exactly the live game's world, and it matches",
               first_difference(live, played.world) + ", matches=" +
                   std::to_string(game::replay_matches(game_1, played)));
    }

    // 3
    {
        const std::uint64_t in_file = static_cast<std::uint64_t>(number_after(lines_1[layout_1.end], "steps"));
        const bool ok = game::step_count(game_1) == static_cast<std::uint64_t>(pilot.steps) &&
                        in_file == game::step_count(game_1) && pilot.paused_steps >= 60 &&
                        count_pause_runs(game_1) == 2;
        report(ok, "3   every step that ran is in the file, paused ones included",
               "steps run " + std::to_string(pilot.steps) + ", recorded " + std::to_string(game::step_count(game_1)) +
                   ", end line says " + std::to_string(in_file) + ", paused steps " +
                   std::to_string(pilot.paused_steps) + ", pause presses recorded " +
                   std::to_string(count_pause_runs(game_1)));
    }

    // 4 and 5: steps after the end are not recorded; R starts the next game.
    {
        game::Input idle;
        idle.move_left = true;
        for (int i = 0; i < 50; ++i)
        {
            recorder.step(live, idle);
            live.events.clear();
        }
        const bool untouched = recorder.replay() == game_1;
        const std::uint64_t draws_before_r = live.rng_draws;

        game::Input restart;
        restart.restart = true;
        restart.move_left = true;
        recorder.step(live, restart);
        live.events.clear();
        const game::Replay after_r = recorder.replay();
        const bool first_is_r = !after_r.runs.empty() && after_r.runs[0].input.restart && after_r.runs[0].count == 1;

        Pilot pilot_2;
        pilot_2.steps = 1;
        const bool ended_2 = pilot_2.play_to_end(live, recorder);
        const game::Replay game_2 = recorder.replay();

        report(untouched && first_is_r && after_r.skip == draws_before_r && draws_before_r > game_1.skip &&
                   game_2.levels.size() == 2,
               "4   steps after the end are not recorded, and R starts a new recording from where the generator is",
               "untouched=" + std::to_string(untouched) + " first step is R=" + std::to_string(first_is_r) +
                   " skip " + std::to_string(after_r.skip) + " draws before R " + std::to_string(draws_before_r));

        const game::PlayedReplay played = game::play_replay(game_2);
        report(ended_2 && same_world(live, played.world) && game::replay_matches(game_2, played),
               "5   the game started with R plays back exactly too",
               "ended=" + std::to_string(ended_2) + ", " + first_difference(live, played.world));
    }

    // 6: a test play in the middle of a game.
    {
        game::World world;
        world.size = game::WORLD_SIZE;
        world.seed_rng(77);
        game::Recorder rec;
        rec.start(world, {built_in});
        Pilot early;
        for (int i = 0; i < 300; ++i)
        {
            rec.step(world, early.next(world));
            world.events.clear();
            ++early.steps;
        }
        game::Level canvas = game::editor_canvas(game::Level{});
        canvas.set(2, 1, 1);
        canvas.set(3, 1, 2);
        canvas.set(6, 1, 1);
        const std::uint64_t draws = world.rng_draws;
        rec.start(world, {canvas});
        Pilot pilot_3;
        const bool ended = pilot_3.play_to_end(world, rec);
        const game::Replay test_play = rec.replay();
        const game::PlayedReplay played = game::play_replay(test_play);
        const bool ok = ended && test_play.levels.size() == 1 && test_play.levels[0] == canvas &&
                        test_play.seed == 77 && test_play.skip == draws && same_world(world, played.world) &&
                        game::replay_matches(test_play, played);
        report(ok, "6   a test play is its own recording, with the canvas as its only level, and plays back exactly",
               "ended=" + std::to_string(ended) + " levels " + std::to_string(test_play.levels.size()) +
                   " canvas kept=" + std::to_string(!test_play.levels.empty() && test_play.levels[0] == canvas) +
                   " skip " + std::to_string(test_play.skip) + " vs " + std::to_string(draws) + ", " +
                   first_difference(world, played.world));
    }

    // 7
    {
        const Run run = run_on(file_1);
        const game::ReplayResult r = game_1.result;
        const bool ok = run.exit == 0 && run.err.empty() && has_line(run.out, "score " + std::to_string(r.score)) &&
                        has_line(run.out, "lives " + std::to_string(r.lives)) &&
                        has_line(run.out, "level " + std::to_string(r.level)) && has_line(run.out, "state gameover") &&
                        starts_with(last_line(run.out), "MATCH");
        report(ok, "7   breakout_replay plays the saved game: MATCH and exit 0",
               "exit " + std::to_string(run.exit) + ", stdout '" + one_line(run.out) + "', stderr '" +
                   one_line(run.err) + "'");
    }

    // 8: the end line edited.
    {
        std::vector<std::string> lines = lines_1;
        lines[layout_1.end] = with_number(lines[layout_1.end], "score", game_1.result.score + 10);
        const fs::path file = root / "score.txt";
        write(file, join(lines));
        const Run run = run_on(file);
        report(run.exit == 2 && starts_with(last_line(run.out), "DIFFERENT"),
               "8   a file whose end line expects another score: DIFFERENT and exit 2",
               "exit " + std::to_string(run.exit) + ", last line '" + last_line(run.out) + "', stderr '" +
                   one_line(run.err) + "'");
    }

    // 9: ten idle steps added after the end, with the step total raised.
    {
        std::vector<std::string> lines = lines_1;
        const long long total = number_after(lines[layout_1.end], "steps");
        lines[layout_1.end] = with_number(lines[layout_1.end], "steps", total + 10);
        lines.insert(lines.begin() + static_cast<long>(layout_1.end), "10 -");
        const fs::path file = root / "late.txt";
        write(file, join(lines));
        const Run run = run_on(file);
        report(run.exit == 2 && starts_with(last_line(run.out), "DIFFERENT"),
               "9   a file that goes on after the game ended: DIFFERENT and exit 2",
               "exit " + std::to_string(run.exit) + ", last line '" + last_line(run.out) + "', stderr '" +
                   one_line(run.err) + "'");
    }

    // 10: the last step lines cut off, with the step total lowered.
    {
        // Everything before the end line, then step lines dropped from the
        // back until at least 60 steps are gone (one is always kept).
        std::vector<std::string> lines(lines_1.begin(), lines_1.begin() + static_cast<long>(layout_1.end));
        long long cut = 0;
        while (cut < 60 && lines.size() > layout_1.steps + 2)
        {
            cut += std::atoll(lines.back().c_str());
            lines.pop_back();
        }
        const long long total = number_after(lines_1[layout_1.end], "steps");
        lines.push_back(with_number(lines_1[layout_1.end], "steps", total - cut));
        const fs::path file = root / "short.txt";
        write(file, join(lines));
        const Run run = run_on(file);
        report(run.exit == 2 && starts_with(last_line(run.out), "DIFFERENT"),
               "10  a file cut short before the game ended: DIFFERENT and exit 2",
               "exit " + std::to_string(run.exit) + ", last line '" + last_line(run.out) + "', stderr '" +
                   one_line(run.err) + "'");
    }

    // 11: a recording that missed a test play, like Enter calling start_game
    // without telling the recorder. Same score, lives and state; another game.
    {
        game::World world;
        world.size = game::WORLD_SIZE;
        world.seed_rng(2024);
        game::Recorder rec;
        rec.start(world, {built_in});
        game::Input left;
        left.move_left = true;
        for (int i = 0; i < 60; ++i)
        {
            rec.step(world, left);
            world.events.clear();
        }
        game::Level canvas = game::editor_canvas(built_in);
        for (int column = 1; column <= 7; ++column)
        {
            canvas.set(column, 2, 0);
        }
        game::start_game(world, canvas);

        // The paddle stays where it is and the ball is launched whenever it
        // waits: the recorded game and its playback then score the same.
        game::Input launch;
        launch.launch = true;
        bool ended = false;
        for (int i = 0; i < 30000 && !ended; ++i)
        {
            ended = rec.step(world, world.state == game::GameState::Ready ? launch : game::Input{});
            world.events.clear();
        }
        const game::ReplayResult live_end = game::result_of(world);
        const game::ReplayResult replay_end = game::result_of(game::play_replay(rec.replay()).world);
        const bool same_numbers = live_end.score == replay_end.score && live_end.lives == replay_end.lives &&
                                  live_end.level == replay_end.level && live_end.state == replay_end.state;

        const fs::path file = root / "missed.txt";
        game::save_replay(file, rec.replay());
        const Run run = run_on(file);
        report(ended && same_numbers && run.exit == 2 && starts_with(last_line(run.out), "DIFFERENT"),
               "11  a recording that missed a test play: DIFFERENT and exit 2, though score, lives and state are the same",
               "ended=" + std::to_string(ended) + " live score " + std::to_string(live_end.score) + " lives " +
                   std::to_string(live_end.lives) + ", played back score " + std::to_string(replay_end.score) +
                   " lives " + std::to_string(replay_end.lives) + ", exit " + std::to_string(run.exit) +
                   ", last line '" + last_line(run.out) + "'");
    }

    // 12: a bad key on the first step line.
    {
        std::vector<std::string> lines = lines_1;
        const std::size_t line = layout_1.steps + 1;
        lines[line] = "5 lft";
        const fs::path file = root / "key.txt";
        write(file, join(lines));
        const Run run = run_on(file);
        const std::string where = "key.txt:" + std::to_string(line + 1) + ":";
        report(run.exit == 1 && run.out.empty() && starts_with(run.err, where) &&
                   run.err.find("lft") != std::string::npos,
               "12  a bad key is refused: exit 1, file and line on stderr, nothing on stdout",
               "exit " + std::to_string(run.exit) + ", expected stderr to start with '" + where + "', got '" +
                   one_line(run.err) + "', stdout '" + one_line(run.out) + "'");
    }

    // 13: a bad character in the first row of the first level.
    {
        std::vector<std::string> lines = lines_1;
        std::size_t row = 0;
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            if (starts_with(lines[i], "level "))
            {
                row = i + 1;
                break;
            }
        }
        lines[row][2] = 'x';
        const fs::path file = root / "row.txt";
        write(file, join(lines));
        const Run run = run_on(file);
        const std::string where = "row.txt:" + std::to_string(row + 1) + ":3:";
        report(run.exit == 1 && run.out.empty() && starts_with(run.err, where),
               "13  a bad character in a level row: exit 1, with the file's line and column",
               "exit " + std::to_string(run.exit) + ", expected stderr to start with '" + where + "', got '" +
                   one_line(run.err) + "'");
    }

    // 14
    {
        const Run missing = run_on(root / "no-such-file.txt");
        const Run none = run_program("");
        report(missing.exit == 1 && !missing.err.empty() && missing.out.empty() && none.exit == 1 &&
                   !none.err.empty(),
               "14  a missing file or no file at all: exit 1 with a message on stderr",
               "missing: exit " + std::to_string(missing.exit) + " '" + one_line(missing.err) + "', no file: exit " +
                   std::to_string(none.exit) + " '" + one_line(none.err) + "'");
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

echo "building the game sources, breakout_replay and the check program..."
if [ ! -f src/breakout_replay.cpp ] ||
    ! g++ -std=c++20 -Iinclude src/breakout_replay.cpp src/game/*.cpp -o "$work/breakout_replay" 2> "$work/build.log" ||
    ! g++ -std=c++20 -Iinclude "$work/check.cpp" src/game/*.cpp -o "$work/check" 2>> "$work/build.log"; then
    echo "the replay code is missing or does not build. First errors:"
    grep -m 3 "error" "$work/build.log" 2> /dev/null
    [ -f src/breakout_replay.cpp ] || echo "src/breakout_replay.cpp: no such file"
    echo "RESULT: 0 OF 14 CHECKS PASSED (nothing could run)"
    exit 1
fi

"$work/check" "$work" "$work/breakout_replay"
status=$?
if [ $status -ge 128 ]; then
    echo "RESULT: THE CHECK PROGRAM CRASHED (signal $((status - 128)))"
    exit 1
fi
exit $status
