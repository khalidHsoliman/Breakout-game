#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "game/Editor.h"
#include "game/LevelFiles.h"

namespace
{
    namespace fs = std::filesystem;

    // Every test gets a folder of its own under the system temp directory,
    // removed again afterwards whether the test passed or not.
    class LevelFiles : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            const std::string test_name = ::testing::UnitTest::GetInstance()->current_test_info()->name();
            const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();

            root = fs::temp_directory_path() / ("breakout-level-files-" + test_name + "-" + std::to_string(stamp));
            folder = root / "levels";
            fs::create_directories(folder);
        }

        void TearDown() override
        {
            std::error_code ignored;
            fs::remove_all(root, ignored);
        }

        // Binary, so the bytes on disk are exactly the bytes given.
        void write(int number, std::string_view contents) const
        {
            std::ofstream file(folder / game::level_file_name(number), std::ios::binary);
            file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
            ASSERT_TRUE(file.good());
        }

        fs::path root;
        fs::path folder;
    };

    const game::Level& built_in()
    {
        static const game::Level level = game::parse_level(game::BUILT_IN_LEVEL);
        return level;
    }

    constexpr const char* DIAMOND = "..1..\n.222.\n33333\n.222.\n..1..\n";
}

TEST(LevelFileName, CountsFromOne)
{
    EXPECT_EQ(game::level_file_name(1), "level-1.txt");
    EXPECT_EQ(game::level_file_name(12), "level-12.txt");
}

TEST_F(LevelFiles, ThreeGoodFilesLoadInOrder)
{
    write(1, "1\n");
    write(2, "11\n");
    write(3, "111\n");

    const game::LoadedLevels loaded = game::load_levels(folder);

    EXPECT_TRUE(loaded.errors.empty());
    ASSERT_EQ(loaded.levels.size(), 3u);
    EXPECT_EQ(loaded.levels[0], game::parse_level("1"));
    EXPECT_EQ(loaded.levels[1], game::parse_level("11"));
    EXPECT_EQ(loaded.levels[2], game::parse_level("111"));
}

// By number, not by name: level-10.txt sorts before level-2.txt as text.
TEST_F(LevelFiles, TenAndUpComeAfterNine)
{
    for (int number = 1; number <= 11; number++)
    {
        write(number, std::string(static_cast<std::size_t>(number), '1'));
    }

    const game::LoadedLevels loaded = game::load_levels(folder);

    ASSERT_EQ(loaded.levels.size(), 11u);
    EXPECT_EQ(loaded.levels[9].columns, 10);
    EXPECT_EQ(loaded.levels[10].columns, 11);
}

TEST_F(LevelFiles, ABadLevelTwoIsSkippedAndTheOthersStillLoad)
{
    write(1, "1\n");
    write(2, "11\n\n1x1\n");
    write(3, "111\n");

    const game::LoadedLevels loaded = game::load_levels(folder);

    ASSERT_EQ(loaded.levels.size(), 2u);
    EXPECT_EQ(loaded.levels[0], game::parse_level("1"));
    EXPECT_EQ(loaded.levels[1], game::parse_level("111"));

    ASSERT_EQ(loaded.errors.size(), 1u);
    EXPECT_EQ(loaded.errors[0],
              "level-2.txt:3:2: 'x' is not allowed: a level may only contain digits, '.' and spaces");
}

TEST_F(LevelFiles, AProblemWithTheWholeFileHasNoLineOrColumn)
{
    write(1, "1\n");
    write(2, "");
    write(3, "...\n");

    const game::LoadedLevels loaded = game::load_levels(folder);

    ASSERT_EQ(loaded.levels.size(), 1u);
    EXPECT_EQ(loaded.errors, (std::vector<std::string>{
                                 "level-2.txt: the file is empty",
                                 "level-3.txt: the level has no bricks",
                             }));
}

TEST_F(LevelFiles, AGapEndsTheList)
{
    write(1, "1\n");
    write(3, "111\n");

    const game::LoadedLevels loaded = game::load_levels(folder);

    EXPECT_TRUE(loaded.errors.empty());
    ASSERT_EQ(loaded.levels.size(), 1u);
    EXPECT_EQ(loaded.levels[0], game::parse_level("1"));
}

