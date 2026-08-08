#include <gtest/gtest.h>

#include "game/Editor.h"
#include "game/LevelLayout.h"

namespace
{
    constexpr math::Vec2 WORLD{800.0f, 600.0f};

    game::Level grid()
    {
        return game::parse_level("1234\n....\n1111");   // 4 columns, 3 rows
    }
}

// The centre of a cell must map back to that same cell
TEST(LevelLayout, CellAtRoundTripsWithBrickCenter)
{
    const game::Level level = grid();

    for (int row = 0; row < level.rows; ++row)
    {
        for (int column = 0; column < level.columns; ++column)
        {
            const math::Vec2 center = game::brick_center(level, WORLD, column, row);
            const game::Cell cell = game::cell_at(level, WORLD, center);

            ASSERT_TRUE(cell.inside) << "column " << column << " row " << row;
            EXPECT_EQ(cell.column, column);
            EXPECT_EQ(cell.row, row);
        }
    }
}

TEST(LevelLayout, BrickSizeLeavesAGapBetweenNeighbours)
{
    const game::Level level = grid();

    const math::Vec2 size = game::brick_size(level, WORLD);
    const float left = game::brick_center(level, WORLD, 0, 0).x;
    const float right = game::brick_center(level, WORLD, 1, 0).x;

    EXPECT_LT(size.x, right - left) << "bricks would touch";
}

TEST(LevelLayout, PositionsOutsideTheGridAreNotCells)
{
    const game::Level level = grid();

    EXPECT_FALSE(game::cell_at(level, WORLD, math::Vec2{5.0f, 560.0f}).inside);      // side margin
    EXPECT_FALSE(game::cell_at(level, WORLD, math::Vec2{400.0f, 595.0f}).inside);    // above the top
    EXPECT_FALSE(game::cell_at(level, WORLD, math::Vec2{400.0f, 100.0f}).inside);    // below the grid
    EXPECT_FALSE(game::cell_at(level, WORLD, math::Vec2{795.0f, 560.0f}).inside);    // past the right
}

// The gaps belong to their cell, so there are no dead pixels the editor
// silently ignores.
TEST(LevelLayout, TheGapBetweenBricksStillBelongsToACell)
{
    const game::Level level = grid();

    const float left = game::brick_center(level, WORLD, 0, 0).x;
    const float right = game::brick_center(level, WORLD, 1, 0).x;
    const math::Vec2 between{(left + right) * 0.5f, game::brick_center(level, WORLD, 0, 0).y};

    const game::Cell cell = game::cell_at(level, WORLD, between);
    EXPECT_TRUE(cell.inside);
    EXPECT_EQ(cell.row, 0);
}

TEST(LevelLayout, AnEmptyLevelHasNoCells)
{
    EXPECT_FALSE(game::cell_at(game::Level{}, WORLD, math::Vec2{400.0f, 300.0f}).inside);
}

TEST(Editor, CycleWalksUpToTheCapThenBackToEmpty)
{
    game::Level level = grid();
    const game::Cell cell{1, 1, true};   // an empty cell in the middle row

    ASSERT_EQ(level.at(1, 1), 0);

    for (int expected = 1; expected <= game::MAX_EDITABLE_HIT_POINTS; ++expected)
    {
        game::cycle_cell(level, cell);
        EXPECT_EQ(level.at(1, 1), expected);
    }

    game::cycle_cell(level, cell);
    EXPECT_EQ(level.at(1, 1), 0);
}

// A custom level file may hold 5 to 9, which the game cannot distinguish.
// Cycling one drops it to empty rather than pretending the editor can show it.
TEST(Editor, CycleClearsHitPointsAboveTheCap)
{
    game::Level level = game::parse_level("7");

    game::cycle_cell(level, game::Cell{0, 0, true});

    EXPECT_EQ(level.at(0, 0), 0);
}

TEST(Editor, ClearEmptiesACell)
{
    game::Level level = grid();

    game::clear_cell(level, game::Cell{0, 0, true});

    EXPECT_EQ(level.at(0, 0), 0);
}

TEST(Editor, CellsOutsideTheGridAreIgnored)
{
    game::Level level = grid();
    const game::Level before = level;

    game::cycle_cell(level, game::Cell{0, 0, false});
    game::clear_cell(level, game::Cell{0, 0, false});
    game::cycle_cell(level, game::Cell{99, 0, true});
    game::clear_cell(level, game::Cell{0, 99, true});

    EXPECT_EQ(level, before);
}
