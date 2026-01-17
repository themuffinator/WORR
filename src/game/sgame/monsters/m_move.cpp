/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

m_move.c -- monster movement*/

#include "../g_local.hpp"

// this is used for communications out of g_movestep to say what entity
// is blocking us
gentity_t *new_bad; // pmm

/*
=============
M_CheckBottom

Returns false if any part of the bottom of the entity is off an edge that
is not a staircase.

=============
*/
bool M_CheckBottom_Fast_Generic(const Vector3 &absmins, const Vector3 &absmaxs, const Vector3 &gravityDir) {
	Vector3 start;
	int majorAxis = 0;
	if (fabsf(gravityDir[1]) > fabsf(gravityDir[0])) majorAxis = 1;
	if (fabsf(gravityDir[2]) > fabsf(gravityDir[majorAxis])) majorAxis = 2;

	int axis1 = (majorAxis + 1) % 3;
	int axis2 = (majorAxis + 2) % 3;

	if (gravityDir[majorAxis] > 0) // ceiling / up
		start[majorAxis] = absmaxs[majorAxis] + 1;
	else // floor / down
		start[majorAxis] = absmins[majorAxis] - 1;

	for (int i = 0; i <= 1; i++)
		for (int j = 0; j <= 1; j++) {
			start[axis1] = i ? absmaxs[axis1] : absmins[axis1];
			start[axis2] = j ? absmaxs[axis2] : absmins[axis2];
			if (gi.pointContents(start) != CONTENTS_SOLID)
				return false;
		}

	return true; // we got out easy
}

bool M_CheckBottom_Slow_Generic(const Vector3 &origin, const Vector3 &mins, const Vector3 &maxs, gentity_t *ignore, contents_t mask, const Vector3 &gravityDir, bool allow_any_step_height) {
	Vector3 start, stop;
	int majorAxis = 0;
	if (fabsf(gravityDir[1]) > fabsf(gravityDir[0])) majorAxis = 1;
	if (fabsf(gravityDir[2]) > fabsf(gravityDir[majorAxis])) majorAxis = 2;

	int axis1 = (majorAxis + 1) % 3;
	int axis2 = (majorAxis + 2) % 3;

	//
	// check it for real...
	//
	Vector3 step_quadrant_size = (maxs - mins) * 0.5f;
	step_quadrant_size[majorAxis] = 0;

	Vector3 half_step_quadrant = step_quadrant_size * 0.5f;
	Vector3 half_step_quadrant_mins = -half_step_quadrant;

	start[axis1] = stop[axis1] = origin[axis1];
	start[axis2] = stop[axis2] = origin[axis2];

	if (gravityDir[majorAxis] > 0) { // ceiling / up
		start[majorAxis] = origin[majorAxis] + maxs[majorAxis];
		stop[majorAxis] = start[majorAxis] + STEPSIZE * 2;
	} else { // floor / down
		start[majorAxis] = origin[majorAxis] + mins[majorAxis];
		stop[majorAxis] = start[majorAxis] - STEPSIZE * 2;
	}

	Vector3 mins_flat = mins;
	Vector3 maxs_flat = maxs;
	mins_flat[majorAxis] = maxs_flat[majorAxis] = 0;

	trace_t trace = gi.trace(start, mins_flat, maxs_flat, stop, ignore, mask);

	if (trace.fraction == 1.0f)
		return false;

	// [Paril-KEX]
	if (allow_any_step_height)
		return true;

	start[axis1] = stop[axis1] = origin[axis1] + ((mins[axis1] + maxs[axis1]) * 0.5f);
	start[axis2] = stop[axis2] = origin[axis2] + ((mins[axis2] + maxs[axis2]) * 0.5f);

	float mid = trace.endPos[majorAxis];

	// the corners must be within 16 of the midpoint
	for (int32_t i = 0; i <= 1; i++)
		for (int32_t j = 0; j <= 1; j++) {
			Vector3 quadrant_start = start;

			if (i)
				quadrant_start[axis1] += half_step_quadrant[axis1];
			else
				quadrant_start[axis1] -= half_step_quadrant[axis1];

			if (j)
				quadrant_start[axis2] += half_step_quadrant[axis2];
			else
				quadrant_start[axis2] -= half_step_quadrant[axis2];

			Vector3 quadrant_end = quadrant_start;
			quadrant_end[majorAxis] = stop[majorAxis];

			trace = gi.trace(quadrant_start, half_step_quadrant_mins, half_step_quadrant, quadrant_end, ignore, mask);

			if (gravityDir[majorAxis] > 0) {
				if (trace.fraction == 1.0f || trace.endPos[majorAxis] - mid > (STEPSIZE))
					return false;
			} else {
				if (trace.fraction == 1.0f || mid - trace.endPos[majorAxis] > (STEPSIZE))
					return false;
			}
		}

	return true;
}

bool M_CheckBottom(gentity_t *ent) {
	// if all of the points under the corners are solid world, don't bother
	// with the tougher checks

	if (M_CheckBottom_Fast_Generic(ent->s.origin + ent->mins, ent->s.origin + ent->maxs, ent->gravityVector))
		return true; // we got out easy

	contents_t mask = (ent->svFlags & SVF_MONSTER) ? MASK_MONSTERSOLID : (MASK_SOLID | CONTENTS_MONSTER | CONTENTS_PLAYER);
	return M_CheckBottom_Slow_Generic(ent->s.origin, ent->mins, ent->maxs, ent, mask, ent->gravityVector, ent->spawnFlags.has(SPAWNFLAG_MONSTER_SUPER_STEP));
}

static bool IsBadAhead(gentity_t *self, gentity_t *bad, const Vector3 &move) {
	Vector3 dir;
	Vector3 forward;
	float  dp_bad, dp_move;
	Vector3 move_copy;

	move_copy = move;

	dir = bad->s.origin - self->s.origin;
	dir.normalize();
	AngleVectors(self->s.angles, forward, nullptr, nullptr);
	dp_bad = forward.dot(dir);

	move_copy.normalize();
	AngleVectors(self->s.angles, forward, nullptr, nullptr);
	dp_move = forward.dot(move_copy);

	if ((dp_bad < 0) && (dp_move < 0))
		return true;
	if ((dp_bad > 0) && (dp_move > 0))
		return true;

	return false;
}

