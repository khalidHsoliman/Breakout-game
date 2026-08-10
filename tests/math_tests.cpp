#include <gtest/gtest.h>

#include "math/Vec2.h"

TEST(Vec2, DefaultsToZero)
{
    math::Vec2 v;

    EXPECT_FLOAT_EQ(v.x, 0.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
}

TEST(Vec2, Arithmetic)
{
    const math::Vec2 a{1.0f, 2.0f};
    const math::Vec2 b{3.0f, 4.0f};

    EXPECT_FLOAT_EQ((a + b).x, 4.0f);
    EXPECT_FLOAT_EQ((a + b).y, 6.0f);

    EXPECT_FLOAT_EQ((a - b).x, -2.0f);
    EXPECT_FLOAT_EQ((a - b).y, -2.0f);

    EXPECT_FLOAT_EQ((-a).x, -1.0f);
    EXPECT_FLOAT_EQ((-a).y, -2.0f);

    EXPECT_FLOAT_EQ((b / 2.0f).x, 1.5f);
    EXPECT_FLOAT_EQ((b / 2.0f).y, 2.0f);

    // Both operand orders. If these ever become members the second form stops
    // compiling, which is the point of them being free functions.
    EXPECT_FLOAT_EQ((a * 2.0f).x, 2.0f);
    EXPECT_FLOAT_EQ((2.0f * a).x, 2.0f);
}

TEST(Vec2, CompoundAssignment)
{
    math::Vec2 a{1.0f, 2.0f};

    // Returns a reference to the operand, not a copy of it. Without this
    // someone could change the return type to Vec2 and nothing else would fail.
    math::Vec2& result = (a += math::Vec2{3.0f, 4.0f});
    EXPECT_EQ(&result, &a);
    EXPECT_FLOAT_EQ(a.x, 4.0f);
    EXPECT_FLOAT_EQ(a.y, 6.0f);

    a -= math::Vec2{3.0f, 4.0f};
    EXPECT_FLOAT_EQ(a.x, 1.0f);
    EXPECT_FLOAT_EQ(a.y, 2.0f);

    a *= 3.0f;
    EXPECT_FLOAT_EQ(a.x, 3.0f);
    EXPECT_FLOAT_EQ(a.y, 6.0f);

    a /= 3.0f;
    EXPECT_FLOAT_EQ(a.x, 1.0f);
    EXPECT_FLOAT_EQ(a.y, 2.0f);
}

TEST(Vec2, DotProduct)
{
    EXPECT_FLOAT_EQ(math::dot(math::Vec2{1.0f, 2.0f}, math::Vec2{3.0f, 4.0f}), 11.0f);
    EXPECT_FLOAT_EQ(math::dot(math::Vec2{1.0f, 0.0f}, math::Vec2{0.0f, 1.0f}), 0.0f);
    EXPECT_FLOAT_EQ(math::dot(math::Vec2{1.0f, 2.0f}, math::Vec2{-1.0f, -2.0f}), -5.0f);
}

TEST(Vec2, LengthAndDistance)
{
    EXPECT_FLOAT_EQ(math::length(math::Vec2{3.0f, 4.0f}), 5.0f);
    EXPECT_FLOAT_EQ(math::length_squared(math::Vec2{3.0f, 4.0f}), 25.0f);
    EXPECT_FLOAT_EQ(math::distance(math::Vec2{1.0f, 2.0f}, math::Vec2{4.0f, 6.0f}), 5.0f);
}

// The direction, not just the length: asserting only that the result is one
// unit long would pass for an implementation that ignored its input.
TEST(Vec2, Normalize)
{
    const math::Vec2 normalized = math::normalize(math::Vec2{3.0f, 4.0f});

    EXPECT_FLOAT_EQ(normalized.x, 0.6f);
    EXPECT_FLOAT_EQ(normalized.y, 0.8f);
}

// A ball with no velocity is reachable, and 0/0 would poison everything after.
TEST(Vec2, NormalizeOfZeroIsZero)
{
    const math::Vec2 normalized = math::normalize(math::Vec2{});

    EXPECT_FLOAT_EQ(normalized.x, 0.0f);
    EXPECT_FLOAT_EQ(normalized.y, 0.0f);
}
