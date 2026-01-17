/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

============================================================================== SCRAG / WIZARD
(Quake 1) - WOR Port Floating caster that fires poison spikes while hovering. Revised to mirror
the Hover monster's structure and use supported AI callbacks from g_ai.cpp (ai_stand, ai_walk,
ai_run, ai_charge, ai_turn). No behavior or stats changed.
==============================================================================*/

#include "../g_local.hpp"
#include "m_scrag.hpp"
#include "m_flash.hpp"
#include "q1_support.hpp"

// -----------------------------------------------------------------------------
// Tunables (unchanged to preserve behavior)
// -----------------------------------------------------------------------------
static constexpr float SCRAG_MODEL_SCALE = 1.0f;
static constexpr int   SCRAG_HEALTH = 80;
static constexpr int   SCRAG_GIB_HEALTH = -40;
static constexpr int   SCRAG_DAMAGE = 10;    // wizspike-ish
static constexpr int   SCRAG_SPEED = 500;   // projectile speed
static const Vector3    SCRAG_MUZZLE_OFFSET = { 24.0f, 0.0f, 16.0f };

// -----------------------------------------------------------------------------
// Sounds
// -----------------------------------------------------------------------------
static cached_soundIndex s_idle1;
static cached_soundIndex s_idle2;
static cached_soundIndex s_sight;
static cached_soundIndex s_pain;
static cached_soundIndex s_death;
static cached_soundIndex s_attack;
static cached_soundIndex s_hit;
static cached_soundIndex s_fire;

MONSTERINFO_SEARCH(scrag_search) (gentity_t* self) -> void {
	if (frandom() < 0.5f)
		gi.sound(self, CHAN_VOICE, s_idle1, 1, ATTN_IDLE, 0);
	else
		gi.sound(self, CHAN_VOICE, s_idle2, 1, ATTN_IDLE, 0);
}

/*
===============
scrag_idle
===============
*/
static void scrag_idle(gentity_t* self) {
	if (frandom() < 0.15f)
		scrag_search(self);
}

/*
===============
scrag_sight
===============
*/
MONSTERINFO_SIGHT(scrag_sight) (gentity_t* self, gentity_t* other) -> void {
	gi.sound(self, CHAN_VOICE, s_sight, 1, ATTN_NORM, 0);
}

static void scrag_attack_sound(gentity_t* self) {
	gi.sound(self, CHAN_VOICE, s_attack, 1, ATTN_NORM, 0);
}

/*
===============
scrag_setskin
===============
*/
MONSTERINFO_SETSKIN(scrag_setskin) (gentity_t* self) -> void {
	// Keep base skin; no special pain skin by default.
}

static TOUCH(scrag_acid_touch) (gentity_t* projectile, gentity_t* other, const trace_t& tr, bool otherTouchingSelf) -> void {
	if (other == projectile->owner)
		return;

	if (tr.surface && (tr.surface->flags & SURF_SKY)) {
		FreeEntity(projectile);
		return;
	}

	if (projectile->owner && projectile->owner->client)
		G_PlayerNoise(projectile->owner, projectile->s.origin, PlayerNoise::Impact);

	if (other && other->takeDamage) {
		Damage(other, projectile, projectile->owner, projectile->velocity, projectile->s.origin, tr.plane.normal,
			projectile->dmg, 1, DamageFlags::Energy | DamageFlags::StatOnce, ModID::Gekk);
	}

	gi.sound(projectile, CHAN_AUTO, s_hit, 1, ATTN_NORM, 0);

	FreeEntity(projectile);
}

static void scrag_fire_acid(gentity_t* self, const Vector3& start, const Vector3& dir, int damage, int speed) {
	gentity_t* acid = Spawn();

	acid->svFlags |= SVF_PROJECTILE;
	acid->s.origin = start;
	acid->s.oldOrigin = start;
	acid->s.angles = VectorToAngles(dir);
	acid->velocity = dir * speed;
	acid->moveType = MoveType::FlyMissile;
	acid->clipMask = MASK_PROJECTILE;
	if (self->client && !G_ShouldPlayersCollide(true))
		acid->clipMask &= ~CONTENTS_PLAYER;
	acid->flags |= FL_DODGE;
	acid->solid = SOLID_BBOX;
	acid->s.effects |= EF_GREENGIB;
	acid->s.renderFX |= RF_FULLBRIGHT;
	acid->s.modelIndex = gi.modelIndex("models/objects/loogy/tris.md2");
	acid->owner = self;
	acid->touch = scrag_acid_touch;
	acid->nextThink = level.time + 2_sec;
	acid->think = FreeEntity;
	acid->dmg = damage;
	acid->className = "scrag_acid";

	gi.linkEntity(acid);

	trace_t tr = gi.traceLine(self->s.origin, acid->s.origin, acid, acid->clipMask);
	if (tr.fraction < 1.0f) {
		acid->s.origin = tr.endPos + (tr.plane.normal * 1.0f);
		scrag_acid_touch(acid, tr.ent, tr, false);
	}
}

