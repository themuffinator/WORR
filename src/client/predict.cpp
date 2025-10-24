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

#include "client.hpp"

/*
===================
CL_CheckPredictionError
===================
*/
void CL_CheckPredictionError(void)
{
    int         frame;
    float       delta[3];
    unsigned    cmd;
    float       len;

    if (cls.demo.playback) {
        return;
    }

    if (sv_paused->integer) {
        VectorClear(cl.prediction_error);
        return;
    }

    if (!cl_predict->integer || (cl.frame.ps.pmove.pm_flags & PMF_NO_PREDICTION))
        return;

    // calculate the last usercmd_t we sent that the server has processed
    frame = cls.netchan.incoming_acknowledged & CMD_MASK;
    cmd = cl.history[frame].cmdNumber;

    // compare what the server returned with what we had predicted it to be
    VectorSubtract(cl.frame.ps.pmove.origin, cl.predicted_origins[cmd & CMD_MASK], delta);

    // save the prediction error for interpolation
    len = fabsf(delta[0]) + fabsf(delta[1]) + fabsf(delta[2]);
    if (len > 80) {
        // > 80 world units is a teleport or something
        VectorClear(cl.prediction_error);
        return;
    }

    SHOWMISS("prediction miss on %i: %f (%f %f %f)\n",
             cl.frame.number, len, delta[0], delta[1], delta[2]);

    VectorCopy(cl.frame.ps.pmove.origin, cl.predicted_origins[cmd & CMD_MASK]);

    // save for error interpolation
    VectorCopy(delta, cl.prediction_error);
}

/*
====================
CL_ClipMoveToEntities
====================
*/
static void CL_ClipMoveToEntities(trace_t *tr, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, int contentmask)
{
    int         i;
    trace_t     trace;
    const mnode_t   *headnode;
    const centity_t *ent;
    const mmodel_t  *cmodel;

    for (i = 0; i < cl.numSolidEntities; i++) {
        ent = cl.solidEntities[i];

        if (cl.csr.extended && ent->current.number <= cl.maxclients && !(contentmask & CONTENTS_PLAYER))
            continue;

        if (ent->current.solid == PACKED_BSP) {
            // special value for bmodel
            cmodel = cl.model_clip[ent->current.modelindex];
            if (!cmodel)
                continue;
            headnode = cmodel->headnode;
        } else {
            headnode = CM_HeadnodeForBox(ent->mins, ent->maxs);
        }

        if (tr->allsolid)
            return;

        CM_TransformedBoxTrace(&trace, start, end,
                               mins, maxs, headnode, contentmask,
                               ent->current.origin, ent->current.angles,
                               cl.csr.extended);

        CM_ClipEntity(tr, &trace, (struct edict_s *)ent);
    }
}

/*
================
CL_Trace
================
*/
void CL_Trace(trace_t *tr, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, const struct edict_s* passent, contents_t contentmask)
{
    // check against world
    CM_BoxTrace(tr, start, end, mins, maxs, cl.bsp->nodes, contentmask, cl.csr.extended);
    tr->ent = (struct edict_s *)cl_entities;
    if (tr->fraction == 0)
        return;     // blocked by the world

    // check all other solid models
    CL_ClipMoveToEntities(tr, start, end, mins, maxs, contentmask);
}

static trace_t q_gameabi CL_PMTrace(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, const struct edict_s* passent, contents_t contentmask)
{
    trace_t t;
    CL_Trace(&t, start, end, mins, maxs, passent, contentmask);
    return t;
}

static trace_t q_gameabi CL_Clip(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, contents_t contentmask)
{
    trace_t     trace;

    if (!mins)
        mins = vec3_origin;
    if (!maxs)
        maxs = vec3_origin;

    CM_BoxTrace(&trace, start, end, mins, maxs, cl.bsp->nodes, contentmask, cl.csr.extended);
    return trace;
}

static contents_t CL_PointContents(const vec3_t point)
{
    const centity_t *ent;
    const mmodel_t  *cmodel;
    int i, contents;

    contents = CM_PointContents(point, cl.bsp->nodes, cl.csr.extended);

    for (i = 0; i < cl.numSolidEntities; i++) {
        ent = cl.solidEntities[i];

        if (ent->current.solid != PACKED_BSP) // special value for bmodel
            continue;

        cmodel = cl.model_clip[ent->current.modelindex];
        if (!cmodel)
            continue;

        if (cl.csr.extended) {
	        // Kex: mins/maxs check, required for certain
	        // weird bmodels that only have a single leaf
	        // and contain contents like SLIME. in Kex we
	        // also had a secondary fix because of func_train's
	        // that had SOLID on them but had no faces, but I think
	        // this block fixes both.
	        vec3_t pos_l;

	        // subtract origin offset
	        VectorSubtract(point, ent->current.origin, pos_l);

	        // rotate start and end into the models frame of reference
	        if (!VectorEmpty(ent->current.angles)) {
	            vec3_t angles, axis[3];
	            AnglesToAxis(angles, axis);
	            RotatePoint(pos_l, axis);
	        }
        
	        // see if the ent needs to be tested
	        if (pos_l[0] <= cmodel->mins[0] ||
	            pos_l[1] <= cmodel->mins[1] ||
	            pos_l[2] <= cmodel->mins[2] ||
	            pos_l[0] >= cmodel->maxs[0] ||
	            pos_l[1] >= cmodel->maxs[1] ||
	            pos_l[2] >= cmodel->maxs[2])
	            continue;
        }

        contents |= CM_TransformedPointContents(
                        point, cmodel->headnode,
                        ent->current.origin,
                        ent->current.angles,
                        cl.csr.extended);
    }

    return contents;
}

