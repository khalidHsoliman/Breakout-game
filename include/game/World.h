#pragma once

#include <cstdint>
#include <vector>

#include "core/Color.h"
#include "core/ComponentStore.h"
#include "core/Entity.h"
#include "game/Components.h"
#include "game/Events.h"
#include "game/Level.h"
#include "math/Vec2.h"

namespace game
{
    inline constexpr int STARTING_LIVES = 3;

    // The play area, in world units. The window maps it onto the screen, and
    // the level checks measure against it.
    inline constexpr math::Vec2 WORLD_SIZE{800.0f, 600.0f};

    enum class GameState
    {
        Ready,      // ball held on the paddle, waiting to launch
        Playing,
        Paused,
        GameOver,   // out of lives
        Won         // the last level cleared
    };

    // Owns the entities and every component store.
    class World
    {
    public:
        core::Entity create_entity();

        // Removes the entity from every store.
        void destroy_entity(core::Entity entity);

        // The play area, in world units. Nothing here is measured in pixels.
        math::Vec2 size;

        int score = 0;
        int lives = STARTING_LIVES;
        GameState state = GameState::Ready;

        // Every level of the game in order, and which one is being played.
        // Kept so a restart can rebuild without main handing them back.
        std::vector<Level> levels;
        int level_index = 0;

        // The level being played: a copy of levels[level_index].
        Level level;

        // raised by systems during a step, consumed by main once per frame.
        std::vector<GameEvent> events;

        core::ComponentStore<Transform> transforms;
        core::ComponentStore<core::Color> colors;
        core::ComponentStore<CircleShape> circle_shapes;
        core::ComponentStore<Velocity> velocities;
        core::ComponentStore<Ball> balls;
        core::ComponentStore<Paddle> paddles;
        core::ComponentStore<Brick> bricks;
        core::ComponentStore<Particle> particles;
        core::ComponentStore<Destroyed> destroyed;

    private:
        // Ids are never recycled, so a stale entity can never alias a new one.
        std::uint32_t m_next_id = 1;
    };

    // Marked entities survive until the end of the step, so every system that
    // iterates must check this or it will act on something already dead.
    inline bool is_alive(const World& world, core::Entity entity)
    {
        return !world.destroyed.has(entity);
    }
}
