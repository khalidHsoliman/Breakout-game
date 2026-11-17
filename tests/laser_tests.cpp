#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <set>
#include <utility>
#include <vector>

#include "game/Spawn.h"
#include "game/Systems.h"

namespace
{
    constexpr float DT = 1.0f / 60.0f;
    constexpr float TOLERANCE = 1e-3f;

    // Straight above the paddle's start, where a test puts what a beam hits.
    constexpr float PADDLE_X = 400.0f;
    constexpr float LEFT_BEAM_X = PADDLE_X - (game::PADDLE_WIDTH * 0.5f - game::BEAM_INSET);
    constexpr float RIGHT_BEAM_X = PADDLE_X + (game::PADDLE_WIDTH * 0.5f - game::BEAM_INSET);

    game::World make_world()
    {
        game::World world;
        world.size = math::Vec2{800.0f, 600.0f};
        world.state = game::GameState::Playing;
        world.rng.seed(1);
        return world;
    }

    core::Entity add_brick(game::World& world, math::Vec2 position, int hit_points = 1,
                           math::Vec2 size = math::Vec2{60.0f, 24.0f})
    {
        const core::Entity entity = world.create_entity();
        world.transforms.add(entity, game::Transform{position, size, position});
        world.colors.add(entity, game::brick_color(hit_points));
        world.bricks.add(entity, game::Brick{hit_points});
        return entity;
    }

    core::Entity add_steel(game::World& world, math::Vec2 position)
    {
        const core::Entity entity = world.create_entity();
        world.transforms.add(entity, game::Transform{position, math::Vec2{60.0f, 24.0f}, position});
        world.colors.add(entity, game::brick_color(game::STEEL));
        world.steel.add(entity, game::Steel{});
        return entity;
    }

    core::Entity add_ball(game::World& world, math::Vec2 position, math::Vec2 velocity)
    {
        const core::Entity entity = world.create_entity();
        world.transforms.add(entity, game::Transform{position, math::Vec2{16.0f, 16.0f}, position});
        world.velocities.add(entity, game::Velocity{velocity});
        world.balls.add(entity, game::Ball{8.0f});
        return entity;
    }

    // Playing, with the real paddle, and a tough brick up in the corner, out
    // of every beam's way, so the level is never cleared while a test steps it.
    game::World playing_world()
    {
        game::World world = make_world();
        game::spawn_paddle(world);
        add_brick(world, math::Vec2{100.0f, 560.0f}, 9);
        return world;
    }

    core::Entity paddle_of(const game::World& world)
    {
        return world.paddles.entities()[0];
    }

    const game::Paddle& paddle_state(const game::World& world)
    {
        return *world.paddles.find(paddle_of(world));
    }

    game::Input launch_input()
    {
        game::Input input;
        input.launch = true;
        return input;
    }

    game::Input pause_input()
    {
        game::Input input;
        input.toggle_pause = true;
        return input;
    }

    void run_steps(game::World& world, int steps, const game::Input& input = game::Input{})
    {
        for (int i = 0; i < steps; i++)
        {
            game::step(world, input, DT);
        }
    }

    // A capsule dropped right onto the paddle, and one step to catch it.
    void catch_capsule(game::World& world, game::PowerUp kind)
    {
        const math::Vec2 paddle = world.transforms.find(paddle_of(world))->position;
        game::spawn_capsule(world, paddle, kind);
        game::step(world, game::Input{}, DT);
    }

    // One step with Space pressed.
    void press_space(game::World& world)
    {
        game::step(world, launch_input(), DT);
    }

    // A beam whose top is 5 units under the bottom of the 24-high brick at
    // brick_y, so the next step takes it in.
    core::Entity beam_under(game::World& world, float x, float brick_y)
    {
        return game::spawn_beam(world, math::Vec2{x, brick_y - 12.0f - game::BEAM_HEIGHT * 0.5f - 5.0f});
    }

    int count(const game::World& world, game::GameEvent event)
    {
        return static_cast<int>(std::count(world.events.begin(), world.events.end(), event));
    }

    game::World started_world(std::vector<game::Level> levels)
    {
        game::World world;
        world.size = math::Vec2{800.0f, 600.0f};
        world.rng.seed(1);
        game::start_game(world, std::move(levels));
        game::step(world, launch_input(), DT);
        return world;
    }

    // Every beam there has been, however many have gone since.
    struct BeamLog
    {
        std::set<std::uint32_t> seen;

