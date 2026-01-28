// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "cg_entity_local.h"

#if USE_DEBUG
#define CG_SHOWMISS(...) \
    do { \
        cvar_t *showmiss = Cvar_FindVar("cl_showmiss"); \
        if (showmiss && showmiss->integer) { \
            Com_LPrintf(PRINT_DEVELOPER, __VA_ARGS__); \
        } \
    } while (0)
#else
#define CG_SHOWMISS(...)
#endif

static trace_t q_gameabi CG_PMTrace(const vec3_t start, const vec3_t mins, const vec3_t maxs,
                                    const vec3_t end, const struct edict_s *passent, contents_t contentmask)
{
    trace_t t;
    CL_Trace(&t, start, end, mins, maxs, passent, contentmask);
    return t;
}

static trace_t q_gameabi CG_Clip(const vec3_t start, const vec3_t mins, const vec3_t maxs,
                                 const vec3_t end, contents_t contentmask)
{
    trace_t trace;

    if (!mins)
        mins = vec3_origin;
    if (!maxs)
        maxs = vec3_origin;

    CM_BoxTrace(&trace, start, end, mins, maxs, cl.bsp->nodes, contentmask, cl.csr.extended);
    return trace;
}

static contents_t CG_PointContents(const vec3_t point)
{
    if (!cgei || !CL_PointContents)
        return 0;

    return CL_PointContents(point);
}

void CL_PredictAngles(void)
{
    cl.predicted_angles[0] = cl.viewangles[0] + cl.frame.ps.pmove.delta_angles[0];
    cl.predicted_angles[1] = cl.viewangles[1] + cl.frame.ps.pmove.delta_angles[1];
    cl.predicted_angles[2] = cl.viewangles[2] + cl.frame.ps.pmove.delta_angles[2];
}

void CL_CheckPredictionError(void)
{
    int frame;
    float delta[3];
    unsigned cmd;
    float len;

    if (cls.demo.playback)
        return;

    if (sv_paused && sv_paused->integer) {
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

    CG_SHOWMISS("prediction miss on %i: %f (%f %f %f)\n",
                cl.frame.number, len, delta[0], delta[1], delta[2]);

    VectorCopy(cl.frame.ps.pmove.origin, cl.predicted_origins[cmd & CMD_MASK]);

    // save for error interpolation
    VectorCopy(delta, cl.prediction_error);
}

#define MAX_STEP_CHANGE 32

void CL_PredictMovement(void)
{
    unsigned ack, current, frame;
    pmove_t pm;
    float step;

    if (!cgei || !cgei->Pmove)
        return;

    if (cls.state != ca_active)
        return;

    if (cls.demo.playback)
        return;

    if (sv_paused && sv_paused->integer)
        return;

    if (!cl_predict->integer || (cl.frame.ps.pmove.pm_flags & PMF_NO_PREDICTION)) {
        // just set angles
        CL_PredictAngles();
        return;
    }

    ack = cl.history[cls.netchan.incoming_acknowledged & CMD_MASK].cmdNumber;
    current = cl.cmdNumber;

    // if we are too far out of date, just freeze
    if (current - ack > CMD_BACKUP - 1) {
        CG_SHOWMISS("%i: exceeded CMD_BACKUP\n", cl.frame.number);
        return;
    }

    if (!cl.cmd.msec && current == ack) {
        CG_SHOWMISS("%i: not moved\n", cl.frame.number);
        return;
    }

    // copy current state to pmove
    memset(&pm, 0, sizeof(pm));
    pm.trace = CG_PMTrace;
    pm.clip = CG_Clip;
    pm.pointcontents = CG_PointContents;
    pm.s = cl.frame.ps.pmove;
    VectorCopy(cl.frame.ps.viewoffset, pm.viewoffset);
    pm.snapinitial = qtrue;
    const bool haste = (pm.s.pm_flags & PMF_HASTE) != 0;

    // run frames
    while (++ack <= current) {
        pm.cmd = cl.cmds[ack & CMD_MASK];
        pm.s.haste = haste;
        cgei->Pmove(&pm);
        pm.snapinitial = qfalse;

        // save for debug checking
        VectorCopy(pm.s.origin, cl.predicted_origins[ack & CMD_MASK]);
    }

    // run pending cmd
    if (cl.cmd.msec) {
        pm.cmd = cl.cmd;
        pm.cmd.forwardmove = cl.localmove[0];
        pm.cmd.sidemove = cl.localmove[1];
        pm.s.haste = haste;
        cgei->Pmove(&pm);
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
        float fabsStep = fabsf(step);
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
