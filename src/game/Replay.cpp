#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "game/LevelCheck.h"
#include "game/LevelFiles.h"
#include "game/Replay.h"
#include "game/Spawn.h"
#include "game/Systems.h"

namespace game
{
    namespace
    {
        constexpr std::uint64_t MAX_COUNT = std::numeric_limits<std::uint32_t>::max();
        constexpr std::uint64_t MAX_INT = static_cast<std::uint64_t>(std::numeric_limits<int>::max());

        // No game draws anywhere near this many: two numbers at most per
        // brick broken. Skipping them all takes seconds in a Release build,
        // and up to a minute in Debug.
        constexpr std::uint64_t MAX_SKIP = std::numeric_limits<std::uint32_t>::max();

        constexpr const char* KEYS = "the keys are left, right, launch, pause and restart";
        constexpr const char* END_STATES = "a replay ends in gameover or won";

        constexpr const char* SEED_LINE = "'seed' and a number from 0 to 4294967295";
        constexpr const char* SKIP_LINE = "'skip' and a number from 0 to 4294967295";
        constexpr const char* LEVEL_OR_STEPS = "'level' and its number of rows, or 'steps'";
        constexpr const char* STEP_OR_END = "a step count and its keys, or the end line";
        constexpr const char* END_LINE = "'end steps <n> score <n> lives <n> level <n> bricks <n> state <state>'";

        bool is_over(GameState state)
        {
            return state == GameState::GameOver || state == GameState::Won;
        }

        std::optional<GameState> state_named(std::string_view name)
        {
            for (const GameState state : {GameState::Ready, GameState::Playing, GameState::Paused,
                                          GameState::GameOver, GameState::Won})
            {
                if (name == state_name(state))
                {
                    return state;
                }
            }
            return std::nullopt;
        }

        // The keys in the order they are written.
        struct Key
        {
            const char* name;
            bool Input::* flag;
        };

        constexpr Key ALL_KEYS[] = {
            {"left", &Input::move_left},
            {"right", &Input::move_right},
            {"launch", &Input::launch},
            {"pause", &Input::toggle_pause},
            {"restart", &Input::restart},
        };

        std::string keys_text(const Input& input)
        {
            std::string text;
            for (const Key& key : ALL_KEYS)
            {
                if (input.*key.flag)
                {
                    text += text.empty() ? "" : " ";
                    text += key.name;
                }
            }
            return text.empty() ? "-" : text;
        }

        // Split on LF with one trailing CR removed, as for level files.
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

        bool is_blank(std::string_view line)
        {
            return line.find_first_not_of(" \t") == std::string_view::npos;
        }

        // Split on spaces; several in a row count as one.
        std::vector<std::string_view> words(std::string_view line)
        {
            std::vector<std::string_view> result;
            std::size_t start = 0;

            while (start < line.size())
            {
                if (line[start] == ' ')
                {
                    ++start;
                    continue;
                }

                std::size_t end = line.find(' ', start);
                if (end == std::string_view::npos)
                {
                    end = line.size();
                }

                result.push_back(line.substr(start, end - start));
                start = end;
            }

            return result;
        }

        // Digits only: no sign, no spaces, nothing after.
        std::optional<std::uint64_t> number(std::string_view word, std::uint64_t max)
        {
            if (word.empty() || word.find_first_not_of("0123456789") != std::string_view::npos)
            {
                return std::nullopt;
            }

            std::uint64_t value = 0;
            const std::from_chars_result result = std::from_chars(word.data(), word.data() + word.size(), value);
            if (result.ec != std::errc{} || value > max)
            {
                return std::nullopt;
            }

            return value;
        }

        // A line as it is quoted in a message, cut short if it is long.
        std::string quoted(std::string_view line)
        {
            constexpr std::size_t LONGEST = 40;
            if (line.size() > LONGEST)
            {
                return "'" + std::string(line.substr(0, LONGEST)) + "...'";
            }
            return "'" + std::string(line) + "'";
        }

        // Thrown on the first problem and caught in parse_replay, so none
        // escapes it.
        struct BadReplay
        {
            ReplayProblem problem;
        };

