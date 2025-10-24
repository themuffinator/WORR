/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "shared/shared.hpp"
#include "shared/game.hpp"
#include "shared/game3_shared.hpp"
#include "common/game3_convert.hpp"
#include "common/game3_pmove.hpp"
#include "common/pmove.hpp"

static trace_t (* q_gameabi current_pmove_trace)(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, const struct edict_s* passent, contents_t contentmask);
static contents_t (* current_pmove_pointcontents)(const vec3_t point);

static int pm_clipmask;
static csurface_v3_t trace3_result_surface;

static qboolean BoolToQBoolean(bool value)
{
    return value ? qtrue : qfalse;
}

static bool QBooleanToBool(qboolean value)
{
    return value != qfalse;
}

static game3_trace_t convert_trace(const trace_t* trace)
{
    game3_trace_t trace3;
    trace3.allsolid = BoolToQBoolean(trace->allsolid);
    trace3.startsolid = BoolToQBoolean(trace->startsolid);
    trace3.fraction = trace->fraction;
    VectorCopy(trace->endpos, trace3.endpos);
    trace3.plane = trace->plane;
    Q_strlcpy(trace3_result_surface.name, trace->surface->name, sizeof(trace3_result_surface.name));
    trace3_result_surface.flags = trace->surface->flags;
    trace3_result_surface.value = trace->surface->value;
    trace3.surface = &trace3_result_surface;
    trace3.contents = trace->contents;
    /* This looks wrong, but is actually okay:
     * Any entities by "client" traces aren't sensibly useable, anyway, as clients don't keep
     * track of edicts; the only thing you can do is distinguish NULL and non-NULL values.
     * To do that, cast is fine ... */
    trace3.ent = (game3_edict_t*)trace->ent;
    return trace3;
}

static game3_trace_t wrap_pmove_trace_old(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end)
{
    trace_t trace = current_pmove_trace(start, mins, maxs, end, NULL, pm_clipmask);
    return convert_trace(&trace);
}

static game3_trace_t wrap_pmove_trace_new(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int contentmask)
{
    trace_t trace = current_pmove_trace(start, mins, maxs, end, NULL, contentmask ? contentmask : pm_clipmask);
    return convert_trace(&trace);
}

static int wrap_pmove_pointcontents(const vec3_t point)
{
    return (int)current_pmove_pointcontents(point);
}

/*
================
Pmove

Can be called by either the server or the client
================
*/
void Pmove(pmove_t *pmove, const pmoveParams_t *params)
{
    pm_clipmask = MASK_PLAYERSOLID;

    // remaster player collision rules
    if (params->extended_server_ver != 0) {
        if (pmove->s.pm_type == PM_DEAD || pmove->s.pm_type == PM_GIB)
            pm_clipmask = MASK_DEADSOLID;

        if (!(pmove->s.pm_flags & PMF_IGNORE_PLAYER_COLLISION))
            pm_clipmask |= CONTENTS_PLAYER;
    }

    if (params->extended_server_ver >= 2) {
        game3_pmove_new_t game3_pmove;
        ConvertToGame3_pmove_state_new(&game3_pmove.s, &pmove->s, true);

        ConvertToGame3_usercmd(&game3_pmove.cmd, &pmove->cmd);
        game3_pmove.snapinitial = BoolToQBoolean(pmove->snapinitial);
        game3_pmove.trace = wrap_pmove_trace_new;
        // "Classic" pmove doesn't actually use clip(), so no need to set it
        game3_pmove.pointcontents = wrap_pmove_pointcontents;

        current_pmove_trace = pmove->trace;
        current_pmove_pointcontents = pmove->pointcontents;
        game3_PmoveNew(&game3_pmove, &pmove->groundplane, params);

        ConvertFromGame3_pmove_state_new(&pmove->s, &game3_pmove.s, true);
        pmove->snapinitial = QBooleanToBool(game3_pmove.snapinitial);

        VectorCopy(game3_pmove.viewangles, pmove->viewangles);
        VectorCopy(game3_pmove.mins, pmove->mins);
        VectorCopy(game3_pmove.maxs, pmove->maxs);

        // See comment on trace.ent above
        pmove->groundentity = (edict_t *)game3_pmove.groundentity;

        pmove->watertype = game3_pmove.watertype;
        pmove->waterlevel = game3_pmove.waterlevel;
    } else {
        game3_pmove_old_t game3_pmove;
        ConvertToGame3_pmove_state_old(&game3_pmove.s, &pmove->s, params->extended_server_ver != 0);

        ConvertToGame3_usercmd(&game3_pmove.cmd, &pmove->cmd);
        game3_pmove.snapinitial = BoolToQBoolean(pmove->snapinitial);
        game3_pmove.trace = wrap_pmove_trace_old;
        // "Classic" pmove doesn't actually use clip(), so no need to set it
        game3_pmove.pointcontents = wrap_pmove_pointcontents;

        current_pmove_trace = pmove->trace;
        current_pmove_pointcontents = pmove->pointcontents;
        game3_PmoveOld(&game3_pmove, &pmove->groundplane, params);

        ConvertFromGame3_pmove_state_old(&pmove->s, &game3_pmove.s, params->extended_server_ver != 0);
        pmove->snapinitial = QBooleanToBool(game3_pmove.snapinitial);

        VectorCopy(game3_pmove.viewangles, pmove->viewangles);
        VectorCopy(game3_pmove.mins, pmove->mins);
        VectorCopy(game3_pmove.maxs, pmove->maxs);

        // See comment on trace.ent above
        pmove->groundentity = (edict_t *)game3_pmove.groundentity;

        pmove->watertype = game3_pmove.watertype;
        pmove->waterlevel = game3_pmove.waterlevel;
    }

    /* viewheight is now added to the viewoffset; this didn't happen in vanilla,
     * so clear out the viewheight */
    pmove->s.viewheight = 0;

    /* Prediction (what Pmove is still used for) doesn't care about
     * touches, so cheat and pretend 0 touches (and don't bother to somehow
     * produce correct touch info). */
    pmove->touch.num = 0;
}

void PmoveInit(pmoveParams_t *pmp)
{
    // set up default pmove parameters
    memset(pmp, 0, sizeof(*pmp));

    pmp->speedmult = 1;
    pmp->watermult = 0.5f;
    pmp->maxspeed = 300;
    pmp->friction = 6;
    pmp->waterfriction = 1;
    pmp->flyfriction = 9;
    pmp->time_shift = 3;
    pmp->coord_bits = 16;
}

void PmoveEnableQW(pmoveParams_t *pmp)
{
    pmp->qwmode = true;
    pmp->watermult = 0.7f;
    pmp->maxspeed = 320;
    //pmp->upspeed = (sv_qwmod->integer > 1) ? 310 : 350;
    pmp->friction = 4;
    pmp->waterfriction = 4;
    pmp->airaccelerate = true;
}

void PmoveEnableExt(pmoveParams_t *pmp)
{
    pmp->time_shift = 0;
    pmp->coord_bits = 23;
}
