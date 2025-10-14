#pragma once

#include "common/cvar.h"
#include "refresh/refresh.h"

extern refcfg_t r_config;
extern unsigned r_registration_sequence;

extern cvar_t *gl_swapinterval;
extern cvar_t *gl_partscale;
extern cvar_t *gl_partstyle;
extern cvar_t *gl_beamstyle;

void Renderer_InitSharedCvars();