static Vector3 G_IdealHoverPosition(gentity_t *ent) {
	if ((!ent->enemy && !(ent->monsterInfo.aiFlags & AI_MEDIC)) || (ent->monsterInfo.aiFlags & (AI_COMBAT_POINT | AI_SOUND_TARGET | AI_HINT_PATH | AI_PATHING)))
		return { 0, 0, 0 }; // go right for the center

	// pick random direction
	float theta = frandom(2 * PIf);
	float phi;

	// buzzards pick half sphere
	if (ent->monsterInfo.fly_above)
		phi = acos(0.7f + frandom(0.3f));
	else if (ent->monsterInfo.fly_buzzard || (ent->monsterInfo.aiFlags & AI_MEDIC))
		phi = acos(frandom());
	// non-buzzards pick a level around the center
	else
		phi = acos(crandom() * 0.06f);

	Vector3 d{
		sin(phi) * cos(theta),
		sin(phi) * sin(theta),
		cos(phi)
	};

	return d * frandom(ent->monsterInfo.fly_min_distance, ent->monsterInfo.fly_max_distance);
}

static inline bool G_flystep_testvisposition(Vector3 start, Vector3 end, Vector3 starta, Vector3 startb, gentity_t *ent) {
	trace_t tr = gi.traceLine(start, end, ent, MASK_SOLID | CONTENTS_MONSTERCLIP);

	if (tr.fraction == 1.0f) {
		tr = gi.trace(starta, ent->mins, ent->maxs, startb, ent, MASK_SOLID | CONTENTS_MONSTERCLIP);

		if (tr.fraction == 1.0f)
			return true;
	}

	return false;
}

