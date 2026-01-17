/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

g_turret.cpp (Game Turret Logic) This file contains the logic for all turret-based entities in
the game. It manages the behavior of the turret driver (the AI brain), the base, and the breach
(the firing mechanism), allowing them to work together as a cohesive unit. Key Responsibilities:
- Turret AI: Implements the `turret_driver_think` function, which handles target acquisition,
aiming, and firing decisions for the turret. - Movement and Rotation: The `turret_breach_think`
function controls the physical rotation of the turret barrel to track targets. - Blocking and
Damage: Defines how turrets react when their movement is blocked by another entity. -
Activation: Handles the logic for turrets that are spawned inactive and must be triggered by
another entity before they begin operating.*/

#include "../g_local.hpp"

/*
=============
	TurretRequestFire

Marks a turret breach to fire on its next think tick.
=============
*/
void TurretRequestFire(gentity_t* breach) {
	if (!breach)
	return;

	breach->turretFireRequested = true;
}

/*
=============
TurretConsumeFireRequest

Returns true if a pending turret breach fire request is present and clears it.
=============
*/
bool TurretConsumeFireRequest(gentity_t* breach) {
	if (!breach || !breach->turretFireRequested)
	return false;

	breach->turretFireRequested = false;
	return true;
}

/*
=============
AnglesNormalize
=============
*/
static void AnglesNormalize(Vector3& vec) {
	while (vec[0] > 360)
	vec[0] -= 360;
	while (vec[0] < 0)
	vec[0] += 360;
	while (vec[1] > 360)
	vec[1] -= 360;
	while (vec[1] < 0)
	vec[1] += 360;
}

/*
=============
turret_blocked
=============
*/
MOVEINFO_BLOCKED(turret_blocked) (gentity_t* self, gentity_t* other) -> void {
	gentity_t* attacker;

	if (other->takeDamage) {
		if (self->teamMaster->owner)
			attacker = self->teamMaster->owner;
		else
			attacker = self->teamMaster;
		Damage(other, self, attacker, vec3_origin, other->s.origin, vec3_origin, self->teamMaster->dmg, 10, DamageFlags::Normal, ModID::Crushed);
	}
}

/*QUAKED turret_breach (0 0 0) ? x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
This portion of the turret can change both pitch and yaw.
The model  should be made with a flat pitch.
It (and the associated base) need to be oriented towards 0.
Use "angle" to set the starting angle.

"speed"		default 50
"dmg"		default 10
"angle"		point this forward
"target"	point this at an info_notnull at the muzzle tip
"minPitch"	min acceptable pitch angle : default -30
"maxPitch"	max acceptable pitch angle : default 30
"minYaw"	min acceptable yaw angle   : default 0
"maxYaw"	max acceptable yaw angle   : default 360
*/

/*
=============
		turret_breach_fire
=============
*/
static void turret_breach_fire(gentity_t* self) {
	Vector3 f, r, u;
	Vector3 start;
	int	   damage;
	int	   speed;

	AngleVectors(self->s.angles, f, r, u);
	start = self->s.origin + (f * self->moveOrigin[_X]);
	start += (r * self->moveOrigin[_Y]);
	start += (u * self->moveOrigin[_Z]);

	if (self->count)
		damage = self->count;
	else
		damage = (int)frandom(100, 150);
	speed = 550 + 50 * skill->integer;
	gentity_t* rocket = fire_rocket(self->teamMaster->owner->activator ? self->teamMaster->owner->activator : self->teamMaster->owner, start, f, damage, speed, 150, damage);
	rocket->s.scale = self->teamMaster->splashRadius;

	gi.positionedSound(start, self, CHAN_WEAPON, gi.soundIndex("chick/chkatck2.wav"), 1, ATTN_NORM, 0);
}