/*
===============
scrag_fire
===============
*/
static void scrag_fire(gentity_t* self) {
	if (!self->enemy || !self->enemy->inUse)
		return;

	Vector3 forward, right;
	AngleVectors(self->s.angles, forward, right, nullptr);

	Vector3 start = M_ProjectFlashSource(self, SCRAG_MUZZLE_OFFSET, forward, right);

	Vector3 end = self->enemy->s.origin;
	end[2] += self->enemy->viewHeight;

	gi.sound(self, CHAN_WEAPON, s_fire, 1, ATTN_NORM, 0);

	// Match Quake 1's green acid bolt while keeping the familiar flash cue.
	const MonsterMuzzleFlashID flash = MZ2_FLYER_BLASTER_1;
	monster_muzzleflash(self, start, flash);
	Vector3 dir = end - start;
	dir.normalize();
	scrag_fire_acid(self, start, dir, SCRAG_DAMAGE, SCRAG_SPEED);
}

/*
===============
scrag_stand
===============
*/
void scrag_stand(gentity_t* self);

static MonsterFrame scrag_frames_stand[] = {
	{ ai_stand, 0, scrag_idle },
	{ ai_stand }, { ai_stand }, { ai_stand },
	{ ai_stand }, { ai_stand }
};
MMOVE_T(scrag_move_stand) = { FRAME_idle01, FRAME_idle06, scrag_frames_stand, scrag_stand };

/*
===============
scrag_stand
===============
*/
MONSTERINFO_STAND(scrag_stand) (gentity_t* self) -> void {
	M_SetAnimation(self, &scrag_move_stand);
}

/*
===============
scrag_fly
Use run-style motion like Hover to simulate flight.
===============
*/
void scrag_fly(gentity_t* self);

static MonsterFrame scrag_frames_fly[] = {
	{ ai_run, 4 }, { ai_run, 6 }, { ai_run, 5 }, { ai_run, 6 },
	{ ai_run, 4 }, { ai_run, 6 }, { ai_run, 5 }, { ai_run, 6 }
};
MMOVE_T(scrag_move_fly) = { FRAME_fly01, FRAME_fly08, scrag_frames_fly, nullptr };

/*
===============
scrag_fly
===============
*/
MONSTERINFO_RUN(scrag_fly) (gentity_t* self) -> void {
	if (self->monsterInfo.aiFlags & AI_STAND_GROUND) {
		M_SetAnimation(self, &scrag_move_stand);
		return;
	}
	M_SetAnimation(self, &scrag_move_fly);
}

/*
===============
scrag_walk
For completeness; uses a gentler motion than run.
===============
*/
MONSTERINFO_WALK(scrag_walk) (gentity_t* self) -> void {
	// Reuse run frames; distances come from the frame list and active_move.
	M_SetAnimation(self, &scrag_move_fly);
}

MONSTERINFO_DODGE(scrag_dodge) (gentity_t* self, gentity_t* attacker, GameTime eta, trace_t* tr, bool gravity) -> void {
	if (self->health <= 0)
		return;

	if (!self->enemy) {
		self->enemy = attacker;
		FoundTarget(self);
	}

	if ((eta < FRAME_TIME_MS) || (eta > 5_sec))
		return;

	if (self->timeStamp > level.time)
		return;

	self->timeStamp = level.time + random_time(1_sec, 5_sec);

	Vector3 offset{ crandom(), crandom(), crandom() };
	if (offset.lengthSquared() < 0.001f)
		offset = { 0.f, 0.f, 1.f };
	offset.normalize();

	float distance = (frandom() * 128.f) + 128.f;
	Vector3 desired = self->s.origin + (offset * distance);

	trace_t dodgeTrace = gi.trace(self->s.origin, self->mins, self->maxs, desired, self, MASK_MONSTERSOLID);
	if (dodgeTrace.fraction < 1.0f)
		desired = dodgeTrace.endPos;

	self->monsterInfo.fly_ideal_position = desired;
	self->monsterInfo.fly_position_time = level.time + random_time(600_ms, 1.4_sec);
	self->monsterInfo.fly_pinned = true;
}

/*
===============
scrag_attack
===============
*/
void scrag_attack(gentity_t* self);

static MonsterFrame scrag_frames_attack[] = {
		{ ai_charge },
		{ ai_charge },
		{ ai_charge, 0, scrag_attack_sound },
		{ ai_charge },
		{ ai_charge },
		{ ai_charge, -1, scrag_fire },
		{ ai_charge, -2 },
		{ ai_charge, -3 },
		{ ai_charge, -2, scrag_fire },
		{ ai_charge, -1 },
		{ ai_charge, 0, scrag_fire },
		{ ai_charge },
		{ ai_charge }
};
MMOVE_T(scrag_move_attack) = { FRAME_attack01, FRAME_attack13, scrag_frames_attack, scrag_fly };

/*
===============
scrag_attack
===============
*/
MONSTERINFO_ATTACK(scrag_attack) (gentity_t* self) -> void {
	self->monsterInfo.attackFinished = level.time + 1.4_sec;
	M_SetAnimation(self, &scrag_move_attack);
}

