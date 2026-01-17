/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

m_hellknight.cpp implementation.*/

#include "../g_local.hpp"
#include "m_hellknight.hpp"
#include "m_flash.hpp"
#include "q1_support.hpp"

// Sounds used by the Hell Knight
static cached_soundIndex s_idle;
static cached_soundIndex s_sight;
static cached_soundIndex s_pain;
static cached_soundIndex s_death;
static cached_soundIndex s_slash;
static cached_soundIndex s_magic;
static cached_soundIndex s_sword1;
static cached_soundIndex s_sword2;

// Constants defining the Hell Knight's properties
static constexpr Vector3 HK_MINS = { -16.0f, -16.0f, -24.0f };
static constexpr Vector3 HK_MAXS = { 16.0f,  16.0f,  40.0f };
static constexpr int     HK_HEALTH = 250;
static constexpr int     HK_GIBHEALTH = -40; // Corrected to match QC source
static constexpr int     HK_MASS = 250;
static constexpr Vector3 HK_CAST_OFFSET = { 20.0f, 0.0f, 16.0f }; // Projectile spawn offset
static constexpr int     HK_FLAME_DAMAGE = 15;
static constexpr int     HK_FLAME_SPEED = 600;

// Forward declarations for AI state functions
void hk_run(gentity_t* self);

//
// HELPERS
// These are small, reusable functions called during animation frames.
//

static void hk_idle(gentity_t* self) {
	if (frandom() < 0.2f)
		gi.sound(self, CHAN_VOICE, s_idle, 1, ATTN_IDLE, 0);
}

static void hk_step(gentity_t* self) {
	monster_footstep(self);
}

MONSTERINFO_SIGHT(hk_sight) (gentity_t* self, gentity_t* other) -> void {
	gi.sound(self, CHAN_VOICE, s_sight, 1, ATTN_NORM, 0);
}

static void hk_melee_swing(gentity_t* self) {
	gi.sound(self, CHAN_WEAPON, s_slash, 1, ATTN_NORM, 0);
}

static void hk_charge_swing(gentity_t* self) {
	if (brandom())
		gi.sound(self, CHAN_WEAPON, s_sword1, 1, ATTN_NORM, 0);
	else
		gi.sound(self, CHAN_WEAPON, s_sword2, 1, ATTN_NORM, 0);
}

// Generic melee damage function for slice/smash attacks
static void hk_melee_damage(gentity_t* self) {
	Vector3 aim = { MELEE_DISTANCE, 0, 8.0f };
	fire_hit(self, aim, irandom(15, 25), 100);
}

// Melee damage for the charge attack
static void hk_charge_damage(gentity_t* self) {
	Vector3 aim = { MELEE_DISTANCE, 0, 8.0f };
	fire_hit(self, aim, irandom(20, 30), 150);
}

// Core function to fire a single magic spike
static void hk_fire_spike_core(gentity_t* self, int yawStep) {
	if (!self->enemy || !self->enemy->inUse)
		return;

	// Aiming logic ported directly from QuakeC source
	Vector3 toEnemy = self->enemy->s.origin - self->s.origin;
	Vector3 ang = VectorToAngles(toEnemy);
	ang[1] += yawStep * 6.0f;

	Vector3 fwd, right;
	AngleVectors(ang, fwd, right, nullptr);
	Vector3 start = M_ProjectFlashSource(self, HK_CAST_OFFSET, fwd, right);

	Vector3 dir = fwd;
	dir[2] = -dir[2] + (frandom() - 0.5f) * 0.1f; // Mimic QuakeC vertical adjustment
	dir.normalize();

	gi.sound(self, CHAN_WEAPON, s_magic, 1, ATTN_NORM, 0);
	// Fire the Quake 1-style flame bolt
	monster_muzzleflash(self, start, MZ2_FLYER_BLASTER_1);
	[[maybe_unused]] gentity_t* projectile =
		fire_flame(self, start, dir, HK_FLAME_DAMAGE, HK_FLAME_SPEED, ModID::IonRipper);
}

// Wrapper functions for different projectile spread angles, creating a volley effect
static void hk_fire_spike_m2(gentity_t* self) { hk_fire_spike_core(self, -2); }
static void hk_fire_spike_m1(gentity_t* self) { hk_fire_spike_core(self, -1); }
static void hk_fire_spike_0(gentity_t* self) { hk_fire_spike_core(self, 0); }
static void hk_fire_spike_p1(gentity_t* self) { hk_fire_spike_core(self, 1); }
static void hk_fire_spike_p2(gentity_t* self) { hk_fire_spike_core(self, 2); }
static void hk_fire_spike_p3(gentity_t* self) { hk_fire_spike_core(self, 3); }