/*
=============
turret_breach_think
=============
*/
static THINK(turret_breach_think) (gentity_t* self) -> void {
	gentity_t* ent;
	Vector3	 current_angles;
	Vector3	 delta;

	current_angles = self->s.angles;
	AnglesNormalize(current_angles);

	AnglesNormalize(self->moveAngles);
	if (self->moveAngles[PITCH] > 180)
		self->moveAngles[PITCH] -= 360;

	// std::clamp angles to mins & maxs
	if (self->moveAngles[PITCH] > self->pos1[PITCH])
		self->moveAngles[PITCH] = self->pos1[PITCH];
	else if (self->moveAngles[PITCH] < self->pos2[PITCH])
		self->moveAngles[PITCH] = self->pos2[PITCH];

	if ((self->moveAngles[YAW] < self->pos1[YAW]) || (self->moveAngles[YAW] > self->pos2[YAW])) {
		float dmin, dmax;

		dmin = std::fabs(self->pos1[YAW] - self->moveAngles[YAW]);
		if (dmin < -180)
			dmin += 360;
		else if (dmin > 180)
			dmin -= 360;
		dmax = std::fabs(self->pos2[YAW] - self->moveAngles[YAW]);
		if (dmax < -180)
			dmax += 360;
		else if (dmax > 180)
			dmax -= 360;
		if (std::fabs(dmin) < std::fabs(dmax))
			self->moveAngles[YAW] = self->pos1[YAW];
		else
			self->moveAngles[YAW] = self->pos2[YAW];
	}

	delta = self->moveAngles - current_angles;
	if (delta[0] < -180)
		delta[0] += 360;
	else if (delta[0] > 180)
		delta[0] -= 360;
	if (delta[1] < -180)
		delta[1] += 360;
	else if (delta[1] > 180)
		delta[1] -= 360;
	delta[2] = 0;

	if (delta[0] > self->speed * gi.frameTimeSec)
		delta[0] = self->speed * gi.frameTimeSec;
	if (delta[0] < -1 * self->speed * gi.frameTimeSec)
		delta[0] = -1 * self->speed * gi.frameTimeSec;
	if (delta[1] > self->speed * gi.frameTimeSec)
		delta[1] = self->speed * gi.frameTimeSec;
	if (delta[1] < -1 * self->speed * gi.frameTimeSec)
		delta[1] = -1 * self->speed * gi.frameTimeSec;

	for (ent = self->teamMaster; ent; ent = ent->teamChain) {
		if (ent->noiseIndex) {
			if (delta[0] || delta[1]) {
				ent->s.sound = ent->noiseIndex;
				ent->s.loopAttenuation = ATTN_NORM;
			}
			else
				ent->s.sound = 0;
		}
	}

	self->aVelocity = delta * (1.0f / gi.frameTimeSec);

	self->nextThink = level.time + FRAME_TIME_S;

	for (ent = self->teamMaster; ent; ent = ent->teamChain)
		ent->aVelocity[1] = self->aVelocity[1];

	// if we have a driver, adjust his velocities
	if (self->owner) {
		float  angle;
		float  target_z;
		float  diff;
		Vector3 target{};
		Vector3 dir;

		// angular is easy, just copy ours
		self->owner->aVelocity[0] = self->aVelocity[0];
		self->owner->aVelocity[1] = self->aVelocity[1];

		// x & y
		angle = self->s.angles[YAW] + self->owner->moveOrigin[_Y];
		angle *= (float)(PI * 2 / 360);
		target[0] = self->s.origin[_X] + cosf(angle) * self->owner->moveOrigin[_X];
		target[1] = self->s.origin[_Y] + std::sinf(angle) * self->owner->moveOrigin[_X];
		target[2] = self->owner->s.origin[_Z];

		dir = target - self->owner->s.origin;
		self->owner->velocity[_X] = dir[0] * 1.0f / gi.frameTimeSec;
		self->owner->velocity[_Y] = dir[1] * 1.0f / gi.frameTimeSec;

		// z
		angle = self->s.angles[PITCH] * (float)(PI * 2 / 360);
		target_z = self->s.origin[_Z] + self->owner->moveOrigin[_X] * tan(angle) + self->owner->moveOrigin[_Z];

		diff = target_z - self->owner->s.origin[_Z];
		self->owner->velocity[_Z] = diff * 1.0f / gi.frameTimeSec;

		if (TurretConsumeFireRequest(self))
		turret_breach_fire(self);
}
}

