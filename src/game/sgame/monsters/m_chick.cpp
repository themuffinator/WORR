/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

============================================================================== chick
==============================================================================*/

#include "../g_local.hpp"
#include "m_chick.hpp"
#include "m_flash.hpp"

void chick_stand(gentity_t *self);
void chick_run(gentity_t *self);
void chick_reslash(gentity_t *self);
void chick_rerocket(gentity_t *self);
void chick_attack1(gentity_t *self);

static cached_soundIndex sound_missile_prelaunch;
static cached_soundIndex sound_missile_launch;
static cached_soundIndex sound_melee_swing;
static cached_soundIndex sound_melee_hit;
static cached_soundIndex sound_missile_reload;
static cached_soundIndex sound_death1;
static cached_soundIndex sound_death2;
static cached_soundIndex sound_fall_down;
static cached_soundIndex sound_idle1;
static cached_soundIndex sound_idle2;
static cached_soundIndex sound_pain1;
static cached_soundIndex sound_pain2;
static cached_soundIndex sound_pain3;
static cached_soundIndex sound_sight;
static cached_soundIndex sound_search;

static void ChickMoan(gentity_t *self) {
	if (frandom() < 0.5f)
		gi.sound(self, CHAN_VOICE, sound_idle1, 1, ATTN_IDLE, 0);
	else
		gi.sound(self, CHAN_VOICE, sound_idle2, 1, ATTN_IDLE, 0);
}

MonsterFrame chick_frames_fidget[] = {
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand, 0, ChickMoan },
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
MMOVE_T(chick_move_fidget) = { FRAME_stand201, FRAME_stand230, chick_frames_fidget, chick_stand };

static void chick_fidget(gentity_t *self) {
	if (self->monsterInfo.aiFlags & AI_STAND_GROUND)
		return;
	else if (self->enemy)
		return;
	if (frandom() <= 0.3f)
		M_SetAnimation(self, &chick_move_fidget);
}

MonsterFrame chick_frames_stand[] = {
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
	{ ai_stand, 0, chick_fidget },
};
MMOVE_T(chick_move_stand) = { FRAME_stand101, FRAME_stand130, chick_frames_stand, nullptr };

MONSTERINFO_STAND(chick_stand) (gentity_t *self) -> void {
	M_SetAnimation(self, &chick_move_stand);
}

MonsterFrame chick_frames_start_run[] = {
	{ ai_run, 1 },
	{ ai_run },
	{ ai_run, 0, monster_footstep },
	{ ai_run, -1 },
	{ ai_run, -1, monster_footstep },
	{ ai_run },
	{ ai_run, 1 },
	{ ai_run, 3 },
	{ ai_run, 6 },
	{ ai_run, 3 }
};
MMOVE_T(chick_move_start_run) = { FRAME_walk01, FRAME_walk10, chick_frames_start_run, chick_run };

MonsterFrame chick_frames_run[] = {
	{ ai_run, 6 },
	{ ai_run, 8, monster_footstep },
	{ ai_run, 13 },
	{ ai_run, 5, monster_done_dodge }, // make sure to clear dodge bit
	{ ai_run, 7 },
	{ ai_run, 4 },
	{ ai_run, 11, monster_footstep },
	{ ai_run, 5 },
	{ ai_run, 9 },
	{ ai_run, 7 }
};

MMOVE_T(chick_move_run) = { FRAME_walk11, FRAME_walk20, chick_frames_run, nullptr };

MonsterFrame chick_frames_walk[] = {
	{ ai_walk, 6 },
	{ ai_walk, 8, monster_footstep },
	{ ai_walk, 13 },
	{ ai_walk, 5 },
	{ ai_walk, 7 },
	{ ai_walk, 4 },
	{ ai_walk, 11, monster_footstep },
	{ ai_walk, 5 },
	{ ai_walk, 9 },
	{ ai_walk, 7 }
};