        [[noreturn]] void fail(int line, std::string message)
        {
            throw BadReplay{ReplayProblem{line, 0, std::move(message)}};
        }

        class Reader
        {
        public:
            explicit Reader(std::string_view text)
                : m_lines(split_lines(skip_bom(text)))
            {
                // Blank lines at the end, the last newline's included, are
                // not lines of the replay.
                while (!m_lines.empty() && is_blank(m_lines.back()))
                {
                    m_lines.pop_back();
                }
            }

            bool at_end() const
            {
                return m_next >= m_lines.size();
            }

            // Counted from 1: the line take() returns next.
            int line_number() const
            {
                return static_cast<int>(m_next) + 1;
            }

            std::string_view peek() const
            {
                return m_lines[m_next];
            }

            // The next line, or a problem saying what should have been there.
            std::string_view take(const char* expected)
            {
                if (at_end())
                {
                    fail(0, std::string("the file ends where ") + expected +
                                " should be: it may have been cut short");
                }
                return m_lines[m_next++];
            }

        private:
            std::vector<std::string_view> m_lines;
            std::size_t m_next = 0;
        };

        void expect_header(Reader& reader)
        {
            const std::string version = std::to_string(REPLAY_VERSION);
            const std::string header = "'breakout replay " + version + "'";

            const int line_number = reader.line_number();
            const std::string_view line = reader.take(header.c_str());
            const std::vector<std::string_view> parts = words(line);

            if (parts.size() == 3 && parts[0] == "breakout" && parts[1] == "replay")
            {
                if (parts[2] == version)
                {
                    return;
                }
                if (const std::optional<std::uint64_t> other = number(parts[2], MAX_INT))
                {
                    // Recorded by an older game, under rules this one no
                    // longer plays by, so it cannot be played back here.
                    if (*other >= 1 && *other < static_cast<std::uint64_t>(REPLAY_VERSION))
                    {
                        fail(line_number, "this is a version " + std::string(parts[2]) +
                                              " replay, from an older version of the game: only version " + version +
                                              " can be read");
                    }
                    fail(line_number, "this is a version " + std::string(parts[2]) + " replay, but only version " +
                                          version + " can be read");
                }
            }

            fail(line_number, "this is not a breakout replay: the first line should be 'breakout replay " +
                                  std::to_string(REPLAY_VERSION) + "'");
        }

        // "<keyword> <number>", the number at most max.
        std::uint64_t expect_number_line(Reader& reader, std::string_view keyword, std::uint64_t max,
                                         const char* expected)
        {
            const int line_number = reader.line_number();
            const std::string_view line = reader.take(expected);
            const std::vector<std::string_view> parts = words(line);

            if (parts.size() == 2 && parts[0] == keyword)
            {
                if (const std::optional<std::uint64_t> value = number(parts[1], max))
                {
                    return *value;
                }
            }

            fail(line_number, std::string("expected ") + expected + ", not " + quoted(line));
        }

        // The rows after "level <rows>", checked like a level file. header is
        // the line number of "level <rows>".
        Level read_level(Reader& reader, int header, std::uint64_t rows)
        {
            const std::string where = "the level that starts on line " + std::to_string(header);
            const int first_row = reader.line_number();

            std::string text;
            for (std::uint64_t row = 0; row < rows; ++row)
            {
                const int line_number = reader.line_number();
                if (reader.at_end())
                {
                    fail(0, "the file ends inside " + where + ": it may have been cut short");
                }

                const std::string_view line = reader.peek();
                const std::vector<std::string_view> parts = words(line);
                if (!parts.empty() && (parts[0] == "level" || parts[0] == "steps"))
                {
                    fail(line_number, where + " has " + std::to_string(row) + " rows, not the " +
                                          std::to_string(rows) + " its 'level' line says");
                }
                if (is_blank(line))
                {
                    fail(line_number, "a level row cannot be blank: an empty cell is '.'");
                }

                text += reader.take("a level row");
                text += '\n';
            }

            // Positions in the rows become positions in the file; a problem
            // with the level as a whole points at its 'level' line.
            if (const std::optional<LevelProblem> problem = check_level_text(text, WORLD_SIZE))
            {
                if (problem->line == 0)
                {
                    fail(header, where + ": " + problem->message);
                }
                throw BadReplay{ReplayProblem{first_row + problem->line - 1, problem->column, problem->message}};
            }

            return parse_level(text);
        }

