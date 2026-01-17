/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

============================================================================== SUPERTANK
==============================================================================*/

#include "../g_local.hpp"
#include "m_supertank.hpp"
#include "m_flash.hpp"
#include <limits>

constexpr SpawnFlags SPAWNFLAG_SUPERTANK_POWERSHIELD = 8_spawnflag;
// n64
constexpr SpawnFlags SPAWNFLAG_SUPERTANK_LONG_DEATH = 16_spawnflag;

static cached_soundIndex sound_pain1;
static cached_soundIndex sound_pain2;
static cached_soundIndex sound_pain3;
static cached_soundIndex sound_death;
static cached_soundIndex sound_search1;
static cached_soundIndex sound_search2;

static cached_soundIndex tread_sound;

/*
=============
SupertankApplyN64Tuning

Configures Nintendo 64 specific behavior to match the port's toned-down death
sequences: the death animation loops longer, gore is disabled, and the corpse
sticks around without extra drops.
=============
*/
void SupertankApplyN64Tuning(gentity_t *self) {
	if (!self)
		return;

	self->spawnFlags |= SPAWNFLAG_SUPERTANK_LONG_DEATH;
	self->count = 10;
	self->gibHealth = std::numeric_limits<int32_t>::min();
}

static void TreadSound(gentity_t *self) {
	gi.sound(self, CHAN_BODY, tread_sound, 1, ATTN_NORM, 0);
}

MONSTERINFO_SEARCH(supertank_search) (gentity_t *self) -> void {
	if (frandom() < 0.5f)
		gi.sound(self, CHAN_VOICE, sound_search1, 1, ATTN_NORM, 0);
	else
		gi.sound(self, CHAN_VOICE, sound_search2, 1, ATTN_NORM, 0);
}

void supertank_dead(gentity_t *self);
void supertankRocket(gentity_t *self);
void supertankMachineGun(gentity_t *self);
void supertank_reattack1(gentity_t *self);

//
// stand
//

MonsterFrame supertank_frames_stand[] = {
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand }
};
MMOVE_T(supertank_move_stand) = { FRAME_stand_1, FRAME_stand_60, supertank_frames_stand, nullptr };

MONSTERINFO_STAND(supertank_stand) (gentity_t *self) -> void {
	M_SetAnimation(self, &supertank_move_stand);
}

MonsterFrame supertank_frames_run[] = {
	{ ai_run, 12, TreadSound },
	{ ai_run, 12 },
	{ ai_run, 12 },
	{ ai_run, 12 },
	{ ai_run, 12 },
	{ ai_run, 12 },
	{ ai_run, 12 },
	{ ai_run, 12 },
	{ ai_run, 12 },
	{ ai_run, 12 },
	{ ai_run, 12 },
	{ ai_run, 12 },
	{ ai_run, 12 },
	{ ai_run, 12 },
	{ ai_run, 12 },
	{ ai_run, 12 },
	{ ai_run, 12 },
	{ ai_run, 12 }
};
MMOVE_T(supertank_move_run) = { FRAME_forwrd_1, FRAME_forwrd_18, supertank_frames_run, nullptr };

//
// walk
//

MonsterFrame supertank_frames_forward[] = {
	{ ai_walk, 4, TreadSound },
	{ ai_walk, 4 },
	{ ai_walk, 4 },
	{ ai_walk, 4 },
	{ ai_walk, 4 },
	{ ai_walk, 4 },
	{ ai_walk, 4 },
	{ ai_walk, 4 },
	{ ai_walk, 4 },
	{ ai_walk, 4 },
	{ ai_walk, 4 },
	{ ai_walk, 4 },
	{ ai_walk, 4 },
	{ ai_walk, 4 },
	{ ai_walk, 4 },
	{ ai_walk, 4 },
	{ ai_walk, 4 },
	{ ai_walk, 4 }
};
MMOVE_T(supertank_move_forward) = { FRAME_forwrd_1, FRAME_forwrd_18, supertank_frames_forward, nullptr };

[[maybe_unused]] static void supertank_forward(gentity_t *self) {
	M_SetAnimation(self, &supertank_move_forward);
}