MMOVE_T(chick_move_walk) = { FRAME_walk11, FRAME_walk20, chick_frames_walk, nullptr };

MONSTERINFO_WALK(chick_walk) (gentity_t *self) -> void {
	M_SetAnimation(self, &chick_move_walk);
}

MONSTERINFO_RUN(chick_run) (gentity_t *self) -> void {
	monster_done_dodge(self);

	if (self->monsterInfo.aiFlags & AI_STAND_GROUND) {
		M_SetAnimation(self, &chick_move_stand);
		return;
	}

	if (self->monsterInfo.active_move == &chick_move_walk ||
		self->monsterInfo.active_move == &chick_move_start_run) {
		M_SetAnimation(self, &chick_move_run);
	} else {
		M_SetAnimation(self, &chick_move_start_run);
	}
}

MonsterFrame chick_frames_pain1[] = {
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move }
};
MMOVE_T(chick_move_pain1) = { FRAME_pain101, FRAME_pain105, chick_frames_pain1, chick_run };

MonsterFrame chick_frames_pain2[] = {
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move }
};
MMOVE_T(chick_move_pain2) = { FRAME_pain201, FRAME_pain205, chick_frames_pain2, chick_run };

MonsterFrame chick_frames_pain3[] = {
	{ ai_move },
	{ ai_move, 0, monster_footstep },
	{ ai_move, -6 },
	{ ai_move, 3, monster_footstep },
	{ ai_move, 11 },
	{ ai_move, 3, monster_footstep },
	{ ai_move },
	{ ai_move },
	{ ai_move, 4 },
	{ ai_move, 1 },
	{ ai_move },
	{ ai_move, -3 },
	{ ai_move, -4 },
	{ ai_move, 5 },
	{ ai_move, 7 },
	{ ai_move, -2 },
	{ ai_move, 3 },
	{ ai_move, -5 },
	{ ai_move, -2 },
	{ ai_move, -8 },
	{ ai_move, 2, monster_footstep }
};
MMOVE_T(chick_move_pain3) = { FRAME_pain301, FRAME_pain321, chick_frames_pain3, chick_run };

static PAIN(chick_pain) (gentity_t *self, gentity_t *other, float kick, int damage, const MeansOfDeath &mod) -> void {
	float r;

	monster_done_dodge(self);

	if (level.time < self->pain_debounce_time)
		return;

	self->pain_debounce_time = level.time + 3_sec;

	r = frandom();
	if (r < 0.33f)
		gi.sound(self, CHAN_VOICE, sound_pain1, 1, ATTN_NORM, 0);
	else if (r < 0.66f)
		gi.sound(self, CHAN_VOICE, sound_pain2, 1, ATTN_NORM, 0);
	else
		gi.sound(self, CHAN_VOICE, sound_pain3, 1, ATTN_NORM, 0);

	if (!M_ShouldReactToPain(self, mod))
		return; // no pain anims in nightmare

	// PMM - clear this from blindFire
	self->monsterInfo.aiFlags &= ~AI_MANUAL_STEERING;

	if (damage <= 10)
		M_SetAnimation(self, &chick_move_pain1);
	else if (damage <= 25)
		M_SetAnimation(self, &chick_move_pain2);
	else
		M_SetAnimation(self, &chick_move_pain3);

	// PMM - clear duck flag
	if (self->monsterInfo.aiFlags & AI_DUCKED)
		monster_duck_up(self);
}

MONSTERINFO_SETSKIN(chick_setpain) (gentity_t *self) -> void {
	if (self->health < (self->maxHealth / 2))
		self->s.skinNum |= 1;
	else
		self->s.skinNum &= ~1;
}

static void chick_dead(gentity_t *self) {
	self->mins = { -16, -16, 0 };
	self->maxs = { 16, 16, 8 };
	monster_dead(self);
}

