#include <gtest/gtest.h>

#include "game/Level.h"

TEST(ParseLevel, ReadsAGrid)
{
    const game::Level level = game::parse_level("1.2\n.3.");

    EXPECT_EQ(level.columns, 3);
    EXPECT_EQ(level.rows, 2);
    EXPECT_EQ(level.at(0, 0), 1);
    EXPECT_EQ(level.at(1, 0), 0);
    EXPECT_EQ(level.at(2, 0), 2);
    EXPECT_EQ(level.at(1, 1), 3);
}

// A level saved by the editor on Windows will have CRLF. Treating the carriage
// return as a cell would give every row one phantom column.
TEST(ParseLevel, HandlesWindowsLineEndings)
{
    const game::Level level = game::parse_level("1.2\r\n.3.\r\n");

    EXPECT_EQ(level.columns, 3);
    EXPECT_EQ(level.rows, 2);
    EXPECT_EQ(level.at(2, 0), 2);
}

// So a raw string literal can start on the line after R"( and end before )".
TEST(ParseLevel, IgnoresLeadingAndTrailingBlankLines)
{
    const game::Level level = game::parse_level("\n\n11\n22\n\n");

    EXPECT_EQ(level.rows, 2);
    EXPECT_EQ(level.columns, 2);
    EXPECT_EQ(level.at(0, 0), 1);
    EXPECT_EQ(level.at(0, 1), 2);
}

TEST(ParseLevel, KeepsBlankRowsInTheMiddle)
{
    const game::Level level = game::parse_level("11\n\n11");

    EXPECT_EQ(level.rows, 3);
    EXPECT_EQ(level.at(0, 1), 0);
    EXPECT_EQ(level.at(0, 2), 1);
}

// A hand-edited file often has trailing spaces stripped, so short rows must
// pad rather than be rejected.
TEST(ParseLevel, PadsRaggedRows)
{
    const game::Level level = game::parse_level("111\n1");

    EXPECT_EQ(level.columns, 3);
    EXPECT_EQ(level.rows, 2);
    EXPECT_EQ(level.at(0, 1), 1);
    EXPECT_EQ(level.at(1, 1), 0);
    EXPECT_EQ(level.at(2, 1), 0);
}

TEST(ParseLevel, TreatsUnknownCharactersAsEmpty)
{
    const game::Level level = game::parse_level("1x2");

    EXPECT_EQ(level.at(0, 0), 1);
    EXPECT_EQ(level.at(1, 0), 0);
    EXPECT_EQ(level.at(2, 0), 2);
}

TEST(ParseLevel, ZeroIsEmpty)
{
    const game::Level level = game::parse_level("101");

    EXPECT_EQ(level.at(1, 0), 0);
}

TEST(ParseLevel, EmptyInputGivesAnEmptyLevel)
{
    const game::Level level = game::parse_level("");

    EXPECT_EQ(level.columns, 0);
    EXPECT_EQ(level.rows, 0);
    EXPECT_TRUE(level.hit_points.empty());
}

TEST(ParseLevel, ReadsAllNineToughnessLevels)
{
    const game::Level level = game::parse_level("123456789");

    EXPECT_EQ(level.columns, 9);
    for (int column = 0; column < 9; ++column)
    {
        EXPECT_EQ(level.at(column, 0), column + 1);
    }
}

TEST(Level, AtIsSafeOutsideTheGrid)
{
    const game::Level level = game::parse_level("11\n11");

    EXPECT_EQ(level.at(-1, 0), 0);
    EXPECT_EQ(level.at(0, -1), 0);
    EXPECT_EQ(level.at(2, 0), 0);
    EXPECT_EQ(level.at(0, 2), 0);
}
