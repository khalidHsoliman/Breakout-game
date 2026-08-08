#include <gtest/gtest.h>

#include <numbers>

#include "math/Collision.h"

namespace
{
    // A brick-shaped box: 100 wide, 50 tall, lower-left at the origin.
    constexpr math::AABB BRICK{math::Vec2{0.0f, 0.0f}, math::Vec2{100.0f, 50.0f}};

    constexpr float MAX_BOUNCE_ANGLE = std::numbers::pi_v<float> / 3.0f;   // 60 degrees
    constexpr float TOLERANCE = 1e-5f;
}

TEST(CircleVsAABB, MissesWhenFarAway)
{
    const math::Contact contact = math::circle_vs_aabb(math::Circle{math::Vec2{50.0f, 100.0f}, 10.0f}, BRICK);

    EXPECT_FALSE(contact.hit);
}

TEST(CircleVsAABB, HitsFromAbove)
{
    const math::Contact contact = math::circle_vs_aabb(math::Circle{math::Vec2{50.0f, 55.0f}, 10.0f}, BRICK);

    EXPECT_TRUE(contact.hit);
    EXPECT_NEAR(contact.normal.x, 0.0f, TOLERANCE);
    EXPECT_NEAR(contact.normal.y, 1.0f, TOLERANCE);
    EXPECT_NEAR(contact.penetration, 5.0f, TOLERANCE);
}

TEST(CircleVsAABB, HitsFromBelow)
{
    const math::Contact contact = math::circle_vs_aabb(math::Circle{math::Vec2{50.0f, -5.0f}, 10.0f}, BRICK);

    EXPECT_TRUE(contact.hit);
    EXPECT_NEAR(contact.normal.x, 0.0f, TOLERANCE);
    EXPECT_NEAR(contact.normal.y, -1.0f, TOLERANCE);
    EXPECT_NEAR(contact.penetration, 5.0f, TOLERANCE);
}

TEST(CircleVsAABB, HitsFromTheLeft)
{
    const math::Contact contact = math::circle_vs_aabb(math::Circle{math::Vec2{-5.0f, 25.0f}, 10.0f}, BRICK);

    EXPECT_TRUE(contact.hit);
    EXPECT_NEAR(contact.normal.x, -1.0f, TOLERANCE);
    EXPECT_NEAR(contact.normal.y, 0.0f, TOLERANCE);
    EXPECT_NEAR(contact.penetration, 5.0f, TOLERANCE);
}

TEST(CircleVsAABB, HitsFromTheRight)
{
    const math::Contact contact = math::circle_vs_aabb(math::Circle{math::Vec2{105.0f, 25.0f}, 10.0f}, BRICK);

    EXPECT_TRUE(contact.hit);
    EXPECT_NEAR(contact.normal.x, 1.0f, TOLERANCE);
    EXPECT_NEAR(contact.normal.y, 0.0f, TOLERANCE);
    EXPECT_NEAR(contact.penetration, 5.0f, TOLERANCE);
}

// The case a box-shaped ball gets wrong: clipping a corner must reflect off the
// corner, not off whichever face happens to be nearer.
TEST(CircleVsAABB, HitsACornerDiagonally)
{
    const math::Contact contact = math::circle_vs_aabb(math::Circle{math::Vec2{103.0f, 53.0f}, 10.0f}, BRICK);

    EXPECT_TRUE(contact.hit);
    EXPECT_NEAR(contact.normal.x, 0.70710678f, TOLERANCE);
    EXPECT_NEAR(contact.normal.y, 0.70710678f, TOLERANCE);
    EXPECT_NEAR(math::length(contact.normal), 1.0f, TOLERANCE);
    EXPECT_NEAR(contact.penetration, 10.0f - 4.24264069f, TOLERANCE);
}

TEST(CircleVsAABB, TouchingExactlyIsNotAHit)
{
    const math::Contact contact = math::circle_vs_aabb(math::Circle{math::Vec2{50.0f, 60.0f}, 10.0f}, BRICK);

    EXPECT_FALSE(contact.hit);
}

// Reachable with a fast ball and discrete collision. The closest point is the
// centre itself, so the usual normal calculation would divide by zero.
TEST(CircleVsAABB, CentreInsideThePushesOutOfTheNearestFace)
{
    const math::Contact contact = math::circle_vs_aabb(math::Circle{math::Vec2{50.0f, 15.0f}, 10.0f}, BRICK);

    EXPECT_TRUE(contact.hit);
    EXPECT_NEAR(contact.normal.x, 0.0f, TOLERANCE);
    EXPECT_NEAR(contact.normal.y, -1.0f, TOLERANCE);
    EXPECT_NEAR(contact.penetration, 25.0f, TOLERANCE);
}

