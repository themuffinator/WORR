/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

g_misc.cpp (Game Miscellaneous Entities) This file contains the implementation for a wide
variety of miscellaneous map entities that don't fit into other major categories like items,
triggers, or monsters. It's a collection of special-purpose objects used for scripting,
decoration, and unique gameplay mechanics. Key Responsibilities: - Decorative Objects:
Implements entities that are primarily for visual effect, such as `misc_banner`,
`misc_deadsoldier`, and `misc_explobox`. - Scripting Helpers: Contains the logic for positional
markers like `path_corner` and `point_combat`, which are used to guide AI movement and
scripting. - Special Effects: Implements entities that create special effects, like
`misc_teleporter` and `misc_blackhole`. - Gibs and Corpses: Manages the spawning and physics of
gibs (`ThrowGib`) and player heads (`ThrowClientHead`) upon death.*/

#include "../g_local.hpp"

//=====================================================

/*
===============
Respawn_Think
===============
*/
THINK(Respawn_Think) (gentity_t* ent) -> void {
	if (!ent->saved)
		return;

	const Vector3& spawnOrigin = ent->saved->origin;
	const Vector3& origin = ent->saved->origin;
	const Vector3& mins = ent->saved->mins;
	const Vector3& maxs = ent->saved->maxs;

	// (a) Check player visibility (in PVS and in front)
	for (int i = 0; i < MAX_CLIENTS; ++i) {
		gentity_t* cl = &g_entities[i];
		if (!cl->inUse || !cl->client)
			continue;

		if (LocCanSee(ent, cl)) {
			ent->nextThink = level.time + 1_sec;
			return;
		}

		Vector3 forward;
		AngleVectors(cl->client->ps.viewAngles, forward, nullptr, nullptr);

		Vector3 dir = spawnOrigin - cl->s.origin;
		dir.normalize();

		const float dot = dir.dot(forward);
		if (dot > 0.15f) {
			ent->nextThink = level.time + 1_sec;
			return;
		}
	}

	// (b) Telefrag check
	Vector3 p = origin + Vector3{ 0.f, 0.f, 9.f };
	trace_t tr = gi.trace(p, mins, maxs, p, ent, CONTENTS_PLAYER | CONTENTS_MONSTER);
	if (tr.startSolid) {
		ent->nextThink = level.time + 1_sec;
		return;
	}

	// (c) Proximity check: is any client inside a 128u radius of bbox?
	Vector3 rangeMins = origin - Vector3{ 128.f, 128.f, 128.f };
	Vector3 rangeMaxs = origin + Vector3{ 128.f, 128.f, 128.f };

	for (int i = 0; i < MAX_CLIENTS; ++i) {
		gentity_t* cl = &g_entities[i];
		if (!cl->inUse || !cl->client)
			continue;

		Vector3 clientMins = cl->s.origin + cl->mins;
		Vector3 clientMaxs = cl->s.origin + cl->maxs;

		if (!(clientMins.x > rangeMaxs.x || clientMaxs.x < rangeMins.x ||
			clientMins.y > rangeMaxs.y || clientMaxs.y < rangeMins.y ||
			clientMins.z > rangeMaxs.z || clientMaxs.z < rangeMins.z)) {
			ent->nextThink = level.time + 1_sec;
			return;
		}
	}

	// Spawn new entity
	gentity_t* newEnt = Spawn();
	newEnt->className = ent->saved->className;
	//Q_strlcpy((char *)newEnt->className, ent->saved->className, sizeof(newEnt->className));
	newEnt->s.origin = ent->saved->origin;
	newEnt->s.angles = ent->saved->angles;
	newEnt->health = ent->saved->health;
	newEnt->dmg = ent->saved->dmg;
	newEnt->s.scale = ent->saved->scale;
	newEnt->target = ent->saved->target;
	newEnt->targetName = ent->saved->targetName;
	newEnt->spawnFlags = ent->saved->spawnFlags;
	newEnt->mass = ent->saved->mass;
	newEnt->mins = ent->saved->mins;
	newEnt->maxs = ent->saved->maxs;
	newEnt->model = ent->saved->model;

	newEnt->saved = ent->saved;
	ent->saved = nullptr;

	newEnt->saved->spawnFunc(newEnt);
	FreeEntity(ent);
}

//=====================================================

/*
=================
Misc functions
=================
*/
void VelocityForDamage(int damage, Vector3& v) {
	v[0] = 100.0f * crandom();
	v[1] = 100.0f * crandom();
	v[2] = frandom(200.0f, 300.0f);

	if (damage < 50)
		v = v * 0.7f;
	else
		v = v * 1.2f;
}

void ClipGibVelocity(gentity_t* ent) {
	if (ent->velocity[_X] < -300)
		ent->velocity[_X] = -300;
	else if (ent->velocity[_X] > 300)
		ent->velocity[_X] = 300;
	if (ent->velocity[_Y] < -300)
		ent->velocity[_Y] = -300;
	else if (ent->velocity[_Y] > 300)
		ent->velocity[_Y] = 300;
	if (ent->velocity[_Z] < 200)
		ent->velocity[_Z] = 200; // always some upwards
	else if (ent->velocity[_Z] > 500)
		ent->velocity[_Z] = 500;
}

/*
=================
gibs
=================
*/
DIE(gib_die) (gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, const Vector3& point, const MeansOfDeath& mod) -> void {
	if (mod.id == ModID::Crushed)
		FreeEntity(self);
}

static TOUCH(gib_touch) (gentity_t* self, gentity_t* other, const trace_t& tr, bool otherTouchingSelf) -> void {
	if (tr.plane.normal[2] > 0.7f) {
		self->s.angles[PITCH] = std::clamp(self->s.angles[PITCH], -5.0f, 5.0f);
		self->s.angles[ROLL] = std::clamp(self->s.angles[ROLL], -5.0f, 5.0f);
	}
}

/*
=============
GibSink

After sitting around for x seconds, fall into the ground and disappear
=============
*/
static THINK(GibSink) (gentity_t* ent) -> void {
	if (!ent->timeStamp)
		ent->timeStamp = level.time + 1_sec;

	if (level.time > ent->timeStamp) {
		ent->svFlags = SVF_NOCLIENT;
		ent->takeDamage = false;
		ent->solid = SOLID_NOT;
		FreeEntity(ent);
		return;
	}
	//ent->s.renderFX = RF_FULLBRIGHT;
	ent->nextThink = level.time + FRAME_TIME_S;
	ent->s.origin[_Z] -= 0.5;
}

/*
=============
GibThink
=============
*/
static THINK(GibThink) (gentity_t* self) -> void {
	if (self->timeStamp && level.time >= self->timeStamp) {

		if (g_instaGib->integer)
			self->nextThink = level.time + random_time(1_sec, 5_sec);
		else
			self->nextThink = level.time + random_time(10_sec, 20_sec);

		self->think = GibSink;
		self->timeStamp = 0_sec;
		return;
	}

	if (self->velocity) {
		float p = self->s.angles.x;
		float z = self->s.angles.z;
		float speed_frac = std::clamp(self->velocity.lengthSquared() / (self->speed * self->speed), 0.f, 1.f);
		self->s.angles = VectorToAngles(self->velocity);
		self->s.angles.x = LerpAngle(p, self->s.angles.x, speed_frac);
		self->s.angles.z = z + (gi.frameTimeSec * 360 * speed_frac);
	}

	self->nextThink = level.time + FRAME_TIME_S;
}

/*
=============
GibTouch
=============
*/
static TOUCH(GibTouch) (gentity_t* ent, gentity_t* other, const trace_t& tr, bool otherTouchingSelf) -> void {
	if (other == ent->owner)
		return;

	if (level.time > ent->pain_debounce_time) {

		if (tr.surface && (tr.surface->flags & SURF_SKY)) {
			FreeEntity(ent);
			return;
		}

		// bounce sound variation
		static constexpr std::array<const char*, 3> gib_sounds = {
			"player/gibimp1.wav",
			"player/gibimp2.wav",
			"player/gibimp3.wav"
		};
		const char* sfx = random_element(gib_sounds);
		gi.sound(ent, CHAN_VOICE, gi.soundIndex(sfx), 1.0f, ATTN_NORM, 0);

		ent->pain_debounce_time = level.time + 500_ms;
	}
}

/*
=============
ThrowGib
=============
*/
gentity_t* ThrowGib(gentity_t* self, std::string gibname, int damage, gib_type_t type, float scale) {
	gentity_t* gib;
	Vector3	 vd;
	Vector3	 origin;
	Vector3	 size;
	float	 vscale;

	if (type & GIB_HEAD) {
		gib = self;
		gib->s.event = EV_OTHER_TELEPORT;
		// remove setSkin so that it doesn't set the skin wrongly later
		self->monsterInfo.setSkin = nullptr;
	}
	else
		gib = Spawn();

	size = self->size * 0.5f;
	// since absMin is bloated by 1, un-bloat it here
	origin = (self->absMin + Vector3{ 1, 1, 1 }) + size;

	int32_t i;

	for (i = 0; i < 3; i++) {
		gib->s.origin = origin + Vector3{ crandom(), crandom(), crandom() }.scaled(size);

		// try 3 times to get a good, non-solid position
		if (!(gi.pointContents(gib->s.origin) & MASK_SOLID))
			break;
	}

	if (i == 3) {
		// only free us if we're not being turned into the gib, otherwise
		// just spawn inside a wall
		if (gib != self) {
			FreeEntity(gib);
			return nullptr;
		}
	}

	gib->s.modelIndex = gi.modelIndex(gibname.c_str());
	gib->s.modelIndex2 = 0;
	gib->s.scale = scale;
	gib->solid = SOLID_NOT;
	gib->svFlags |= SVF_DEADMONSTER;
	gib->svFlags &= ~SVF_MONSTER;
	gib->clipMask = MASK_SOLID;
	gib->s.effects = type ? EF_NONE : EF_GIB;
	gib->s.renderFX = RF_NONE;	// RF_LOW_PRIORITY | RF_FULLBRIGHT;
	//gib->s.renderFX = RF_LOW_PRIORITY;
	gib->s.renderFX |= RF_NOSHADOW;

	if (!(type & GIB_DEBRIS)) {
		if (type & GIB_ACID)
			gib->s.effects |= EF_GREENGIB;
		else
			gib->s.effects |= EF_GIB;
		gib->s.renderFX |= RF_IR_VISIBLE;
	}
	gib->flags |= FL_NO_KNOCKBACK | FL_NO_DAMAGE_EFFECTS;
	gib->takeDamage = true;
	gib->die = gib_die;
	gib->className = "gib";
	if (type & GIB_SKINNED)
		gib->s.skinNum = self->s.skinNum;
	else
		gib->s.skinNum = 0;
	gib->s.frame = 0;
	gib->mins = gib->maxs = {};
	gib->s.sound = 0;
	gib->monsterInfo.engineSound = 0;

	if (Game::Is(GameType::FreezeTag)) {
		gib->s.renderFX |= (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE);
		gib->s.effects |= EF_COLOR_SHELL;
	}

	if (!(type & GIB_METALLIC)) {
		gib->moveType = MoveType::Toss;
		vscale = (type & GIB_ACID) ? 3.0 : 0.5;
	}
	else {
		gib->moveType = MoveType::Bounce;
		vscale = 1.0;
	}

	if (type & GIB_DEBRIS) {
		Vector3 v{};
		v[0] = 100 * crandom();
		v[1] = 100 * crandom();
		v[2] = 100 + 100 * crandom();
		gib->velocity = self->velocity + (v * damage);
	}
	else {
		/*
		VelocityForDamage(damage, vd);
		gib->velocity = self->velocity + (vd * vscale);
		ClipGibVelocity(gib);
		*/
		VelocityForDamage(damage, vd);

		// base velocity plus scaled damage vector
		gib->velocity = self->velocity + (vd * vscale);

		// add a little random 'kick' in all three axes
		Vector3 rnd = { frandom(200.0f), frandom(200.0f), frandom(200.0f) };
		gib->velocity += rnd;

		// std::clamp it so you don't exceed your clip speed
		ClipGibVelocity(gib);
	}

	if (type & GIB_UPRIGHT) {
		gib->touch = gib_touch;
		gib->flags |= FL_ALWAYS_TOUCH;
	}
	else {
		gib->touch = type ? nullptr : GibTouch;
		gib->flags |= FL_ALWAYS_TOUCH;
	}

	gib->aVelocity[0] = 200 + frandom(400);
	gib->aVelocity[1] = 200 + frandom(400);
	gib->aVelocity[2] = 200 + frandom(400);

	gib->s.angles[PITCH] = frandom(359);
	gib->s.angles[YAW] = frandom(359);
	gib->s.angles[ROLL] = frandom(359);

	gib->think = GibThink;

	gib->nextThink = level.time + FRAME_TIME_S;
	gib->timeStamp = gib->nextThink + GameTime::from_sec(1.5);

	gi.linkEntity(gib);

	gib->waterType = gi.pointContents(gib->s.origin);

	if (gib->waterType & MASK_WATER)
		gib->waterLevel = WATER_FEET;
	else
		gib->waterLevel = WATER_NONE;

	gib->clipMask = MASK_PROJECTILE;
	gib->solid = SOLID_BBOX;
	gib->svFlags |= SVF_PROJECTILE;

	return gib;
}

