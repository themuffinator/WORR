/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

g_phys.cpp (Game Physics) This file is responsible for the server-side physics simulation of all
non-player entities. It is called every frame for each active entity to update its position and
state based on its `moveType`. Key Responsibilities: - Physics Dispatcher: `G_RunEntity` is the
main function that selects the correct physics function (e.g., `G_Physics_Pusher`,
`G_Physics_Toss`) based on the entity's `moveType`. - Mover Physics: Implements
`G_Physics_Pusher` for solid, moving brush models like doors and platforms, including the
complex logic for pushing other entities. - Projectile and Gib Physics: Implements
`G_Physics_Toss` for entities that are affected by gravity and can bounce off surfaces. - Static
and NoClip Physics: Handles entities that do not move or that move without any collision. - Core
Utilities: Contains fundamental physics helpers like `G_AddGravity` and `G_Impact` for handling
collisions.*/

#include "../g_local.hpp"

/*


pushmove objects do not obey gravity, and do not interact with each other or trigger fields, but block normal movement
and push normal objects when they move.

onground is set for toss objects when they come to a complete rest.  it is set for steping or walking objects

doors, plats, etc are SOLID_BSP, and MoveType::Push
bonus items are SOLID_TRIGGER touch, and MoveType::Toss
corpses are SOLID_NOT and MoveType::Toss
crates are SOLID_BBOX and MoveType::Toss
walking monsters are SOLID_SLIDEBOX and MoveType::Step
flying/floating monsters are SOLID_SLIDEBOX and MoveType::Fly

solid_edge items only clip against bsp models.

*/

void G_Physics_NewToss(gentity_t* ent); // PGM

// [Paril-KEX] fetch the clipMask for this entity; certain modifiers
// affect the clipping behavior of objects.
contents_t G_GetClipMask(gentity_t* ent) {
	contents_t mask = ent->clipMask;

	// default masks
	if (!mask) {
		if (ent->svFlags & SVF_MONSTER)
			mask = MASK_MONSTERSOLID;
		else if (ent->svFlags & SVF_PROJECTILE)
			mask = MASK_PROJECTILE;
		else
			mask = MASK_SHOT & ~CONTENTS_DEADMONSTER;
	}

	// non-solid objects (items, etc) shouldn't try to clip
	// against players/monsters
	if (ent->solid == SOLID_NOT || ent->solid == SOLID_TRIGGER)
		mask &= ~(CONTENTS_MONSTER | CONTENTS_PLAYER);

	// monsters/players that are also dead shouldn't clip
	// against players/monsters
	if ((ent->svFlags & (SVF_MONSTER | SVF_PLAYER)) && (ent->svFlags & SVF_DEADMONSTER))
		mask &= ~(CONTENTS_MONSTER | CONTENTS_PLAYER);

	return mask;
}

/*
============
G_TestEntityPosition

============
*/
static gentity_t* G_TestEntityPosition(gentity_t* ent) {
	trace_t	   trace;

	trace = gi.trace(ent->s.origin, ent->mins, ent->maxs, ent->s.origin, ent, G_GetClipMask(ent));

	if (trace.startSolid)
		return g_entities;

	return nullptr;
}

/*
================
G_CheckVelocity
================
*/
void G_CheckVelocity(gentity_t* ent) {
	//
	// bound velocity
	//
	float speed = ent->velocity.length();

	if (speed > g_maxvelocity->value)
		ent->velocity = (ent->velocity / speed) * g_maxvelocity->value;
}

/*
=============
G_RunThink

Runs thinking code for this frame if necessary
=============
*/
bool G_RunThink(gentity_t* ent) {
	GameTime thinktime = ent->nextThink;
	if (thinktime <= 0_ms)
		return true;
	if (thinktime > level.time)
		return true;

	ent->nextThink = 0_ms;
	if (!ent->think)
		//gi.Com_Error("nullptr ent->think");
		return false;	//true;
	ent->think(ent);

	return false;
}

