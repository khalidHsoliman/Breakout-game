#pragma once

namespace game
{
    // What the player is asking for, with no idea which keys produced it.
    // main fills this in from Window, so the game never sees a key code.
    struct Input
    {
        bool move_left = false;
        bool move_right = false;
    };
}
