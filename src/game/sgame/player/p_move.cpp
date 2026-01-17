/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

p_move.cpp (Player Movement) This file contains the core player physics and movement logic for
the game, encapsulated within the `Pmove` function. This code is shared between the server
(game.dll) and the client (cgame.dll) to ensure that player movement is predicted accurately on
the client and authoritatively simulated on the server. Key Responsibilities: - `Pmove`: The
main entry point for a single step of player movement simulation. - Movement Logic: Implements
different types of movement, including walking, air movement, swimming, ladder climbing, and
noclip. - Collision Detection: The `PM_StepSlideMove` function handles collision detection and
response, causing the player to slide along walls and step up small ledges. - Player State
Updates: Modifies the `player_state_t` based on the movement simulation, updating the player's
origin, velocity, and ground status. - Special Moves: Handles the physics for actions like
jumping and crouching.*/

#include "../../bgame/q_std.hpp"

#define GAME_INCLUDE
#include "../../bgame/bg_local.hpp"

/*
===============
G_FixStuckObject_Generic
Detects and resolves a stuck hull by probing each axial side,
measuring clearance, and choosing the least-movement fix.
===============
*/
StuckResult G_FixStuckObject_Generic(
	Vector3& origin,
	const Vector3& ownMins,
	const Vector3& ownMaxs,
	std::function<stuck_object_trace_fn_t> trace) {
	if (!trace(origin, ownMins, ownMaxs, origin).startSolid)
		return StuckResult::GoodPosition;

	struct GoodPos {
		float  dist2 = 0.0f;
		Vector3 pos{};
	};
	// We consider exactly six axial directions (pairs), so six slots is enough.
	std::array<GoodPos, 6> good{};
	size_t numGood = 0;

	// Axial side probes (paired so sn ^ 1 is opposite).
	struct Side {
		std::array<int8_t, 3> normal; // -1, 0, 1 for which side to sample
		std::array<int8_t, 3> mins;   // -1 -> use ownMins, +1 -> use ownMaxs, 0 -> ignore
		std::array<int8_t, 3> maxs;   // -1 -> use ownMins, +1 -> use ownMaxs, 0 -> ignore
	};
	static constexpr std::array<Side, 6> kSides = { {
		{{  0,  0,  1 }, { -1, -1,  0 }, {  1,  1,  0 }}, // +Z
		{{  0,  0, -1 }, { -1, -1,  0 }, {  1,  1,  0 }}, // -Z
		{{  1,  0,  0 }, {  0, -1, -1 }, {  0,  1,  1 }}, // +X
		{{ -1,  0,  0 }, {  0, -1, -1 }, {  0,  1,  1 }}, // -X
		{{  0,  1,  0 }, { -1,  0, -1 }, {  1,  0,  1 }}, // +Y
		{{  0, -1,  0 }, { -1,  0, -1 }, {  1,  0,  1 }}, // -Y
	} };

	static constexpr float kWallNudge = 0.125f; // push slightly off the wall

	for (size_t sn = 0; sn < kSides.size(); ++sn) {
		const Side& side = kSides[sn];

		Vector3 start = origin;
		Vector3 mins = {};
		Vector3 maxs = {};

		// Pick a corner on this face to start the probe; also build the probe hull.
		for (size_t i = 0; i < 3; ++i) {
			if (side.normal[i] < 0)
				start[i] += ownMins[i];
			else if (side.normal[i] > 0)
				start[i] += ownMaxs[i];

			if (side.mins[i] == -1)      mins[i] = ownMins[i];
			else if (side.mins[i] == 1)  mins[i] = ownMaxs[i];

			if (side.maxs[i] == -1)      maxs[i] = ownMins[i];
			else if (side.maxs[i] == 1)  maxs[i] = ownMaxs[i];
		}

		trace_t tr = trace(start, mins, maxs, start);

		// If we are still solid here, try tiny epsilon shoves along non-normal axes.
		int  epsAxis = -1;
		int  epsDir = 0;
		if (tr.startSolid) {
			for (size_t axis = 0; axis < 3; ++axis) {
				if (side.normal[axis] != 0)
					continue;

				Vector3 probe = start;

				// +1
				probe[axis] += 1;
				tr = trace(probe, mins, maxs, probe);
				if (!tr.startSolid) {
					start = probe;
					epsAxis = static_cast<int>(axis);
					epsDir = 1;
					break;
				}

				// -1
				probe[axis] -= 2;
				tr = trace(probe, mins, maxs, probe);
				if (!tr.startSolid) {
					start = probe;
					epsAxis = static_cast<int>(axis);
					epsDir = -1;
					break;
				}
			}
		}

		// Still bad? This side cannot help us.
		if (tr.startSolid)
			continue;

		// Compute the opposite corner on the opposite face so we can measure clearance.
		Vector3 opp = origin;
		const Side& oppSide = kSides[sn ^ 1];
		for (size_t i = 0; i < 3; ++i) {
			if (oppSide.normal[i] < 0)
				opp[i] += ownMins[i];
			else if (oppSide.normal[i] > 0)
				opp[i] += ownMaxs[i];
		}
		if (epsAxis >= 0)
			opp[epsAxis] += static_cast<float>(epsDir);

		// Push from face corner toward the opposite corner to find how much free space exists.
		tr = trace(start, mins, maxs, opp);
		if (tr.startSolid)
			continue;

		// Move slightly off the wall along the tested face normal.
		Vector3 end = tr.endPos;
		end += Vector3{
			static_cast<float>(side.normal[0]),
			static_cast<float>(side.normal[1]),
			static_cast<float>(side.normal[2])
		} *kWallNudge;

		const Vector3 delta = end - opp;
		Vector3       fix = origin + delta;
		if (epsAxis >= 0)
			fix[epsAxis] += static_cast<float>(epsDir);

		// Verify the candidate fix.
		tr = trace(fix, ownMins, ownMaxs, fix);
		if (tr.startSolid)
			continue;

		// Record candidate; prefer the least movement.
		if (numGood < good.size()) {
			good[numGood].pos = fix;
			good[numGood].dist2 = delta.lengthSquared();
			++numGood;
		}
	}

	if (numGood) {
		// NOTE: original code sorted with the wrong end iterator. Fixed below.
		std::sort(good.begin(), good.begin() + static_cast<std::ptrdiff_t>(numGood),
			[](const GoodPos& a, const GoodPos& b) { return a.dist2 < b.dist2; });

		origin = good[0].pos;
		return StuckResult::Fixed;
	}

	return StuckResult::NoGoodPosition;
}

// all of the locals will be zeroed before each
// pmove, just to make damn sure we don't have
// any differences when running on client or server

struct PMoveLocal {
	Vector3		origin{};
	Vector3		velocity{};

	Vector3		forward{}, right{}, up{};
	float		frameTime = 0.0f;

	csurface_t* groundSurface{};
	int			groundContents = 0;

	Vector3		previousOrigin{};
	Vector3		startVelocity{};
};

pm_config_t	pm_config;

PMove* pm;
PMoveLocal	pml;