void ThrowClientHead(gentity_t* self, int damage) {
	Vector3		vd;
	const char* gibname;

	if (brandom()) {
		gibname = "models/objects/gibs/head2/tris.md2";
		self->s.skinNum = 1; // second skin is player
	}
	else {
		gibname = "models/objects/gibs/skull/tris.md2";
		self->s.skinNum = 0;
	}

	self->s.origin[_Z] += 16;
	self->s.frame = 0;
	gi.setModel(self, gibname);
	self->mins = { -8, -8, 0 };	//{ -16, -16, 0 };
	self->maxs = { 8, 8, 8 };	//{ 16, 16, 16 };

	self->takeDamage = true; // [Paril-KEX] allow takeDamage so we get crushed
	self->solid = SOLID_TRIGGER; // [Paril-KEX] make 'trigger' so we still move but don't block shots/explode
	self->svFlags |= SVF_DEADMONSTER;
	self->s.effects = EF_GIB;
	self->s.renderFX = RF_LOW_PRIORITY | RF_FULLBRIGHT | RF_IR_VISIBLE;
	self->s.sound = 0;
	self->flags |= FL_NO_KNOCKBACK | FL_NO_DAMAGE_EFFECTS;

	self->moveType = MoveType::Bounce;
	VelocityForDamage(damage, vd);
	self->velocity += vd;

	if (self->client) {	// bodies in the queue don't have a client anymore
		self->client->anim.priority = ANIM_DEATH;
		self->client->anim.end = self->s.frame;
	}
	else {
		self->think = nullptr;
		self->nextThink = 0_ms;
	}

	self->think = GibThink;

	self->touch = GibTouch;

	if (g_instaGib->integer)
		self->nextThink = level.time + random_time(1_sec, 5_sec);
	else
		self->nextThink = level.time + random_time(10_sec, 20_sec);

	self->timeStamp = self->nextThink + GameTime::from_sec(1.5);

	gi.linkEntity(self);
}

void BecomeExplosion1(gentity_t* self) {
	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_EXPLOSION1);
	gi.WritePosition(self->s.origin);
	gi.multicast(self->s.origin, MULTICAST_PHS, false);

	FreeEntity(self);
}

static void BecomeExplosion2(gentity_t* self) {
	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_EXPLOSION2);
	gi.WritePosition(self->s.origin);
	gi.multicast(self->s.origin, MULTICAST_PHS, false);

	FreeEntity(self);
}

/*QUAKED path_corner (.5 .3 0) (-8 -8 -8) (8 8 8) TELEPORT x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Target: next path corner
Pathtarget: gets used when an entity that has
	this path_corner targeted touches it
*/

static TOUCH(path_corner_touch) (gentity_t* self, gentity_t* other, const trace_t& tr, bool otherTouchingSelf) -> void {
	Vector3	 v;
	gentity_t* next;

	if (other->moveTarget != self)
		return;

	if (other->enemy)
		return;

	if (self->pathTarget) {
		const char* savetarget;

		savetarget = self->target;
		self->target = self->pathTarget;
		UseTargets(self, other);
		self->target = savetarget;
	}

	// see m_move; this is just so we don't needlessly check it
	self->flags |= FL_PARTIALGROUND;

	if (self->target)
		next = PickTarget(self->target);
	else
		next = nullptr;

	// [Paril-KEX] don't teleport to a point_combat, it means HOLD for them.
	if ((next) && !strcmp(next->className, "path_corner") && next->spawnFlags.has(SPAWNFLAG_PATH_CORNER_TELEPORT)) {
		v = next->s.origin;
		v[2] += next->mins[2];
		v[2] -= other->mins[2];
		other->s.origin = v;
		next = PickTarget(next->target);
		other->s.event = EV_OTHER_TELEPORT;
	}

	other->goalEntity = other->moveTarget = next;

	if (self->wait) {
		other->monsterInfo.pauseTime = level.time + GameTime::from_sec(self->wait);
		other->monsterInfo.stand(other);
		return;
	}

	if (!other->moveTarget) {
		// N64 cutscene behavior
		if (other->hackFlags & HACKFLAG_END_CUTSCENE) {
			FreeEntity(other);
			return;
		}

		other->monsterInfo.pauseTime = HOLD_FOREVER;
		other->monsterInfo.stand(other);
	}
	else {
		v = other->goalEntity->s.origin - other->s.origin;
		other->ideal_yaw = vectoyaw(v);
	}
}

void SP_path_corner(gentity_t* self) {
	if (!self->targetName) {
		gi.Com_PrintFmt("{} with no targetName\n", *self);
		FreeEntity(self);
		return;
	}

	self->solid = SOLID_TRIGGER;
	self->touch = path_corner_touch;
	self->mins = { -8, -8, -8 };
	self->maxs = { 8, 8, 8 };
	self->svFlags |= SVF_NOCLIENT;
	gi.linkEntity(self);
}

/*QUAKED point_combat (0.5 0.3 0) (-8 -8 -8) (8 8 8) HOLD x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Makes this the target of a monster and it will head here
when first activated before going after the activator.  If
hold is selected, it will stay here.
*/
static TOUCH(point_combat_touch) (gentity_t* self, gentity_t* other, const trace_t& tr, bool otherTouchingSelf) -> void {
	gentity_t* activator;

	if (other->moveTarget != self)
		return;

	if (self->target) {
		other->target = self->target;
		other->goalEntity = other->moveTarget = PickTarget(other->target);
		if (!other->goalEntity) {
			gi.Com_PrintFmt("{} target {} does not exist\n", *self, self->target);
			other->moveTarget = self;
		}
		// [Paril-KEX] allow them to be re-used
		//self->target = nullptr;
	}
	else if (self->spawnFlags.has(SPAWNFLAG_POINT_COMBAT_HOLD) && !(other->flags & (FL_SWIM | FL_FLY))) {
		// already standing
		if (other->monsterInfo.aiFlags & AI_STAND_GROUND)
			return;

		other->monsterInfo.pauseTime = HOLD_FOREVER;
		other->monsterInfo.aiFlags |= AI_STAND_GROUND | AI_REACHED_HOLD_COMBAT | AI_THIRD_EYE;
		other->monsterInfo.stand(other);
	}

	if (other->moveTarget == self) {
		// [Paril-KEX] if we're holding, keep moveTarget set; we will
		// use this to make sure we haven't moved too far from where
		// we want to "guard".
		if (!self->spawnFlags.has(SPAWNFLAG_POINT_COMBAT_HOLD)) {
			other->target = nullptr;
			other->moveTarget = nullptr;
		}

		other->goalEntity = other->enemy;
		other->monsterInfo.aiFlags &= ~AI_COMBAT_POINT;
	}

	if (self->pathTarget) {
		const char* savetarget;

		savetarget = self->target;
		self->target = self->pathTarget;
		if (other->enemy && other->enemy->client)
			activator = other->enemy;
		else if (other->oldEnemy && other->oldEnemy->client)
			activator = other->oldEnemy;
		else if (other->activator && other->activator->client)
			activator = other->activator;
		else
			activator = other;
		UseTargets(self, activator);
		self->target = savetarget;
	}
}

void SP_point_combat(gentity_t* self) {
	if (deathmatch->integer && !ai_allow_dm_spawn->integer) {
		FreeEntity(self);
		return;
	}
	self->solid = SOLID_TRIGGER;
	self->touch = point_combat_touch;
	self->mins = { -8, -8, -16 };
	self->maxs = { 8, 8, 16 };
	self->svFlags = SVF_NOCLIENT;
	gi.linkEntity(self);
}

/*QUAKED info_null (0 0.5 0) (-4 -4 -4) (4 4 4) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Used as a positional target for calculations in the utilities (spotlights, etc), but removed during gameplay.
*/
void SP_info_null(gentity_t* self) {
	FreeEntity(self);
}

/*QUAKED info_notnull (0 0.5 0) (-4 -4 -4) (4 4 4) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Used as a positional target for in-game calculation, like jumppad targets.
target_position does the same thing
*/
void SP_info_notnull(gentity_t* self) {
	self->absMin = self->s.origin;
	self->absMax = self->s.origin;
}

/*QUAKED light (0 1 0) (-8 -8 -8) (8 8 8) START_OFF ALLOW_IN_DM x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Non-displayed light.
Default light value is 300.
Default style is 0.
If targeted, will toggle between on and off.
Default _cone value is 10 (used to set size of light for spotlights)
*/

constexpr SpawnFlags SPAWNFLAG_LIGHT_START_OFF = 1_spawnflag;
constexpr SpawnFlags SPAWNFLAG_LIGHT_ALLOW_IN_DM = 2_spawnflag;