static void chick_shrink(gentity_t *self) {
	self->maxs[2] = 12;
	self->svFlags |= SVF_DEADMONSTER;
	gi.linkEntity(self);
}

MonsterFrame chick_frames_death2[] = {
	{ ai_move, -6 },
	{ ai_move },
	{ ai_move, -1 },
	{ ai_move, -5, monster_footstep },
	{ ai_move },
	{ ai_move, -1 },
	{ ai_move, -2 },
	{ ai_move, 1 },
	{ ai_move, 10 },
	{ ai_move, 2 },
	{ ai_move, 3, monster_footstep },
	{ ai_move, 1 },
	{ ai_move, 2 },
	{ ai_move },
	{ ai_move, 3 },
	{ ai_move, 3 },
	{ ai_move, 1, monster_footstep },
	{ ai_move, -3 },
	{ ai_move, -5 },
	{ ai_move, 4 },
	{ ai_move, 15, chick_shrink },
	{ ai_move, 14, monster_footstep },
	{ ai_move, 1 }
};
MMOVE_T(chick_move_death2) = { FRAME_death201, FRAME_death223, chick_frames_death2, chick_dead };

MonsterFrame chick_frames_death1[] = {
	{ ai_move },
	{ ai_move, 0, monster_footstep },
	{ ai_move, -7 },
	{ ai_move, 4, monster_footstep },
	{ ai_move, 11, chick_shrink },
	{ ai_move },
	{ ai_move, 0, monster_footstep },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move, 0, monster_footstep },
	{ ai_move }
};
MMOVE_T(chick_move_death1) = { FRAME_death101, FRAME_death112, chick_frames_death1, chick_dead };

static DIE(chick_die) (gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, const Vector3 &point, const MeansOfDeath &mod) -> void {
	int n;

	// check for gib
	if (M_CheckGib(self, mod)) {
		gi.sound(self, CHAN_VOICE, gi.soundIndex("misc/udeath.wav"), 1, ATTN_NORM, 0);

		self->s.skinNum /= 2;

		ThrowGibs(self, damage, {
			{ 2, "models/objects/gibs/bone/tris.md2" },
			{ 3, "models/objects/gibs/sm_meat/tris.md2" },
			{ "models/monsters/bitch/gibs/arm.md2", GIB_SKINNED | GIB_UPRIGHT },
			{ "models/monsters/bitch/gibs/foot.md2", GIB_SKINNED | GIB_UPRIGHT },
			{ "models/monsters/bitch/gibs/tube.md2", GIB_SKINNED | GIB_UPRIGHT },
			{ "models/monsters/bitch/gibs/chest.md2", GIB_SKINNED },
			{ "models/monsters/bitch/gibs/head.md2", GIB_HEAD | GIB_SKINNED }
			});
		self->deadFlag = true;

		return;
	}

	if (self->deadFlag)
		return;

	// regular death
	self->deadFlag = true;
	self->takeDamage = true;

	n = brandom();

	if (n == 0) {
		M_SetAnimation(self, &chick_move_death1);
		gi.sound(self, CHAN_VOICE, sound_death1, 1, ATTN_NORM, 0);
	} else {
		M_SetAnimation(self, &chick_move_death2);
		gi.sound(self, CHAN_VOICE, sound_death2, 1, ATTN_NORM, 0);
	}
}

// PMM - changes to duck code for new dodge

MonsterFrame chick_frames_duck[] = {
	{ ai_move, 0, monster_duck_down },
	{ ai_move, 1 },
	{ ai_move, 4, monster_duck_hold },
	{ ai_move, -4 },
	{ ai_move, -5, monster_duck_up },
	{ ai_move, 3 },
	{ ai_move, 1 }
};
MMOVE_T(chick_move_duck) = { FRAME_duck01, FRAME_duck07, chick_frames_duck, chick_run };

