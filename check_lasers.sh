#!/bin/bash
# check_lasers.sh - end-to-end check of the laser power-up.
#
# Run from the repo root:   bash check_lasers.sh
#
# Needs only g++ with C++20. It compiles the game sources (src/game/*.cpp), the
# breakout_replay program (src/breakout_replay.cpp) and a small check program
# into a temp folder. The check program plays worlds through game::step with
# the real paddle, capsules and beams, and runs breakout_replay on the laser
# replay in tests/replays. It prints one line per check. Nothing is written
# inside the repo, and the temp folder is removed at the end.
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
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#include "game/Replay.h"
#include "game/Spawn.h"
#include "game/Systems.h"

namespace fs = std::filesystem;

namespace
{
    constexpr int TOTAL = 14;
    constexpr float DT = game::FIXED_DT;
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
        std::fflush(stdout);
    }

    std::string num(float value)
    {
        char buffer[32];
        std::snprintf(buffer, sizeof buffer, "%.2f", static_cast<double>(value));
        return buffer;
    }

    const char* yes(bool value)
    {
        return value ? "yes" : "no";
    }

    std::string read(const fs::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        return std::string{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }

    // Playing, with the real paddle in the middle at the bottom and a tough
    // brick in the top left corner, out of every beam's way, so the level is
    // never cleared. No ball, so no life can be lost unless a check adds one.
    game::World playing_world()
    {
        game::World world;
        world.size = game::WORLD_SIZE;
        world.state = game::GameState::Playing;
        world.seed_rng(1);
        game::spawn_paddle(world);
        const core::Entity corner = world.create_entity();
        const math::Vec2 at{40.0f, 580.0f};
        world.transforms.add(corner, game::Transform{at, math::Vec2{60.0f, 24.0f}, at});
        world.colors.add(corner, game::brick_color(9));
        world.bricks.add(corner, game::Brick{9});
        return world;
    }

    core::Entity add_brick(game::World& world, math::Vec2 at, int hit_points, float width = 60.0f)
    {
        const core::Entity entity = world.create_entity();
        world.transforms.add(entity, game::Transform{at, math::Vec2{width, 24.0f}, at});
        world.colors.add(entity, game::brick_color(hit_points));
        world.bricks.add(entity, game::Brick{hit_points});
        return entity;
    }

    core::Entity add_steel(game::World& world, math::Vec2 at)
    {
        const core::Entity entity = world.create_entity();
        world.transforms.add(entity, game::Transform{at, math::Vec2{60.0f, 24.0f}, at});
        world.colors.add(entity, game::brick_color(game::STEEL));
        world.steel.add(entity, game::Steel{});
        return entity;
    }

    core::Entity add_ball(game::World& world, math::Vec2 at, math::Vec2 velocity)
    {
        const core::Entity entity = world.create_entity();
        world.transforms.add(entity, game::Transform{at, math::Vec2{16.0f, 16.0f}, at});
        world.velocities.add(entity, game::Velocity{velocity});
        world.balls.add(entity, game::Ball{8.0f});
        return entity;
    }

    core::Entity paddle_of(const game::World& world)
    {
        return world.paddles.entities()[0];
    }

    const game::Paddle& paddle(const game::World& world)
    {
        return *world.paddles.find(paddle_of(world));
    }

    float paddle_x(const game::World& world)
    {
        return world.transforms.find(paddle_of(world))->position.x;
    }

    game::Input space()
    {
        game::Input input;
        input.launch = true;
        return input;
    }

    game::Input pause_key()
    {
        game::Input input;
        input.toggle_pause = true;
        return input;
    }

    // Every event of every step, so a check can count them over many steps.
    std::vector<game::GameEvent> events;

    void step(game::World& world, const game::Input& input = game::Input{})
    {
        game::step(world, input, DT);
        events.insert(events.end(), world.events.begin(), world.events.end());
        world.events.clear();
    }

    void steps(game::World& world, int count, const game::Input& input = game::Input{})
    {
        for (int i = 0; i < count; ++i)
        {
            step(world, input);
        }
    }

    int count_events(game::GameEvent event)
    {
        return static_cast<int>(std::count(events.begin(), events.end(), event));
    }

    // A capsule dropped right onto the paddle, and one step to catch it.
    void catch_capsule(game::World& world, game::PowerUp kind)
    {
        game::spawn_capsule(world, world.transforms.find(paddle_of(world))->position, kind);
        step(world);
    }

    std::vector<float> beam_xs(const game::World& world)
    {
        std::vector<float> xs;
        for (const core::Entity beam : world.beams.entities())
        {
            xs.push_back(world.transforms.find(beam)->position.x);
        }
        std::sort(xs.begin(), xs.end());
        return xs;
    }

    std::string xs_text(const std::vector<float>& xs)
    {
        std::string text;
        for (const float x : xs)
        {
            text += (text.empty() ? "" : " ") + num(x);
        }
        return text.empty() ? "none" : text;
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

    float distance(core::Color a, core::Color b)
    {
        return std::sqrt((a.r - b.r) * (a.r - b.r) + (a.g - b.g) * (a.g - b.g) + (a.b - b.b) * (a.b - b.b));
    }

    float speed_of(const game::World& world, core::Entity ball)
    {
        const math::Vec2 v = world.velocities.find(ball)->value;
        return std::sqrt(v.x * v.x + v.y * v.y);
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::printf("usage: check <breakout_replay program>\n");
        return 2;
    }
    const std::string program = argv[1];
    const float half_paddle = game::PADDLE_WIDTH * 0.5f;

    // 1
    {
        game::World world = playing_world();
        const core::Entity ball = add_ball(world, math::Vec2{700.0f, 300.0f}, math::Vec2{0.0f, 0.0f});
        catch_capsule(world, game::PowerUp::Laser);
        const game::Paddle& p = paddle(world);
        const float width = world.transforms.find(paddle_of(world))->size.x;
        const bool laser_on = std::abs(p.laser_time_left - (game::LASER_TIME - DT)) < 0.02f ||
                              std::abs(p.laser_time_left - game::LASER_TIME) < 0.02f;

        bool others_give_none = true;
        for (const game::PowerUp kind : {game::PowerUp::WidePaddle, game::PowerUp::MultiBall})
        {
            game::World other = playing_world();
            add_ball(other, math::Vec2{700.0f, 300.0f}, math::Vec2{0.0f, 0.0f});
            catch_capsule(other, kind);
            others_give_none = others_give_none && paddle(other).laser_time_left == 0.0f;
        }

        report(laser_on && width == game::PADDLE_WIDTH && world.balls.size() == 1 && world.capsules.size() == 0 &&
                   world.balls.has(ball) && others_give_none,
               "1   catching an L capsule gives 10 s of lasers and nothing else; W and M give no lasers",
               "laser time " + num(p.laser_time_left) + ", paddle width " + num(width) + ", balls " +
                   std::to_string(world.balls.size()) + ", W/M give none " + yes(others_give_none));
    }

    // 2
    {
        const core::Color laser = game::capsule_color(game::PowerUp::Laser);
        float closest = 99.0f;
        std::string closest_to;
        for (int hit_points = 1; hit_points <= 9; ++hit_points)
        {
            const float d = distance(laser, game::brick_color(hit_points));
            if (d < closest)
            {
                closest = d;
                closest_to = std::to_string(hit_points) + "-hit brick";
            }
        }
        for (const auto& [name, color] : {std::pair{"steel", game::brick_color(game::STEEL)},
                                          std::pair{"W capsule", game::capsule_color(game::PowerUp::WidePaddle)},
                                          std::pair{"M capsule", game::capsule_color(game::PowerUp::MultiBall)}})
        {
            const float d = distance(laser, color);
            if (d < closest)
            {
                closest = d;
                closest_to = name;
            }
        }

        game::World world = playing_world();
        catch_capsule(world, game::PowerUp::Laser);
        step(world, space());
        bool beams_red = world.beams.size() == 2;
        for (const core::Entity beam : world.beams.entities())
        {
            const core::Color* c = world.colors.find(beam);
            beams_red = beams_red && c != nullptr && c->r == laser.r && c->g == laser.g && c->b == laser.b;
        }

        const bool red = laser.r >= 0.6f && laser.g <= 0.2f && laser.b <= 0.2f;
        report(red && closest > 0.4f && beams_red,
               "2   the L capsule is red, clearly unlike every brick and the other capsules, and beams are its red",
               "colour " + num(laser.r) + " " + num(laser.g) + " " + num(laser.b) + ", closest is the " + closest_to +
                   " at " + num(closest) + ", beams red " + yes(beams_red));
    }

    // 3
    {
        game::World world = playing_world();
        world.capsule_drop_chance = 1.0f;
        int kinds[3] = {0, 0, 0};
        int drops = 0;
        bool two_draws_each = true;
        std::set<std::uint32_t> seen;
        for (int i = 0; i < 3000; ++i)
        {
            add_brick(world, math::Vec2{400.0f, 300.0f}, 1);
            game::spawn_beam(world, math::Vec2{400.0f, 300.0f - 12.0f - game::BEAM_HEIGHT * 0.5f - 5.0f});
            const std::uint64_t before = world.rng_draws;
            step(world);
            two_draws_each = two_draws_each && world.rng_draws - before == 2;
            for (const core::Entity capsule : world.capsules.entities())
            {
                if (seen.insert(capsule.id).second)
                {
                    ++drops;
                    ++kinds[static_cast<int>(world.capsules.find(capsule)->kind)];
                }
            }
        }
        bool thirds = drops == 3000;
        for (const int k : kinds)
        {
            thirds = thirds && k >= 900 && k <= 1100;
        }
        report(thirds && two_draws_each,
               "3   over 3000 drops the kinds come out about a third each, and a drop still draws 2 numbers",
               "drops " + std::to_string(drops) + ": W " + std::to_string(kinds[0]) + ", M " +
                   std::to_string(kinds[1]) + ", L " + std::to_string(kinds[2]) + "; 2 draws per drop " +
                   yes(two_draws_each));
    }

    // 4
    {
        game::World world = playing_world();
        catch_capsule(world, game::PowerUp::Laser);
        step(world, space());
        const std::vector<float> xs = beam_xs(world);
        const float x = paddle_x(world);
        const bool from_ends = xs.size() == 2 && std::abs(xs[0] - (x - half_paddle + game::BEAM_INSET)) < 0.01f &&
                               std::abs(xs[1] - (x + half_paddle - game::BEAM_INSET)) < 0.01f;
        float y0 = 0.0f;
        if (!world.beams.entities().empty())
        {
            y0 = world.transforms.find(world.beams.entities()[0])->position.y;
        }
        step(world);
        float y1 = y0;
        if (!world.beams.entities().empty())
        {
            y1 = world.transforms.find(world.beams.entities()[0])->position.y;
        }
        const float speed = (y1 - y0) / DT;

        game::World without = playing_world();
        steps(without, 60, space());
        report(from_ends && std::abs(speed - game::BEAM_SPEED) < 1.0f && without.beams.size() == 0,
               "4   Space fires a pair from just inside each end of the paddle, going up at 600/s; without lasers nothing",
               "beams at " + xs_text(xs) + " for a paddle at " + num(x) + ", speed " + num(speed) +
                   ", beams without lasers " + std::to_string(without.beams.size()));
    }

    // 5
    {
        game::World world = playing_world();
        catch_capsule(world, game::PowerUp::Laser);
        BeamLog held;
        for (int i = 0; i < 60; ++i)
        {
            step(world, space());
            held.look(world);
        }

        game::World taps = playing_world();
        catch_capsule(taps, game::PowerUp::Laser);
        step(taps, space());
        const std::size_t first = taps.beams.size();
        steps(taps, 15);
        step(taps, space());
        const std::size_t too_soon = taps.beams.size();
        steps(taps, 3);
        step(taps, space());
        const std::size_t after = taps.beams.size();

        report(held.seen.size() == 8 && first == 2 && too_soon == 2 && after == 4,
               "5   at most one pair every 0.3 s: Space on every step for 1 s gives 4 pairs, a press 0.27 s after a pair none",
               "beams over 1 s of Space " + std::to_string(held.seen.size()) + " (want 8); beams after 1st press " +
                   std::to_string(first) + ", after a press 0.27 s later " + std::to_string(too_soon) +
                   ", after one 0.33 s later " + std::to_string(after));
    }

    // 6
    {
        events.clear();
        game::World world = playing_world();
        const core::Entity ball = add_ball(world, math::Vec2{700.0f, 300.0f}, math::Vec2{0.0f, 200.0f});
        catch_capsule(world, game::PowerUp::Laser);
        const float x = paddle_x(world);
        const core::Entity brick = add_brick(world, math::Vec2{x - half_paddle + game::BEAM_INSET, 300.0f}, 1);
        const int score_before = world.score;
        const float ball_speed = speed_of(world, ball);
        step(world, space());
        steps(world, 40);
        // Lets the ball keep moving straight up and down, out of the way.
        const bool brick_gone = !world.bricks.has(brick);
        report(brick_gone && world.score == score_before + 10 && count_events(game::GameEvent::LaserHit) == 1 &&
                   count_events(game::GameEvent::BrickHit) == 0 && world.beams.size() == 1 &&
                   std::abs(speed_of(world, ball) - ball_speed) < 0.01f,
               "6   a beam breaks a 1-hit brick: +10, one LaserHit and no BrickHit, the beam is used up, the ball no faster",
               std::string("brick gone ") + yes(brick_gone) + ", score +" + std::to_string(world.score - score_before) +
                   ", LaserHit " + std::to_string(count_events(game::GameEvent::LaserHit)) + ", BrickHit " +
                   std::to_string(count_events(game::GameEvent::BrickHit)) + ", beams left " +
                   std::to_string(world.beams.size()) + " (the other one), ball speed " + num(ball_speed) + " -> " +
                   num(speed_of(world, ball)));
    }

    // 7
    {
        game::World world = playing_world();
        catch_capsule(world, game::PowerUp::Laser);
        const float x = paddle_x(world);
        const core::Entity brick = add_brick(world, math::Vec2{x - half_paddle + game::BEAM_INSET, 300.0f}, 3);
        step(world, space());
        steps(world, 40);
        const game::Brick* b = world.bricks.find(brick);
        const core::Color* c = world.colors.find(brick);
        const core::Color two = game::brick_color(2);
        const bool recoloured = c != nullptr && c->r == two.r && c->g == two.g && c->b == two.b;
        report(b != nullptr && b->hit_points == 2 && recoloured && world.beams.size() == 1,
               "7   a beam takes one hit off a 3-hit brick and is used up; the brick turns the 2-hit colour",
               "hit points left " + (b ? std::to_string(b->hit_points) : std::string("brick gone")) +
                   ", recoloured " + yes(recoloured) + ", beams left " + std::to_string(world.beams.size()));
    }

    // 8
    {
        events.clear();
        game::World world = playing_world();
        catch_capsule(world, game::PowerUp::Laser);
        const float x = paddle_x(world);
        const core::Entity low = add_brick(world, math::Vec2{x, 300.0f}, 1, 140.0f);
        const core::Entity high = add_brick(world, math::Vec2{x, 400.0f}, 1, 140.0f);
        step(world, space());
        steps(world, 60);
        report(!world.bricks.has(low) && !world.bricks.has(high) && count_events(game::GameEvent::LaserHit) == 2 &&
                   world.score == 20,
               "8   both beams of a pair under one wide brick: the first breaks it, the second goes on to the next",
               std::string("low brick gone ") + yes(!world.bricks.has(low)) + ", high brick gone " + yes(!world.bricks.has(high)) +
                   ", LaserHit " + std::to_string(count_events(game::GameEvent::LaserHit)) + ", score " +
                   std::to_string(world.score));
    }

    // 9
    {
        events.clear();
        game::World world = playing_world();
        catch_capsule(world, game::PowerUp::Laser);
        const float bx = paddle_x(world) - half_paddle + game::BEAM_INSET;
        const core::Entity steel = add_steel(world, math::Vec2{bx, 300.0f});
        const core::Entity behind = add_brick(world, math::Vec2{bx, 400.0f}, 1);
        step(world, space());
        steps(world, 60);
        report(world.steel.has(steel) && world.bricks.has(behind) && world.score == 0 &&
                   count_events(game::GameEvent::SteelHit) == 1 && count_events(game::GameEvent::LaserHit) == 0,
               "9   steel stops a beam with a SteelHit: the steel stays, the brick behind it is untouched, no points",
               std::string("steel there ") + yes(world.steel.has(steel)) + ", brick behind there " + yes(world.bricks.has(behind)) +
                   ", score " + std::to_string(world.score) + ", SteelHit " +
                   std::to_string(count_events(game::GameEvent::SteelHit)) + ", LaserHit " +
                   std::to_string(count_events(game::GameEvent::LaserHit)));
    }

    // 10
    {
        events.clear();
        game::World world = playing_world();
        catch_capsule(world, game::PowerUp::Laser);
        const float bx = paddle_x(world) - half_paddle + game::BEAM_INSET;
        const core::Entity ball = add_ball(world, math::Vec2{bx, 200.0f}, math::Vec2{0.0f, 0.0f});
        const core::Entity capsule = game::spawn_capsule(world, math::Vec2{bx + 2.0f * half_paddle - 2.0f * game::BEAM_INSET, 300.0f},
                                                         game::PowerUp::MultiBall);
        BeamLog log;
        step(world, space());
        log.look(world);
        int top_reached = 0;
        for (int i = 0; i < 30; ++i)
        {
            step(world);
            for (const core::Entity beam : world.beams.entities())
            {
                top_reached = std::max(top_reached, static_cast<int>(world.transforms.find(beam)->position.y));
            }
        }
        const math::Vec2 v = world.velocities.find(ball)->value;
        report(world.balls.has(ball) && world.capsules.has(capsule) && v.x == 0.0f && v.y == 0.0f && top_reached > 320 &&
                   events.empty(),
               "10  beams fly straight through a ball and a capsule, changing neither, with no event",
               std::string("ball there ") + yes(world.balls.has(ball)) + ", capsule there " + yes(world.capsules.has(capsule)) +
                   ", ball velocity " + num(v.x) + " " + num(v.y) + ", highest beam y " + std::to_string(top_reached) +
                   ", events " + std::to_string(events.size()));
    }

    // 11
    {
        game::World world = playing_world();
        catch_capsule(world, game::PowerUp::Laser);
        steps(world, 300);
        catch_capsule(world, game::PowerUp::Laser);
        const float after_second = paddle(world).laser_time_left;
        step(world, pause_key());
        const float paused_at = paddle(world).laser_time_left;
        steps(world, 120);
        const float after_pause = paddle(world).laser_time_left;
        step(world, pause_key());
        steps(world, 590);
        step(world, space());
        const std::size_t near_end = world.beams.size();
        steps(world, 8);
        const float flying_y = world.beams.size() > 0 ? world.transforms.find(world.beams.entities()[0])->position.y : 0.0f;
        steps(world, 3);
        step(world, space());
        const float left = paddle(world).laser_time_left;
        const std::size_t after_end = world.beams.size();
        const float still_y = world.beams.size() > 0 ? world.transforms.find(world.beams.entities()[0])->position.y : 0.0f;
        report(after_second > 9.9f && paused_at == after_pause && near_end == 2 && left == 0.0f && after_end == 2 &&
                   still_y > flying_y,
               "11  a second catch restarts the 10 s, a pause stops it, and at the end Space fires nothing but beams fly on",
               "after the 2nd catch " + num(after_second) + ", paused " + num(paused_at) + " -> " + num(after_pause) +
                   ", beams at 9.9 s " + std::to_string(near_end) + ", time left after 10 s " + num(left) +
                   ", beams after Space then " + std::to_string(after_end) + " (the old pair, still rising " +
                   yes(still_y > flying_y) + ")");
    }

    // 12
    {
        game::World world;
        world.size = game::WORLD_SIZE;
        world.seed_rng(1);
        game::start_game(world, game::parse_level("1111111111\n1111111111\n"));
        step(world, space());
        catch_capsule(world, game::PowerUp::Laser);
        step(world, space());
        const std::size_t fired = world.beams.size();
        const int lives = world.lives;
        // Moves the ball to just under the floor, so it is lost on the next step.
        const core::Entity ball = world.balls.entities()[0];
        world.transforms.find(ball)->position = math::Vec2{700.0f, -40.0f};
        world.transforms.find(ball)->previous_position = math::Vec2{700.0f, -40.0f};
        world.velocities.find(ball)->value = math::Vec2{0.0f, -350.0f};
        step(world);
        const bool ready = world.state == game::GameState::Ready;
        const std::size_t beams_after = world.beams.size();
        const float laser_after = paddle(world).laser_time_left;
        step(world, space());
        report(fired == 2 && world.lives == lives - 1 && ready && beams_after == 0 && laser_after == 0.0f &&
                   world.state == game::GameState::Playing && world.beams.size() == 0,
               "12  losing a life clears the beams and the lasers, and the next Space serves the ball without firing",
               "beams fired " + std::to_string(fired) + ", lives " + std::to_string(lives) + " -> " +
                   std::to_string(world.lives) + ", ready " + yes(ready) + ", beams " + std::to_string(beams_after) +
                   ", laser time " + num(laser_after) + ", after Space: playing " +
                   yes(world.state == game::GameState::Playing) + " with " + std::to_string(world.beams.size()) +
                   " beams");
    }

    // 13
    {
        game::World world = playing_world();
        catch_capsule(world, game::PowerUp::WidePaddle);
        catch_capsule(world, game::PowerUp::Laser);
        step(world, space());
        const std::vector<float> xs = beam_xs(world);
        const float x = paddle_x(world);
        const float half_wide = game::WIDE_PADDLE_WIDTH * 0.5f;
        const bool wide_ends = xs.size() == 2 && std::abs(xs[0] - (x - half_wide + game::BEAM_INSET)) < 0.01f &&
                               std::abs(xs[1] - (x + half_wide - game::BEAM_INSET)) < 0.01f;
        report(wide_ends && paddle(world).wide_time_left > 0.0f && paddle(world).laser_time_left > 0.0f,
               "13  with the wide paddle on too, both stay on and the beams leave from the wider ends",
               "beams at " + xs_text(xs) + " for a wide paddle at " + num(x) + ", wide " +
                   num(paddle(world).wide_time_left) + ", lasers " + num(paddle(world).laser_time_left));
    }

    // 14
    {
        const fs::path file = "tests/replays/laser.txt";
        const game::LoadedReplay loaded = game::load_replay(file);
        int catches = 0;
        int laser_hits = 0;
        bool header = false;
        if (loaded.ok)
        {
            header = read(file).rfind("breakout replay " + std::to_string(game::REPLAY_VERSION) + "\n", 0) == 0;
            const game::Replay& replay = loaded.replay;
            game::World world;
            world.size = game::WORLD_SIZE;
            world.seed_rng(replay.seed);
            world.rng.discard(replay.skip);
            world.rng_draws = replay.skip;
            game::start_game(world, replay.levels);
            float before = 0.0f;
            events.clear();
            for (const game::ReplayRun& run : replay.runs)
            {
                for (std::uint32_t i = 0; i < run.count; ++i)
                {
                    step(world, run.input);
                    const float now = world.paddles.size() > 0 ? paddle(world).laser_time_left : 0.0f;
                    if (now > before + 0.5f)
                    {
                        ++catches;
                    }
                    before = now;
                }
            }
            laser_hits = count_events(game::GameEvent::LaserHit);
        }

        const fs::path out = fs::temp_directory_path() / ("laser_out_" + std::to_string(::getpid()));
        const std::string command = "\"" + program + "\" \"" + file.string() + "\" > \"" + out.string() + "\" 2>&1";
        const int status = std::system(command.c_str());
        const int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        const std::string text = read(out);
        fs::remove(out);
        const bool match = code == 0 && text.find("MATCH:") != std::string::npos;
        report(loaded.ok && header && catches >= 1 && laser_hits >= 1 && match,
               "14  the laser replay is this version's, has a laser caught and beams hitting bricks, and plays back: MATCH",
               std::string("loads ") + yes(loaded.ok) + " " + loaded.error + ", version line " + yes(header) + ", laser catches " +
                   std::to_string(catches) + ", beam hits " + std::to_string(laser_hits) + ", breakout_replay exit " +
                   std::to_string(code));
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

echo "building the game sources, breakout_replay and the check program..."
if ! g++ -std=c++20 -Iinclude src/breakout_replay.cpp src/game/*.cpp -o "$work/breakout_replay" 2> "$work/build.log" ||
   ! g++ -std=c++20 -Iinclude "$work/check.cpp" src/game/*.cpp -o "$work/check" 2>> "$work/build.log"; then
    echo "the check program does not build against this code. First errors:"
    grep -m 3 "error" "$work/build.log" 2>/dev/null
    echo "RESULT: 0 OF 14 CHECKS PASSED (nothing could run)"
    exit 1
fi

"$work/check" "$work/breakout_replay"
code=$?
if [ $code -gt 128 ]; then
    echo "RESULT: THE CHECK PROGRAM CRASHED (signal $((code - 128))), so not every check ran"
    exit 1
fi
exit $code