static USE(light_use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	if (self->spawnFlags.has(SPAWNFLAG_LIGHT_START_OFF)) {
		gi.configString(CS_LIGHTS + self->style, self->style_on);
		self->spawnFlags &= ~SPAWNFLAG_LIGHT_START_OFF;
	}
	else {
		gi.configString(CS_LIGHTS + self->style, self->style_off);
		self->spawnFlags |= SPAWNFLAG_LIGHT_START_OFF;
	}
}

// ---------------------------------------------------------------------------------
// [Sam-KEX] For keeping track of shadow light parameters and setting them up on
// the server side.

const shadow_light_data_t* GetShadowLightData(int32_t entity_number) {
	for (size_t i = 0; i < level.shadowLightCount; i++) {
		if (level.shadowLightInfo[i].entity_number == entity_number)
			return &level.shadowLightInfo[i].shadowlight;
	}

	return nullptr;
}

void setup_shadow_lights() {
	for (int i = 0; i < level.shadowLightCount; ++i) {
		gentity_t* self = &g_entities[level.shadowLightInfo[i].entity_number];

		level.shadowLightInfo[i].shadowlight.lightType = shadow_light_type_t::point;
		level.shadowLightInfo[i].shadowlight.coneDirection = {};

		if (self->target) {
			gentity_t* target = G_FindByString<&gentity_t::targetName>(nullptr, self->target);
			if (target) {
				level.shadowLightInfo[i].shadowlight.coneDirection = (target->s.origin - self->s.origin).normalized();
				level.shadowLightInfo[i].shadowlight.lightType = shadow_light_type_t::cone;
			}
		}

		if (self->itemTarget) {
			gentity_t* target = G_FindByString<&gentity_t::targetName>(nullptr, self->itemTarget);
			if (target)
				level.shadowLightInfo[i].shadowlight.lightStyle = target->style;
		}

		gi.configString(CS_SHADOWLIGHTS + i, G_Fmt("{};{};{:1};{};{:1};{:1};{:1};{};{:1};{:1};{:1};{:1}",
			self->s.number,
			(int)level.shadowLightInfo[i].shadowlight.lightType,
			level.shadowLightInfo[i].shadowlight.radius,
			level.shadowLightInfo[i].shadowlight.resolution,
			level.shadowLightInfo[i].shadowlight.intensity,
			level.shadowLightInfo[i].shadowlight.fadeStart,
			level.shadowLightInfo[i].shadowlight.fadeEnd,
			level.shadowLightInfo[i].shadowlight.lightStyle,
			level.shadowLightInfo[i].shadowlight.coneAngle,
			level.shadowLightInfo[i].shadowlight.coneDirection[0],
			level.shadowLightInfo[i].shadowlight.coneDirection[1],
			level.shadowLightInfo[i].shadowlight.coneDirection[2]).data());
	}
}

// fix an oversight in shadow light code that causes
// lights to be ordered wrong on return levels
// if the spawn functions are changed.
// this will work without changing the save/load code.
void G_LoadShadowLights() {
	for (size_t i = 0; i < level.shadowLightCount; i++) {
		const char* cstr = gi.get_configString(CS_SHADOWLIGHTS + i);
		const char* token = COM_ParseEx(&cstr, ";");

		if (token && *token) {
			level.shadowLightInfo[i].entity_number = atoi(token);

			token = COM_ParseEx(&cstr, ";");
			level.shadowLightInfo[i].shadowlight.lightType = (shadow_light_type_t)atoi(token);

			token = COM_ParseEx(&cstr, ";");
			level.shadowLightInfo[i].shadowlight.radius = atof(token);

			token = COM_ParseEx(&cstr, ";");
			level.shadowLightInfo[i].shadowlight.resolution = atoi(token);

			token = COM_ParseEx(&cstr, ";");
			level.shadowLightInfo[i].shadowlight.intensity = atof(token);

			token = COM_ParseEx(&cstr, ";");
			level.shadowLightInfo[i].shadowlight.fadeStart = atof(token);

			token = COM_ParseEx(&cstr, ";");
			level.shadowLightInfo[i].shadowlight.fadeEnd = atof(token);

			token = COM_ParseEx(&cstr, ";");
			level.shadowLightInfo[i].shadowlight.lightStyle = atoi(token);

			token = COM_ParseEx(&cstr, ";");
			level.shadowLightInfo[i].shadowlight.coneAngle = atof(token);

			token = COM_ParseEx(&cstr, ";");
			level.shadowLightInfo[i].shadowlight.coneDirection[0] = atof(token);

			token = COM_ParseEx(&cstr, ";");
			level.shadowLightInfo[i].shadowlight.coneDirection[1] = atof(token);

			token = COM_ParseEx(&cstr, ";");
			level.shadowLightInfo[i].shadowlight.coneDirection[2] = atof(token);
		}
	}
}
// ---------------------------------------------------------------------------------

static void setup_dynamic_light(gentity_t* self) {
	// [Sam-KEX] Shadow stuff
	if (st.sl.data.radius > 0) {
		self->s.renderFX = RF_CASTSHADOW;
		self->itemTarget = st.sl.lightStyleTarget;

		level.shadowLightInfo[level.shadowLightCount].entity_number = self->s.number;
		level.shadowLightInfo[level.shadowLightCount].shadowlight = st.sl.data;
		level.shadowLightCount++;

		self->mins[0] = self->mins[1] = self->mins[2] = 0;
		self->maxs[0] = self->maxs[1] = self->maxs[2] = 0;

		gi.linkEntity(self);
	}
}

static USE(dynamic_light_use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	self->svFlags ^= SVF_NOCLIENT;
}

void SP_dynamic_light(gentity_t* self) {
	setup_dynamic_light(self);

	if (self->targetName) {
		self->use = dynamic_light_use;
	}

	if (self->spawnFlags.has(SPAWNFLAG_LIGHT_START_OFF))
		self->svFlags ^= SVF_NOCLIENT;
}

void SP_light(gentity_t* self) {
	// no targeted lights in deathmatch, because they cause global messages
	if ((!self->targetName || (deathmatch->integer && !(self->spawnFlags.has(SPAWNFLAG_LIGHT_ALLOW_IN_DM)))) && st.sl.data.radius == 0) // [Sam-KEX]
	{
		FreeEntity(self);
		return;
	}

	if (self->style >= 32) {
		self->use = light_use;

		if (!self->style_on || !*self->style_on)
			self->style_on = "m";
		else if (*self->style_on >= '0' && *self->style_on <= '9')
			self->style_on = gi.get_configString(CS_LIGHTS + atoi(self->style_on));
		if (!self->style_off || !*self->style_off)
			self->style_off = "a";
		else if (*self->style_off >= '0' && *self->style_off <= '9')
			self->style_off = gi.get_configString(CS_LIGHTS + atoi(self->style_off));

		if (self->spawnFlags.has(SPAWNFLAG_LIGHT_START_OFF))
			gi.configString(CS_LIGHTS + self->style, self->style_off);
		else
			gi.configString(CS_LIGHTS + self->style, self->style_on);
	}

	setup_dynamic_light(self);
}

/*QUAKED misc_explobox (0 .5 .8) (-16 -16 0) (16 16 40) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Large exploding box.  You can override its mass (100),
health (80), and dmg (150).
*/

static TOUCH(barrel_touch) (gentity_t* self, gentity_t* other, const trace_t& tr, bool otherTouchingSelf) -> void {
	float  ratio;
	Vector3 v;

	if ((!other->groundEntity) || (other->groundEntity == self))
		return;
	else if (!otherTouchingSelf)
		return;

	ratio = (float)other->mass / (float)self->mass;
	v = self->s.origin - other->s.origin;
	M_walkmove(self, vectoyaw(v), 20 * ratio * gi.frameTimeSec);
}

static THINK(barrel_explode) (gentity_t* self) -> void {
	self->takeDamage = false;

	RadiusDamage(self, self->activator, (float)self->dmg, nullptr, (float)(self->dmg + 40), DamageFlags::Normal, ModID::Barrel);

	ThrowGibs(self, (1.5f * self->dmg / 200.f), {
		{ 2, "models/objects/debris1/tris.md2", GIB_METALLIC | GIB_DEBRIS },
		{ 4, "models/objects/debris3/tris.md2", GIB_METALLIC | GIB_DEBRIS },
		{ 8, "models/objects/debris2/tris.md2", GIB_METALLIC | GIB_DEBRIS }
		});

	if (deathmatch->integer && self->saved) {
		gentity_t* respawner = Spawn();
		respawner->think = Respawn_Think;
		respawner->nextThink = level.time + 1_min;
		respawner->saved = self->saved;
		self->saved = nullptr;
	}

	if (self->groundEntity)
		BecomeExplosion2(self);
	else
		BecomeExplosion1(self);
}

static THINK(barrel_burn) (gentity_t* self) -> void {
	if (level.time >= self->timeStamp)
		self->think = barrel_explode;

	self->s.effects |= EF_BARREL_EXPLODING;
	self->s.sound = gi.soundIndex("weapons/bfg__l1a.wav");
	self->nextThink = level.time + FRAME_TIME_S;
}

static DIE(barrel_delay) (gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, const Vector3& point, const MeansOfDeath& mod) -> void {
	// allow "dead" barrels waiting to explode to still receive knockback
	if (self->think == barrel_burn || self->think == barrel_explode)
		return;

	// allow big booms to immediately blow up barrels (rockets, rail, other explosions) because it feels good and powerful
	if (damage >= 90) {
		self->think = barrel_explode;
		self->activator = attacker;
	}
	else {
		self->timeStamp = level.time + 750_ms;
		self->think = barrel_burn;
		self->activator = attacker;
	}

}

static THINK(barrel_think) (gentity_t* self) -> void {
	// the think needs to be first since later stuff may override.
	self->think = barrel_think;
	self->nextThink = level.time + FRAME_TIME_S;

	M_CatagorizePosition(self, self->s.origin, self->waterLevel, self->waterType);
	self->flags |= FL_IMMUNE_SLIME;
	self->airFinished = level.time + 100_sec;
	M_WorldEffects(self);
}

static THINK(barrel_start) (gentity_t* self) -> void {
	M_droptofloor(self);
	self->think = barrel_think;
	self->nextThink = level.time + FRAME_TIME_S;
}

void SP_misc_explobox(gentity_t* self) {
	gi.modelIndex("models/objects/debris1/tris.md2");
	gi.modelIndex("models/objects/debris2/tris.md2");
	gi.modelIndex("models/objects/debris3/tris.md2");
	gi.soundIndex("weapons/bfg__l1a.wav");

	self->solid = SOLID_BBOX;
	self->moveType = MoveType::Step;

	self->model = "models/objects/barrels/tris.md2";
	self->s.modelIndex = gi.modelIndex(self->model);

	float scale = self->s.scale;
	if (!scale)
		scale = 1.0f;
	self->mins = { -16 * scale, -16 * scale, 0 };
	self->maxs = { 16 * scale, 16 * scale, 40 * scale };

	if (!self->mass)
		self->mass = 50;
	if (!self->health)
		self->health = 10;
	if (!self->dmg)
		self->dmg = 150;

	self->die = barrel_delay;
	self->takeDamage = true;
	self->flags |= FL_TRAP;

	self->touch = barrel_touch;

	self->think = barrel_start;
	self->nextThink = level.time + 20_hz;

	gi.linkEntity(self);
}

