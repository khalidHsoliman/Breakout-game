#!/bin/bash
# check_powerups.sh - end-to-end check of the power-ups.
#
# Run from the repo root:   bash check_powerups.sh
#
# Needs only g++ with C++20. It compiles the game sources (src/game/*.cpp) and a
# small check program into a temp folder, plays seeded games through
# game::step, and prints one line per check. Nothing is written inside the
# repo, and the temp folder is removed at the end.
#
# Last line: "RESULT: ALL 13 CHECKS PASSED" (exit 0), or how many failed (exit 1).

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
#include <cstdlib>
#include <string>
#include <vector>

#include "game/Level.h"
#include "game/Spawn.h"
#include "game/Systems.h"

namespace
{
    constexpr int TOTAL = 13;
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

    std::string num(float value)
    {
        char text[32];
        std::snprintf(text, sizeof(text), "%.2f", value);
        return text;
    }

    void step(game::World& world, const game::Input& input = game::Input{})
    {
        game::step(world, input, game::FIXED_DT);
    }

    void steps(game::World& world, int count)
    {
        for (int i = 0; i < count; ++i)
        {
            step(world);
        }
    }

    game::World new_world(const std::vector<game::Level>& levels, unsigned seed = 1)
    {
        game::World world;
        world.size = game::WORLD_SIZE;
        world.rng.seed(seed);
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

    // The ball stays in play but out of the way: it hangs still in mid-air.
    void hang_ball(game::World& world)
    {
        const core::Entity ball = first_ball(world);
        world.transforms.find(ball)->position = math::Vec2{100.0f, 300.0f};
        world.velocities.find(ball)->value = math::Vec2{};
    }

    core::Entity paddle(const game::World& world)
    {
        return world.paddles.entities()[0];
    }

    float paddle_width(const game::World& world)
    {
        return world.transforms.find(paddle(world))->size.x;
    }

    float paddle_x(const game::World& world)
    {
        return world.transforms.find(paddle(world))->position.x;
    }

    float wide_time(const game::World& world)
    {
        return world.paddles.find(paddle(world))->wide_time_left;
    }

    // A capsule just above the paddle, dx from its centre; one step lets it
    // fall onto the paddle.
    core::Entity drop_on_paddle(game::World& world, game::PowerUp kind, float dx = 0.0f)
    {
        const float y = game::PADDLE_Y + game::PADDLE_HEIGHT * 0.5f + game::CAPSULE_HEIGHT * 0.5f + 1.0f;
        const core::Entity capsule = game::spawn_capsule(world, math::Vec2{paddle_x(world) + dx, y}, kind);
        step(world);
        return capsule;
    }

    // A playing world whose ball hangs still: only what the check does happens.
    game::World quiet_world()
    {
        game::World world = new_world({game::parse_level("1.1")});
        launch(world);
        hang_ball(world);
        return world;
    }

    // Puts the ball under the first live brick, moving up, and steps until the
    // score changes (one hit).
    void hit_first_brick(game::World& world)
    {
        const core::Entity brick = world.bricks.entities()[0];
        const math::Vec2 at = world.transforms.find(brick)->position;
        const core::Entity ball = first_ball(world);
        world.transforms.find(ball)->position = at - math::Vec2{0.0f, 30.0f};
        world.velocities.find(ball)->value = math::Vec2{0.0f, 300.0f};

        const int score = world.score;
        for (int i = 0; i < 30 && world.score == score; ++i)
        {
            step(world);
        }
    }

    std::vector<game::PowerUp> drops_from(unsigned seed, int bricks)
    {
        game::World world;
        world.size = game::WORLD_SIZE;
        world.state = game::GameState::Playing;
        world.rng.seed(seed);

        const core::Entity ball = world.create_entity();
        world.transforms.add(ball, game::Transform{{400.0f, 490.0f}, {16.0f, 16.0f}, {400.0f, 490.0f}});
        world.velocities.add(ball, game::Velocity{});
        world.balls.add(ball, game::Ball{game::BALL_RADIUS});

        for (int i = 0; i < bricks; ++i)
        {
            const core::Entity brick = world.create_entity();
            world.transforms.add(brick, game::Transform{{400.0f, 500.0f}, {60.0f, 24.0f}, {400.0f, 500.0f}});
            world.bricks.add(brick, game::Brick{1});
            world.transforms.find(ball)->position = math::Vec2{400.0f, 490.0f};
            world.velocities.find(ball)->value = math::Vec2{0.0f, 200.0f};
            game::collision_system(world);
            game::sweep_destroyed(world);
        }

        std::vector<game::PowerUp> kinds;
        for (const game::Capsule& capsule : world.capsules.components())
        {
            kinds.push_back(capsule.kind);
        }
        return kinds;
    }

    float lowest_ball_x(const game::World& world)
    {
        float x = 400.0f;
        float lowest = 1e9f;
        for (const core::Entity ball : world.balls.entities())
        {
            const game::Transform* transform = world.transforms.find(ball);
            if (transform != nullptr && transform->position.y < lowest)
            {
                lowest = transform->position.y;
                x = transform->position.x;
            }
        }
        return x;
    }

    // A simple autopilot: launch when held, follow the lowest ball.
    game::Input autopilot(const game::World& world)
    {
        game::Input input;
        input.launch = world.state == game::GameState::Ready;
        input.restart = world.state == game::GameState::GameOver || world.state == game::GameState::Won;
        const float target = lowest_ball_x(world);
        input.move_left = target < paddle_x(world) - 10.0f;
        input.move_right = target > paddle_x(world) + 10.0f;
        return input;
    }
}

int main()
{
    // 1
    {
        game::World always = new_world({game::parse_level("1.1")});
        always.capsule_drop_chance = 1.0f;
        launch(always);
        const math::Vec2 brick_at = always.transforms.find(always.bricks.entities()[0])->position;
        hit_first_brick(always);
        bool where = false;
        for (const core::Entity capsule : always.capsules.entities())
        {
            const math::Vec2 at = always.transforms.find(capsule)->position;
            where = where || (std::abs(at.x - brick_at.x) < 0.01f && brick_at.y - at.y < 10.0f && brick_at.y - at.y >= 0.0f);
        }

        game::World never = new_world({game::parse_level("1.1")});
        never.capsule_drop_chance = 0.0f;
        launch(never);
        hit_first_brick(never);

        game::World tough = new_world({game::parse_level("2.1")});
        tough.capsule_drop_chance = 1.0f;
        launch(tough);
        hit_first_brick(tough);

        report(always.capsules.size() == 1 && where && never.capsules.size() == 0 &&
                   tough.capsules.size() == 0 && tough.bricks.size() == 2,
               "1   a destroyed brick drops a capsule where it was; none at chance 0 or when a brick survives",
               "chance 1: " + std::to_string(always.capsules.size()) + " capsules, in place=" + (where ? "yes" : "no") +
                   "; chance 0: " + std::to_string(never.capsules.size()) + "; brick survived: " +
                   std::to_string(tough.capsules.size()) + " capsules, " + std::to_string(tough.bricks.size()) + " bricks");
    }

    // 2
    {
        const std::vector<game::PowerUp> a = drops_from(42, 2000);
        const std::vector<game::PowerUp> b = drops_from(42, 2000);
        const std::vector<game::PowerUp> c = drops_from(43, 2000);
        int wide = 0;
        for (const game::PowerUp kind : a)
        {
            wide += kind == game::PowerUp::WidePaddle ? 1 : 0;
        }
        const int count = static_cast<int>(a.size());
        const int multi = count - wide;
        report(count >= 200 && count <= 400 && wide >= count / 4 && multi >= count / 4 && a == b && a != c,
               "2   about 15% of destroyed bricks drop, both kinds, and the same seed drops the same capsules",
               std::to_string(count) + " of 2000 dropped (" + std::to_string(wide) + " wide, " + std::to_string(multi) +
                   " multi-ball); same seed same drops=" + (a == b ? "yes" : "no") + ", other seed differs=" +
                   (a != c ? "yes" : "no"));
    }

    // 3
    {
        game::World world = quiet_world();
        const core::Entity falling = game::spawn_capsule(world, math::Vec2{300.0f, 400.0f}, game::PowerUp::WidePaddle);
        steps(world, 60);
        const math::Vec2 at = world.transforms.find(falling)->position;
        const bool straight = std::abs(at.x - 300.0f) < 0.001f && std::abs(at.y - 250.0f) < 1.0f;

        const int lives = world.lives;
        const core::Entity missed = game::spawn_capsule(world, math::Vec2{700.0f, 20.0f}, game::PowerUp::MultiBall);
        steps(world, 15);
        const bool gone = !world.capsules.has(missed);
        report(straight && gone && world.lives == lives && world.state == game::GameState::Playing,
               "3   a capsule falls straight down at 150 units/s; a missed one disappears and costs no life",
               "after 1 s at (" + num(at.x) + ", " + num(at.y) + "), expected (300, 250); missed gone=" +
                   (gone ? "yes" : "no") + ", lives " + std::to_string(world.lives));
    }

    // 4
    {
        game::World world = quiet_world();
        drop_on_paddle(world, game::PowerUp::WidePaddle);
        const float width = paddle_width(world);
        steps(world, 590);
        const float still = paddle_width(world);
        steps(world, 20);
        report(width == game::WIDE_PADDLE_WIDTH && still == game::WIDE_PADDLE_WIDTH && paddle_width(world) == game::PADDLE_WIDTH,
               "4   catching a wide capsule makes the paddle 180 wide for 10 seconds, then 120 again",
               "after catch " + num(width) + ", after 9.8 s " + num(still) + ", after 10.2 s " + num(paddle_width(world)));
    }

    // 5
    {
        game::World world = quiet_world();
        drop_on_paddle(world, game::PowerUp::WidePaddle);
        steps(world, 300);
        drop_on_paddle(world, game::PowerUp::WidePaddle);
        const float after_second = paddle_width(world);
        steps(world, 580);
        const float late = paddle_width(world);
        steps(world, 30);
        report(after_second == game::WIDE_PADDLE_WIDTH && late == game::WIDE_PADDLE_WIDTH && paddle_width(world) == game::PADDLE_WIDTH,
               "5   a second catch restarts the 10 seconds without making the paddle wider",
               "after second catch " + num(after_second) + ", 9.7 s later " + num(late) + ", 10.2 s later " +
                   num(paddle_width(world)));
    }

    // 6
    {
        game::World left = quiet_world();
        left.transforms.find(paddle(left))->position.x = game::PADDLE_WIDTH * 0.5f;
        drop_on_paddle(left, game::PowerUp::WidePaddle);
        game::World right = quiet_world();
        right.transforms.find(paddle(right))->position.x = game::WORLD_SIZE.x - game::PADDLE_WIDTH * 0.5f;
        drop_on_paddle(right, game::PowerUp::WidePaddle);
        const float half = game::WIDE_PADDLE_WIDTH * 0.5f;
        report(paddle_width(left) == game::WIDE_PADDLE_WIDTH && paddle_x(left) >= half - 0.01f &&
                   paddle_width(right) == game::WIDE_PADDLE_WIDTH && paddle_x(right) <= game::WORLD_SIZE.x - half + 0.01f,
               "6   a paddle widened against either wall stays inside the walls",
               "left centre " + num(paddle_x(left)) + " (min " + num(half) + "), right centre " + num(paddle_x(right)) +
                   " (max " + num(game::WORLD_SIZE.x - half) + ")");
    }

    // 7
    {
        game::World world = quiet_world();
        drop_on_paddle(world, game::PowerUp::WidePaddle);
        steps(world, 60);
        game::Input pause;
        pause.toggle_pause = true;
        step(world, pause);
        const bool paused = world.state == game::GameState::Paused;
        steps(world, 900);
        step(world, pause);
        report(paused && world.state == game::GameState::Playing && paddle_width(world) == game::WIDE_PADDLE_WIDTH &&
                   wide_time(world) > 8.5f,
               "7   the wide paddle's time does not run while the game is paused",
               std::string("paused=") + (paused ? "yes" : "no") + ", after 15 s paused: width " + num(paddle_width(world)) +
                   ", time left " + num(wide_time(world)) + " s");
    }

    // 8
    {
        const float outer = game::PADDLE_WIDTH * 0.5f + game::CAPSULE_WIDTH * 0.5f + 5.0f;
        game::World wide = quiet_world();
        drop_on_paddle(wide, game::PowerUp::WidePaddle);
        steps(wide, 60);
        const core::Entity far = drop_on_paddle(wide, game::PowerUp::WidePaddle, outer);
        const bool wide_caught = !wide.capsules.has(far) && wide_time(wide) > 9.9f;

        game::World normal = quiet_world();
        const core::Entity missed = drop_on_paddle(normal, game::PowerUp::WidePaddle, outer);
        const bool normal_missed = normal.capsules.has(missed) && paddle_width(normal) == game::PADDLE_WIDTH;
        report(wide_caught && normal_missed,
               "8   the outer part of a wide paddle catches capsules a normal paddle would miss",
               std::string("wide caught=") + (wide_caught ? "yes" : "no") + ", normal missed=" + (normal_missed ? "yes" : "no"));
    }

    // 9
    {
        game::World world = quiet_world();
        const core::Entity source = first_ball(world);
        world.transforms.find(source)->position = math::Vec2{400.0f, 300.0f};
        world.velocities.find(source)->value = math::Vec2{0.0f, 350.0f};
        drop_on_paddle(world, game::PowerUp::MultiBall);

        int good = 0;
        std::string seen;
        for (const core::Entity ball : world.balls.entities())
        {
            if (ball == source)
            {
                continue;
            }
            const math::Vec2 v = world.velocities.find(ball)->value;
            const float speed = std::sqrt(v.x * v.x + v.y * v.y);
            const bool ok = std::abs(speed - 350.0f) < 0.5f && v.y > 0.0f && std::abs(std::abs(v.x) / speed - 0.5f) < 0.01f;
            good += ok ? 1 : 0;
            seen += "(" + num(v.x) + ", " + num(v.y) + ") ";
        }
        report(world.balls.size() == 3 && good == 2,
               "9   a multi-ball capsule adds two balls going up 30 degrees either side of vertical at the ball's speed",
               std::to_string(world.balls.size()) + " balls; new velocities " + seen);
    }

    // 10
    {
        game::World world = quiet_world();
        drop_on_paddle(world, game::PowerUp::MultiBall);
        const std::size_t three = world.balls.size();
        world.destroy_entity(world.balls.entities()[2]);
        drop_on_paddle(world, game::PowerUp::MultiBall);
        const std::size_t four = world.balls.size();
        drop_on_paddle(world, game::PowerUp::MultiBall);
        const std::size_t five = world.balls.size();
        drop_on_paddle(world, game::PowerUp::MultiBall);
        const std::size_t still_five = world.balls.size();

        game::World fallen = quiet_world();
        fallen.transforms.find(first_ball(fallen))->position = math::Vec2{100.0f, -50.0f};
        drop_on_paddle(fallen, game::PowerUp::MultiBall);
        report(three == 3 && four == 4 && five == game::MAX_BALLS && still_five == game::MAX_BALLS && fallen.balls.size() == 1,
               "10  never more than 5 balls (from 4 a catch adds one), and nothing when no ball is in play",
               "1->" + std::to_string(three) + ", 2->" + std::to_string(four) + ", 4->" + std::to_string(five) + ", 5->" +
                   std::to_string(still_five) + "; with the only ball fallen: " + std::to_string(fallen.balls.size()) + " ball");
    }

    // 11
    {
        game::World world = quiet_world();
        const core::Entity source = first_ball(world);
        world.transforms.find(source)->position = math::Vec2{400.0f, 300.0f};
        world.velocities.find(source)->value = math::Vec2{0.0f, 350.0f};
        drop_on_paddle(world, game::PowerUp::MultiBall);
        drop_on_paddle(world, game::PowerUp::WidePaddle);

        const std::vector<core::Entity> balls = world.balls.entities();
        world.transforms.find(balls[0])->position.y = -50.0f;
        step(world);
        const bool one_gone = world.lives == game::STARTING_LIVES && world.state == game::GameState::Playing &&
                              world.balls.size() == 2;

        game::spawn_capsule(world, math::Vec2{300.0f, 400.0f}, game::PowerUp::MultiBall);
        for (const core::Entity ball : world.balls.entities())
        {
            world.transforms.find(ball)->position.y = -50.0f;
        }
        step(world);
        const bool one_life = world.lives == game::STARTING_LIVES - 1 && world.state == game::GameState::Ready &&
                              world.balls.size() == 1 && world.capsules.size() == 0 &&
                              paddle_width(world) == game::PADDLE_WIDTH && wide_time(world) == 0.0f;
        launch(world);
        const math::Vec2 v = world.velocities.find(first_ball(world))->value;
        const bool serve = world.balls.size() == 1 && world.state == game::GameState::Playing && (v.x != 0.0f || v.y != 0.0f);
        report(one_gone && one_life && serve,
               "11  only the last ball costs a life, one life however many fall; then capsules and wide paddle go and one ball is served",
               std::string("one of three falls costs nothing=") + (one_gone ? "yes" : "no") + "; all fall: lives " +
                   std::to_string(world.lives) + ", one life and cleared=" + (one_life ? "yes" : "no") + "; serve plays one ball=" +
                   (serve ? "yes" : "no"));
    }

    // 12
    {
        game::World two = new_world({game::parse_level("1.1"), game::parse_level("11")});
        launch(two);
        hang_ball(two);
        drop_on_paddle(two, game::PowerUp::WidePaddle);
        drop_on_paddle(two, game::PowerUp::MultiBall);
        game::spawn_capsule(two, math::Vec2{300.0f, 400.0f}, game::PowerUp::WidePaddle);
        for (const core::Entity brick : std::vector<core::Entity>(two.bricks.entities()))
        {
            two.destroy_entity(brick);
        }
        step(two);
        const bool next_level = two.level_index == 1 && two.balls.size() == 1 && two.capsules.size() == 0 &&
                                paddle_width(two) == game::PADDLE_WIDTH;

        game::World one = new_world({game::parse_level("1.1")});
        launch(one);
        hang_ball(one);
        drop_on_paddle(one, game::PowerUp::WidePaddle);
        drop_on_paddle(one, game::PowerUp::MultiBall);
        game::spawn_capsule(one, math::Vec2{300.0f, 400.0f}, game::PowerUp::WidePaddle);
        for (const core::Entity brick : std::vector<core::Entity>(one.bricks.entities()))
        {
            one.destroy_entity(brick);
        }
        step(one);
        const bool won_clear = one.state == game::GameState::Won && one.capsules.size() == 0;
        game::Input restart;
        restart.restart = true;
        step(one, restart);
        const bool restarted = one.state == game::GameState::Ready && one.balls.size() == 1 && one.capsules.size() == 0 &&
                               paddle_width(one) == game::PADDLE_WIDTH && one.score == 0;
        report(next_level && won_clear && restarted,
               "12  the next level, a win and R clear the capsules, the extra balls and the wide paddle",
               std::string("next level clean=") + (next_level ? "yes" : "no") + ", win clears capsules=" +
                   (won_clear ? "yes" : "no") + ", R after win clean=" + (restarted ? "yes" : "no"));
    }

    // 13
    {
        std::vector<game::World> worlds;
        int most_capsules = 0;
        std::size_t most_balls = 0;
        for (int run = 0; run < 2; ++run)
        {
            std::srand(1);
            game::World world = new_world({game::parse_level(game::BUILT_IN_LEVEL)}, 7);
            world.capsule_drop_chance = 0.5f;
            for (int i = 0; i < 6000; ++i)
            {
                game::Input input = autopilot(world);
                step(world, input);
                most_capsules = std::max(most_capsules, static_cast<int>(world.capsules.size()));
                most_balls = std::max(most_balls, world.balls.size());
            }
            worlds.push_back(world);
        }
        const game::World& a = worlds[0];
        const game::World& b = worlds[1];
        bool same = a.score == b.score && a.lives == b.lives && a.state == b.state && a.level_index == b.level_index &&
                    a.rng == b.rng && a.balls.size() == b.balls.size() && a.capsules.size() == b.capsules.size();
        for (std::size_t i = 0; same && i < a.balls.size(); ++i)
        {
            const math::Vec2 pa = a.transforms.find(a.balls.entities()[i])->position;
            const math::Vec2 pb = b.transforms.find(b.balls.entities()[i])->position;
            same = pa.x == pb.x && pa.y == pb.y;
        }
        report(same && most_capsules > 0,
               "13  two games with the same seed and the same input end exactly the same, with capsules dropping",
               std::string("same=") + (same ? "yes" : "no") + ", score " + std::to_string(a.score) + " vs " +
                   std::to_string(b.score) + ", most capsules at once " + std::to_string(most_capsules) +
                   ", most balls " + std::to_string(most_balls));
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
    echo "RESULT: 0 OF 13 CHECKS PASSED (nothing could run)"
    exit 1
fi

"$work/check"