/*
=============
turret_breach_finish_init
=============
*/
static THINK(turret_breach_finish_init) (gentity_t* self) -> void {
	// get and save info for muzzle location
	if (!self->target) {
		gi.Com_PrintFmt("{}: needs a target\n", *self);
	}
	else {
		self->targetEnt = PickTarget(self->target);
		if (self->targetEnt) {
			self->moveOrigin = self->targetEnt->s.origin - self->s.origin;
			FreeEntity(self->targetEnt);
		}
		else
			gi.Com_PrintFmt("{}: could not find target entity \"{}\"\n", *self, self->target);
	}

	self->teamMaster->dmg = self->dmg;
	self->teamMaster->splashRadius = self->splashRadius; // scale
	self->think = turret_breach_think;
	self->think(self);
}

/*
=============
SP_turret_breach
=============
*/
void SP_turret_breach(gentity_t* self) {
	self->solid = SOLID_BSP;
	self->moveType = MoveType::Push;

	if (st.noise)
		self->noiseIndex = gi.soundIndex(st.noise);

	gi.setModel(self, self->model);

	if (!self->speed)
		self->speed = 50;
	if (!self->dmg)
		self->dmg = 10;

	if (!st.minPitch)
		st.minPitch = -30;
	if (!st.maxPitch)
		st.maxPitch = 30;
	if (!st.maxYaw)
		st.maxYaw = 360;

	self->pos1[PITCH] = -1 * st.minPitch;
	self->pos1[YAW] = st.minYaw;
	self->pos2[PITCH] = -1 * st.maxPitch;
	self->pos2[YAW] = st.maxYaw;

	// scale used for rocket scale
	self->splashRadius = self->s.scale;
	self->s.scale = 0;

	self->ideal_yaw = self->s.angles[YAW];
	self->moveAngles[YAW] = self->ideal_yaw;

	self->moveInfo.blocked = turret_blocked;

	self->think = turret_breach_finish_init;
	self->nextThink = level.time + FRAME_TIME_S;
	gi.linkEntity(self);
}

/*QUAKED turret_base (0 0 0) ? x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
This portion of the turret changes yaw only.
MUST be teamed with a turret_breach.
*/

/*
=============
SP_turret_base
=============
*/
void SP_turret_base(gentity_t* self) {
	self->solid = SOLID_BSP;
	self->moveType = MoveType::Push;

	if (st.noise)
		self->noiseIndex = gi.soundIndex(st.noise);

	gi.setModel(self, self->model);
	self->moveInfo.blocked = turret_blocked;
	gi.linkEntity(self);
}

/*QUAKED turret_driver (1 .5 0) (-16 -16 -24) (16 16 32) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Must NOT be on the team with the rest of the turret parts.
Instead it must target the turret_breach.
*/

void infantry_die(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, const Vector3& point, const MeansOfDeath& mod);
void infantry_stand(gentity_t* self);
void infantry_pain(gentity_t* self, gentity_t* other, float kick, int damage, const MeansOfDeath& mod);
void infantry_setskin(gentity_t* self);

