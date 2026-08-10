#include <gtest/gtest.h>

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

TEST(Font, SpaceAndUnknownCharactersAreBlank)
{
    EXPECT_TRUE(is_blank(platform::glyph_for(' ')));
    EXPECT_TRUE(is_blank(platform::glyph_for('!')));
    EXPECT_TRUE(is_blank(platform::glyph_for('\n')));
}

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

// Five columns per glyph and one of spacing between them, not after, all
// scaled by the pixel size.
TEST(Font, TextWidthCountsGlyphsAndTheGapsBetweenThem)
{
    EXPECT_FLOAT_EQ(platform::text_width("A", 3.0f), 15.0f);
    EXPECT_FLOAT_EQ(platform::text_width("AB", 3.0f), 33.0f);
    EXPECT_FLOAT_EQ(platform::text_width("ABC", 3.0f), 51.0f);

    EXPECT_FLOAT_EQ(platform::text_width("ABC", 6.0f), 102.0f);
}

TEST(Font, TextHeightIsSevenPixels)
{
    EXPECT_FLOAT_EQ(platform::text_height(3.0f), 21.0f);
}
