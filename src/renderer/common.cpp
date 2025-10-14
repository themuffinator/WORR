#include "renderer/common.h"

#include "common/cvar.h"

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