/*
==================
G_Impact

Two entities have touched, so run their touch functions
==================
*/
void G_Impact(gentity_t* e1, const trace_t& trace) {
	gentity_t* e2 = trace.ent;

	if (e1->touch && (e1->solid != SOLID_NOT || (e1->flags & FL_ALWAYS_TOUCH)))
		e1->touch(e1, e2, trace, false);

	if (e2->touch && (e2->solid != SOLID_NOT || (e2->flags & FL_ALWAYS_TOUCH)))
		e2->touch(e2, e1, trace, true);
}

/*
============
G_FlyMove

The basic solid body movement clip that slides along multiple planes
============
*/
void G_FlyMove(gentity_t* ent, float time, contents_t mask) {
	ent->groundEntity = nullptr;

	touch_list_t touch;
	PM_StepSlideMove_Generic(ent->s.origin, ent->velocity, time, ent->mins, ent->maxs, touch, false, [&](const Vector3& start, const Vector3& mins, const Vector3& maxs, const Vector3& end) {
		return gi.trace(start, mins, maxs, end, ent, mask);
		});

	for (size_t i = 0; i < touch.num; i++) {
		auto& trace = touch.traces[i];

		if (trace.plane.normal[2] > 0.7f) {
			ent->groundEntity = trace.ent;
			ent->groundEntity_linkCount = trace.ent->linkCount;
		}

		//
		// run the impact function
		//
		G_Impact(ent, trace);

		// impact func requested velocity kill
		if (ent->flags & FL_KILL_VELOCITY) {
			ent->flags &= ~FL_KILL_VELOCITY;
			ent->velocity = {};
		}
	}
}

/*
============
G_AddGravity

============
*/
void G_AddGravity(gentity_t* ent) {
	ent->velocity += ent->gravityVector * (ent->gravity * level.gravity * gi.frameTimeSec);
}

/*
===============================================================================

PUSHMOVE

===============================================================================
*/

/*
============
G_PushEntity

Does not change the entities velocity at all
============
*/
static trace_t G_PushEntity(gentity_t* ent, const Vector3& push) {
	Vector3 start = ent->s.origin;
	Vector3 end = start + push;

	trace_t trace = gi.trace(start, ent->mins, ent->maxs, end, ent, G_GetClipMask(ent));

	ent->s.origin = trace.endPos + (trace.plane.normal * .5f);
	gi.linkEntity(ent);

	if (trace.fraction != 1.0f || trace.startSolid) {
		G_Impact(ent, trace);

		// if the pushed entity went away and the pusher is still there
		if (!trace.ent->inUse && ent->inUse) {
			// move the pusher back and try again
			ent->s.origin = start;
			gi.linkEntity(ent);
			return G_PushEntity(ent, push);
		}
	}

	// FIXME - is this needed?
	ent->gravity = 1.0;

	if (ent->inUse)
		TouchTriggers(ent);

	return trace;
}

struct pushed_t {
	gentity_t* ent = nullptr;
	Vector3	 origin = vec3_origin;
	Vector3	 angles = vec3_origin;
	bool	 rotated = false;
	float	 yaw = 0;
};

pushed_t pushed[MAX_ENTITIES], * pushed_p;

gentity_t* obstacle;

