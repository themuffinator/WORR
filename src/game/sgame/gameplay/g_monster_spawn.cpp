/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

g_monster_spawn.cpp (Game Monster Spawning) This file provides a set of utility functions
specifically for spawning monsters into the world, often used by other entities like the Carrier
or Medic Commander, or by game modes like Horde. Key Responsibilities: - Spawn Point Validation:
Implements `FindSpawnPoint`, `CheckSpawnPoint`, and `CheckGroundSpawnPoint` to find safe and
valid locations on the ground or in the air for new monsters to appear without getting stuck in
geometry. - Monster Creation: Provides high-level wrapper functions like `CreateFlyMonster` and
`CreateGroundMonster` that combine the spawn point validation with the actual entity creation
process. - Visual Effects: Contains the logic for the "spawngrow" effect, a visual cue that
plays where a monster is about to materialize.*/

#include <cmath>

#include "../g_local.hpp"

/*
=============
GetSpawnGravity

Returns the normalized gravity vector for spawn validation, defaulting to
standard downward gravity when no world entity is available or when the stored
gravity vector is zero.
=============
*/
static Vector3 GetSpawnGravity() {
	Vector3 gravity = { 0.0f, 0.0f, -1.0f };

	if (g_entities)
		gravity = world->gravityVector;

	if (gravity.is_zero())
		gravity = { 0.0f, 0.0f, -1.0f };

	gravity.normalize();

	return gravity;
}

//
// Monster spawning code
//
// Used by the carrier, the medic_commander, and the black widow
//
// The sequence to create a flying monster is:
//
//  FindSpawnPoint - tries to find suitable spot to spawn the monster in
//  CreateFlyMonster  - this verifies the point as good and creates the monster

// To create a ground walking monster:
//
//  FindSpawnPoint - same thing
//  CreateGroundMonster - this checks the volume and makes sure the floor under the volume is suitable
//

// FIXME - for the black widow, if we want the stalkers coming in on the roof, we'll have to tweak some things

/*
=============
CreateMonster

Spawns and initializes a monster entity at the given origin/angles, marking
it as not counting toward kill totals.
=============
*/
gentity_t* CreateMonster(const Vector3& origin, const Vector3& angles, const char* className) {
	gentity_t* newEnt;

	newEnt = Spawn();

	newEnt->s.origin = origin;
	newEnt->s.angles = angles;
	newEnt->className = className;
	newEnt->monsterInfo.aiFlags |= AI_DO_NOT_COUNT;

	newEnt->gravityVector = { 0, 0, -1 };
	ED_CallSpawn(newEnt);
	newEnt->s.renderFX |= RF_IR_VISIBLE;

	return newEnt;
}

/*
=============
CreateFlyMonster
=============
*/
gentity_t* CreateFlyMonster(const Vector3& origin, const Vector3& angles, const Vector3& mins, const Vector3& maxs, const char* className) {
	if (!CheckSpawnPoint(origin, mins, maxs))
		return nullptr;

	return (CreateMonster(origin, angles, className));
}

// This is just a wrapper for CreateMonster that looks down height # of CMUs and sees if there
// are bad things down there or not

/*
=============
CreateGroundMonster
=============
*/
gentity_t* CreateGroundMonster(const Vector3& origin, const Vector3& angles, const Vector3& entMins, const Vector3& entMaxs, const char* className, float height, bool ceiling) {
	gentity_t* newEnt;

	// check the ground to make sure it's there, it's relatively flat, and it's not toxic
	if (!CheckGroundSpawnPoint(origin, entMins, entMaxs, height, ceiling))
		return nullptr;

	newEnt = CreateMonster(origin, angles, className);
	if (!newEnt)
		return nullptr;

	return newEnt;
}

// FindSpawnPoint
// PMM - this is used by the medic commander (possibly by the carrier) to find a good spawn point
// if the startpoint is bad, try above the startpoint for a bit

