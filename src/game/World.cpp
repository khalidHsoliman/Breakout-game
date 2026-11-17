#include "game/World.h"

namespace game
{
    core::Entity World::create_entity()
    {
        return core::Entity{m_next_id++};
    }

    void World::seed_rng(std::uint32_t seed)
    {
        rng.seed(seed);
        rng_seed = seed;
        rng_draws = 0;
    }

    std::uint32_t World::next_random()
    {
        rng_draws += 1;
        return static_cast<std::uint32_t>(rng());
    }

    void World::destroy_entity(core::Entity entity)
    {
        transforms.remove(entity);
        colors.remove(entity);
        circle_shapes.remove(entity);
        velocities.remove(entity);
        balls.remove(entity);
        paddles.remove(entity);
        bricks.remove(entity);
        steel.remove(entity);
        particles.remove(entity);
        capsules.remove(entity);
        beams.remove(entity);
        destroyed.remove(entity);
    }
}
