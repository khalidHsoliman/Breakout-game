#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <string_view>

#include "game/Editor.h"
#include "game/LevelCheck.h"
#include "game/Spawn.h"
#include "game/Systems.h"
#include "math/Collision.h"

namespace
{
    constexpr math::Vec2 WORLD{800.0f, 600.0f};

    std::optional<game::LevelProblem> check(std::string_view text)
    {
        return game::check_level_text(text, WORLD);
    }

    // rows lines of row_text, each ending in end_of_line.
    std::string repeat_rows(int rows, const std::string& row_text, const std::string& end_of_line = "\n")
    {
        std::string text;
        for (int i = 0; i < rows; i++)
        {
            text += row_text + end_of_line;
        }
        return text;
    }

    bool contains(const std::string& text, std::string_view part)
    {
        return text.find(part) != std::string::npos;
    }

    // Every brick of the level is clear of the ball parked on the paddle.
    bool held_ball_is_clear_of_the_bricks(const std::string& text)
    {
        game::World world;
        world.size = WORLD;
        game::start_game(world, game::parse_level(text));
        game::step(world, game::Input{}, game::FIXED_DT);   // serve_system parks the ball

        const core::Entity ball = world.balls.entities()[0];
        const math::Circle circle{world.transforms.find(ball)->position, world.balls.find(ball)->radius};

        for (const core::Entity brick : world.bricks.entities())
        {
            const game::Transform& transform = *world.transforms.find(brick);
            const math::Vec2 half = transform.size * 0.5f;
            const math::AABB box{transform.position - half, transform.position + half};

            if (math::circle_vs_aabb(circle, box).hit)
            {
                return false;
            }
        }
        return true;
    }
}

// ---- The limits themselves --------------------------------------------------

// The numbers agreed on, worked out from the game's own constants.
TEST(LevelLimits, AreSeventeenRowsAndFortySevenColumnsInThePlayArea)
{
    EXPECT_EQ(game::max_level_rows(WORLD), 17);
    EXPECT_EQ(game::max_level_columns(WORLD), 47);
}

// Checked against spawned entities, not the arithmetic that produced the limit.
TEST(LevelLimits, TheLowestAllowedRowClearsTheHeldBallAndOneLowerDoesNot)
{
    EXPECT_TRUE(held_ball_is_clear_of_the_bricks(repeat_rows(17, "1")));
    EXPECT_FALSE(held_ball_is_clear_of_the_bricks(repeat_rows(18, "1")));
}

TEST(LevelLimits, TheNarrowestAllowedBrickIsWiderThanOneStepOfTheFastestBall)
{
    const float longest_step = game::MAX_BALL_SPEED * game::FIXED_DT;

    game::Level level;
    level.columns = 47;
    EXPECT_GT(game::brick_size(level, WORLD).x, longest_step);

    level.columns = 48;
    EXPECT_LE(game::brick_size(level, WORLD).x, longest_step);
}

// ---- Rows -------------------------------------------------------------------

TEST(CheckLevelText, SeventeenRowsOfBricksAreFine)
{
    EXPECT_EQ(check(repeat_rows(17, "1111")), std::nullopt);
}

TEST(CheckLevelText, EighteenRowsOfBricksAreNot)
{
    const auto problem = check(repeat_rows(18, ".11."));

    ASSERT_TRUE(problem.has_value());
    EXPECT_EQ(problem->line, 18);
    EXPECT_EQ(problem->column, 2);   // the first brick in the row
    EXPECT_TRUE(contains(problem->message, "row 18")) << problem->message;
    EXPECT_TRUE(contains(problem->message, "17 rows")) << problem->message;
}

// Only rows with bricks count towards the limit.
TEST(CheckLevelText, AnEighteenthRowOfOnlyDotsIsFine)
{
    EXPECT_EQ(check(repeat_rows(17, "1111") + "....\n"), std::nullopt);
}

// Blank lines at the top are not rows, but they do count as lines.
TEST(CheckLevelText, BlankLinesAtTheTopAreNotRowsButAreLines)
{
    EXPECT_EQ(check("\n  \n" + repeat_rows(17, "1")), std::nullopt);

    const auto problem = check("\n  \n" + repeat_rows(18, "1"));
    ASSERT_TRUE(problem.has_value());
    EXPECT_EQ(problem->line, 20);
    EXPECT_EQ(problem->column, 1);
}