/*
=============
FindSpawnPoint
=============
*/
bool FindSpawnPoint(const Vector3& startpoint, const Vector3& mins, const Vector3& maxs, Vector3& spawnpoint, float maxMoveUp, bool drop, bool ceiling) {
	const Vector3 gravity = GetSpawnGravity();

	auto TryDrop = [mins, maxs, drop, ceiling, gravity](const Vector3& point, Vector3& out) {
		out = point;

		if (drop)
			return M_droptofloor_generic(out, mins, maxs, gravity, ceiling, nullptr, MASK_MONSTERSOLID, false);

		return CheckSpawnPoint(out, mins, maxs);
	};

	spawnpoint = startpoint;

	if (TryDrop(spawnpoint, spawnpoint))
		return true;

	spawnpoint = startpoint;

	if (G_FixStuckObject_Generic(spawnpoint, mins, maxs, [](const Vector3& start, const Vector3& mins, const Vector3& maxs, const Vector3& end) {
			return gi.trace(start, mins, maxs, end, nullptr, MASK_MONSTERSOLID);
		}) == StuckResult::GoodPosition && TryDrop(spawnpoint, spawnpoint))
		return true;

	const Vector3 moveDir = ceiling ? gravity : -gravity;

	for (float move = 16.0f; move <= maxMoveUp; move += 16.0f) {
		Vector3 candidate = startpoint + (moveDir * move);

		if (TryDrop(candidate, spawnpoint))
			return true;
	}

	return false;
}

// FIXME - all of this needs to be tweaked to handle the new gravity rules
// if we ever want to spawn stuff on the roof

//
// CheckSpawnPoint
//
// PMM - checks volume to make sure we can spawn a monster there (is it solid?)
//
// This is all fliers should need

/*
=============
CheckSpawnPoint
=============
*/
bool CheckSpawnPoint(const Vector3& origin, const Vector3& mins, const Vector3& maxs) {
	trace_t tr;

	if (!mins || !maxs)
		return false;

	tr = gi.trace(origin, mins, maxs, origin, nullptr, MASK_MONSTERSOLID);
	if (tr.startSolid || tr.allSolid)
		return false;

	if (tr.ent != world)
		return false;

	return true;
}

// CheckGroundSpawnPoint
//
// PMM - used for walking monsters
//  checks:
//		1)	is there a ground within the specified height of the origin?
//		2)	is the ground non-water?
//		3)	is the ground flat enough to walk on?
//

/*
=============
CheckGroundSpawnPoint
=============
*/
bool CheckGroundSpawnPoint(const Vector3& origin, const Vector3& entMins, const Vector3& entMaxs, float height, bool ceiling) {
	if (!CheckSpawnPoint(origin, entMins, entMaxs))
		return false;

	const Vector3 gravity = GetSpawnGravity();
	const Vector3 dropDir = ceiling ? -gravity : gravity;
	const bool verticalGravity = fabsf(gravity.x) < 0.001f && fabsf(gravity.y) < 0.001f;

	trace_t support = gi.trace(origin, entMins, entMaxs, origin + (dropDir * height), nullptr, MASK_MONSTERSOLID);

	if (support.startSolid || support.allSolid || support.fraction == 1.0f || support.ent != world)
		return false;

	if (!support.plane.normal.is_zero()) {
		const float alignment = support.plane.normal.dot(-dropDir);

		if (alignment < 0.7f)
			return false;
	}

	if (verticalGravity) {
		Vector3 gravityDir = ceiling ? Vector3(0, 0, 1) : Vector3(0, 0, -1);
		if (M_CheckBottom_Fast_Generic(support.endPos + entMins, support.endPos + entMaxs, gravityDir))
			return true;

		if (M_CheckBottom_Slow_Generic(support.endPos, entMins, entMaxs, nullptr, MASK_MONSTERSOLID, gravityDir, false))
			return true;

		return false;
	}

	return true;
}

// ****************************
// SPAWNGROW stuff
// ****************************

constexpr GameTime SPAWNGROW_LIFESPAN = 1000_ms;