/*
=============
turret_driver_die
=============
*/
static DIE(turret_driver_die) (gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, const Vector3& point, const MeansOfDeath& mod) -> void {
	if (!self->deadFlag) {
		gentity_t* ent;

		// level the gun
		self->targetEnt->moveAngles[PITCH] = 0;

		// remove the driver from the end of them team chain
		for (ent = self->targetEnt->teamMaster; ent->teamChain != self; ent = ent->teamChain)
			;
		ent->teamChain = nullptr;
		self->teamMaster = nullptr;
		self->flags &= ~FL_TEAMSLAVE;

		self->targetEnt->owner = nullptr;
		self->targetEnt->teamMaster->owner = nullptr;

		self->targetEnt->moveInfo.blocked = nullptr;

		// clear pitch
		self->s.angles[PITCH] = 0;
		self->moveType = MoveType::Step;

		self->think = monster_think;
	}

	infantry_die(self, inflictor, attacker, damage, point, mod);

	G_FixStuckObject(self, self->s.origin);
	AngleVectors(self->s.angles, self->velocity, nullptr, nullptr);
	self->velocity *= -50;
	self->velocity.z += 110.f;
}

bool FindTarget(gentity_t* self);

/*
=============
turret_driver_think
=============
*/
static THINK(turret_driver_think) (gentity_t* self) -> void {
	Vector3 target;
	Vector3 dir;

	self->nextThink = level.time + FRAME_TIME_S;

	if (self->enemy && (!self->enemy->inUse || self->enemy->health <= 0))
		self->enemy = nullptr;

	if (!self->enemy) {
		if (!FindTarget(self))
			return;
		self->monsterInfo.trailTime = level.time;
		self->monsterInfo.aiFlags &= ~AI_LOST_SIGHT;
	}
	else {
		if (visible(self, self->enemy)) {
			if (self->monsterInfo.aiFlags & AI_LOST_SIGHT) {
				self->monsterInfo.trailTime = level.time;
				self->monsterInfo.aiFlags &= ~AI_LOST_SIGHT;
			}
		}
		else {
			self->monsterInfo.aiFlags |= AI_LOST_SIGHT;
			return;
		}
	}

	// let the turret know where we want it to aim

	if (!self->enemy)
		return;

	target = self->enemy->s.origin;
	target[2] += self->enemy->viewHeight;
	dir = target - self->targetEnt->s.origin;
	self->targetEnt->moveAngles = VectorToAngles(dir);

	// decide if we should shoot
	if (level.time < self->monsterInfo.attackFinished)
		return;

	GameTime reaction_time = GameTime::from_sec(3 - skill->integer);
	if ((level.time - self->monsterInfo.trailTime) < reaction_time)
		return;

	self->monsterInfo.attackFinished = level.time + reaction_time + 1_sec;
	TurretRequestFire(self->targetEnt);
}

/*
=============
turret_driver_link
=============
*/
static THINK(turret_driver_link) (gentity_t* self) -> void {
	Vector3	 vec{};
	gentity_t* ent;

	self->think = turret_driver_think;
	self->nextThink = level.time + FRAME_TIME_S;

	self->targetEnt = PickTarget(self->target);
	self->targetEnt->owner = self;
	self->targetEnt->teamMaster->owner = self;
	self->s.angles = self->targetEnt->s.angles;

	vec[0] = self->targetEnt->s.origin[_X] - self->s.origin[_X];
	vec[1] = self->targetEnt->s.origin[_Y] - self->s.origin[_Y];
	vec[2] = 0;
	self->moveOrigin[_X] = vec.length();

	vec = self->s.origin - self->targetEnt->s.origin;
	vec = VectorToAngles(vec);
	AnglesNormalize(vec);
	self->moveOrigin[_Y] = vec[1];

	self->moveOrigin[_Z] = self->s.origin[_Z] - self->targetEnt->s.origin[_Z];

	// add the driver to the end of them team chain
	for (ent = self->targetEnt->teamMaster; ent->teamChain; ent = ent->teamChain)
		;
	ent->teamChain = self;
	self->teamMaster = self->targetEnt->teamMaster;
	self->flags |= FL_TEAMSLAVE;
}

void InfantryPrecache();