// movement parameters
float pm_stopSpeed = 100;
float pm_maxSpeed = 300;
float pm_duckSpeed = 100;
float pm_accelerate = 10;
float pm_waterAccelerate = 10;
float pm_friction = 6;
float pm_waterFriction = 1;
float pm_waterSpeed = 400;
float pm_ladderScale = 0.5f;
constexpr float kOverbounceDefault = 1.01f;
constexpr float kOverbounceQ3 = 1.001f;

static inline float PM_GetOverbounceFactor() {
	return pm_config.q3Overbounce ? kOverbounceQ3 : kOverbounceDefault;
}

/*
==================
MaxSpeed
==================
*/
static float MaxSpeed(pmove_state_t* ps) {
	return ps->haste ? pm_maxSpeed * 1.25 : pm_maxSpeed;
}

/*
  walking up a step should kill some velocity
*/

/*
===============
PM_ClipVelocity
Slides the input velocity along a collision plane.

in:        incoming velocity
normal:    plane normal (expected unit length)
out:       clipped velocity result
overBounce:1.0f for pure slide; >1.0f adds a small bounce (e.g., 1.01f)
===============
*/
static inline void PM_ClipVelocity(const Vector3& in, const Vector3& normal, Vector3& out, float overBounce) {
	// Project the incoming velocity onto the plane normal and remove that component.
	float backOff = in.dot(normal);

	// Quake 3 overbounce bug applies asymmetric scaling to the backoff term.
	if (pm_config.q3Overbounce) {
		if (backOff < 0.0f) {
			backOff *= overBounce;
		}
		else {
			backOff /= overBounce;
		}
	}
	else {
		backOff *= overBounce;
	}

	out = in - normal * backOff;

	// Kill tiny components to avoid jitter in corners and on slopes.
	for (int i = 0; i < 3; ++i) {
		if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON)
			out[i] = 0.0f;
	}
}

/*
==================
PM_Clip
==================
*/
static trace_t PM_Clip(const Vector3& start, const Vector3& mins, const Vector3& maxs, const Vector3& end, contents_t mask) {
	return pm->clip(start, &mins, &maxs, end, mask);
}

/*
==================
PM_Trace
==================
*/
static trace_t PM_Trace(const Vector3& start, const Vector3& mins, const Vector3& maxs, const Vector3& end, contents_t mask = CONTENTS_NONE) {
	if (pm->s.pmType == PM_SPECTATOR)
		return PM_Clip(start, mins, maxs, end, MASK_SOLID);

	if (mask == CONTENTS_NONE) {
		if (pm->s.pmType == PM_DEAD || pm->s.pmType == PM_GIB)
			mask = MASK_DEADSOLID;
		else if (pm->s.pmType == PM_SPECTATOR)
			mask = MASK_SOLID;
		else
			mask = MASK_PLAYERSOLID;

		if (pm->s.pmFlags & PMF_IGNORE_PLAYER_COLLISION)
			mask &= ~CONTENTS_PLAYER;
	}

	return pm->trace(start, &mins, &maxs, end, pm->player, mask);
}

/*
==================
PM_Trace_Auto
only here to satisfy pm_trace_t
==================
*/
static inline trace_t PM_Trace_Auto(const Vector3& start, const Vector3& mins, const Vector3& maxs, const Vector3& end) {
	return PM_Trace(start, mins, maxs, end);
}

/*
==================
PM_StepSlideMove

Each intersection will try to step over the obstruction instead of
sliding along it.

Returns a new origin, velocity, and contact entity
Does not modify any world state?
==================
*/
constexpr float	 MIN_STEP_NORMAL = 0.7f; // can't step up onto very steep slopes
constexpr size_t MAX_CLIP_PLANES = 5;

static inline void PM_RecordTrace(touch_list_t& touch, trace_t& tr) {
	if (touch.num == MAXTOUCH)
		return;

	for (size_t i = 0; i < touch.num; i++)
		if (touch.traces[i].ent == tr.ent)
			return;

	touch.traces[touch.num++] = tr;
}

/*
===============
PM_StepSlideMove_Generic
Iterative step/slide resolver for generic hulls.

- Integrates origin along velocity over frameTime, sliding across collision planes.
- Handles startSolid/allSolid, dual-plane (curved surface) selection, and
  epsilon nudge for nearly-parallel non-axial planes.
- Records all touches via PM_RecordTrace.
- If has_time is true, velocity is restored to its primal value at the end.
===============
*/
void PM_StepSlideMove_Generic(
	Vector3& origin,
	Vector3& velocity,
	float frameTime,
	const Vector3& mins,
	const Vector3& maxs,
	touch_list_t& touch,
	bool has_time,
	pm_trace_t traceFunc
) {
	static constexpr int   MAX_BUMPS = 4;
	static constexpr float PARALLEL_DOT = 0.99f;   // consider planes effectively parallel
	static constexpr float NUDGE_DIST = 0.01f;   // small push along plane normal
	const float overBounce = PM_GetOverbounceFactor();

	// Early out: nothing to do.
	if (velocity[_X] == 0.0f && velocity[_Y] == 0.0f && velocity[_Z] == 0.0f) {
		return;
	}

	Vector3 primalVelocity = velocity;
	Vector3 planes[MAX_CLIP_PLANES] = {};
	int    numPlanes = 0;

	float  time_left = frameTime;

	for (int bump = 0; bump < MAX_BUMPS; ++bump) {
		// Compute intended end position for this substep.
		Vector3 end = origin + velocity * time_left;

		trace_t tr = traceFunc(origin, mins, maxs, end);

		// Inside a solid: zero vertical motion to avoid stacking fall damage, record touch, and bail.
		if (tr.allSolid) {
			velocity[_Z] = 0.0f;
			PM_RecordTrace(touch, tr);
			return;
		}

		// Experimental dual-plane handling on curved surfaces:
		// pick the plane that produces the "smaller" post-clip velocity.
		if (tr.surface2) {
			Vector3 clip_a, clip_b;
			PM_ClipVelocity(velocity, tr.plane.normal, clip_a, overBounce);
			PM_ClipVelocity(velocity, tr.plane2.normal, clip_b, overBounce);

			float sum_a = std::fabs(clip_a[0]) + std::fabs(clip_a[1]) + std::fabs(clip_a[2]);
			float sum_b = std::fabs(clip_b[0]) + std::fabs(clip_b[1]) + std::fabs(clip_b[2]);

			// Choose the plane that gives the smaller magnitude result.
			if (sum_b < sum_a) {
				tr.plane = tr.plane2;
				tr.surface = tr.surface2;
			}
		}

		// We actually moved some fraction.
		if (tr.fraction > 0.0f) {
			origin = tr.endPos;
			primalVelocity = velocity;
			numPlanes = 0; // reset clip set after progress
		}

		// Moved the entire distance this substep.
		if (tr.fraction == 1.0f) {
			break;
		}

		// Save entity for contact feedback.
		PM_RecordTrace(touch, tr);

		// Reduce remaining time proportionally to the fraction traveled.
		time_left -= time_left * tr.fraction;
		if (time_left <= 0.0f) {
			break;
		}

		// Start in solid but not allSolid: kill vertical and keep trying.
		if (tr.startSolid) {
			velocity[_Z] = 0.0f;
		}

		// Too many clip planes means we are wedged; hard stop.
		if (numPlanes >= MAX_CLIP_PLANES) {
			velocity = {};
			break;
		}

		// If this plane is effectively the same as one we already have,
		// nudge origin a touch along the plane normal to escape epsilon traps.
		int i = 0;
		for (; i < numPlanes; ++i) {
			if (tr.plane.normal.dot(planes[i]) > PARALLEL_DOT) {
				origin[_X] += tr.plane.normal[0] * NUDGE_DIST;
				origin[_Y] += tr.plane.normal[1] * NUDGE_DIST;
				origin[_Z] += tr.plane.normal[2] * NUDGE_DIST * 0.0f; // do not nudge Z here
				G_FixStuckObject_Generic(origin, mins, maxs, traceFunc);
				break;
			}
		}
		if (i < numPlanes) {
			// We nudged; try the movement again from this updated origin.
			continue;
		}

		// Add this plane to the set.
		planes[numPlanes++] = tr.plane.normal;

		// Reclip velocity so it is parallel to all planes hit so far.
		for (i = 0; i < numPlanes; ++i) {
			PM_ClipVelocity(velocity, planes[i], velocity, overBounce);

			// Ensure we are not moving into any other plane.
			int j = 0;
			for (j = 0; j < numPlanes; ++j) {
				if (j == i) continue;
				if (velocity.dot(planes[j]) < 0.0f) {
					break; // still penetrating another plane, need another pass
				}
			}
			if (j == numPlanes) {
				break; // good: parallel to all planes
			}
		}

		// If we could not find a single plane to move along, try the crease of two planes.
		if (i == numPlanes) {
			if (numPlanes != 2) {
				velocity = {};
				break;
			}
			Vector3 dir = planes[0].cross(planes[1]); // crease direction
			float  d = dir.dot(velocity);
			velocity = dir * d;
		}

		// If new velocity opposes the original direction, stop to avoid corner jitter.
		if (velocity.dot(primalVelocity) <= 0.0f) {
			velocity = {};
			break;
		}
	}

	// Caller wants only position integration; restore original velocity.
	if (has_time) {
		velocity = primalVelocity;
	}
}