static bool G_alternate_flystep(gentity_t *ent, Vector3 move, bool relink, gentity_t *current_bad) {
	// swimming monsters just follow their velocity in the air
	if ((ent->flags & FL_SWIM) && ent->waterLevel < WATER_UNDER)
		return true;

	if (ent->monsterInfo.fly_position_time <= level.time ||
		(ent->enemy && ent->monsterInfo.fly_pinned && !visible(ent, ent->enemy))) {
		ent->monsterInfo.fly_pinned = false;
		ent->monsterInfo.fly_position_time = level.time + random_time(3_sec, 10_sec);
		ent->monsterInfo.fly_ideal_position = G_IdealHoverPosition(ent);
	}

	Vector3 towards_origin, towards_velocity = {};

	float currentSpeed;
	Vector3 dir = ent->velocity.normalized(currentSpeed);

	// FIXME
	if (isnan(dir[0]) || isnan(dir[1]) || isnan(dir[2])) {
#if defined(_DEBUG) && defined(_WIN32)
		__debugbreak();
#endif
		return false;
	}

	if (ent->monsterInfo.aiFlags & AI_PATHING)
		towards_origin = (ent->monsterInfo.nav_path.returnCode == PathReturnCode::TraversalPending) ?
		ent->monsterInfo.nav_path.secondMovePoint : ent->monsterInfo.nav_path.firstMovePoint;
	else if (ent->enemy && !(ent->monsterInfo.aiFlags & (AI_COMBAT_POINT | AI_SOUND_TARGET | AI_LOST_SIGHT))) {
		towards_origin = ent->enemy->s.origin;
		towards_velocity = ent->enemy->velocity;
	} else if (ent->goalEntity)
		towards_origin = ent->goalEntity->s.origin;
	else // what we're going towards probably died or something
	{
		// change speed
		if (currentSpeed) {
			if (currentSpeed > 0)
				currentSpeed = max(0.f, currentSpeed - ent->monsterInfo.fly_acceleration);
			else if (currentSpeed < 0)
				currentSpeed = min(0.f, currentSpeed + ent->monsterInfo.fly_acceleration);

			ent->velocity = dir * currentSpeed;
		}

		return true;
	}

	Vector3 wanted_pos;

	if (ent->monsterInfo.fly_pinned)
		wanted_pos = ent->monsterInfo.fly_ideal_position;
	else if (ent->monsterInfo.aiFlags & (AI_PATHING | AI_COMBAT_POINT | AI_SOUND_TARGET | AI_LOST_SIGHT))
		wanted_pos = towards_origin;
	else
		wanted_pos = (towards_origin + (towards_velocity * 0.25f)) + ent->monsterInfo.fly_ideal_position;

	// find a place we can fit in from here
	trace_t tr = gi.trace(towards_origin, { -8.f, -8.f, -8.f }, { 8.f, 8.f, 8.f }, wanted_pos, ent, MASK_SOLID | CONTENTS_MONSTERCLIP);

	if (!tr.allSolid)
		wanted_pos = tr.endPos;

	float dist_to_wanted;
	Vector3 dest_diff = (wanted_pos - ent->s.origin);

	if (dest_diff.z > ent->mins.z && dest_diff.z < ent->maxs.z)
		dest_diff.z = 0;

	Vector3 wanted_dir = dest_diff.normalized(dist_to_wanted);

	if (!(ent->monsterInfo.aiFlags & AI_MANUAL_STEERING))
		ent->ideal_yaw = vectoyaw((towards_origin - ent->s.origin).normalized());

	// check if we're blocked from moving this way from where we are
	tr = gi.trace(ent->s.origin, ent->mins, ent->maxs, ent->s.origin + (wanted_dir * ent->monsterInfo.fly_acceleration), ent, MASK_SOLID | CONTENTS_MONSTERCLIP);

	Vector3 aim_fwd, aim_rgt, aim_up;
	Vector3 yaw_angles = { 0, ent->s.angles.y, 0 };

	AngleVectors(yaw_angles, aim_fwd, aim_rgt, aim_up);

	// it's a fairly close block, so we may want to shift more dramatically
	if (tr.fraction < 0.25f) {
		bool bottom_visible = G_flystep_testvisposition(ent->s.origin + Vector3{ 0, 0, ent->mins.z }, wanted_pos,
			ent->s.origin, ent->s.origin + Vector3{ 0, 0, ent->mins.z - ent->monsterInfo.fly_acceleration }, ent);
		bool top_visible = G_flystep_testvisposition(ent->s.origin + Vector3{ 0, 0, ent->maxs.z }, wanted_pos,
			ent->s.origin, ent->s.origin + Vector3{ 0, 0, ent->maxs.z + ent->monsterInfo.fly_acceleration }, ent);

		// top & bottom are same, so we need to try right/left
		if (bottom_visible == top_visible) {
			bool left_visible = gi.traceLine(ent->s.origin + aim_fwd.scaled(ent->maxs) - aim_rgt.scaled(ent->maxs), wanted_pos, ent, MASK_SOLID | CONTENTS_MONSTERCLIP).fraction == 1.0f;
			bool right_visible = gi.traceLine(ent->s.origin + aim_fwd.scaled(ent->maxs) + aim_rgt.scaled(ent->maxs), wanted_pos, ent, MASK_SOLID | CONTENTS_MONSTERCLIP).fraction == 1.0f;

			if (left_visible != right_visible) {
				if (right_visible)
					wanted_dir += aim_rgt;
				else
					wanted_dir -= aim_rgt;
			} else
				// we're probably stuck, push us directly away
				wanted_dir = tr.plane.normal;
		} else {
			if (top_visible)
				wanted_dir += aim_up;
			else
				wanted_dir -= aim_up;
		}

		wanted_dir.normalize();
	}

	// the closer we are to zero, the more we can change dir.
	// if we're pushed past our max speed we shouldn't
	// turn at all.
	float turn_factor;

	if (((ent->monsterInfo.fly_thrusters && !ent->monsterInfo.fly_pinned) || ent->monsterInfo.aiFlags & (AI_PATHING | AI_COMBAT_POINT | AI_LOST_SIGHT)) && dir.dot(wanted_dir) > 0.0f)
		turn_factor = 0.45f;
	else
		turn_factor = min(1.f, 0.84f + (0.08f * (currentSpeed / ent->monsterInfo.fly_speed)));

	Vector3 final_dir = dir ? dir : wanted_dir;

	// FIXME
	if (isnan(final_dir[0]) || isnan(final_dir[1]) || isnan(final_dir[2])) {
#if defined(_DEBUG) && defined(_WIN32)
		__debugbreak();
#endif
		return false;
	}

	// swimming monsters don't exit water voluntarily, and
	// flying monsters don't enter water voluntarily (but will
	// try to leave it)
	bool bad_movement_direction = false;

	//if (!(ent->monsterInfo.aiFlags & AI_COMBAT_POINT))
	{
		if (ent->flags & FL_SWIM)
			bad_movement_direction = !(gi.pointContents(ent->s.origin + (wanted_dir * currentSpeed)) & CONTENTS_WATER);
		else if ((ent->flags & FL_FLY) && ent->waterLevel < WATER_UNDER)
			bad_movement_direction = gi.pointContents(ent->s.origin + (wanted_dir * currentSpeed)) & CONTENTS_WATER;
	}

	if (bad_movement_direction) {
		if (ent->monsterInfo.fly_recovery_time < level.time) {
			ent->monsterInfo.fly_recovery_dir = Vector3{ crandom(), crandom(), crandom() }.normalized();
			ent->monsterInfo.fly_recovery_time = level.time + 1_sec;
		}

		wanted_dir = ent->monsterInfo.fly_recovery_dir;
	}

	if (dir && turn_factor > 0)
		final_dir = slerp(dir, wanted_dir, 1.0f - turn_factor).normalized();

	// the closer we are to the wanted position, we want to slow
	// down so we don't fly past it.
	float speed_factor;

	if (!ent->enemy || (ent->monsterInfo.fly_thrusters && !ent->monsterInfo.fly_pinned) || (ent->monsterInfo.aiFlags & (AI_PATHING | AI_COMBAT_POINT | AI_LOST_SIGHT)))
		speed_factor = 1.f;
	else if (aim_fwd.dot(wanted_dir) < -0.25 && dir)
		speed_factor = 0.f;
	else
		speed_factor = min(1.f, dist_to_wanted / ent->monsterInfo.fly_speed);

	if (bad_movement_direction)
		speed_factor = -speed_factor;

	float accel = ent->monsterInfo.fly_acceleration;

	// if we're flying away from our destination, apply reverse thrusters
	if (final_dir.dot(wanted_dir) < 0.25f)
		accel *= 2.0f;

	float wanted_speed = ent->monsterInfo.fly_speed * speed_factor;

	if (ent->monsterInfo.aiFlags & AI_MANUAL_STEERING)
		wanted_speed = 0;

	// change speed
	if (currentSpeed > wanted_speed)
		currentSpeed = max(wanted_speed, currentSpeed - accel);
	else if (currentSpeed < wanted_speed)
		currentSpeed = min(wanted_speed, currentSpeed + accel);

	// FIXME
	if (isnan(final_dir[0]) || isnan(final_dir[1]) || isnan(final_dir[2]) ||
		isnan(currentSpeed)) {
#if defined(_DEBUG) && defined(_WIN32)
		__debugbreak();
#endif
		return false;
	}

	// commit
	ent->velocity = final_dir * currentSpeed;

	// for buzzards, set their pitch
	if (ent->enemy && (ent->monsterInfo.fly_buzzard || (ent->monsterInfo.aiFlags & AI_MEDIC))) {
		Vector3 d = (ent->s.origin - towards_origin).normalized();
		d = VectorToAngles(d);
		ent->s.angles[PITCH] = LerpAngle(ent->s.angles[PITCH], -d[PITCH], gi.frameTimeSec * 4.0f);
	} else
		ent->s.angles[PITCH] = 0;

	return true;
}

