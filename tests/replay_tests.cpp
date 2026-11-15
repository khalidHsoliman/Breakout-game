#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <ostream>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "game/Editor.h"
#include "game/Replay.h"
#include "game/Spawn.h"
#include "game/Systems.h"

namespace
{
    namespace fs = std::filesystem;

    game::Input keys(bool left, bool right, bool launch, bool pause, bool restart)
    {
        game::Input input;
        input.move_left = left;
        input.move_right = right;
        input.launch = launch;
        input.toggle_pause = pause;
        input.restart = restart;
        return input;
    }

    const game::Input NOTHING{};
    const game::Input LAUNCH = keys(false, false, true, false, false);
    const game::Input LEFT = keys(true, false, false, false, false);
    const game::Input RIGHT = keys(false, true, false, false, false);
    const game::Input PAUSE = keys(false, false, false, true, false);
    const game::Input RESTART = keys(false, false, false, false, true);

    game::World new_world()
    {
        game::World world;
        world.size = game::WORLD_SIZE;
        return world;
    }

    // Exact, bit for bit, so two worlds compare equal only if they are.
    std::string hex(float value)
    {
        char text[32];
        std::snprintf(text, sizeof(text), "%a", static_cast<double>(value));
        return text;
    }

    std::string vec(math::Vec2 value)
    {
        return "(" + hex(value.x) + " " + hex(value.y) + ")";
    }

    std::string transform(const game::World& world, core::Entity entity)
    {
        const game::Transform* t = world.transforms.find(entity);
        if (t == nullptr)
        {
            return " no-transform";
        }
        return " at " + vec(t->position) + " from " + vec(t->previous_position) + " size " + vec(t->size);
    }

    std::string velocity(const game::World& world, core::Entity entity)
    {
        const game::Velocity* v = world.velocities.find(entity);
        return v == nullptr ? " no-velocity" : " moving " + vec(v->value);
    }

    // Everything that decides how a game goes on, apart from the particles,
    // which use std::rand and never change it. Each store in its own order,
    // so entity ids, which a restarted game does not start over, do not count.
    std::string describe(const game::World& world)
    {
        std::ostringstream generator;
        generator << world.rng;

        std::string text = "score " + std::to_string(world.score) + " lives " + std::to_string(world.lives) +
                           " level " + std::to_string(world.level_index) + " state " +
                           std::to_string(static_cast<int>(world.state)) + " seed " + std::to_string(world.rng_seed) +
                           " draws " + std::to_string(world.rng_draws) + " levels " +
                           std::to_string(world.levels.size()) + "\n";

        for (const core::Entity entity : world.balls.entities())
        {
            const game::Ball& ball = *world.balls.find(entity);
            text += "ball" + transform(world, entity) + velocity(world, entity) + " radius " + hex(ball.radius) +
                    " untouched " + hex(ball.time_since_touch) + "\n";
        }
        for (const core::Entity entity : world.paddles.entities())
        {
            const game::Paddle& paddle = *world.paddles.find(entity);
            text += "paddle" + transform(world, entity) + " speed " + hex(paddle.speed) + " wide " +
                    hex(paddle.wide_time_left) + "\n";
        }
        for (const core::Entity entity : world.bricks.entities())
        {
            const core::Color& color = *world.colors.find(entity);
            text += "brick" + transform(world, entity) + " hp " + std::to_string(world.bricks.find(entity)->hit_points) +
                    " colour " + hex(color.r) + " " + hex(color.g) + " " + hex(color.b) + "\n";
        }
        for (const core::Entity entity : world.steel.entities())
        {
            text += "steel" + transform(world, entity) + "\n";
        }
        for (const core::Entity entity : world.capsules.entities())
        {
            text += "capsule" + transform(world, entity) + velocity(world, entity) + " kind " +
                    std::to_string(static_cast<int>(world.capsules.find(entity)->kind)) + "\n";
        }

        return text + "generator " + generator.str() + "\n";
    }

    // Puts the only ball below the floor, so the next step loses a life.
    // Changes the world behind the recorder's back: for recorder tests only,
    // never for one that plays the recording back.
    void drop_the_ball(game::World& world)
    {
        const core::Entity ball = world.balls.entities()[0];
        world.transforms.find(ball)->position = math::Vec2{400.0f, -100.0f};
    }

    void lose_the_game(game::World& world, game::Recorder& recorder, std::vector<bool>& ended)
    {
        while (world.state != game::GameState::GameOver)
        {
            if (world.state == game::GameState::Ready)
            {
                ended.push_back(recorder.step(world, LAUNCH));
            }
            drop_the_ball(world);
            ended.push_back(recorder.step(world, NOTHING));
        }
    }

    // ------------------------------------------------------------ the autopilot

    // Follows whatever is coming down lowest, a ball or a capsule, and hits the
    // ball a little off centre by an amount that changes with the score and
    // every ten seconds. Every other ten seconds it ignores capsules, for four
    // seconds of every three minutes it does not move, and it pauses for a
    // second every minute, so games are won and lost, capsules caught and
    // missed, and paused steps recorded. R after a win or a loss.
    game::Input autopilot(const game::World& world, int step)
    {
        game::Input input;
        input.launch = world.state == game::GameState::Ready;
        input.restart = world.state == game::GameState::GameOver || world.state == game::GameState::Won;

        // On, and off again a second later.
        const int in_minute = step % 3600;
        input.toggle_pause = in_minute == 1800 || in_minute == 1860;

        if (step % 10800 >= 10560)
        {
            return input;
        }
        const bool ignore_capsules = (step / 600) % 2 == 1;

        const game::Transform& paddle = *world.transforms.find(world.paddles.entities()[0]);
        float lowest = world.size.y;
        float target = paddle.position.x;

        for (const core::Entity ball : world.balls.entities())
        {
            const math::Vec2 position = world.transforms.find(ball)->position;
            if (world.velocities.find(ball)->value.y < 0.0f && position.y > paddle.position.y && position.y < lowest)
            {
                lowest = position.y;
                const int aim = (world.score / 10 + step / 600) % 5 - 2;
                target = position.x - static_cast<float>(aim) * 15.0f;
            }
        }

        for (const core::Entity capsule : world.capsules.entities())
        {
            const math::Vec2 position = world.transforms.find(capsule)->position;
            if (!ignore_capsules && position.y > paddle.position.y && position.y < lowest)
            {
                lowest = position.y;
                target = position.x;
            }
        }

        input.move_left = target < paddle.position.x - 4.0f;
        input.move_right = target > paddle.position.x + 4.0f;
        return input;
    }

