#pragma once

#include <cmath>

namespace math
{
    struct Vec2
    {
        float x = 0.0f, y = 0.0f;
    };

    // Unary operators
    inline Vec2 operator-(Vec2 a)
    {
        return Vec2{-a.x, -a.y};
    }

    // Binary operators
    inline Vec2 operator+(Vec2 a, Vec2 b)
    {
        return Vec2{a.x + b.x, a.y + b.y};
    }

    inline Vec2 operator-(Vec2 a, Vec2 b)
    {
        return Vec2{a.x - b.x, a.y - b.y};
    }

    inline Vec2 operator*(Vec2 a, float b)
    {
        return Vec2{a.x * b, a.y * b};
    }

    inline Vec2 operator*(float a, Vec2 b)
    {
        return Vec2{a * b.x, a * b.y};
    }

    inline Vec2 operator/(Vec2 a, float b)
    {
        if (b == 0.0f)
        {
            return Vec2{0.0f, 0.0f};
        }
        return Vec2{a.x / b, a.y / b};
    }

    // Assignment operators
    inline Vec2& operator+=(Vec2& a, Vec2 b)
    {
        a.x += b.x;
        a.y += b.y;
        return a;
    }

    inline Vec2& operator-=(Vec2& a, Vec2 b)
    {
        a.x -= b.x;
        a.y -= b.y;
        return a;
    }

    inline Vec2& operator*=(Vec2& a, float b)
    {
        a.x *= b;
        a.y *= b;
        return a;
    }

    inline Vec2& operator/=(Vec2& a, float b)
    {
        a = a / b;
        return a;
    }

    // Utility functions
    inline float length_squared(Vec2 a)
    {
        return a.x * a.x + a.y * a.y;
    }

    inline float length(Vec2 a)
    {
        return std::sqrt(length_squared(a));
    }

    inline float distance(Vec2 a, Vec2 b)
    {
        return length(a - b);
    }

    inline Vec2 normalize(Vec2 a)
    {
        const float len = length(a);
        if (len == 0.0f)
        {
            return Vec2{0.0f, 0.0f};
        }
        return a / len;
    }

    inline float dot(Vec2 a, Vec2 b)
    {
        return a.x * b.x + a.y * b.y;
    }
}