// flying monsters don't step up
static bool G_flystep(gentity_t *ent, Vector3 move, bool relink, gentity_t *current_bad) {
	if (ent->monsterInfo.aiFlags & AI_ALTERNATE_FLY) {
		if (G_alternate_flystep(ent, move, relink, current_bad))
			return true;
	}

	// try the move
	Vector3 oldorg = ent->s.origin;
	Vector3 neworg = ent->s.origin + move;

	// fixme: move to monsterInfo
	// we want the carrier to stay a certain distance off the ground, to help prevent him
	// from shooting his fliers, who spawn in below him
	float minheight;

	if (!strcmp(ent->className, "monster_carrier"))
		minheight = 104;
	else
		minheight = 40;

	// try one move with vertical motion, then one without
	for (int i = 0; i < 2; i++) {
		Vector3 new_move = move;

		if (i == 0 && ent->enemy) {
			if (!ent->goalEntity)
				ent->goalEntity = ent->enemy;

			Vector3 &goal_position = (ent->monsterInfo.aiFlags & AI_PATHING) ? ent->monsterInfo.nav_path.firstMovePoint : ent->goalEntity->s.origin;

			float dz = ent->s.origin[_Z] - goal_position[2];
			float dist = move.length();

			if (ent->goalEntity->client) {
				if (dz > minheight) {
					//	pmm
					new_move *= 0.5f;
					new_move[2] -= dist;
				}
				if (!((ent->flags & FL_SWIM) && (ent->waterLevel < WATER_WAIST)))
					if (dz < (minheight - 10)) {
						new_move *= 0.5f;
						new_move[2] += dist;
					}
			} else {
				if (strcmp(ent->className, "monster_fixbot") == 0) {
					if (ent->s.frame >= 105 && ent->s.frame <= 120) {
						if (dz > 12)
							new_move[2]--;
						else if (dz < -12)
							new_move[2]++;
					} else if (ent->s.frame >= 31 && ent->s.frame <= 88) {
						if (dz > 12)
							new_move[2] -= 12;
						else if (dz < -12)
							new_move[2] += 12;
					} else {
						if (dz > 12)
							new_move[2] -= 8;
						else if (dz < -12)
							new_move[2] += 8;
					}
				} else {
					if (dz > 0) {
						new_move *= 0.5f;
						new_move[2] -= min(dist, dz);
					} else if (dz < 0) {
						new_move *= 0.5f;
						new_move[2] += -max(-dist, dz);
					}
				}
			}
		}

		neworg = ent->s.origin + new_move;

		trace_t trace = gi.trace(ent->s.origin, ent->mins, ent->maxs, neworg, ent, MASK_MONSTERSOLID);

		// fly monsters don't enter water voluntarily
		if (ent->flags & FL_FLY) {
			if (!ent->waterLevel) {
				Vector3 test{ trace.endPos[0], trace.endPos[1], trace.endPos[2] + ent->mins[2] + 1 };
				contents_t contents = gi.pointContents(test);
				if (contents & MASK_WATER)
					return false;
			}
		}

		// swim monsters don't exit water voluntarily
		if (ent->flags & FL_SWIM) {
			if (ent->waterLevel < WATER_WAIST) {
				Vector3 test{ trace.endPos[0], trace.endPos[1], trace.endPos[2] + ent->mins[2] + 1 };
				contents_t contents = gi.pointContents(test);
				if (!(contents & MASK_WATER))
					return false;
			}
		}

		if ((trace.fraction == 1) && (!trace.allSolid) && (!trace.startSolid)) {
			ent->s.origin = trace.endPos;
			if (!current_bad && CheckForBadArea(ent))
				ent->s.origin = oldorg;
			else {
				if (relink) {
					gi.linkEntity(ent);
					TouchTriggers(ent);
				}

				return true;
			}
		}

		G_Impact(ent, trace);

		if (!ent->enemy)
			break;
	}

	return false;
}