static inline void PM_StepSlideMove_() {
	PM_StepSlideMove_Generic(pml.origin, pml.velocity, pml.frameTime, pm->mins, pm->maxs, pm->touch, pm->s.pmTime, PM_Trace_Auto);
}

/*
===============
PM_StepSlideMove
Performs a slide move, then attempts a step-up and re-slide, choosing the
result that gives better horizontal progress. Includes stair-clip fix and
optional step-down to keep feet on stairs/slopes.
===============
*/
static void PM_StepSlideMove() {
	Vector3 start_o = pml.origin;
	Vector3 start_v = pml.velocity;

	// First: plain slide at current height
	PM_StepSlideMove_();

	Vector3 down_o = pml.origin;
	Vector3 down_v = pml.velocity;

	// Attempt to step up
	Vector3 up = start_o;
	up[2] += (pml.origin[_Z] < 0.0f) ? STEPSIZE_BELOW : STEPSIZE;

	trace_t tr = PM_Trace(start_o, pm->mins, pm->maxs, up);
	if (tr.allSolid) {
		// Cannot step up; keep the initial slide result
		return;
	}

	const float stepSize = tr.endPos[2] - start_o[2];

	// Try sliding above
	pml.origin = tr.endPos;
	pml.velocity = start_v;
	PM_StepSlideMove_();

	// Push down by the amount we stepped up
	Vector3 down = pml.origin;
	down[2] -= stepSize;

	// Stair-clip fix (jitspoe suggestion):
	// If we started lower than the down end Z, choose a better down trace start.
	const Vector3 original_down = down;
	if (start_o[2] < down[2]) {
		down[2] = start_o[2] - 1.0f;
	}

	tr = PM_Trace(pml.origin, pm->mins, pm->maxs, down);
	if (!tr.allSolid) {
		// Do the proper trace to the original intended down end
		const trace_t realTrace = PM_Trace(pml.origin, pm->mins, pm->maxs, original_down);
		pml.origin = realTrace.endPos;

		// Only upward vertical velocity counts as a stair clip
		if (pml.velocity[_Z] > 0.0f) {
			pm->stepClip = true;
		}
	}

	// Decide which path yielded better horizontal progress
	auto dist2_xy = [](const Vector3& a, const Vector3& b) -> float {
		const float dx = a[0] - b[0];
		const float dy = a[1] - b[1];
		return dx * dx + dy * dy;
		};

	const float down_dist = dist2_xy(down_o, start_o);
	const float up_dist = dist2_xy(pml.origin, start_o);

	// Prefer the down path if it went farther, or if the step plane is too steep
	if (down_dist > up_dist || tr.plane.normal[2] < MIN_STEP_NORMAL) {
		pml.origin = down_o;
		pml.velocity = down_v;
	}
	else {
		// NB: Keeping this to enable ramp-jumps (per KEX/Jitspoe notes)
		// If we were walking along a plane, copy Z velocity from the down slide
		pml.velocity[_Z] = down_v[2];
	}

	// Optional: step down stairs/slopes to keep feet grounded
	if ((pm->s.pmFlags & PMF_ON_GROUND) &&
		!(pm->s.pmFlags & PMF_ON_LADDER) &&
		(pm->waterLevel < WATER_WAIST ||
			(!(pm->cmd.buttons & BUTTON_JUMP) && pml.velocity[_Z] <= 0.0f))) {

		Vector3 step_down = pml.origin;
		step_down[2] -= (pml.origin[_Z] < 0.0f) ? STEPSIZE_BELOW : STEPSIZE;

		const trace_t down_tr = PM_Trace(pml.origin, pm->mins, pm->maxs, step_down);
		if (down_tr.fraction < 1.0f) {
			pml.origin = down_tr.endPos;
		}
	}
}

/*
===============
PM_Friction
Applies ground and water friction to pml.velocity.
===============
*/
static void PM_Friction() {
	Vector3& vel = pml.velocity;

	const float speed = vel.length();
	if (speed < 1.0f) {
		vel[0] = vel[1] = 0.0f;
		// keep Z for vertical impulses (jump/fall)
		return;
	}

	float drop = 0.0f;

	// --- Ground friction ---
	const bool onGround = (pm->groundEntity &&
		pml.groundSurface &&
		!(pml.groundSurface->flags & SURF_SLICK)) ||
		(pm->s.pmFlags & PMF_ON_LADDER);

	if (onGround) {
		const float friction = pm_friction;
		if (!(pm->s.pmFlags & PMF_TIME_KNOCKBACK)) {
			const float control = (speed < pm_stopSpeed) ? pm_stopSpeed : speed;
			drop += control * friction * pml.frameTime;
		}
	}

	// --- Water friction ---
	if (pm->waterLevel > 0 && !(pm->s.pmFlags & PMF_ON_LADDER)) {
		drop += speed * pm_waterFriction * static_cast<float>(pm->waterLevel) * pml.frameTime;
	}

	// --- Scale velocity ---
	float newSpeed = speed - drop;
	if (newSpeed < 0.0f) {
		newSpeed = 0.0f;
	}

	const float scale = newSpeed / speed;
	vel *= scale;
}