        // "level <rows>" blocks until "steps".
        std::vector<Level> read_levels(Reader& reader)
        {
            std::vector<Level> levels;

            while (true)
            {
                const int line_number = reader.line_number();
                const std::string_view line = reader.take(LEVEL_OR_STEPS);
                const std::vector<std::string_view> parts = words(line);

                if (parts.size() == 1 && parts[0] == "steps")
                {
                    if (levels.empty())
                    {
                        fail(line_number, "there is no level before 'steps': a replay needs at least one");
                    }
                    return levels;
                }

                if (parts.size() == 2 && parts[0] == "level")
                {
                    const std::optional<std::uint64_t> rows = number(parts[1], MAX_INT);
                    if (!rows || *rows == 0)
                    {
                        fail(line_number, "'" + std::string(parts[1]) +
                                              "' is not a number of rows: a level has at least one row");
                    }
                    levels.push_back(read_level(reader, line_number, *rows));
                    continue;
                }

                fail(line_number, std::string("expected ") + LEVEL_OR_STEPS + ", not " + quoted(line));
            }
        }

        ReplayRun read_run(int line_number, const std::vector<std::string_view>& parts)
        {
            const std::optional<std::uint64_t> count = number(parts[0], MAX_COUNT);
            if (!count || *count == 0)
            {
                fail(line_number, "'" + std::string(parts[0]) +
                                      "' is not a step count: a count is a whole number from 1 to 4294967295");
            }

            if (parts.size() == 1)
            {
                fail(line_number, "the step count has no keys after it: '-' is for none");
            }

            ReplayRun run;
            run.count = static_cast<std::uint32_t>(*count);

            if (parts[1] == "-")
            {
                if (parts.size() > 2)
                {
                    fail(line_number, "'-' means no keys, so nothing can follow it");
                }
                return run;
            }

            for (std::size_t i = 1; i < parts.size(); ++i)
            {
                const Key* found = nullptr;
                for (const Key& key : ALL_KEYS)
                {
                    if (parts[i] == key.name)
                    {
                        found = &key;
                    }
                }

                if (found == nullptr)
                {
                    fail(line_number, quoted(parts[i]) + " is not a key: " + KEYS);
                }
                if (run.input.*found->flag)
                {
                    fail(line_number, quoted(parts[i]) + " is given twice");
                }
                run.input.*found->flag = true;
            }

            return run;
        }

        ReplayResult read_end(int line_number, std::string_view line, const std::vector<std::string_view>& parts,
                              std::uint64_t steps, std::size_t levels)
        {
            const bool shaped = parts.size() == 13 && parts[1] == "steps" && parts[3] == "score" &&
                                parts[5] == "lives" && parts[7] == "level" && parts[9] == "bricks" &&
                                parts[11] == "state";
            const std::optional<std::uint64_t> end_steps = shaped ? number(parts[2], std::numeric_limits<std::uint64_t>::max()) : std::nullopt;
            const std::optional<std::uint64_t> score = shaped ? number(parts[4], MAX_INT) : std::nullopt;
            const std::optional<std::uint64_t> lives = shaped ? number(parts[6], MAX_INT) : std::nullopt;
            const std::optional<std::uint64_t> level = shaped ? number(parts[8], MAX_INT) : std::nullopt;
            const std::optional<std::uint64_t> bricks = shaped ? number(parts[10], MAX_INT) : std::nullopt;

            if (!end_steps || !score || !lives || !level || !bricks)
            {
                fail(line_number, std::string("expected ") + END_LINE + ", not " + quoted(line));
            }

            // Recordings end on the step that loses or wins the game, and a
            // playback has to end there too.
            const std::optional<GameState> state = state_named(parts[12]);
            if (state != GameState::GameOver && state != GameState::Won)
            {
                fail(line_number, quoted(parts[12]) + " cannot end a replay: " + END_STATES);
            }

            if (*end_steps != steps)
            {
                fail(line_number, "the end line says " + std::to_string(*end_steps) +
                                      " steps, but the step lines add up to " + std::to_string(steps));
            }

            if (*level == 0 || *level > levels)
            {
                fail(line_number, "the end line says level " + std::to_string(*level) + ", but the replay has " +
                                      std::to_string(levels) + (levels == 1 ? " level" : " levels"));
            }

            return ReplayResult{static_cast<int>(*score), static_cast<int>(*lives), static_cast<int>(*level),
                                static_cast<int>(*bricks), *state};
        }