        void look(const game::World& world)
        {
            for (const core::Entity beam : world.beams.entities())
            {
                seen.insert(beam.id);
            }
        }
    };
}

// ---------------------------------------------------------------- the capsule

TEST(LaserCapsule, CatchingItGivesTheLasersForTenSecondsAndNothingElse)
{
    ASSERT_FLOAT_EQ(game::LASER_TIME, 10.0f);

    game::World world = playing_world();
    const core::Entity ball = add_ball(world, math::Vec2{700.0f, 300.0f}, math::Vec2{0.0f, 100.0f});

    catch_capsule(world, game::PowerUp::Laser);

    EXPECT_FLOAT_EQ(paddle_state(world).laser_time_left, 10.0f) << "the catch's own step took time off";
    EXPECT_FLOAT_EQ(world.transforms.find(paddle_of(world))->size.x, game::PADDLE_WIDTH);
    EXPECT_FLOAT_EQ(paddle_state(world).wide_time_left, 0.0f);
    EXPECT_EQ(world.balls.size(), 1u);
    EXPECT_TRUE(world.balls.has(ball));
    EXPECT_EQ(world.capsules.size(), 0u);
    EXPECT_EQ(world.beams.size(), 0u) << "nothing is fired without Space";
    EXPECT_EQ(world.score, 0);
}

TEST(LaserCapsule, OtherCapsulesGiveNoLasers)
{
    for (const game::PowerUp kind : {game::PowerUp::WidePaddle, game::PowerUp::MultiBall})
    {
        game::World world = playing_world();
        add_ball(world, math::Vec2{700.0f, 300.0f}, math::Vec2{0.0f, 100.0f});

        catch_capsule(world, kind);
        press_space(world);

        EXPECT_FLOAT_EQ(paddle_state(world).laser_time_left, 0.0f);
        EXPECT_EQ(world.beams.size(), 0u);
    }
}

// ---------------------------------------------------------------- firing

TEST(LaserFire, SpaceFiresAPairFromJustInsideEachEndOfThePaddle)
{
    game::World world = playing_world();
    catch_capsule(world, game::PowerUp::Laser);

    press_space(world);

    ASSERT_EQ(world.beams.size(), 2u);
    const float paddle_top = game::PADDLE_Y + game::PADDLE_HEIGHT * 0.5f;
    std::vector<float> xs;
    for (const core::Entity beam : world.beams.entities())
    {
        const game::Transform& transform = *world.transforms.find(beam);
        xs.push_back(transform.position.x);

        EXPECT_FLOAT_EQ(transform.size.x, 4.0f);
        EXPECT_FLOAT_EQ(transform.size.y, 16.0f);
        EXPECT_FLOAT_EQ(transform.position.y - transform.size.y * 0.5f, paddle_top) << "its bottom on the paddle's top";
        EXPECT_FLOAT_EQ(transform.previous_position.x, transform.position.x) << "would be drawn sliding in";
        EXPECT_FLOAT_EQ(transform.previous_position.y, transform.position.y) << "would be drawn sliding in";

        const math::Vec2 velocity = world.velocities.find(beam)->value;
        EXPECT_FLOAT_EQ(velocity.x, 0.0f);
        EXPECT_FLOAT_EQ(velocity.y, 600.0f);

        const core::Color color = *world.colors.find(beam);
        const core::Color red = game::capsule_color(game::PowerUp::Laser);
        EXPECT_TRUE(color.r == red.r && color.g == red.g && color.b == red.b);
    }
    std::sort(xs.begin(), xs.end());
    EXPECT_FLOAT_EQ(xs[0], LEFT_BEAM_X);
    EXPECT_FLOAT_EQ(xs[1], RIGHT_BEAM_X);
}

TEST(LaserFire, WithoutLasersSpaceFiresNothing)
{
    game::World world = playing_world();

    run_steps(world, 60, launch_input());

    EXPECT_EQ(world.beams.size(), 0u);
    EXPECT_EQ(world.state, game::GameState::Playing);
}