/*
===============
PM_Accelerate
Handles user-intended acceleration.
===============
*/
static void PM_Accelerate(const Vector3& wishDir, float wishSpeed, float accel) {
	const float currentSpeed = pml.velocity.dot(wishDir);
	const float addSpeed = wishSpeed - currentSpeed;
	if (addSpeed <= 0.0f) {
		return;
	}

	float accelSpeed = accel * pml.frameTime * wishSpeed;
	if (accelSpeed > addSpeed) {
		accelSpeed = addSpeed;
	}

	pml.velocity += wishDir * accelSpeed;
}

/*
===============
PM_AirAccelerate
Handles air acceleration with a capped wish speed.
===============
*/
static void PM_AirAccelerate(const Vector3& wishDir, float wishSpeed, float accel) {
	// Cap wish speed to prevent excessive air acceleration
	const float cappedWishSpeed = (wishSpeed > 30.0f) ? 30.0f : wishSpeed;

	const float currentSpeed = pml.velocity.dot(wishDir);
	const float addSpeed = cappedWishSpeed - currentSpeed;
	if (addSpeed <= 0.0f) {
		return;
	}

	float accelSpeed = accel * wishSpeed * pml.frameTime;
	if (accelSpeed > addSpeed) {
		accelSpeed = addSpeed;
	}

	pml.velocity += wishDir * accelSpeed;
}

/*
===============
PM_AddCurrents
Adds ladder, water, and conveyor currents to the intended movement velocity.
===============
*/
static void PM_AddCurrents(Vector3& wishVel) {
	// --- Ladder handling ---
	if (pm->s.pmFlags & PMF_ON_LADDER) {
		// Vertical movement on ladder
		if (pm->cmd.buttons & (BUTTON_JUMP | BUTTON_CROUCH)) {
			// Full ladder speed when underwater
			const float ladderSpeed = (pm->waterLevel >= WATER_WAIST) ? MaxSpeed(&pm->s) : 200.0f;

			if (pm->cmd.buttons & BUTTON_JUMP)
				wishVel[2] = ladderSpeed;
			else
				wishVel[2] = -ladderSpeed;
		}
		else if (pm->cmd.forwardMove != 0) {
			// Clamp forward/backward ladder speed
			float ladderSpeed = std::clamp(static_cast<float>(pm->cmd.forwardMove), -200.0f, 200.0f);

			if (pm->cmd.forwardMove > 0) {
				// Climb up if looking mostly forward, otherwise down
				wishVel[2] = (pm->viewAngles[PITCH] < 15.0f) ? ladderSpeed : -ladderSpeed;
			}
			else {
				// Allow back arrow to climb down ladder
				if (!pm->groundEntity) {
					// prevent sliding off ladder when in air
					wishVel[0] = wishVel[1] = 0.0f;
				}
				wishVel[2] = ladderSpeed;
			}
		}
		else {
			wishVel[2] = 0.0f;
		}

		// Horizontal speed limiting while on ladder (unless grounded)
		if (!pm->groundEntity) {
			if (pm->cmd.sideMove) {
				// Clamp sideMove
				float ladderSpeed = std::clamp(static_cast<float>(pm->cmd.sideMove), -150.0f, 150.0f);
				if (pm->waterLevel < WATER_WAIST)
					ladderSpeed *= pm_ladderScale;

				// Check for ladder surface in front
				Vector3 forwardFlat = { pml.forward[0], pml.forward[1], 0.0f };
				forwardFlat.normalize();

				Vector3 spot = pml.origin + (forwardFlat * 1.0f);
				trace_t tr = PM_Trace(pml.origin, pm->mins, pm->maxs, spot, CONTENTS_LADDER);

				if (tr.fraction != 1.0f && (tr.contents & CONTENTS_LADDER)) {
					Vector3 right = tr.plane.normal.cross({ 0, 0, 1 });
					wishVel[0] = wishVel[1] = 0.0f;
					wishVel += right * -ladderSpeed;
				}
			}
			else {
				// Clamp residual horizontal velocity while on ladder
				wishVel[0] = std::clamp(wishVel[0], -25.0f, 25.0f);
				wishVel[1] = std::clamp(wishVel[1], -25.0f, 25.0f);
			}
		}
	}

	// --- Water currents ---
	if (pm->waterType & MASK_CURRENT) {
		Vector3 v = {};

		if (pm->waterType & CONTENTS_CURRENT_0)   v[0] += 1.0f;
		if (pm->waterType & CONTENTS_CURRENT_90)  v[1] += 1.0f;
		if (pm->waterType & CONTENTS_CURRENT_180) v[0] -= 1.0f;
		if (pm->waterType & CONTENTS_CURRENT_270) v[1] -= 1.0f;
		if (pm->waterType & CONTENTS_CURRENT_UP)  v[2] += 1.0f;
		if (pm->waterType & CONTENTS_CURRENT_DOWN)v[2] -= 1.0f;

		float scale = pm_waterSpeed;
		if (pm->waterLevel == WATER_FEET && pm->groundEntity)
			scale *= 0.5f;

		wishVel += v * scale;
	}

	// --- Conveyor belt currents (ground only) ---
	if (pm->groundEntity) {
		Vector3 v = {};

		if (pml.groundContents & CONTENTS_CURRENT_0)   v[0] += 1.0f;
		if (pml.groundContents & CONTENTS_CURRENT_90)  v[1] += 1.0f;
		if (pml.groundContents & CONTENTS_CURRENT_180) v[0] -= 1.0f;
		if (pml.groundContents & CONTENTS_CURRENT_270) v[1] -= 1.0f;
		if (pml.groundContents & CONTENTS_CURRENT_UP)  v[2] += 1.0f;
		if (pml.groundContents & CONTENTS_CURRENT_DOWN)v[2] -= 1.0f;

		wishVel += v * 100.0f;
	}
}

/*
===============
PM_WaterMove
Player movement while submerged: builds wish velocity from inputs, applies
currents, clamps to max/duck speeds, accelerates, and resolves via step/slide.
===============
*/
static void PM_WaterMove() {
	Vector3 wishVel = {};
	const float maxSpeed = MaxSpeed(&pm->s);

	// Build horizontal intent from inputs
	wishVel += pml.forward * static_cast<float>(pm->cmd.forwardMove);
	wishVel += pml.right * static_cast<float>(pm->cmd.sideMove);

	// Vertical intent
	if (!pm->cmd.forwardMove && !pm->cmd.sideMove &&
		!(pm->cmd.buttons & (BUTTON_JUMP | BUTTON_CROUCH))) {
		// No input: gently drift down if not grounded
		if (!pm->groundEntity) {
			wishVel[2] -= 60.0f;
		}
	}
	else {
		// Swim up/down with jump/crouch
		const float vStep = pm_waterSpeed * 0.5f;
		if (pm->cmd.buttons & BUTTON_CROUCH) {
			wishVel[2] -= vStep;
		}
		else if (pm->cmd.buttons & BUTTON_JUMP) {
			wishVel[2] += vStep;
		}
	}

	// Environmental currents (ladder, water, conveyors)
	PM_AddCurrents(wishVel);

	// Normalize to get wishDir and speed
	Vector3 wishDir = wishVel;
	float  wishSpeed = wishDir.normalize();

	// Clamp to max speed
	if (wishSpeed > maxSpeed) {
		const float scale = maxSpeed / wishSpeed;
		wishVel *= scale;
		wishSpeed = maxSpeed;
	}

	// Water halves effective speed for acceleration target
	wishSpeed *= 0.5f;

	// Ducking std::clamp
	if ((pm->s.pmFlags & PMF_DUCKED) && wishSpeed > pm_duckSpeed) {
		const float scale = pm_duckSpeed / wishSpeed;
		wishVel *= scale;
		wishSpeed = pm_duckSpeed;
	}

	// Accelerate toward wish direction/speed
	PM_Accelerate(wishDir, wishSpeed, pm_waterAccelerate);

	// Resolve motion against world
	PM_StepSlideMove();
}

