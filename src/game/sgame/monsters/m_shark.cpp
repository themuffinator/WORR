/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

============================================================================== FLIPPER
==============================================================================*/

#include "../g_local.hpp"
#include "m_shark.hpp"

static cached_soundIndex sound_chomp;
static cached_soundIndex sound_attack;
static cached_soundIndex sound_pain1;
static cached_soundIndex sound_pain2;
static cached_soundIndex sound_death;
static cached_soundIndex sound_idle;
static cached_soundIndex sound_search;
static cached_soundIndex sound_sight;

MonsterFrame flipper_frames_stand[] = {
	{ ai_stand }
};

MMOVE_T(flipper_move_stand) = { FRAME_flphor01, FRAME_flphor01, flipper_frames_stand, nullptr };

MONSTERINFO_STAND(flipper_stand) (gentity_t *self) -> void {
	M_SetAnimation(self, &flipper_move_stand);
}

constexpr float FLIPPER_RUN_SPEED = 24;

MonsterFrame flipper_frames_run[] = {
	{ ai_run, FLIPPER_RUN_SPEED }, // 6
	{ ai_run, FLIPPER_RUN_SPEED },
	{ ai_run, FLIPPER_RUN_SPEED },
	{ ai_run, FLIPPER_RUN_SPEED },
	{ ai_run, FLIPPER_RUN_SPEED }, // 10

	{ ai_run, FLIPPER_RUN_SPEED },
	{ ai_run, FLIPPER_RUN_SPEED },
	{ ai_run, FLIPPER_RUN_SPEED },
	{ ai_run, FLIPPER_RUN_SPEED },
	{ ai_run, FLIPPER_RUN_SPEED },
	{ ai_run, FLIPPER_RUN_SPEED },
	{ ai_run, FLIPPER_RUN_SPEED },
	{ ai_run, FLIPPER_RUN_SPEED },
	{ ai_run, FLIPPER_RUN_SPEED },
	{ ai_run, FLIPPER_RUN_SPEED }, // 20

	{ ai_run, FLIPPER_RUN_SPEED },
	{ ai_run, FLIPPER_RUN_SPEED },
	{ ai_run, FLIPPER_RUN_SPEED },
	{ ai_run, FLIPPER_RUN_SPEED },
	{ ai_run, FLIPPER_RUN_SPEED },
	{ ai_run, FLIPPER_RUN_SPEED },
	{ ai_run, FLIPPER_RUN_SPEED },
	{ ai_run, FLIPPER_RUN_SPEED },
	{ ai_run, FLIPPER_RUN_SPEED } // 29
};
MMOVE_T(flipper_move_run_loop) = { FRAME_flpver06, FRAME_flpver29, flipper_frames_run, nullptr };

static void flipper_run_loop(gentity_t *self) {
	M_SetAnimation(self, &flipper_move_run_loop);
}

MonsterFrame flipper_frames_run_start[] = {
	{ ai_run, 8 },
	{ ai_run, 8 },
	{ ai_run, 8 },
	{ ai_run, 8 },
	{ ai_run, 8 },
	{ ai_run, 8 }
};
MMOVE_T(flipper_move_run_start) = { FRAME_flpver01, FRAME_flpver06, flipper_frames_run_start, flipper_run_loop };

static void flipper_run(gentity_t *self) {
	M_SetAnimation(self, &flipper_move_run_start);
}

/* Standard Swimming */
MonsterFrame flipper_frames_walk[] = {
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
	{ ai_walk, 4 },
	{ ai_walk, 4 },
	{ ai_walk, 4 },
	{ ai_walk, 4 },
	{ ai_walk, 4 },
	{ ai_walk, 4 },
	{ ai_walk, 4 },
	{ ai_walk, 4 }
};
MMOVE_T(flipper_move_walk) = { FRAME_flphor01, FRAME_flphor24, flipper_frames_walk, nullptr };

MONSTERINFO_WALK(flipper_walk) (gentity_t *self) -> void {
	M_SetAnimation(self, &flipper_move_walk);
}

MonsterFrame flipper_frames_start_run[] = {
	{ ai_run },
	{ ai_run },
	{ ai_run },
	{ ai_run },
	{ ai_run, 8, flipper_run }
};
MMOVE_T(flipper_move_start_run) = { FRAME_flphor01, FRAME_flphor05, flipper_frames_start_run, nullptr };

MONSTERINFO_RUN(flipper_start_run) (gentity_t *self) -> void {
	M_SetAnimation(self, &flipper_move_start_run);
}

MonsterFrame flipper_frames_pain2[] = {
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move }
};
MMOVE_T(flipper_move_pain2) = { FRAME_flppn101, FRAME_flppn105, flipper_frames_pain2, flipper_run };

MonsterFrame flipper_frames_pain1[] = {
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move }
};
MMOVE_T(flipper_move_pain1) = { FRAME_flppn201, FRAME_flppn205, flipper_frames_pain1, flipper_run };

static void flipper_bite(gentity_t *self) {
	Vector3 aim = { MELEE_DISTANCE, 0, 0 };
	fire_hit(self, aim, 5, 0);
}

static void flipper_preattack(gentity_t *self) {
	gi.sound(self, CHAN_WEAPON, sound_chomp, 1, ATTN_NORM, 0);
}

