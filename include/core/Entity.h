#pragma once

#include <cstdint>

namespace core
{
    struct Entity
    {
        std::uint32_t id = 0;
    };

    // Reserved to mean "no entity". World never hands this out.
    inline constexpr Entity null_entity{0};

    // C++20 synthesises operator!= from this.
    inline bool operator==(Entity a, Entity b)
    {
        return a.id == b.id;
    }
}
