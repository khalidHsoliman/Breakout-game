#include <gtest/gtest.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "game/Spawn.h"
#include "game/Systems.h"

namespace
{
    constexpr float DT = 1.0f / 60.0f;

    // A different number of bricks in each, so the brick count says which
    // level is on screen.
    std::vector<game::Level> three_levels()
    {
        return {game::parse_level("1"), game::parse_level("11"), game::parse_level("111")};
    }

    game::World started_world(std::vector<game::Level> levels)
    {
        game::World world;
        world.size = math::Vec2{800.0f, 600.0f};
        game::start_game(world, std::move(levels));
        return world;
    }

    game::Input launch_input()
    {
        game::Input input;
        input.launch = true;
        return input;
    }

    game::Input restart_input()
    {
        game::Input input;
        input.restart = true;
        return input;
    }

    void launch_if_ready(game::World& world)
    {
        if (world.state == game::GameState::Ready)
        {
            game::step(world, launch_input(), DT);
        }
    }

    // Removes every brick while playing, so the next step sees a cleared level.
    void clear_level(game::World& world)
    {
        launch_if_ready(world);

        const std::vector<core::Entity> bricks = world.bricks.entities();
        for (const core::Entity brick : bricks)
        {
            world.destroy_entity(brick);
        }

        game::step(world, game::Input{}, DT);
    }

    void lose_a_life(game::World& world)
    {
        launch_if_ready(world);

        const core::Entity ball = world.balls.entities()[0];
        world.transforms.find(ball)->position = math::Vec2{400.0f, -100.0f};

        game::step(world, game::Input{}, DT);
    }

    bool has_event(const game::World& world, game::GameEvent event)
    {
        return std::find(world.events.begin(), world.events.end(), event) != world.events.end();
    }
}

TEST(Progression, StartsOnTheFirstLevel)
{
    const game::World world = started_world(three_levels());

    EXPECT_EQ(world.level_index, 0);
    EXPECT_EQ(world.levels.size(), 3u);
    EXPECT_EQ(world.level, game::parse_level("1"));
    EXPECT_EQ(world.bricks.size(), 1u);
    EXPECT_EQ(world.state, game::GameState::Ready);
}

TEST(Progression, ClearingALevelMovesOnToTheNext)
{
    game::World world = started_world(three_levels());

    clear_level(world);

    EXPECT_EQ(world.level_index, 1);
    EXPECT_EQ(world.level, game::parse_level("11"));
    EXPECT_EQ(world.bricks.size(), 2u);
}

// The next level starts like the first did: ball held on the paddle, nothing
// left over from the last one.
TEST(Progression, TheNextLevelStartsReadyWithAFreshBallAndPaddle)
{
    game::World world = started_world(three_levels());

    clear_level(world);

    ASSERT_EQ(world.level_index, 1);
    EXPECT_EQ(world.state, game::GameState::Ready);
    EXPECT_EQ(world.balls.size(), 1u);
    EXPECT_EQ(world.paddles.size(), 1u);
    EXPECT_EQ(world.particles.size(), 0u);

    const core::Entity ball = world.balls.entities()[0];
    EXPECT_EQ(world.velocities.find(ball)->value.x, 0.0f);
    EXPECT_EQ(world.velocities.find(ball)->value.y, 0.0f);
}

TEST(Progression, KeepsScoreAndLivesBetweenLevels)
{
    game::World world = started_world(three_levels());
    launch_if_ready(world);
    world.score = 120;
    world.lives = 2;

    clear_level(world);

    ASSERT_EQ(world.level_index, 1);
    EXPECT_EQ(world.score, 120);
    EXPECT_EQ(world.lives, 2);
}

TEST(Progression, ClearingTheLastLevelWins)
{
    game::World world = started_world(three_levels());

    clear_level(world);
    clear_level(world);
    ASSERT_EQ(world.level_index, 2);
    ASSERT_NE(world.state, game::GameState::Won);

    clear_level(world);

    EXPECT_EQ(world.state, game::GameState::Won);
    EXPECT_EQ(world.level_index, 2);
}