MONSTERINFO_WALK(supertank_walk) (gentity_t *self) -> void {
	M_SetAnimation(self, &supertank_move_forward);
}

MONSTERINFO_RUN(supertank_run) (gentity_t *self) -> void {
	if (self->monsterInfo.aiFlags & AI_STAND_GROUND)
		M_SetAnimation(self, &supertank_move_stand);
	else
		M_SetAnimation(self, &supertank_move_run);
}

MonsterFrame supertank_frames_pain3[] = {
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move }
};
MMOVE_T(supertank_move_pain3) = { FRAME_pain3_9, FRAME_pain3_12, supertank_frames_pain3, supertank_run };

MonsterFrame supertank_frames_pain2[] = {
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move }
};
MMOVE_T(supertank_move_pain2) = { FRAME_pain2_5, FRAME_pain2_8, supertank_frames_pain2, supertank_run };

MonsterFrame supertank_frames_pain1[] = {
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move }
};
MMOVE_T(supertank_move_pain1) = { FRAME_pain1_1, FRAME_pain1_4, supertank_frames_pain1, supertank_run };

static void BossLoop(gentity_t *self) {
	if (!(self->spawnFlags & SPAWNFLAG_SUPERTANK_LONG_DEATH))
		return;

	if (self->count)
		self->count--;
	else
		self->spawnFlags &= ~SPAWNFLAG_SUPERTANK_LONG_DEATH;

	self->monsterInfo.nextFrame = FRAME_death_19;
}

static void supertankGrenade(gentity_t *self) {
	Vector3					 forward, right;
	Vector3					 start;
	MonsterMuzzleFlashID flash_number;

	if (!self->enemy || !self->enemy->inUse)
		return;

	if (self->s.frame == FRAME_attack4_1)
		flash_number = MZ2_SUPERTANK_GRENADE_1;
	else
		flash_number = MZ2_SUPERTANK_GRENADE_2;

	AngleVectors(self->s.angles, forward, right, nullptr);
	start = M_ProjectFlashSource(self, monster_flash_offset[flash_number], forward, right);

	Vector3 aim_point;
	PredictAim(self, self->enemy, start, 0, false, crandom_open() * 0.1f, &forward, &aim_point);

	for (float speed = 500.f; speed < 1000.f; speed += 100.f) {
		if (!M_CalculatePitchToFire(self, aim_point, start, forward, speed, 2.5f, true))
			continue;

		monster_fire_grenade(self, start, forward, 50, speed, flash_number, 0.f, 0.f);
		break;
	}
}

MonsterFrame supertank_frames_death1[] = {
	{ ai_move, 0, BossExplode },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move, 0, BossLoop }
};
MMOVE_T(supertank_move_death) = { FRAME_death_1, FRAME_death_24, supertank_frames_death1, supertank_dead };

MonsterFrame supertank_frames_attack4[] = {
	{ ai_move, 0, supertankGrenade },
	{ ai_move },
	{ ai_move },
	{ ai_move, 0, supertankGrenade },
	{ ai_move },
	{ ai_move }
};
MMOVE_T(supertank_move_attack4) = { FRAME_attack4_1, FRAME_attack4_6, supertank_frames_attack4, supertank_run };

MonsterFrame supertank_frames_attack2[] = {
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge, 0, supertankRocket },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge, 0, supertankRocket },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge, 0, supertankRocket },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move }
};
MMOVE_T(supertank_move_attack2) = { FRAME_attack2_1, FRAME_attack2_27, supertank_frames_attack2, supertank_run };

MonsterFrame supertank_frames_attack1[] = {
	{ ai_charge, 0, supertankMachineGun },
	{ ai_charge, 0, supertankMachineGun },
	{ ai_charge, 0, supertankMachineGun },
	{ ai_charge, 0, supertankMachineGun },
	{ ai_charge, 0, supertankMachineGun },
	{ ai_charge, 0, supertankMachineGun },
};
MMOVE_T(supertank_move_attack1) = { FRAME_attack1_1, FRAME_attack1_6, supertank_frames_attack1, supertank_reattack1 };