TEST(LaserFire, AWidePaddleFiresFromItsWiderEnds)
{
    game::World world = playing_world();
    catch_capsule(world, game::PowerUp::WidePaddle);
    catch_capsule(world, game::PowerUp::Laser);

    press_space(world);

    ASSERT_EQ(world.beams.size(), 2u);
    std::vector<float> xs;
    for (const core::Entity beam : world.beams.entities())
    {
        xs.push_back(world.transforms.find(beam)->position.x);
    }
    std::sort(xs.begin(), xs.end());
    const float offset = game::WIDE_PADDLE_WIDTH * 0.5f - game::BEAM_INSET;
    EXPECT_FLOAT_EQ(xs[0], PADDLE_X - offset);
    EXPECT_FLOAT_EQ(xs[1], PADDLE_X + offset);
}

// 0.3 seconds is 18 steps: a press 17 steps after a pair fires nothing, one
// 18 steps after fires the next.
TEST(LaserFire, AtMostOnePairEveryPointThreeSeconds)
{
    ASSERT_FLOAT_EQ(game::LASER_FIRE_INTERVAL, 0.3f);

    game::World world = playing_world();
    catch_capsule(world, game::PowerUp::Laser);
    press_space(world);
    ASSERT_EQ(world.beams.size(), 2u);

    run_steps(world, 16);
    press_space(world);
    EXPECT_EQ(world.beams.size(), 2u) << "fired again after 17 steps";

    press_space(world);
    EXPECT_EQ(world.beams.size(), 4u) << "did not fire after 18 steps";
}

// Space held for two seconds: a pair at 0, 0.3, 0.6 ... 1.8 seconds.
TEST(LaserFire, HoldingSpaceFiresAPairEveryPointThreeSeconds)
{
    game::World world = playing_world();
    catch_capsule(world, game::PowerUp::Laser);

    BeamLog log;
    for (int i = 0; i < 120; i++)
    {
        press_space(world);
        log.look(world);
    }

    EXPECT_EQ(log.seen.size(), 14u);
}

// ---------------------------------------------------------------- how long

TEST(Laser, LastsTenSeconds)
{
    game::World world = playing_world();
    catch_capsule(world, game::PowerUp::Laser);

    run_steps(world, 597);
    press_space(world);
    EXPECT_EQ(world.beams.size(), 2u) << "gone before 10 seconds";

    run_steps(world, 4);
    EXPECT_FLOAT_EQ(paddle_state(world).laser_time_left, 0.0f) << "still on after 10 seconds";

    BeamLog log;
    log.look(world);
    run_steps(world, 60, launch_input());
    log.look(world);
    EXPECT_EQ(log.seen.size(), 2u) << "fired after 10 seconds";
}

TEST(Laser, ASecondCatchRestartsTheTime)
{
    game::World world = playing_world();
    catch_capsule(world, game::PowerUp::Laser);
    run_steps(world, 360);

    catch_capsule(world, game::PowerUp::Laser);
    EXPECT_FLOAT_EQ(paddle_state(world).laser_time_left, 10.0f);

    // Past the first catch's 10 seconds, inside the second's.
    run_steps(world, 360);
    EXPECT_GT(paddle_state(world).laser_time_left, 0.0f);

    run_steps(world, 240);
    EXPECT_FLOAT_EQ(paddle_state(world).laser_time_left, 0.0f);
}

TEST(Laser, TheTimeAndTheBeamsStopWhilePaused)
{
    game::World world = playing_world();
    catch_capsule(world, game::PowerUp::Laser);
    press_space(world);
    run_steps(world, 5);

    game::step(world, pause_input(), DT);
    ASSERT_EQ(world.state, game::GameState::Paused);
    const float left_when_paused = paddle_state(world).laser_time_left;
    const std::vector<core::Entity> beams = world.beams.entities();
    ASSERT_EQ(beams.size(), 2u);
    const float beam_y = world.transforms.find(beams[0])->position.y;

    // Twenty seconds of pause with Space held.
    run_steps(world, 1200, launch_input());

    EXPECT_FLOAT_EQ(paddle_state(world).laser_time_left, left_when_paused);
    EXPECT_EQ(world.beams.entities(), beams) << "fired or lost beams while paused";
    EXPECT_FLOAT_EQ(world.transforms.find(beams[0])->position.y, beam_y);
}

TEST(Laser, BeamsInFlightWhenItRunsOutFlyOn)
{
    game::World world = playing_world();
    catch_capsule(world, game::PowerUp::Laser);
    run_steps(world, 597);
    press_space(world);
    const std::vector<core::Entity> beams = world.beams.entities();
    ASSERT_EQ(beams.size(), 2u);
    const float beam_y = world.transforms.find(beams[0])->position.y;

    run_steps(world, 4);

    ASSERT_FLOAT_EQ(paddle_state(world).laser_time_left, 0.0f);
    EXPECT_EQ(world.beams.entities(), beams);
    EXPECT_NEAR(world.transforms.find(beams[0])->position.y, beam_y + 4.0f * game::BEAM_SPEED * DT, TOLERANCE);
}

