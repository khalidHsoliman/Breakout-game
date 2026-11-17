#pragma once

namespace game
{
    enum class GameEvent
    {
        BrickHit,
        SteelHit,
        PaddleBounce,

        // A beam hit a brick that can be broken. A beam that stops at steel
        // raises SteelHit.
        LaserHit
    };
}
