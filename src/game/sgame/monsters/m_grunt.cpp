/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

============================================================================== GRUNT (Q1-styled
grunt) - WOR port reference - Uses WOR soldier structure (moves, ai_* helpers, muzzle-flash
offsets) - Single weapon profile (shotgun), 9-frame fire sequence with flash mapping -
Sight/search/idle, pain, death
==============================================================================*/

#include "../g_local.hpp"
#include "m_grunt.hpp"
#include "m_flash.hpp"

// sounds
static cached_soundIndex snd_idle;
static cached_soundIndex snd_sight;
static cached_soundIndex snd_search;
static cached_soundIndex snd_pain1;
static cached_soundIndex snd_pain2;
static cached_soundIndex snd_death;

/*
===============
grunt_idle
===============
*/
static void grunt_idle(gentity_t* self) {
	if (frandom() > 0.8f)
		gi.sound(self, CHAN_VOICE, snd_idle, 1, ATTN_IDLE, 0);
}

/*
===============
grunt_stand
===============
*/
static MonsterFrame grunt_frames_stand[] = {
	{ ai_stand, 0, grunt_idle },
	{ ai_stand },{ ai_stand },{ ai_stand },{ ai_stand },
	{ ai_stand },{ ai_stand },{ ai_stand },{ ai_stand },{ ai_stand }
};
MMOVE_T(grunt_move_stand) = { FRAME_stand01, FRAME_stand10, grunt_frames_stand, nullptr };

MONSTERINFO_STAND(grunt_stand) (gentity_t* self) -> void {
	M_SetAnimation(self, &grunt_move_stand);
}

/*
===============
grunt_walk
===============
*/
static MonsterFrame grunt_frames_walk[] = {
	{ ai_walk, 4 },{ ai_walk, 4 },{ ai_walk, 6 },{ ai_walk, 4 },
	{ ai_walk, 6 },{ ai_walk, 4 },{ ai_walk, 6 },{ ai_walk, 4 }
};
MMOVE_T(grunt_move_walk) = { FRAME_walk01, FRAME_walk08, grunt_frames_walk, nullptr };

MONSTERINFO_WALK(grunt_walk) (gentity_t* self) -> void {
	M_SetAnimation(self, &grunt_move_walk);
}

/*
===============
grunt_run
===============
*/
static MonsterFrame grunt_frames_run[] = {
	{ ai_run, 10 },{ ai_run, 12 },{ ai_run, 12 },
	{ ai_run, 14 },{ ai_run, 10 },{ ai_run, 14 }
};
MMOVE_T(grunt_move_run) = { FRAME_run01, FRAME_run06, grunt_frames_run, nullptr };

MONSTERINFO_RUN(grunt_run) (gentity_t* self) -> void {
	if (self->monsterInfo.aiFlags & AI_STAND_GROUND)
		M_SetAnimation(self, &grunt_move_stand);
	else
		M_SetAnimation(self, &grunt_move_run);
}

/*
===============
grunt_pain
===============
*/
static MonsterFrame grunt_frames_pain[] = {
	{ ai_move, -2 },{ ai_move, 2 },{ ai_move, 1 },{ ai_move },{ ai_move }
};
MMOVE_T(grunt_move_pain) = { FRAME_pain01, FRAME_pain05, grunt_frames_pain, grunt_run };

static PAIN(grunt_pain) (gentity_t* self, gentity_t* other, float kick, int damage, const MeansOfDeath& mod) -> void {
	if (level.time < self->pain_debounce_time)
		return;

	self->pain_debounce_time = level.time + 2.0_sec;

	if (brandom())
		gi.sound(self, CHAN_VOICE, snd_pain1, 1, ATTN_NORM, 0);
	else
		gi.sound(self, CHAN_VOICE, snd_pain2, 1, ATTN_NORM, 0);

	if (!M_ShouldReactToPain(self, mod))
		return;

	M_SetAnimation(self, &grunt_move_pain);
}

/*
===============
grunt_death helpers
===============
*/
static void grunt_shrink(gentity_t* self) {
	self->svFlags |= SVF_DEADMONSTER;
	self->maxs[2] = 0;
	gi.linkEntity(self);
}

/*
===============
grunt_die
===============
*/
static MonsterFrame grunt_frames_death[] = {
	{ ai_move },
	{ ai_move, -6 },
	{ ai_move, -6, grunt_shrink },
	{ ai_move, -4 },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move }
};
MMOVE_T(grunt_move_death) = { FRAME_death01, FRAME_death08, grunt_frames_death, monster_dead };

