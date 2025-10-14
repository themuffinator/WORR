#pragma once

#include "common/cvar.h"
#include "refresh/refresh.h"

#include <array>
#include <cstdint>

struct GlyphQuad {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    float s0 = 0.0f;
    float t0 = 0.0f;
    float s1 = 0.0f;
    float t1 = 0.0f;
    color_t color = COLOR_WHITE;
};

struct GlyphDrawData {
    bool visible = false;
    GlyphQuad primary = {};
    std::array<GlyphQuad, 2> shadows = {};
    int shadowCount = 0;
};

struct AtlasGlyphParams {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int flags = 0;
    uint8_t glyph = 0;
    color_t color = COLOR_WHITE;
    int dropShadowStrength = 0;
};

struct KFontGlyphParams {
    int x = 0;
    int y = 0;
    int scale = 1;
    int flags = 0;
    color_t color = COLOR_WHITE;
    const kfont_char_t *metrics = nullptr;
    float sw = 0.0f;
    float sh = 0.0f;
    int dropShadowStrength = 0;
};

extern refcfg_t r_config;
extern unsigned r_registration_sequence;

extern cvar_t *gl_swapinterval;
extern cvar_t *gl_partscale;
extern cvar_t *gl_partstyle;
extern cvar_t *gl_beamstyle;

struct rUvWindow_t {
    float s0;
    float t0;
    float s1;
    float t1;
};

void Renderer_InitSharedCvars();

GlyphDrawData Renderer_BuildAtlasGlyph(const AtlasGlyphParams &params);
GlyphDrawData Renderer_BuildKFontGlyph(const KFontGlyphParams &params);
bool R_ComputeKeepAspectUVWindow(int w, int h, float image_aspect, rUvWindow_t *out);
rUvWindow_t R_ComputeTileUVWindow(int x, int y, int w, int h);

