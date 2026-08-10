#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>

#include "game/Editor.h"
#include "game/LevelLayout.h"
#include "game/Spawn.h"
#include "game/Systems.h"
#include "math/Lerp.h"
#include "platform/Audio.h"
#include "platform/FileSystem.h"
#include "platform/Font.h"
#include "platform/Renderer.h"
#include "platform/Window.h"

namespace
{
    enum class AppMode
    {
        Playing,
        Editing
    };

    // Starting mode only - F11 toggles at runtime.
#ifdef BREAKOUT_DEBUG
    constexpr bool START_FULLSCREEN = false;
#else
    constexpr bool START_FULLSCREEN = true;
#endif

    // Used for windowed mode, whether that is at startup or after a toggle.
    constexpr int WINDOW_WIDTH = 800;
    constexpr int WINDOW_HEIGHT = 600;

    // World units. Currently 1:1 with pixels at the default window size, but
    // nothing in the game may rely on that.
    constexpr math::Vec2 WORLD_SIZE{800.0f, 600.0f};

    // A long stall must not queue up seconds of catch-up work: past this many
    // steps the leftover time is dropped instead.
    constexpr int MAX_STEPS_PER_FRAME = 5;

    // Digits are hit points; anything else is an empty cell.
    constexpr const char* FIRST_LEVEL = R"(
..111111..
.12222221.
1233333321
.12222221.
..111111..
)";

    constexpr const char* LEVEL_FILE_NAME = "custom-level.txt";

    constexpr float HUD_PIXEL = 3.0f;
    constexpr float HEADLINE_PIXEL = 6.0f;
    constexpr float PROMPT_PIXEL = 3.0f;

    constexpr core::Color HUD_COLOR{0.85f, 0.88f, 0.92f};
    constexpr core::Color GOOD_COLOR{0.60f, 0.90f, 0.55f};
    constexpr core::Color BAD_COLOR{0.92f, 0.45f, 0.45f};

    // An empty cell is drawn rather than skipped, so the grid shows where the
    // clickable area is.
    constexpr core::Color EMPTY_CELL_COLOR{0.16f, 0.18f, 0.22f};
    constexpr float CURSOR_BORDER = 3.0f;

    constexpr float BORDER_THICKNESS = 6.0f;
    constexpr core::Color BORDER_COLOR{0.28f, 0.32f, 0.38f};

    // Screen shake.
    constexpr float SHAKE_MAGNITUDE = 4.0f;
    constexpr float SHAKE_DECAY = 22.0f;

    // Held state is overwritten each frame. One-shot actions are OR-ed in and
    // survive until a step consumes them.
    void read_input(const platform::Window& window, game::Input& input)
    {
        input.move_left = window.is_key_down(platform::Key::MoveLeft);
        input.move_right = window.is_key_down(platform::Key::MoveRight);

        input.launch |= window.is_key_pressed(platform::Key::Launch);
        input.toggle_pause |= window.is_key_pressed(platform::Key::Pause);
        input.restart |= window.is_key_pressed(platform::Key::Restart);
    }

    math::Vec2 shake_offset(float magnitude)
    {
        const float x = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
        const float y = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);

        return math::Vec2{
            math::lerp(-magnitude, magnitude, x),
            math::lerp(-magnitude, magnitude, y)
        };
    }

    void draw_centered(
        platform::Renderer& renderer,
        std::string_view text,
        float y,
        float pixel_size,
        core::Color color
    )
    {
        const float x = (WORLD_SIZE.x - platform::text_width(text, pixel_size)) * 0.5f;
        renderer.draw_text(text, math::Vec2{x, y}, pixel_size, color);
    }

    // step_fraction is how far past the last simulated step this frame is, so
    // motion stays smooth on a display refreshing faster than the game steps.
    void render_world(const game::World& world, platform::Renderer& renderer, float step_fraction)
    {
        const std::vector<core::Entity>& entities = world.transforms.entities();
        const std::vector<game::Transform>& transforms = world.transforms.components();

        for (std::size_t i = 0; i < transforms.size(); i++)
        {
            const core::Entity entity = entities[i];
            const core::Color* color = world.colors.find(entity);
            if (color == nullptr)
            {
                continue;
            }

            const math::Vec2 position = math::lerp(transforms[i].previous_position, transforms[i].position, step_fraction);

            if (world.circle_shapes.has(entity))
            {
                renderer.draw_circle(position, transforms[i].size.x * 0.5f, *color);
            }
            else
            {
                renderer.draw_quad(position, transforms[i].size, *color);
            }
        }
    }

    void render_hud(const game::World& world, platform::Renderer& renderer)
    {
        const std::string score = "SCORE " + std::to_string(world.score);
        renderer.draw_text(score, math::Vec2{20.0f, 568.0f}, HUD_PIXEL, HUD_COLOR);

        const std::string lives = "LIVES " + std::to_string(world.lives);
        const float lives_x = WORLD_SIZE.x - 20.0f - platform::text_width(lives, HUD_PIXEL);
        renderer.draw_text(lives, math::Vec2{lives_x, 568.0f}, HUD_PIXEL, HUD_COLOR);

        switch (world.state)
        {
        case game::GameState::Ready:
            draw_centered(renderer, "PRESS SPACE TO LAUNCH", 200.0f, PROMPT_PIXEL, HUD_COLOR);
            draw_centered(renderer, "E FOR THE LEVEL EDITOR", 160.0f, PROMPT_PIXEL, HUD_COLOR);
            break;
        case game::GameState::Paused:
            draw_centered(renderer, "PAUSED", 300.0f, HEADLINE_PIXEL, HUD_COLOR);
            break;
        case game::GameState::GameOver:
            draw_centered(renderer, "GAME OVER", 320.0f, HEADLINE_PIXEL, BAD_COLOR);
            draw_centered(renderer, "PRESS R TO RESTART", 270.0f, PROMPT_PIXEL, HUD_COLOR);
            break;
        case game::GameState::Won:
            draw_centered(renderer, "YOU WIN", 320.0f, HEADLINE_PIXEL, GOOD_COLOR);
            draw_centered(renderer, "PRESS R TO RESTART", 270.0f, PROMPT_PIXEL, HUD_COLOR);
            break;
        case game::GameState::Playing:
            break;
        }
    }

    void render_editor(const game::Level& level, game::Cell cursor, platform::Renderer& renderer)
    {
        const math::Vec2 size = game::brick_size(level, WORLD_SIZE);

        for (int row = 0; row < level.rows; ++row)
        {
            for (int column = 0; column < level.columns; ++column)
            {
                const math::Vec2 center = game::brick_center(level, WORLD_SIZE, column, row);

                if (cursor.inside && cursor.column == column && cursor.row == row)
                {
                    renderer.draw_quad(
                        center,
                        size + math::Vec2{2.0f * CURSOR_BORDER, 2.0f * CURSOR_BORDER},
                        HUD_COLOR
                    );
                }

                const int hit_points = level.at(column, row);
                renderer.draw_quad(
                    center,
                    size,
                    hit_points > 0 ? game::brick_color(hit_points) : EMPTY_CELL_COLOR
                );
            }
        }
    }

    void render_editor_hud(platform::Renderer& renderer, std::string_view status, core::Color status_color)
    {
        draw_centered(renderer, "LEVEL EDITOR", 560.0f, HUD_PIXEL, HUD_COLOR);
        draw_centered(renderer, "CLICK CYCLES A BRICK   RIGHT CLICK CLEARS", 150.0f, PROMPT_PIXEL, HUD_COLOR);
        draw_centered(renderer, "S SAVE   L LOAD   ENTER PLAY   E BACK", 120.0f, PROMPT_PIXEL, HUD_COLOR);

        if (!status.empty())
        {
            draw_centered(renderer, status, 70.0f, PROMPT_PIXEL, status_color);
        }
    }

    void render_borders(platform::Renderer& renderer)
    {
        const float half = BORDER_THICKNESS * 0.5f;

        renderer.draw_quad(
            math::Vec2{half, WORLD_SIZE.y * 0.5f},
            math::Vec2{BORDER_THICKNESS, WORLD_SIZE.y},
            BORDER_COLOR
        );
        renderer.draw_quad(
            math::Vec2{WORLD_SIZE.x - half, WORLD_SIZE.y * 0.5f},
            math::Vec2{BORDER_THICKNESS, WORLD_SIZE.y},
            BORDER_COLOR
        );
        renderer.draw_quad(
            math::Vec2{WORLD_SIZE.x * 0.5f, WORLD_SIZE.y - half},
            math::Vec2{WORLD_SIZE.x, BORDER_THICKNESS},
            BORDER_COLOR
        );
    }
}

