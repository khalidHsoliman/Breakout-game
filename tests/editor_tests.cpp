#include <gtest/gtest.h>

#include <cstddef>
#include <string>

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

TEST(Editor, CycleWalksUpToTheCapThenSteelThenBackToEmpty)
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
    EXPECT_EQ(level.at(1, 1), game::STEEL);

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

TEST(EditorCanvas, IsAlwaysTheSameShape)
{
    for (const game::Level& source : {game::Level{}, grid(), game::parse_level("1111111111111\n2222222222222")})
    {
        const game::Level canvas = game::editor_canvas(source);

        EXPECT_EQ(canvas.columns, game::EDITOR_COLUMNS);
        EXPECT_EQ(canvas.rows, game::EDITOR_ROWS);
    }
}

TEST(EditorCanvas, KeepsWhatFitsAndLeavesTheRestEmpty)
{
    const game::Level canvas = game::editor_canvas(grid());   // 4 columns, 3 rows

    EXPECT_EQ(canvas.at(0, 0), 1);
    EXPECT_EQ(canvas.at(3, 0), 4);
    EXPECT_EQ(canvas.at(0, 2), 1);

    EXPECT_EQ(canvas.at(4, 0), 0);   // past the source's last column
    EXPECT_EQ(canvas.at(0, 3), 0);   // past the source's last row
}

TEST(Editor, HasBricksOnlyWhenACellIsNotEmpty)
{
    game::Level canvas = game::editor_canvas(game::Level{});
    EXPECT_FALSE(game::has_bricks(canvas));

    canvas.set(game::EDITOR_COLUMNS - 1, game::EDITOR_ROWS - 1, 1);
    EXPECT_TRUE(game::has_bricks(canvas));

    EXPECT_FALSE(game::has_bricks(game::Level{}));
}

// ---- Steel in the editor ----------------------------------------------------

// From any state, six clicks go once round empty, 1, 2, 3, 4, steel and back.
TEST(Editor, SixClicksGoOnceRoundEveryStateWithSteelAfterFour)
{
    const int states[] = {0, 1, 2, 3, 4, game::STEEL};
    const game::Cell cell{0, 0, true};

    for (std::size_t start = 0; start < 6; start++)
    {
        game::Level level = game::editor_canvas(game::Level{});
        level.set(0, 0, states[start]);

        for (std::size_t click = 1; click <= 6; click++)
        {
            game::cycle_cell(level, cell);
            EXPECT_EQ(level.at(0, 0), states[(start + click) % 6]) << "from " << states[start] << ", click " << click;
        }
    }
}

TEST(Editor, ClickingSteelEmptiesTheCell)
{
    game::Level level = game::parse_level("#");

    game::cycle_cell(level, game::Cell{0, 0, true});

    EXPECT_EQ(level.at(0, 0), 0);
}

// Still to empty, never to steel, as CycleClearsHitPointsAboveTheCap has it for 7.
TEST(Editor, EveryValueAboveTheCapGoesToEmptyNotToSteel)
{
    for (int hit_points = game::MAX_EDITABLE_HIT_POINTS + 1; hit_points <= 9; ++hit_points)
    {
        game::Level level = game::parse_level(std::to_string(hit_points));

        game::cycle_cell(level, game::Cell{0, 0, true});

        EXPECT_EQ(level.at(0, 0), 0) << hit_points;
    }
}

TEST(Editor, RightClickEmptiesASteelCell)
{
    game::Level level = game::parse_level("#1");

    game::clear_cell(level, game::Cell{0, 0, true});

    EXPECT_EQ(level.at(0, 0), 0);
    EXPECT_EQ(level.at(1, 0), 1);
}

TEST(EditorCanvas, KeepsSteel)
{
    const game::Level canvas = game::editor_canvas(game::parse_level("1#\n#2"));

    EXPECT_EQ(canvas.at(1, 0), game::STEEL);
    EXPECT_EQ(canvas.at(0, 1), game::STEEL);
    EXPECT_EQ(canvas.at(1, 1), 2);
}

// Steel alone cannot be cleared, so it is as good as an empty canvas: Enter
// shows NOTHING TO PLAY and S shows NOTHING TO SAVE.
TEST(Editor, OnlySteelIsNotBricks)
{
    game::Level canvas = game::editor_canvas(game::Level{});
    for (int column = 0; column < game::EDITOR_COLUMNS; ++column)
    {
        canvas.set(column, 0, game::STEEL);
    }
    EXPECT_FALSE(game::has_bricks(canvas));

    canvas.set(0, game::EDITOR_ROWS - 1, 1);
    EXPECT_TRUE(game::has_bricks(canvas));
}