//
// miscellaneous specialty items
//

/*QUAKED misc_blackhole (1 .5 0) (-8 -8 -8) (8 8 8) AUTO_NOISE x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/objects/black/tris.md2"
*/

constexpr SpawnFlags SPAWNFLAG_BLACKHOLE_AUTO_NOISE = 1_spawnflag;

static USE(misc_blackhole_use) (gentity_t* ent, gentity_t* other, gentity_t* activator) -> void {
	/*
	gi.WriteByte (svc_temp_entity);
	gi.WriteByte (TE_BOSSTPORT);
	gi.WritePosition (ent->s.origin);
	gi.multicast (ent->s.origin, MULTICAST_PVS);
	*/
	FreeEntity(ent);
}

static THINK(misc_blackhole_think) (gentity_t* self) -> void {
	if (self->timeStamp <= level.time) {
		if (++self->s.frame >= 19)
			self->s.frame = 0;

		self->timeStamp = level.time + 10_hz;
	}

	if (self->spawnFlags.has(SPAWNFLAG_BLACKHOLE_AUTO_NOISE)) {
		self->s.angles[PITCH] += 50.0f * gi.frameTimeSec;
		self->s.angles[YAW] += 50.0f * gi.frameTimeSec;
	}

	self->nextThink = level.time + FRAME_TIME_MS;
}

void SP_misc_blackhole(gentity_t* ent) {
	ent->moveType = MoveType::None;
	ent->solid = SOLID_NOT;
	ent->mins = { -64, -64, 0 };
	ent->maxs = { 64, 64, 8 };
	ent->s.modelIndex = gi.modelIndex("models/objects/black/tris.md2");
	ent->s.renderFX = RF_TRANSLUCENT;
	ent->use = misc_blackhole_use;
	ent->think = misc_blackhole_think;
	ent->nextThink = level.time + 20_hz;

	if (ent->spawnFlags.has(SPAWNFLAG_BLACKHOLE_AUTO_NOISE)) {
		ent->s.sound = gi.soundIndex("world/blackhole.wav");
		ent->s.loopAttenuation = ATTN_NORM;
	}

	gi.linkEntity(ent);
}

/*QUAKED misc_eastertank (1 .5 0) (-32 -32 -16) (32 32 32) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
 */

static THINK(misc_eastertank_think) (gentity_t* self) -> void {
	if (++self->s.frame < 293)
		self->nextThink = level.time + 10_hz;
	else {
		self->s.frame = 254;
		self->nextThink = level.time + 10_hz;
	}
}

void SP_misc_eastertank(gentity_t* ent) {
	ent->moveType = MoveType::None;
	ent->solid = SOLID_BBOX;
	ent->mins = { -32, -32, -16 };
	ent->maxs = { 32, 32, 32 };
	ent->s.modelIndex = gi.modelIndex("models/monsters/tank/tris.md2");
	ent->s.frame = 254;
	ent->think = misc_eastertank_think;
	ent->nextThink = level.time + 20_hz;
	gi.linkEntity(ent);
}

/*QUAKED misc_easterchick (1 .5 0) (-32 -32 0) (32 32 32) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
 */

static THINK(misc_easterchick_think) (gentity_t* self) -> void {
	if (++self->s.frame < 247)
		self->nextThink = level.time + 10_hz;
	else {
		self->s.frame = 208;
		self->nextThink = level.time + 10_hz;
	}
}

void SP_misc_easterchick(gentity_t* ent) {
	ent->moveType = MoveType::None;
	ent->solid = SOLID_BBOX;
	ent->mins = { -32, -32, 0 };
	ent->maxs = { 32, 32, 32 };
	ent->s.modelIndex = gi.modelIndex("models/monsters/bitch/tris.md2");
	ent->s.frame = 208;
	ent->think = misc_easterchick_think;
	ent->nextThink = level.time + 20_hz;
	gi.linkEntity(ent);
}

/*QUAKED misc_easterchick2 (1 .5 0) (-32 -32 0) (32 32 32) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
 */

static THINK(misc_easterchick2_think) (gentity_t* self) -> void {
	if (++self->s.frame < 287)
		self->nextThink = level.time + 10_hz;
	else {
		self->s.frame = 248;
		self->nextThink = level.time + 10_hz;
	}
}

void SP_misc_easterchick2(gentity_t* ent) {
	ent->moveType = MoveType::None;
	ent->solid = SOLID_BBOX;
	ent->mins = { -32, -32, 0 };
	ent->maxs = { 32, 32, 32 };
	ent->s.modelIndex = gi.modelIndex("models/monsters/bitch/tris.md2");
	ent->s.frame = 248;
	ent->think = misc_easterchick2_think;
	ent->nextThink = level.time + 20_hz;
	gi.linkEntity(ent);
}

/*QUAKED monster_commander_body (1 .5 0) (-32 -32 0) (32 32 48) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Not really a monster, this is the Tank Commander's decapitated body.
There should be a item_commander_head that has this as it's target.
*/

static THINK(commander_body_think) (gentity_t* self) -> void {
	if (++self->s.frame < 24)
		self->nextThink = level.time + 10_hz;
	else
		self->nextThink = 0_ms;

	if (self->s.frame == 22)
		gi.sound(self, CHAN_BODY, gi.soundIndex("tank/thud.wav"), 1, ATTN_NORM, 0);
}

static USE(commander_body_use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	self->think = commander_body_think;
	self->nextThink = level.time + 10_hz;
	gi.sound(self, CHAN_BODY, gi.soundIndex("tank/pain.wav"), 1, ATTN_NORM, 0);
}

static THINK(commander_body_drop) (gentity_t* self) -> void {
	self->moveType = MoveType::Toss;
	self->s.origin[_Z] += 2;
}

void SP_monster_commander_body(gentity_t* self) {
	self->moveType = MoveType::None;
	self->solid = SOLID_BBOX;
	self->model = "models/monsters/commandr/tris.md2";
	self->s.modelIndex = gi.modelIndex(self->model);
	self->mins = { -32, -32, 0 };
	self->maxs = { 32, 32, 48 };
	self->use = commander_body_use;
	self->takeDamage = true;
	self->flags = FL_GODMODE;
	gi.linkEntity(self);

	gi.soundIndex("tank/thud.wav");
	gi.soundIndex("tank/pain.wav");

	self->think = commander_body_drop;
	self->nextThink = level.time + 50_hz;
}

/*QUAKED misc_banner (1 .5 0) (-4 -4 -4) (4 4 4) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
The origin is the bottom of the banner.
The banner is 128 tall.
model="models/objects/banner/tris.md2"
*/
static THINK(misc_banner_think) (gentity_t* ent) -> void {
	ent->s.frame = (ent->s.frame + 1) % 16;
	ent->nextThink = level.time + 10_hz;
}

void SP_misc_banner(gentity_t* ent) {
	ent->moveType = MoveType::None;
	ent->solid = SOLID_NOT;
	ent->s.modelIndex = gi.modelIndex("models/objects/banner/tris.md2");
	ent->s.frame = irandom(16);
	gi.linkEntity(ent);

	ent->think = misc_banner_think;
	ent->nextThink = level.time + 10_hz;
}

/*-----------------------------------------------------------------------*/
/*QUAKED misc_ctf_banner (1 .5 0) (-4 -64 0) (4 64 248) Team::Blue x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
The origin is the bottom of the banner.
The banner is 248 tall.
*/
static THINK(misc_ctf_banner_think) (gentity_t* ent) -> void {
	ent->s.frame = (ent->s.frame + 1) % 16;
	ent->nextThink = level.time + 10_hz;
}

constexpr SpawnFlags SPAWNFLAG_CTF_BANNER_BLUE = 1_spawnflag;

void SP_misc_ctf_banner(gentity_t* ent) {
	ent->moveType = MoveType::None;
	ent->solid = SOLID_NOT;
	ent->s.modelIndex = gi.modelIndex("models/ctf/banner/tris.md2");
	if (ent->spawnFlags.has(SPAWNFLAG_CTF_BANNER_BLUE)) // Team::Blue
		ent->s.skinNum = 1;

	ent->s.frame = irandom(16);
	gi.linkEntity(ent);

	ent->think = misc_ctf_banner_think;
	ent->nextThink = level.time + 10_hz;
}

/*QUAKED misc_ctf_small_banner (1 .5 0) (-4 -32 0) (4 32 124) Team::Blue x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
The origin is the bottom of the banner.
The banner is 124 tall.
*/
void SP_misc_ctf_small_banner(gentity_t* ent) {
	ent->moveType = MoveType::None;
	ent->solid = SOLID_NOT;
	ent->s.modelIndex = gi.modelIndex("models/ctf/banner/small.md2");
	if (ent->spawnFlags.has(SPAWNFLAG_CTF_BANNER_BLUE)) // Team::Blue
		ent->s.skinNum = 1;

	ent->s.frame = irandom(16);
	gi.linkEntity(ent);

	ent->think = misc_ctf_banner_think;
	ent->nextThink = level.time + 10_hz;
}

/*QUAKED misc_deadsoldier (1 .5 0) (-16 -16 0) (16 16 16) ON_BACK ON_STOMACH BACK_DECAP FETAL_POS SIT_DECAP IMPALED x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
This is the dead player model. Comes in 6 exciting different poses!
*/

constexpr SpawnFlags SPAWNFLAGS_DEADSOLDIER_ON_BACK = 1_spawnflag;
constexpr SpawnFlags SPAWNFLAGS_DEADSOLDIER_ON_STOMACH = 2_spawnflag;
constexpr SpawnFlags SPAWNFLAGS_DEADSOLDIER_BACK_DECAP = 4_spawnflag;
constexpr SpawnFlags SPAWNFLAGS_DEADSOLDIER_FETAL_POS = 8_spawnflag;
constexpr SpawnFlags SPAWNFLAGS_DEADSOLDIER_SIT_DECAP = 16_spawnflag;
constexpr SpawnFlags SPAWNFLAGS_DEADSOLDIER_IMPALED = 32_spawnflag;

static DIE(misc_deadsoldier_die) (gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, const Vector3& point, const MeansOfDeath& mod) -> void {
	if (self->health > -30)
		return;

	gi.sound(self, CHAN_BODY, gi.soundIndex("misc/udeath.wav"), 1, ATTN_NORM, 0);
	ThrowGibs(self, damage, {
		{ 4, "models/objects/gibs/sm_meat/tris.md2" },
		{ "models/objects/gibs/head2/tris.md2", GIB_HEAD }
		});
}