static void ChickSlash(gentity_t *self) {
	Vector3 aim = { MELEE_DISTANCE, self->mins[0], 10 };
	gi.sound(self, CHAN_WEAPON, sound_melee_swing, 1, ATTN_NORM, 0);
	fire_hit(self, aim, irandom(10, 16), 100);
}

static void ChickRocket(gentity_t *self) {
	Vector3	forward, right;
	Vector3	start;
	Vector3	dir;
	Vector3	vec;
	trace_t trace;
	int		rocketSpeed;
	Vector3	target;
	bool	blindFire = false;

	if (self->monsterInfo.aiFlags & AI_MANUAL_STEERING)
		blindFire = true;
	else
		blindFire = false;

	if (!self->enemy || !self->enemy->inUse)
		return;

	AngleVectors(self->s.angles, forward, right, nullptr);
	start = M_ProjectFlashSource(self, monster_flash_offset[MZ2_CHICK_ROCKET_1], forward, right);

	// [Paril-KEX]
	if (self->s.skinNum > 1)
		rocketSpeed = 500;
	else
		rocketSpeed = 650;

	if (blindFire)
		target = self->monsterInfo.blind_fire_target;
	else
		target = self->enemy->s.origin;

	if (blindFire) {
		vec = target;
		dir = vec - start;
	}

	// don't shoot at feet if they're above where i'm shooting from.
	else if (frandom() < 0.33f || (start[2] < self->enemy->absMin[2])) {
		vec = target;
		vec[2] += self->enemy->viewHeight;
		dir = vec - start;
	} else {
		vec = target;
		vec[2] = self->enemy->absMin[2] + 1;
		dir = vec - start;
	}

	// lead target  (not when blindfiring) - 20, 35, 50, 65 chance of leading
	if ((!blindFire) && (frandom() < 0.35f))
		PredictAim(self, self->enemy, start, rocketSpeed, false, 0.f, &dir, &vec);

	dir.normalize();

	// pmm blindFire doesn't check target (done in checkAttack)
	// paranoia, make sure we're not shooting a target right next to us
	trace = gi.traceLine(start, vec, self, MASK_PROJECTILE);
	if (blindFire) {
		// blindFire has different fail criteria for the trace
		if (!(trace.startSolid || trace.allSolid || (trace.fraction < 0.5f))) {
			if (self->s.skinNum > 1)
				monster_fire_heat(self, start, dir, 50, rocketSpeed, MZ2_CHICK_ROCKET_1, 0.075f);
			else
				monster_fire_rocket(self, start, dir, 50, rocketSpeed, MZ2_CHICK_ROCKET_1);
		} else {
			// geez, this is bad.  she's avoiding about 80% of her blindFires due to hitting things.
			// hunt around for a good shot
			// try shifting the target to the left a little (to help counter her large offset)
			vec = target;
			vec += (right * -10);
			dir = vec - start;
			dir.normalize();
			trace = gi.traceLine(start, vec, self, MASK_PROJECTILE);
			if (!(trace.startSolid || trace.allSolid || (trace.fraction < 0.5f))) {
				if (self->s.skinNum > 1)
					monster_fire_heat(self, start, dir, 50, rocketSpeed, MZ2_CHICK_ROCKET_1, 0.075f);
				else
					monster_fire_rocket(self, start, dir, 50, rocketSpeed, MZ2_CHICK_ROCKET_1);
			} else {
				// ok, that failed.  try to the right
				vec = target;
				vec += (right * 10);
				dir = vec - start;
				dir.normalize();
				trace = gi.traceLine(start, vec, self, MASK_PROJECTILE);
				if (!(trace.startSolid || trace.allSolid || (trace.fraction < 0.5f))) {
					if (self->s.skinNum > 1)
						monster_fire_heat(self, start, dir, 50, rocketSpeed, MZ2_CHICK_ROCKET_1, 0.075f);
					else
						monster_fire_rocket(self, start, dir, 50, rocketSpeed, MZ2_CHICK_ROCKET_1);
				}
			}
		}
	} else {
		if (trace.fraction > 0.5f || trace.ent->solid != SOLID_BSP) {
			if (self->s.skinNum > 1)
				monster_fire_heat(self, start, dir, 50, rocketSpeed, MZ2_CHICK_ROCKET_1, 0.15f);
			else
				monster_fire_rocket(self, start, dir, 50, rocketSpeed, MZ2_CHICK_ROCKET_1);
		}
	}
}