MonsterFrame flipper_frames_attack[] = {
	{ ai_charge, 0, flipper_preattack },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge, 0, flipper_bite },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge, 0, flipper_bite },
	{ ai_charge }
};
MMOVE_T(flipper_move_attack) = { FRAME_flpbit01, FRAME_flpbit20, flipper_frames_attack, flipper_run };

MONSTERINFO_MELEE(flipper_melee) (gentity_t *self) -> void {
	M_SetAnimation(self, &flipper_move_attack);
}

static PAIN(flipper_pain) (gentity_t *self, gentity_t *other, float kick, int damage, const MeansOfDeath &mod) -> void {
	if (level.time < self->pain_debounce_time)
		return;

	self->pain_debounce_time = level.time + 3_sec;
	int n = brandom();

	if (n == 0)
		gi.sound(self, CHAN_VOICE, sound_pain1, 1, ATTN_NORM, 0);
	else
		gi.sound(self, CHAN_VOICE, sound_pain2, 1, ATTN_NORM, 0);

	if (!M_ShouldReactToPain(self, mod))
		return; // no pain anims in nightmare

	M_SetAnimation(self, n == 0 ? &flipper_move_pain1 : &flipper_move_pain2);
}

MONSTERINFO_SETSKIN(flipper_setskin) (gentity_t *self) -> void {
	if (self->health < (self->maxHealth / 2))
		self->s.skinNum = 1;
	else
		self->s.skinNum = 0;
}

static void flipper_dead(gentity_t *self) {
	self->mins = { -16, -16, -8 };
	self->maxs = { 16, 16, 8 };
	monster_dead(self);
}

MonsterFrame flipper_frames_death[] = {
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
MMOVE_T(flipper_move_death) = { FRAME_flpdth01, FRAME_flpdth56, flipper_frames_death, flipper_dead };

MONSTERINFO_SIGHT(flipper_sight) (gentity_t *self, gentity_t *other) -> void {
	gi.sound(self, CHAN_VOICE, sound_sight, 1, ATTN_NORM, 0);
}

static DIE(flipper_die) (gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, const Vector3 &point, const MeansOfDeath &mod) -> void {
	// check for gib
	if (M_CheckGib(self, mod)) {
		gi.sound(self, CHAN_VOICE, gi.soundIndex("misc/udeath.wav"), 1, ATTN_NORM, 0);
		ThrowGibs(self, damage, {
			{ 2, "models/objects/gibs/bone/tris.md2" },
			{ 2, "models/objects/gibs/sm_meat/tris.md2" },
			{ "models/objects/gibs/head2/tris.md2", GIB_HEAD }
			});
		self->deadFlag = true;
		return;
	}

	if (self->deadFlag)
		return;

	// regular death
	gi.sound(self, CHAN_VOICE, sound_death, 1, ATTN_NORM, 0);
	self->deadFlag = true;
	self->takeDamage = true;
	self->svFlags |= SVF_DEADMONSTER;
	M_SetAnimation(self, &flipper_move_death);
}

static void flipper_set_fly_parameters(gentity_t *self) {
	self->monsterInfo.fly_thrusters = false;
	self->monsterInfo.fly_acceleration = 30.f;
	self->monsterInfo.fly_speed = 110.f;
	// only melee, so get in close
	self->monsterInfo.fly_min_distance = 10.f;
	self->monsterInfo.fly_max_distance = 10.f;
}

/*QUAKED monster_flipper (1 .5 0) (-16 -16 -24) (16 16 32) AMBUSH TRIGGER_SPAWN SIGHT x CORPSE x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
 */
void SP_monster_flipper(gentity_t *self) {
	if (!M_AllowSpawn(self)) {
		FreeEntity(self);
		return;
	}

	sound_pain1.assign("flipper/flppain1.wav");
	sound_pain2.assign("flipper/flppain2.wav");
	sound_death.assign("flipper/flpdeth1.wav");
	sound_chomp.assign("flipper/flpatck1.wav");
	sound_attack.assign("flipper/flpatck2.wav");
	sound_idle.assign("flipper/flpidle1.wav");
	sound_search.assign("flipper/flpsrch1.wav");
	sound_sight.assign("flipper/flpsght1.wav");

	self->moveType = MoveType::Step;
	self->solid = SOLID_BBOX;
	self->s.modelIndex = gi.modelIndex("models/monsters/flipper/tris.md2");
	self->mins = { -16, -16, -8 };
	self->maxs = { 16, 16, 20 };

	self->health = 50 * st.health_multiplier;
	self->gibHealth = -30;
	self->mass = 100;

	self->pain = flipper_pain;
	self->die = flipper_die;

	self->monsterInfo.stand = flipper_stand;
	self->monsterInfo.walk = flipper_walk;
	self->monsterInfo.run = flipper_start_run;
	self->monsterInfo.melee = flipper_melee;
	self->monsterInfo.sight = flipper_sight;
	self->monsterInfo.setSkin = flipper_setskin;

	gi.linkEntity(self);

	M_SetAnimation(self, &flipper_move_stand);
	self->monsterInfo.scale = MODEL_SCALE;

	self->monsterInfo.aiFlags |= AI_ALTERNATE_FLY;
	flipper_set_fly_parameters(self);

	swimmonster_start(self);
}