void SP_misc_deadsoldier(gentity_t* ent) {
	if (deathmatch->integer) { // auto-remove for deathmatch
		FreeEntity(ent);
		return;
	}

	ent->moveType = MoveType::None;
	ent->solid = SOLID_BBOX;
	ent->s.modelIndex = gi.modelIndex("models/deadbods/dude/tris.md2");

	// Defaults to frame 0
	if (ent->spawnFlags.has(SPAWNFLAGS_DEADSOLDIER_ON_STOMACH))
		ent->s.frame = 1;
	else if (ent->spawnFlags.has(SPAWNFLAGS_DEADSOLDIER_BACK_DECAP))
		ent->s.frame = 2;
	else if (ent->spawnFlags.has(SPAWNFLAGS_DEADSOLDIER_FETAL_POS))
		ent->s.frame = 3;
	else if (ent->spawnFlags.has(SPAWNFLAGS_DEADSOLDIER_SIT_DECAP))
		ent->s.frame = 4;
	else if (ent->spawnFlags.has(SPAWNFLAGS_DEADSOLDIER_IMPALED))
		ent->s.frame = 5;
	else if (ent->spawnFlags.has(SPAWNFLAGS_DEADSOLDIER_ON_BACK))
		ent->s.frame = 0;
	else
		ent->s.frame = 0;

	ent->mins = { -16, -16, 0 };
	ent->maxs = { 16, 16, 16 };
	ent->deadFlag = true;
	ent->takeDamage = true;
	// nb: SVF_MONSTER is here so it bleeds
	ent->svFlags |= SVF_MONSTER | SVF_DEADMONSTER;
	ent->die = misc_deadsoldier_die;
	ent->monsterInfo.aiFlags |= AI_GOOD_GUY | AI_DO_NOT_COUNT;

	gi.linkEntity(ent);
}

/*QUAKED misc_viper (1 .5 0) (-16 -16 0) (16 16 32) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
This is the Viper for the flyby bombing.
It is trigger_spawned, so you must have something use it for it to show up.
There must be a path for it to follow once it is activated.

"speed"		How fast the Viper should fly
*/

USE(misc_viper_use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	self->svFlags &= ~SVF_NOCLIENT;
	self->use = train_use;
	train_use(self, other, activator);
}

void SP_misc_viper(gentity_t* ent) {
	if (!ent->target) {
		gi.Com_PrintFmt("{} without a target\n", *ent);
		FreeEntity(ent);
		return;
	}

	if (!ent->speed)
		ent->speed = 300;

	ent->moveType = MoveType::Push;
	ent->solid = SOLID_NOT;
	ent->s.modelIndex = gi.modelIndex("models/ships/viper/tris.md2");
	ent->mins = { -16, -16, 0 };
	ent->maxs = { 16, 16, 32 };

	ent->think = func_train_find;
	ent->nextThink = level.time + 10_hz;
	ent->use = misc_viper_use;
	ent->svFlags |= SVF_NOCLIENT;
	ent->moveInfo.accel = ent->moveInfo.decel = ent->moveInfo.speed = ent->speed;

	gi.linkEntity(ent);
}

/*QUAKED misc_bigviper (1 .5 0) (-176 -120 -24) (176 120 72) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
This is a large stationary viper as seen in Paul's intro
*/
void SP_misc_bigviper(gentity_t* ent) {
	ent->moveType = MoveType::None;
	ent->solid = SOLID_BBOX;
	ent->mins = { -176, -120, -24 };
	ent->maxs = { 176, 120, 72 };
	ent->s.modelIndex = gi.modelIndex("models/ships/bigviper/tris.md2");
	gi.linkEntity(ent);
}

/*QUAKED misc_viper_bomb (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
"dmg"	how much boom should the bomb make?
*/
static TOUCH(misc_viper_bomb_touch) (gentity_t* self, gentity_t* other, const trace_t& tr, bool otherTouchingSelf) -> void {
	UseTargets(self, self->activator);

	self->s.origin[_Z] = self->absMin[2] + 1;
	RadiusDamage(self, self, (float)self->dmg, nullptr, (float)(self->dmg + 40), DamageFlags::Normal, ModID::Bomb);
	BecomeExplosion2(self);
}

static PRETHINK(misc_viper_bomb_prethink) (gentity_t* self) -> void {
	self->groundEntity = nullptr;

	float diff = (self->timeStamp - level.time).seconds();
	if (diff < -1.0f)
		diff = -1.0f;

	Vector3 v = self->moveInfo.dir * (1.0f + diff);
	v[2] = diff;

	diff = self->s.angles[ROLL];
	self->s.angles = VectorToAngles(v);
	self->s.angles[ROLL] = diff + 10;
}

static USE(misc_viper_bomb_use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	gentity_t* viper;

	self->solid = SOLID_BBOX;
	self->svFlags &= ~SVF_NOCLIENT;
	self->s.effects |= EF_ROCKET;
	self->use = nullptr;
	self->moveType = MoveType::Toss;
	self->preThink = misc_viper_bomb_prethink;
	self->touch = misc_viper_bomb_touch;
	self->activator = activator;

	viper = G_FindByString<&gentity_t::className>(nullptr, "misc_viper");
	self->velocity = viper->moveInfo.dir * viper->moveInfo.speed;

	self->timeStamp = level.time;
	self->moveInfo.dir = viper->moveInfo.dir;
}

void SP_misc_viper_bomb(gentity_t* self) {
	self->moveType = MoveType::None;
	self->solid = SOLID_NOT;
	self->mins = { -8, -8, -8 };
	self->maxs = { 8, 8, 8 };

	self->s.modelIndex = gi.modelIndex("models/objects/bomb/tris.md2");

	if (!self->dmg)
		self->dmg = 1000;

	self->use = misc_viper_bomb_use;
	self->svFlags |= SVF_NOCLIENT;

	gi.linkEntity(self);
}

/*QUAKED misc_strogg_ship (1 .5 0) (-16 -16 0) (16 16 32) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
This is a Storgg ship for the flybys.
It is trigger_spawned, so you must have something use it for it to show up.
There must be a path for it to follow once it is activated.

"speed"		How fast it should fly
*/
USE(misc_strogg_ship_use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	self->svFlags &= ~SVF_NOCLIENT;
	self->use = train_use;
	train_use(self, other, activator);
}

void SP_misc_strogg_ship(gentity_t* ent) {
	if (!ent->target) {
		gi.Com_PrintFmt("{} without a target\n", *ent);
		FreeEntity(ent);
		return;
	}

	if (!ent->speed)
		ent->speed = 300;

	ent->moveType = MoveType::Push;
	ent->solid = SOLID_NOT;
	ent->s.modelIndex = gi.modelIndex("models/ships/strogg1/tris.md2");
	ent->mins = { -16, -16, 0 };
	ent->maxs = { 16, 16, 32 };

	ent->think = func_train_find;
	ent->nextThink = level.time + 10_hz;
	ent->use = misc_strogg_ship_use;
	ent->svFlags |= SVF_NOCLIENT;
	ent->moveInfo.accel = ent->moveInfo.decel = ent->moveInfo.speed = ent->speed;

	gi.linkEntity(ent);
}

/*QUAKED misc_satellite_dish (1 .5 0) (-64 -64 0) (64 64 128) x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/objects/satellite/tris.md2"
*/
static THINK(misc_satellite_dish_think) (gentity_t* self) -> void {
	self->s.frame++;
	if (self->s.frame < 38)
		self->nextThink = level.time + 10_hz;
}

static USE(misc_satellite_dish_use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	self->s.frame = 0;
	self->think = misc_satellite_dish_think;
	self->nextThink = level.time + 10_hz;
}

void SP_misc_satellite_dish(gentity_t* ent) {
	ent->moveType = MoveType::None;
	ent->solid = SOLID_BBOX;
	ent->mins = { -64, -64, 0 };
	ent->maxs = { 64, 64, 128 };
	ent->s.modelIndex = gi.modelIndex("models/objects/satellite/tris.md2");
	ent->use = misc_satellite_dish_use;
	gi.linkEntity(ent);
}

/*QUAKED light_mine1 (0 1 0) (-2 -2 -12) (2 2 12) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
 */
void SP_light_mine1(gentity_t* ent) {
	ent->moveType = MoveType::None;
	ent->solid = SOLID_NOT;
	ent->svFlags = SVF_DEADMONSTER;
	ent->s.modelIndex = gi.modelIndex("models/objects/minelite/light1/tris.md2");
	gi.linkEntity(ent);
}

/*QUAKED light_mine2 (0 1 0) (-2 -2 -12) (2 2 12) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
 */
void SP_light_mine2(gentity_t* ent) {
	ent->moveType = MoveType::None;
	ent->solid = SOLID_NOT;
	ent->svFlags = SVF_DEADMONSTER;
	ent->s.modelIndex = gi.modelIndex("models/objects/minelite/light2/tris.md2");
	gi.linkEntity(ent);
}

/*QUAKED misc_gib_arm (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Intended for use with the target_spawner
*/
void SP_misc_gib_arm(gentity_t* ent) {
	gi.setModel(ent, "models/objects/gibs/arm/tris.md2");
	ent->solid = SOLID_NOT;
	ent->s.effects |= EF_GIB;
	ent->takeDamage = true;
	ent->die = gib_die;
	ent->moveType = MoveType::Toss;
	ent->deadFlag = true;
	ent->aVelocity[_X] = frandom(200);
	ent->aVelocity[_Y] = frandom(200);
	ent->aVelocity[_Z] = frandom(200);
	ent->think = FreeEntity;
	ent->nextThink = level.time + 10_sec;
	gi.linkEntity(ent);
}

/*QUAKED misc_gib_leg (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Intended for use with the target_spawner
*/
void SP_misc_gib_leg(gentity_t* ent) {
	gi.setModel(ent, "models/objects/gibs/leg/tris.md2");
	ent->solid = SOLID_NOT;
	ent->s.effects |= EF_GIB;
	ent->takeDamage = true;
	ent->die = gib_die;
	ent->moveType = MoveType::Toss;
	ent->deadFlag = true;
	ent->aVelocity[0] = frandom(200);
	ent->aVelocity[1] = frandom(200);
	ent->aVelocity[2] = frandom(200);
	ent->think = FreeEntity;
	ent->nextThink = level.time + 10_sec;
	gi.linkEntity(ent);
}

/*QUAKED misc_gib_head (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Intended for use with the target_spawner
*/
void SP_misc_gib_head(gentity_t* ent) {
	gi.setModel(ent, "models/objects/gibs/head.md2");
	ent->solid = SOLID_NOT;
	ent->s.effects |= EF_GIB;
	ent->takeDamage = true;
	ent->die = gib_die;
	ent->moveType = MoveType::Toss;
	ent->deadFlag = true;
	ent->aVelocity[_X] = frandom(200);
	ent->aVelocity[_Y] = frandom(200);
	ent->aVelocity[_Z] = frandom(200);
	ent->think = FreeEntity;
	ent->nextThink = level.time + 10_sec;
	gi.linkEntity(ent);
}

//=====================================================

/*QUAKED target_character (0 0 1) ? x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
used with target_string (must be on same "team")
"count" is position in the string (starts at 1)
*/