// A row of dots at the top or a blank line in the middle is a row, as it is
// in parse_level, so it pushes the bricks below it down.
TEST(CheckLevelText, EmptyRowsAboveBricksStillPushThemDown)
{
    const auto dots_on_top = check("...\n" + repeat_rows(17, "1"));
    ASSERT_TRUE(dots_on_top.has_value());
    EXPECT_EQ(dots_on_top->line, 18);

    const auto gap_in_middle = check(repeat_rows(8, "1") + "\n" + repeat_rows(9, "1"));
    ASSERT_TRUE(gap_in_middle.has_value());
    EXPECT_EQ(gap_in_middle->line, 18);
}

// ---- Columns ----------------------------------------------------------------

TEST(CheckLevelText, FortySevenColumnsAreFine)
{
    EXPECT_EQ(check(std::string(47, '1')), std::nullopt);
}

TEST(CheckLevelText, FortyEightColumnsAreNot)
{
    const auto problem = check("11\n" + std::string(48, '1'));

    ASSERT_TRUE(problem.has_value());
    EXPECT_EQ(problem->line, 2);
    EXPECT_EQ(problem->column, 48);
    EXPECT_TRUE(contains(problem->message, "48 columns")) << problem->message;
    EXPECT_TRUE(contains(problem->message, "47")) << problem->message;
}

TEST(CheckLevelText, TrailingSpacesWidenTheRow)
{
    const auto problem = check("1" + std::string(47, ' '));

    ASSERT_TRUE(problem.has_value());
    EXPECT_EQ(problem->line, 1);
    EXPECT_EQ(problem->column, 48);
}

// parse_level does not count blank lines above or below the grid as rows, so
// their width does not matter either.
TEST(CheckLevelText, BlankLinesAroundTheGridMayBeWider)
{
    const std::string wide_blank(60, ' ');
    EXPECT_EQ(check(wide_blank + "\n111\n" + wide_blank + "\n"), std::nullopt);
}

TEST(CheckLevelText, RaggedRowsAreFine)
{
    EXPECT_EQ(check("111\n1\n11"), std::nullopt);
}

// ---- Characters -------------------------------------------------------------

TEST(CheckLevelText, ABadCharacterAfterBlankLinesIsReportedWhereItIsInTheFile)
{
    const auto problem = check("\n\n11x1\n");

    ASSERT_TRUE(problem.has_value());
    EXPECT_EQ(problem->line, 3);
    EXPECT_EQ(problem->column, 3);
    EXPECT_TRUE(contains(problem->message, "'x'")) << problem->message;
}

TEST(CheckLevelText, ABadCharacterInACrlfFileIsReportedWhereItIsInTheFile)
{
    EXPECT_EQ(check(repeat_rows(3, "1.1", "\r\n")), std::nullopt);

    const auto problem = check("11\r\n1x\r\n");

    ASSERT_TRUE(problem.has_value());
    EXPECT_EQ(problem->line, 2);
    EXPECT_EQ(problem->column, 2);
}

TEST(CheckLevelText, TabsAreNotAllowed)
{
    const auto problem = check("1\t1");

    ASSERT_TRUE(problem.has_value());
    EXPECT_EQ(problem->line, 1);
    EXPECT_EQ(problem->column, 2);
    EXPECT_TRUE(contains(problem->message, "tab")) << problem->message;
}

// Only a CR right before the line break is a line ending.
TEST(CheckLevelText, ACarriageReturnInsideALineIsNotAllowed)
{
    const auto problem = check("1\r1\n");

    ASSERT_TRUE(problem.has_value());
    EXPECT_EQ(problem->column, 2);
    EXPECT_TRUE(contains(problem->message, "carriage return")) << problem->message;
}

TEST(CheckLevelText, ANonAsciiCharacterIsReportedByItsFirstByte)
{
    const auto problem = check("1\xC3\xA9");   // "1é" in UTF-8

    ASSERT_TRUE(problem.has_value());
    EXPECT_EQ(problem->column, 2);
    EXPECT_TRUE(contains(problem->message, "0xC3")) << problem->message;
}

TEST(CheckLevelText, ZeroIsAnEmptyCell)
{
    EXPECT_EQ(check("101"), std::nullopt);
    ASSERT_TRUE(check("000").has_value());
    EXPECT_EQ(check("000")->message, "the level has no bricks");
}