/*
=============
SP_turret_driver
=============
*/
void SP_turret_driver(gentity_t* self) {
	if (deathmatch->integer) {
		FreeEntity(self);
		return;
	}

	InfantryPrecache();

	self->moveType = MoveType::Push;
	self->solid = SOLID_BBOX;
	self->s.modelIndex = gi.modelIndex("models/monsters/infantry/tris.md2");
	self->mins = { -16, -16, -24 };
	self->maxs = { 16, 16, 32 };

	self->health = self->maxHealth = 100;
	self->gibHealth = GIB_HEALTH;
	self->mass = 200;
	self->viewHeight = 24;

	self->pain = infantry_pain;
	self->die = turret_driver_die;
	self->monsterInfo.stand = infantry_stand;

	self->flags |= FL_NO_KNOCKBACK;

	if (g_debug_monster_kills->integer)
		level.campaign.monstersRegistered[level.campaign.totalMonsters] = self;
	level.campaign.totalMonsters++;

	self->svFlags |= SVF_MONSTER;
	self->takeDamage = true;
	self->use = monster_use;
	self->clipMask = MASK_MONSTERSOLID;
	self->s.oldOrigin = self->s.origin;
	self->monsterInfo.aiFlags |= AI_STAND_GROUND;
	self->monsterInfo.setSkin = infantry_setskin;

	if (st.item) {
		self->item = FindItemByClassname(st.item);
		if (!self->item)
			gi.Com_PrintFmt("{}: bad item: {}\n", *self, st.item);
	}

	self->think = turret_driver_link;
	self->nextThink = level.time + FRAME_TIME_S;

	gi.linkEntity(self);
}

// invisible turret drivers so we can have unmanned turrets.
// originally designed to shoot at func_trains and such, so they
// fire at the center of the bounding box, rather than the entity's
// origin.

constexpr SpawnFlags SPAWNFLAG_TURRET_BRAIN_IGNORE_SIGHT = 1_spawnflag;

/*
=============
turret_brain_think
=============
*/
static THINK(turret_brain_think) (gentity_t* self) -> void {
	Vector3	target;
	Vector3	dir;
	Vector3	endPos;
	trace_t trace;

	self->nextThink = level.time + FRAME_TIME_S;

	if (self->enemy) {
		if (!self->enemy->inUse)
			self->enemy = nullptr;
		else if (self->enemy->takeDamage && self->enemy->health <= 0)
			self->enemy = nullptr;
	}

	if (!self->enemy) {
		if (!FindTarget(self))
			return;
		self->monsterInfo.trailTime = level.time;
		self->monsterInfo.aiFlags &= ~AI_LOST_SIGHT;
	}

	if (self->enemy) {
		endPos = self->enemy->absMax + self->enemy->absMin;
		endPos *= 0.5f;
	}

	if (!self->spawnFlags.has(SPAWNFLAG_TURRET_BRAIN_IGNORE_SIGHT)) {
		trace = gi.traceLine(self->targetEnt->s.origin, endPos, self->targetEnt, MASK_SHOT);
		if (trace.fraction == 1 || trace.ent == self->enemy) {
			if (self->monsterInfo.aiFlags & AI_LOST_SIGHT) {
				self->monsterInfo.trailTime = level.time;
				self->monsterInfo.aiFlags &= ~AI_LOST_SIGHT;
			}
		}
		else {
			self->monsterInfo.aiFlags |= AI_LOST_SIGHT;
			return;
		}
	}

	// let the turret know where we want it to aim
	target = endPos;
	dir = target - self->targetEnt->s.origin;
	self->targetEnt->moveAngles = VectorToAngles(dir);

	// decide if we should shoot
	if (level.time < self->monsterInfo.attackFinished)
		return;

	GameTime reaction_time;

	if (self->delay)
		reaction_time = GameTime::from_sec(self->delay);
	else
		reaction_time = GameTime::from_sec(3 - skill->integer);

	if ((level.time - self->monsterInfo.trailTime) < reaction_time)
	return;

	self->monsterInfo.attackFinished = level.time + reaction_time + 1_sec;
	TurretRequestFire(self->targetEnt);
}