void SP_target_character(gentity_t* self) {
	self->moveType = MoveType::Push;
	gi.setModel(self, self->model);
	self->solid = SOLID_BSP;
	self->s.frame = 12;
	gi.linkEntity(self);
	return;
}

/*QUAKED target_string (0 0 1) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
 */

static USE(target_string_use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	gentity_t* e;
	int		 n;
	size_t	 l;
	char	 c;

	l = strlen(self->message);
	for (e = self->teamMaster; e; e = e->teamChain) {
		if (!e->count)
			continue;
		n = e->count - 1;
		if (n > l) {
			e->s.frame = 12;
			continue;
		}

		c = self->message[n];
		if (c >= '0' && c <= '9')
			e->s.frame = c - '0';
		else if (c == '-')
			e->s.frame = 10;
		else if (c == ':')
			e->s.frame = 11;
		else
			e->s.frame = 12;
	}
}

void SP_target_string(gentity_t* self) {
	if (!self->message)
		self->message = "";
	self->use = target_string_use;
}

//=================================================================================

constexpr SpawnFlags SPAWNFLAG_TELEPORTER_NO_SOUND = 1_spawnflag;
constexpr SpawnFlags SPAWNFLAG_TELEPORTER_NO_TELEPORT_EFFECT = 2_spawnflag;

static TOUCH(teleporter_touch) (gentity_t* self, gentity_t* other, const trace_t& tr, bool otherTouchingSelf) -> void {
	if (!other->client)
		return;

	gentity_t* dest = G_FindByString<&gentity_t::targetName>(nullptr, self->target);
	if (!dest) {
		gi.Com_PrintFmt("{}: Couldn't find destination, removing.\n", *self);
		FreeEntity(self);
		return;
	}

	TeleportPlayer(other, dest->s.origin, dest->s.angles);

	bool fx = !self->spawnFlags.has(SPAWNFLAG_TELEPORTER_NO_TELEPORT_EFFECT);

	// draw the teleport splash at source and on the player
	if (ClientIsPlaying(other->client)) {
		self->owner->s.event = fx ? EV_PLAYER_TELEPORT : EV_OTHER_TELEPORT;
		other->s.event = fx ? EV_PLAYER_TELEPORT : EV_OTHER_TELEPORT;
	}
}

/*QUAKED misc_teleporter (1 0 0) (-32 -32 -24) (32 32 -16) NO_SOUND NO_TELEPORT_EFFECT N64_EFFECT x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Stepping onto this disc will teleport players to the targeted misc_teleporter_dest object.

"mins" and "maxs" can be used to specify the size of the touch trigger.
If not specified, a default size of (-8 -8 8) to (8 8 24) will be used.

"target" is the name of the misc_teleporter_dest to teleport to.

NO_SOUND : If set, the teleporter will not play the teleport sound when present.
NO_TELEPORT_EFFECT : If set, the teleporter will not play the teleport effect when used.
N64_EFFECT : If set, the teleporter will use the N64 teleport effect.
*/
constexpr SpawnFlags SPAWNFLAG_TEMEPORTER_N64_EFFECT = 4_spawnflag;

void SP_misc_teleporter(gentity_t* ent) {
	Vector3		mins, maxs;
	bool		createSpawnPad = true;

	if (ent->target) {
		if (st.was_key_specified("mins")) {
			mins = ent->mins;
		}
		else mins = { -8, -8, 8 };
		if (st.was_key_specified("maxs")) {
			maxs = ent->maxs;
			if (mins)
				createSpawnPad = false;
		}
		else maxs = { 8, 8, 24 };
	}

	if (createSpawnPad) {
		//gi.Com_PrintFmt("{}: DMSPOT\n", *ent);
		gi.setModel(ent, "models/objects/dmspot/tris.md2");
		ent->s.skinNum = 1;
		if (level.isN64 || ent->spawnFlags.has(SPAWNFLAG_TEMEPORTER_N64_EFFECT))
			ent->s.effects = EF_TELEPORTER2;
		else
			ent->s.effects = EF_TELEPORTER;
		if (!(ent->spawnFlags & SPAWNFLAG_TELEPORTER_NO_SOUND))
			ent->s.sound = gi.soundIndex("world/amb10.wav");
		ent->solid = SOLID_BBOX;

		ent->mins = { -32, -32, -24 };
		ent->maxs = { 32, 32, -16 };

		gi.linkEntity(ent);
	}

	// N64 has some of these for visual effects
	if (!ent->target)
		return;

	gentity_t* trig = Spawn();
	trig->className = "teleporter_touch";
	trig->touch = teleporter_touch;
	trig->solid = SOLID_TRIGGER;
	trig->target = ent->target;
	trig->owner = ent;
	trig->s.origin = ent->s.origin;
	trig->mins = mins;
	trig->maxs = maxs;

	gi.linkEntity(trig);
}

/*QUAKED misc_teleporter_dest (1 0 0) (-32 -32 -24) (32 32 -16) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Point teleporters to these.
*/

void SP_misc_teleporter_dest(gentity_t* ent) {
	// Paril-KEX N64 doesn't display these
	if (level.isN64)
		return;

	CreateSpawnPad(ent);
}

/*QUAKED misc_flare (1.0 1.0 0.0) (-32 -32 -32) (32 32 32) RED GREEN BLUE LOCK_ANGLE x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Creates a flare seen in the N64 version.

"radius"	How large the flare should be (default 64)
"image"		The image to use for the flare (default "gfx/flare")
"fade_start_dist"	How far away the flare should start fading (default 512)
"fade_end_dist"	How far away the flare should be completely faded (default 1024)

If targeted, the flare will toggle on and off when used.

LOCK_ANGLE : If set, the flare will not rotate and will always face the player.
*/

static constexpr SpawnFlags SPAWNFLAG_FLARE_RED = 1_spawnflag;
static constexpr SpawnFlags SPAWNFLAG_FLARE_GREEN = 2_spawnflag;
static constexpr SpawnFlags SPAWNFLAG_FLARE_BLUE = 4_spawnflag;
static constexpr SpawnFlags SPAWNFLAG_FLARE_LOCK_ANGLE = 8_spawnflag;

static USE(misc_flare_use) (gentity_t* ent, gentity_t* other, gentity_t* activator) -> void {
	ent->svFlags ^= SVF_NOCLIENT;
	gi.linkEntity(ent);
}

void SP_misc_flare(gentity_t* ent) {
	ent->s.modelIndex = 1;
	ent->s.renderFX = RF_FLARE;
	ent->solid = SOLID_NOT;
	ent->s.scale = st.radius;

	if (ent->spawnFlags.has(SPAWNFLAG_FLARE_RED))
		ent->s.renderFX |= RF_SHELL_RED;

	if (ent->spawnFlags.has(SPAWNFLAG_FLARE_GREEN))
		ent->s.renderFX |= RF_SHELL_GREEN;

	if (ent->spawnFlags.has(SPAWNFLAG_FLARE_BLUE))
		ent->s.renderFX |= RF_SHELL_BLUE;

	if (ent->spawnFlags.has(SPAWNFLAG_FLARE_LOCK_ANGLE))
		ent->s.renderFX |= RF_FLARE_LOCK_ANGLE;

	if (st.image && *st.image) {
		ent->s.renderFX |= RF_CUSTOMSKIN;
		ent->s.frame = gi.imageIndex(st.image);
	}

	ent->mins = { -32, -32, -32 };
	ent->maxs = { 32, 32, 32 };

	ent->s.modelIndex2 = st.fade_start_dist;
	ent->s.modelIndex3 = st.fade_end_dist;

	if (ent->targetName)
		ent->use = misc_flare_use;

	gi.linkEntity(ent);
}

static THINK(misc_hologram_think) (gentity_t* ent) -> void {
	ent->s.angles[YAW] += 100 * gi.frameTimeSec;
	ent->nextThink = level.time + FRAME_TIME_MS;
	ent->s.alpha = frandom(0.2f, 0.6f);
}

/*QUAKED misc_hologram (1.0 1.0 0.0) (-16 -16 0) (16 16 32) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Ship hologram seen in the N64 version.
*/
void SP_misc_hologram(gentity_t* ent) {
	ent->solid = SOLID_NOT;
	ent->s.modelIndex = gi.modelIndex("models/ships/strogg1/tris.md2");
	ent->mins = { -16, -16, 0 };
	ent->maxs = { 16, 16, 32 };
	ent->s.effects = EF_HOLOGRAM;
	ent->think = misc_hologram_think;
	ent->nextThink = level.time + FRAME_TIME_MS;
	ent->s.alpha = frandom(0.2f, 0.6f);
	ent->s.scale = 0.75f;
	gi.linkEntity(ent);
}


/*QUAKED misc_fireball (0 .5 .8) (-8 -8 -8) (8 8 8) NO_EXPLODE x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Lava Balls. Shamelessly copied from Quake 1,
like N64 guys probably did too.

"dmg"		How much damage it does on impact (default 20 in SP, 5 in MP)

NO_EXPLODE : If set, the fireball will not explode on impact.
*/

constexpr SpawnFlags SPAWNFLAG_LAVABALL_NO_EXPLODE = 1_spawnflag;

static TOUCH(fire_touch) (gentity_t* self, gentity_t* other, const trace_t& tr, bool otherTouchingSelf) -> void {
	if (self->spawnFlags.has(SPAWNFLAG_LAVABALL_NO_EXPLODE)) {
		FreeEntity(self);
		return;
	}

	if (other->takeDamage)
		Damage(other, self, self, vec3_origin, self->s.origin, vec3_origin, self->dmg, 0, DamageFlags::Normal, ModID::Explosives);

	if (gi.pointContents(self->s.origin) & CONTENTS_LAVA)
		FreeEntity(self);
	else
		BecomeExplosion1(self);
}

static THINK(fire_fly) (gentity_t* self) -> void {
	gentity_t* fireball = Spawn();
	fireball->s.effects = EF_FIREBALL;
	fireball->s.renderFX = RF_MINLIGHT;
	fireball->solid = SOLID_BBOX;
	fireball->moveType = MoveType::Toss;
	fireball->clipMask = MASK_SHOT;
	fireball->velocity[_X] = crandom() * 50;
	fireball->velocity[_Y] = crandom() * 50;
	fireball->aVelocity = { crandom() * 360, crandom() * 360, crandom() * 360 };
	fireball->velocity[_Z] = (self->speed * 1.75f) + (frandom() * 200);
	fireball->className = "fireball";
	gi.setModel(fireball, "models/objects/gibs/sm_meat/tris.md2");
	fireball->s.origin = self->s.origin;
	fireball->nextThink = level.time + 5_sec;
	fireball->think = FreeEntity;
	fireball->touch = fire_touch;
	if (!fireball->dmg)
		fireball->dmg = deathmatch->integer ? 5 : 20;
	fireball->spawnFlags = self->spawnFlags;
	gi.linkEntity(fireball);
	self->nextThink = level.time + random_time(5_sec);
}

void SP_misc_lavaball(gentity_t* self) {
	self->className = "fireball";
	self->nextThink = level.time + random_time(5_sec);
	self->think = fire_fly;
	if (!self->speed)
		self->speed = 185;
}

