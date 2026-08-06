#pragma once

#include <cstdint>

#include "core/Color.h"
#include "core/ComponentStore.h"
#include "core/Entity.h"
#include "game/Components.h"

namespace game
{
    // Owns the entities and every component store. The stores are named members
    // rather than a type-erased map: there are only a handful of component
    // types, all known at compile time, so this file doubles as the data model.
    class World
    {
    public:
        core::Entity create_entity();

        // Removes the entity from every store. Adding a store below means
        // adding a line here too.
        void destroy_entity(core::Entity entity);

        core::ComponentStore<Transform> transforms;
        core::ComponentStore<core::Color> colors;
        core::ComponentStore<CircleShape> circle_shapes;

    private:
        // Ids are never recycled, so a stale entity can never alias a new one.
        std::uint32_t m_next_id = 1;
    };
}