/*
=================
CL_PredictMovement

Sets cl.predicted_origin and cl.predicted_angles
=================
*/
void CL_PredictAngles(void)
{
    cl.predicted_angles[0] = cl.viewangles[0] + cl.frame.ps.pmove.delta_angles[0];
    cl.predicted_angles[1] = cl.viewangles[1] + cl.frame.ps.pmove.delta_angles[1];
    cl.predicted_angles[2] = cl.viewangles[2] + cl.frame.ps.pmove.delta_angles[2];
}

#define	MAX_STEP_CHANGE 32

void CL_PredictMovement(void)
{
    unsigned    ack, current, frame;
    pmove_t     pm;
    float       step;

    if (cls.state != ca_active) {
        return;
    }

    if (cls.demo.playback) {
        return;
    }

    if (sv_paused->integer) {
        return;
    }

    if (!cl_predict->integer || (cl.frame.ps.pmove.pm_flags & PMF_NO_PREDICTION)) {
        // just set angles
        CL_PredictAngles();
        return;
    }

    ack = cl.history[cls.netchan.incoming_acknowledged & CMD_MASK].cmdNumber;
    current = cl.cmdNumber;

    // if we are too far out of date, just freeze
    if (current - ack > CMD_BACKUP - 1) {
        SHOWMISS("%i: exceeded CMD_BACKUP\n", cl.frame.number);
        return;
    }

    if (!cl.cmd.msec && current == ack) {
        SHOWMISS("%i: not moved\n", cl.frame.number);
        return;
    }

    // copy current state to pmove
    memset(&pm, 0, sizeof(pm));
    pm.trace = CL_PMTrace;
    pm.clip = CL_Clip;
    pm.pointcontents = CL_PointContents;
    pm.s = cl.frame.ps.pmove;
    VectorCopy(cl.frame.ps.viewoffset, pm.viewoffset);
    pm.snapinitial = qtrue;

    // run framesgit 
    while (++ack <= current) {
        pm.cmd = cl.cmds[ack & CMD_MASK];
        cgame->Pmove(&pm);
        pm.snapinitial = qfalse;

        // save for debug checking
        VectorCopy(pm.s.origin, cl.predicted_origins[ack & CMD_MASK]);
    }

    // run pending cmd
    if (cl.cmd.msec) {
        pm.cmd = cl.cmd;
        pm.cmd.forwardmove = cl.localmove[0];
        pm.cmd.sidemove = cl.localmove[1];
        cgame->Pmove(&pm);
        frame = current;

        // save for debug checking
        VectorCopy(pm.s.origin, cl.predicted_origins[(current + 1) & CMD_MASK]);
    } else {
        frame = current - 1;
    }

    if (pm.s.pm_type != PM_SPECTATOR) {
        // Step detection
        float oldz = cl.predicted_origins[frame & CMD_MASK][2];
        step = pm.s.origin[2] - oldz;
        float fabsStep = fabsf( step );
        // Consider a Z change being "stepping" if...
        bool step_detected = (fabsStep > 1 && fabsStep < 20) // absolute change is in this limited range
            && ((cl.frame.ps.pmove.pm_flags & PMF_ON_GROUND) || pm.step_clip) // and we started off on the ground
            && ((pm.s.pm_flags & PMF_ON_GROUND) && pm.s.pm_type <= PM_GRAPPLE) // and are still predicted to be on the ground
            && (memcmp(&cl.last_groundplane, &pm.groundplane, sizeof(cplane_t)) != 0
                || cl.last_groundentity != pm.groundentity);                   // and don't stand on another plane or entity
        if (step_detected) {
            // Code below adapted from Q3A.
            // check for stepping up before a previous step is completed
            float delta = cls.realtime - cl.predicted_step_time;
            float old_step;
            if (delta < STEP_TIME) {
                old_step = cl.predicted_step * (STEP_TIME - delta) / STEP_TIME;
            } else {
                old_step = 0;
            }

            // add this amount
            cl.predicted_step = Q_clip(old_step + step, -MAX_STEP_CHANGE, MAX_STEP_CHANGE);
            cl.predicted_step_time = cls.realtime;
        }
    }

    // copy results out for rendering
    VectorCopy(pm.s.origin, cl.predicted_origin);
    VectorCopy(pm.s.velocity, cl.predicted_velocity);
    VectorCopy(pm.viewangles, cl.predicted_angles);
    Vector4Copy(pm.screen_blend, cl.predicted_screen_blend);
    cl.predicted_rdflags = pm.rdflags;

    cl.last_groundplane = pm.groundplane;
    cl.last_groundentity = pm.groundentity;
}
