/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

============================================================================== QUAKE WYVERN
==============================================================================*/

#include "../g_local.hpp"
#include "m_wyvern.hpp"
#include "q1_support.hpp"

static cached_soundIndex sound_sight1;
static cached_soundIndex sound_sight2;
static cached_soundIndex sound_search1;
static cached_soundIndex sound_search2;
static cached_soundIndex sound_search3;
static cached_soundIndex sound_attack;
static cached_soundIndex sound_die1;
static cached_soundIndex sound_die2;
static cached_soundIndex sound_pain1;
static cached_soundIndex sound_pain2;
static cached_soundIndex sound_flame;

static void wyvern_reattack(gentity_t* self);

MONSTERINFO_SIGHT(wyvern_sight) (gentity_t* self, gentity_t* other) -> void {
	if (frandom() > 0.5f)
		gi.sound(self, CHAN_VOICE, sound_sight2, 1, ATTN_NONE, 0);
	else
		gi.sound(self, CHAN_VOICE, sound_sight1, 1, ATTN_NONE, 0);
}

MONSTERINFO_SEARCH(wyvern_search) (gentity_t* self) -> void {
	if (frandom() > 0.66f)
		gi.sound(self, CHAN_VOICE, sound_search3, 1, ATTN_NONE, 0);
	else if (frandom() > 0.33f)
		gi.sound(self, CHAN_VOICE, sound_search2, 1, ATTN_NONE, 0);
	else
		gi.sound(self, CHAN_VOICE, sound_search1, 1, ATTN_NONE, 0);
}

//================
// HOVER
//================
static MonsterFrame wyvern_frames_hover[] = {
		{ ai_stand },
		{ ai_stand },
		{ ai_stand },
		{ ai_stand },
		{ ai_stand },

		{ ai_stand },
		{ ai_stand },
		{ ai_stand }
};
MMOVE_T(wyvern_move_hover) = { FRAME_fly1, FRAME_fly8, wyvern_frames_hover, nullptr };

MONSTERINFO_STAND(wyvern_hover) (gentity_t* self) -> void {
	M_SetAnimation(self, &wyvern_move_hover);
}

//================
// WALK
//================
static MonsterFrame wyvern_frames_walk[] = {
		{ ai_walk, 5 },
		{ ai_walk, 5 },
		{ ai_walk, 5 },
		{ ai_walk, 5 },
		{ ai_walk, 5 },

		{ ai_walk, 5 },
		{ ai_walk, 5 },
		{ ai_walk, 5 }
};
MMOVE_T(wyvern_move_walk) = { FRAME_fly1, FRAME_fly8, wyvern_frames_walk, nullptr };

MONSTERINFO_WALK(wyvern_walk) (gentity_t* self) -> void {
	M_SetAnimation(self, &wyvern_move_walk);
}

//================
// FLY
//================
static MonsterFrame wyvern_frames_run[] = {
		{ ai_run, 10 },
		{ ai_run, 10 },
		{ ai_run, 10 },
		{ ai_run, 10 },
		{ ai_run, 10 },

		{ ai_run, 10 },
		{ ai_run, 10 },
		{ ai_run, 10 }
};
MMOVE_T(wyvern_move_run) = { FRAME_fly1, FRAME_fly8, wyvern_frames_run, nullptr };

MONSTERINFO_RUN(wyvern_run) (gentity_t* self) -> void {
	M_SetAnimation(self, &wyvern_move_run);
}

//================
// PAIN
//================
static MonsterFrame wyvern_frames_pain1[] = {
		{ ai_move },
		{ ai_move },
		{ ai_move },
		{ ai_move },
		{ ai_move },

		{ ai_move }
};
MMOVE_T(wyvern_move_pain1) = { FRAME_pain1, FRAME_pain6, wyvern_frames_pain1, wyvern_run };

static MonsterFrame wyvern_frames_pain2[] = {
		{ ai_move },
		{ ai_move },
		{ ai_move },
		{ ai_move },
		{ ai_move },

		{ ai_move }
};
MMOVE_T(wyvern_move_pain2) = { FRAME_painb1, FRAME_painb6, wyvern_frames_pain2, wyvern_run };

static MonsterFrame wyvern_frames_pain3[] = {
		{ ai_move },
		{ ai_move },
		{ ai_move },
		{ ai_move },
		{ ai_move },

		{ ai_move }
};
MMOVE_T(wyvern_move_pain3) = { FRAME_painc1, FRAME_painc6, wyvern_frames_pain3, wyvern_run };

