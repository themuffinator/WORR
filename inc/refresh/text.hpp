#pragma once

#include "refresh/refresh.hpp"

#include <cstddef>

struct text_shadow_t {
    float   offset_x{0.0f};
    float   offset_y{0.0f};
    color_t color{ColorA(0)};
};

struct text_style_t {
    bool        bold{false};
    bool        italic{false};
    bool        underline{false};
    float       outline_thickness{0.0f};
    color_t     outline_color{COLOR_BLACK};
    text_shadow_t shadow{};
    bool        allow_color_codes{true};
};

struct text_render_request_t {
    int                 x{0};
    int                 y{0};
    int                 scale{1};
    int                 flags{0};
    size_t              max_bytes{MAX_STRING_CHARS};
    const char         *text{nullptr};
    color_t             base_color{COLOR_WHITE};
    qhandle_t           font{0};
    const ftfont_t     *ftfont{nullptr};
    const kfont_t      *kfont{nullptr};
    text_style_t        style{};
    float               dpi_scale{0.0f}; // 0 = auto (renderer scale)
};

struct text_measure_result_t {
    int     width{0};
    int     height{0};
    color_t final_color{COLOR_WHITE};
};

void R_TextSystemInit(void);
void R_TextSystemShutdown(void);

int R_TextDrawString(const text_render_request_t &request);
text_measure_result_t R_TextMeasureString(const text_render_request_t &request);
float R_TextLineHeight(const text_render_request_t &request);