//
// ANIMATION SEQUENCES
// Each MMOVE_T defines a sequence of frames and the function to call upon completion.
//

// Ranged attack volley "A"
static MonsterFrame frames_magica[] = {
	{ ai_stand }, { ai_stand }, { ai_stand }, { ai_stand }, { ai_stand }, { ai_stand },
	{ ai_stand, 0, hk_fire_spike_m2 }, { ai_stand, 0, hk_fire_spike_m1 }, { ai_stand, 0, hk_fire_spike_0  },
	{ ai_stand, 0, hk_fire_spike_p1 }, { ai_stand, 0, hk_fire_spike_p2 }, { ai_stand, 0, hk_fire_spike_p3 },
	{ ai_stand }, { ai_stand }
};
MMOVE_T(hellknight_move_magica) = { FRAME_magica1, FRAME_magica14, frames_magica, hk_run };

// Ranged attack volley "B"
static MonsterFrame frames_magicb[] = {
	{ ai_stand }, { ai_stand }, { ai_stand }, { ai_stand }, { ai_stand }, { ai_stand },
	{ ai_stand, 0, hk_fire_spike_m2 }, { ai_stand, 0, hk_fire_spike_m1 }, { ai_stand, 0, hk_fire_spike_0  },
	{ ai_stand, 0, hk_fire_spike_p1 }, { ai_stand, 0, hk_fire_spike_p2 }, { ai_stand, 0, hk_fire_spike_p3 },
	{ ai_stand }
};
MMOVE_T(hellknight_move_magicb) = { FRAME_magicb1, FRAME_magicb13, frames_magicb, hk_run };

// Ranged attack volley "C"
static MonsterFrame frames_magicc[] = {
	{ ai_stand }, { ai_stand }, { ai_stand }, { ai_stand }, { ai_stand },
	{ ai_stand, 0, hk_fire_spike_m2 }, { ai_stand, 0, hk_fire_spike_m1 }, { ai_stand, 0, hk_fire_spike_0  },
	{ ai_stand, 0, hk_fire_spike_p1 }, { ai_stand, 0, hk_fire_spike_p2 }, { ai_stand, 0, hk_fire_spike_p3 }
};
MMOVE_T(hellknight_move_magicc) = { FRAME_magicc1, FRAME_magicc11, frames_magicc, hk_run };

// Melee attack: Slice
static MonsterFrame frames_slice[] = {
	{ ai_charge, 9 }, { ai_charge, 6 }, { ai_charge, 13 }, { ai_charge, 4, hk_melee_swing }, { ai_charge, 7, hk_melee_damage },
	{ ai_charge, 15, hk_melee_damage }, { ai_charge, 8, hk_melee_damage }, { ai_charge, 2, hk_melee_damage },
	{ ai_charge, 0, hk_melee_damage }, { ai_charge, 3 }
};
MMOVE_T(hellknight_move_slice) = { FRAME_slice1, FRAME_slice10, frames_slice, hk_run };

// Melee attack: Smash
static MonsterFrame frames_smash[] = {
	{ ai_charge, 1 }, { ai_charge, 13 }, { ai_charge, 9 }, { ai_charge, 11, hk_melee_swing }, { ai_charge, 10, hk_melee_damage },
	{ ai_charge, 7, hk_melee_damage }, { ai_charge, 12, hk_melee_damage }, { ai_charge, 2, hk_melee_damage },
	{ ai_charge, 3, hk_melee_damage }, { ai_charge, 0 }, { ai_charge, 0 }
};
MMOVE_T(hellknight_move_smash) = { FRAME_smash1, FRAME_smash11, frames_smash, hk_run };

// Charge Attack
static MonsterFrame frames_charge[] = {
	{ ai_charge, 20 }, { ai_charge, 25 }, { ai_charge, 18, hk_charge_swing }, { ai_charge, 16 }, { ai_charge, 14 },
	{ ai_charge, 20, hk_charge_damage }, { ai_charge, 21, hk_charge_damage }, { ai_charge, 13, hk_charge_damage },
	{ ai_charge, 20, hk_charge_damage }, { ai_charge, 20, hk_charge_damage }, { ai_charge, 18, hk_charge_damage },
	{ ai_charge, 16 }, { ai_charge, 14 }, { ai_charge, 25 }, { ai_charge, 21 }, { ai_charge, 13 }
};
MMOVE_T(hellknight_move_charge) = { FRAME_char_a1, FRAME_char_a16, frames_charge, hk_run };