// ---------------------------------------------------------------- beams

TEST(Beam, GoesStraightUpAtSixHundredUnitsASecond)
{
    ASSERT_FLOAT_EQ(game::BEAM_SPEED, 600.0f);

    game::World world = playing_world();
    const core::Entity beam = game::spawn_beam(world, math::Vec2{300.0f, 100.0f});

    run_steps(world, 30);

    const game::Transform& transform = *world.transforms.find(beam);
    EXPECT_FLOAT_EQ(transform.position.x, 300.0f);
    EXPECT_NEAR(transform.position.y, 400.0f, TOLERANCE);
}

TEST(Beam, IsGoneOnceAllOfItIsAboveThePlayArea)
{
    game::World world = playing_world();

    // Its bottom at 587: after one step 597, still inside; after two, 607.
    const core::Entity beam = game::spawn_beam(world, math::Vec2{300.0f, 595.0f});

    game::step(world, game::Input{}, DT);
    EXPECT_TRUE(world.beams.has(beam));

    game::step(world, game::Input{}, DT);
    EXPECT_FALSE(world.beams.has(beam));
    EXPECT_FALSE(world.transforms.has(beam));
    EXPECT_TRUE(world.events.empty());
}

TEST(Beam, BreaksAOneHitBrickAsTheBallWouldAndIsUsedUp)
{
    game::World world = playing_world();
    world.capsule_drop_chance = 0.0f;
    const core::Entity brick = add_brick(world, math::Vec2{300.0f, 300.0f});
    const core::Entity beam = beam_under(world, 300.0f, 300.0f);

    game::step(world, game::Input{}, DT);

    EXPECT_FALSE(world.bricks.has(brick));
    EXPECT_FALSE(world.beams.has(beam));
    EXPECT_EQ(world.score, 10);
    EXPECT_GT(world.particles.size(), 0u);
    EXPECT_EQ(count(world, game::GameEvent::LaserHit), 1);
    EXPECT_EQ(count(world, game::GameEvent::BrickHit), 0) << "the ball's event pauses and shakes the game";
    EXPECT_EQ(world.rng_draws, 1u) << "whether it drops a capsule is drawn as for the ball";
}

TEST(Beam, ABrickItBreaksCanDropACapsuleWhereItWas)
{
    game::World world = playing_world();
    world.capsule_drop_chance = 1.0f;
    add_brick(world, math::Vec2{300.0f, 300.0f});
    beam_under(world, 300.0f, 300.0f);

    game::step(world, game::Input{}, DT);

    ASSERT_EQ(world.capsules.size(), 1u);
    const math::Vec2 at = world.transforms.find(world.capsules.entities()[0])->position;
    EXPECT_FLOAT_EQ(at.x, 300.0f);
    EXPECT_FLOAT_EQ(at.y, 300.0f);
    EXPECT_EQ(world.rng_draws, 2u) << "whether, then which kind";
}

TEST(Beam, TakesOneHitPointOffAToughBrickAndRecoloursIt)
{
    game::World world = playing_world();
    world.capsule_drop_chance = 1.0f;
    const core::Entity brick = add_brick(world, math::Vec2{300.0f, 300.0f}, 3);
    const core::Entity beam = beam_under(world, 300.0f, 300.0f);

    game::step(world, game::Input{}, DT);

    ASSERT_TRUE(world.bricks.has(brick));
    EXPECT_EQ(world.bricks.find(brick)->hit_points, 2);
    const core::Color color = *world.colors.find(brick);
    const core::Color two = game::brick_color(2);
    EXPECT_TRUE(color.r == two.r && color.g == two.g && color.b == two.b);
    EXPECT_FALSE(world.beams.has(beam)) << "a beam does one hit";
    EXPECT_EQ(world.score, 10);
    EXPECT_EQ(world.capsules.size(), 0u) << "only the hit that breaks a brick drops";
}