/*
=============
G_movestep

Called by monster program code.
The move will be adjusted for slopes and stairs, but if the move isn't
possible, no move is done, false is returned, and
pr_global_struct->trace_normal is set to the normal of the blocking wall
=============
*/
// FIXME since we need to test end position contents here, can we avoid doing
// it again later in catagorize position?
static bool G_movestep(gentity_t *ent, Vector3 move, bool relink) {
	gentity_t *current_bad = nullptr;

	// PMM - who cares about bad areas if you're dead?
	if (ent->health > 0) {
		current_bad = CheckForBadArea(ent);
		if (current_bad) {
			ent->bad_area = current_bad;

			if (ent->enemy && !strcmp(ent->enemy->className, "tesla_mine")) {
				// if the tesla is in front of us, back up...
				if (IsBadAhead(ent, current_bad, move))
					move *= -1;
			}
		} else if (ent->bad_area) {
			// if we're no longer in a bad area, get back to business.
			ent->bad_area = nullptr;
			if (ent->oldEnemy) // && ent->bad_area->owner == ent->enemy)
			{
				ent->enemy = ent->oldEnemy;
				ent->goalEntity = ent->oldEnemy;
				FoundTarget(ent);
			}
		}
	}

	// flying monsters don't step up
	if (ent->flags & (FL_SWIM | FL_FLY))
		return G_flystep(ent, move, relink, current_bad);

	// try the move
	Vector3 oldorg = ent->s.origin;

	float stepsize;

	// push down from a step height above the wished position
	if (ent->spawnFlags.has(SPAWNFLAG_MONSTER_SUPER_STEP) && ent->health > 0)
		stepsize = 64.f;
	else if (!(ent->monsterInfo.aiFlags & AI_NOSTEP))
		stepsize = STEPSIZE;
	else
		stepsize = 1;

	stepsize += 0.75f;

	contents_t mask = (ent->svFlags & SVF_MONSTER) ? MASK_MONSTERSOLID : (MASK_SOLID | CONTENTS_MONSTER | CONTENTS_PLAYER);

	Vector3 start_up = oldorg + ent->gravityVector * (-1 * stepsize);

	start_up = gi.trace(oldorg, ent->mins, ent->maxs, start_up, ent, mask).endPos;

	Vector3 end_up = start_up + move;

	trace_t up_trace = gi.trace(start_up, ent->mins, ent->maxs, end_up, ent, mask);

	if (up_trace.startSolid) {
		start_up += ent->gravityVector * (-1 * stepsize);
		up_trace = gi.trace(start_up, ent->mins, ent->maxs, end_up, ent, mask);
	}

	Vector3 start_fwd = oldorg;
	Vector3 end_fwd = start_fwd + move;

	trace_t fwd_trace = gi.trace(start_fwd, ent->mins, ent->maxs, end_fwd, ent, mask);

	if (fwd_trace.startSolid) {
		start_up += ent->gravityVector * (-1 * stepsize);
		fwd_trace = gi.trace(start_fwd, ent->mins, ent->maxs, end_fwd, ent, mask);
	}

	// pick the one that went farther
	trace_t &chosen_forward = (up_trace.fraction > fwd_trace.fraction) ? up_trace : fwd_trace;

	if (chosen_forward.startSolid || chosen_forward.allSolid)
		return false;

	int32_t steps = 1;
	bool stepped = false;

	if (up_trace.fraction > fwd_trace.fraction)
		steps = 2;

	// step us down
	Vector3 end = chosen_forward.endPos + (ent->gravityVector * (steps * stepsize));
	trace_t trace = gi.trace(chosen_forward.endPos, ent->mins, ent->maxs, end, ent, mask);

	if (std::fabs(ent->s.origin.z - trace.endPos.z) > 8.f)
		stepped = true;

	// Paril: improved the water handling here.
	// monsters are okay with stepping into water
	// up to their waist.
	if (ent->waterLevel <= WATER_WAIST) {
		water_level_t end_waterlevel;
		contents_t	  end_watertype;
		M_CatagorizePosition(ent, trace.endPos, end_waterlevel, end_watertype);

		// don't go into deep liquids or
		// slime/lava voluntarily
		if (end_watertype & (CONTENTS_SLIME | CONTENTS_LAVA) ||
			end_waterlevel > WATER_WAIST)
			return false;
	}

	if (trace.fraction == 1) {
		// if monster had the ground pulled out, go ahead and fall
		if (ent->flags & FL_PARTIALGROUND) {
			ent->s.origin += move;
			if (relink) {
				gi.linkEntity(ent);
				TouchTriggers(ent);
			}
			ent->groundEntity = nullptr;
			return true;
		}
		// [Paril-KEX] allow dead monsters to "fall" off of edges in their death animation
		else if (!ent->spawnFlags.has(SPAWNFLAG_MONSTER_SUPER_STEP) && ent->health > 0)
			return false; // walked off an edge
	}

	// [Paril-KEX] if we didn't move at all (or barely moved), don't count it
	if ((trace.endPos - oldorg).length() < move.length() * 0.05f) {
		ent->monsterInfo.bad_move_time = level.time + 1000_ms;

		if (ent->monsterInfo.bump_time < level.time && chosen_forward.fraction < 1.0f) {
			// adjust ideal_yaw to move against the object we hit and try again
			Vector3 dir = SlideClipVelocity(AngleVectors(Vector3{ 0.f, ent->ideal_yaw, 0.f }).forward, chosen_forward.plane.normal, 1.0f);
			float new_yaw = vectoyaw(dir);

			if (dir.lengthSquared() > 0.1f && ent->ideal_yaw != new_yaw) {
				ent->ideal_yaw = new_yaw;
				ent->monsterInfo.random_change_time = level.time + 100_ms;
				ent->monsterInfo.bump_time = level.time + 200_ms;
				return true;
			}
		}

		return false;
	}

	// check point traces down for dangling corners
	ent->s.origin = trace.endPos;

	if (ent->health > 0) {
		// use AI_BLOCKED to tell the calling layer that we're now mad at a tesla
		new_bad = CheckForBadArea(ent);
		if (!current_bad && new_bad) {
			if (new_bad->owner) {
				if (!strcmp(new_bad->owner->className, "tesla_mine")) {
					if ((!(ent->enemy)) || (!(ent->enemy->inUse))) {
						TargetTesla(ent, new_bad->owner);
						ent->monsterInfo.aiFlags |= AI_BLOCKED;
					} else if (!strcmp(ent->enemy->className, "tesla_mine")) {
					} else if ((ent->enemy) && (ent->enemy->client)) {
						if (!visible(ent, ent->enemy)) {
							TargetTesla(ent, new_bad->owner);
							ent->monsterInfo.aiFlags |= AI_BLOCKED;
						}
					} else {
						TargetTesla(ent, new_bad->owner);
						ent->monsterInfo.aiFlags |= AI_BLOCKED;
					}
				}
			}

			ent->s.origin = oldorg;
			return false;
		}
	}

	if (!M_CheckBottom(ent)) {
		if (ent->flags & FL_PARTIALGROUND) { // entity had floor mostly pulled out from underneath it
			// and is trying to correct
			if (relink) {
				gi.linkEntity(ent);
				TouchTriggers(ent);
			}
			return true;
		}

		// walked off an edge that wasn't a stairway
		ent->s.origin = oldorg;
		return false;
	}

	if (ent->spawnFlags.has(SPAWNFLAG_MONSTER_SUPER_STEP) && ent->health > 0) {
		if (!ent->groundEntity || ent->groundEntity->solid == SOLID_BSP) {
			if (!(trace.ent->solid == SOLID_BSP)) {
				// walked off an edge
				ent->s.origin = oldorg;
				M_CheckGround(ent, G_GetClipMask(ent));
				return false;
			}
		}
	}

	// [Paril-KEX]
	M_CheckGround(ent, G_GetClipMask(ent));

	if (!ent->groundEntity) {
		// walked off an edge
		ent->s.origin = oldorg;
		M_CheckGround(ent, G_GetClipMask(ent));
		return false;
	}

	if (ent->flags & FL_PARTIALGROUND) {
		ent->flags &= ~FL_PARTIALGROUND;
	}
	ent->groundEntity = trace.ent;
	ent->groundEntity_linkCount = trace.ent->linkCount;

	// the move is ok
	if (relink) {
		gi.linkEntity(ent);

		// [Paril-KEX] this is something N64 does to avoid doors opening
		// at the start of a level, which triggers some monsters to spawn.
		if (!level.isN64 || level.time > FRAME_TIME_S)
			TouchTriggers(ent);
	}

	if (stepped)
		ent->s.renderFX |= RF_STAIR_STEP;

	if (trace.fraction < 1.f)
		G_Impact(ent, trace);

	return true;
}

// check if a movement would succeed
bool ai_check_move(gentity_t *self, float dist) {
	if (ai_movement_disabled->integer) {
		return false;
	}

	float yaw = self->s.angles[YAW] * PIf * 2 / 360;
	Vector3 move = {
		cosf(yaw) * dist,
		std::sinf(yaw) * dist,
		0
	};

	Vector3 oldOrigin = self->s.origin;

	if (!G_movestep(self, move, false))
		return false;

	self->s.origin = oldOrigin;
	gi.linkEntity(self);
	return true;
}

//============================================================================