static void Chick_PreAttack1(gentity_t *self) {
	gi.sound(self, CHAN_VOICE, sound_missile_prelaunch, 1, ATTN_NORM, 0);

	if (self->monsterInfo.aiFlags & AI_MANUAL_STEERING) {
		Vector3 aim = self->monsterInfo.blind_fire_target - self->s.origin;
		self->ideal_yaw = vectoyaw(aim);
	}
}

static void ChickReload(gentity_t *self) {
	gi.sound(self, CHAN_VOICE, sound_missile_reload, 1, ATTN_NORM, 0);
}

MonsterFrame chick_frames_start_attack1[] = {
	{ ai_charge, 0, Chick_PreAttack1 },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge, 4 },
	{ ai_charge },
	{ ai_charge, -3 },
	{ ai_charge, 3 },
	{ ai_charge, 5 },
	{ ai_charge, 7, monster_footstep },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge, 0, chick_attack1 }
};
MMOVE_T(chick_move_start_attack1) = { FRAME_attack101, FRAME_attack113, chick_frames_start_attack1, nullptr };

MonsterFrame chick_frames_attack1[] = {
	{ ai_charge, 19, ChickRocket },
	{ ai_charge, -6, monster_footstep },
	{ ai_charge, -5 },
	{ ai_charge, -2 },
	{ ai_charge, -7, monster_footstep },
	{ ai_charge },
	{ ai_charge, 1 },
	{ ai_charge, 10, ChickReload },
	{ ai_charge, 4 },
	{ ai_charge, 5, monster_footstep },
	{ ai_charge, 6 },
	{ ai_charge, 6 },
	{ ai_charge, 4 },
	{ ai_charge, 3, [](gentity_t *self) { chick_rerocket(self); monster_footstep(self); } }
};
MMOVE_T(chick_move_attack1) = { FRAME_attack114, FRAME_attack127, chick_frames_attack1, nullptr };

MonsterFrame chick_frames_end_attack1[] = {
	{ ai_charge, -3 },
	{ ai_charge },
	{ ai_charge, -6 },
	{ ai_charge, -4 },
	{ ai_charge, -2, monster_footstep }
};
MMOVE_T(chick_move_end_attack1) = { FRAME_attack128, FRAME_attack132, chick_frames_end_attack1, chick_run };

void chick_rerocket(gentity_t *self) {
	if (self->monsterInfo.aiFlags & AI_MANUAL_STEERING) {
		self->monsterInfo.aiFlags &= ~AI_MANUAL_STEERING;
		M_SetAnimation(self, &chick_move_end_attack1);
		return;
	}

	if (!M_CheckClearShot(self, monster_flash_offset[MZ2_CHICK_ROCKET_1])) {
		M_SetAnimation(self, &chick_move_end_attack1);
		return;
	}

	if (self->enemy->health > 0) {
		if (range_to(self, self->enemy) > RANGE_MELEE)
			if (visible(self, self->enemy))
				if (frandom() <= 0.7f) {
					M_SetAnimation(self, &chick_move_attack1);
					return;
				}
	}
	M_SetAnimation(self, &chick_move_end_attack1);
}

void chick_attack1(gentity_t *self) {
	M_SetAnimation(self, &chick_move_attack1);
}