static PAIN(wyvern_pain) (gentity_t* self, gentity_t* other, float kick, int damage, const MeansOfDeath& mod) -> void {
	if (level.time < self->pain_debounce_time)
		return;

	self->pain_debounce_time = level.time + 1_sec;

	if (!M_ShouldReactToPain(self, mod))
		return; // no pain anims in nightmare

	if (damage < 30) {
		gi.sound(self, CHAN_VOICE, sound_pain1, 1, ATTN_NORM, 0);
		M_SetAnimation(self, &wyvern_move_pain1);
	}
	else {
		gi.sound(self, CHAN_VOICE, sound_pain2, 1, ATTN_NORM, 0);
		if (frandom() >= 0.5f)
			M_SetAnimation(self, &wyvern_move_pain2);
		else
			M_SetAnimation(self, &wyvern_move_pain3);
	}
}

MONSTERINFO_SETSKIN(wyvern_setskin) (gentity_t* self) -> void {
	if (self->health < (self->maxHealth / 2))
		self->s.skinNum |= 1;
	else
		self->s.skinNum &= ~1;
}

//================
// DEAD
//================
static void wyvern_gib(gentity_t* self) {
	gi.sound(self, CHAN_VOICE, gi.soundIndex("misc/udeath.wav"), 1, ATTN_NORM, 0);

	self->s.skinNum /= 2;

	ThrowGibs(self, 1000, {
			{ 2, "models/objects/gibs/bone/tris.md2" },
			{ 4, "models/objects/gibs/sm_meat/tris.md2" },
			{ "models/monsters/wyvern/gibs/tail.md2", GIB_SKINNED | GIB_HEAD },
			{ "models/monsters/wyvern/gibs/rwing.md2", GIB_SKINNED },
			{ "models/monsters/wyvern/gibs/lwing.md2", GIB_SKINNED },
		});
}

THINK(wyvern_deadthink) (gentity_t* self) -> void {
	if (!self->groundEntity && level.time < self->timeStamp) {
		self->nextThink = level.time + FRAME_TIME_S;
		return;
	}
}

static void wyvern_dead(gentity_t* self) {
	self->mins = { -144, -136, -36 };
	self->maxs = { 88, 128, 24 };
	self->moveType = MoveType::Toss;
	self->think = wyvern_deadthink;
	self->nextThink = level.time + FRAME_TIME_S;
	self->timeStamp = level.time + 15_sec;
	gi.linkEntity(self);

	wyvern_gib(self);
}