/*
============
G_Push

Objects need to be moved back on a failed push,
otherwise riders would continue to slide.
============
*/
static bool G_Push(gentity_t* pusher, Vector3& move, Vector3& amove) {
	gentity_t* check, * block = nullptr;
	Vector3	  mins, maxs;
	pushed_t* p;
	Vector3	  org, org2{}, move2, forward, right, up;

	// find the bounding box
	mins = pusher->absMin + move;
	maxs = pusher->absMax + move;

	// we need this for pushing things later
	org = -amove;
	AngleVectors(org, forward, right, up);

	// save the pusher's original position
	pushed_p->ent = pusher;
	pushed_p->origin = pusher->s.origin;
	pushed_p->angles = pusher->s.angles;
	pushed_p->rotated = false;
	pushed_p++;

	// move the pusher to it's final position
	pusher->s.origin += move;
	pusher->s.angles += amove;
	gi.linkEntity(pusher);

	// see if any solid entities are inside the final position
	check = g_entities + 1;
	for (uint32_t e = 1; e < globals.numEntities; e++, check++) {
		if (!check->inUse)
			continue;
		if (check->moveType == MoveType::Push || check->moveType == MoveType::Stop || check->moveType == MoveType::None ||
			check->moveType == MoveType::NoClip || check->moveType == MoveType::FreeCam)
			continue;

		if (!check->linked)
			continue; // not linked in anywhere

		// if the entity is standing on the pusher, it will definitely be moved
		if (check->groundEntity != pusher) {
			// see if the ent needs to be tested
			if (check->absMin[0] >= maxs[0] || check->absMin[1] >= maxs[1] || check->absMin[2] >= maxs[2] ||
				check->absMax[0] <= mins[0] || check->absMax[1] <= mins[1] || check->absMax[2] <= mins[2])
				continue;

			// see if the ent's bbox is inside the pusher's final position
			if (!G_TestEntityPosition(check))
				continue;
		}

			if (pusher && ((pusher->moveType == MoveType::Push) || (check->groundEntity == pusher))) {
			// move this entity
			pushed_p->ent = check;
			pushed_p->origin = check->s.origin;
			pushed_p->angles = check->s.angles;
			pushed_p->rotated = !!amove[YAW];
			if (pusher && pushed_p->rotated)
				pushed_p->yaw = pusher->client ? (float)pusher->client->ps.pmove.deltaAngles[YAW] : pusher->s.angles[YAW];
			pushed_p++;

			Vector3 old_position = check->s.origin;

			// try moving the contacted entity
			check->s.origin += move;
			if (check->client) {
				// Paril: disabled because in vanilla deltaAngles are never
				// lerped. deltaAngles can probably be lerped as long as event
				// isn't EV_PLAYER_TELEPORT or a new RDF flag is set
				// check->client->ps.pmove.deltaAngles[YAW] += amove[YAW];
			}
			else
				check->s.angles[YAW] += amove[YAW];

			// figure movement due to the pusher's amove
			if (pusher)
				org = check->s.origin - pusher->s.origin;
			org2[0] = org.dot(forward);
			org2[1] = -(org.dot(right));
			org2[2] = org.dot(up);
			move2 = org2 - org;
			check->s.origin += move2;

			// may have pushed them off an edge
			if (check->groundEntity != pusher)
				check->groundEntity = nullptr;

			block = G_TestEntityPosition(check);

			// [Paril-KEX] this is a bit of a hack; allow dead player skulls
			// to be a blocker because otherwise elevators/doors get stuck
			if (block && check->client && !check->takeDamage) {
				check->s.origin = old_position;
				block = nullptr;
			}

			if (!block) { // pushed ok
				gi.linkEntity(check);
				// impact?
				continue;
			}

			// if it is ok to leave in the old position, do it.
			// this is only relevent for riding entities, not pushed
			check->s.origin = old_position;
			block = G_TestEntityPosition(check);
			if (!block) {
				pushed_p--;
				continue;
			}
		}

		// save off the obstacle so we can call the block function
		obstacle = check;

		// move back any entities we already moved
		// go backwards, so if the same entity was pushed
		// twice, it goes back to the original position
		for (p = pushed_p - 1; p >= pushed; p--) {
			if (p->ent) {
				p->ent->s.origin = p->origin;
				p->ent->s.angles = p->angles;
			}
			if (p->rotated) {
				//if (p->ent->client)
				//	p->ent->client->ps.pmove.deltaAngles[YAW] = p->yaw;
				//else
				p->ent->s.angles[YAW] = p->yaw;
			}
			gi.linkEntity(p->ent);
		}
		return false;
	}

	// FIXME: is there a better way to handle this?
	//  see if anything we moved has touched a trigger
	for (p = pushed_p - 1; p >= pushed; p--)
		TouchTriggers(p->ent);

	return true;
}

