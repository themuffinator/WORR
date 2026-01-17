// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

// g_local.h -- local definitions for game module
#pragma once

#include "bg_local.hpp"

extern cgame_import_t cgi;
extern cgame_export_t cglobals;

#define SERVER_TICK_RATE cgi.tickRate // in hz
#define FRAME_TIME_S cgi.frameTimeSec
#define FRAME_TIME_MS cgi.frameTimeMs