TEST_F(LevelFiles, AMissingFolderFallsBackToTheBuiltInLevel)
{
    const game::LoadedLevels loaded = game::load_levels(folder / "not-there");

    EXPECT_TRUE(loaded.errors.empty());
    ASSERT_EQ(loaded.levels.size(), 1u);
    EXPECT_EQ(loaded.levels[0], built_in());
}

TEST_F(LevelFiles, AnEmptyFolderFallsBackToTheBuiltInLevel)
{
    const game::LoadedLevels loaded = game::load_levels(folder);

    EXPECT_TRUE(loaded.errors.empty());
    ASSERT_EQ(loaded.levels.size(), 1u);
    EXPECT_EQ(loaded.levels[0], built_in());
}

// The errors still come back, so the player finds out why.
TEST_F(LevelFiles, OnlyBadFilesFallBackToTheBuiltInLevelAndReportEachOne)
{
    write(1, "1\t1\n");
    write(2, std::string(48, '1'));

    const game::LoadedLevels loaded = game::load_levels(folder);

    ASSERT_EQ(loaded.levels.size(), 1u);
    EXPECT_EQ(loaded.levels[0], built_in());
    ASSERT_EQ(loaded.errors.size(), 2u);
    EXPECT_EQ(loaded.errors[0].rfind("level-1.txt:1:2: a tab", 0), 0u) << loaded.errors[0];
    EXPECT_EQ(loaded.errors[1].rfind("level-2.txt:1:48: ", 0), 0u) << loaded.errors[1];
}

TEST_F(LevelFiles, ABomLoadsExactlyLikeTheSameLevelWithout)
{
    write(1, DIAMOND);
    write(2, std::string("\xEF\xBB\xBF") + DIAMOND);

    const game::LoadedLevels loaded = game::load_levels(folder);

    EXPECT_TRUE(loaded.errors.empty());
    ASSERT_EQ(loaded.levels.size(), 2u);
    EXPECT_EQ(loaded.levels[1], loaded.levels[0]);
    EXPECT_EQ(loaded.levels[0], game::parse_level(DIAMOND));
}

TEST_F(LevelFiles, CrlfLoadsExactlyLikeTheSameLevelWithLf)
{
    write(1, DIAMOND);
    write(2, "..1..\r\n.222.\r\n33333\r\n.222.\r\n..1..\r\n");

    const game::LoadedLevels loaded = game::load_levels(folder);

    EXPECT_TRUE(loaded.errors.empty());
    ASSERT_EQ(loaded.levels.size(), 2u);
    EXPECT_EQ(loaded.levels[1], loaded.levels[0]);
}

// Something that exists under the name but cannot be read is reported, not
// taken for the end of the list.
TEST_F(LevelFiles, AFolderNamedLikeALevelIsReportedAndSkipped)
{
    write(1, "1\n");
    fs::create_directory(folder / game::level_file_name(2));
    write(3, "111\n");

    const game::LoadedLevels loaded = game::load_levels(folder);

    ASSERT_EQ(loaded.levels.size(), 2u);
    EXPECT_EQ(loaded.levels[1], game::parse_level("111"));
    EXPECT_EQ(loaded.errors, (std::vector<std::string>{"level-2.txt: it is not a file that can be read"}));
}

TEST_F(LevelFiles, AFileWithoutReadPermissionIsReportedAndSkipped)
{
#ifdef _WIN32
    GTEST_SKIP() << "Windows has no owner read permission to take away";
#else
    write(1, "1\n");
    write(2, "11\n");
    write(3, "111\n");
    fs::permissions(folder / game::level_file_name(2), fs::perms::none);

    // Root reads it anyway, so there is nothing to test.
    if (std::ifstream(folder / game::level_file_name(2)).good())
    {
        GTEST_SKIP() << "running as a user who can read any file";
    }

    const game::LoadedLevels loaded = game::load_levels(folder);

    ASSERT_EQ(loaded.levels.size(), 2u);
    EXPECT_EQ(loaded.levels[1], game::parse_level("111"));
    EXPECT_EQ(loaded.errors, (std::vector<std::string>{"level-2.txt: the file could not be opened"}));
#endif
}

