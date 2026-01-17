/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

m_sword.cpp implementation.*/

#include "../g_local.hpp"
#include "m_sword.hpp"

constexpr SpawnFlags SPAWNFLAG_SWORD_NOJUMPING = 8_spawnflag;
constexpr SpawnFlags SPAWNFLAG_SWORD_KNEEL = 16_spawnflag;

static cached_soundIndex s_sword1;
static cached_soundIndex s_sword2;
static cached_soundIndex s_death;
static cached_soundIndex s_pain;
static cached_soundIndex s_idle;
static cached_soundIndex s_sight;

void sword_stand(gentity_t* self);
void sword_walk(gentity_t* self);
void sword_run(gentity_t* self);

static void sword_check_dist(gentity_t* self);
static void sword_hit_left(gentity_t* self);
static void sword_set_fly_parameters(gentity_t* self);

MONSTERINFO_SIGHT(sword_sight) (gentity_t* self, gentity_t* other) -> void {
	gi.sound(self, CHAN_VOICE, s_sight, 1, ATTN_NORM, 0);
}

MONSTERINFO_SEARCH(sword_search) (gentity_t* self) -> void {
	gi.sound(self, CHAN_VOICE, s_idle, 1, ATTN_IDLE, 0);
}

static void sword_sword_sound(gentity_t* self) {
	if (frandom() <= 0.5f)
		gi.sound(self, CHAN_VOICE, s_sword1, 1, ATTN_NORM, 0);
	else
		gi.sound(self, CHAN_VOICE, s_sword2, 1, ATTN_NORM, 0);
}

static MonsterFrame sword_frames_stand[] = {
		{ ai_stand },
};
MMOVE_T(sword_move_stand) = { FRAME_stand1, FRAME_stand1, sword_frames_stand, sword_stand };

MONSTERINFO_STAND(sword_stand) (gentity_t* self) -> void {
	M_SetAnimation(self, &sword_move_stand);
}

static MonsterFrame sword_frames_walk[] = {
		{ ai_walk, 0 },
};
MMOVE_T(sword_move_walk) = { FRAME_stand1, FRAME_stand1, sword_frames_walk, sword_walk };

MONSTERINFO_WALK(sword_walk) (gentity_t* self) -> void {
	M_SetAnimation(self, &sword_move_walk);
}

static MonsterFrame sword_frames_run[] = {
		{ ai_run, 16 },
		{ ai_run, 20 },
		{ ai_run, 13 },
		{ ai_run, 7 },
		{ ai_run, 16 },
		{ ai_run, 20 },
		{ ai_run, 14 },
		{ ai_run, 6, sword_check_dist },
};
MMOVE_T(sword_move_run) = { FRAME_runb1, FRAME_runb8, sword_frames_run, nullptr };

MONSTERINFO_RUN(sword_run) (gentity_t* self) -> void {
	if (self->monsterInfo.aiFlags & AI_STAND_GROUND)
		M_SetAnimation(self, &sword_move_stand);
	else
		M_SetAnimation(self, &sword_move_run);
}

static MonsterFrame sword_frames_attack[] = {
		{ ai_charge },
		{ ai_charge },
		{ ai_charge },
		{ ai_charge },
		{ ai_charge },
		{ ai_charge },
		{ ai_charge, 0, sword_hit_left },
		{ ai_charge },
		{ ai_charge },
		{ ai_charge, 0, sword_check_dist },
};
MMOVE_T(sword_move_attack) = { FRAME_attackb1, FRAME_attackb10, sword_frames_attack, sword_run };

MONSTERINFO_MELEE(sword_melee) (gentity_t* self) -> void {
	M_SetAnimation(self, &sword_move_attack);
}

static void sword_check_dist(gentity_t* self) {
	if (!self->enemy || !self->enemy->inUse || self->enemy->health <= 0)
		return;

	if (range_to(self, self->enemy) <= RANGE_MELEE)
		M_SetAnimation(self, &sword_move_attack);
	else
		M_SetAnimation(self, &sword_move_run);
}

static void sword_hit_left(gentity_t* self) {
	Vector3 aim{ MELEE_DISTANCE, self->mins.x, 4.0f };

	if (fire_hit(self, aim, irandom(1, 9), 100))
		sword_sword_sound(self);
	else {
		sword_sword_sound(self);
		self->monsterInfo.melee_debounce_time = level.time + 1.5_sec;
	}
}

static MonsterFrame sword_frames_pain1[] = {
		{ ai_move },
		{ ai_move },
		{ ai_move },
		{ ai_move },
		{ ai_move },
		{ ai_move },
		{ ai_move },
		{ ai_move },
};
MMOVE_T(sword_move_pain1) = { FRAME_runb1, FRAME_runb8, sword_frames_pain1, sword_run };

static PAIN(sword_pain) (gentity_t* self, gentity_t* other, float kick, int damage, const MeansOfDeath& mod) -> void {
	if (level.time < self->pain_debounce_time)
		return;

	self->pain_debounce_time = level.time + 3_sec;

	if (!M_ShouldReactToPain(self, mod))
		return;

	if (frandom() < 0.5f)
		gi.sound(self, CHAN_VOICE, s_pain, 1, ATTN_NORM, 0);

	M_SetAnimation(self, &sword_move_pain1);
}