MonsterFrame supertank_frames_end_attack1[] = {
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move }
};
MMOVE_T(supertank_move_end_attack1) = { FRAME_attack1_7, FRAME_attack1_20, supertank_frames_end_attack1, supertank_run };

void supertank_reattack1(gentity_t *self) {
	if (visible(self, self->enemy)) {
		if (self->timeStamp >= level.time || frandom() < 0.3f)
			M_SetAnimation(self, &supertank_move_attack1);
		else
			M_SetAnimation(self, &supertank_move_end_attack1);
	} else
		M_SetAnimation(self, &supertank_move_end_attack1);
}

static PAIN(supertank_pain) (gentity_t *self, gentity_t *other, float kick, int damage, const MeansOfDeath &mod) -> void {
	if (level.time < self->pain_debounce_time)
		return;

	// Lessen the chance of him going into his pain frames
	if (mod.id != ModID::Chainfist) {
		if (damage <= 25)
			if (frandom() < 0.2f)
				return;

		// Don't go into pain if he's firing his rockets
		if ((self->s.frame >= FRAME_attack2_1) && (self->s.frame <= FRAME_attack2_14))
			return;
	}

	if (damage <= 10)
		gi.sound(self, CHAN_VOICE, sound_pain1, 1, ATTN_NORM, 0);
	else if (damage <= 25)
		gi.sound(self, CHAN_VOICE, sound_pain3, 1, ATTN_NORM, 0);
	else
		gi.sound(self, CHAN_VOICE, sound_pain2, 1, ATTN_NORM, 0);

	self->pain_debounce_time = level.time + 3_sec;

	if (!M_ShouldReactToPain(self, mod))
		return; // no pain anims in nightmare

	if (damage <= 10)
		M_SetAnimation(self, &supertank_move_pain1);
	else if (damage <= 25)
		M_SetAnimation(self, &supertank_move_pain2);
	else
		M_SetAnimation(self, &supertank_move_pain3);
}

MONSTERINFO_SETSKIN(supertank_setskin) (gentity_t *self) -> void {
	if (self->health < (self->maxHealth / 2))
		self->s.skinNum |= 1;
	else
		self->s.skinNum &= ~1;
}

void supertankRocket(gentity_t *self) {
	Vector3					 forward, right;
	Vector3					 start;
	Vector3					 dir;
	Vector3					 vec;
	MonsterMuzzleFlashID flash_number;

	if (!self->enemy || !self->enemy->inUse)
		return;

	if (self->s.frame == FRAME_attack2_8)
		flash_number = MZ2_SUPERTANK_ROCKET_1;
	else if (self->s.frame == FRAME_attack2_11)
		flash_number = MZ2_SUPERTANK_ROCKET_2;
	else // (self->s.frame == FRAME_attack2_14)
		flash_number = MZ2_SUPERTANK_ROCKET_3;

	AngleVectors(self->s.angles, forward, right, nullptr);
	start = M_ProjectFlashSource(self, monster_flash_offset[flash_number], forward, right);

	if (self->spawnFlags.has(SPAWNFLAG_SUPERTANK_POWERSHIELD)) {
		vec = self->enemy->s.origin;
		vec[2] += self->enemy->viewHeight;
		dir = vec - start;
		dir.normalize();
		monster_fire_heat(self, start, dir, 40, 500, flash_number, 0.075f);
	} else {
		PredictAim(self, self->enemy, start, 750, false, 0.f, &forward, nullptr);
		monster_fire_rocket(self, start, forward, 50, 750, flash_number);
	}
}

void supertankMachineGun(gentity_t *self) {
	Vector3					 dir{};
	Vector3					 start;
	Vector3					 forward, right;
	MonsterMuzzleFlashID flash_number;

	if (!self->enemy || !self->enemy->inUse)
		return;

	flash_number = static_cast<MonsterMuzzleFlashID>(MZ2_SUPERTANK_MACHINEGUN_1 + (self->s.frame - FRAME_attack1_1));

	dir[0] = 0;
	dir[1] = self->s.angles[YAW];
	dir[2] = 0;

	AngleVectors(dir, forward, right, nullptr);
	start = M_ProjectFlashSource(self, monster_flash_offset[flash_number], forward, right);
	PredictAim(self, self->enemy, start, 0, true, -0.1f, &forward, nullptr);
	monster_fire_bullet(self, start, forward, 6, 4, DEFAULT_BULLET_HSPREAD * 3, DEFAULT_BULLET_VSPREAD * 3, flash_number);
}