// ---- The editor's slots -----------------------------------------------------

namespace
{
    // The same temp folder handling; slots are level files.
    class EditorSlots : public LevelFiles
    {
    protected:
        std::string read_back(int slot) const
        {
            std::ifstream file(folder / game::level_file_name(slot), std::ios::binary);
            return std::string{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        }
    };

    // Every toughness the editor can make, and the corners, so an off-by-one
    // in either direction shows.
    game::Level sample_canvas()
    {
        game::Level canvas = game::editor_canvas(game::Level{});
        canvas.set(0, 0, 1);
        canvas.set(game::EDITOR_COLUMNS - 1, 0, 2);
        canvas.set(0, game::EDITOR_ROWS - 1, 3);
        canvas.set(game::EDITOR_COLUMNS - 1, game::EDITOR_ROWS - 1, 4);
        canvas.set(4, 2, 4);
        return canvas;
    }

    // The status goes through the bitmap font, which has no lower case or
    // punctuation.
    bool font_can_draw(const std::string& status)
    {
        for (const char character : status)
        {
            const bool ok = (character >= 'A' && character <= 'Z') ||
                            (character >= '0' && character <= '9') || character == ' ';
            if (!ok)
            {
                return false;
            }
        }
        return !status.empty();
    }
}

TEST(StepSlot, MovesOneAtATimeAndStopsAtTheEnds)
{
    EXPECT_EQ(game::step_slot(3, 1), 4);
    EXPECT_EQ(game::step_slot(3, -1), 2);
    EXPECT_EQ(game::step_slot(game::FIRST_SLOT, -1), 1);
    EXPECT_EQ(game::step_slot(game::LAST_SLOT, 1), 9);
}

TEST_F(EditorSlots, SaveThenLoadGivesTheSameCanvas)
{
    const game::Level canvas = sample_canvas();

    const game::SlotResult saved = game::save_slot(folder, 3, canvas);
    EXPECT_TRUE(saved.ok) << saved.error;
    EXPECT_EQ(saved.status, "SAVED TO SLOT 3");
    EXPECT_TRUE(saved.error.empty());
    EXPECT_EQ(read_back(3), game::to_text(canvas));

    const game::SlotResult loaded = game::load_slot(folder, 3);
    EXPECT_TRUE(loaded.ok) << loaded.error;
    EXPECT_EQ(loaded.status, "LOADED SLOT 3");
    EXPECT_EQ(loaded.level, canvas);
}

// What the editor saves, the game plays.
TEST_F(EditorSlots, ASavedSlotIsALevelOfTheGame)
{
    ASSERT_TRUE(game::save_slot(folder, 1, sample_canvas()).ok);

    const game::LoadedLevels loaded = game::load_levels(folder);

    EXPECT_TRUE(loaded.errors.empty());
    ASSERT_EQ(loaded.levels.size(), 1u);
    EXPECT_EQ(loaded.levels[0], sample_canvas());
}

TEST_F(EditorSlots, SavingCreatesTheFolder)
{
    const fs::path missing = root / "not-yet" / "levels";
    ASSERT_FALSE(fs::exists(missing));

    const game::SlotResult saved = game::save_slot(missing, 2, sample_canvas());

    EXPECT_TRUE(saved.ok) << saved.error;
    EXPECT_TRUE(fs::is_regular_file(missing / "level-2.txt"));
}

TEST_F(EditorSlots, AnEmptyCanvasIsNotSaved)
{
    const fs::path missing = root / "not-yet";
    const game::SlotResult refused = game::save_slot(missing, 2, game::editor_canvas(game::Level{}));

    EXPECT_FALSE(refused.ok);
    EXPECT_EQ(refused.status, "NOTHING TO SAVE");
    EXPECT_FALSE(fs::exists(missing)) << "not even the folder";

    // And a level already in the slot is left alone.
    write(2, "1\n");
    EXPECT_FALSE(game::save_slot(folder, 2, game::editor_canvas(game::Level{})).ok);
    EXPECT_EQ(read_back(2), "1\n");
}

TEST_F(EditorSlots, ABadFileIsRefusedOnLoadWithTheFullErrorForStderr)
{
    write(4, "11\n1x\n");

    const game::SlotResult refused = game::load_slot(folder, 4);

    EXPECT_FALSE(refused.ok);
    EXPECT_EQ(refused.status, "BAD LEVEL FILE");
    EXPECT_EQ(refused.error, "level-4.txt:2:2: 'x' is not allowed: a level may only contain digits, '.' and spaces");
}

TEST_F(EditorSlots, AFileBiggerThanTheEditorIsRefusedNotCutDown)
{
    write(1, "11111111111\n");         // 11 columns
    write(2, "1\n\n\n\n\n\n1\n");     // 7 rows

    const game::SlotResult too_wide = game::load_slot(folder, 1);
    EXPECT_FALSE(too_wide.ok);
    EXPECT_EQ(too_wide.status, "TOO BIG TO EDIT");
    EXPECT_EQ(too_wide.error,
              "level-1.txt: the level is 11 by 1 cells (columns by rows), but the editor holds at most 10 by 6");

    const game::SlotResult too_tall = game::load_slot(folder, 2);
    EXPECT_FALSE(too_tall.ok);
    EXPECT_EQ(too_tall.status, "TOO BIG TO EDIT");
}

TEST_F(EditorSlots, ExactlyTheEditorsSizeLoads)
{
    write(1, "1111111111\n1111111111\n1111111111\n1111111111\n1111111111\n1111111111\n");

    const game::SlotResult loaded = game::load_slot(folder, 1);

    EXPECT_TRUE(loaded.ok) << loaded.error;
    EXPECT_EQ(loaded.level.columns, 10);
    EXPECT_EQ(loaded.level.rows, 6);
}

// A smaller level fills the top left of the canvas, the rest empty.
TEST_F(EditorSlots, ASmallerHandWrittenFileLoadsIntoTheCanvas)
{
    write(1, "\xEF\xBB\xBF" "1.2\r\n.3.\r\n");   // with Notepad's BOM and CRLF

    const game::SlotResult loaded = game::load_slot(folder, 1);

    ASSERT_TRUE(loaded.ok) << loaded.error;
    EXPECT_EQ(loaded.level, game::editor_canvas(game::parse_level("1.2\n.3.")));
}

TEST_F(EditorSlots, AMissingSlotFileSaysSo)
{
    const game::SlotResult missing = game::load_slot(folder, 5);

    EXPECT_FALSE(missing.ok);
    EXPECT_EQ(missing.status, "NOTHING SAVED IN SLOT 5");
    EXPECT_TRUE(missing.error.empty());
}

TEST_F(EditorSlots, FailingToWriteIsReported)
{
    // A file where the folder should be: the folder cannot be created.
    const fs::path in_the_way = root / "in-the-way";
    std::ofstream(in_the_way) << "x";

    const game::SlotResult failed = game::save_slot(in_the_way, 1, sample_canvas());

    EXPECT_FALSE(failed.ok);
    EXPECT_EQ(failed.status, "COULD NOT SAVE");
    EXPECT_EQ(failed.error.rfind("level-1.txt: could not create the levels folder: ", 0), 0u) << failed.error;
}

TEST_F(EditorSlots, EveryStatusIsSomethingTheFontCanDraw)
{
    write(4, "1x\n");
    write(5, "11111111111\n");
    fs::create_directory(folder / game::level_file_name(6));
    const fs::path in_the_way = root / "in-the-way";
    std::ofstream(in_the_way) << "x";

    const game::SlotResult results[] = {
        game::save_slot(folder, 3, sample_canvas()),
        game::load_slot(folder, 3),
        game::save_slot(folder, 3, game::editor_canvas(game::Level{})),
        game::save_slot(in_the_way, 3, sample_canvas()),
        game::load_slot(folder, 4),
        game::load_slot(folder, 5),
        game::load_slot(folder, 6),
        game::load_slot(folder, 7),
    };

    for (const game::SlotResult& result : results)
    {
        EXPECT_TRUE(font_can_draw(result.status)) << '"' << result.status << '"';
    }
}