/*
===============
PM_AirMove
Handles player movement in air or on ground when not fully submerged.
Covers ladder handling, ground walking, and true air control.
===============
*/
static void PM_AirMove() {
	// Build 2D wish velocity from inputs
	Vector3 wishVel = {};
	const float fMove = static_cast<float>(pm->cmd.forwardMove);
	const float sMove = static_cast<float>(pm->cmd.sideMove);

	wishVel[0] = pml.forward[0] * fMove + pml.right[0] * sMove;
	wishVel[1] = pml.forward[1] * fMove + pml.right[1] * sMove;
	wishVel[2] = 0.0f;

	// Environmental influences (ladder, water, conveyors)
	PM_AddCurrents(wishVel);

	// Normalize to get wish direction and speed
	Vector3 wishDir = wishVel;
	float  wishSpeed = wishDir.normalize();

	// Clamp to server-defined max speed (ducked vs normal)
	const float maxSpeed = (pm->s.pmFlags & PMF_DUCKED) ? pm_duckSpeed : MaxSpeed(&pm->s);
	if (wishSpeed > maxSpeed) {
		const float scale = maxSpeed / wishSpeed;
		wishVel *= scale;
		wishSpeed = maxSpeed;
	}

	// Ladder: accelerate along wish, then bias vertical velocity toward zero if no explicit ladder Z input
	if (pm->s.pmFlags & PMF_ON_LADDER) {
		PM_Accelerate(wishDir, wishSpeed, pm_accelerate);

		if (wishVel[2] == 0.0f) {
			const float gz = pm->s.gravity * pml.frameTime;
			if (pml.velocity[_Z] > 0.0f) {
				pml.velocity[_Z] = std::max(0.0f, pml.velocity[_Z] - gz);
			}
			else {
				pml.velocity[_Z] = std::min(0.0f, pml.velocity[_Z] + gz);
			}
		}

		PM_StepSlideMove();
		return;
	}

	// Grounded walking
	if (pm->groundEntity) {
		// Zero vertical before accel
		pml.velocity[_Z] = 0.0f;

		PM_Accelerate(wishDir, wishSpeed, pm_accelerate);

		// Preserve classic behavior: positive gravity locks Z to 0, negative gravity floats up
		if (pm->s.gravity <= 0.0f) {
			pml.velocity[_Z] -= pm->s.gravity * pml.frameTime; // gravity is negative, so this adds lift
		}
		else {
			pml.velocity[_Z] = 0.0f;
		}

		// If we are not moving horizontally, stop here
		if (pml.velocity[_X] == 0.0f && pml.velocity[_Y] == 0.0f)
			return;

		PM_StepSlideMove();
		return;
	}

	// True air movement
	// True air movement
	// If the knockback timer is active, DO NOT apply air acceleration.
	// This prevents player input from cancelling the knockback impulse.
	if (!pm->s.pmTime) {
		if (pm_config.airAccel) {
			PM_AirAccelerate(wishDir, wishSpeed, pm_config.airAccel);
		}
		else {
			PM_Accelerate(wishDir, wishSpeed, 1.0f);
		}
	}

	// Gravity while airborne (except grapple)
	if (pm->s.pmType != PM_GRAPPLE) {
		pml.velocity[_Z] -= pm->s.gravity * pml.frameTime;
	}

	PM_StepSlideMove();
}

/*
===============
PM_GetWaterLevel
Determines how submerged the player is (feet, waist, or fully under).
Accounts for ducking by sampling at multiple heights.
===============
*/
static inline void PM_GetWaterLevel(const Vector3& position, water_level_t& level, contents_t& type) {
	level = WATER_NONE;
	type = CONTENTS_NONE;

	// Vertical sample distances based on player view height and bbox
	const int sample2 = static_cast<int>(pm->s.viewHeight - pm->mins[2]);
	const int sample1 = sample2 / 2;

	Vector3 point = position;
	point[2] += pm->mins[2] + 1.0f;

	// Check at feet
	contents_t cont = pm->pointContents(point);
	if (!(cont & MASK_WATER)) {
		return; // not in water
	}

	type = cont;
	level = WATER_FEET;

	// Check at waist
	point[2] = pml.origin[_Z] + pm->mins[2] + static_cast<float>(sample1);
	cont = pm->pointContents(point);
	if (!(cont & MASK_WATER)) {
		return;
	}

	level = WATER_WAIST;

	// Check at head
	point[2] = pml.origin[_Z] + pm->mins[2] + static_cast<float>(sample2);
	cont = pm->pointContents(point);
	if (cont & MASK_WATER) {
		level = WATER_UNDER;
	}
}

/*
===============
PM_CatagorizePosition
Determines whether the player is on the ground, updates ground entity/surface,
handles special slope cases, and updates water level.
===============
*/
static void PM_CatagorizePosition() {
	// Check a point just below the player to see if we are standing on solid
	Vector3 point = pml.origin;
	point[2] -= 0.25f;

	// High upward velocity or grapple mode: force off-ground
	if (pml.velocity[_Z] > 180.0f || pm->s.pmType == PM_GRAPPLE) {
		pm->s.pmFlags &= ~PMF_ON_GROUND;
		pm->groundEntity = nullptr;
		PM_GetWaterLevel(pml.origin, pm->waterLevel, pm->waterType);
		return;
	}

	// Trace downward
	trace_t tr = PM_Trace(pml.origin, pm->mins, pm->maxs, point);
	pm->groundPlane = tr.plane;
	pml.groundSurface = tr.surface;
	pml.groundContents = tr.contents;

	// Detect potentially bad "slanted ground" where player can wedge into wall
	bool slantedGround = (tr.fraction < 1.0f && tr.plane.normal[2] < 0.7f);
	if (slantedGround) {
		trace_t slant = PM_Trace(pml.origin, pm->mins, pm->maxs, pml.origin + tr.plane.normal);
		if (slant.fraction < 1.0f && !slant.startSolid) {
			slantedGround = false;
		}
	}

	if (tr.fraction == 1.0f || (slantedGround && !tr.startSolid)) {
		// Not on solid ground
		pm->groundEntity = nullptr;
		pm->s.pmFlags &= ~PMF_ON_GROUND;
	}
	else {
		// On ground
		pm->groundEntity = tr.ent;

		// Touching solid ground ends waterjump
		if (pm->s.pmFlags & PMF_TIME_WATERJUMP) {
			pm->s.pmFlags &= ~(PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_KNOCKBACK | PMF_TIME_TRICK);
			pm->s.pmTime = 0;
		}

		if (!(pm->s.pmFlags & PMF_ON_GROUND)) {
			// Just landed

			// Trick flag (Paril-KEX: N64 physics skip this)
			if (!pm_config.n64Physics &&
				pml.velocity[_Z] >= 100.0f &&
				pm->groundPlane.normal[2] >= 0.9f &&
				!(pm->s.pmFlags & PMF_DUCKED)) {
				pm->s.pmFlags |= PMF_TIME_TRICK;
				pm->s.pmTime = 64;
			}

			// Compute impact delta for fall/land handling
			Vector3 clippedVelocity;
			PM_ClipVelocity(pml.velocity, pm->groundPlane.normal, clippedVelocity, PM_GetOverbounceFactor());
			pm->impactDelta = pml.startVelocity[2] - clippedVelocity[2];

			pm->s.pmFlags |= PMF_ON_GROUND;

			// Land lag when ducked or in N64 physics mode
			if (pm_config.n64Physics || (pm->s.pmFlags & PMF_DUCKED)) {
				pm->s.pmFlags |= PMF_TIME_LAND;
				pm->s.pmTime = 128;
			}
		}
	}

	// Record ground trace for touch list
	PM_RecordTrace(pm->touch, tr);

	// Update water level
	PM_GetWaterLevel(pml.origin, pm->waterLevel, pm->waterType);
}