MonsterFrame chick_frames_slash[] = {
	{ ai_charge, 1 },
	{ ai_charge, 7, ChickSlash },
	{ ai_charge, -7, monster_footstep },
	{ ai_charge, 1 },
	{ ai_charge, -1 },
	{ ai_charge, 1 },
	{ ai_charge },
	{ ai_charge, 1 },
	{ ai_charge, -2, chick_reslash }
};
MMOVE_T(chick_move_slash) = { FRAME_attack204, FRAME_attack212, chick_frames_slash, nullptr };

MonsterFrame chick_frames_end_slash[] = {
	{ ai_charge, -6 },
	{ ai_charge, -1 },
	{ ai_charge, -6 },
	{ ai_charge, 0, monster_footstep }
};
MMOVE_T(chick_move_end_slash) = { FRAME_attack213, FRAME_attack216, chick_frames_end_slash, chick_run };

void chick_reslash(gentity_t *self) {
	if (self->enemy->health > 0) {
		if (range_to(self, self->enemy) <= RANGE_MELEE) {
			if (frandom() <= 0.9f) {
				M_SetAnimation(self, &chick_move_slash);
				return;
			} else {
				M_SetAnimation(self, &chick_move_end_slash);
				return;
			}
		}
	}
	M_SetAnimation(self, &chick_move_end_slash);
}

static void chick_slash(gentity_t *self) {
	M_SetAnimation(self, &chick_move_slash);
}

MonsterFrame chick_frames_start_slash[] = {
	{ ai_charge, 1 },
	{ ai_charge, 8 },
	{ ai_charge, 3 }
};
MMOVE_T(chick_move_start_slash) = { FRAME_attack201, FRAME_attack203, chick_frames_start_slash, chick_slash };

MONSTERINFO_MELEE(chick_melee) (gentity_t *self) -> void {
	M_SetAnimation(self, &chick_move_start_slash);
}

MONSTERINFO_ATTACK(chick_attack) (gentity_t *self) -> void {
	if (!M_CheckClearShot(self, monster_flash_offset[MZ2_CHICK_ROCKET_1]))
		return;

	float r, chance;

	monster_done_dodge(self);

	// PMM
	if (self->monsterInfo.attackState == MonsterAttackState::Blind) {
		// setup shot probabilities
		if (self->monsterInfo.blind_fire_delay < 1.0_sec)
			chance = 1.0;
		else if (self->monsterInfo.blind_fire_delay < 7.5_sec)
			chance = 0.4f;
		else
			chance = 0.1f;

		r = frandom();

		// minimum of 5.5 seconds, plus 0-1, after the shots are done
		self->monsterInfo.blind_fire_delay += random_time(5.5_sec, 6.5_sec);

		// don't shoot at the origin
		if (!self->monsterInfo.blind_fire_target)
			return;

		// don't shoot if the dice say not to
		if (r > chance)
			return;

		// turn on manual steering to signal both manual steering and blindFire
		self->monsterInfo.aiFlags |= AI_MANUAL_STEERING;
		M_SetAnimation(self, &chick_move_start_attack1);
		self->monsterInfo.attackFinished = level.time + random_time(2_sec);
		return;
	}
	// pmm

	M_SetAnimation(self, &chick_move_start_attack1);
}

MONSTERINFO_SIGHT(chick_sight) (gentity_t *self, gentity_t *other) -> void {
	gi.sound(self, CHAN_VOICE, sound_sight, 1, ATTN_NORM, 0);
}

MONSTERINFO_BLOCKED(chick_blocked) (gentity_t *self, float dist) -> bool {
	if (blocked_checkplat(self, dist))
		return true;

	return false;
}

MONSTERINFO_DUCK(chick_duck) (gentity_t *self, GameTime eta) -> bool {
	if ((self->monsterInfo.active_move == &chick_move_start_attack1) ||
		(self->monsterInfo.active_move == &chick_move_attack1)) {
		// if we're shooting don't dodge
		self->monsterInfo.unDuck(self);
		return false;
	}

	M_SetAnimation(self, &chick_move_duck);

	return true;
}

