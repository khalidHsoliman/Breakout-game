#pragma once

#include <cstdint>

#include "core/Color.h"
#include "core/ComponentStore.h"
#include "core/Entity.h"
#include "game/Components.h"
#include "game/Level.h"
#include "math/Vec2.h"

namespace game
{
    inline constexpr int STARTING_LIVES = 3;

    enum class GameState
    {
        Ready,      // ball held on the paddle, waiting to launch
        Playing,
        Paused,
        GameOver,   // out of lives
        Won         // no bricks left
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

        // Singletons: exactly one of each, so plain members rather than a
        // component store holding a single entry.
        int score = 0;
        int lives = STARTING_LIVES;
        GameState state = GameState::Ready;

        // Kept so a restart can rebuild without main handing it back
        Level level;

        core::ComponentStore<Transform> transforms;
        core::ComponentStore<core::Color> colors;
        core::ComponentStore<CircleShape> circle_shapes;
        core::ComponentStore<Velocity> velocities;
        core::ComponentStore<Ball> balls;
        core::ComponentStore<Paddle> paddles;
        core::ComponentStore<Brick> bricks;
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