/*
================
G_Physics_Pusher

Bmodel objects don't interact with each other, but
push all box objects
================
*/
static void G_Physics_Pusher(gentity_t* ent) {
	Vector3	 move, amove;
	gentity_t* part;

	// if not a team captain, so movement will be handled elsewhere
	if (ent->flags & FL_TEAMSLAVE)
		return;

	// make sure all team slaves can move before commiting
	// any moves or calling any think functions
	// if the move is blocked, all moved objects will be backed out
retry:
	pushed_p = pushed;
	for (part = ent; part; part = part->teamChain) {
		if (part->velocity[_X] || part->velocity[_Y] || part->velocity[_Z] || part->aVelocity[0] || part->aVelocity[1] ||
			part->aVelocity[2]) { // object is moving
			move = part->velocity * gi.frameTimeSec;
			amove = part->aVelocity * gi.frameTimeSec;

			if (!G_Push(part, move, amove))
				break; // move was blocked
		}
	}
	if (pushed_p > &pushed[MAX_ENTITIES])
		gi.Com_Error("pushed_p > &pushed[MAX_ENTITIES], memory corrupted");

	if (part) {
		// if the pusher has a "blocked" function, call it
		// otherwise, just stay in place until the obstacle is gone
		if (part->moveInfo.blocked) {
			if (obstacle->inUse && obstacle->moveType != MoveType::FreeCam && obstacle->moveType != MoveType::NoClip)
				part->moveInfo.blocked(part, obstacle);
		}

		if (!obstacle->inUse)
			goto retry;
	}
	else {
		// the move succeeded, so call all think functions
		for (part = ent; part; part = part->teamChain) {
			// prevent entities that are on trains that have gone away from thinking!
			if (part->inUse)
				G_RunThink(part);
		}
	}
}

//==================================================================

/*
=============
G_Physics_None

Non moving objects can only think
=============
*/
static void G_Physics_None(gentity_t* ent) {
	// regular thinking
	G_RunThink(ent);
}

/*
=============
G_Physics_NoClip

A moving object that doesn't obey physics
=============
*/
static void G_Physics_NoClip(gentity_t* ent) {
	// regular thinking
	if (!G_RunThink(ent) || !ent->inUse)
		return;

	ent->s.angles += (ent->aVelocity * gi.frameTimeSec);
	ent->s.origin += (ent->velocity * gi.frameTimeSec);

	gi.linkEntity(ent);
}

/*
==============================================================================

TOSS / BOUNCE

==============================================================================
*/