        Replay read_replay(std::string_view text)
        {
            Reader reader(text);
            if (reader.at_end())
            {
                fail(0, "the file is empty");
            }

            Replay replay;
            expect_header(reader);
            replay.seed = static_cast<std::uint32_t>(expect_number_line(reader, "seed", MAX_COUNT, SEED_LINE));
            replay.skip = expect_number_line(reader, "skip", MAX_SKIP, SKIP_LINE);
            replay.levels = read_levels(reader);

            std::uint64_t steps = 0;
            while (true)
            {
                const int line_number = reader.line_number();
                const std::string_view line = reader.take(STEP_OR_END);
                const std::vector<std::string_view> parts = words(line);

                if (parts.empty())
                {
                    fail(line_number, std::string("a blank line: expected ") + STEP_OR_END);
                }

                if (parts[0] == "end")
                {
                    replay.result = read_end(line_number, line, parts, steps, replay.levels.size());
                    break;
                }

                const ReplayRun run = read_run(line_number, parts);
                steps += run.count;
                replay.runs.push_back(run);
            }

            // Blank lines at the very end are gone already; one with
            // something after it is not.
            while (!reader.at_end() && is_blank(reader.peek()))
            {
                reader.take("");
            }
            if (!reader.at_end())
            {
                fail(reader.line_number(), "nothing can follow the end line");
            }

            return replay;
        }

        std::string describe(const std::string& file_name, const ReplayProblem& problem)
        {
            if (problem.line == 0)
            {
                return file_name + ": " + problem.message;
            }
            if (problem.column == 0)
            {
                return file_name + ":" + std::to_string(problem.line) + ": " + problem.message;
            }
            return file_name + ":" + std::to_string(problem.line) + ":" + std::to_string(problem.column) + ": " +
                   problem.message;
        }
    }

    const char* state_name(GameState state)
    {
        switch (state)
        {
        case GameState::Ready:
            return "ready";
        case GameState::Playing:
            return "playing";
        case GameState::Paused:
            return "paused";
        case GameState::GameOver:
            return "gameover";
        case GameState::Won:
            return "won";
        }
        return "";
    }

    std::uint64_t step_count(const Replay& replay)
    {
        std::uint64_t steps = 0;
        for (const ReplayRun& run : replay.runs)
        {
            steps += run.count;
        }
        return steps;
    }

    ReplayResult result_of(const World& world)
    {
        return ReplayResult{world.score, world.lives, world.level_index + 1, static_cast<int>(world.bricks.size()),
                            world.state};
    }

    void Recorder::start(World& world, std::vector<Level> levels)
    {
        start_game(world, std::move(levels));
        begin(world);
    }

    void Recorder::begin(const World& world)
    {
        m_replay = Replay{};
        m_replay.seed = world.rng_seed;
        m_replay.skip = world.rng_draws;
        m_replay.levels = world.levels;
        m_recording = true;
    }

    bool Recorder::step(World& world, const Input& input)
    {
        const bool was_over = is_over(world.state);

        // transition_system restarts from world.levels at the start of this
        // step and draws nothing doing it, so the new game starts from here.
        if (was_over && input.restart)
        {
            begin(world);
        }

        if (m_recording)
        {
            std::vector<ReplayRun>& runs = m_replay.runs;
            if (!runs.empty() && runs.back().input == input && runs.back().count < MAX_COUNT)
            {
                runs.back().count += 1;
            }
            else
            {
                runs.push_back(ReplayRun{1, input});
            }
        }

        game::step(world, input, FIXED_DT);

        if (!m_recording || was_over || !is_over(world.state))
        {
            return false;
        }

        m_replay.result = result_of(world);
        m_recording = false;
        return true;
    }

