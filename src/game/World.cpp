#include "game/World.h"

namespace game
{
    core::Entity World::create_entity()
    {
        return core::Entity{m_next_id++};
    }

    void World::destroy_entity(core::Entity entity)
    {
        transforms.remove(entity);
        colors.remove(entity);
        circle_shapes.remove(entity);
    }
}