//
// AI BEHAVIORS
// These functions define the monster's actions and decisions.
//

// Ranged attack: Randomly selects one of the three magic volley animations.
MONSTERINFO_ATTACK(hk_attack) (gentity_t* self) -> void {
	const int r = irandom(3);
	if (r == 0)      M_SetAnimation(self, &hellknight_move_magica);
	else if (r == 1) M_SetAnimation(self, &hellknight_move_magicb);
	else             M_SetAnimation(self, &hellknight_move_magicc);
}

// Melee attack: Cycles between two different close-quarters attacks.
MONSTERINFO_MELEE(hk_melee) (gentity_t* self) -> void {
	static int melee_type = 0;

	if (melee_type == 0) M_SetAnimation(self, &hellknight_move_slice);
	else M_SetAnimation(self, &hellknight_move_smash);

	melee_type = (melee_type + 1) % 2; // Cycle between slice and smash
}

// Stand state: Plays idle animations and looks for targets.
void hk_stand(gentity_t* self);
static MonsterFrame frames_stand[] = {
	{ ai_stand, 0, hk_idle }, { ai_stand }, { ai_stand }, { ai_stand }, { ai_stand },
	{ ai_stand }, { ai_stand }, { ai_stand }, { ai_stand }
};
MMOVE_T(hellknight_move_stand) = { FRAME_hk_stand1, FRAME_hk_stand9, frames_stand, hk_stand };
MONSTERINFO_STAND(hk_stand) (gentity_t* self) -> void {
	M_SetAnimation(self, &hellknight_move_stand);
}

// Walk state: Moves the monster along its path.
void hk_walk(gentity_t* self);
static MonsterFrame frames_walk[] = {
	{ ai_walk, 2 }, { ai_walk, 5 }, { ai_walk, 5, hk_step }, { ai_walk, 4 },
	{ ai_walk, 4 }, { ai_walk, 2 }, { ai_walk, 2, hk_step }, { ai_walk, 3 },
	{ ai_walk, 3 }, { ai_walk, 4 }, { ai_walk, 3, hk_step }, { ai_walk, 4 },
	{ ai_walk, 6 }, { ai_walk, 2 }, { ai_walk, 2, hk_step }, { ai_walk, 4 },
	{ ai_walk, 3 }, { ai_walk, 3 }, { ai_walk, 3, hk_step }, { ai_walk, 2 }
};
MMOVE_T(hellknight_move_walk) = { FRAME_hk_walk1, FRAME_hk_walk20, frames_walk, hk_walk };
MONSTERINFO_WALK(hk_walk) (gentity_t* self) -> void {
	M_SetAnimation(self, &hellknight_move_walk);
}

// Run state: Chases the enemy and decides whether to charge.
static MonsterFrame frames_run[] = {
	{ ai_run, 20, hk_step }, { ai_run, 25 }, { ai_run, 18, hk_step }, { ai_run, 16 },
	{ ai_run, 14 }, { ai_run, 25, hk_step }, { ai_run, 21 }, { ai_run, 13 }
};
MMOVE_T(hellknight_move_run) = { FRAME_hk_run1, FRAME_hk_run8, frames_run, nullptr };
MONSTERINFO_RUN(hk_run) (gentity_t* self) -> void {
	if (self->monsterInfo.aiFlags & AI_STAND_GROUND) {
		M_SetAnimation(self, &hellknight_move_stand);
		return;
	}

	// Check if conditions are right for a charge attack
	if (self->enemy && self->enemy->inUse && visible(self, self->enemy)) {
		float dist = (self->s.origin - self->enemy->s.origin).length();
		if ((dist > 80) && (dist < 300) && (fabs(self->s.origin[_Z] - self->enemy->s.origin[_Z]) <= 20)) {
			self->monsterInfo.attackFinished = level.time + 2_sec;
			M_SetAnimation(self, &hellknight_move_charge);
			return;
		}
	}

	M_SetAnimation(self, &hellknight_move_run);
}

// Pain state: Reacts to taking damage.
static MonsterFrame frames_pain[] = {
	{ ai_move }, { ai_move }, { ai_move }, { ai_move }, { ai_move }
};
MMOVE_T(hellknight_move_pain) = { FRAME_hk_pain1, FRAME_hk_pain5, frames_pain, hk_run };
static PAIN(hk_pain) (gentity_t* self, gentity_t* other, float kick, int damage, const MeansOfDeath& mod) -> void {
	if (level.time < self->pain_debounce_time)
		return;

	self->pain_debounce_time = level.time + 1_sec;
	gi.sound(self, CHAN_VOICE, s_pain, 1, ATTN_NORM, 0);

	if (!M_ShouldReactToPain(self, mod) || (frandom() * 30 > damage))
		return;

	M_SetAnimation(self, &hellknight_move_pain);
}