    // A game the recorder finished, and the world as it was at that moment.
    struct Finished
    {
        game::Replay replay;
        std::string world;
    };

    struct LongGame
    {
        std::vector<Finished> finished;
        game::World world;

        int wins = 0;
        int losses = 0;
        int most_balls = 0;
        int paused_steps = 0;
        bool played_steel = false;
    };

    std::vector<game::Level> two_levels()
    {
        return {game::parse_level(game::BUILT_IN_LEVEL), game::parse_level("1#1#11#1#1\n1111111111\n")};
    }

    // Played once and kept, since several tests look at it. Twenty minutes of
    // game time.
    const LongGame& long_game()
    {
        static const LongGame game = []
        {
            LongGame run{{}, new_world()};
            game::World& world = run.world;

            world.seed_rng(20240611u);
            game::Recorder recorder;
            recorder.start(world, two_levels());

            for (int step = 0; step < 72000; ++step)
            {
                const game::Input input = autopilot(world, step);
                const bool ended = recorder.step(world, input);
                world.events.clear();

                run.paused_steps += world.state == game::GameState::Paused ? 1 : 0;
                run.most_balls = std::max(run.most_balls, static_cast<int>(world.balls.size()));
                run.played_steel = run.played_steel || world.steel.size() > 0;

                if (ended)
                {
                    run.wins += world.state == game::GameState::Won ? 1 : 0;
                    run.losses += world.state == game::GameState::GameOver ? 1 : 0;
                    run.finished.push_back(Finished{recorder.replay(), describe(world)});
                }
            }

            return run;
        }();

        return game;
    }

    // A small replay with a bit of everything, and its text.
    game::Replay small_replay()
    {
        game::Replay replay;
        replay.seed = 42;
        replay.skip = 7;
        replay.levels = {game::parse_level("1.1\n.#.\n"), game::parse_level("#2\n")};
        replay.runs = {
            game::ReplayRun{3, NOTHING},
            game::ReplayRun{1, keys(true, false, true, false, false)},
            game::ReplayRun{2, keys(false, true, false, true, true)},
        };
        replay.result = game::ReplayResult{10, 2, 2, 1, game::GameState::GameOver};
        return replay;
    }

    constexpr std::string_view SMALL_TEXT =
        "breakout replay 1\n"   // 1
        "seed 42\n"             // 2
        "skip 7\n"              // 3
        "level 2\n"             // 4
        "1.1\n"                 // 5
        ".#.\n"                 // 6
        "level 1\n"             // 7
        "#2\n"                  // 8
        "steps\n"               // 9
        "3 -\n"                 // 10
        "1 left launch\n"       // 11
        "2 right pause restart\n"   // 12
        "end steps 6 score 10 lives 2 level 2 bricks 1 state gameover\n";   // 13

    // SMALL_TEXT with line number (from 1) replaced by replacement, which may
    // be several lines or none.
    std::string replace_line(int number, std::string_view replacement)
    {
        std::string text;
        int line = 1;
        std::size_t start = 0;
        while (start < SMALL_TEXT.size())
        {
            const std::size_t end = SMALL_TEXT.find('\n', start);
            if (line == number)
            {
                text += replacement;
                if (!replacement.empty())
                {
                    text += '\n';
                }
            }
            else
            {
                text += SMALL_TEXT.substr(start, end - start + 1);
            }
            start = end + 1;
            ++line;
        }
        return text;
    }

    std::string problem_text(const std::optional<game::ReplayProblem>& problem)
    {
        if (!problem)
        {
            return "no problem";
        }
        return std::to_string(problem->line) + ":" + std::to_string(problem->column) + ": " + problem->message;
    }
}

// ---------------------------------------------------------------- the generator

TEST(ReplayGenerator, SeedingStartsTheCountAgain)
{
    game::World world = new_world();
    world.next_random();
    world.seed_rng(99);

    EXPECT_EQ(world.rng_seed, 99u);
    EXPECT_EQ(world.rng_draws, 0u);
    EXPECT_TRUE(world.rng == std::mt19937(99));
}

TEST(ReplayGenerator, ANewWorldIsSeededWithTheDefaultAndHasDrawnNothing)
{
    const game::World world = new_world();

    EXPECT_EQ(world.rng_seed, game::DEFAULT_SEED);
    EXPECT_EQ(world.rng_draws, 0u);
    EXPECT_TRUE(world.rng == std::mt19937(game::DEFAULT_SEED));
}

TEST(ReplayGenerator, EveryDrawIsTheGeneratorsNextNumberAndIsCounted)
{
    game::World world = new_world();
    world.seed_rng(5);
    std::mt19937 expected(5);

    for (int i = 0; i < 10; ++i)
    {
        EXPECT_EQ(world.next_random(), expected());
    }
    EXPECT_EQ(world.rng_draws, 10u);
}

TEST(ReplayGenerator, ARestartKeepsTheGeneratorAndTheCount)
{
    game::World world = new_world();
    world.seed_rng(5);
    game::start_game(world, game::parse_level("1"));
    world.next_random();
    world.next_random();
    const std::mt19937 before = world.rng;

    world.state = game::GameState::GameOver;
    game::step(world, RESTART, game::FIXED_DT);

    EXPECT_EQ(world.state, game::GameState::Ready);
    EXPECT_EQ(world.rng_draws, 2u);
    EXPECT_TRUE(world.rng == before);
}