MONSTERINFO_SIDESTEP(chick_sidestep) (gentity_t *self) -> bool {
	if ((self->monsterInfo.active_move == &chick_move_start_attack1) ||
		(self->monsterInfo.active_move == &chick_move_attack1) ||
		(self->monsterInfo.active_move == &chick_move_pain3)) {
		// if we're shooting, don't dodge
		return false;
	}

	if (self->monsterInfo.active_move != &chick_move_run)
		M_SetAnimation(self, &chick_move_run);

	return true;
}

/*QUAKED monster_chick (1 .5 0) (-16 -16 -24) (16 16 32) AMBUSH TRIGGER_SPAWN SIGHT x CORPSE x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
 */
void SP_monster_chick(gentity_t *self) {
	if (!M_AllowSpawn(self)) {
		FreeEntity(self);
		return;
	}

	sound_missile_prelaunch.assign("chick/chkatck1.wav");
	sound_missile_launch.assign("chick/chkatck2.wav");
	sound_melee_swing.assign("chick/chkatck3.wav");
	sound_melee_hit.assign("chick/chkatck4.wav");
	sound_missile_reload.assign("chick/chkatck5.wav");
	sound_death1.assign("chick/chkdeth1.wav");
	sound_death2.assign("chick/chkdeth2.wav");
	sound_fall_down.assign("chick/chkfall1.wav");
	sound_idle1.assign("chick/chkidle1.wav");
	sound_idle2.assign("chick/chkidle2.wav");
	sound_pain1.assign("chick/chkpain1.wav");
	sound_pain2.assign("chick/chkpain2.wav");
	sound_pain3.assign("chick/chkpain3.wav");
	sound_sight.assign("chick/chksght1.wav");
	sound_search.assign("chick/chksrch1.wav");

	self->moveType = MoveType::Step;
	self->solid = SOLID_BBOX;
	self->s.modelIndex = gi.modelIndex("models/monsters/bitch/tris.md2");

	gi.modelIndex("models/monsters/bitch/gibs/arm.md2");
	gi.modelIndex("models/monsters/bitch/gibs/chest.md2");
	gi.modelIndex("models/monsters/bitch/gibs/foot.md2");
	gi.modelIndex("models/monsters/bitch/gibs/head.md2");
	gi.modelIndex("models/monsters/bitch/gibs/tube.md2");

	self->mins = { -16, -16, 0 };
	self->maxs = { 16, 16, 56 };

	self->health = 175 * st.health_multiplier;
	self->gibHealth = -70;
	self->mass = 200;

	self->pain = chick_pain;
	self->die = chick_die;

	self->monsterInfo.stand = chick_stand;
	self->monsterInfo.walk = chick_walk;
	self->monsterInfo.run = chick_run;
	self->monsterInfo.dodge = M_MonsterDodge;
	self->monsterInfo.duck = chick_duck;
	self->monsterInfo.unDuck = monster_duck_up;
	self->monsterInfo.sideStep = chick_sidestep;
	self->monsterInfo.blocked = chick_blocked;
	self->monsterInfo.attack = chick_attack;
	self->monsterInfo.melee = chick_melee;
	self->monsterInfo.sight = chick_sight;
	self->monsterInfo.setSkin = chick_setpain;

	gi.linkEntity(self);

	M_SetAnimation(self, &chick_move_stand);
	self->monsterInfo.scale = MODEL_SCALE;

	self->monsterInfo.blindFire = true;
	walkmonster_start(self);
}

/*QUAKED monster_chick_heat (1 .5 0) (-16 -16 -24) (16 16 32) AMBUSH TRIGGER_SPAWN SIGHT x CORPSE x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
 */
void SP_monster_chick_heat(gentity_t *self) {
	SP_monster_chick(self);
	self->s.skinNum = 2;
	gi.soundIndex("weapons/railgr1a.wav");
}
