#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace platform
{
    // A 5x7 bitmap font, drawn one quad per lit pixel through the renderer that
    // already exists. No font library, no texture, no asset file.
    //
    // Each glyph is seven rows. Row 0 is the top. Within a row the leftmost
    // column is bit 4 (0b10000), so the binary literals below are pictures of
    // the letters, rotated ninety degrees in your head.
    inline constexpr int glyph_width = 5;
    inline constexpr int glyph_height = 7;
    inline constexpr int glyph_spacing = 1;

    using Glyph = std::array<std::uint8_t, glyph_height>;

    inline constexpr std::array<Glyph, 10> digit_glyphs = {{
        {0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110},   // 0
        {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110},   // 1
        {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111},   // 2
        {0b11111, 0b00010, 0b00100, 0b00010, 0b00001, 0b10001, 0b01110},   // 3
        {0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010},   // 4
        {0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110},   // 5
        {0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110},   // 6
        {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000},   // 7
        {0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110},   // 8
        {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b01100},   // 9
    }};

    inline constexpr std::array<Glyph, 26> letter_glyphs = {{
        {0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001},   // A
        {0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110},   // B
        {0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110},   // C
        {0b11110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11110},   // D
        {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111},   // E
        {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000},   // F
        {0b01110, 0b10001, 0b10000, 0b10011, 0b10001, 0b10001, 0b01110},   // G
        {0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001},   // H
        {0b01110, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110},   // I
        {0b00001, 0b00001, 0b00001, 0b00001, 0b10001, 0b10001, 0b01110},   // J
        {0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001},   // K
        {0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111},   // L
        {0b10001, 0b11011, 0b10101, 0b10001, 0b10001, 0b10001, 0b10001},   // M
        {0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001, 0b10001},   // N
        {0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110},   // O
        {0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000},   // P
        {0b01110, 0b10001, 0b10001, 0b10001, 0b10101, 0b10010, 0b01101},   // Q
        {0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001},   // R
        {0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110},   // S
        {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100},   // T
        {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110},   // U
        {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100},   // V
        {0b10001, 0b10001, 0b10001, 0b10001, 0b10101, 0b11011, 0b10001},   // W
        {0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001},   // X
        {0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100},   // Y
        {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b11111},   // Z
    }};

    // Unknown characters, including space, are blank. Lower case is folded to upper
    inline Glyph glyph_for(char character)
    {
        if (character >= '0' && character <= '9')
        {
            return digit_glyphs[static_cast<std::size_t>(character - '0')];
        }
        if (character >= 'A' && character <= 'Z')
        {
            return letter_glyphs[static_cast<std::size_t>(character - 'A')];
        }
        if (character >= 'a' && character <= 'z')
        {
            return letter_glyphs[static_cast<std::size_t>(character - 'a')];
        }
        return Glyph{};
    }

    // In world units. Callers centre text by subtracting half of this - two
    inline float text_width(std::string_view text, float pixel_size)
    {
        if (text.empty())
        {
            return 0.0f;
        }

        const int spans = static_cast<int>(text.size()) * (glyph_width + glyph_spacing) - glyph_spacing;
        return static_cast<float>(spans) * pixel_size;
    }

    inline float text_height(float pixel_size)
    {
        return static_cast<float>(glyph_height) * pixel_size;
    }
}