static THINK(spawngrow_think) (gentity_t* self) -> void {
	if (level.time >= self->timeStamp) {
		FreeEntity(self->targetEnt);
		FreeEntity(self);
		return;
	}

	self->s.angles += self->aVelocity * gi.frameTimeSec;

	float t = 1.f - ((level.time - self->teleportTime).seconds() / self->wait);

	self->s.scale = std::clamp(lerp(self->decel, self->accel, t) / 16.f, 0.001f, 16.f);
	self->s.alpha = t * t;

	self->nextThink += FRAME_TIME_MS;
}

static Vector3 SpawnGro_laser_pos(gentity_t* ent) {
	// pick random direction
	float theta = frandom(2 * PIf);
	float phi = acos(crandom());

	Vector3 d{
		sin(phi) * cos(theta),
		sin(phi) * sin(theta),
		cos(phi)
	};

	return ent->s.origin + (d * ent->owner->s.scale * 9.f);
}

static THINK(SpawnGro_laser_think) (gentity_t* self) -> void {
	self->s.oldOrigin = SpawnGro_laser_pos(self);
	gi.linkEntity(self);
	self->nextThink = level.time + 1_ms;
}

void SpawnGrow_Spawn(const Vector3& startpos, float start_size, float end_size) {
	gentity_t* ent;

	ent = Spawn();
	ent->s.origin = startpos;

	ent->s.angles[PITCH] = (float)irandom(360);
	ent->s.angles[YAW] = (float)irandom(360);
	ent->s.angles[ROLL] = (float)irandom(360);

	ent->aVelocity[0] = frandom(280.f, 360.f) * 2.f;
	ent->aVelocity[1] = frandom(280.f, 360.f) * 2.f;
	ent->aVelocity[2] = frandom(280.f, 360.f) * 2.f;

	ent->solid = SOLID_NOT;
	ent->s.renderFX |= RF_IR_VISIBLE;
	ent->moveType = MoveType::None;
	ent->className = "spawngro";

	ent->s.modelIndex = gi.modelIndex("models/items/spawngro3/tris.md2");
	ent->s.skinNum = 1;

	ent->accel = start_size;
	ent->decel = end_size;
	ent->think = spawngrow_think;

	ent->s.scale = std::clamp(start_size / 16.f, 0.001f, 8.f);

	ent->teleportTime = level.time;
	ent->wait = SPAWNGROW_LIFESPAN.seconds();
	ent->timeStamp = level.time + SPAWNGROW_LIFESPAN;

	ent->nextThink = level.time + FRAME_TIME_MS;

	gi.linkEntity(ent);

	// [Paril-KEX]
	gentity_t* beam = ent->targetEnt = Spawn();
	beam->s.modelIndex = MODELINDEX_WORLD;
	beam->s.renderFX = RF_BEAM_LIGHTNING | RF_NO_ORIGIN_LERP;
	beam->s.frame = 1;
	beam->s.skinNum = 0x30303030;
	beam->className = "spawngro_beam";
	beam->angle = end_size;
	beam->owner = ent;
	beam->s.origin = ent->s.origin;
	beam->think = SpawnGro_laser_think;
	beam->nextThink = level.time + 1_ms;
	beam->s.oldOrigin = SpawnGro_laser_pos(beam);
	gi.linkEntity(beam);
}

// ****************************
// WidowLeg stuff
// ****************************

constexpr int32_t MAX_LEGSFRAME = 23;
constexpr GameTime LEG_WAIT_TIME = 1_sec;

void ThrowMoreStuff(gentity_t* self, const Vector3& point);
void ThrowSmallStuff(gentity_t* self, const Vector3& point);
void ThrowWidowGibLoc(gentity_t* self, const char* gibname, int damage, gib_type_t type, const Vector3* startpos, bool fade);
void ThrowWidowGibSized(gentity_t* self, const char* gibname, int damage, gib_type_t type, const Vector3* startpos, int hitsound, bool fade);

