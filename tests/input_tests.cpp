#include <gtest/gtest.h>

#include "game/Input.h"

TEST(Input, DefaultsToNothingPressed)
{
    const game::Input input;

    EXPECT_FALSE(input.move_left);
    EXPECT_FALSE(input.move_right);
    EXPECT_FALSE(input.launch);
    EXPECT_FALSE(input.toggle_pause);
    EXPECT_FALSE(input.restart);
}

TEST(Input, ClearOneShotsClearsEveryAction)
{
    game::Input input;
    input.launch = true;
    input.toggle_pause = true;
    input.restart = true;

    input.clear_one_shots();

    EXPECT_FALSE(input.launch);
    EXPECT_FALSE(input.toggle_pause);
    EXPECT_FALSE(input.restart);
}

TEST(Input, ClearOneShotsLeavesHeldStateAlone)
{
    game::Input input;
    input.move_left = true;
    input.move_right = true;
    input.launch = true;

    input.clear_one_shots();

    EXPECT_TRUE(input.move_left);
    EXPECT_TRUE(input.move_right);
    EXPECT_FALSE(input.launch);
}