MONSTERINFO_SETSKIN(hk_setskin) (gentity_t* self) -> void {
	if (self->health < (self->maxHealth / 2))
		self->s.skinNum |= 1;
	else
		self->s.skinNum &= ~1;
}

// Death state: Handles dying and gibbing.
static void hk_dead(gentity_t* self) {
	self->mins = { -16, -16, -24 };
	self->maxs = { 16,  16, -8 };
	monster_dead(self);
}

static void hk_shrink(gentity_t* self) {
	self->svFlags |= SVF_DEADMONSTER;
	self->maxs[2] = 0;
	gi.linkEntity(self);
}

static MonsterFrame frames_deatha[] = {
	{ ai_move, 10 }, { ai_move, 8 }, { ai_move, 7 }, { ai_move }, { ai_move }, { ai_move },
	{ ai_move }, { ai_move, 10 }, { ai_move, 11, hk_shrink }, { ai_move }, { ai_move }
};
MMOVE_T(hellknight_move_deatha) = { FRAME_hk_deatha1, FRAME_hk_deatha11, frames_deatha, hk_dead };

static MonsterFrame frames_deathb[] = {
	{ ai_move }, { ai_move }, { ai_move }, { ai_move },
	{ ai_move, 0, hk_shrink }, { ai_move }, { ai_move }, { ai_move }, { ai_move }
};
MMOVE_T(hellknight_move_deathb) = { FRAME_hk_deathb1, FRAME_hk_deathb9, frames_deathb, hk_dead };

static DIE(hk_die) (gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, const Vector3& point, const MeansOfDeath& mod) -> void {
	if (M_CheckGib(self, mod)) {
		gi.sound(self, CHAN_VOICE, gi.soundIndex("misc/udeath.wav"), 1, ATTN_NORM, 0);
		self->s.skinNum &= ~1;
		ThrowGibs(self, damage, {
			{ 2, "models/objects/gibs/bone/tris.md2" },
			{ 3, "models/objects/gibs/sm_meat/tris.md2" },
			{ 1, "models/objects/gibs/gear/tris.md2" },
			});
		self->deadFlag = true;
		return;
	}

	if (self->deadFlag)
		return;

	gi.sound(self, CHAN_VOICE, s_death, 1, ATTN_NORM, 0);
	self->deadFlag = true;
	self->takeDamage = true;

	// Randomly choose between two death animations
	if (brandom())
		M_SetAnimation(self, &hellknight_move_deatha);
	else
		M_SetAnimation(self, &hellknight_move_deathb);
}

/*QUAKED monster_hell_knight (1 0 0) (-16 -16 -24) (16 16 40) Ambush
*/
void SP_monster_hell_knight(gentity_t* self) {
	if (!M_AllowSpawn(self)) {
		FreeEntity(self);
		return;
	}

	// Precache all necessary assets
	gi.modelIndex("models/monsters/hknight/tris.md2");
	s_idle.assign("hknight/idle.wav");
	s_sight.assign("hknight/sight1.wav");
	s_pain.assign("hknight/pain1.wav");
	s_death.assign("hknight/death1.wav");
	s_slash.assign("hknight/slash1.wav");
	s_magic.assign("hknight/attack1.wav");
	s_sword1.assign("knight/sword1.wav");
	s_sword2.assign("knight/sword2.wav");

	self->className = "monster_hell_knight";
	self->s.modelIndex = gi.modelIndex("models/monsters/hknight/tris.md2");
	self->mins = HK_MINS;
	self->maxs = HK_MAXS;
	self->yawSpeed = 15;

	self->health = self->maxHealth = HK_HEALTH * st.health_multiplier;
	self->gibHealth = HK_GIBHEALTH;
	self->mass = HK_MASS;

	self->pain = hk_pain;
	self->die = hk_die;

	self->monsterInfo.stand = hk_stand;
	self->monsterInfo.walk = hk_walk;
	self->monsterInfo.run = hk_run;
	self->monsterInfo.attack = hk_attack;
	self->monsterInfo.melee = hk_melee;
	self->monsterInfo.sight = hk_sight;
	self->monsterInfo.setSkin = hk_setskin;
	self->monsterInfo.checkAttack = M_CheckAttack; // Use the engine's default attack decision logic

	self->s.skinNum &= ~1;

	M_SetAnimation(self, &hellknight_move_stand);
	walkmonster_start(self);
}