// The guard: if anything in a game draws from the generator without counting
// it, the seed and the count no longer lead to where the generator is, and
// replays of every game after the first would go wrong.
TEST(ReplayGenerator, TheSeedAndTheCountFindTheGeneratorAfterALongGame)
{
    const game::World& world = long_game().world;

    std::mt19937 found(world.rng_seed);
    found.discard(world.rng_draws);

    EXPECT_GT(world.rng_draws, 100u) << "too few capsules were rolled for this to show anything";
    EXPECT_TRUE(found == world.rng) << "something drew from the generator without next_random";
}

// ---------------------------------------------------------------- the recorder

TEST(Recorder, DoesNothingBeforeItIsStarted)
{
    game::World world = new_world();
    game::start_game(world, game::parse_level("1"));
    game::Recorder recorder;

    EXPECT_FALSE(recorder.step(world, LAUNCH));
    EXPECT_FALSE(recorder.recording());
    EXPECT_TRUE(recorder.replay().runs.empty());
    EXPECT_EQ(world.state, game::GameState::Playing) << "the step still runs";
}

TEST(Recorder, StartStartsTheGameAndRecordsWhereTheGeneratorIs)
{
    game::World world = new_world();
    world.seed_rng(77);
    world.next_random();
    world.next_random();
    world.next_random();

    const std::vector<game::Level> levels = two_levels();
    game::Recorder recorder;
    recorder.start(world, levels);

    EXPECT_TRUE(recorder.recording());
    EXPECT_EQ(recorder.replay().seed, 77u);
    EXPECT_EQ(recorder.replay().skip, 3u);
    EXPECT_EQ(recorder.replay().levels, levels);
    EXPECT_TRUE(recorder.replay().runs.empty());

    EXPECT_EQ(world.levels, levels);
    EXPECT_EQ(world.state, game::GameState::Ready);
    EXPECT_EQ(world.bricks.size(), 38u);
}

TEST(Recorder, TheSameInputInARowIsOneRun)
{
    game::World world = new_world();
    game::Recorder recorder;
    recorder.start(world, {game::parse_level("1")});

    for (int i = 0; i < 5; ++i)
    {
        recorder.step(world, LEFT);
    }
    recorder.step(world, LAUNCH);
    recorder.step(world, RIGHT);
    recorder.step(world, RIGHT);

    const std::vector<game::ReplayRun> expected{{5, LEFT}, {1, LAUNCH}, {2, RIGHT}};
    EXPECT_EQ(recorder.replay().runs, expected);
    EXPECT_EQ(game::step_count(recorder.replay()), 8u);
}

TEST(Recorder, TheStepThatLosesTheGameEndsTheRecording)
{
    game::World world = new_world();
    game::Recorder recorder;
    recorder.start(world, {game::parse_level("1")});
    world.score = 30;

    std::vector<bool> ended;
    lose_the_game(world, recorder, ended);

    ASSERT_FALSE(ended.empty());
    EXPECT_TRUE(ended.back()) << "the last step lost the game";
    EXPECT_EQ(std::count(ended.begin(), ended.end(), true), 1);
    EXPECT_FALSE(recorder.recording());
    EXPECT_EQ(recorder.replay().result, (game::ReplayResult{30, 0, 1, 1, game::GameState::GameOver}));
    EXPECT_EQ(game::step_count(recorder.replay()), ended.size());
}

TEST(Recorder, TheStepThatWinsEndsTheRecording)
{
    game::World world = new_world();
    game::Recorder recorder;
    recorder.start(world, {game::parse_level("1"), game::parse_level("2")});
    world.level_index = 1;
    game::start_level(world);
    world.score = 120;

    recorder.step(world, LAUNCH);
    world.destroy_entity(world.bricks.entities()[0]);

    EXPECT_TRUE(recorder.step(world, NOTHING));
    EXPECT_EQ(world.state, game::GameState::Won);
    EXPECT_FALSE(recorder.recording());
    EXPECT_EQ(recorder.replay().result, (game::ReplayResult{120, 3, 2, 0, game::GameState::Won}));
}

TEST(Recorder, StepsAfterTheEndAreNotRecorded)
{
    game::World world = new_world();
    game::Recorder recorder;
    recorder.start(world, {game::parse_level("1")});
    std::vector<bool> ended;
    lose_the_game(world, recorder, ended);
    const game::Replay finished = recorder.replay();

    EXPECT_FALSE(recorder.step(world, LEFT));
    EXPECT_FALSE(recorder.step(world, PAUSE));
    EXPECT_FALSE(recorder.step(world, LAUNCH));

    EXPECT_EQ(recorder.replay(), finished);
    EXPECT_FALSE(recorder.recording());
}

TEST(Recorder, RAfterTheEndStartsTheNextRecordingWhereTheGeneratorIs)
{
    game::World world = new_world();
    world.seed_rng(11);
    game::Recorder recorder;
    recorder.start(world, two_levels());

    std::vector<bool> ended;
    lose_the_game(world, recorder, ended);

    // As if bricks had dropped capsules along the way.
    world.next_random();
    world.next_random();

    const game::Input r_and_left = keys(true, false, false, false, true);
    EXPECT_FALSE(recorder.step(world, r_and_left));

    EXPECT_TRUE(recorder.recording());
    EXPECT_EQ(world.state, game::GameState::Ready);
    EXPECT_EQ(recorder.replay().seed, 11u);
    EXPECT_EQ(recorder.replay().skip, 2u);
    EXPECT_EQ(recorder.replay().levels, two_levels());
    EXPECT_EQ(recorder.replay().runs, (std::vector<game::ReplayRun>{{1, r_and_left}}));

    // The R step belongs to the new game, and plays back to the same place.
    EXPECT_EQ(describe(game::play_replay(recorder.replay()).world), describe(world));
}