MONSTERINFO_ATTACK(supertank_attack) (gentity_t *self) -> void {
	Vector3 vec;
	float  range;

	vec = self->enemy->s.origin - self->s.origin;
	range = range_to(self, self->enemy);

	// Attack 1 == Chaingun
	// Attack 2 == Rocket Launcher
	// Attack 3 == Grenade Launcher
	bool chaingun_good = M_CheckClearShot(self, monster_flash_offset[MZ2_SUPERTANK_MACHINEGUN_1]);
	bool rocket_good = M_CheckClearShot(self, monster_flash_offset[MZ2_SUPERTANK_ROCKET_1]);
	bool grenade_good = M_CheckClearShot(self, monster_flash_offset[MZ2_SUPERTANK_GRENADE_1]);

	// fire rockets more often at distance
	if (chaingun_good && (!rocket_good || range <= 540 || frandom() < 0.3f)) {
		// prefer grenade if the enemy is above us
		if (grenade_good && (range >= 350 || vec.z > 120.f || frandom() < 0.2f))
			M_SetAnimation(self, &supertank_move_attack4);
		else {
			M_SetAnimation(self, &supertank_move_attack1);
			self->timeStamp = level.time + random_time(1500_ms, 2700_ms);
		}
	} else if (rocket_good) {
		// prefer grenade if the enemy is above us
		if (grenade_good && (vec.z > 120.f || frandom() < 0.2f))
			M_SetAnimation(self, &supertank_move_attack4);
		else
			M_SetAnimation(self, &supertank_move_attack2);
	} else if (grenade_good)
		M_SetAnimation(self, &supertank_move_attack4);
}

//
// death
//

static void supertank_gib(gentity_t *self) {
	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_EXPLOSION1_BIG);
	gi.WritePosition(self->s.origin);
	gi.multicast(self->s.origin, MULTICAST_PHS, false);

	self->s.sound = 0;
	self->s.skinNum /= 2;

	ThrowGibs(self, 500, {
		{ 2, "models/objects/gibs/sm_meat/tris.md2" },
		{ 2, "models/objects/gibs/sm_metal/tris.md2", GIB_METALLIC },
		{ "models/monsters/boss1/gibs/cgun.md2", GIB_SKINNED | GIB_METALLIC },
		{ "models/monsters/boss1/gibs/chest.md2", GIB_SKINNED },
		{ "models/monsters/boss1/gibs/core.md2", GIB_SKINNED },
		{ "models/monsters/boss1/gibs/ltread.md2", GIB_SKINNED | GIB_UPRIGHT },
		{ "models/monsters/boss1/gibs/rgun.md2", GIB_SKINNED | GIB_UPRIGHT },
		{ "models/monsters/boss1/gibs/rtread.md2", GIB_SKINNED | GIB_UPRIGHT },
		{ "models/monsters/boss1/gibs/tube.md2", GIB_SKINNED | GIB_UPRIGHT },
		{ "models/monsters/boss1/gibs/head.md2", GIB_SKINNED | GIB_METALLIC | GIB_HEAD }
		});
}

void supertank_dead(gentity_t *self) {
	// no blowy on deady
	if (self->spawnFlags.has(SPAWNFLAG_MONSTER_CORPSE)) {
		self->deadFlag = false;
		self->takeDamage = true;
		return;
	}

	supertank_gib(self);
}

static DIE(supertank_die) (gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, const Vector3 &point, const MeansOfDeath &mod) -> void {
	if (self->spawnFlags.has(SPAWNFLAG_MONSTER_CORPSE)) {
		// check for gib
		if (M_CheckGib(self, mod)) {
			supertank_gib(self);
			self->deadFlag = true;
			return;
		}

		if (self->deadFlag)
			return;
	} else {
		gi.sound(self, CHAN_VOICE, sound_death, 1, ATTN_NORM, 0);
		self->deadFlag = true;
		self->takeDamage = false;
	}

	M_SetAnimation(self, &supertank_move_death);
}