/*
=============
widowlegs_think

Drives the timing and effects for the widow leg death animation, spawning
explosions and debris before removing the placeholder entity.
=============
*/
static THINK(widowlegs_think) (gentity_t* self) -> void {
	Vector3 offset;
	Vector3 point;
	Vector3 f, r, u;

	if (self->s.frame == 17) {
	offset = { 11.77f, -7.24f, 23.31f };
	AngleVectors(self->s.angles, f, r, u);
	point = G_ProjectSource2(self->s.origin, offset, f, r, u);
	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_EXPLOSION1);
	gi.WritePosition(point);
	gi.multicast(point, MULTICAST_ALL, false);
	ThrowSmallStuff(self, point);
	}

	if (self->s.frame < MAX_LEGSFRAME) {
	self->s.frame++;
	self->nextThink = level.time + 10_hz;
	return;
	}
	else if (self->wait == 0) {
	self->wait = (level.time + LEG_WAIT_TIME).seconds();
	}

	if (level.time > GameTime::from_sec(self->wait)) {
	AngleVectors(self->s.angles, f, r, u);

	offset = { -65.6f, -8.44f, 28.59f };
	point = G_ProjectSource2(self->s.origin, offset, f, r, u);
	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_EXPLOSION1);
	gi.WritePosition(point);
	gi.multicast(point, MULTICAST_ALL, false);
	ThrowSmallStuff(self, point);

	ThrowWidowGibSized(self, "models/monsters/blackwidow/gib1/tris.md2", 80 + (int)frandom(20.0f), GIB_METALLIC, &point, 0, true);
	ThrowWidowGibSized(self, "models/monsters/blackwidow/gib2/tris.md2", 80 + (int)frandom(20.0f), GIB_METALLIC, &point, 0, true);

	offset = { -1.04f, -51.18f, 7.04f };
	point = G_ProjectSource2(self->s.origin, offset, f, r, u);
	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_EXPLOSION1);
	gi.WritePosition(point);
	gi.multicast(point, MULTICAST_ALL, false);
	ThrowSmallStuff(self, point);

	ThrowWidowGibSized(self, "models/monsters/blackwidow/gib1/tris.md2", 80 + (int)frandom(20.0f), GIB_METALLIC, &point, 0, true);
	ThrowWidowGibSized(self, "models/monsters/blackwidow/gib2/tris.md2", 80 + (int)frandom(20.0f), GIB_METALLIC, &point, 0, true);
	ThrowWidowGibSized(self, "models/monsters/blackwidow/gib3/tris.md2", 80 + (int)frandom(20.0f), GIB_METALLIC, &point, 0, true);

	FreeEntity(self);
	return;
	}

	if ((level.time > GameTime::from_sec(self->wait - 0.5f)) && (self->count == 0)) {
	self->count = 1;
	AngleVectors(self->s.angles, f, r, u);

	offset = { 31, -88.7f, 10.96f };
	point = G_ProjectSource2(self->s.origin, offset, f, r, u);
	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_EXPLOSION1);
	gi.WritePosition(point);
	gi.multicast(point, MULTICAST_ALL, false);
	//		ThrowSmallStuff (self, point);

	offset = { -12.67f, -4.39f, 15.68f };
	point = G_ProjectSource2(self->s.origin, offset, f, r, u);
	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_EXPLOSION1);
	gi.WritePosition(point);
	gi.multicast(point, MULTICAST_ALL, false);
	//		ThrowSmallStuff (self, point);

	self->nextThink = level.time + 10_hz;
	return;
	}

	self->nextThink = level.time + 10_hz;
}

void Widowlegs_Spawn(const Vector3& startpos, const Vector3& angles) {
	gentity_t* ent;

	ent = Spawn();
	ent->s.origin = startpos;
	ent->s.angles = angles;
	ent->solid = SOLID_NOT;
	ent->s.renderFX = RF_IR_VISIBLE;
	ent->moveType = MoveType::None;
	ent->className = "widowlegs";

	ent->s.modelIndex = gi.modelIndex("models/monsters/legs/tris.md2");
	ent->think = widowlegs_think;

	ent->nextThink = level.time + 10_hz;
	gi.linkEntity(ent);
}