/*
===============
M_ChangeYaw

===============
*/
void M_ChangeYaw(gentity_t *ent) {
	float ideal;
	float current;
	float move;
	float speed;

	current = anglemod(ent->s.angles[YAW]);
	ideal = ent->ideal_yaw;

	if (current == ideal)
		return;

	move = ideal - current;
	// [Paril-KEX] high tick rate
	speed = ent->yawSpeed / (static_cast<float>(gi.tickRate) / 10.0f);

	if (ideal > current) {
		if (move >= 180)
			move = move - 360;
	} else {
		if (move <= -180)
			move = move + 360;
	}
	if (move > 0) {
		if (move > speed)
			move = speed;
	} else {
		if (move < -speed)
			move = -speed;
	}

	ent->s.angles[YAW] = anglemod(current + move);
}

/*
======================
G_StepDirection

Turns to the movement direction, and walks the current distance if
facing it.

======================
*/
static bool G_StepDirection(gentity_t *ent, float yaw, float dist, bool allow_no_turns) {
	Vector3 move{}, oldorigin;

	if (!ent->inUse)
		return true; // PGM g_touchtrigger free problem

	float old_ideal_yaw = ent->ideal_yaw;
	float old_current_yaw = ent->s.angles[YAW];

	ent->ideal_yaw = yaw;
	M_ChangeYaw(ent);

	yaw = yaw * PIf * 2 / 360;
	move[0] = cosf(yaw) * dist;
	move[1] = std::sinf(yaw) * dist;
	move[2] = 0;

	oldorigin = ent->s.origin;
	if (G_movestep(ent, move, false)) {
		ent->monsterInfo.aiFlags &= ~AI_BLOCKED;
		if (!ent->inUse)
			return true; // PGM g_touchtrigger free problem

		if (strncmp(ent->className, "monster_widow", 13)) {
			if (!FacingIdeal(ent)) {
				// not turned far enough, so don't take the step
				// but still turn
				ent->s.origin = oldorigin;
				M_CheckGround(ent, G_GetClipMask(ent));
				return allow_no_turns; // [Paril-KEX]
			}
		}
		gi.linkEntity(ent);
		TouchTriggers(ent);
		G_TouchProjectiles(ent, oldorigin);
		return true;
	}
	gi.linkEntity(ent);
	TouchTriggers(ent);
	ent->ideal_yaw = old_ideal_yaw;
	ent->s.angles[YAW] = old_current_yaw;
	return false;
}

/*
======================
G_FixCheckBottom

======================
*/
static void G_FixCheckBottom(gentity_t *ent) {
	ent->flags |= FL_PARTIALGROUND;
}

/*
================
G_NewChaseDir

================
*/
constexpr float DI_NODIR = -1;

static bool G_NewChaseDir(gentity_t *actor, Vector3 pos, float dist) {
	float deltax, deltay;
	float d[3]{};
	float tdir, olddir, turnaround;

	olddir = anglemod(truncf(actor->ideal_yaw / 45) * 45);
	turnaround = anglemod(olddir - 180);

	deltax = pos[0] - actor->s.origin[_X];
	deltay = pos[1] - actor->s.origin[_Y];
	if (deltax > 10)
		d[1] = 0;
	else if (deltax < -10)
		d[1] = 180;
	else
		d[1] = DI_NODIR;
	if (deltay < -10)
		d[2] = 270;
	else if (deltay > 10)
		d[2] = 90;
	else
		d[2] = DI_NODIR;

	// try direct route
	if (d[1] != DI_NODIR && d[2] != DI_NODIR) {
		if (d[1] == 0)
			tdir = d[2] == 90 ? 45.f : 315.f;
		else
			tdir = d[2] == 90 ? 135.f : 215.f;

		if (tdir != turnaround && G_StepDirection(actor, tdir, dist, false))
			return true;
	}

	// try other directions
	if (brandom() || std::fabs(deltay) > std::fabs(deltax)) {
		tdir = d[1];
		d[1] = d[2];
		d[2] = tdir;
	}

	if (d[1] != DI_NODIR && d[1] != turnaround && G_StepDirection(actor, d[1], dist, false))
		return true;

	if (d[2] != DI_NODIR && d[2] != turnaround && G_StepDirection(actor, d[2], dist, false))
		return true;

	if (actor->monsterInfo.blocked) {
		if ((actor->inUse) && (actor->health > 0) && !(actor->monsterInfo.aiFlags & AI_TARGET_ANGER)) {
			// if block "succeeds", the actor will not move or turn.
			if (actor->monsterInfo.blocked(actor, dist)) {
				actor->monsterInfo.move_block_counter = -2;
				return true;
			}

			// we couldn't step; instead of running endlessly in our current
			// spot, try switching to node navigation temporarily to get to
			// where we need to go.
			if (!(actor->monsterInfo.aiFlags & (AI_LOST_SIGHT | AI_COMBAT_POINT | AI_TARGET_ANGER | AI_PATHING | AI_TEMP_MELEE_COMBAT | AI_NO_PATH_FINDING))) {
				if (++actor->monsterInfo.move_block_counter > 2) {
					actor->monsterInfo.aiFlags |= AI_TEMP_MELEE_COMBAT;
					actor->monsterInfo.move_block_change_time = level.time + 3_sec;
					actor->monsterInfo.move_block_counter = 0;
				}
			}
		}
	}

	/* there is no direct path to the player, so pick another direction */

	if (olddir != DI_NODIR && G_StepDirection(actor, olddir, dist, false))
		return true;

	if (brandom()) /*randomly determine direction of search*/
	{
		for (tdir = 0; tdir <= 315; tdir += 45)
			if (tdir != turnaround && G_StepDirection(actor, tdir, dist, false))
				return true;
	} else {
		for (tdir = 315; tdir >= 0; tdir -= 45)
			if (tdir != turnaround && G_StepDirection(actor, tdir, dist, false))
				return true;
	}

	if (turnaround != DI_NODIR && G_StepDirection(actor, turnaround, dist, false))
		return true;

	actor->ideal_yaw = frandom(0, 360); // can't move; pick a random yaw...

	// if a bridge was pulled out from underneath a monster, it may not have
	// a valid standing position at all

	if (!M_CheckBottom(actor))
		G_FixCheckBottom(actor);

	return false;
}