MONSTERINFO_BLOCKED(supertank_blocked) (gentity_t *self, float dist) -> bool {
	if (blocked_checkplat(self, dist))
		return true;

	return false;
}

//
// monster_supertank
//

/*QUAKED monster_supertank (1 .5 0) (-64 -64 0) (64 64 72) AMBUSH TRIGGER_SPAWN SIGHT POWERSHIELD LONGDEATH x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
*/
void SP_monster_supertank(gentity_t *self) {
	if (!M_AllowSpawn(self)) {
		FreeEntity(self);
		return;
	}

	sound_pain1.assign("bosstank/btkpain1.wav");
	sound_pain2.assign("bosstank/btkpain2.wav");
	sound_pain3.assign("bosstank/btkpain3.wav");
	sound_death.assign("bosstank/btkdeth1.wav");
	sound_search1.assign("bosstank/btkunqv1.wav");
	sound_search2.assign("bosstank/btkunqv2.wav");

	tread_sound.assign("bosstank/btkengn1.wav");

	gi.soundIndex("gunner/gunatck3.wav");
	gi.soundIndex("infantry/infatck1.wav");
	gi.soundIndex("tank/rocket.wav");

	self->moveType = MoveType::Step;
	self->solid = SOLID_BBOX;
	self->s.modelIndex = gi.modelIndex("models/monsters/boss1/tris.md2");

	gi.modelIndex("models/monsters/boss1/gibs/cgun.md2");
	gi.modelIndex("models/monsters/boss1/gibs/chest.md2");
	gi.modelIndex("models/monsters/boss1/gibs/core.md2");
	gi.modelIndex("models/monsters/boss1/gibs/head.md2");
	gi.modelIndex("models/monsters/boss1/gibs/ltread.md2");
	gi.modelIndex("models/monsters/boss1/gibs/rgun.md2");
	gi.modelIndex("models/monsters/boss1/gibs/rtread.md2");
	gi.modelIndex("models/monsters/boss1/gibs/tube.md2");

	self->mins = { -64, -64, 0 };
	self->maxs = { 64, 64, 112 };

	self->health = 1500 * st.health_multiplier;
	self->gibHealth = -500;
	self->mass = 800;

	self->pain = supertank_pain;
	self->die = supertank_die;
	self->monsterInfo.stand = supertank_stand;
	self->monsterInfo.walk = supertank_walk;
	self->monsterInfo.run = supertank_run;
	self->monsterInfo.dodge = nullptr;
	self->monsterInfo.attack = supertank_attack;
	self->monsterInfo.search = supertank_search;
	self->monsterInfo.melee = nullptr;
	self->monsterInfo.sight = nullptr;
	self->monsterInfo.blocked = supertank_blocked;
	self->monsterInfo.setSkin = supertank_setskin;

	gi.linkEntity(self);

	M_SetAnimation(self, &supertank_move_stand);
	self->monsterInfo.scale = MODEL_SCALE;

	if (self->spawnFlags.has(SPAWNFLAG_SUPERTANK_POWERSHIELD)) {
		if (!st.was_key_specified("powerArmorType"))
			self->monsterInfo.powerArmorType = IT_POWER_SHIELD;
		if (!st.was_key_specified("powerArmorPower"))
			self->monsterInfo.powerArmorPower = 400;
	}

	walkmonster_start(self);

	self->monsterInfo.aiFlags |= AI_IGNORE_SHOTS;

	if (level.isN64)
		SupertankApplyN64Tuning(self);
}

/*QUAKED monster_boss5 (1 .5 0) (-64 -64 0) (64 64 72) AMBUSH TRIGGER_SPAWN SIGHT x CORPSE x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
 */
void SP_monster_boss5(gentity_t *self) {
	self->spawnFlags |= SPAWNFLAG_SUPERTANK_POWERSHIELD;
	SP_monster_supertank(self);
	gi.soundIndex("weapons/railgr1a.wav");
	self->s.skinNum = 2;
}