/*
=============
G_Physics_Toss

Toss, bounce, and fly movement.  When onground, do nothing.
=============
*/
static void G_Physics_Toss(gentity_t* ent) {
	trace_t	 trace;
	Vector3	 move;
	float	 backoff;
	gentity_t* slave;
	bool	 wasinwater;
	bool	 isinwater;
	Vector3	 oldOrigin;

	// regular thinking
	G_RunThink(ent);

	if (!ent->inUse)
		return;

	// if not a team captain, so movement will be handled elsewhere
	if (ent->flags & FL_TEAMSLAVE)
		return;

	if (ent->velocity[_Z] > 0)
		ent->groundEntity = nullptr;

	// check for the groundEntity going away
	if (ent->groundEntity)
		if (!ent->groundEntity->inUse)
			ent->groundEntity = nullptr;

	// if onground, return without moving
	if (ent->groundEntity && ent->gravity > 0.0f) // PGM - gravity hack
	{
		if (ent->svFlags & SVF_MONSTER) {
			M_CatagorizePosition(ent, ent->s.origin, ent->waterLevel, ent->waterType);
			M_WorldEffects(ent);
		}

		return;
	}

	oldOrigin = ent->s.origin;

	G_CheckVelocity(ent);

	// add gravity
	if (ent->moveType != MoveType::Fly && ent->moveType != MoveType::FlyMissile && ent->moveType != MoveType::WallBounce)
		G_AddGravity(ent);

	// move angles
	ent->s.angles += (ent->aVelocity * gi.frameTimeSec);

	// move origin
	int num_tries = 5;
	float time_left = gi.frameTimeSec;

	while (time_left) {
		if (num_tries == 0)
			break;

		num_tries--;
		move = ent->velocity * time_left;
		trace = G_PushEntity(ent, move);

		if (!ent->inUse)
			return;

		if (trace.fraction == 1.f)
			break;
		// [Paril-KEX] don't build up velocity if we're stuck.
		// just assume that the object we hit is our ground.
		else if (trace.allSolid) {
			ent->groundEntity = trace.ent;
			ent->groundEntity_linkCount = trace.ent->linkCount;
			ent->velocity = {};
			ent->aVelocity = {};
			break;
		}

		time_left -= time_left * trace.fraction;

		if (ent->moveType == MoveType::Toss)
			ent->velocity = SlideClipVelocity(ent->velocity, trace.plane.normal, 0.5f);
		else {
			if (ent->moveType == MoveType::WallBounce)
				backoff = 2.0f;
			else
				backoff = 1.6f;

			ent->velocity = ClipVelocity(ent->velocity, trace.plane.normal, backoff);
		}

		if (ent->moveType == MoveType::WallBounce)
			ent->s.angles = VectorToAngles(ent->velocity);

		// stop if on ground
		else {
			if (trace.plane.normal[2] > 0.7f) {
				if ((ent->moveType == MoveType::Toss && ent->velocity.length() < 60.f) ||
					(ent->moveType != MoveType::Toss && ent->velocity.scaled(trace.plane.normal).length() < 60.f)) {
					if (!(ent->flags & FL_NO_STANDING) || trace.ent->solid == SOLID_BSP) {
						ent->groundEntity = trace.ent;
						ent->groundEntity_linkCount = trace.ent->linkCount;
					}
					ent->velocity = {};
					ent->aVelocity = {};
					break;
				}

				// friction for tossing stuff (gibs, etc)
				if (ent->moveType == MoveType::Toss) {
					ent->velocity *= 0.75f;
					ent->aVelocity *= 0.75f;
				}
			}
		}

		// only toss "slides" multiple times
		if (ent->moveType != MoveType::Toss)
			break;
	}

	// check for water transition
	wasinwater = (ent->waterType & MASK_WATER);
	ent->waterType = gi.pointContents(ent->s.origin);
	isinwater = ent->waterType & MASK_WATER;

	if (isinwater)
		ent->waterLevel = WATER_FEET;
	else
		ent->waterLevel = WATER_NONE;

	if (ent->svFlags & SVF_MONSTER) {
		M_CatagorizePosition(ent, ent->s.origin, ent->waterLevel, ent->waterType);
		M_WorldEffects(ent);
	}
	else {
		if (!wasinwater && isinwater)
			gi.positionedSound(oldOrigin, g_entities, CHAN_AUTO, gi.soundIndex("misc/h2ohit1.wav"), 1, 1, 0);
		else if (wasinwater && !isinwater)
			gi.positionedSound(ent->s.origin, g_entities, CHAN_AUTO, gi.soundIndex("misc/h2ohit1.wav"), 1, 1, 0);
	}

	// prevent softlocks from keys falling into slime/lava
	if (isinwater && ent->waterType & (CONTENTS_SLIME | CONTENTS_LAVA) && ent->item &&
		(ent->item->flags & IF_KEY) && ent->spawnFlags.has(SPAWNFLAG_ITEM_DROPPED))
		ent->velocity = { crandom_open() * 300, crandom_open() * 300, 300.f + (crandom_open() * 300.f) };

	// move teamslaves
	for (slave = ent->teamChain; slave; slave = slave->teamChain) {
		slave->s.origin = ent->s.origin;
		gi.linkEntity(slave);
	}
}