TEST(Recorder, RDuringPlayIsOnlyInput)
{
    game::World world = new_world();
    game::Recorder recorder;
    recorder.start(world, {game::parse_level("1")});

    recorder.step(world, LAUNCH);
    const game::Replay before = recorder.replay();
    recorder.step(world, RESTART);

    EXPECT_EQ(world.state, game::GameState::Playing);
    EXPECT_EQ(recorder.replay().skip, before.skip);
    EXPECT_EQ(recorder.replay().runs, (std::vector<game::ReplayRun>{{1, LAUNCH}, {1, RESTART}}));
}

TEST(Recorder, PausedStepsAreRecordedAndPlayBack)
{
    game::World world = new_world();
    game::Recorder recorder;
    recorder.start(world, {game::parse_level(game::BUILT_IN_LEVEL)});

    recorder.step(world, LAUNCH);
    for (int i = 0; i < 20; ++i)
    {
        recorder.step(world, RIGHT);
    }
    recorder.step(world, PAUSE);
    for (int i = 0; i < 40; ++i)
    {
        recorder.step(world, LEFT);
    }
    recorder.step(world, PAUSE);
    for (int i = 0; i < 30; ++i)
    {
        recorder.step(world, NOTHING);
    }

    const std::vector<game::ReplayRun> expected{{1, LAUNCH}, {20, RIGHT}, {1, PAUSE}, {40, LEFT}, {1, PAUSE}, {30, NOTHING}};
    EXPECT_EQ(recorder.replay().runs, expected);
    EXPECT_EQ(world.state, game::GameState::Playing);
    EXPECT_EQ(describe(game::play_replay(recorder.replay()).world), describe(world));
}

TEST(Recorder, ATestPlayIsANewRecordingWithTheCanvasAsItsOnlyLevel)
{
    game::World world = new_world();
    world.seed_rng(3);
    game::Recorder recorder;
    recorder.start(world, two_levels());
    for (int step = 0; step < 600; ++step)
    {
        recorder.step(world, autopilot(world, step));
    }

    // What Enter in the editor will do: the generator carries on.
    world.next_random();
    game::Level canvas = game::editor_canvas(game::Level{});
    canvas.set(2, 1, 1);
    canvas.set(3, 1, 4);
    canvas.set(5, 2, game::STEEL);
    recorder.start(world, {canvas});

    EXPECT_EQ(recorder.replay().levels, std::vector<game::Level>{canvas});
    EXPECT_EQ(recorder.replay().skip, world.rng_draws);
    EXPECT_TRUE(recorder.replay().runs.empty());

    bool ended = false;
    for (int step = 0; step < 36000 && !ended; ++step)
    {
        ended = recorder.step(world, autopilot(world, step));
        world.events.clear();
    }

    const game::ParsedReplay parsed = game::parse_replay(game::to_text(recorder.replay()));
    ASSERT_FALSE(parsed.problem) << problem_text(parsed.problem);
    EXPECT_TRUE(ended) << "the canvas game did not end";
    const game::PlayedReplay played = game::play_replay(parsed.replay);
    EXPECT_EQ(describe(played.world), describe(world));
    EXPECT_TRUE(game::replay_matches(parsed.replay, played));
}

// ---------------------------------------------------------------- playing back

TEST(PlayReplay, StartsTheGeneratorFromTheSeedAndTheSkip)
{
    game::Replay replay = small_replay();
    replay.runs.clear();

    const game::World world = game::play_replay(replay).world;

    std::mt19937 expected(42);
    expected.discard(7);
    EXPECT_TRUE(world.rng == expected);
    EXPECT_EQ(world.rng_seed, 42u);
    EXPECT_EQ(world.rng_draws, 7u);
    EXPECT_EQ(world.size.x, game::WORLD_SIZE.x);
    EXPECT_EQ(world.size.y, game::WORLD_SIZE.y);
    EXPECT_EQ(world.levels, replay.levels);
    EXPECT_EQ(world.state, game::GameState::Ready);
}

TEST(PlayReplay, PlaysTheRunsStepByStep)
{
    game::Replay replay;
    replay.levels = {game::parse_level(game::BUILT_IN_LEVEL)};
    replay.runs = {{30, LEFT}, {1, LAUNCH}, {200, RIGHT}, {1, PAUSE}, {10, NOTHING}, {1, PAUSE}, {400, LEFT}};

    game::World stepped = new_world();
    game::start_game(stepped, replay.levels);
    for (const game::ReplayRun& run : replay.runs)
    {
        for (std::uint32_t i = 0; i < run.count; ++i)
        {
            game::step(stepped, run.input, game::FIXED_DT);
        }
    }

    const game::World played = game::play_replay(replay).world;

    EXPECT_GT(played.score, 0) << "nothing was hit, so this shows little";
    EXPECT_EQ(describe(played), describe(stepped));
    EXPECT_TRUE(played.events.empty()) << "events are cleared after every step";
}

// Every game the autopilot finished, saved as text, read back and played,
// ends exactly as it did: the result, every ball, brick, steel, capsule and
// the paddle, and the generator.
TEST(PlayReplay, EveryGameOfALongSessionPlaysBackExactly)
{
    const LongGame& run = long_game();

    ASSERT_GE(run.finished.size(), 3u);
    EXPECT_GE(run.wins, 1) << "no game was won";
    EXPECT_GE(run.losses, 1) << "no game was lost";
    EXPECT_GE(run.most_balls, 3) << "multi-ball never added balls";
    EXPECT_GT(run.paused_steps, 0);
    EXPECT_TRUE(run.played_steel);
    EXPECT_EQ(run.finished[0].replay.skip, 0u);
    EXPECT_GT(run.finished.back().replay.skip, 0u) << "no game started with numbers already drawn";

    for (std::size_t i = 0; i < run.finished.size(); ++i)
    {
        SCOPED_TRACE("game " + std::to_string(i + 1));
        const game::Replay& recorded = run.finished[i].replay;

        const game::ParsedReplay parsed = game::parse_replay(game::to_text(recorded));
        ASSERT_FALSE(parsed.problem) << problem_text(parsed.problem);
        EXPECT_EQ(parsed.replay, recorded);

        const game::PlayedReplay played = game::play_replay(parsed.replay);
        EXPECT_EQ(game::result_of(played.world), recorded.result);
        EXPECT_EQ(played.ended_on_step, game::step_count(recorded));
        EXPECT_TRUE(game::replay_matches(recorded, played));
        EXPECT_EQ(describe(played.world), run.finished[i].world);
    }
}

