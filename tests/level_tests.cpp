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

// Only 1 to 9 mean anything, so a zero is as empty as a letter or a dot.
TEST(ParseLevel, AnythingOutsideOneToNineIsEmpty)
{
    const game::Level level = game::parse_level("1x2.0");

    EXPECT_EQ(level.at(0, 0), 1);
    EXPECT_EQ(level.at(1, 0), 0);
    EXPECT_EQ(level.at(2, 0), 2);
    EXPECT_EQ(level.at(3, 0), 0);
    EXPECT_EQ(level.at(4, 0), 0);
}

TEST(ParseLevel, EmptyInputGivesAnEmptyLevel)
{
    const game::Level level = game::parse_level("");

    EXPECT_EQ(level.columns, 0);
    EXPECT_EQ(level.rows, 0);
    EXPECT_TRUE(level.hit_points.empty());
}

TEST(ToText, AnEmptyLevelIsEmptyText)
{
    EXPECT_EQ(game::to_text(game::Level{}), "");
}

TEST(ToText, WritesDotsForEmptyCellsAndDigitsOtherwise)
{
    EXPECT_EQ(game::to_text(game::parse_level("1.2")), "1.2\n");
}

TEST(ToText, WritesOneLinePerRow)
{
    EXPECT_EQ(game::to_text(game::parse_level("12\n34")), "12\n34\n");
}

TEST(ToText, PadsShortRowsToTheFullWidth)
{
    EXPECT_EQ(game::to_text(game::parse_level("111\n1")), "111\n1..\n");
}

TEST(ToText, RoundTripsThroughTheParser)
{
    const char* sources[] = {
        "1",
        "1.2\n.3.",
        "..111111..\n.12222221.\n1233333321\n.12222221.\n..111111..",
        "123456789",
        ".....\n.....",
    };

    for (const char* source : sources)
    {
        const game::Level original = game::parse_level(source);
        EXPECT_EQ(game::parse_level(game::to_text(original)), original) << source;
    }
}

TEST(ToText, ClampsHitPointsToASingleDigit)
{
    game::Level level;
    level.columns = 2;
    level.rows = 1;
    level.hit_points = {12, -3};

    EXPECT_EQ(game::to_text(level), "9.\n");
}

TEST(Level, AtIsSafeOutsideTheGrid)
{
    const game::Level level = game::parse_level("11\n11");

    EXPECT_EQ(level.at(-1, 0), 0);
    EXPECT_EQ(level.at(0, -1), 0);
    EXPECT_EQ(level.at(2, 0), 0);
    EXPECT_EQ(level.at(0, 2), 0);
}
