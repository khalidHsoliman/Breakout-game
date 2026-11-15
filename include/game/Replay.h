#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "game/Input.h"
#include "game/Level.h"
#include "game/World.h"

namespace game
{
    // The first line of every replay file. A format change gets a new number.
    inline constexpr int REPLAY_VERSION = 1;

    // count steps in a row that all got the same input.
    struct ReplayRun
    {
        std::uint32_t count = 1;
        Input input;

        bool operator==(const ReplayRun&) const = default;
    };

    // How a game stood at the end. level counts from 1, as on screen, and
    // bricks is how many that can be broken are left on the field.
    struct ReplayResult
    {
        int score = 0;
        int lives = STARTING_LIVES;
        int level = 1;
        int bricks = 0;
        GameState state = GameState::GameOver;

        bool operator==(const ReplayResult&) const = default;
    };

    // Everything needed to play one game again. The game is played in a
    // world of WORLD_SIZE, one FIXED_DT step per input.
    struct Replay
    {
        // The generator starts from seed with skip numbers drawn, which is
        // where the game found it: a restart carries on with it.
        std::uint32_t seed = DEFAULT_SEED;
        std::uint64_t skip = 0;

        std::vector<Level> levels;
        std::vector<ReplayRun> runs;

        // What the game ended with, as recorded, on its last step. Checked
        // against a playback, not used by one.
        ReplayResult result;

        bool operator==(const Replay&) const = default;
    };

    // As replay files write it: "ready", "playing", "paused", "gameover" or
    // "won".
    const char* state_name(GameState state);

    // The sum of the counts of the runs.
    std::uint64_t step_count(const Replay& replay);

    // score, lives, level (from 1), bricks left and state.
    ReplayResult result_of(const World& world);

    // Records one game at a time, from its start until the step that ends it
    // in a game over or a win. Steps after that are not recorded; R after it
    // starts the next recording. The game is only ever stepped by FIXED_DT.
    class Recorder
    {
    public:
        // start_game with these levels, and a new recording of it from the
        // generator where it is now. Used at startup and for a test play.
        void start(World& world, std::vector<Level> levels);

        // One game::step, recorded. True for the step that ends the game,
        // once the result is filled in: that is when to save. R after an
        // end begins a new recording with this step as its first.
        bool step(World& world, const Input& input);

        // False before start and after the game ended, until R.
        bool recording() const { return m_recording; }

        // The game being recorded, or the last one when not recording.
        const Replay& replay() const { return m_replay; }

    private:
        void begin(const World& world);

        Replay m_replay;
        bool m_recording = false;
    };

    struct PlayedReplay
    {
        World world;

        // The step, counted from 1, on which the game first went to a game
        // over or a win; 0 if it never did.
        std::uint64_t ended_on_step = 0;
    };

    // A fresh world of WORLD_SIZE, with the generator seeded and skipped,
    // started on the replay's levels and stepped through every run. Events
    // are cleared after every step, so they do not pile up.
    PlayedReplay play_replay(const Replay& replay);

    // The same game: it ended on the replay's last step, not before and not
    // after, with the result the replay recorded.
    bool replay_matches(const Replay& replay, const PlayedReplay& played);

    // The file format, in order. Every line ends with LF:
    //   breakout replay 1
    //   seed <0 to 4294967295>
    //   skip <numbers already drawn, 0 to 4294967295>
    //   level <rows>        then that many rows, as to_text(Level) writes them
    //   ... one level block per level, at least one
    //   steps
    //   <count> <keys>      '-' for none, else some of: left right launch pause restart
    //   ...
    //   end steps <n> score <n> lives <n> level <n> bricks <n> state <gameover|won>
    std::string to_text(const Replay& replay);

    // What is wrong with a replay file, and where. line and column count from
    // 1 and a leading byte order mark is not counted. column is 0 unless the
    // problem is in a level row; line is 0 for the file as a whole.
    struct ReplayProblem
    {
        int line = 0;
        int column = 0;
        std::string message;

        bool operator==(const ReplayProblem&) const = default;
    };

    struct ParsedReplay
    {
        Replay replay;

        // The first problem in reading order; when set, replay is not usable.
        std::optional<ReplayProblem> problem;
    };

    // Reads what to_text writes. Also takes CRLF, a byte order mark and
    // blank lines after the end line, for a file saved by Notepad. Every
    // level must pass check_level_text against WORLD_SIZE.
    ParsedReplay parse_replay(std::string_view text);

    struct LoadedReplay
    {
        bool ok = false;
        Replay replay;

        // With the file name in front, as for level files:
        // "last.txt:12: ..." or "last.txt:7:3: ...".
        std::string error;
    };

    LoadedReplay load_replay(const std::filesystem::path& path);

    // Writes to_text(replay) to path, creating its folder if needed. Returns
    // why it could not, with the file name in front, or an empty string when
    // it was saved.
    std::string save_replay(const std::filesystem::path& path, const Replay& replay);
}