TEST(CircleVsAABB, ContactNormalIsAlwaysUnitLength)
{
    const math::Circle probes[] = {
        math::Circle{math::Vec2{50.0f, 55.0f}, 10.0f},
        math::Circle{math::Vec2{-5.0f, 25.0f}, 10.0f},
        math::Circle{math::Vec2{103.0f, 53.0f}, 10.0f},
        math::Circle{math::Vec2{-4.0f, -4.0f}, 10.0f},
    };

    for (const math::Circle& probe : probes)
    {
        const math::Contact contact = math::circle_vs_aabb(probe, BRICK);
        ASSERT_TRUE(contact.hit);
        EXPECT_NEAR(math::length(contact.normal), 1.0f, TOLERANCE);
    }
}

TEST(Reflect, StraightDownOffAFloorGoesStraightUp)
{
    const math::Vec2 reflected = math::reflect(math::Vec2{0.0f, -1.0f}, math::Vec2{0.0f, 1.0f});

    EXPECT_NEAR(reflected.x, 0.0f, TOLERANCE);
    EXPECT_NEAR(reflected.y, 1.0f, TOLERANCE);
}

TEST(Reflect, MirrorsTheNormalComponentOnly)
{
    const math::Vec2 reflected = math::reflect(math::Vec2{3.0f, -4.0f}, math::Vec2{0.0f, 1.0f});

    EXPECT_NEAR(reflected.x, 3.0f, TOLERANCE);
    EXPECT_NEAR(reflected.y, 4.0f, TOLERANCE);
}

TEST(Reflect, PreservesLength)
{
    const math::Vec2 incoming{3.0f, -4.0f};
    const math::Vec2 reflected = math::reflect(incoming, math::normalize(math::Vec2{1.0f, 2.0f}));

    EXPECT_NEAR(math::length(reflected), math::length(incoming), TOLERANCE);
}

TEST(Reflect, ReflectingTwiceReturnsTheOriginal)
{
    const math::Vec2 incoming{3.0f, -4.0f};
    const math::Vec2 normal = math::normalize(math::Vec2{1.0f, 2.0f});

    const math::Vec2 there_and_back = math::reflect(math::reflect(incoming, normal), normal);

    EXPECT_NEAR(there_and_back.x, incoming.x, TOLERANCE);
    EXPECT_NEAR(there_and_back.y, incoming.y, TOLERANCE);
}

TEST(PaddleBounce, CentreHitGoesStraightUp)
{
    const math::Vec2 direction = math::paddle_bounce_direction(0.0f, MAX_BOUNCE_ANGLE);

    EXPECT_NEAR(direction.x, 0.0f, TOLERANCE);
    EXPECT_NEAR(direction.y, 1.0f, TOLERANCE);
}

TEST(PaddleBounce, RightEdgeUsesTheMaximumAngle)
{
    const math::Vec2 direction = math::paddle_bounce_direction(1.0f, MAX_BOUNCE_ANGLE);

    EXPECT_NEAR(direction.x, 0.86602540f, TOLERANCE);   // sin 60
    EXPECT_NEAR(direction.y, 0.50000000f, TOLERANCE);   // cos 60
}

TEST(PaddleBounce, LeftEdgeMirrorsTheRight)
{
    const math::Vec2 direction = math::paddle_bounce_direction(-1.0f, MAX_BOUNCE_ANGLE);

    EXPECT_NEAR(direction.x, -0.86602540f, TOLERANCE);
    EXPECT_NEAR(direction.y, 0.50000000f, TOLERANCE);
}

// The ball's centre can be slightly past the paddle's edge when it clips a
// corner, which would otherwise produce an angle beyond the maximum.
TEST(PaddleBounce, OffsetIsClampedBeyondTheEdges)
{
    const math::Vec2 beyond = math::paddle_bounce_direction(2.5f, MAX_BOUNCE_ANGLE);
    const math::Vec2 edge = math::paddle_bounce_direction(1.0f, MAX_BOUNCE_ANGLE);

    EXPECT_NEAR(beyond.x, edge.x, TOLERANCE);
    EXPECT_NEAR(beyond.y, edge.y, TOLERANCE);
}

TEST(PaddleBounce, DirectionIsAlwaysUnitLength)
{
    for (float offset = -1.5f; offset <= 1.5f; offset += 0.25f)
    {
        const math::Vec2 direction = math::paddle_bounce_direction(offset, MAX_BOUNCE_ANGLE);
        EXPECT_NEAR(math::length(direction), 1.0f, TOLERANCE) << "offset " << offset;
    }
}

// The invariant the whole design exists to guarantee: the ball can never leave
// the paddle downward, whatever came in and wherever it hit.
TEST(PaddleBounce, AlwaysLeavesUpward)
{
    for (float offset = -1.5f; offset <= 1.5f; offset += 0.25f)
    {
        const math::Vec2 direction = math::paddle_bounce_direction(offset, MAX_BOUNCE_ANGLE);
        EXPECT_GT(direction.y, 0.0f) << "offset " << offset;
    }
}