TEST(CheckLevelText, TheFirstProblemInReadingOrderIsReported)
{
    const auto problem = check(std::string(48, '1') + "\n1x\n");

    ASSERT_TRUE(problem.has_value());
    EXPECT_EQ(problem->line, 1);
    EXPECT_EQ(problem->column, 48);
}

// ---- Byte order mark --------------------------------------------------------

TEST(CheckLevelText, ALeadingBomIsSkippedAndIsNotAColumn)
{
    const std::string bom = "\xEF\xBB\xBF";

    EXPECT_EQ(check(bom + "111"), std::nullopt);
    EXPECT_EQ(check(bom + std::string(47, '1')), std::nullopt);

    const auto problem = check(bom + "1x");
    ASSERT_TRUE(problem.has_value());
    EXPECT_EQ(problem->line, 1);
    EXPECT_EQ(problem->column, 2);
}

TEST(CheckLevelText, ABomAnywhereElseIsABadCharacter)
{
    const auto problem = check("11\n\xEF\xBB\xBF" "11");

    ASSERT_TRUE(problem.has_value());
    EXPECT_EQ(problem->line, 2);
    EXPECT_EQ(problem->column, 1);
}

// parse_level is unchanged, so whoever parses must skip the BOM first.
TEST(SkipBom, LeavesWhatParseLevelExpects)
{
    EXPECT_EQ(game::parse_level(game::skip_bom("\xEF\xBB\xBF" "1.2\n.3.")), game::parse_level("1.2\n.3."));
    EXPECT_EQ(game::skip_bom("1.2"), "1.2");
    EXPECT_EQ(game::skip_bom(""), "");
}

// ---- Empty and no bricks ----------------------------------------------------

TEST(CheckLevelText, AnEmptyFileIsAProblem)
{
    EXPECT_EQ(check(""), (game::LevelProblem{0, 0, "the file is empty"}));
    EXPECT_EQ(check("\n  \r\n\n"), (game::LevelProblem{0, 0, "the file is empty"}));
    EXPECT_EQ(check("\xEF\xBB\xBF"), (game::LevelProblem{0, 0, "the file is empty"}));
}

TEST(CheckLevelText, ALevelWithNoBricksIsAProblem)
{
    EXPECT_EQ(check("...\n. .\n"), (game::LevelProblem{0, 0, "the level has no bricks"}));
}

// ---- Levels that must pass --------------------------------------------------

TEST(CheckLevelText, TheBuiltInLevelPasses)
{
    EXPECT_EQ(check(game::BUILT_IN_LEVEL), std::nullopt);
}

TEST(CheckLevelText, AFullEditorCanvasPasses)
{
    game::Level canvas = game::editor_canvas(game::Level{});
    for (int row = 0; row < canvas.rows; ++row)
    {
        for (int column = 0; column < canvas.columns; ++column)
        {
            canvas.set(column, row, game::MAX_EDITABLE_HIT_POINTS);
        }
    }
    ASSERT_EQ(canvas.columns, 10);
    ASSERT_EQ(canvas.rows, 6);

    EXPECT_EQ(check(game::to_text(canvas)), std::nullopt);
}

// The check counts rows and columns itself, so it must agree with what
// parse_level then builds from the same text.
TEST(CheckLevelText, WhatItAcceptsParsesIntoALevelThatFits)
{
    const std::string accepted[] = {
        game::BUILT_IN_LEVEL,
        "\n\n  \n" + repeat_rows(17, "1"),
        repeat_rows(17, "1", "\r\n") + "...\r\n\r\n",
        "\xEF\xBB\xBF" + std::string(47, '1'),
        "111\n\n1" + std::string(46, ' ') + "\n",
    };

    for (const std::string& text : accepted)
    {
        ASSERT_EQ(check(text), std::nullopt) << text;

        const game::Level level = game::parse_level(game::skip_bom(text));
        EXPECT_LE(level.columns, game::max_level_columns(WORLD)) << text;

        bool has_brick = false;
        for (int row = 0; row < level.rows; ++row)
        {
            for (int column = 0; column < level.columns; ++column)
            {
                if (level.at(column, row) > 0)
                {
                    has_brick = true;
                    EXPECT_LT(row, game::max_level_rows(WORLD)) << text;
                }
            }
        }
        EXPECT_TRUE(has_brick) << text;
    }
}