/*QUAKED info_landmark (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
This is a landmark for the map, used to mark a specific point in the world.
It is not a solid entity and does not interact with the game world.
It is used for map navigation and scripting purposes.
*/

void SP_info_landmark(gentity_t* self) {
	self->absMin = self->s.origin;
	self->absMax = self->s.origin;
}

/*
================
SPAWNFLAGS for info_world_text
================
*/
constexpr SpawnFlags SPAWNFLAG_WORLD_TEXT_START_OFF = 1_spawnflag;
constexpr SpawnFlags SPAWNFLAG_WORLD_TEXT_TRIGGER_ONCE = 2_spawnflag;
constexpr SpawnFlags SPAWNFLAG_WORLD_TEXT_REMOVE_ON_TRIGGER = 4_spawnflag;
constexpr SpawnFlags SPAWNFLAG_WORLD_TEXT_LEADER_BOARD = 8_spawnflag;

/*
================
info_world_text_use
================
*/
static USE(info_world_text_use)(gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	if (!self->activator) {
		self->activator = activator;
		self->think(self);
	}
	else {
		self->nextThink = 0_ms;
		self->activator = nullptr;
	}

	if (self->spawnFlags.has(SPAWNFLAG_WORLD_TEXT_TRIGGER_ONCE)) {
		self->use = nullptr;
	}

	if (self->target) {
		if (gentity_t* target = PickTarget(self->target); target && target->inUse && target->use) {
			target->use(target, self, self);
		}
	}

	if (self->spawnFlags.has(SPAWNFLAG_WORLD_TEXT_REMOVE_ON_TRIGGER)) {
		FreeEntity(self);
	}
}

/*
================
info_world_text_think
================
*/
static THINK(info_world_text_think)(gentity_t* self) -> void {
	static constexpr std::array<rgba_t, 8> kColors = {
		rgba_white, rgba_red, rgba_blue, rgba_green,
		rgba_yellow, rgba_black, rgba_cyan, rgba_orange
	};

	rgba_t color = rgba_white;
	if (self->sounds >= 0 && self->sounds < static_cast<int>(kColors.size())) {
		color = kColors[self->sounds];
	}
	else {
		gi.Com_PrintFmt("{}: invalid color\n", *self);
	}

	std::string textBuf;
	std::string_view text = self->message;

	if (deathmatch->integer && self->spawnFlags.has(SPAWNFLAG_WORLD_TEXT_LEADER_BOARD)) {
		const gentity_t* leader = &g_entities[level.sortedClients[0] + 1];

		switch (level.matchState) {
			using enum MatchState;
		case Warmup_ReadyUp:
			textBuf = std::format("Welcome to {}\nKindly ready the fuck up...", worr::version::kGameTitle);
			text = textBuf;
			break;

		case Warmup_Default:
			textBuf = std::format("Welcome to {}", worr::version::kGameTitle);
			text = textBuf;
			break;

		default:
			if (leader && leader->client && level.match.totalDeaths > 0 && leader->client->resp.score > 0) {
				textBuf = std::format("{} is in the lead\nwith a score of {}",
					leader->client->sess.netName, leader->client->resp.score);
				text = textBuf;
			}
			break;
		}
	}

	if (self->s.angles[YAW] == -3.0f) {
		gi.Draw_OrientedWorldText(self->s.origin, text.data(), color, self->size[2], FRAME_TIME_MS.seconds(), true);
	}
	else {
		Vector3 textAngle = { 0.0f, anglemod(self->s.angles[YAW]) + 180.0f, 0.0f };
		if (textAngle[YAW] > 360.0f)
			textAngle[YAW] -= 360.0f;

		gi.Draw_StaticWorldText(self->s.origin, textAngle, text.data(), color, self->size[2], FRAME_TIME_MS.seconds(), true);
	}

	self->nextThink = level.time + FRAME_TIME_MS;
}

/*QUAKED info_world_text (1.0 1.0 0.0) (-16 -16 0) (16 16 32) START_OFF TRIGGER_ONCE REMOVE_ON_TRIGGER LEADER x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Designer placed in world text for debugging.

"message"	- The text to display, can be a string or a key/value pair
"radius"	- The size of the text, defaults to 0.2
"sounds"	- The color of the text, 0-7, defaults to white
"target"	- If set, will trigger the target when the text is displayed
*/
void SP_info_world_text(gentity_t* self) {
	if (self->message == nullptr) {
		if (!self->spawnFlags.has(SPAWNFLAG_WORLD_TEXT_LEADER_BOARD)) {
			gi.Com_PrintFmt("{}: no message\n", *self);
			FreeEntity(self);
			return;
		}
	} // not much point without something to print...

	self->think = info_world_text_think;
	self->use = info_world_text_use;
	self->size[2] = st.radius ? st.radius : 0.2f;

	if (!self->spawnFlags.has(SPAWNFLAG_WORLD_TEXT_START_OFF)) {
		self->nextThink = level.time + FRAME_TIME_MS;
		self->activator = self;
	}
}

#include "../monsters/m_player.hpp"

static USE(misc_player_mannequin_use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	self->monsterInfo.aiFlags |= AI_TARGET_ANGER;
	self->enemy = activator;

	switch (self->count) {
	case GESTURE_FLIP_OFF:
		self->s.frame = FRAME_flip01;
		self->monsterInfo.nextFrame = FRAME_flip12;
		break;

	case GESTURE_SALUTE:
		self->s.frame = FRAME_salute01;
		self->monsterInfo.nextFrame = FRAME_salute11;
		break;

	case GESTURE_TAUNT:
		self->s.frame = FRAME_taunt01;
		self->monsterInfo.nextFrame = FRAME_taunt17;
		break;

	case GESTURE_WAVE:
		self->s.frame = FRAME_wave01;
		self->monsterInfo.nextFrame = FRAME_wave11;
		break;

	case GESTURE_POINT:
		self->s.frame = FRAME_point01;
		self->monsterInfo.nextFrame = FRAME_point12;
		break;
	}
}

static THINK(misc_player_mannequin_think) (gentity_t* self) -> void {
	if (self->teleportTime <= level.time) {
		self->s.frame++;

		if ((self->monsterInfo.aiFlags & AI_TARGET_ANGER) == 0) {
			if (self->s.frame > FRAME_stand40) {
				self->s.frame = FRAME_stand01;
			}
		}
		else {
			if (self->s.frame > self->monsterInfo.nextFrame) {
				self->s.frame = FRAME_stand01;
				self->monsterInfo.aiFlags &= ~AI_TARGET_ANGER;
				self->enemy = nullptr;
			}
		}

		self->teleportTime = level.time + 10_hz;
	}

	if (self->enemy != nullptr) {
		const Vector3 vec = (self->enemy->s.origin - self->s.origin);
		self->ideal_yaw = vectoyaw(vec);
		M_ChangeYaw(self);
	}

	self->nextThink = level.time + FRAME_TIME_MS;
}

static void SetupMannequinModel(gentity_t* self, const int32_t modelType, const char* weapon, const char* skin) {
	const char* modelName = nullptr;
	const char* defaultSkin = nullptr;

	switch (modelType) {
	case 1: {
		self->s.skinNum = (MAX_CLIENTS - 1);
		modelName = "female";
		defaultSkin = "venus";
		break;
	}

	case 2: {
		self->s.skinNum = (MAX_CLIENTS - 2);
		modelName = "male";
		defaultSkin = "rampage";
		break;
	}

	case 3: {
		self->s.skinNum = (MAX_CLIENTS - 3);
		modelName = "cyborg";
		defaultSkin = "oni911";
		break;
	}

	default: {
		self->s.skinNum = (MAX_CLIENTS - 1);
		modelName = "female";
		defaultSkin = "venus";
		break;
	}
	}

	if (modelName != nullptr) {
		self->model = G_Fmt("players/{}/tris.md2", modelName).data();

		const char* weaponName = nullptr;
		if (weapon != nullptr) {
			weaponName = G_Fmt("players/{}/{}.md2", modelName, weapon).data();
		}
		else {
			weaponName = G_Fmt("players/{}/{}.md2", modelName, "w_hyperblaster").data();
		}
		self->s.modelIndex2 = gi.modelIndex(weaponName);

		const char* skinName = nullptr;
		if (skin != nullptr) {
			skinName = G_Fmt("mannequin\\{}/{}", modelName, skin).data();
		}
		else {
			skinName = G_Fmt("mannequin\\{}/{}", modelName, defaultSkin).data();
		}
		gi.configString(CS_PLAYERSKINS + self->s.skinNum, skinName);
	}
}

/*QUAKED misc_player_mannequin (1.0 1.0 0.0) (-32 -32 -32) (32 32 32) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Creates a player mannequin that stands around.

NOTE: this is currently very limited, and only allows one unique model
from each of the three player model types.

 "distance"		- Sets the type of gesture mannequin when use when triggered
 "height"		- Sets the type of model to use ( valid numbers: 1 - 3 )
 "goals"		- Name of the weapon to use.
 "image"		- Name of the player skin to use.
 "radius"		- How much to scale the model in-game
*/
void SP_misc_player_mannequin(gentity_t* self) {
	self->moveType = MoveType::None;
	self->solid = SOLID_BBOX;
	if (!st.was_key_specified("effects"))
		self->s.effects = EF_NONE;
	if (!st.was_key_specified("renderFX"))
		self->s.renderFX = RF_MINLIGHT;
	self->mins = { -16, -16, -24 };
	self->maxs = { 16, 16, 32 };
	self->yawSpeed = 30;
	self->ideal_yaw = 0;
	self->teleportTime = level.time + 10_hz;
	self->s.modelIndex = MODELINDEX_PLAYER;
	self->count = st.distance;

	SetupMannequinModel(self, st.height, st.goals, st.image);

	self->s.scale = 1.0f;
	if (ai_model_scale->value > 0.0f) {
		self->s.scale = ai_model_scale->value;
	}
	else if (st.radius > 0.0f) {
		self->s.scale = st.radius;
	}

	self->mins *= self->s.scale;
	self->maxs *= self->s.scale;

	self->think = misc_player_mannequin_think;
	self->nextThink = level.time + FRAME_TIME_MS;

	if (self->targetName) {
		self->use = misc_player_mannequin_use;
	}

	gi.linkEntity(self);
}

/*QUAKED misc_model (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
This entity is used to spawn a model in the world.
*/
void SP_misc_model(gentity_t* ent) {
	gi.setModel(ent, ent->model);
	gi.linkEntity(ent);
}