// Beams break bricks but touch no ball: no speed-up and, in a level with
// steel, no restart of the stuck-ball clock.
TEST(Beam, ItsHitsLeaveTheBallAlone)
{
    game::World world = playing_world();
    add_steel(world, math::Vec2{700.0f, 500.0f});
    const core::Entity ball = add_ball(world, math::Vec2{600.0f, 200.0f}, math::Vec2{0.0f, 100.0f});
    world.balls.find(ball)->time_since_touch = 5.0f;
    add_brick(world, math::Vec2{300.0f, 300.0f});
    beam_under(world, 300.0f, 300.0f);

    game::step(world, game::Input{}, DT);

    ASSERT_EQ(count(world, game::GameEvent::LaserHit), 1);
    EXPECT_FLOAT_EQ(world.velocities.find(ball)->value.y, 100.0f);
    EXPECT_NEAR(world.balls.find(ball)->time_since_touch, 5.0f + DT, TOLERANCE);
}

TEST(Beam, TwoBeamsOnOneToughBrickTakeAHitEach)
{
    game::World world = playing_world();
    const core::Entity brick = add_brick(world, math::Vec2{PADDLE_X, 300.0f}, 2, math::Vec2{300.0f, 24.0f});
    catch_capsule(world, game::PowerUp::Laser);
    press_space(world);

    run_steps(world, 60);

    EXPECT_FALSE(world.bricks.has(brick));
    EXPECT_EQ(world.score, 20);
    EXPECT_EQ(world.beams.size(), 0u);
}

// Both beams of a pair reach a wide brick in the same step. The first breaks
// it; the second finds it gone and carries on to the brick above. The brick
// above the first beam is left alone.
TEST(Beam, WhenTheFirstOfTwoBreaksABrickTheSecondGoesOn)
{
    game::World world = playing_world();
    world.capsule_drop_chance = 0.0f;
    const core::Entity wide = add_brick(world, math::Vec2{PADDLE_X, 300.0f}, 1, math::Vec2{300.0f, 24.0f});
    const core::Entity above_left = add_brick(world, math::Vec2{LEFT_BEAM_X, 400.0f});
    const core::Entity above_right = add_brick(world, math::Vec2{RIGHT_BEAM_X, 400.0f});
    catch_capsule(world, game::PowerUp::Laser);
    press_space(world);

    run_steps(world, 60);

    EXPECT_FALSE(world.bricks.has(wide));
    EXPECT_EQ(world.bricks.has(above_left), !world.bricks.has(above_right)) << "exactly one beam went on";
    EXPECT_EQ(world.score, 20);
    EXPECT_EQ(world.beams.size(), 0u);
}

TEST(Beam, StopsAtSteelWithTheSteelSoundAndDoesNothingElse)
{
    game::World world = playing_world();
    world.capsule_drop_chance = 1.0f;
    const core::Entity steel = add_steel(world, math::Vec2{300.0f, 300.0f});
    const core::Entity above = add_brick(world, math::Vec2{300.0f, 400.0f});
    const core::Entity beam = beam_under(world, 300.0f, 300.0f);

    run_steps(world, 30);

    EXPECT_FALSE(world.beams.has(beam));
    EXPECT_TRUE(world.steel.has(steel));
    EXPECT_TRUE(world.bricks.has(above)) << "went through the steel";
    EXPECT_EQ(world.score, 0);
    EXPECT_EQ(count(world, game::GameEvent::SteelHit), 1);
    EXPECT_EQ(count(world, game::GameEvent::LaserHit), 0);
    EXPECT_EQ(world.particles.size(), 0u);
    EXPECT_EQ(world.capsules.size(), 0u);
    EXPECT_EQ(world.rng_draws, 0u);
}

TEST(Beam, PassesThroughBallsAndCapsules)
{
    game::World world = playing_world();
    const core::Entity ball = add_ball(world, math::Vec2{300.0f, 200.0f}, math::Vec2{0.0f, 0.0f});
    const core::Entity capsule = game::spawn_capsule(world, math::Vec2{300.0f, 260.0f}, game::PowerUp::MultiBall);
    const core::Entity beam = game::spawn_beam(world, math::Vec2{300.0f, 170.0f});

    run_steps(world, 15);

    EXPECT_TRUE(world.beams.has(beam));
    EXPECT_NEAR(world.transforms.find(beam)->position.y, 170.0f + 150.0f, TOLERANCE);
    EXPECT_TRUE(world.balls.has(ball));
    EXPECT_FLOAT_EQ(world.velocities.find(ball)->value.x, 0.0f);
    EXPECT_FLOAT_EQ(world.velocities.find(ball)->value.y, 0.0f);
    EXPECT_TRUE(world.capsules.has(capsule));
    EXPECT_EQ(world.balls.size(), 1u) << "the capsule was used";
    EXPECT_TRUE(world.events.empty());
}