/*
===============
PM_CheckJump
Checks input and state to determine if a jump should occur.
Handles jump hold, landing delay, water cases, and vertical boost.
===============
*/
static void PM_CheckJump() {
	// Too soon after landing to jump again
	if (pm->s.pmFlags & PMF_TIME_LAND) {
		return;
	}

	// Jump button not held
	if (!(pm->cmd.buttons & BUTTON_JUMP)) {
		pm->s.pmFlags &= ~PMF_JUMP_HELD;
		return;
	}

	// Must release jump before pressing again
	if (pm->s.pmFlags & PMF_JUMP_HELD) {
		return;
	}

	// Dead players cannot jump
	if (pm->s.pmType == PM_DEAD) {
		return;
	}

	// Swimming: treat as no ground jump
	if (pm->waterLevel >= WATER_WAIST) {
		pm->groundEntity = nullptr;
		return;
	}

	// Must be on ground to jump
	if (!pm->groundEntity) {
		return;
	}

	// Initiate jump
	pm->s.pmFlags |= PMF_JUMP_HELD;
	pm->jumpSound = true;
	pm->groundEntity = nullptr;
	pm->s.pmFlags &= ~PMF_ON_GROUND;

	float jumpHeight = 270.0f;
	if (pml.origin[_Z] < 0.0f) {
		jumpHeight += 4.0f;
	}

	// Apply vertical boost, preserving upward momentum if already rising
	pml.velocity[_Z] = std::ceil(pml.velocity[_Z] + jumpHeight);
	if (pml.velocity[_Z] < jumpHeight) {
		pml.velocity[_Z] = jumpHeight;
	}
}

/*
===============
PM_CheckSpecialMovement
Checks for ladders and waterjump opportunities, updating pm flags and
velocity accordingly.
===============
*/
static void PM_CheckSpecialMovement() {
	// Cannot perform special moves during pmTime countdown
	if (pm->s.pmTime) {
		return;
	}

	// Reset ladder flag
	pm->s.pmFlags &= ~PMF_ON_LADDER;

	// --- Ladder detection ---
	Vector3 flatForward = { pml.forward[0], pml.forward[1], 0.0f };
	flatForward.normalize();

	Vector3 spot = pml.origin + flatForward * 1.0f;
	trace_t tr = PM_Trace(pml.origin, pm->mins, pm->maxs, spot, CONTENTS_LADDER);

	if (tr.fraction < 1.0f &&
		(tr.contents & CONTENTS_LADDER) &&
		pm->waterLevel < WATER_WAIST) {
		pm->s.pmFlags |= PMF_ON_LADDER;
	}

	// No gravity = skip waterjump
	if (pm->s.gravity == 0.0f) {
		return;
	}

	// --- Waterjump detection ---
	// Must be moving forward or pressing jump
	if (!(pm->cmd.buttons & BUTTON_JUMP) && pm->cmd.forwardMove <= 0) {
		return;
	}
	if (pm->waterLevel != WATER_WAIST) {
		return;
	}
	if (pm->waterType & CONTENTS_NO_WATERJUMP) {
		return;
	}

	// Check if blocked in front
	tr = PM_Trace(pml.origin, pm->mins, pm->maxs, pml.origin + flatForward * 40.0f, MASK_SOLID);
	if (tr.fraction == 1.0f || tr.plane.normal.z >= 0.7f) {
		return;
	}

	// Simulate forward waterjump trajectory
	Vector3 waterjumpVel = flatForward * 50.0f;
	waterjumpVel[2] = 350.0f;

	Vector3 waterjumpOrigin = pml.origin;
	touch_list_t touches;
	bool hasTime = true;
	const float stepTime = 0.1f;

	const int maxSteps = std::min(50, static_cast<int>(10.0f * (800.0f / pm->s.gravity)));
	for (int i = 0; i < maxSteps; ++i) {
		waterjumpVel[2] -= pm->s.gravity * stepTime;
		if (waterjumpVel[2] < 0.0f) {
			hasTime = false;
		}
		PM_StepSlideMove_Generic(waterjumpOrigin, waterjumpVel, stepTime,
			pm->mins, pm->maxs, touches, hasTime, PM_Trace_Auto);
	}

	// Snap down to test if we can stand at the end of the jump
	tr = PM_Trace(waterjumpOrigin, pm->mins, pm->maxs,
		waterjumpOrigin - Vector3{ 0, 0, 2.0f }, MASK_SOLID);

	// Invalid landing
	if (tr.fraction == 1.0f || tr.plane.normal.z < 0.7f || tr.endPos.z < pml.origin.z) {
		return;
	}

	// Already standing on ground at a valid step height
	const float stepSize = (pml.origin[_Z] < 0.0f) ? STEPSIZE_BELOW : STEPSIZE;
	if (pm->groundEntity && std::fabs(pml.origin.z - tr.endPos.z) <= stepSize) {
		return;
	}

	// Ensure target spot is not underwater
	water_level_t level;
	contents_t type;
	PM_GetWaterLevel(tr.endPos, level, type);
	if (level >= WATER_WAIST) {
		return;
	}

	// Valid waterjump! Commit jump
	pml.velocity = flatForward * 50.0f;
	pml.velocity[_Z] = 350.0f;

	pm->s.pmFlags |= PMF_TIME_WATERJUMP;
	pm->s.pmTime = 2048;
}

