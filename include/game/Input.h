#pragma once

namespace game
{
    // What the player is asking for, with no idea which keys produced it.
    // main fills this in from Window, so the game never sees a key code.
    struct Input
    {
        // Held, assigned every frame
        bool move_left = false;
        bool move_right = false;

        // One-shot, OR-ed in every frame, cleared by the game when consumed
        bool launch = false;
        bool toggle_pause = false;
        bool restart = false;

        void clear_one_shots()
        {
            launch = false;
            toggle_pause = false;
            restart = false;
        }
    };
}