// A single level is a game of one: clearing it wins, as it always did.
TEST(Progression, AOneLevelGameIsWonByClearingIt)
{
    game::World world = started_world({game::parse_level("11")});

    clear_level(world);

    EXPECT_EQ(world.state, game::GameState::Won);
    EXPECT_EQ(world.level_index, 0);
}

TEST(Progression, TheOneLevelOverloadIsAGameOfOneLevel)
{
    game::World world;
    world.size = math::Vec2{800.0f, 600.0f};
    game::start_game(world, game::parse_level("11"));

    EXPECT_EQ(world.levels.size(), 1u);
    EXPECT_EQ(world.level_index, 0);

    clear_level(world);
    EXPECT_EQ(world.state, game::GameState::Won);

    game::step(world, restart_input(), DT);
    EXPECT_EQ(world.state, game::GameState::Ready);
    EXPECT_EQ(world.bricks.size(), 2u);
}

TEST(Progression, RestartAfterAWinGoesBackToLevelOneWithScoreZero)
{
    game::World world = started_world(three_levels());
    clear_level(world);
    clear_level(world);
    world.score = 300;
    world.lives = 1;
    clear_level(world);
    ASSERT_EQ(world.state, game::GameState::Won);

    game::step(world, restart_input(), DT);

    EXPECT_EQ(world.state, game::GameState::Ready);
    EXPECT_EQ(world.level_index, 0);
    EXPECT_EQ(world.level, game::parse_level("1"));
    EXPECT_EQ(world.bricks.size(), 1u);
    EXPECT_EQ(world.score, 0);
    EXPECT_EQ(world.lives, game::STARTING_LIVES);
    EXPECT_EQ(world.levels.size(), 3u);
}

TEST(Progression, LosingALifeStaysOnTheSameLevel)
{
    game::World world = started_world(three_levels());
    clear_level(world);
    ASSERT_EQ(world.level_index, 1);

    lose_a_life(world);

    EXPECT_EQ(world.state, game::GameState::Ready);
    EXPECT_EQ(world.level_index, 1);
    EXPECT_EQ(world.lives, game::STARTING_LIVES - 1);
    EXPECT_EQ(world.bricks.size(), 2u);
}

TEST(Progression, GameOverOnLevelTwoRestartsFromLevelOne)
{
    game::World world = started_world(three_levels());
    clear_level(world);
    ASSERT_EQ(world.level_index, 1);
    world.score = 90;

    for (int life = 0; life < game::STARTING_LIVES; life++)
    {
        lose_a_life(world);
    }
    ASSERT_EQ(world.state, game::GameState::GameOver);
    EXPECT_EQ(world.level_index, 1);

    game::step(world, restart_input(), DT);

    EXPECT_EQ(world.state, game::GameState::Ready);
    EXPECT_EQ(world.level_index, 0);
    EXPECT_EQ(world.level, game::parse_level("1"));
    EXPECT_EQ(world.bricks.size(), 1u);
    EXPECT_EQ(world.score, 0);
    EXPECT_EQ(world.lives, game::STARTING_LIVES);
}

// Moving on rebuilds the world inside the step that noticed the level was
// clear. The hit on the last brick must survive that, or main never plays its
// sound.
TEST(Progression, TheHitOnTheLastBrickIsStillReported)
{
    game::World world = started_world(three_levels());
    launch_if_ready(world);

    // Just under the only brick of level 1, heading for it.
    const core::Entity ball = world.balls.entities()[0];
    const math::Vec2 brick = world.transforms.find(world.bricks.entities()[0])->position;
    world.transforms.find(ball)->position = math::Vec2{brick.x, brick.y - 30.0f};
    world.velocities.find(ball)->value = math::Vec2{0.0f, 300.0f};
    world.events.clear();

    for (int i = 0; i < 60 && world.level_index == 0; i++)
    {
        game::step(world, game::Input{}, DT);
    }

    ASSERT_EQ(world.level_index, 1);
    EXPECT_TRUE(has_event(world, game::GameEvent::BrickHit));
    EXPECT_EQ(world.score, 10);
}