/*
===============
PM_FlyMove
Handles free-flying movement (noclip/flight). Applies friction, input acceleration,
and optionally clips against world geometry.
===============
*/
static void PM_FlyMove(bool doClip) {
	const float maxSpeed = MaxSpeed(&pm->s);

	// Adjust view height depending on clipping mode
	pm->s.viewHeight = doClip ? 0 : 22;

	// --- Apply friction ---
	const float speed = pml.velocity.length();
	if (speed >= 1.0f) {
		const float friction = pm_friction * 1.5f;
		const float control = (speed < pm_stopSpeed) ? pm_stopSpeed : speed;
		const float drop = control * friction * pml.frameTime;

		const float newSpeed = std::max(0.0f, speed - drop) / speed;
		pml.velocity *= newSpeed;
	}
	else {
		pml.velocity = {};
	}

	// --- Build desired movement vector ---
	const float fMove = static_cast<float>(pm->cmd.forwardMove);
	const float sMove = static_cast<float>(pm->cmd.sideMove);

	Vector3 wishVel = (pml.forward.normalized() * fMove) +
		(pml.right.normalized() * sMove);

	if (pm->cmd.buttons & BUTTON_JUMP) {
		wishVel[2] += pm_waterSpeed * 0.5f;
	}
	if (pm->cmd.buttons & BUTTON_CROUCH) {
		wishVel[2] -= pm_waterSpeed * 0.5f;
	}

	Vector3 wishDir = wishVel;
	float  wishSpeed = wishDir.normalize();

	// Clamp to server max speed
	if (wishSpeed > maxSpeed) {
		const float scale = maxSpeed / wishSpeed;
		wishVel *= scale;
		wishSpeed = maxSpeed;
	}

	// Paril-KEX tweak: double fly movement speed
	wishSpeed *= 2.0f;

	// --- Accelerate toward desired velocity ---
	const float currentSpeed = pml.velocity.dot(wishDir);
	const float addSpeed = wishSpeed - currentSpeed;

	if (addSpeed > 0.0f) {
		float accelSpeed = pm_accelerate * pml.frameTime * wishSpeed;
		accelSpeed = std::min(accelSpeed, addSpeed);
		pml.velocity += wishDir * accelSpeed;
	}

	// --- Apply motion ---
	if (doClip) {
		PM_StepSlideMove();
	}
	else {
		pml.origin += pml.velocity * pml.frameTime;
	}
}

/*
===============
PM_SetDimensions
Sets player bounding box (mins/maxs) and view height based on state.
===============
*/
static void PM_SetDimensions() {
	// Fixed horizontal size
	pm->mins[0] = -16.0f;
	pm->mins[1] = -16.0f;
	pm->maxs[0] = 16.0f;
	pm->maxs[1] = 16.0f;

	// Gib state: tiny bbox and low view height
	if (pm->s.pmType == PM_GIB) {
		pm->mins[2] = 0.0f;
		pm->maxs[2] = 16.0f;
		pm->s.viewHeight = 8;
		return;
	}

	// Normal vertical min
	pm->mins[2] = -24.0f;

	// Ducking or dead: short bbox
	if ((pm->s.pmFlags & PMF_DUCKED) || pm->s.pmType == PM_DEAD) {
		pm->maxs[2] = 4.0f;
		pm->s.viewHeight = -2;
	}
	else {
		// Standing
		pm->maxs[2] = 32.0f;
		pm->s.viewHeight = DEFAULT_VIEWHEIGHT;
	}
}

/*
===============
PM_AboveWater
Checks if player is positioned directly above water (with no solid below).
===============
*/
static inline bool PM_AboveWater() {
	const Vector3 below = pml.origin - Vector3{ 0.0f, 0.0f, 8.0f };

	// First check: is there solid immediately below?
	const bool solidBelow =
		pm->trace(pml.origin, &pm->mins, &pm->maxs, below, pm->player, MASK_SOLID).fraction < 1.0f;
	if (solidBelow) {
		return false;
	}

	// Second check: is there water immediately below?
	const bool waterBelow =
		pm->trace(pml.origin, &pm->mins, &pm->maxs, below, pm->player, MASK_WATER).fraction < 1.0f;
	return waterBelow;
}

/*
===============
PM_CheckDuck
Updates ducking state (mins, maxs, view height) based on player input and environment.
Returns true if flags/dimensions changed.
===============
*/
static bool PM_CheckDuck() {
	// Gibs never duck
	if (pm->s.pmType == PM_GIB) {
		return false;
	}

	bool flagsChanged = false;

	// --- Dead players are always ducked ---
	if (pm->s.pmType == PM_DEAD) {
		if (!(pm->s.pmFlags & PMF_DUCKED)) {
			pm->s.pmFlags |= PMF_DUCKED;
			flagsChanged = true;
		}
	}
	// --- Ducking input ---
	else if ((pm->cmd.buttons & BUTTON_CROUCH) &&
		(pm->groundEntity || (pm->waterLevel <= WATER_FEET && !PM_AboveWater())) &&
		!(pm->s.pmFlags & PMF_ON_LADDER) &&
		!pm_config.n64Physics) {
		if (!(pm->s.pmFlags & PMF_DUCKED)) {
			// Check head clearance for duck bbox
			Vector3 checkMaxs = { pm->maxs[0], pm->maxs[1], 4.0f };
			trace_t tr = PM_Trace(pml.origin, pm->mins, checkMaxs, pml.origin);
			if (!tr.allSolid) {
				pm->s.pmFlags |= PMF_DUCKED;
				flagsChanged = true;
			}
		}
	}
	// --- Standing up ---
	else {
		if (pm->s.pmFlags & PMF_DUCKED) {
			// Check head clearance for standing bbox
			Vector3 checkMaxs = { pm->maxs[0], pm->maxs[1], 32.0f };
			trace_t tr = PM_Trace(pml.origin, pm->mins, checkMaxs, pml.origin);
			if (!tr.allSolid) {
				pm->s.pmFlags &= ~PMF_DUCKED;
				flagsChanged = true;
			}
		}
	}

	if (!flagsChanged) {
		return false;
	}

	// Update dimensions when state changes
	PM_SetDimensions();
	return true;
}

/*
===============
PM_DeadMove
Applies heavy friction when dead, slowing velocity to a stop.
===============
*/
static void PM_DeadMove() {
	// Must be on the ground to apply dead-move friction
	if (!pm->groundEntity) {
		return;
	}

	float speed = pml.velocity.length() - 20.0f;
	if (speed <= 0.0f) {
		pml.velocity = {};
	}
	else {
		pml.velocity = pml.velocity.normalized() * speed;
	}
}

/*
===============
PM_GoodPosition
Checks if current origin is a valid non-solid position.
===============
*/
static bool PM_GoodPosition() {
	if (pm->s.pmType == PM_NOCLIP) {
		return true;
	}

	const trace_t tr = PM_Trace(pm->s.origin, pm->mins, pm->maxs, pm->s.origin);
	return !tr.allSolid;
}

/*
===============
PM_SnapPosition
Quantizes and validates the player's origin at the end of a move.
Falls back to previous origin if no good position can be found.
===============
*/
static void PM_SnapPosition() {
	pm->s.velocity = pml.velocity;
	pm->s.origin = pml.origin;

	if (PM_GoodPosition()) {
		return;
	}

	if (G_FixStuckObject_Generic(pm->s.origin, pm->mins, pm->maxs, PM_Trace_Auto) ==
		StuckResult::NoGoodPosition) {
		pm->s.origin = pml.previousOrigin;
	}
}

