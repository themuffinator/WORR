#pragma once

#include "common/cvar.h"
#include "refresh/refresh.h"

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

int Renderer_ComputeAutoScale(const refcfg_t &cfg, int (*getDpiScale)());
bool R_ComputeKeepAspectUVWindow(int w, int h, float image_aspect, rUvWindow_t *out);
rUvWindow_t R_ComputeTileUVWindow(int x, int y, int w, int h);