static MonsterFrame wyvern_frames_die1[] = {
		{ ai_move, 0, Q1BossExplode },
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
MMOVE_T(wyvern_move_die1) = { FRAME_die1, FRAME_die13, wyvern_frames_die1, wyvern_dead };

static MonsterFrame wyvern_frames_die2[] = {
		{ ai_move, 0, Q1BossExplode },
		{ ai_move },
		{ ai_move },
		{ ai_move },
		{ ai_move },

		{ ai_move }
};
MMOVE_T(wyvern_move_die2) = { FRAME_dieb1, FRAME_dieb6, wyvern_frames_die2, wyvern_dead };

static DIE(wyvern_die) (gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, const Vector3& point, const MeansOfDeath& mod) -> void {
	if (M_CheckGib(self, mod)) {
		wyvern_gib(self);

		self->deadFlag = true;
		return;
	}

	if (self->deadFlag)
		return;

	if (frandom() > 0.5f) {
		gi.sound(self, CHAN_VOICE, sound_die1, 1, ATTN_NORM, 0);
		M_SetAnimation(self, &wyvern_move_die1);
	}
	else {
		gi.sound(self, CHAN_VOICE, sound_die2, 1, ATTN_NORM, 0);
		M_SetAnimation(self, &wyvern_move_die2);
	}
	self->deadFlag = true;
	self->takeDamage = true;
}

//================
// ATTACK
//================
static void wyvern_fireball(gentity_t* self) {
	const Vector3 offset = { 73, 0, -22 };
	Vector3 forward, right;
	AngleVectors(self->s.angles, forward, right, nullptr);
	const Vector3 start = M_ProjectFlashSource(self, offset, forward, right);

	const bool blindFire = (self->monsterInfo.aiFlags & AI_MANUAL_STEERING) != 0;

	if (!self->enemy || !self->enemy->inUse)
		return;

	Vector3 target = blindFire ? self->monsterInfo.blind_fire_target : self->enemy->s.origin;
	Vector3 vec;
	Vector3 dir;

	if (blindFire) {
		vec = target;
		dir = vec - start;
	}
	else if (frandom() < 0.33f || (start.z < self->enemy->absMin.z)) {
		vec = target;
		vec.z += self->enemy->viewHeight;
		dir = vec - start;
	}
	else {
		vec = target;
		vec.z = self->enemy->absMin.z + 1.f;
		dir = vec - start;
	}

	if (!blindFire && frandom() < 0.35f)
		PredictAim(self, self->enemy, start, 750.f, false, 0.f, &dir, &vec);

	dir.normalize();

	trace_t trace = gi.traceLine(start, vec, self, MASK_PROJECTILE);
	constexpr int damage = 100;
	constexpr int speed = 750;

        if (blindFire) {
                if (!(trace.startSolid || trace.allSolid || (trace.fraction < 0.5f))) {
                        [[maybe_unused]] gentity_t *projectile = fire_lavaball(
                                self, start, dir, damage, speed, static_cast<float>(damage), damage);
                } else {
                        vec = target + (right * -10.f);
                        dir = (vec - start).normalized();
                        trace = gi.traceLine(start, vec, self, MASK_PROJECTILE);
                        if (!(trace.startSolid || trace.allSolid || (trace.fraction < 0.5f))) {
                                [[maybe_unused]] gentity_t *projectile = fire_lavaball(
                                        self, start, dir, damage, speed, static_cast<float>(damage), damage);
                        } else {
                                vec = target + (right * 10.f);
                                dir = (vec - start).normalized();
                                trace = gi.traceLine(start, vec, self, MASK_PROJECTILE);
                                if (!(trace.startSolid || trace.allSolid || (trace.fraction < 0.5f))) {
                                        [[maybe_unused]] gentity_t *projectile = fire_lavaball(
                                                self, start, dir, damage, speed, static_cast<float>(damage), damage);
                                }
                        }
                }
        } else {
                if (trace.fraction > 0.5f || (trace.ent && trace.ent->solid != SOLID_BSP)) {
                        [[maybe_unused]] gentity_t *projectile = fire_lavaball(
                                self, start, dir, damage, speed, static_cast<float>(damage), damage);
                }
        }

	gi.sound(self, CHAN_VOICE, sound_attack, 1, ATTN_NORM, 0);
}

static void wyvern_firebreath(gentity_t* self) {
	const Vector3 offset = { 73, 0, -22 };
	Vector3 forward, right;
	AngleVectors(self->s.angles, forward, right, nullptr);
	const Vector3 start = M_ProjectFlashSource(self, offset, forward, right);

	Vector3 end = self->enemy->s.origin;
	end.z += self->enemy->viewHeight;
	Vector3 aim = end - start;

	Vector3 aimAngles = VectorToAngles(aim);
	Vector3 up;
	AngleVectors(aimAngles, forward, right, up);

	int fireCount = min(3, skill->integer + 1);

	while (fireCount-- > 0) {
		float r = crandom() * 2000.f;
		float u = crandom() * 1000.f;

		Vector3 sprayEnd = start + (forward * 8192.f) + (right * r) + (up * u);
		Vector3 dir = (sprayEnd - start).normalized();

                [[maybe_unused]] gentity_t* projectile =
                        fire_flame(self, start, dir, 12, 500, ModID::IonRipper);
                gi.sound(self, CHAN_VOICE, sound_attack, 1, ATTN_NORM, 0);
                gi.sound(self, CHAN_WEAPON, sound_flame, 1, ATTN_NORM, 0);
        }
}

static MonsterFrame wyvern_frames_attack1[] = {
		{ ai_charge, 45 },
		{ ai_charge, 30 },
		{ ai_charge, 15 },
		{ ai_charge, 0 },
		{ ai_charge, 0 },
		{ ai_charge, 0, wyvern_fireball },
		{ ai_charge },
		{ ai_charge },
		{ ai_charge, 15, wyvern_reattack }
};
MMOVE_T(wyvern_move_attack1) = { FRAME_attack1, FRAME_attack9, wyvern_frames_attack1, nullptr };

static MonsterFrame wyvern_frames_attack2[] = {
		{ ai_charge, 0 },
		{ ai_charge, 0 },
		{ ai_charge, 0, wyvern_firebreath },
		{ ai_charge, 0, wyvern_firebreath },
		{ ai_charge, 0, wyvern_firebreath },
		{ ai_charge, 0, wyvern_firebreath },
		{ ai_charge },
		{ ai_charge },
		{ ai_charge, 15, wyvern_reattack }
};
MMOVE_T(wyvern_move_attack2) = { FRAME_attack1, FRAME_attack9, wyvern_frames_attack2, nullptr };

MONSTERINFO_ATTACK(wyvern_attack) (gentity_t* self) -> void {
	if (!M_CheckClearShot(self, { 0, 96, 32 }))
		return;

	if (self->monsterInfo.attackState == MonsterAttackState::Blind) {
		float chance;
		if (self->monsterInfo.blind_fire_delay < 1_sec)
			chance = 1.0f;
		else if (self->monsterInfo.blind_fire_delay < 7.5_sec)
			chance = 0.4f;
		else
			chance = 0.1f;

		float r = frandom();

		self->monsterInfo.blind_fire_delay += random_time(5.5_sec, 6.5_sec);

		if (!self->monsterInfo.blind_fire_target)
			return;

		if (r > chance)
			return;

		self->monsterInfo.aiFlags |= AI_MANUAL_STEERING;
		M_SetAnimation(self, &wyvern_move_attack1);
		self->monsterInfo.attackFinished = level.time + random_time(2_sec);
		return;
	}

	const float range = range_to(self, self->enemy);
	if ((range <= RANGE_NEAR && frandom() >= 0.5f) || range <= (RANGE_NEAR / 3.f))
		M_SetAnimation(self, &wyvern_move_attack2);
	else
		M_SetAnimation(self, &wyvern_move_attack1);
}

static void wyvern_reattack(gentity_t* self) {
	if (self->enemy && self->enemy->health > 0 && visible(self, self->enemy) && frandom() <= 0.6f) {
		M_SetAnimation(self, &wyvern_move_attack1);
		return;
	}

	wyvern_run(self);
}

MONSTERINFO_CHECKATTACK(wyvern_checkattack) (gentity_t* self) -> bool {
	if (!self->enemy)
		return false;

	return M_CheckAttack_Base(self, 0.4f, 0.8f, 0.8f, 0.8f, 0.f, 0.f);
}

static void wyvern_set_fly_parameters(gentity_t* self) {
	self->monsterInfo.fly_thrusters = false;
	self->monsterInfo.fly_acceleration = 20.f;
	self->monsterInfo.fly_speed = 120.f;
	self->monsterInfo.fly_min_distance = 550.f;
	self->monsterInfo.fly_max_distance = 750.f;
}

/*QUAKED monster_wyvern(1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight
model="models/monsters/wyvern/tris.md2"
*/
void SP_monster_wyvern(gentity_t* self) {
	if (!M_AllowSpawn(self)) {
		FreeEntity(self);
		return;
	}

	sound_sight1.assign("wyvern/sight1.wav");
	sound_sight2.assign("wyvern/sight2.wav");
	sound_search1.assign("wyvern/idle1.wav");
	sound_search2.assign("wyvern/idle2.wav");
	sound_search3.assign("wyvern/idlefly.wav");
	sound_pain1.assign("wyvern/pain1.wav");
	sound_pain2.assign("wyvern/pain2.wav");
	sound_die1.assign("wyvern/death.wav");
	sound_die2.assign("wyvern/death2.wav");
	sound_attack.assign("wyvern/fire.wav");
	sound_flame.assign("hknight/attack1.wav");

	self->s.modelIndex = gi.modelIndex("models/monsters/wyvern/tris.md2");
	gi.modelIndex("models/monsters/wyvern/gibs/tail.md2");
	gi.modelIndex("models/monsters/wyvern/gibs/rwing.md2");
	gi.modelIndex("models/monsters/wyvern/gibs/lwing.md2");

	self->mins = { -48, -64, -36 };
	self->maxs = { 48, 64, 24 };

	self->moveType = MoveType::Step;
	self->solid = SOLID_BBOX;

	self->health = max(2000, 2000 + 1000 * (skill->integer - 1)) * st.health_multiplier;
	if (!st.was_key_specified("armor_type"))
		self->monsterInfo.armorType = IT_ARMOR_BODY;
	if (!st.was_key_specified("armor_power"))
		self->monsterInfo.armor_power = max(350, 350 + 100 * (skill->integer - 1));
	self->gibHealth = -500;
	self->mass = 500;
	if (coop->integer) {
		const int additionalPlayers = max(0, CountPlayers() - 1);
		self->health += 250 * additionalPlayers;
		self->monsterInfo.armor_power += 100 * additionalPlayers;
	}

	self->maxHealth = self->health;

	self->pain = wyvern_pain;
	self->die = wyvern_die;

	self->yawSpeed = 20;

	self->monsterInfo.stand = wyvern_hover;
	self->monsterInfo.walk = wyvern_walk;
	self->monsterInfo.run = wyvern_run;
	self->monsterInfo.attack = wyvern_attack;
	self->monsterInfo.melee = nullptr;
	self->monsterInfo.sight = wyvern_sight;
	self->monsterInfo.search = wyvern_search;
	self->monsterInfo.setSkin = wyvern_setskin;
	self->monsterInfo.checkAttack = wyvern_checkattack;

	gi.linkEntity(self);

	M_SetAnimation(self, &wyvern_move_hover);
	self->monsterInfo.scale = MODEL_SCALE;

	flymonster_start(self);

	self->monsterInfo.aiFlags |= AI_ALTERNATE_FLY;
	wyvern_set_fly_parameters(self);
}
