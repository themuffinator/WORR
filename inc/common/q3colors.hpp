#pragma once

#include <cstddef>

#include "shared/shared.hpp"

constexpr size_t Q3_COLOR_TABLE_SIZE = 32;

extern const color_t Q3_COLOR_TABLE[Q3_COLOR_TABLE_SIZE];

int     Q3_ColorIndexForCode(char code) noexcept;
color_t Q3_ColorForIndex(int index, uint8_t alpha) noexcept;
color_t Q3_ColorForCode(char code, uint8_t alpha) noexcept;
bool    Q3_ParseColorEscape(const char *text, size_t length, color_t &color, size_t &consumed) noexcept;
