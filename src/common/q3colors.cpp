#include "common/q3colors.hpp"

const color_t Q3_COLOR_TABLE[Q3_COLOR_TABLE_SIZE] = {
    ColorRGB(0, 0, 0),
    ColorRGB(255, 0, 0),
    ColorRGB(0, 255, 0),
    ColorRGB(255, 255, 0),
    ColorRGB(0, 0, 255),
    ColorRGB(0, 255, 255),
    ColorRGB(255, 0, 255),
    ColorRGB(255, 255, 255),
    ColorRGB(255, 128, 0),
    ColorRGB(128, 128, 128),
    ColorRGB(191, 191, 191),
    ColorRGB(191, 128, 64),
    ColorRGB(128, 191, 64),
    ColorRGB(191, 191, 0),
    ColorRGB(128, 128, 0),
    ColorRGB(128, 0, 0),
    ColorRGB(128, 64, 64),
    ColorRGB(191, 191, 191),
    ColorRGB(0, 128, 128),
    ColorRGB(64, 128, 64),
    ColorRGB(128, 128, 128),
    ColorRGB(0, 128, 255),
    ColorRGB(128, 0, 255),
    ColorRGB(64, 64, 64),
    ColorRGB(191, 64, 0),
    ColorRGB(191, 0, 64),
    ColorRGB(0, 191, 64),
    ColorRGB(0, 64, 191),
    ColorRGB(128, 0, 64),
    ColorRGB(191, 64, 191),
    ColorRGB(191, 191, 64),
    ColorRGB(0, 64, 64),
};

int Q3_ColorIndexForCode(char code) noexcept
{
    const int index = static_cast<unsigned char>(code) - static_cast<unsigned char>('0');
    return index & static_cast<int>(Q3_COLOR_TABLE_SIZE - 1);
}

color_t Q3_ColorForIndex(int index, uint8_t alpha) noexcept
{
    const color_t base = Q3_COLOR_TABLE[index & static_cast<int>(Q3_COLOR_TABLE_SIZE - 1)];
    return ColorSetAlpha(base, alpha);
}

color_t Q3_ColorForCode(char code, uint8_t alpha) noexcept
{
    return Q3_ColorForIndex(Q3_ColorIndexForCode(code), alpha);
}

bool Q3_ParseColorEscape(const char *text, size_t length, color_t &color, size_t &consumed) noexcept
{
    if (length < 2 || text[0] != '^')
        return false;

    const char code = text[1];

    if (code == '^' || code == '\0')
        return false;

    color = Q3_ColorForCode(code, color.a);
    consumed = 2;
    return true;
}
