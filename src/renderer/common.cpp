#include "renderer/common.h"

#include "common/cvar.h"

#include <algorithm>

namespace {
constexpr float kTileDivisor = 1.0f / 64.0f;
}

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

bool R_ComputeKeepAspectUVWindow(int w, int h, float image_aspect, rUvWindow_t *out) {
    if (!out || w <= 0 || h <= 0 || image_aspect <= 0.0f) {
        return false;
    }

    float scale_w = static_cast<float>(w);
    float scale_h = static_cast<float>(h) * image_aspect;
    float scale = std::max(scale_w, scale_h);
    if (scale <= 0.0f) {
        return false;
    }

    float s = 0.5f * (1.0f - (scale_w / scale));
    float t = 0.5f * (1.0f - (scale_h / scale));

    out->s0 = s;
    out->t0 = t;
    out->s1 = 1.0f - s;
    out->t1 = 1.0f - t;
    return true;
}

rUvWindow_t R_ComputeTileUVWindow(int x, int y, int w, int h) {
    rUvWindow_t window{};
    window.s0 = static_cast<float>(x) * kTileDivisor;
    window.t0 = static_cast<float>(y) * kTileDivisor;
    window.s1 = static_cast<float>(x + w) * kTileDivisor;
    window.t1 = static_cast<float>(y + h) * kTileDivisor;
    return window;
}