    PlayedReplay play_replay(const Replay& replay)
    {
        PlayedReplay played;
        World& world = played.world;
        world.size = WORLD_SIZE;

        world.seed_rng(replay.seed);
        world.rng.discard(replay.skip);
        world.rng_draws = replay.skip;

        start_game(world, replay.levels);

        std::uint64_t steps = 0;
        for (const ReplayRun& run : replay.runs)
        {
            for (std::uint32_t i = 0; i < run.count; ++i)
            {
                const bool was_over = is_over(world.state);
                step(world, run.input, FIXED_DT);
                world.events.clear();

                steps += 1;
                if (played.ended_on_step == 0 && !was_over && is_over(world.state))
                {
                    played.ended_on_step = steps;
                }
            }
        }

        return played;
    }

    bool replay_matches(const Replay& replay, const PlayedReplay& played)
    {
        return played.ended_on_step != 0 && played.ended_on_step == step_count(replay) &&
               result_of(played.world) == replay.result;
    }

    std::string to_text(const Replay& replay)
    {
        std::string text = "breakout replay " + std::to_string(REPLAY_VERSION) + "\n";
        text += "seed " + std::to_string(replay.seed) + "\n";
        text += "skip " + std::to_string(replay.skip) + "\n";

        for (const Level& level : replay.levels)
        {
            text += "level " + std::to_string(level.rows) + "\n";
            text += to_text(level);
        }

        text += "steps\n";
        for (const ReplayRun& run : replay.runs)
        {
            text += std::to_string(run.count) + " " + keys_text(run.input) + "\n";
        }

        const ReplayResult& result = replay.result;
        text += "end steps " + std::to_string(step_count(replay)) + " score " + std::to_string(result.score) +
                " lives " + std::to_string(result.lives) + " level " + std::to_string(result.level) + " bricks " +
                std::to_string(result.bricks) + " state " + state_name(result.state) + "\n";

        return text;
    }

    ParsedReplay parse_replay(std::string_view text)
    {
        try
        {
            return ParsedReplay{read_replay(text), std::nullopt};
        }
        catch (const BadReplay& bad)
        {
            return ParsedReplay{Replay{}, bad.problem};
        }
    }

    LoadedReplay load_replay(const std::filesystem::path& path)
    {
        const std::string file_name = path.filename().string();

        std::error_code code;
        const bool exists = std::filesystem::exists(path, code);
        if (code)
        {
            return LoadedReplay{false, {}, file_name + ": could not look for the file: " + code.message()};
        }
        if (!exists)
        {
            return LoadedReplay{false, {}, file_name + ": there is no such file"};
        }

        const ReadResult read = read_file(path);
        if (!read.error.empty())
        {
            return LoadedReplay{false, {}, file_name + ": " + read.error};
        }

        ParsedReplay parsed = parse_replay(read.contents);
        if (parsed.problem)
        {
            return LoadedReplay{false, {}, describe(file_name, *parsed.problem)};
        }

        return LoadedReplay{true, std::move(parsed.replay), {}};
    }

    std::string save_replay(const std::filesystem::path& path, const Replay& replay)
    {
        const std::string file_name = path.filename().string();

        if (path.has_parent_path())
        {
            std::error_code code;
            std::filesystem::create_directories(path.parent_path(), code);
            if (code)
            {
                return file_name + ": could not create the folder: " + code.message();
            }
        }

        // Binary, so the file has LF line ends on Windows too.
        const std::string text = to_text(replay);
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file)
        {
            return file_name + ": the file could not be opened for writing";
        }

        file.write(text.data(), static_cast<std::streamsize>(text.size()));
        file.close();
        if (!file)
        {
            return file_name + ": the file could not be written";
        }

        return {};
    }
}