static DIE(grunt_die) (gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, const Vector3& point, const MeansOfDeath& mod) -> void {
	if (M_CheckGib(self, mod)) {
		gi.sound(self, CHAN_VOICE, gi.soundIndex("misc/udeath.wav"), 1, ATTN_NORM, 0);
		ThrowGibs(self, damage, {
			{ 2, "models/objects/gibs/bone/tris.md2" },
			{ 3, "models/objects/gibs/sm_meat/tris.md2" },
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

	M_SetAnimation(self, &grunt_move_death);
}

/*
===============
grunt_sight
===============
*/
MONSTERINFO_SIGHT(grunt_sight) (gentity_t* self, gentity_t* other) -> void {
	gi.sound(self, CHAN_VOICE, snd_sight, 1, ATTN_NORM, 0);
}

/*
===============
grunt_search
===============
*/
MONSTERINFO_SEARCH(grunt_search) (gentity_t* self) -> void {
	gi.sound(self, CHAN_VOICE, snd_search, 1, ATTN_IDLE, 0);
}

/*
===============
grunt_setskin
===============
*/
MONSTERINFO_SETSKIN(grunt_setskin) (gentity_t* self) -> void {
	if (self->health < (self->maxHealth / 2))
		self->s.skinNum |= 1;
	else
		self->s.skinNum &= ~1;
}

/*
===============
grunt_select_flash
Map attack frames to 9 muzzle flash slots like WOR soldier
===============
*/
static inline MonsterMuzzleFlashID grunt_select_flash(int i) {
	static constexpr MonsterMuzzleFlashID shotgun_flash[9] = {
		MZ2_SOLDIER_SHOTGUN_1, MZ2_SOLDIER_SHOTGUN_2, MZ2_SOLDIER_SHOTGUN_3,
		MZ2_SOLDIER_SHOTGUN_4, MZ2_SOLDIER_SHOTGUN_5, MZ2_SOLDIER_SHOTGUN_6,
		MZ2_SOLDIER_SHOTGUN_7, MZ2_SOLDIER_SHOTGUN_8, MZ2_SOLDIER_SHOTGUN_9
	};
	if (i < 0) i = 0;
	if (i > 8) i = 8;
	return shotgun_flash[i];
}

/*
===============
grunt_fire_shotgun
Shot with WOR-style muzzle flash positioning and random right/up jitter
===============
*/
static void grunt_fire_shotgun(gentity_t* self, int flash_number, bool angle_limited) {
	Vector3 start, forward, right, up;
	Vector3 aim, end, dir, aim_norm;
	float angle;

	AngleVectors(self->s.angles, forward, right, nullptr);

	MonsterMuzzleFlashID flash_index = grunt_select_flash(flash_number);
	start = M_ProjectFlashSource(self, monster_flash_offset[flash_index], forward, right);

	// no enemy -> bail out and release hold
	if ((!self->enemy) || (!self->enemy->inUse)) {
		self->monsterInfo.aiFlags &= ~AI_HOLD_FRAME;
		return;
	}

	end = (self->monsterInfo.attackState == MonsterAttackState::Blind) ? self->monsterInfo.blind_fire_target : self->enemy->s.origin;
	end[2] += self->enemy->viewHeight;

	aim = end - start;

	if (angle_limited) {
		aim_norm = aim;
		aim_norm.normalize();
		angle = aim_norm.dot(forward);
		if (angle < 0.5f) {
			if (level.time >= self->monsterInfo.fireWait)
				self->monsterInfo.aiFlags &= ~AI_HOLD_FRAME;
			else
				self->monsterInfo.aiFlags |= AI_HOLD_FRAME;
			return;
		}
	}

	dir = VectorToAngles(aim);
	AngleVectors(dir, forward, right, up);

	float r = crandom() * 100.0f;
	float u = crandom() * 50.0f;

	end = start + (forward * 8192.0f) + (right * r) + (up * u);
	aim = end - start;
	aim.normalize();

	monster_fire_shotgun(self, start, aim, 4, 1, 1500, 750, 9, flash_index);

	// light hold/release window like WOR soldier
	if (!(self->monsterInfo.aiFlags & AI_HOLD_FRAME))
		self->monsterInfo.fireWait = level.time + random_time(300_ms, 1.1_sec);

	if (level.time >= self->monsterInfo.fireWait)
		self->monsterInfo.aiFlags &= ~AI_HOLD_FRAME;
	else
		self->monsterInfo.aiFlags |= AI_HOLD_FRAME;
}

/*
===============
grunt_attack helpers
===============
*/
static void grunt_fire_slot0(gentity_t* self) { grunt_fire_shotgun(self, 0, false); }
static void grunt_fire_slot1(gentity_t* self) { grunt_fire_shotgun(self, 1, false); }
static void grunt_fire_slot2(gentity_t* self) { grunt_fire_shotgun(self, 2, false); }
static void grunt_fire_slot3(gentity_t* self) { grunt_fire_shotgun(self, 3, false); }
static void grunt_fire_slot4(gentity_t* self) { grunt_fire_shotgun(self, 4, false); }
static void grunt_fire_slot5(gentity_t* self) { grunt_fire_shotgun(self, 5, false); }
static void grunt_fire_slot6(gentity_t* self) { grunt_fire_shotgun(self, 6, false); }
static void grunt_fire_slot7(gentity_t* self) { grunt_fire_shotgun(self, 7, false); }
static void grunt_fire_slot8(gentity_t* self) { grunt_fire_shotgun(self, 8, false); }

/*
===============
grunt_attack
Single 9-frame fire sequence; refire chance if still visible and near
===============
*/
static MonsterFrame grunt_frames_attack[] = {
	{ ai_charge, 0, grunt_fire_slot0 },
	{ ai_charge, 0, grunt_fire_slot1 },
	{ ai_charge, 0, grunt_fire_slot2 },
	{ ai_charge, 0, grunt_fire_slot3 },
	{ ai_charge, 0, grunt_fire_slot4 },
	{ ai_charge, 0, grunt_fire_slot5 },
	{ ai_charge, 0, grunt_fire_slot6 },
	{ ai_charge, 0, grunt_fire_slot7 },
	{ ai_charge, 0, grunt_fire_slot8 }
};
MMOVE_T(grunt_move_attack) = { FRAME_attk01, FRAME_attk09, grunt_frames_attack, grunt_run };

MONSTERINFO_ATTACK(grunt_attack) (gentity_t* self) -> void {
	M_SetAnimation(self, &grunt_move_attack);
}

/*
===============
SP_monster_army
===============
*/
/*QUAKED monster_army (1 .5 0) (-16 -16 -24) (16 16 32) AMBUSH TRIGGER_SPAWN SIGHT x CORPSE x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/monsters/grunt/tris.md2"
*/
void SP_monster_army(gentity_t* self) {
	if (!M_AllowSpawn(self)) {
		FreeEntity(self);
		return;
	}

	// sounds
	snd_idle.assign("grunt/idle.wav");
	snd_sight.assign("grunt/sight1.wav");
	snd_search.assign("grunt/solsrch1.wav");	//remove
	snd_pain1.assign("grunt/pain1.wav");
	snd_pain2.assign("grunt/pain2.wav");
	snd_death.assign("grunt/death1.wav");
	//todo: sattck1
	//snd_sattck1.assign("grunt/sattck1.wav");

	// model and bbox
	self->moveType = MoveType::Step;
	self->solid = SOLID_BBOX;
	self->s.modelIndex = gi.modelIndex("models/monsters/grunt/tris.md2");
	self->mins = { -16, -16, -24 };
	self->maxs = { 16,  16,  32 };

	// stats
	self->health = self->maxHealth = 30 * st.health_multiplier;
	self->gibHealth = -30;
	self->mass = 100;

	// callbacks
	self->pain = grunt_pain;
	self->die = grunt_die;

	self->monsterInfo.stand = grunt_stand;
	self->monsterInfo.walk = grunt_walk;
	self->monsterInfo.run = grunt_run;
	self->monsterInfo.dodge = M_MonsterDodge;
	self->monsterInfo.attack = grunt_attack;
	self->monsterInfo.melee = nullptr;
	self->monsterInfo.sight = grunt_sight;
	self->monsterInfo.search = grunt_search;
	self->monsterInfo.setSkin = grunt_setskin;

	gi.linkEntity(self);

	M_SetAnimation(self, &grunt_move_stand);
	self->monsterInfo.scale = MODEL_SCALE;

	walkmonster_start(self);
}
