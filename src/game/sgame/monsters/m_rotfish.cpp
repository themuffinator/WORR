/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

============================================================================== FISH (inspired by
FLIPPER structure) - Swim monster (water-native): walk/run are swimming speeds - Melee bite only
- Idle/search/sight barks - Pain and death sets - Swim (fly) parameters tuned for close-range
harassment ==============================================================================*/

#include "../g_local.hpp"
#include "m_rotfish.hpp"

/*
===============
Sound indices
===============
*/
static cached_soundIndex snd_chomp;
static cached_soundIndex snd_death;
static cached_soundIndex snd_idle;

/*
===============
fish_stand
===============
*/
static MonsterFrame fish_frames_stand[] = {
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
MMOVE_T(fish_move_stand) = { FRAME_swim01, FRAME_swim18, fish_frames_stand, nullptr };

MONSTERINFO_STAND(fish_stand) (gentity_t* self) -> void {
	M_SetAnimation(self, &fish_move_stand);
}

/*
===============
fish_swim (walk)
Standard swimming loop at patrol speed
===============
*/
constexpr float FISH_SWIM_SPEED = 8.0f;

static MonsterFrame fish_frames_swim[] = {
	{ ai_walk, FISH_SWIM_SPEED },
	{ ai_walk, FISH_SWIM_SPEED },
	{ ai_walk, FISH_SWIM_SPEED },
	{ ai_walk, FISH_SWIM_SPEED },
	{ ai_walk, FISH_SWIM_SPEED },
	{ ai_walk, FISH_SWIM_SPEED },
	{ ai_walk, FISH_SWIM_SPEED },
	{ ai_walk, FISH_SWIM_SPEED },
	{ ai_walk, FISH_SWIM_SPEED },
	{ ai_walk, FISH_SWIM_SPEED },
	{ ai_walk, FISH_SWIM_SPEED },
	{ ai_walk, FISH_SWIM_SPEED },
	{ ai_walk, FISH_SWIM_SPEED },
	{ ai_walk, FISH_SWIM_SPEED },
	{ ai_walk, FISH_SWIM_SPEED },
	{ ai_walk, FISH_SWIM_SPEED },
	{ ai_walk, FISH_SWIM_SPEED },
	{ ai_walk, FISH_SWIM_SPEED }
};
MMOVE_T(fish_move_swim) = { FRAME_swim01, FRAME_swim18, fish_frames_swim, nullptr };

MONSTERINFO_WALK(fish_walk) (gentity_t* self) -> void {
	M_SetAnimation(self, &fish_move_swim);
}

/*
===============
fish_run
Faster swim loop when aggroed
===============
*/
constexpr float FISH_RUN_SPEED = 12.0f;

static MonsterFrame fish_frames_run[] = {
	{ ai_run, FISH_RUN_SPEED },
	{ ai_run, FISH_RUN_SPEED },
	{ ai_run, FISH_RUN_SPEED },
	{ ai_run, FISH_RUN_SPEED },
	{ ai_run, FISH_RUN_SPEED },
	{ ai_run, FISH_RUN_SPEED },
	{ ai_run, FISH_RUN_SPEED },
	{ ai_run, FISH_RUN_SPEED },
	{ ai_run, FISH_RUN_SPEED },
	{ ai_run, FISH_RUN_SPEED },
	{ ai_run, FISH_RUN_SPEED },
	{ ai_run, FISH_RUN_SPEED },
	{ ai_run, FISH_RUN_SPEED },
	{ ai_run, FISH_RUN_SPEED },
	{ ai_run, FISH_RUN_SPEED },
	{ ai_run, FISH_RUN_SPEED },
	{ ai_run, FISH_RUN_SPEED },
	{ ai_run, FISH_RUN_SPEED }
};
MMOVE_T(fish_move_run) = { FRAME_swim01, FRAME_swim18, fish_frames_run, nullptr };

MONSTERINFO_RUN(fish_run) (gentity_t* self) -> void {
	M_SetAnimation(self, &fish_move_run);
}

/*
===============
fish_bite
===============
*/
static void fish_bite(gentity_t* self) {
	gi.sound(self, CHAN_WEAPON, snd_chomp, 1, ATTN_NORM, 0);
	Vector3 aim = { MELEE_DISTANCE, 0, 0 };
	// light hit, no knockback
	fire_hit(self, aim, 5, 0);
}

/*
===============
fish_melee
===============
*/
constexpr float FISH_ATTACK_SPEED = 10.0f;

static MonsterFrame fish_frames_attack[] = {
	{ ai_charge, FISH_ATTACK_SPEED },
	{ ai_charge, FISH_ATTACK_SPEED },
	{ ai_charge, 0, fish_bite },
	{ ai_charge, FISH_ATTACK_SPEED },
	{ ai_charge, FISH_ATTACK_SPEED },
	{ ai_charge, FISH_ATTACK_SPEED },
	{ ai_charge, FISH_ATTACK_SPEED },
	{ ai_charge, FISH_ATTACK_SPEED },
	{ ai_charge, 0, fish_bite },
	{ ai_charge, FISH_ATTACK_SPEED },
	{ ai_charge, FISH_ATTACK_SPEED },
	{ ai_charge, FISH_ATTACK_SPEED },
	{ ai_charge, FISH_ATTACK_SPEED },
	{ ai_charge, FISH_ATTACK_SPEED },
	{ ai_charge, 0, fish_bite },
	{ ai_charge, FISH_ATTACK_SPEED },
	{ ai_charge, FISH_ATTACK_SPEED },
	{ ai_charge, FISH_ATTACK_SPEED }
};
MMOVE_T(fish_move_attack) = { FRAME_bite01, FRAME_bite18, fish_frames_attack, fish_run };

MONSTERINFO_MELEE(fish_melee) (gentity_t* self) -> void {
	M_SetAnimation(self, &fish_move_attack);
}

/*
===============
fish_pain
===============
*/
static MonsterFrame fish_frames_pain[] = {
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
MMOVE_T(fish_move_pain) = { FRAME_pain01, FRAME_pain09, fish_frames_pain, fish_run };

static PAIN(fish_pain) (gentity_t* self, gentity_t* other, float kick, int damage, const MeansOfDeath& mod) -> void {
	if (level.time < self->pain_debounce_time)
		return;

	self->pain_debounce_time = level.time + 1.0_sec;

	gi.sound(self, CHAN_VOICE, snd_death, 1, ATTN_NORM, 0);

	if (!M_ShouldReactToPain(self, mod))
		return; // no pain anims in nightmare

	M_SetAnimation(self, &fish_move_pain);
}

/*
===============
fish_setskin
===============
*/
MONSTERINFO_SETSKIN(fish_setskin) (gentity_t* self) -> void {
	if (self->health < (self->maxHealth / 2))
		self->s.skinNum |= 1;
	else
		self->s.skinNum &= ~1;
}

/*
===============
fish_dead
===============
*/
static void fish_dead(gentity_t* self) {
	self->mins = { -12, -12, -6 };
	self->maxs = { 12,  12,  6 };
	monster_dead(self);
}

/*
===============
fish_die
===============
*/
static MonsterFrame fish_frames_death[] = {
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
MMOVE_T(fish_move_death) = { FRAME_death01, FRAME_death21, fish_frames_death, fish_dead };

static DIE(fish_die) (gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, const Vector3& point, const MeansOfDeath& mod) -> void {
	// gibbing
	if (M_CheckGib(self, mod)) {
		gi.sound(self, CHAN_VOICE, gi.soundIndex("misc/udeath.wav"), 1, ATTN_NORM, 0);
		ThrowGibs(self, damage, {
			{ 1, "models/objects/gibs/bone/tris.md2" },
			{ 2, "models/objects/gibs/sm_meat/tris.md2" },
			{ "models/objects/gibs/head2/tris.md2", GIB_HEAD }
			});
		self->deadFlag = true;
		return;
	}

	if (self->deadFlag)
		return;

	gi.sound(self, CHAN_VOICE, snd_death, 1, ATTN_NORM, 0);
	self->deadFlag = true;
	self->takeDamage = true;
	self->svFlags |= SVF_DEADMONSTER;

	M_SetAnimation(self, &fish_move_death);
}

/*
===============
fish_sight
===============
*/
MONSTERINFO_SIGHT(fish_sight) (gentity_t* self, gentity_t* other) -> void {
	gi.sound(self, CHAN_VOICE, snd_idle, 1, ATTN_NORM, 0);
}

/*
===============
fish_search
===============
*/
MONSTERINFO_SEARCH(fish_search) (gentity_t* self) -> void {
	gi.sound(self, CHAN_VOICE, snd_idle, 1, ATTN_IDLE, 0);
}

/*
===============
fish_idle
===============
*/
MONSTERINFO_IDLE(fish_idle) (gentity_t* self) -> void {
	if (frandom() < 0.5f)
		gi.sound(self, CHAN_VOICE, snd_idle, 1, ATTN_IDLE, 0);
}

/*
===============
fish_set_swim_parameters
===============
*/
static void fish_set_swim_parameters(gentity_t* self) {
	self->monsterInfo.fly_thrusters = false;
	self->monsterInfo.fly_acceleration = 30.0f;
	self->monsterInfo.fly_speed = 110.0f;

	// melee only: press the target
	self->monsterInfo.fly_min_distance = 10.0f;
	self->monsterInfo.fly_max_distance = 10.0f;
}

/*
===============
SP_monster_fish
===============
*/
/*QUAKED monster_fish (1 .5 0) (-12 -12 -8) (12 12 16) AMBUSH TRIGGER_SPAWN SIGHT x CORPSE x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/monsters/fish/tris.md2"
*/
void SP_monster_fish(gentity_t* self) {
	if (!M_AllowSpawn(self)) {
		FreeEntity(self);
		return;
	}

	// sounds
	snd_death.assign("fish/death.wav");
	snd_chomp.assign("fish/bite.wav");
	snd_idle.assign("fish/idle.wav");

	// model and bbox
	self->moveType = MoveType::Step;
	self->solid = SOLID_BBOX;
	self->s.modelIndex = gi.modelIndex("models/monsters/fish/tris.md2");
	self->mins = { -12, -12, -8 };
	self->maxs = { 12,  12, 16 };

	// stats
	self->health = 40 * st.health_multiplier;
	self->gibHealth = -20;
	self->mass = 100;

	// callbacks
	self->pain = fish_pain;
	self->die = fish_die;

	self->monsterInfo.stand = fish_stand;
	self->monsterInfo.walk = fish_walk;
	self->monsterInfo.run = fish_run;
	self->monsterInfo.melee = fish_melee;
	self->monsterInfo.sight = fish_sight;
	self->monsterInfo.search = fish_search;
	self->monsterInfo.idle = fish_idle;
	self->monsterInfo.setSkin = fish_setskin;

	gi.linkEntity(self);

	M_SetAnimation(self, &fish_move_stand);
	self->monsterInfo.scale = MODEL_SCALE;

	// swim monster
	self->monsterInfo.aiFlags |= AI_ALTERNATE_FLY;
	fish_set_swim_parameters(self);

	swimmonster_start(self);
}