/*
=============
G_Physics_NewToss

Toss, bounce, and fly movement. When on ground and no velocity, do nothing. With velocity,
slide.
=============
*/
void G_Physics_NewToss(gentity_t* ent) {
	trace_t trace;
	Vector3	move;
	//	float		backoff;
	gentity_t* slave;
	bool	 wasinwater;
	bool	 isinwater;
	float	 speed, newSpeed;
	Vector3	 oldOrigin;
	//	float		firstmove;
	//	int			mask;

	// regular thinking
	G_RunThink(ent);

	// if not a team captain, so movement will be handled elsewhere
	if (ent->flags & FL_TEAMSLAVE)
		return;

	wasinwater = ent->waterLevel;

	// find out what we're sitting on.
	move = ent->s.origin;
	move[2] -= 0.25f;
	trace = gi.trace(ent->s.origin, ent->mins, ent->maxs, move, ent, ent->clipMask);
	if (ent->groundEntity && ent->groundEntity->inUse)
		ent->groundEntity = trace.ent;
	else
		ent->groundEntity = nullptr;

	// if we're sitting on something flat and have no velocity of our own, return.
	if (ent->groundEntity && (trace.plane.normal[2] == 1.0f) &&
		!ent->velocity[_X] && !ent->velocity[_Y] && !ent->velocity[_Z]) {
		return;
	}

	// store the old origin
	oldOrigin = ent->s.origin;

	G_CheckVelocity(ent);

	// add gravity
	G_AddGravity(ent);

	if (ent->aVelocity[0] || ent->aVelocity[1] || ent->aVelocity[2])
		G_AddRotationalFriction(ent);

	// add friction
	speed = ent->velocity.length();
	if (ent->waterLevel) // friction for water movement
	{
		newSpeed = speed - (g_water_friction * 6 * (float)ent->waterLevel);
		if (newSpeed < 0)
			newSpeed = 0;
		newSpeed /= speed;
		ent->velocity *= newSpeed;
	}
	else if (!ent->groundEntity) // friction for air movement
	{
		newSpeed = speed - ((g_friction));
		if (newSpeed < 0)
			newSpeed = 0;
		newSpeed /= speed;
		ent->velocity *= newSpeed;
	}
	else // use ground friction
	{
		newSpeed = speed - (g_friction * 6);
		if (newSpeed < 0)
			newSpeed = 0;
		newSpeed /= speed;
		ent->velocity *= newSpeed;
	}

	G_FlyMove(ent, gi.frameTimeSec, ent->clipMask);
	gi.linkEntity(ent);

	TouchTriggers(ent);

	// check for water transition
	wasinwater = (ent->waterType & MASK_WATER);
	ent->waterType = gi.pointContents(ent->s.origin);
	isinwater = ent->waterType & MASK_WATER;

	if (isinwater)
		ent->waterLevel = WATER_FEET;
	else
		ent->waterLevel = WATER_NONE;

	if (!wasinwater && isinwater)
		gi.positionedSound(oldOrigin, g_entities, CHAN_AUTO, gi.soundIndex("misc/h2ohit1.wav"), 1, 1, 0);
	else if (wasinwater && !isinwater)
		gi.positionedSound(ent->s.origin, g_entities, CHAN_AUTO, gi.soundIndex("misc/h2ohit1.wav"), 1, 1, 0);

	// move teamslaves
	for (slave = ent->teamChain; slave; slave = slave->teamChain) {
		slave->s.origin = ent->s.origin;
		gi.linkEntity(slave);
	}
}

/*
===============================================================================

STEPPING MOVEMENT

===============================================================================
*/

/*
=============
G_Physics_Step

Monsters freefall when they don't have a ground entity, otherwise
all movement is done with discrete steps.

This is also used for objects that have become still on the ground, but
will fall if the floor is pulled out from under them.
=============
*/

void G_AddRotationalFriction(gentity_t* ent) {
	int	  n;
	float adjustment;

	ent->s.angles += (ent->aVelocity * gi.frameTimeSec);
	adjustment = gi.frameTimeSec * g_stopspeed->value * g_friction;

	for (n = 0; n < 3; n++) {
		if (ent->aVelocity[n] > 0) {
			ent->aVelocity[n] -= adjustment;
			if (ent->aVelocity[n] < 0)
				ent->aVelocity[n] = 0;
		}
		else {
			ent->aVelocity[n] += adjustment;
			if (ent->aVelocity[n] > 0)
				ent->aVelocity[n] = 0;
		}
	}
}