int main()
{
    platform::Window window;
    if (!window.init(WINDOW_WIDTH, WINDOW_HEIGHT, "Breakout", START_FULLSCREEN))
    {
        return 1;
    }

    platform::Renderer renderer;
    if (!renderer.init(WORLD_SIZE, window.framebuffer_size()))
    {
        return 1;
    }

    const std::filesystem::path executable_directory = platform::executable_directory();

    platform::Audio audio;
    audio.init(executable_directory / "assets");
    audio.start_music();

    game::World world;
    world.size = WORLD_SIZE;
    game::start_game(world, game::parse_level(FIRST_LEVEL));

    std::chrono::steady_clock::time_point previous = std::chrono::steady_clock::now();
    float accumulator = 0.0f;
    game::Input input;

    float shake = 0.0f;

    AppMode mode = AppMode::Playing;

    // The level under construction. Separate from world.level
    game::Level canvas;
    game::Cell cursor;

    std::string status;
    core::Color status_color = HUD_COLOR;

    const std::filesystem::path level_path = executable_directory / LEVEL_FILE_NAME;

    while (!window.should_close())
    {
        window.poll_events();

        if (window.is_key_down(platform::Key::Escape))
        {
            window.request_close();
        }

        if (window.is_key_pressed(platform::Key::ToggleFullscreen))
        {
            window.set_fullscreen(!window.is_fullscreen());
            renderer.set_viewport(window.framebuffer_size());
        }

        if (window.is_key_pressed(platform::Key::ToggleEditor))
        {
            if (mode == AppMode::Playing)
            {
                canvas = game::editor_canvas(world.level);
                status.clear();

                // No steps run in the editor, so nothing would ever decay it.
                shake = 0.0f;
            }

            mode = (mode == AppMode::Playing) ? AppMode::Editing : AppMode::Playing;
        }

        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        accumulator += std::chrono::duration<float>(now - previous).count();
        previous = now;

        if (mode == AppMode::Editing)
        {
            cursor = game::cell_at(canvas, WORLD_SIZE, renderer.world_from_pixel(window.cursor_pixel()));

            if (window.is_mouse_pressed(platform::MouseButton::Left))
            {
                game::cycle_cell(canvas, cursor);
                status.clear();
            }

            if (window.is_mouse_pressed(platform::MouseButton::Right))
            {
                game::clear_cell(canvas, cursor);
                status.clear();
            }

            if (window.is_key_pressed(platform::Key::Save))
            {
                const bool saved = platform::write_text_file(level_path, game::to_text(canvas));
                status = saved ? "SAVED" : "COULD NOT SAVE";
                status_color = saved ? GOOD_COLOR : BAD_COLOR;
            }

            if (window.is_key_pressed(platform::Key::Load))
            {
                std::string text;
                const bool loaded = platform::read_text_file(level_path, text);
                if (loaded)
                {
                    canvas = game::editor_canvas(game::parse_level(text));
                }

                status = loaded ? "LOADED" : "NOTHING SAVED YET";
                status_color = loaded ? GOOD_COLOR : BAD_COLOR;
            }

            if (window.is_key_pressed(platform::Key::Play))
            {
                game::start_game(world, canvas);
                mode = AppMode::Playing;
            }

            // No game time passes while editing
            accumulator = 0.0f;
        }
        else
        {
            read_input(window, input);

            int steps = 0;
            while (accumulator >= game::FIXED_DT && steps < MAX_STEPS_PER_FRAME)
            {
                game::step(world, input, game::FIXED_DT);

                // Shake decays over time
                shake = std::max(0.0f, shake - SHAKE_DECAY * game::FIXED_DT);

                // Consumed. Held state is refreshed next frame regardless.
                input.clear_one_shots();

                accumulator -= game::FIXED_DT;
                ++steps;
            }

            if (steps == MAX_STEPS_PER_FRAME)
            {
                accumulator = 0.0f;
            }

            for (const game::GameEvent event : world.events)
            {
                audio.play(
                    event == game::GameEvent::BrickHit
                        ? platform::Sound::BrickHit
                        : platform::Sound::PaddleBounce
                    );

                if (event == game::GameEvent::BrickHit)
                {
                    shake = SHAKE_MAGNITUDE;
                }
            }

            world.events.clear();
        }

        renderer.set_shake(shake_offset(shake));
        renderer.begin_frame();
        renderer.clear(core::Color{0.10f, 0.12f, 0.15f});

        if (mode == AppMode::Editing)
        {
            render_editor(canvas, cursor, renderer);
            render_editor_hud(renderer, status, status_color);
        }
        else
        {
            render_world(world, renderer, accumulator / game::FIXED_DT);
            render_hud(world, renderer);
        }

        // Both modes share the play area, so both get the frame.
        render_borders(renderer);

        renderer.end_frame();

        window.swap_buffers();
    }

    return 0;
}
