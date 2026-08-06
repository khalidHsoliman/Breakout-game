#include <gtest/gtest.h>

#include "game/World.h"

TEST(World, CreateEntityReturnsDistinctIds)
{
    game::World world;

    const core::Entity first = world.create_entity();
    const core::Entity second = world.create_entity();

    EXPECT_FALSE(first == second);
}

TEST(World, CreateEntityNeverReturnsNullEntity)
{
    game::World world;

    EXPECT_FALSE(world.create_entity() == core::null_entity);
}

TEST(World, DestroyEntityRemovesAllOfItsComponents)
{
    game::World world;
    const core::Entity entity = world.create_entity();
    world.transforms.add(entity, game::Transform{math::Vec2{1.0f, 2.0f}, math::Vec2{3.0f, 4.0f}});
    world.colors.add(entity, core::Color{1.0f, 0.0f, 0.0f});

    world.destroy_entity(entity);

    EXPECT_FALSE(world.transforms.has(entity));
    EXPECT_FALSE(world.colors.has(entity));
}

TEST(World, DestroyEntityLeavesOtherEntitiesIntact)
{
    game::World world;
    const core::Entity doomed = world.create_entity();
    const core::Entity survivor = world.create_entity();

    world.transforms.add(doomed, game::Transform{math::Vec2{1.0f, 1.0f}, math::Vec2{1.0f, 1.0f}});
    world.transforms.add(survivor, game::Transform{math::Vec2{5.0f, 6.0f}, math::Vec2{7.0f, 8.0f}});

    world.destroy_entity(doomed);

    ASSERT_NE(world.transforms.find(survivor), nullptr);
    EXPECT_FLOAT_EQ(world.transforms.find(survivor)->position.x, 5.0f);
    EXPECT_FLOAT_EQ(world.transforms.find(survivor)->size.y, 8.0f);
}

// The reason no generation counter is needed: a destroyed id is never handed
// out again, so a stale entity can never refer to a live one.
TEST(World, IdsAreNotRecycled)
{
    game::World world;

    const core::Entity first = world.create_entity();
    world.destroy_entity(first);
    const core::Entity second = world.create_entity();

    EXPECT_FALSE(first == second);
}
