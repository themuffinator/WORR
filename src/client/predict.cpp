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

#include "client.h"

static void CL_RequireCGameEntity(const char *what)
{
    if (!cgame_entity)
        Com_Error(ERR_DROP, "cgame entity extension required for %s", what);
}

/*
===================
CL_CheckPredictionError
===================
*/
void CL_CheckPredictionError(void)
{
    if (cls.state != ca_active)
        return;
    CL_RequireCGameEntity(__func__);
    if (!cgame_entity->CheckPredictionError)
        Com_Error(ERR_DROP, "cgame entity CheckPredictionError not available");

    cgame_entity->CheckPredictionError();
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

contents_t CL_PointContents(const vec3_t point)
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

void CL_PredictMovement(void)
{
    if (cls.state != ca_active)
        return;
    CL_RequireCGameEntity(__func__);
    if (!cgame_entity->PredictMovement)
        Com_Error(ERR_DROP, "cgame entity PredictMovement not available");

    cgame_entity->PredictMovement();
}