// ---------------------------------------------------------------- MATCH

namespace
{
    // A game over played for real, never touched behind the recorder's back:
    // the paddle goes left and the ball is launched whenever it is held.
    game::Replay recorded_game_over()
    {
        game::World world = new_world();
        world.seed_rng(2024);
        game::Recorder recorder;
        recorder.start(world, {game::parse_level(game::BUILT_IN_LEVEL)});

        bool ended = false;
        for (int step = 0; step < 20000 && !ended; ++step)
        {
            ended = recorder.step(world, step < 60 ? LEFT : (world.state == game::GameState::Ready ? LAUNCH : NOTHING));
            world.events.clear();
        }
        EXPECT_TRUE(ended);
        return recorder.replay();
    }
}

TEST(ReplayMatch, TheSameGameEndingOnTheLastStepMatches)
{
    const game::Replay replay = recorded_game_over();
    const game::PlayedReplay played = game::play_replay(replay);

    EXPECT_EQ(played.ended_on_step, game::step_count(replay));
    EXPECT_GT(replay.result.bricks, 0);
    EXPECT_TRUE(game::replay_matches(replay, played));
}

TEST(ReplayMatch, EndingBeforeTheLastStepIsDifferent)
{
    game::Replay replay = recorded_game_over();
    replay.runs.push_back({1, NOTHING});

    const game::PlayedReplay played = game::play_replay(replay);

    EXPECT_EQ(played.ended_on_step, game::step_count(replay) - 1);
    EXPECT_EQ(game::result_of(played.world), replay.result) << "only the step differs";
    EXPECT_FALSE(game::replay_matches(replay, played));
}

TEST(ReplayMatch, NotEndingByTheLastStepIsDifferent)
{
    game::Replay replay = recorded_game_over();
    replay.runs.back().count -= 1;
    if (replay.runs.back().count == 0)
    {
        replay.runs.pop_back();
    }

    const game::PlayedReplay played = game::play_replay(replay);

    EXPECT_EQ(played.ended_on_step, 0u);
    EXPECT_FALSE(game::replay_matches(replay, played));
}

TEST(ReplayMatch, EveryNumberOfTheResultCounts)
{
    const game::Replay recorded = recorded_game_over();
    const game::PlayedReplay played = game::play_replay(recorded);
    ASSERT_TRUE(game::replay_matches(recorded, played));

    for (int field = 0; field < 5; ++field)
    {
        game::Replay replay = recorded;
        game::ReplayResult& result = replay.result;
        switch (field)
        {
        case 0: result.score += 10; break;
        case 1: result.lives += 1; break;
        case 2: result.level += 1; break;
        case 3: result.bricks -= 1; break;
        case 4: result.state = game::GameState::Won; break;
        }
        EXPECT_FALSE(game::replay_matches(replay, played)) << "field " << field;
    }
}

// The game has to end on the last step; R in the file after a first end does
// not make the second end count. Played by hand rather than recorded, since
// the recorder would have started a second replay at R.
TEST(ReplayMatch, EndingRAndEndingAgainOnTheLastStepIsDifferent)
{
    const game::Replay first = recorded_game_over();
    game::Replay replay = first;
    game::World world = game::play_replay(first).world;
    ASSERT_EQ(world.state, game::GameState::GameOver);

    bool ended = false;
    for (int step = -1; step < 20000 && !ended; ++step)
    {
        const game::Input input = step < 0 ? RESTART
                                  : step < 60 ? LEFT
                                  : world.state == game::GameState::Ready ? LAUNCH : NOTHING;
        replay.runs.push_back({1, input});
        game::step(world, input, game::FIXED_DT);
        world.events.clear();
        ended = world.state == game::GameState::GameOver || world.state == game::GameState::Won;
    }
    ASSERT_TRUE(ended);
    replay.result = game::result_of(world);

    const game::ParsedReplay parsed = game::parse_replay(game::to_text(replay));
    ASSERT_FALSE(parsed.problem) << problem_text(parsed.problem);
    const game::PlayedReplay played = game::play_replay(parsed.replay);

    EXPECT_EQ(played.ended_on_step, game::step_count(first)) << "the first end counts";
    EXPECT_EQ(game::result_of(played.world), replay.result) << "the playback does end as the file says";
    EXPECT_FALSE(game::replay_matches(parsed.replay, played));
}

// With no steps the game never ends, so it is never a match, not even when
// the result is exactly how the game starts. Only a Replay built in code can
// get here: a file cannot end in ready.
TEST(ReplayMatch, AReplayWithNoStepsNeverMatches)
{
    game::Replay replay = small_replay();
    replay.runs.clear();
    replay.result = game::result_of(game::play_replay(replay).world);
    ASSERT_EQ(replay.result.state, game::GameState::Ready);

    const game::PlayedReplay played = game::play_replay(replay);

    EXPECT_EQ(played.ended_on_step, 0u);
    EXPECT_EQ(game::result_of(played.world), replay.result);
    EXPECT_FALSE(game::replay_matches(replay, played));
}