static MonsterFrame pain_frames[] = {
	{ ai_move }, { ai_move }, { ai_move }, { ai_move }
};
MMOVE_T(scrag_move_pain) = { FRAME_pain01, FRAME_pain04, pain_frames, scrag_fly };

/*
===============
scrag_pain
===============
*/
static PAIN(scrag_pain) (gentity_t* self, gentity_t* other, float kick, int damage, const MeansOfDeath& mod) -> void {
	if (level.time < self->pain_debounce_time)
		return;

	self->pain_debounce_time = level.time + 300_ms;
	gi.sound(self, CHAN_VOICE, s_pain, 1, ATTN_NORM, 0);

	M_SetAnimation(self, &scrag_move_pain);
}

/*
===============
scrag_dead
===============
*/
static void scrag_dead(gentity_t* self) {
	self->mins = { -16, -16, -24 };
	self->maxs = { 16,  16,  -8 };
	self->moveType = MoveType::Toss;
	self->nextThink = level.time + FRAME_TIME_S;
	gi.linkEntity(self);
}

static MonsterFrame death_frames[] = {
	{ ai_move }, { ai_move }, { ai_move }, { ai_move }, { ai_move }, { ai_move }, { ai_move }
};

MMOVE_T(scrag_move_death) = { FRAME_death01, FRAME_death07, death_frames, scrag_dead };

/*
===============
scrag_die
===============
*/
static DIE(scrag_die) (gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, const Vector3& point, const MeansOfDeath& mod) -> void {
	if (M_CheckGib(self, mod)) {
		ThrowGibs(self, 120, {
				{ "models/objects/gibs/sm_meat/tris.md2" }
			});
		return;
	}
	if (self->deadFlag)
		return;

	self->deadFlag = true;
	self->takeDamage = true;

	gi.sound(self, CHAN_VOICE, s_death, 1, ATTN_NORM, 0);
	M_SetAnimation(self, &scrag_move_death);
}

static void scrag_set_fly_parameters(gentity_t* self) {
	self->monsterInfo.fly_thrusters = false;
	self->monsterInfo.fly_acceleration = 20.f;
	self->monsterInfo.fly_speed = 120.f;
	self->monsterInfo.fly_min_distance = 200.f;
	self->monsterInfo.fly_max_distance = 400.f;
}

/*
===============
scrag_precache
===============
*/
static void scrag_precache() {
	gi.modelIndex("models/monsters/scrag/tris.md2");
	gi.modelIndex("models/objects/loogy/tris.md2");
	s_idle1.assign("scrag/widle1.wav");
	s_idle2.assign("scrag/widle2.wav");
	s_sight.assign("scrag/wsight.wav");
	s_pain.assign("scrag/wpain.wav");
	s_death.assign("scrag/wdeath.wav");
	s_attack.assign("scrag/wattack.wav");
	s_hit.assign("scrag/hit.wav");
	s_fire.assign("scrag/wfire.wav");
}

/*
===============
scrag_start
===============
*/
static void scrag_start(gentity_t* self) {
	// Hook table (mirrors hover layout)
	self->monsterInfo.stand = scrag_stand;
	self->monsterInfo.walk = scrag_walk;
	self->monsterInfo.run = scrag_fly;
	self->monsterInfo.attack = scrag_attack;
	self->monsterInfo.dodge = scrag_dodge;
	self->monsterInfo.sight = scrag_sight;
	self->monsterInfo.search = scrag_search;
	self->monsterInfo.setSkin = scrag_setskin;

	self->pain = scrag_pain;
	self->die = scrag_die;

	// Spawn setup (kept identical where it matters)
	self->moveType = MoveType::Step;
	self->solid = SOLID_BBOX;

	self->mins = { -16, -16, -24 };
	self->maxs = { 16,  16,  40 };
	self->s.scale = SCRAG_MODEL_SCALE;
	if (self->yawSpeed == 0)
		self->yawSpeed = 10;

	self->health = self->maxHealth = SCRAG_HEALTH * st.health_multiplier;
	self->gibHealth = SCRAG_GIB_HEALTH;
	self->mass = 50;
	self->viewHeight = 10;

	// Floating
	self->flags |= FL_FLY;
	self->monsterInfo.aiFlags |= AI_ALTERNATE_FLY;
	scrag_set_fly_parameters(self);
	self->monsterInfo.fly_pinned = false;
	self->monsterInfo.fly_position_time = 0_ms;

	gi.linkEntity(self);

	M_SetAnimation(self, &scrag_move_stand);
	self->monsterInfo.scale = SCRAG_MODEL_SCALE;

	flymonster_start(self);
}

/*QUAKED monster_wizard (1 .5 0) (-16 -16 -24) (16 16 40) AMBUSH TRIGGER_SPAWN SIGHT NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Quake 1 Scrag (Wizard). Floating monster that fires poison spikes.
*/
void SP_monster_wizard(gentity_t* self) {
	if (!M_AllowSpawn(self)) {
		FreeEntity(self);
		return;
	}

	scrag_precache();

	self->className = "monster_wizard";
	self->s.modelIndex = gi.modelIndex("models/monsters/scrag/tris.md2");

	scrag_start(self);
}