// =================
// =================
/*
=============
turret_brain_link
=============
*/
static THINK(turret_brain_link) (gentity_t* self) -> void {
	Vector3	 vec{};
	gentity_t* ent;

	if (self->killTarget) {
		self->enemy = PickTarget(self->killTarget);
	}

	self->think = turret_brain_think;
	self->nextThink = level.time + FRAME_TIME_S;

	self->targetEnt = PickTarget(self->target);
	self->targetEnt->owner = self;
	self->targetEnt->teamMaster->owner = self;
	self->s.angles = self->targetEnt->s.angles;

	vec[0] = self->targetEnt->s.origin[_X] - self->s.origin[_X];
	vec[1] = self->targetEnt->s.origin[_Y] - self->s.origin[_Y];
	vec[2] = 0;
	self->moveOrigin[_X] = vec.length();

	vec = self->s.origin - self->targetEnt->s.origin;
	vec = VectorToAngles(vec);
	AnglesNormalize(vec);
	self->moveOrigin[_Y] = vec[1];

	self->moveOrigin[_Z] = self->s.origin[_Z] - self->targetEnt->s.origin[_Z];

	// add the driver to the end of them team chain
	for (ent = self->targetEnt->teamMaster; ent->teamChain; ent = ent->teamChain)
		ent->activator = self->activator; // pass along activator to breach, etc

	ent->teamChain = self;
	self->teamMaster = self->targetEnt->teamMaster;
	self->flags |= FL_TEAMSLAVE;
}

// =================
// =================
/*
=============
turret_brain_deactivate
=============
*/
static USE(turret_brain_deactivate) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	self->think = nullptr;
	self->nextThink = 0_ms;
}

// =================
// =================
/*
=============
turret_brain_activate
=============
*/
static USE(turret_brain_activate) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	if (!self->enemy)
		self->enemy = activator;

	// wait at least 3 seconds to fire.
	if (self->wait)
		self->monsterInfo.attackFinished = level.time + GameTime::from_sec(self->wait);
	else
		self->monsterInfo.attackFinished = level.time + 3_sec;
	self->use = turret_brain_deactivate;

	// Paril NOTE: rhangar1 has a turret_invisible_brain that breaks the
	// hangar ceiling; once the final rocket explodes the barrier,
	// it attempts to print "Barrier neutralized." to the rocket owner
	// who happens to be this brain rather than the player that activated
	// the turret. this resolves this by passing it along to fire_rocket.
	self->activator = activator;

	self->think = turret_brain_link;
	self->nextThink = level.time + FRAME_TIME_S;
}

/*QUAKED turret_invisible_brain (1 .5 0) (-16 -16 -16) (16 16 16) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Invisible brain to drive the turret.

Does not search for targets. If targeted, can only be turned on once
and then off once. After that they are completely disabled.

"delay" the delay between firing (default ramps for skill level)
"Target" the turret breach
"Killtarget" the item you want it to attack.
Target the brain if you want it activated later, instead of immediately. It will wait 3 seconds
before firing to acquire the target.
*/
/*
=============
SP_turret_invisible_brain
=============
*/
void SP_turret_invisible_brain(gentity_t* self) {
	if (!self->killTarget) {
		gi.Com_Print("turret_invisible_brain with no killTarget!\n");
		FreeEntity(self);
		return;
	}
	if (!self->target) {
		gi.Com_Print("turret_invisible_brain with no target!\n");
		FreeEntity(self);
		return;
	}

	if (self->targetName) {
		self->use = turret_brain_activate;
	}
	else {
		self->think = turret_brain_link;
		self->nextThink = level.time + FRAME_TIME_S;
	}

	self->moveType = MoveType::Push;
	gi.linkEntity(self);
}