/*QUAKED misc_crashviper (1 .5 0) (-176 -120 -24) (176 120 72) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
A large viper about to crash.
*/
void SP_misc_crashviper(gentity_t* ent) {
	if (!ent->target) {
		gi.Com_PrintFmt("{}: no target\n", *ent);
		FreeEntity(ent);
		return;
	}

	if (!ent->speed)
		ent->speed = 300;

	ent->moveType = MoveType::Push;
	ent->solid = SOLID_NOT;
	ent->s.modelIndex = gi.modelIndex("models/ships/bigviper/tris.md2");
	ent->mins = { -16, -16, 0 };
	ent->maxs = { 16, 16, 32 };

	ent->think = func_train_find;
	ent->nextThink = level.time + 10_hz;
	ent->use = misc_viper_use;
	ent->svFlags |= SVF_NOCLIENT;
	ent->moveInfo.accel = ent->moveInfo.decel = ent->moveInfo.speed = ent->speed;

	gi.linkEntity(ent);
}

/*QUAKED misc_viper_missile (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
"dmg":	how much boom should the bomb make? the default value is 250
*/

static USE(misc_viper_missile_use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	Vector3 forward, right, up;

	AngleVectors(self->s.angles, forward, right, up);

	self->enemy = G_FindByString<&gentity_t::targetName>(nullptr, self->target);

	Vector3 vec = self->enemy->s.origin;
	Vector3 start = self->s.origin;
	Vector3 dir = vec - start;
	dir.normalize();

	monster_fire_rocket(self, start, dir, self->dmg, 500, MZ2_CHICK_ROCKET_1);

	self->nextThink = level.time + 10_hz;
	self->think = FreeEntity;
}

void SP_misc_viper_missile(gentity_t* self) {
	self->moveType = MoveType::None;
	self->solid = SOLID_NOT;
	self->mins = { -8, -8, -8 };
	self->maxs = { 8, 8, 8 };

	if (!self->dmg)
		self->dmg = 250;

	self->s.modelIndex = gi.modelIndex("models/objects/bomb/tris.md2");

	self->use = misc_viper_missile_use;
	self->svFlags |= SVF_NOCLIENT;

	gi.linkEntity(self);
}

/*QUAKED misc_transport (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Maxx's transport at end of game

"speed": How fast the transport moves. Default is 300.
*/
void SP_misc_transport(gentity_t* ent) {
	if (!ent->target) {
		gi.Com_PrintFmt("{}: no target\n", *ent);
		FreeEntity(ent);
		return;
	}

	if (!ent->speed)
		ent->speed = 300;

	ent->moveType = MoveType::Push;
	ent->solid = SOLID_NOT;
	ent->s.modelIndex = gi.modelIndex("models/objects/ship/tris.md2");

	ent->mins = { -16, -16, 0 };
	ent->maxs = { 16, 16, 32 };

	ent->think = func_train_find;
	ent->nextThink = level.time + 10_hz;
	ent->use = misc_strogg_ship_use;
	ent->svFlags |= SVF_NOCLIENT;
	ent->moveInfo.accel = ent->moveInfo.decel = ent->moveInfo.speed = ent->speed;

	if (!(ent->spawnFlags & SPAWNFLAG_TRAIN_START_ON))
		ent->spawnFlags |= SPAWNFLAG_TRAIN_START_ON;

	gi.linkEntity(ent);
}

/*QUAKED misc_amb4 (1 0 0) (-16 -16 -16) (16 16 16) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Mal's amb4 loop entity
*/
static cached_soundIndex amb4sound;

static THINK(amb4_think) (gentity_t* ent) -> void {
	ent->nextThink = level.time + 2.7_sec;
	gi.sound(ent, CHAN_VOICE, amb4sound, 1, ATTN_NONE, 0);
}

void SP_misc_amb4(gentity_t* ent) {
	ent->think = amb4_think;
	ent->nextThink = level.time + 1_sec;
	amb4sound.assign("world/amb4.wav");
	gi.linkEntity(ent);
}

/*QUAKED misc_nuke (1 0 0) (-16 -16 -16) (16 16 16) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
 */
extern void target_killplayers_use(gentity_t* self, gentity_t* other, gentity_t* activator);

static THINK(misc_nuke_think) (gentity_t* self) -> void {
	Nuke_Explode(self);
}

static USE(misc_nuke_use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	gentity_t* nuke = Spawn();
	nuke->s.origin = self->s.origin;
	nuke->clipMask = MASK_PROJECTILE;
	nuke->solid = SOLID_NOT;
	nuke->mins = { -1, -1, 1 };
	nuke->maxs = { 1, 1, 1 };
	nuke->owner = self;
	nuke->teamMaster = self;
	nuke->nextThink = level.time + FRAME_TIME_S;
	nuke->dmg = 800;
	nuke->splashRadius = 8192;
	nuke->think = misc_nuke_think;
}

void SP_misc_nuke(gentity_t* ent) {
	ent->use = misc_nuke_use;
}

/*QUAKED misc_nuke_core (1 0 0) (-16 -16 -16) (16 16 16) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Toggles visible/not visible. Starts visible.
*/
static USE(misc_nuke_core_use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	if (self->svFlags & SVF_NOCLIENT)
		self->svFlags &= ~SVF_NOCLIENT;
	else
		self->svFlags |= SVF_NOCLIENT;
}

void SP_misc_nuke_core(gentity_t* ent) {
	gi.setModel(ent, "models/objects/core/tris.md2");
	gi.linkEntity(ent);

	ent->use = misc_nuke_core_use;
}

/*QUAKED misc_camera (1 0 0 ) (-8 -8 -8) (8 8 8) FREEZE
Cutscene camera type thing.

FREEZE: freezes player's movement when viewing through camera

"angles" - sets the starting view dir, target overrides this
"wait" - time to view through this camera.  Overridden if the
		 camera encounters a path_corner with delay -1.  A
		 wait of -1 means the camera stays on indefinitely.  Default
		 is 3.
"speed" - speed to move until reset by a path_corner
"target" - entity to stay focused on
"pathtarget" - this allows the camera to move
*/

// Forward declarations for camera functions
void misc_camera_stop(gentity_t* self);
void camera_move_next(gentity_t* self);


static void misc_camera_stop(gentity_t* self) {
	gentity_t* player = self->activator;

	// Ensure the player exists and is still being controlled by this camera
	if (player && player->client && player->goalEntity == self) {
		// Restore original movement state
		player->client->ps.pmove.pmType = (pmtype_t)self->count;
		player->moveType = (MoveType)self->style;
		player->goalEntity = nullptr;
		player->velocity = {}; // Stop any movement
	}

	// Stop the camera from thinking
	self->think = nullptr;
	self->nextThink = 0_ms;
}


static THINK(misc_camera_think) (gentity_t* self) -> void {
	gentity_t* player = self->activator;

	// If player is gone or no longer controlled by this camera, stop.
	if (!player || !player->inUse || player->goalEntity != self) {
		misc_camera_stop(self);
		return;
	}

	// Update view angles to look at the target entity
	if (self->targetEnt && self->targetEnt->inUse) {
		Vector3 dir = self->targetEnt->s.origin - player->s.origin;
		player->client->ps.viewAngles = VectorToAngles(dir);
	}

	// Continue this think loop until the linear move completes
	self->nextThink = level.time + FRAME_TIME_S;
}


MOVEINFO_ENDFUNC(camera_reached_corner) (gentity_t* player) -> void {
	gentity_t* self = player->goalEntity;

	// Stop the angle-updating think loop now that we've arrived
	self->think = nullptr;
	self->nextThink = 0_ms;

	// Handle wait time defined in the path_corner
	float wait_time = self->moveTarget->wait;
	if (wait_time == -1) { // Indefinite wait
		// Keep looking at the target indefinitely
		self->think = misc_camera_think;
		self->nextThink = level.time + FRAME_TIME_S;
		return;
	}

	// Move to the next path corner after the wait
	self->target = self->moveTarget->target;
	if (self->target) {
		self->think = camera_move_next;
		self->nextThink = level.time + GameTime::from_sec(wait_time);
	}
	else {
		// No next target, so end the camera sequence
		misc_camera_stop(self);
	}
}


static void camera_move_next(gentity_t* self) {
	gentity_t* player = self->activator;
	gentity_t* dest_corner = PickTarget(self->target);

	if (!dest_corner) {
		misc_camera_stop(self);
		return;
	}

	self->moveTarget = dest_corner;

	// Use speed from the camera or override with the corner's speed
	float speed = dest_corner->speed ? dest_corner->speed : self->speed;
	player->moveInfo.speed = player->moveInfo.accel = player->moveInfo.decel = speed;

	// Start the linear movement
	Move_Calc(player, dest_corner->s.origin, camera_reached_corner);

	// Start the angle-updating think loop
	self->think = misc_camera_think;
	self->nextThink = level.time + FRAME_TIME_S;
}


static USE(misc_camera_use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	if (!activator || !activator->client)
		return;

	// Don't start a new camera sequence if one is already active for this player
	if (activator->goalEntity)
		return;

	// --- Override logic for when activated by a trigger_misc_camera ---
	const char* lookAtTargetName = self->killTarget; // Default look-at from camera's "target" key
	bool lookAtIsActivator = false;

	if (other && !Q_strcasecmp(other->className, "trigger_misc_camera")) {
		// Override camera's wait time with the trigger's wait time
		if (other->wait != 0) // 0 is default from map editor if key isn't present
			self->wait = other->wait;

		// Override camera's look-at target with the trigger's pathtarget
		if (other->pathTarget) {
			lookAtTargetName = other->pathTarget;
		}
		else {
			// If trigger has no pathtarget, default to the activator
			lookAtIsActivator = true;
		}
	}

	// Set the final look-at entity
	if (lookAtIsActivator) {
		self->targetEnt = activator;
	}
	else if (lookAtTargetName) {
		self->targetEnt = PickTarget(lookAtTargetName);
	}
	// --- End of Override Logic ---

	// Store player's original movement state to restore later
	self->activator = activator;
	self->count = activator->client->ps.pmove.pmType; // Using 'count' for original pm_type
	self->style = (int)activator->moveType;           // Using 'style' for original movetype

	// Take control of the player
	if (self->spawnFlags.has(1_spawnflag)) // FREEZE spawnflag
		activator->client->ps.pmove.pmType = PM_FREEZE;
	activator->moveType = MoveType::Push; // Use push to follow path
	activator->goalEntity = self;

	// Start the path
	camera_move_next(self);
}


void SP_misc_camera(gentity_t* ent) {
	if (!ent->pathTarget) {
		gi.Com_PrintFmt("{}: misc_camera needs a pathtarget.\n", *ent);
		FreeEntity(ent);
		return;
	}

	ent->svFlags |= SVF_NOCLIENT;
	ent->use = misc_camera_use;

	// Store path and look-at targets using available fields
	ent->target = ent->pathTarget;
	ent->killTarget = ent->target;

	if (!ent->speed)
		ent->speed = 100;
	if (!ent->wait)
		ent->wait = 3;
}


/*QUAKED misc_camera_target (1 0 0 ) (-8 -8 -8) (8 8 8)
Target for cutscene misc_camera.

"speed" - speed to move until reset by a path_corner
"target" - entity to stay focused on
*/
void SP_misc_camera_target(gentity_t* ent) {
	// This entity is just a named, positional marker.
	// It has no logic of its own.
	ent->solid = SOLID_NOT;
	gi.linkEntity(ent);
}
