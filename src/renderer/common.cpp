#include "renderer/common.h"

#include "client/client.h"
#include "common/cvar.h"
#include "shared/shared.h"

#include <algorithm>

namespace {

constexpr float kAtlasCellSize = 1.0f / 16.0f;

int clampShadowPasses(int count) {
    return std::clamp(count, 0, 2);
}

} // namespace

refcfg_t r_config = {};
unsigned r_registration_sequence = 0;

cvar_t *gl_swapinterval = nullptr;
cvar_t *gl_partscale = nullptr;
cvar_t *gl_partstyle = nullptr;
cvar_t *gl_beamstyle = nullptr;

void Renderer_InitSharedCvars() {
    gl_swapinterval = Cvar_Get("gl_swapinterval", "1", CVAR_ARCHIVE);
    gl_partscale = Cvar_Get("gl_partscale", "2", 0);
    gl_partstyle = Cvar_Get("gl_partstyle", "0", 0);
    gl_beamstyle = Cvar_Get("gl_beamstyle", "0", 0);

    Cvar_Get("gl_waterwarp", "1", 0);
    Cvar_Get("gl_bloom", "1", 0);
    Cvar_Get("gl_polyblend", "1", 0);
    Cvar_Get("gl_fog", "1", 0);
    Cvar_Get("gl_dynamic", "1", 0);
    Cvar_Get("gl_per_pixel_lighting", "1", 0);
}

GlyphDrawData Renderer_BuildAtlasGlyph(const AtlasGlyphParams &params) {
    GlyphDrawData data{};

    uint8_t glyph = params.glyph;
    if ((glyph & 127u) == 32u) {
        return data;
    }

    if (params.flags & UI_ALTCOLOR) {
        glyph |= 0x80u;
    }
    if (params.flags & UI_XORCOLOR) {
        glyph ^= 0x80u;
    }

    float s0 = static_cast<float>(glyph & 15u) * kAtlasCellSize;
    float t0 = static_cast<float>(glyph >> 4) * kAtlasCellSize;
    float s1 = s0 + kAtlasCellSize;
    float t1 = t0 + kAtlasCellSize;

    color_t primaryColor = params.color;
    if (glyph & 0x80u) {
        primaryColor = ColorSetAlpha(COLOR_WHITE, params.color.a);
    }

    data.visible = true;
    data.primary = {
        static_cast<float>(params.x),
        static_cast<float>(params.y),
        static_cast<float>(params.width),
        static_cast<float>(params.height),
        s0,
        t0,
        s1,
        t1,
        primaryColor,
    };

    int dropSetting = std::max(params.dropShadowStrength, 0);
    if ((params.flags & UI_DROPSHADOW) && glyph != 0x83u) {
        int passes = clampShadowPasses(dropSetting > 1 ? 2 : 1);
        color_t shadowColor = ColorA(params.color.a);

        for (int i = 0; i < passes; ++i) {
            GlyphQuad shadow = data.primary;
            float offset = static_cast<float>(i + 1);
            shadow.x += offset;
            shadow.y += offset;
            shadow.color = shadowColor;
            data.shadows[i] = shadow;
        }

        data.shadowCount = passes;
    }

    return data;
}

GlyphDrawData Renderer_BuildKFontGlyph(const KFontGlyphParams &params) {
    GlyphDrawData data{};

    if (!params.metrics) {
        return data;
    }

    int w = static_cast<int>(params.metrics->w) * params.scale;
    int h = static_cast<int>(params.metrics->h) * params.scale;
    if (w <= 0 || h <= 0) {
        return data;
    }

    float s0 = params.metrics->x * params.sw;
    float t0 = params.metrics->y * params.sh;
    float s1 = s0 + params.metrics->w * params.sw;
    float t1 = t0 + params.metrics->h * params.sh;

    data.visible = true;
    data.primary = {
        static_cast<float>(params.x),
        static_cast<float>(params.y),
        static_cast<float>(w),
        static_cast<float>(h),
        s0,
        t0,
        s1,
        t1,
        params.color,
    };

    int dropSetting = std::max(params.dropShadowStrength, 0);
    if ((params.flags & UI_DROPSHADOW) || dropSetting > 0) {
        int passes = clampShadowPasses(dropSetting > 1 ? 2 : 1);
        color_t shadowColor = ColorA(params.color.a);
        int offsetStep = std::max(params.scale, 1);

        for (int i = 0; i < passes; ++i) {
            GlyphQuad shadow = data.primary;
            float offset = static_cast<float>((i + 1) * offsetStep);
            shadow.x += offset;
            shadow.y += offset;
            shadow.color = shadowColor;
            data.shadows[i] = shadow;
        }

        data.shadowCount = passes;
    }

    return data;
}

