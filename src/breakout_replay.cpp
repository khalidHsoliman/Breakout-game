// breakout_replay <file>
//
// Plays a replay file through game::step, with no window and no sound, and
// prints how the game ended and on which step:
//
//   score 1230
//   lives 0
//   level 2
//   bricks 14
//   state gameover
//   ended on step 4321 of 4321
//   MATCH: the file expected score 1230 lives 0 level 2 bricks 14 state gameover on step 4321
//
// MATCH only when the game ended on the file's last step, not before and not
// after, as the file expected. Exit code 0 for MATCH, 2 for DIFFERENT, and 1
// for wrong arguments or a bad file, with the reason on stderr.

#include <cstdio>
#include <exception>

#include "game/Replay.h"

namespace
{
    constexpr int MATCHED = 0;
    constexpr int FAILED = 1;
    constexpr int DIFFERED = 2;
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::fprintf(stderr, "usage: breakout_replay <file>\n");
        return FAILED;
    }

    try
    {
        const game::LoadedReplay loaded = game::load_replay(argv[1]);
        if (!loaded.ok)
        {
            std::fprintf(stderr, "%s\n", loaded.error.c_str());
            return FAILED;
        }

        const game::Replay& replay = loaded.replay;
        const game::PlayedReplay played = game::play_replay(replay);
        const game::ReplayResult result = game::result_of(played.world);
        const game::ReplayResult& expected = replay.result;
        const unsigned long long steps = game::step_count(replay);

        std::printf("score %d\n", result.score);
        std::printf("lives %d\n", result.lives);
        std::printf("level %d\n", result.level);
        std::printf("bricks %d\n", result.bricks);
        std::printf("state %s\n", game::state_name(result.state));

        if (played.ended_on_step == 0)
        {
            std::printf("did not end in its %llu steps\n", steps);
        }
        else
        {
            std::printf("ended on step %llu of %llu\n", static_cast<unsigned long long>(played.ended_on_step), steps);
        }

        const bool matched = game::replay_matches(replay, played);
        std::printf("%s: the file expected score %d lives %d level %d bricks %d state %s on step %llu\n",
                    matched ? "MATCH" : "DIFFERENT", expected.score, expected.lives, expected.level,
                    expected.bricks, game::state_name(expected.state), steps);

        return matched ? MATCHED : DIFFERED;
    }
    catch (const std::exception& error)
    {
        // Out of memory, say. Nothing in reading or playing throws otherwise.
        std::fprintf(stderr, "breakout_replay: %s\n", error.what());
        return FAILED;
    }
}
