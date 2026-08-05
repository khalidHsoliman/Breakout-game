#include <gtest/gtest.h>

#include "math/Vec2.h"

TEST(Vec2, DefaultConstructorIsZero)
{
    math::Vec2 v;
    EXPECT_FLOAT_EQ(v.x, 0.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
}

TEST(Vec2, Addition)
{
    math::Vec2 sum = math::Vec2{1.0f, 2.0f} + math::Vec2{3.0f, 4.0f};
    EXPECT_FLOAT_EQ(sum.x, 4.0f);
    EXPECT_FLOAT_EQ(sum.y, 6.0f);
}

TEST(Vec2, Subtraction)
{
    math::Vec2 difference = math::Vec2{1.0f, 2.0f} - math::Vec2{3.0f, 4.0f};
    EXPECT_FLOAT_EQ(difference.x, -2.0f);
    EXPECT_FLOAT_EQ(difference.y, -2.0f);
}

TEST(Vec2, UnaryNegation)
{
    math::Vec2 negated = -math::Vec2{1.0f, -2.0f};
    EXPECT_FLOAT_EQ(negated.x, -1.0f);
    EXPECT_FLOAT_EQ(negated.y, 2.0f);
}

TEST(Vec2, ScalarMultiplication)
{
    math::Vec2 v{1.0f, 2.0f};

    math::Vec2 scaledOnRight = v * 2.0f;
    EXPECT_FLOAT_EQ(scaledOnRight.x, 2.0f);
    EXPECT_FLOAT_EQ(scaledOnRight.y, 4.0f);

    math::Vec2 scaledOnLeft = 2.0f * v;
    EXPECT_FLOAT_EQ(scaledOnLeft.x, 2.0f);
    EXPECT_FLOAT_EQ(scaledOnLeft.y, 4.0f);
}

TEST(Vec2, ScalarDivision)
{
    math::Vec2 halved = math::Vec2{3.0f, 4.0f} / 2.0f;
    EXPECT_FLOAT_EQ(halved.x, 1.5f);
    EXPECT_FLOAT_EQ(halved.y, 2.0f);
}

TEST(Vec2, ScalarDivisionByZeroIsZero)
{
    math::Vec2 divided = math::Vec2{3.0f, 4.0f} / 0.0f;
    EXPECT_FLOAT_EQ(divided.x, 0.0f);
    EXPECT_FLOAT_EQ(divided.y, 0.0f);
}

TEST(Vec2, AddAssign)
{
    math::Vec2 a{1.0f, 2.0f};
    math::Vec2& result = (a += math::Vec2{3.0f, 4.0f});

    EXPECT_FLOAT_EQ(a.x, 4.0f);
    EXPECT_FLOAT_EQ(a.y, 6.0f);
    EXPECT_EQ(&result, &a);
}

TEST(Vec2, SubtractAssign)
{
    math::Vec2 a{1.0f, 2.0f};
    a -= math::Vec2{3.0f, 4.0f};

    EXPECT_FLOAT_EQ(a.x, -2.0f);
    EXPECT_FLOAT_EQ(a.y, -2.0f);
}

TEST(Vec2, MultiplyAssign)
{
    math::Vec2 a{1.0f, 2.0f};
    a *= 3.0f;

    EXPECT_FLOAT_EQ(a.x, 3.0f);
    EXPECT_FLOAT_EQ(a.y, 6.0f);
}

TEST(Vec2, DivideAssign)
{
    math::Vec2 a{3.0f, 4.0f};
    a /= 2.0f;

    EXPECT_FLOAT_EQ(a.x, 1.5f);
    EXPECT_FLOAT_EQ(a.y, 2.0f);
}

TEST(Vec2, DivideAssignByZeroIsZero)
{
    math::Vec2 a{3.0f, 4.0f};
    a /= 0.0f;

    EXPECT_FLOAT_EQ(a.x, 0.0f);
    EXPECT_FLOAT_EQ(a.y, 0.0f);
}

TEST(Vec2, DotProduct)
{
    math::Vec2 v1{1.0f, 2.0f};
    math::Vec2 v2{3.0f, 4.0f};
    EXPECT_FLOAT_EQ(math::dot(v1, v2), 11.0f);
}

TEST(Vec2, DotOfPerpendicularVectorsIsZero)
{
    EXPECT_FLOAT_EQ(math::dot(math::Vec2{1.0f, 0.0f}, math::Vec2{0.0f, 1.0f}), 0.0f);
}

TEST(Vec2, DotOfOpposingVectorsIsNegative)
{
    EXPECT_FLOAT_EQ(math::dot(math::Vec2{1.0f, 2.0f}, math::Vec2{-1.0f, -2.0f}), -5.0f);
}

TEST(Vec2, Length)
{
    math::Vec2 v1{3.0f, 4.0f};
    EXPECT_FLOAT_EQ(math::length(v1), 5.0f);
}

TEST(Vec2, LengthSquared)
{
    math::Vec2 v1{3.0f, 4.0f};
    EXPECT_FLOAT_EQ(math::length_squared(v1), 25.0f);
}

TEST(Vec2, Distance)
{
    math::Vec2 v1{1.0f, 2.0f};
    math::Vec2 v2{4.0f, 6.0f};
    EXPECT_FLOAT_EQ(math::distance(v1, v2), 5.0f);
}

TEST(Vec2, Normalize)
{
    math::Vec2 v1{3.0f, 4.0f};
    math::Vec2 normalized = math::normalize(v1);
    EXPECT_FLOAT_EQ(normalized.x, 0.6f);
    EXPECT_FLOAT_EQ(normalized.y, 0.8f);
}

TEST(Vec2, NormalizeZeroLength)
{
    math::Vec2 v1{0.0f, 0.0f};
    math::Vec2 normalized = math::normalize(v1);
    EXPECT_FLOAT_EQ(normalized.x, 0.0f);
    EXPECT_FLOAT_EQ(normalized.y, 0.0f);
}
