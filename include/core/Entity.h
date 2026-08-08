#pragma once

#include <cstdint>

namespace core
{
    struct Entity
    {
        std::uint32_t id = 0;

        bool operator==(const Entity&) const = default;
    };

    // Reserved to mean "no entity". World never hands this out.
    inline constexpr Entity NULL_ENTITY{0};
}