TEST(Beam, BreakingTheLastBrickClearsTheLevel)
{
    game::World world = started_world({game::parse_level("1"), game::parse_level("11")});
    catch_capsule(world, game::PowerUp::Laser);
    press_space(world);

    for (int i = 0; i < 120 && world.level_index == 0; i++)
    {
        game::step(world, game::Input{}, DT);
    }

    EXPECT_EQ(world.level_index, 1);
    EXPECT_EQ(world.score, 10);
    EXPECT_EQ(count(world, game::GameEvent::LaserHit), 1);
    EXPECT_EQ(count(world, game::GameEvent::BrickHit), 0) << "the ball got there first";
}

// ---------------------------------------------------------------- what ends it

TEST(Laser, LosingALifeTurnsItOffAndClearsTheBeams)
{
    game::World world = playing_world();
    const core::Entity ball = add_ball(world, math::Vec2{700.0f, 300.0f}, math::Vec2{0.0f, 100.0f});
    catch_capsule(world, game::PowerUp::Laser);
    press_space(world);
    ASSERT_EQ(world.beams.size(), 2u);

    world.transforms.find(ball)->position = math::Vec2{700.0f, -100.0f};
    game::step(world, game::Input{}, DT);

    ASSERT_EQ(world.state, game::GameState::Ready);
    EXPECT_EQ(world.lives, game::STARTING_LIVES - 1);
    EXPECT_EQ(world.beams.size(), 0u);
    EXPECT_FLOAT_EQ(paddle_state(world).laser_time_left, 0.0f);
    EXPECT_FLOAT_EQ(paddle_state(world).laser_cooldown, 0.0f);

    // Space serves the ball and fires nothing.
    press_space(world);
    EXPECT_EQ(world.state, game::GameState::Playing);
    EXPECT_EQ(world.beams.size(), 0u);
}

TEST(Laser, AStuckBallGoingBackOnThePaddleTurnsItOff)
{
    game::World world = started_world({game::parse_level("1#")});
    catch_capsule(world, game::PowerUp::Laser);
    press_space(world);
    ASSERT_EQ(world.beams.size(), 2u);

    world.balls.find(world.balls.entities()[0])->time_since_touch = game::STUCK_BALL_TIME;
    game::step(world, game::Input{}, DT);

    ASSERT_EQ(world.state, game::GameState::Ready);
    EXPECT_EQ(world.lives, game::STARTING_LIVES);
    EXPECT_EQ(world.beams.size(), 0u);
    EXPECT_FLOAT_EQ(paddle_state(world).laser_time_left, 0.0f);
}

TEST(Laser, TheNextLevelTurnsItOffAndClearsTheBeams)
{
    game::World world = started_world({game::parse_level("1"), game::parse_level("11")});
    catch_capsule(world, game::PowerUp::Laser);
    press_space(world);
    ASSERT_EQ(world.beams.size(), 2u);

    const std::vector<core::Entity> bricks = world.bricks.entities();
    for (const core::Entity brick : bricks)
    {
        world.destroy_entity(brick);
    }
    game::step(world, game::Input{}, DT);

    ASSERT_EQ(world.level_index, 1);
    EXPECT_EQ(world.beams.size(), 0u);
    EXPECT_FLOAT_EQ(paddle_state(world).laser_time_left, 0.0f);
}

TEST(Laser, AWinClearsTheBeamsAndRStartsWithoutLasers)
{
    game::World world = started_world({game::parse_level("1")});
    catch_capsule(world, game::PowerUp::Laser);
    press_space(world);
    ASSERT_EQ(world.beams.size(), 2u);

    const std::vector<core::Entity> bricks = world.bricks.entities();
    for (const core::Entity brick : bricks)
    {
        world.destroy_entity(brick);
    }
    game::step(world, game::Input{}, DT);
    ASSERT_EQ(world.state, game::GameState::Won);
    EXPECT_EQ(world.beams.size(), 0u);

    game::Input restart;
    restart.restart = true;
    game::step(world, restart, DT);

    ASSERT_EQ(world.state, game::GameState::Ready);
    EXPECT_FLOAT_EQ(paddle_state(world).laser_time_left, 0.0f);
    EXPECT_EQ(world.beams.size(), 0u);
}
