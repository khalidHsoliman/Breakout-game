#include <gtest/gtest.h>

#include "core/ComponentStore.h"

namespace
{
    constexpr core::Entity ENTITY_A{1};
    constexpr core::Entity ENTITY_B{2};
    constexpr core::Entity ENTITY_C{3};
}

TEST(ComponentStore, EmptyStoreHasNothing)
{
    core::ComponentStore<int> store;

    EXPECT_EQ(store.size(), 0u);
    EXPECT_FALSE(store.has(ENTITY_A));
    EXPECT_EQ(store.find(ENTITY_A), nullptr);
}

TEST(ComponentStore, AddThenFind)
{
    core::ComponentStore<int> store;
    store.add(ENTITY_A, 10);

    EXPECT_TRUE(store.has(ENTITY_A));
    ASSERT_NE(store.find(ENTITY_A), nullptr);
    EXPECT_EQ(*store.find(ENTITY_A), 10);
    EXPECT_EQ(store.size(), 1u);
}

TEST(ComponentStore, AddReplacesExistingComponent)
{
    core::ComponentStore<int> store;
    store.add(ENTITY_A, 10);
    store.add(ENTITY_A, 99);

    ASSERT_NE(store.find(ENTITY_A), nullptr);
    EXPECT_EQ(*store.find(ENTITY_A), 99);
    EXPECT_EQ(store.size(), 1u);
}

TEST(ComponentStore, RemoveMakesEntityAbsent)
{
    core::ComponentStore<int> store;
    store.add(ENTITY_A, 10);
    store.remove(ENTITY_A);

    EXPECT_FALSE(store.has(ENTITY_A));
    EXPECT_EQ(store.find(ENTITY_A), nullptr);
    EXPECT_EQ(store.size(), 0u);
}

// The case swap-and-pop is easy to get wrong: removing from the middle moves
// the last component into the hole, and that entity's sparse entry must be
// repaired or its component becomes unreachable.
TEST(ComponentStore, RemoveFromMiddleKeepsOtherEntitiesReachable)
{
    core::ComponentStore<int> store;
    store.add(ENTITY_A, 10);
    store.add(ENTITY_B, 20);
    store.add(ENTITY_C, 30);

    store.remove(ENTITY_B);

    EXPECT_FALSE(store.has(ENTITY_B));
    EXPECT_EQ(store.size(), 2u);

    ASSERT_NE(store.find(ENTITY_A), nullptr);
    ASSERT_NE(store.find(ENTITY_C), nullptr);
    EXPECT_EQ(*store.find(ENTITY_A), 10);
    EXPECT_EQ(*store.find(ENTITY_C), 30);
}

TEST(ComponentStore, RemoveLastElement)
{
    core::ComponentStore<int> store;
    store.add(ENTITY_A, 10);
    store.add(ENTITY_B, 20);

    store.remove(ENTITY_B);

    EXPECT_FALSE(store.has(ENTITY_B));
    ASSERT_NE(store.find(ENTITY_A), nullptr);
    EXPECT_EQ(*store.find(ENTITY_A), 10);
    EXPECT_EQ(store.size(), 1u);
}

TEST(ComponentStore, RemoveOfUnknownEntityIsANoOp)
{
    core::ComponentStore<int> store;
    store.add(ENTITY_A, 10);

    store.remove(ENTITY_B);

    EXPECT_EQ(store.size(), 1u);
    EXPECT_TRUE(store.has(ENTITY_A));
}

// The invariant every system relies on: entities()[i] owns components()[i],
// with no holes, whatever sequence of adds and removes got us here.
TEST(ComponentStore, DenseArraysStayParallelAfterRemoval)
{
    core::ComponentStore<int> store;
    store.add(ENTITY_A, 10);
    store.add(ENTITY_B, 20);
    store.add(ENTITY_C, 30);
    store.remove(ENTITY_A);

    ASSERT_EQ(store.entities().size(), store.size());
    ASSERT_EQ(store.components().size(), store.size());

    for (std::size_t i = 0; i < store.size(); ++i)
    {
        const core::Entity entity = store.entities()[i];
        ASSERT_NE(store.find(entity), nullptr);
        EXPECT_EQ(*store.find(entity), store.components()[i]);
    }
}

// A high id must grow only the sparse lookup, never the dense array.
TEST(ComponentStore, HandlesSparseEntityIds)
{
    core::ComponentStore<int> store;
    store.add(core::Entity{1000}, 42);

    EXPECT_TRUE(store.has(core::Entity{1000}));
    EXPECT_FALSE(store.has(core::Entity{5}));
    EXPECT_EQ(store.size(), 1u);
}

TEST(ComponentStore, ComponentsCanBeMutatedThroughFind)
{
    core::ComponentStore<int> store;
    store.add(ENTITY_A, 10);

    *store.find(ENTITY_A) = 55;

    EXPECT_EQ(*store.find(ENTITY_A), 55);
}