// What the Enter bug found with the fake platform did: the game restarted on
// a test play, but the recording carried on with the old levels. The paddle is
// parked on the left, so the playback scores what the game scored and even
// ends on the same step; only the bricks left give it away (36, not 29).
TEST(ReplayMatch, ARecordingThatMissedARestartIsDifferent)
{
    game::World world = new_world();
    world.seed_rng(2024);
    game::Recorder recorder;
    recorder.start(world, {game::parse_level(game::BUILT_IN_LEVEL)});

    for (int step = 0; step < 60; ++step)
    {
        recorder.step(world, LEFT);
    }

    // The bug: a new game without a new recording.
    game::Level canvas = game::editor_canvas(game::parse_level(game::BUILT_IN_LEVEL));
    for (int column = 1; column <= 7; ++column)
    {
        canvas.set(column, 2, 0);
    }
    game::start_game(world, canvas);

    bool ended = false;
    for (int step = 0; step < 20000 && !ended; ++step)
    {
        ended = recorder.step(world, world.state == game::GameState::Ready ? LAUNCH : NOTHING);
        world.events.clear();
    }
    ASSERT_TRUE(ended);

    const game::Replay& replay = recorder.replay();
    const game::PlayedReplay played = game::play_replay(replay);

    EXPECT_FALSE(game::replay_matches(replay, played));
    EXPECT_NE(game::result_of(played.world).bricks, replay.result.bricks);
}

// ---------------------------------------------------------------- the text

TEST(ReplayText, IsWrittenExactlyLikeThis)
{
    EXPECT_EQ(game::to_text(small_replay()), SMALL_TEXT);
}

TEST(ReplayText, ReadsBackWhatIsWritten)
{
    const game::ParsedReplay parsed = game::parse_replay(SMALL_TEXT);

    ASSERT_FALSE(parsed.problem) << problem_text(parsed.problem);
    EXPECT_EQ(parsed.replay, small_replay());
}

TEST(ReplayText, TakesCrlfAByteOrderMarkAndBlankLinesAtTheEnd)
{
    std::string text = "\xEF\xBB\xBF";
    for (const char character : SMALL_TEXT)
    {
        text += (character == '\n') ? std::string("\r\n") : std::string(1, character);
    }
    text += "\r\n  \r\n";

    const game::ParsedReplay parsed = game::parse_replay(text);

    ASSERT_FALSE(parsed.problem) << problem_text(parsed.problem);
    EXPECT_EQ(parsed.replay, small_replay());
}

TEST(ReplayText, TakesNoNewlineAtTheEndAndKeysInAnyOrder)
{
    std::string text = replace_line(12, "2 restart pause  right");
    text.pop_back();

    const game::ParsedReplay parsed = game::parse_replay(text);

    ASSERT_FALSE(parsed.problem) << problem_text(parsed.problem);
    EXPECT_EQ(parsed.replay, small_replay());
}

TEST(ReplayText, AGameOverAndAWinAreWrittenAndReadBack)
{
    for (const game::GameState state : {game::GameState::GameOver, game::GameState::Won})
    {
        game::Replay replay = small_replay();
        replay.result.state = state;

        const game::ParsedReplay parsed = game::parse_replay(game::to_text(replay));
        ASSERT_FALSE(parsed.problem) << problem_text(parsed.problem);
        EXPECT_EQ(parsed.replay.result.state, state);
    }
}

// Read, not played: skipping that many takes seconds, or a minute in Debug.
TEST(ReplayText, ASkipAtTheLimitIsRead)
{
    const game::ParsedReplay parsed = game::parse_replay(replace_line(3, "skip 4294967295"));

    ASSERT_FALSE(parsed.problem) << problem_text(parsed.problem);
    EXPECT_EQ(parsed.replay.skip, 4294967295u);
}

TEST(ReplayText, ALongRunIsOneLine)
{
    game::Replay replay = small_replay();
    replay.runs = {{4294967295u, PAUSE}};
    const std::string text = game::to_text(replay);

    EXPECT_NE(text.find("\n4294967295 pause\nend steps 4294967295 "), std::string::npos);
    const game::ParsedReplay parsed = game::parse_replay(text);
    ASSERT_FALSE(parsed.problem) << problem_text(parsed.problem);
    EXPECT_EQ(parsed.replay, replay);
}

// ---------------------------------------------------------------- bad files

namespace
{
    struct BadText
    {
        const char* name;
        std::string text;
        int line;
        int column;
        std::string message;
    };

    void PrintTo(const BadText& bad, std::ostream* out)
    {
        *out << bad.name;
    }

    // The second level made 18 rows deep, with a brick in the last row, on
    // line 25 of the file.
    std::string too_low_level()
    {
        std::string rows = "level 18\n1\n";
        for (int i = 0; i < 16; ++i)
        {
            rows += ".\n";
        }
        rows += "1\n";

        std::string text(SMALL_TEXT);
        const std::string second_level = "level 1\n#2\n";
        return text.replace(text.find(second_level), second_level.size(), rows);
    }

