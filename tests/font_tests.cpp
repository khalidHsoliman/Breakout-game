#include <gtest/gtest.h>

#include <string_view>

#include "platform/Font.h"

namespace
{
    bool is_blank(const platform::Glyph& glyph)
    {
        for (const std::uint8_t row : glyph)
        {
            if (row != 0)
            {
                return false;
            }
        }
        return true;
    }

    // Every string the game puts on screen.
    constexpr const char* MESSAGES[] = {
        "SCORE 0123456789",
        "LIVES 3",
        "PRESS SPACE TO LAUNCH",
        "PAUSED",
        "GAME OVER",
        "YOU WIN",
        "PRESS R TO RESTART",
    };
}

TEST(Font, DigitsAndLettersHaveGlyphs)
{
    for (char character = '0'; character <= '9'; ++character)
    {
        EXPECT_FALSE(is_blank(platform::glyph_for(character))) << "digit " << character;
    }
    for (char character = 'A'; character <= 'Z'; ++character)
    {
        EXPECT_FALSE(is_blank(platform::glyph_for(character))) << "letter " << character;
    }
}

// A missing glyph would show as a gap on screen rather than a crash, so this
// checks the thing the eye would have to catch.
TEST(Font, EveryCharacterTheGameDisplaysCanBeDrawn)
{
    for (const std::string_view message : MESSAGES)
    {
        for (const char character : message)
        {
            if (character == ' ')
            {
                continue;
            }
            EXPECT_FALSE(is_blank(platform::glyph_for(character)))
                << "character '" << character << "' in \"" << message << '"';
        }
    }
}

TEST(Font, SpaceAndUnknownCharactersAreBlank)
{
    EXPECT_TRUE(is_blank(platform::glyph_for(' ')));
    EXPECT_TRUE(is_blank(platform::glyph_for('!')));
    EXPECT_TRUE(is_blank(platform::glyph_for('\n')));
}

// Five pixels wide has no room for a second alphabet, so lower case folds.
TEST(Font, LowerCaseFoldsToUpper)
{
    EXPECT_EQ(platform::glyph_for('a'), platform::glyph_for('A'));
    EXPECT_EQ(platform::glyph_for('z'), platform::glyph_for('Z'));
}

// Nothing may set bit 5 or above: the glyph is five columns wide, and a stray
// bit would draw a pixel outside the character's own cell.
TEST(Font, NoGlyphUsesMoreThanFiveColumns)
{
    for (char character = '0'; character <= '9'; ++character)
    {
        for (const std::uint8_t row : platform::glyph_for(character))
        {
            EXPECT_LT(row, 1u << platform::GLYPH_WIDTH) << "digit " << character;
        }
    }
    for (char character = 'A'; character <= 'Z'; ++character)
    {
        for (const std::uint8_t row : platform::glyph_for(character))
        {
            EXPECT_LT(row, 1u << platform::GLYPH_WIDTH) << "letter " << character;
        }
    }
}

TEST(Font, TextWidthIsEmptyForEmptyText)
{
    EXPECT_FLOAT_EQ(platform::text_width("", 3.0f), 0.0f);
}

TEST(Font, TextWidthIsTheGlyphWidthForOneCharacter)
{
    EXPECT_FLOAT_EQ(platform::text_width("A", 3.0f), 15.0f);
}

// Two glyphs plus one column of spacing between them, not after.
TEST(Font, TextWidthAddsSpacingBetweenCharactersOnly)
{
    EXPECT_FLOAT_EQ(platform::text_width("AB", 3.0f), 33.0f);
    EXPECT_FLOAT_EQ(platform::text_width("ABC", 3.0f), 51.0f);
}

TEST(Font, TextWidthScalesWithPixelSize)
{
    EXPECT_FLOAT_EQ(platform::text_width("HELLO", 2.0f) * 2.0f,
                    platform::text_width("HELLO", 4.0f));
}

TEST(Font, TextHeightIsSevenPixels)
{
    EXPECT_FLOAT_EQ(platform::text_height(3.0f), 21.0f);
}