/*
======================
G_CloseEnough

======================
*/
bool G_CloseEnough(gentity_t *ent, gentity_t *goal, float dist) {
	int i;

	for (i = 0; i < 3; i++) {
		if (goal->absMin[i] > ent->absMax[i] + dist)
			return false;
		if (goal->absMax[i] < ent->absMin[i] - dist)
			return false;
	}
	return true;
}

static bool M_NavPathToGoal(gentity_t *self, float dist, const Vector3 &goal) {
	// mark us as *trying* now (nav_pos is valid)
	self->monsterInfo.aiFlags |= AI_PATHING;

	Vector3 &path_to = (self->monsterInfo.nav_path.returnCode == PathReturnCode::TraversalPending) ?
		self->monsterInfo.nav_path.secondMovePoint : self->monsterInfo.nav_path.firstMovePoint;

	if ((self->monsterInfo.nav_path.returnCode != PathReturnCode::TraversalPending && (path_to - self->s.origin).length() <= (self->size.length() * 0.5f)) ||
		self->monsterInfo.nav_path_cache_time <= level.time) {
		PathRequest request;
		if (self->enemy)
			request.goal = self->enemy->s.origin;
		else
			request.goal = self->goalEntity->s.origin;
		request.moveDist = dist;
		if (g_debug_monster_paths->integer == 1)
			request.debugging.drawTime = gi.frameTimeSec;
		request.start = self->s.origin;
		request.pathFlags = PathFlags::Walk;

		if (self->monsterInfo.canJump || (self->flags & FL_FLY)) {
			if (self->monsterInfo.jumpHeight) {
				request.pathFlags |= PathFlags::BarrierJump;
				request.traversals.jumpHeight = self->monsterInfo.jumpHeight;
			}
			if (self->monsterInfo.dropHeight) {
				request.pathFlags |= PathFlags::WalkOffLedge;
				request.traversals.dropHeight = self->monsterInfo.dropHeight;
			}
		}

		if (self->flags & FL_FLY) {
			request.nodeSearch.maxHeight = request.nodeSearch.minHeight = 8192.f;
			request.pathFlags |= PathFlags::LongJump;
		}

		if (!gi.GetPathToGoal(request, self->monsterInfo.nav_path)) {
			// fatal error, don't bother ever trying nodes
			if (self->monsterInfo.nav_path.returnCode == PathReturnCode::NoNavAvailable)
				self->monsterInfo.aiFlags |= AI_NO_PATH_FINDING;
			return false;
		}

		self->monsterInfo.nav_path_cache_time = level.time + 2_sec;
	}

	float yaw;
	float old_yaw = self->s.angles[YAW];
	float old_ideal_yaw = self->ideal_yaw;

	if (self->monsterInfo.random_change_time >= level.time &&
		!(self->monsterInfo.aiFlags & AI_ALTERNATE_FLY))
		yaw = self->ideal_yaw;
	else
		yaw = vectoyaw((path_to - self->s.origin).normalized());

	if (!G_StepDirection(self, yaw, dist, true)) {

		if (!self->inUse)
			return false;

		if (self->monsterInfo.blocked && !(self->monsterInfo.aiFlags & AI_TARGET_ANGER)) {
			if ((self->inUse) && (self->health > 0)) {
				// if we're blocked, the blocked function will be deferred to for yaw
				self->s.angles[YAW] = old_yaw;
				self->ideal_yaw = old_ideal_yaw;
				if (self->monsterInfo.blocked(self, dist))
					return true;
			}
		}

		// try the first point
		if (self->monsterInfo.random_change_time >= level.time)
			yaw = self->ideal_yaw;
		else
			yaw = vectoyaw((self->monsterInfo.nav_path.firstMovePoint - self->s.origin).normalized());

		if (!G_StepDirection(self, yaw, dist, true)) {

			// we got blocked, but all is not lost yet; do a similar bump around-ish behavior
			// to try to regain our composure
			if (self->monsterInfo.aiFlags & AI_BLOCKED) {
				self->monsterInfo.aiFlags &= ~AI_BLOCKED;
				return true;
			}

			if (self->monsterInfo.random_change_time < level.time && self->inUse) {
				self->monsterInfo.random_change_time = level.time + 1500_ms;
				if (G_NewChaseDir(self, path_to, dist))
					return true;
			}

			self->monsterInfo.path_blocked_counter += FRAME_TIME_S * 3;
		}

		if (self->monsterInfo.path_blocked_counter > 1.5_sec)
			return false;
	}

	return true;
}

/*
=============
M_MoveToPath

Advanced movement code that use the bots pathfinder if allowed and conditions are right.
Feel free to add any other conditions needed.
=============
*/
static bool M_MoveToPath(gentity_t *self, float dist) {
	if (self->flags & FL_STATIONARY)
		return false;
	else if (self->monsterInfo.aiFlags & AI_NO_PATH_FINDING)
		return false;
	else if (self->monsterInfo.path_wait_time > level.time)
		return false;
	else if (!self->enemy)
		return false;
  else if (self->enemy->client && self->enemy->client->PowerupTimer(PowerupTimer::Invisibility) > level.time && self->enemy->client->invisibility_fade_time <= level.time)
		return false;
	else if (self->monsterInfo.attackState >= MonsterAttackState::Missile)
		return true;

	CombatStyle style = self->monsterInfo.combatStyle;

	if (self->monsterInfo.aiFlags & AI_TEMP_MELEE_COMBAT)
		style = CombatStyle::Melee;

	if (visible(self, self->enemy, false)) {
		if ((self->flags & (FL_SWIM | FL_FLY)) || style == CombatStyle::Ranged) {
			// do the normal "shoot, walk, shoot" behavior...
			return false;
		} else if (style == CombatStyle::Melee) {
			// path pretty close to the enemy, then let normal Quake movement take over.
			if (range_to(self, self->enemy) > 240.f ||
				fabs(self->s.origin.z - self->enemy->s.origin.z) > max(self->maxs.z, -self->mins.z)) {
				if (M_NavPathToGoal(self, dist, self->enemy->s.origin)) {
					return true;
				}
				self->monsterInfo.aiFlags &= ~AI_TEMP_MELEE_COMBAT;
			} else {
				self->monsterInfo.aiFlags &= ~AI_TEMP_MELEE_COMBAT;
				return false;
			}
		} else if (style == CombatStyle::Mixed) {
			// most mixed combat AI have fairly short range attacks, so try to path within mid range.
			if (range_to(self, self->enemy) > RANGE_NEAR ||
				fabs(self->s.origin.z - self->enemy->s.origin.z) > max(self->maxs.z, -self->mins.z) * 2.0f) {
				if (M_NavPathToGoal(self, dist, self->enemy->s.origin)) {
					return true;
				}
			} else {
				return false;
			}
		}
	} else {
		// we can't see our enemy, let's see if we can path to them
		if (M_NavPathToGoal(self, dist, self->enemy->s.origin)) {
			return true;
		}
	}

	if (!self->inUse)
		return false;

	if (self->monsterInfo.nav_path.returnCode > PathReturnCode::StartPathErrors) {
		self->monsterInfo.path_wait_time = level.time + 10_sec;
		return false;
	}

	self->monsterInfo.path_blocked_counter += FRAME_TIME_S * 3;

	if (self->monsterInfo.path_blocked_counter > 5_sec) {
		self->monsterInfo.path_blocked_counter = 0_ms;
		self->monsterInfo.path_wait_time = level.time + 5_sec;

		return false;
	}

	return true;
}