    const std::vector<BadText>& bad_texts()
    {
        static const std::vector<BadText> cases{
            {"Empty", "", 0, 0, "the file is empty"},
            {"OnlyBlankLines", "\n  \n\r\n", 0, 0, "the file is empty"},
            {"NotAReplay", replace_line(1, "..111.."), 1, 0,
             "this is not a breakout replay: the first line should be 'breakout replay 1'"},
            {"AnotherVersion", replace_line(1, "breakout replay 2"), 1, 0,
             "this is a version 2 replay, but only version 1 can be read"},
            {"SeedMisspelt", replace_line(2, "sed 42"), 2, 0,
             "expected 'seed' and a number from 0 to 4294967295, not 'sed 42'"},
            {"SeedNegative", replace_line(2, "seed -1"), 2, 0,
             "expected 'seed' and a number from 0 to 4294967295, not 'seed -1'"},
            {"SeedTooBig", replace_line(2, "seed 4294967296"), 2, 0,
             "expected 'seed' and a number from 0 to 4294967295, not 'seed 4294967296'"},
            {"SeedMissing", replace_line(2, "seed"), 2, 0,
             "expected 'seed' and a number from 0 to 4294967295, not 'seed'"},
            {"SkipNotANumber", replace_line(3, "skip 7x"), 3, 0,
             "expected 'skip' and a number from 0 to 4294967295, not 'skip 7x'"},
            {"SkipLeftOut", replace_line(3, ""), 3, 0,
             "expected 'skip' and a number from 0 to 4294967295, not 'level 2'"},
            {"SkipTooBig", replace_line(3, "skip 4294967296"), 3, 0,
             "expected 'skip' and a number from 0 to 4294967295, not 'skip 4294967296'"},
            {"SkipFarTooBig", replace_line(3, "skip 18446744073709551616"), 3, 0,
             "expected 'skip' and a number from 0 to 4294967295, not 'skip 18446744073709551616'"},
            {"LevelOfNoRows", replace_line(4, "level 0"), 4, 0,
             "'0' is not a number of rows: a level has at least one row"},
            {"LevelWithFewerRows", replace_line(4, "level 3"), 7, 0,
             "the level that starts on line 4 has 2 rows, not the 3 its 'level' line says"},
            {"LevelWithABadCharacter", replace_line(6, ".#x"), 6, 3,
             "'x' is not allowed: a level may only contain digits, '#', '.' and spaces"},
            {"LevelWithATab", replace_line(5, "1\t1"), 5, 2,
             "a tab is not allowed: a level may only contain digits, '#', '.' and spaces"},
            {"LevelTooWide", replace_line(8, std::string(48, '1')), 8, 48,
             "the row is 48 columns wide, but at most 47 fit (trailing spaces count too)"},
            {"LevelTooLow", too_low_level(), 25, 1,
             "a brick in row 18 is too low: at most 17 rows fit above the paddle"},
            {"LevelOfOnlySteel", replace_line(8, "##"), 7, 0,
             "the level that starts on line 7: the level has only steel bricks, which cannot be broken"},
            {"LevelWithNoBricks", replace_line(8, ".."), 7, 0,
             "the level that starts on line 7: the level has no bricks"},
            {"LevelWithABlankRow", replace_line(5, "   "), 5, 0, "a level row cannot be blank: an empty cell is '.'"},
            {"NoLevel", replace_line(4, "steps\n3 -\nend steps 3 score 0 lives 3 level 1 state ready"), 4, 0,
             "there is no level before 'steps': a replay needs at least one"},
            {"NotALevelLine", replace_line(7, "lvl 1"), 7, 0,
             "expected 'level' and its number of rows, or 'steps', not 'lvl 1'"},
            {"CountZero", replace_line(10, "0 -"), 10, 0,
             "'0' is not a step count: a count is a whole number from 1 to 4294967295"},
            {"CountNotANumber", replace_line(10, "three -"), 10, 0,
             "'three' is not a step count: a count is a whole number from 1 to 4294967295"},
            {"CountTooBig", replace_line(10, "4294967296 -"), 10, 0,
             "'4294967296' is not a step count: a count is a whole number from 1 to 4294967295"},
            {"CountWithoutKeys", replace_line(10, "3"), 10, 0, "the step count has no keys after it: '-' is for none"},
            {"DashAndAKey", replace_line(10, "3 - left"), 10, 0, "'-' means no keys, so nothing can follow it"},
            {"UnknownKey", replace_line(11, "1 left jump"), 11, 0,
             "'jump' is not a key: the keys are left, right, launch, pause and restart"},
            {"KeyTwice", replace_line(11, "1 left launch left"), 11, 0, "'left' is given twice"},
            {"BlankStepLine", replace_line(11, "\n1 left launch"), 11, 0,
             "a blank line: expected a step count and its keys, or the end line"},
            {"CutShortInTheSteps", replace_line(13, ""), 0, 0,
             "the file ends where a step count and its keys, or the end line should be: it may have been cut short"},
            {"CutShortInTheHeader", "breakout replay 1\nseed 42\n", 0, 0,
             "the file ends where 'skip' and a number from 0 to 4294967295 should be: it may have been cut short"},
            {"CutShortInALevel", "breakout replay 1\nseed 42\nskip 0\nlevel 2\n1.1\n", 0, 0,
             "the file ends inside the level that starts on line 4: it may have been cut short"},
            {"EndLineShort", replace_line(13, "end steps 6 score 10"), 13, 0,
             "expected 'end steps <n> score <n> lives <n> level <n> bricks <n> state <state>', "
             "not 'end steps 6 score 10'"},
            {"EndLineWithoutBricks", replace_line(13, "end steps 6 score 10 lives 2 level 2 state gameover"), 13, 0,
             "expected 'end steps <n> score <n> lives <n> level <n> bricks <n> state <state>', "
             "not 'end steps 6 score 10 lives 2 level 2"},
            {"EndLineNegativeLives", replace_line(13, "end steps 6 score 10 lives -1 level 2 bricks 1 state gameover"),
             13, 0,
             "expected 'end steps <n> score <n> lives <n> level <n> bricks <n> state <state>', "
             "not 'end steps 6 score 10 lives -1"},
            {"EndLineBricksNotANumber", replace_line(13, "end steps 6 score 10 lives 2 level 2 bricks x state gameover"),
             13, 0, "expected 'end steps <n> score <n> lives <n> level <n> bricks <n> state <state>'"},
            {"EndLineUnknownState", replace_line(13, "end steps 6 score 10 lives 2 level 2 bricks 1 state lost"), 13, 0,
             "'lost' cannot end a replay: a replay ends in gameover or won"},
            {"EndLineStillPlaying", replace_line(13, "end steps 6 score 10 lives 2 level 2 bricks 1 state playing"), 13,
             0, "'playing' cannot end a replay: a replay ends in gameover or won"},
            {"EndLineReady", replace_line(13, "end steps 6 score 10 lives 2 level 2 bricks 1 state ready"), 13, 0,
             "'ready' cannot end a replay: a replay ends in gameover or won"},
            {"EndLinePaused", replace_line(13, "end steps 6 score 10 lives 2 level 2 bricks 1 state paused"), 13, 0,
             "'paused' cannot end a replay: a replay ends in gameover or won"},
            {"EndLineWrongSteps", replace_line(13, "end steps 7 score 10 lives 2 level 2 bricks 1 state gameover"), 13, 0,
             "the end line says 7 steps, but the step lines add up to 6"},
            {"EndLineLevelTooHigh", replace_line(13, "end steps 6 score 10 lives 2 level 3 bricks 1 state gameover"), 13,
             0, "the end line says level 3, but the replay has 2 levels"},
            {"EndLineLevelZero", replace_line(13, "end steps 6 score 10 lives 2 level 0 bricks 1 state gameover"), 13, 0,
             "the end line says level 0, but the replay has 2 levels"},
            {"SomethingAfterTheEnd", std::string(SMALL_TEXT) + "\n1 left\n", 15, 0, "nothing can follow the end line"},
        };
        return cases;
    }