static void G_Physics_Step(gentity_t* ent) {
	bool	   wasonground;
	bool	   hitsound = false;
	float* vel;
	float	   speed, newSpeed, control;
	float	   friction;
	gentity_t* groundEntity;
	contents_t mask = G_GetClipMask(ent);

	// airborne monsters should always check for ground
	if (!ent->groundEntity)
		M_CheckGround(ent, mask);

	groundEntity = ent->groundEntity;

	G_CheckVelocity(ent);

	if (groundEntity)
		wasonground = true;
	else
		wasonground = false;

	if (ent->aVelocity[0] || ent->aVelocity[1] || ent->aVelocity[2])
		G_AddRotationalFriction(ent);

	// FIXME: figure out how or why this is happening
	if (isnan(ent->velocity[_X]) || isnan(ent->velocity[_Y]) || isnan(ent->velocity[_Z]))
		ent->velocity = {};

	// add gravity except:
	//   flying monsters
	//   swimming monsters who are in the water
	if (!wasonground)
		if (!(ent->flags & FL_FLY))
			if (!((ent->flags & FL_SWIM) && (ent->waterLevel > WATER_WAIST))) {
				if (ent->velocity[_Z] < level.gravity * -0.1f)
					hitsound = true;
				if (ent->waterLevel != WATER_UNDER)
					G_AddGravity(ent);
			}

	// friction for flying monsters that have been given vertical velocity
	if ((ent->flags & FL_FLY) && (ent->velocity[_Z] != 0) && !(ent->monsterInfo.aiFlags & AI_ALTERNATE_FLY)) {
		speed = std::fabs(ent->velocity[_Z]);
		control = speed < g_stopspeed->value ? g_stopspeed->value : speed;
		friction = g_friction / 3;
		newSpeed = speed - (gi.frameTimeSec * control * friction);
		if (newSpeed < 0)
			newSpeed = 0;
		newSpeed /= speed;
		ent->velocity[_Z] *= newSpeed;
	}

	// friction for flying monsters that have been given vertical velocity
	if ((ent->flags & FL_SWIM) && (ent->velocity[_Z] != 0) && !(ent->monsterInfo.aiFlags & AI_ALTERNATE_FLY)) {
		speed = std::fabs(ent->velocity[_Z]);
		control = speed < g_stopspeed->value ? g_stopspeed->value : speed;
		newSpeed = speed - (gi.frameTimeSec * control * g_water_friction * (float)ent->waterLevel);
		if (newSpeed < 0)
			newSpeed = 0;
		newSpeed /= speed;
		ent->velocity[_Z] *= newSpeed;
	}

	if (ent->velocity[_Z] || ent->velocity[_Y] || ent->velocity[_X]) {
		// apply friction
		if ((wasonground || (ent->flags & (FL_SWIM | FL_FLY))) && !(ent->monsterInfo.aiFlags & AI_ALTERNATE_FLY)) {
			vel = &ent->velocity.x;
			speed = sqrtf(vel[0] * vel[0] + vel[1] * vel[1]);
			if (speed) {
				friction = g_friction;

				// Paril: lower friction for dead monsters
				if (ent->deadFlag)
					friction *= 0.5f;

				control = speed < g_stopspeed->value ? g_stopspeed->value : speed;
				newSpeed = speed - gi.frameTimeSec * control * friction;

				if (newSpeed < 0)
					newSpeed = 0;
				newSpeed /= speed;

				vel[0] *= newSpeed;
				vel[1] *= newSpeed;
			}
		}

		Vector3 oldOrigin = ent->s.origin;

		G_FlyMove(ent, gi.frameTimeSec, mask);

		G_TouchProjectiles(ent, oldOrigin);

		M_CheckGround(ent, mask);

		gi.linkEntity(ent);

		// ========
		// PGM - reset this every time they move.
		//       G_touchtriggers will set it back if appropriate
		ent->gravity = 1.0;
		// ========

		// [Paril-KEX] this is something N64 does to avoid doors opening
		// at the start of a level, which triggers some monsters to spawn.
		if (!level.isN64 || level.time > FRAME_TIME_S)
			TouchTriggers(ent);

		if (!ent->inUse)
			return;

		if (ent->groundEntity)
			if (!wasonground)
				if (hitsound && !(RS(Quake1)))
					ent->s.event = EV_FOOTSTEP;
	}

	if (!ent->inUse) // PGM g_touchtrigger free problem
		return;

	if (ent->svFlags & SVF_MONSTER) {
		M_CatagorizePosition(ent, ent->s.origin, ent->waterLevel, ent->waterType);
		M_WorldEffects(ent);

		// [Paril-KEX] last minute hack to fix Stalker upside down gravity
		if (wasonground != !!ent->groundEntity) {
			if (ent->monsterInfo.physicsChange)
				ent->monsterInfo.physicsChange(ent);
		}
	}

	// regular thinking
	G_RunThink(ent);
}

