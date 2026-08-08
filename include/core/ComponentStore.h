#pragma once

#include <cstddef>
#include <utility>
#include <vector>

#include "core/Entity.h"

namespace core
{
    // Sparse set. Components live packed in m_dense so iteration has no holes,
    // and m_sparse maps an entity id to its position in that dense array.
    //
    // Removal is swap-and-pop, so the dense order is not stable: removing a
    // component moves the last one into its place.
    //
    // The sparse set layout is the entity storage worked through in Handmade
    // Hero, day 277: https://handmadehero.org
    template <typename T>
    class ComponentStore
    {
    public:
        bool has(Entity entity) const
        {
            return entity.id < m_sparse.size() && m_sparse[entity.id] != INVALID_INDEX;
        }

        // Null if the entity has no component of this type.
        T* find(Entity entity)
        {
            if (!has(entity))
            {
                return nullptr;
            }
            return &m_dense[m_sparse[entity.id]];
        }

        const T* find(Entity entity) const
        {
            if (!has(entity))
            {
                return nullptr;
            }
            return &m_dense[m_sparse[entity.id]];
        }

        // Replaces the existing component if the entity already has one.
        void add(Entity entity, T component)
        {
            if (has(entity))
            {
                m_dense[m_sparse[entity.id]] = std::move(component);
                return;
            }

            if (entity.id >= m_sparse.size())
            {
                m_sparse.resize(entity.id + 1, INVALID_INDEX);
            }

            m_sparse[entity.id] = m_dense.size();
            m_dense.push_back(std::move(component));
            m_dense_entities.push_back(entity);
        }

        void remove(Entity entity)
        {
            if (!has(entity))
            {
                return;
            }

            const std::size_t index = m_sparse[entity.id];
            const std::size_t last = m_dense.size() - 1;

            // Move the last component into the freed slot and repair the sparse
            // entry of whichever entity owned it.
            if (index != last)
            {
                m_dense[index] = std::move(m_dense[last]);
                m_dense_entities[index] = m_dense_entities[last];
                m_sparse[m_dense_entities[index].id] = index;
            }

            m_dense.pop_back();
            m_dense_entities.pop_back();
            m_sparse[entity.id] = INVALID_INDEX;
        }

        std::size_t size() const
        {
            return m_dense.size();
        }

        // m_dense_entities[i] owns m_dense[i]; iterate by index over size().
        const std::vector<Entity>& entities() const
        {
            return m_dense_entities;
        }

        std::vector<T>& components()
        {
            return m_dense;
        }

        const std::vector<T>& components() const
        {
            return m_dense;
        }

    private:
        static constexpr std::size_t INVALID_INDEX = static_cast<std::size_t>(-1);

        std::vector<T> m_dense;
        std::vector<Entity> m_dense_entities;
        std::vector<std::size_t> m_sparse;
    };
}