/*
===============
PM_InitialSnapPosition
Attempts to place the player in a valid starting origin by checking nearby
offsets around the intended spawn location.
===============
*/
static void PM_InitialSnapPosition() {
	constexpr int offsets[3] = { 0, -1, 1 };
	const Vector3 base = pm->s.origin;

	for (int z : offsets) {
		pm->s.origin[_Z] = base[2] + z;
		for (int y : offsets) {
			pm->s.origin[_Y] = base[1] + y;
			for (int x : offsets) {
				pm->s.origin[_X] = base[0] + x;
				if (PM_GoodPosition()) {
					pml.origin = pm->s.origin;
					pml.previousOrigin = pm->s.origin;
					return;
				}
			}
		}
	}
}

/*
===============
PM_ClampAngles
Clamps view angles to valid ranges, handling knockback lockout.
===============
*/
static void PM_ClampAngles() {
	if (pm->s.pmFlags & PMF_TIME_KNOCKBACK) {
		// Knockback: lock pitch/roll, only update yaw
		pm->viewAngles[YAW] = pm->cmd.angles[YAW] + pm->s.deltaAngles[YAW];
		pm->viewAngles[PITCH] = 0.0f;
		pm->viewAngles[ROLL] = 0.0f;
	}
	else {
		// Add command + delta
		pm->viewAngles = pm->cmd.angles + pm->s.deltaAngles;

		// Clamp pitch: [-89, +89] degrees
		if (pm->viewAngles[PITCH] > 89.0f && pm->viewAngles[PITCH] < 180.0f) {
			pm->viewAngles[PITCH] = 89.0f;
		}
		else if (pm->viewAngles[PITCH] >= 180.0f && pm->viewAngles[PITCH] < 271.0f) {
			pm->viewAngles[PITCH] = 271.0f;
		}
	}

	// Recompute directional vectors
	AngleVectors(pm->viewAngles, pml.forward, pml.right, pml.up);
}

/*
===============
PM_ScreenEffects
Applies screen effects (blend and underwater flag) based on player contents.
===============
*/
static void PM_ScreenEffects() {
	// Sample position at view origin
	const Vector3 viewOrg = pml.origin + pm->viewOffset +
		Vector3{ 0.0f, 0.0f, static_cast<float>(pm->s.viewHeight) };
	const contents_t contents = pm->pointContents(viewOrg);

	// Set underwater render flag
	if (contents & (CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_WATER)) {
		pm->rdFlags |= RDF_UNDERWATER;
	}
	else {
		pm->rdFlags &= ~RDF_UNDERWATER;
	}

	// Apply blends for special volumes
	if (contents & (CONTENTS_SOLID | CONTENTS_LAVA)) {
		G_AddBlend(1.0f, 0.3f, 0.0f, 0.6f, pm->screenBlend);
	}
	else if (contents & CONTENTS_SLIME) {
		G_AddBlend(0.0f, 0.1f, 0.05f, 0.6f, pm->screenBlend);
	}
	else if (contents & CONTENTS_WATER) {
		G_AddBlend(0.5f, 0.3f, 0.2f, 0.4f, pm->screenBlend);
	}
}

/*
===============
Pmove
Performs one player movement frame.
Can be called by either the server or the client.
===============
*/
void Pmove(PMove* pmove) {
	pm = pmove;

	// --- Clear results ---
	pm->touch.num = 0;
	pm->viewAngles = {};
	pm->s.viewHeight = 0;
	pm->groundEntity = nullptr;
	pm->waterType = CONTENTS_NONE;
	pm->waterLevel = WATER_NONE;
	pm->screenBlend = {};
	pm->rdFlags = RDF_NONE;
	pm->jumpSound = false;
	pm->stepClip = false;
	pm->impactDelta = 0;

	// --- Reset local move state ---
	pml = {};
	pml.origin = pm->s.origin;
	pml.velocity = pm->s.velocity;
	pml.startVelocity = pml.velocity;
	pml.previousOrigin = pm->s.origin; // for stuck fallback
	pml.frameTime = pm->cmd.msec * 0.001f;

	// Compute view vectors
	PM_ClampAngles();

	// --- Spectator / noclip ---
	if (pm->s.pmType == PM_SPECTATOR || pm->s.pmType == PM_NOCLIP) {
		pm->s.pmFlags = PMF_NONE;

		// Spectators use a smaller box
		if (pm->s.pmType == PM_SPECTATOR) {
			pm->mins = { -8, -8, -8 };
			pm->maxs = { 8,  8,  8 };
		}

		PM_FlyMove(pm->s.pmType == PM_SPECTATOR);
		PM_SnapPosition();
		PM_ScreenEffects();
		return;
	}

	// --- Dead/frozen states ---
	if (pm->s.pmType >= PM_DEAD) {
		pm->cmd.forwardMove = 0;
		pm->cmd.sideMove = 0;
		pm->cmd.buttons &= ~(BUTTON_JUMP | BUTTON_CROUCH);
	}
	if (pm->s.pmType == PM_FREEZE) {
		return; // No movement at all
	}

	// --- Dimensions & categorization ---
	PM_SetDimensions();
	PM_CatagorizePosition();

	if (pm->snapInitial) {
		PM_InitialSnapPosition();
	}

	// Re-check duck state, which may affect groundEntity
	if (PM_CheckDuck()) {
		PM_CatagorizePosition();
	}

	// --- Movement types ---
	if (pm->s.pmType == PM_DEAD) {
		PM_DeadMove();
	}

	PM_CheckSpecialMovement();

	// --- Drop timers ---
	if (pm->s.pmTime) {
		if (pm->cmd.msec >= pm->s.pmTime) {
			pm->s.pmFlags &= ~(PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_KNOCKBACK | PMF_TIME_TRICK);
			pm->s.pmTime = 0;
		}
		else {
			pm->s.pmTime -= pm->cmd.msec;
		}
	}

	// --- Knockback / waterjump / normal movement ---
	if (pm->s.pmFlags & PMF_TIME_KNOCKBACK) {
		// Knockback freeze: stay in place
	}
	else if (pm->s.pmFlags & PMF_TIME_WATERJUMP) {
		// Waterjump: ballistic arc, no control
		pml.velocity[_Z] -= pm->s.gravity * pml.frameTime;

		// Cancel when falling again
		if (pml.velocity[_Z] < 0.0f) {
			pm->s.pmFlags &= ~(PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_KNOCKBACK | PMF_TIME_TRICK);
			pm->s.pmTime = 0;
		}

		PM_StepSlideMove();
	}
	else {
		// Normal movement path
		PM_CheckJump();
		PM_Friction();

		if (pm->waterLevel >= WATER_WAIST) {
			PM_WaterMove();
		}
		else {
			// Build direction vectors with reduced pitch
			Vector3 angles = pm->viewAngles;
			if (angles[PITCH] > 180.0f) {
				angles[PITCH] -= 360.0f;
			}
			angles[PITCH] /= 3.0f;

			AngleVectors(angles, pml.forward, pml.right, pml.up);
			PM_AirMove();
		}
	}

	// --- Final categorization ---
	PM_CatagorizePosition();

	// Trick jump retry
	if (pm->s.pmFlags & PMF_TIME_TRICK) {
		PM_CheckJump();
	}

	// Visual effects & snap
	PM_ScreenEffects();
	PM_SnapPosition();
}