// [Paril-KEX]
static inline void G_RunBmodelAnimation(gentity_t* ent) {
	auto& anim = ent->bmodel_anim;

	if (anim.currently_alternate != anim.alternate) {
		anim.currently_alternate = anim.alternate;
		anim.next_tick = 0_ms;
	}

	if (level.time < anim.next_tick)
		return;

	const auto& speed = anim.alternate ? anim.alt_speed : anim.speed;

	anim.next_tick = level.time + GameTime::from_ms(speed);

	const auto& style = anim.alternate ? anim.alt_style : anim.style;

	const auto& start = anim.alternate ? anim.alt_start : anim.start;
	const auto& end = anim.alternate ? anim.alt_end : anim.end;

	switch (style) {
	case BMODEL_ANIM_FORWARDS:
		if (end >= start)
			ent->s.frame++;
		else
			ent->s.frame--;
		break;
	case BMODEL_ANIM_BACKWARDS:
		if (end >= start)
			ent->s.frame--;
		else
			ent->s.frame++;
		break;
	case BMODEL_ANIM_RANDOM:
		ent->s.frame = irandom(start, end + 1);
		break;
	}

	const auto& nowrap = anim.alternate ? anim.alt_nowrap : anim.nowrap;

	if (nowrap) {
		if (end >= start)
			ent->s.frame = std::clamp(ent->s.frame, start, end);
		else
			ent->s.frame = std::clamp(ent->s.frame, end, start);
	}
	else {
		if (ent->s.frame < start)
			ent->s.frame = end;
		else if (ent->s.frame > end)
			ent->s.frame = start;
	}
}

//============================================================================

/*
================
G_RunEntity

================
*/
void G_RunEntity(gentity_t* ent) {
	trace_t trace;
	Vector3	previousOrigin;
	bool	has_previousOrigin = false;

	if (level.timeoutActive)
		return;

	if (ent->moveType == MoveType::Step) {
		previousOrigin = ent->s.origin;
		has_previousOrigin = true;
	}

	if (ent->preThink)
		ent->preThink(ent);

	// bmodel animation stuff runs first, so custom entities
	// can override them
	if (ent->bmodel_anim.enabled)
		G_RunBmodelAnimation(ent);

	switch (ent->moveType) {
		using enum MoveType;
	case Push:
	case Stop:
		G_Physics_Pusher(ent);
		break;
	case None:
		G_Physics_None(ent);
		break;
	case NoClip:
	case FreeCam:
		G_Physics_NoClip(ent);
		break;
	case Step:
		G_Physics_Step(ent);
		break;
	case Toss:
	case Bounce:
	case Fly:
	case FlyMissile:
	case WallBounce:
		G_Physics_Toss(ent);
		break;
	case NewToss:
		G_Physics_NewToss(ent);
		break;
	default:
		gi.Com_ErrorFmt("{}: bad moveType {}", __FUNCTION__, (int32_t)ent->moveType);
	}

	if (has_previousOrigin && ent->moveType == MoveType::Step) {
		// if we moved, check and fix origin if needed
		if (ent->s.origin != previousOrigin) {
			trace = gi.trace(ent->s.origin, ent->mins, ent->maxs, previousOrigin, ent, G_GetClipMask(ent));
			if (trace.allSolid || trace.startSolid)
				ent->s.origin = previousOrigin;
		}
	}

	// try to fix buggy lifts this way
	if (has_previousOrigin && ent->moveType == MoveType::Stop) {
		if (ent->s.origin == previousOrigin) {
			switch (ent->moveInfo.state) {
				using enum MoveState;
			case Top:
			case Bottom:
				break;
			case Up:
				ent->s.origin[_Z] = (int)ceil(ent->s.origin[_Z]);
				gi.Com_Print("attempting mover fix\n");
				break;
			case Down:
				ent->s.origin[_Z] = (int)floor(ent->s.origin[_Z]);
				gi.Com_Print("attempting mover fix\n");
				break;
			}
			if (ent->s.origin != previousOrigin) {
				trace = gi.trace(ent->s.origin, ent->mins, ent->maxs, ent->s.origin, ent, G_GetClipMask(ent));
				if (trace.allSolid || trace.startSolid)
					ent->s.origin = previousOrigin;
			}
		}
	}

	if (ent->postThink)
		ent->postThink(ent);
}