/*
======================
M_MoveToGoal
======================
*/
void M_MoveToGoal(gentity_t *ent, float dist) {
	if (ai_movement_disabled->integer) {
		if (!FacingIdeal(ent)) {
			M_ChangeYaw(ent);
		} // mal: don't move, but still face toward target
		return;
	}

	gentity_t *goal;

	goal = ent->goalEntity;

	if (!ent->groundEntity && !(ent->flags & (FL_FLY | FL_SWIM)))
		return;
	// ???
	else if (!goal)
		return;

	// [Paril-KEX] try paths if we can't see the enemy
	if (!(ent->monsterInfo.aiFlags & AI_COMBAT_POINT) && ent->monsterInfo.attackState < MonsterAttackState::Missile) {
		if (M_MoveToPath(ent, dist)) {
			ent->monsterInfo.path_blocked_counter = max(0_ms, ent->monsterInfo.path_blocked_counter - FRAME_TIME_S);
			return;
		}
	}

	ent->monsterInfo.aiFlags &= ~AI_PATHING;

	//if (goal)
	//	gi.Draw_Point(goal->s.origin, 1.f, rgba_red, gi.frameTimeMs, false);

	// [Paril-KEX] dumb hack; in some n64 maps, the corners are way too high and
	// I'm too lazy to fix them individually in maps, so here's a game fix..
	if (!(goal->flags & FL_PARTIALGROUND) && !(ent->flags & (FL_FLY | FL_SWIM)) &&
		goal->className && (!strcmp(goal->className, "path_corner") || !strcmp(goal->className, "point_combat"))) {
		Vector3 p = goal->s.origin;
		p.z = ent->s.origin.z;

		if (boxes_intersect(ent->absMin, ent->absMax, p, p)) {
			// mark this so we don't do it again later
			goal->flags |= FL_PARTIALGROUND;

			if (!boxes_intersect(ent->absMin, ent->absMax, goal->s.origin, goal->s.origin)) {
				// move it if we would have touched it if the corner was lower
				goal->s.origin.z = p.z;
				gi.linkEntity(goal);
			}
		}
	}

	// [Paril-KEX] if we have a straight shot to our target, just move
	// straight instead of trying to stick to invisible guide lines
	if ((ent->monsterInfo.bad_move_time <= level.time || (ent->monsterInfo.aiFlags & AI_CHARGING)) && goal) {
		if (!FacingIdeal(ent)) {
			M_ChangeYaw(ent);
			return;
		}

		trace_t tr = gi.traceLine(ent->s.origin, goal->s.origin, ent, MASK_MONSTERSOLID);

		if (tr.fraction == 1.0f || tr.ent == goal) {
			if (G_StepDirection(ent, vectoyaw((goal->s.origin - ent->s.origin).normalized()), dist, false))
				return;
		}

		// we didn't make a step, so don't try this for a while
		// *unless* we're going to a path corner
		if (goal->className && strcmp(goal->className, "path_corner") && strcmp(goal->className, "point_combat")) {
			ent->monsterInfo.bad_move_time = level.time + 5_sec;
			ent->monsterInfo.aiFlags &= ~AI_CHARGING;
		}
	}

	// bump around...
	if ((ent->monsterInfo.random_change_time <= level.time // random change time is up
		&& irandom(4) == 1 // random bump around
		&& !(ent->monsterInfo.aiFlags & AI_CHARGING) // PMM - charging monsters (AI_CHARGING) don't deflect unless they have to
		&& !((ent->monsterInfo.aiFlags & AI_ALTERNATE_FLY) && ent->enemy && !(ent->monsterInfo.aiFlags & AI_LOST_SIGHT))) // alternate fly monsters don't do this either unless they have to
		|| !G_StepDirection(ent, ent->ideal_yaw, dist, ent->monsterInfo.bad_move_time > level.time)) {
		if (ent->monsterInfo.aiFlags & AI_BLOCKED) {
			ent->monsterInfo.aiFlags &= ~AI_BLOCKED;
			return;
		}
		ent->monsterInfo.random_change_time = level.time + random_time(500_ms, 1000_ms);
		G_NewChaseDir(ent, goal->s.origin, dist);
		ent->monsterInfo.move_block_counter = 0;
	} else
		ent->monsterInfo.bad_move_time -= 250_ms;
}

/*
===============
M_walkmove
===============
*/
bool M_walkmove(gentity_t *ent, float yaw, float dist) {
	if (ai_movement_disabled->integer) {
		return false;
	}

	Vector3 move{};
	// PMM
	bool retval;

	if (!ent->groundEntity && !(ent->flags & (FL_FLY | FL_SWIM)))
		return false;

	yaw = yaw * PIf * 2 / 360;

	move[0] = cosf(yaw) * dist;
	move[1] = std::sinf(yaw) * dist;
	move[2] = 0;

	// PMM
	retval = G_movestep(ent, move, true);
	ent->monsterInfo.aiFlags &= ~AI_BLOCKED;
	return retval;
}