static void sword_shrink(gentity_t* self) {
	self->maxs.z = 0.0f;
	self->svFlags |= SVF_DEADMONSTER;
	gi.linkEntity(self);
}

static MonsterFrame sword_frames_death1[] = {
		{ ai_move },
		{ ai_move },
		{ ai_move },
		{ ai_move },
		{ ai_move, 0, sword_shrink },
		{ ai_move },
		{ ai_move },
		{ ai_move },
		{ ai_move },
		{ ai_move },
};
MMOVE_T(sword_move_death1) = { FRAME_death1, FRAME_death10, sword_frames_death1, monster_dead };

static MonsterFrame sword_frames_death2[] = {
		{ ai_move },
		{ ai_move },
		{ ai_move },
		{ ai_move },
		{ ai_move },
		{ ai_move, 0, sword_shrink },
		{ ai_move },
		{ ai_move },
		{ ai_move },
		{ ai_move },
		{ ai_move },
};
MMOVE_T(sword_move_death2) = { FRAME_deathb1, FRAME_deathb11, sword_frames_death2, monster_dead };

static DIE(sword_die) (gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, const Vector3& point, const MeansOfDeath& mod) -> void {
	self->s.effects &= ~EF_HYPERBLASTER;

	if (M_CheckGib(self, mod)) {
		gi.sound(self, CHAN_VOICE, gi.soundIndex("misc/udeath.wav"), 1, ATTN_NORM, 0);

		self->s.skinNum /= 2;

		ThrowGibs(self, damage, {
				{ 2, "models/objects/gibs/sm_metal/tris.md2", GIB_DEBRIS },
				{ "models/objects/gibs/sm_metal/tris.md2", GIB_DEBRIS | GIB_HEAD }
			});

		self->deadFlag = true;
		return;
	}

	if (self->deadFlag)
		return;

	gi.sound(self, CHAN_VOICE, s_death, 1, ATTN_NORM, 0);
	self->deadFlag = true;
	self->takeDamage = true;

	if (frandom() < 0.5f)
		M_SetAnimation(self, &sword_move_death1);
	else
		M_SetAnimation(self, &sword_move_death2);
}

static void sword_set_fly_parameters(gentity_t* self) {
	self->monsterInfo.fly_pinned = false;
	self->monsterInfo.fly_thrusters = true;
	self->monsterInfo.fly_position_time = 0_ms;
	self->monsterInfo.fly_acceleration = 10.0f;
	self->monsterInfo.fly_speed = 180.0f;
	self->monsterInfo.fly_min_distance = 0.0f;
	self->monsterInfo.fly_max_distance = 10.0f;
}

void SP_monster_sword(gentity_t* self) {
	if (!M_AllowSpawn(self)) {
		FreeEntity(self);
		return;
	}

	self->moveType = MoveType::Step;
	self->solid = SOLID_BBOX;

	self->s.modelIndex = gi.modelIndex("models/monsters/sword/tris.md2");
	self->mins = { -16.0f, -16.0f, -24.0f };
	self->maxs = { 16.0f, 16.0f, 40.0f };

	self->health = self->maxHealth = static_cast<int>(200 * st.health_multiplier);
	self->gibHealth = -80;
	self->mass = 120;

	self->pain = sword_pain;
	self->die = sword_die;

	s_sword1.assign("sword/sword1.wav");
	s_sword2.assign("sword/sword2.wav");
	s_death.assign("sword/kdeath.wav");
	s_pain.assign("sword/khurt.wav");
	s_idle.assign("sword/idle.wav");
	s_sight.assign("sword/ksight.wav");

	self->monsterInfo.stand = sword_stand;
	self->monsterInfo.walk = sword_walk;
	self->monsterInfo.run = sword_run;
	self->monsterInfo.dodge = M_MonsterDodge;
	self->monsterInfo.attack = nullptr;
	self->monsterInfo.melee = sword_melee;
	self->monsterInfo.sight = sword_sight;
	self->monsterInfo.search = sword_search;

	M_SetAnimation(self, &sword_move_stand);

	self->monsterInfo.combatStyle = CombatStyle::Melee;
	self->monsterInfo.scale = MODEL_SCALE;
	self->monsterInfo.canJump = !self->spawnFlags.has(SPAWNFLAG_SWORD_NOJUMPING);

	self->monsterInfo.aiFlags |= AI_ALTERNATE_FLY;
	sword_set_fly_parameters(self);

	if (self->spawnFlags.has(SPAWNFLAG_SWORD_NOJUMPING))
		self->monsterInfo.fly_thrusters = false;

	if (self->spawnFlags.has(SPAWNFLAG_SWORD_KNEEL)) {
		self->monsterInfo.fly_pinned = true;
		self->monsterInfo.fly_position_time = level.time + 1_sec;
	}
	gi.linkEntity(self);
	flymonster_start(self);
}