    class BadReplayText : public ::testing::TestWithParam<BadText>
    {
    };
}

TEST_P(BadReplayText, IsRefusedWithWhereAndWhy)
{
    const BadText& bad = GetParam();

    const game::ParsedReplay parsed = game::parse_replay(bad.text);

    ASSERT_TRUE(parsed.problem.has_value()) << "the text was read:\n" << bad.text;
    EXPECT_EQ(parsed.problem->line, bad.line) << problem_text(parsed.problem);
    EXPECT_EQ(parsed.problem->column, bad.column) << problem_text(parsed.problem);
    EXPECT_EQ(parsed.problem->message.substr(0, bad.message.size()), bad.message) << problem_text(parsed.problem);
}

INSTANTIATE_TEST_SUITE_P(Replay, BadReplayText, ::testing::ValuesIn(bad_texts()),
                         [](const ::testing::TestParamInfo<BadText>& info) { return std::string(info.param.name); });

// ---------------------------------------------------------------- files

namespace
{
    class ReplayFiles : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            const std::string test_name = ::testing::UnitTest::GetInstance()->current_test_info()->name();
            const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
            root = fs::temp_directory_path() / ("breakout-replay-files-" + test_name + "-" + std::to_string(stamp));
            fs::create_directories(root);
        }

        void TearDown() override
        {
            std::error_code ignored;
            fs::remove_all(root, ignored);
        }

        void write(const fs::path& path, std::string_view contents) const
        {
            std::ofstream file(path, std::ios::binary);
            file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
            ASSERT_TRUE(file.good());
        }

        static std::string read(const fs::path& path)
        {
            std::ifstream file(path, std::ios::binary);
            return std::string{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        }

        fs::path root;
    };
}

TEST_F(ReplayFiles, SavingMakesTheFolderAndWritesTheText)
{
    const fs::path path = root / "bin" / "replays" / "last.txt";

    EXPECT_EQ(game::save_replay(path, small_replay()), "");
    EXPECT_EQ(read(path), SMALL_TEXT) << "the bytes on disk are the text, with LF line ends";
}

TEST_F(ReplayFiles, SavingOverwritesTheLastOne)
{
    const fs::path path = root / "last.txt";
    game::Replay longer = small_replay();
    longer.runs.push_back({100, LEFT});

    ASSERT_EQ(game::save_replay(path, longer), "");
    ASSERT_EQ(game::save_replay(path, small_replay()), "");

    EXPECT_EQ(read(path), SMALL_TEXT);
}

TEST_F(ReplayFiles, LoadingReadsBackWhatWasSaved)
{
    const fs::path path = root / "replays" / "last.txt";
    game::World world = new_world();
    world.seed_rng(8);
    world.next_random();
    game::Recorder recorder;
    recorder.start(world, two_levels());
    for (int step = 0; step < 1200; ++step)
    {
        recorder.step(world, autopilot(world, step));
    }
    const game::Replay recorded = recorder.replay();
    ASSERT_GT(recorded.runs.size(), 10u);
    ASSERT_EQ(game::save_replay(path, recorded), "");

    const game::LoadedReplay loaded = game::load_replay(path);

    ASSERT_TRUE(loaded.ok) << loaded.error;
    EXPECT_EQ(loaded.error, "");
    EXPECT_EQ(loaded.replay, recorded);
}

TEST_F(ReplayFiles, AMissingFileIsAnError)
{
    const game::LoadedReplay loaded = game::load_replay(root / "last.txt");

    EXPECT_FALSE(loaded.ok);
    EXPECT_EQ(loaded.error, "last.txt: there is no such file");
}

TEST_F(ReplayFiles, AFolderIsNotAReplay)
{
    fs::create_directories(root / "last.txt");

    EXPECT_EQ(game::load_replay(root / "last.txt").error, "last.txt: it is not a file that can be read");
}

TEST_F(ReplayFiles, ABadFileSaysItsNameAndLine)
{
    const fs::path path = root / "last.txt";
    write(path, replace_line(11, "1 left jump"));

    const game::LoadedReplay loaded = game::load_replay(path);

    EXPECT_FALSE(loaded.ok);
    EXPECT_EQ(loaded.error, "last.txt:11: 'jump' is not a key: the keys are left, right, launch, pause and restart");
    EXPECT_TRUE(loaded.replay.levels.empty());
}

TEST_F(ReplayFiles, ABadLevelSaysItsNameLineAndColumn)
{
    const fs::path path = root / "last.txt";
    write(path, replace_line(6, ".#x"));

    EXPECT_EQ(game::load_replay(path).error,
              "last.txt:6:3: 'x' is not allowed: a level may only contain digits, '#', '.' and spaces");
}

TEST_F(ReplayFiles, AProblemWithTheWholeFileHasNoLine)
{
    const fs::path path = root / "last.txt";
    write(path, "");

    EXPECT_EQ(game::load_replay(path).error, "last.txt: the file is empty");
}

TEST_F(ReplayFiles, SavingWhereAFolderIsInTheWayIsAnError)
{
    const fs::path path = root / "last.txt";
    fs::create_directories(path);

    const std::string error = game::save_replay(path, small_replay());

    EXPECT_EQ(error.rfind("last.txt: ", 0), 0u) << error;
}
