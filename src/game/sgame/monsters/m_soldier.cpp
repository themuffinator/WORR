/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

============================================================================== SOLDIER
==============================================================================*/

#include "../g_local.hpp"
#include "m_soldier.hpp"
#include "m_flash.hpp"

static cached_soundIndex sound_idle;
static cached_soundIndex sound_sight1;
static cached_soundIndex sound_sight2;
static cached_soundIndex sound_pain_light;
static cached_soundIndex sound_pain;
static cached_soundIndex sound_pain_ss;
static cached_soundIndex sound_death_light;
static cached_soundIndex sound_death;
static cached_soundIndex sound_death_ss;
static cached_soundIndex sound_cock;

static void soldier_start_charge(gentity_t *self) {
	self->monsterInfo.aiFlags |= AI_CHARGING;
}

static void soldier_stop_charge(gentity_t *self) {
	self->monsterInfo.aiFlags &= ~AI_CHARGING;
}

static void soldier_idle(gentity_t *self) {
	if (frandom() > 0.8f)
		gi.sound(self, CHAN_VOICE, sound_idle, 1, ATTN_IDLE, 0);
}

static void soldier_cock(gentity_t *self) {
	if (self->s.frame == FRAME_stand322)
		gi.sound(self, CHAN_WEAPON, sound_cock, 1, ATTN_IDLE, 0);
	else
		gi.sound(self, CHAN_WEAPON, sound_cock, 1, ATTN_NORM, 0);

	// [Paril-KEX] reset cockness
	self->dmg = 0;
}

static void soldierh_hyper_laser_sound_start(gentity_t *self) {
	if (self->style == 1) {
		if (self->count >= 2 && self->count < 4)
			self->monsterInfo.weaponSound = gi.soundIndex("weapons/hyprbl1a.wav");
	}
}

static void soldierh_hyper_laser_sound_end(gentity_t *self) {
	if (self->monsterInfo.weaponSound) {
		if (self->count >= 2 && self->count < 4)
			gi.sound(self, CHAN_AUTO, gi.soundIndex("weapons/hyprbd1a.wav"), 1, ATTN_NORM, 0);

		self->monsterInfo.weaponSound = 0;
	}
}

// STAND

void soldier_stand(gentity_t *self);

MonsterFrame soldier_frames_stand1[] = {
	{ ai_stand, 0, soldier_idle },
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
MMOVE_T(soldier_move_stand1) = { FRAME_stand101, FRAME_stand130, soldier_frames_stand1, soldier_stand };

MonsterFrame soldier_frames_stand2[] = {
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
	{ ai_stand, 0, monster_footstep },
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
	{ ai_stand, 0, monster_footstep }
};
MMOVE_T(soldier_move_stand2) = { FRAME_stand201, FRAME_stand240, soldier_frames_stand2, soldier_stand };

MonsterFrame soldier_frames_stand3[] = {
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
	{ ai_stand, 0, soldier_cock },
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
MMOVE_T(soldier_move_stand3) = { FRAME_stand301, FRAME_stand339, soldier_frames_stand3, soldier_stand };

MONSTERINFO_STAND(soldier_stand) (gentity_t *self) -> void {
	float r = frandom();

	if ((self->monsterInfo.active_move != &soldier_move_stand1) || (r < 0.6f))
		M_SetAnimation(self, &soldier_move_stand1);
	else if (r < 0.8f)
		M_SetAnimation(self, &soldier_move_stand2);
	else
		M_SetAnimation(self, &soldier_move_stand3);
	soldierh_hyper_laser_sound_end(self);
}

//
// WALK
//

static void soldier_walk1_random(gentity_t *self) {
	if (frandom() > 0.1f)
		self->monsterInfo.nextFrame = FRAME_walk101;
}

MonsterFrame soldier_frames_walk1[] = {
	{ ai_walk, 3 },
	{ ai_walk, 6 },
	{ ai_walk, 2 },
	{ ai_walk, 2, monster_footstep },
	{ ai_walk, 2 },
	{ ai_walk, 1 },
	{ ai_walk, 6 },
	{ ai_walk, 5 },
	{ ai_walk, 3, monster_footstep },
	{ ai_walk, -1, soldier_walk1_random },
	{ ai_walk },
	{ ai_walk },
	{ ai_walk },
	{ ai_walk },
	{ ai_walk },
	{ ai_walk },
	{ ai_walk },
	{ ai_walk },
	{ ai_walk },
	{ ai_walk },
	{ ai_walk },
	{ ai_walk },
	{ ai_walk },
	{ ai_walk },
	{ ai_walk },
	{ ai_walk },
	{ ai_walk },
	{ ai_walk },
	{ ai_walk },
	{ ai_walk },
	{ ai_walk },
	{ ai_walk },
	{ ai_walk }
};
MMOVE_T(soldier_move_walk1) = { FRAME_walk101, FRAME_walk133, soldier_frames_walk1, nullptr };

MonsterFrame soldier_frames_walk2[] = {
	{ ai_walk, 4, monster_footstep },
	{ ai_walk, 4 },
	{ ai_walk, 9 },
	{ ai_walk, 8 },
	{ ai_walk, 5 },
	{ ai_walk, 1, monster_footstep },
	{ ai_walk, 3 },
	{ ai_walk, 7 },
	{ ai_walk, 6 },
	{ ai_walk, 7 }
};
MMOVE_T(soldier_move_walk2) = { FRAME_walk209, FRAME_walk218, soldier_frames_walk2, nullptr };

MONSTERINFO_WALK(soldier_walk) (gentity_t *self) -> void {
	// [Paril-KEX] during N64 cutscene, always use fast walk or we bog down the line
	if (!(self->hackFlags & HACKFLAG_END_CUTSCENE) && frandom() < 0.5f)
		M_SetAnimation(self, &soldier_move_walk1);
	else
		M_SetAnimation(self, &soldier_move_walk2);
}

//
// RUN
//

void soldier_run(gentity_t *self);

MonsterFrame soldier_frames_start_run[] = {
	{ ai_run, 7 },
	{ ai_run, 5 }
};
MMOVE_T(soldier_move_start_run) = { FRAME_run01, FRAME_run02, soldier_frames_start_run, soldier_run };

MonsterFrame soldier_frames_run[] = {
	{ ai_run, 10 },
	{ ai_run, 11, [](gentity_t *self) { monster_done_dodge(self); monster_footstep(self); } },
	{ ai_run, 11 },
	{ ai_run, 16 },
	{ ai_run, 10, monster_footstep },
	{ ai_run, 15, monster_done_dodge }
};
MMOVE_T(soldier_move_run) = { FRAME_run03, FRAME_run08, soldier_frames_run, nullptr };

MONSTERINFO_RUN(soldier_run) (gentity_t *self) -> void {
	monster_done_dodge(self);
	soldierh_hyper_laser_sound_end(self);

	if (self->monsterInfo.aiFlags & AI_STAND_GROUND) {
		M_SetAnimation(self, &soldier_move_stand1);
		return;
	}

	if (self->monsterInfo.active_move == &soldier_move_walk1 ||
		self->monsterInfo.active_move == &soldier_move_walk2 ||
		self->monsterInfo.active_move == &soldier_move_start_run ||
		self->monsterInfo.active_move == &soldier_move_run) {
		M_SetAnimation(self, &soldier_move_run);
	} else {
		M_SetAnimation(self, &soldier_move_start_run);
	}
}

//
// PAIN
//

MonsterFrame soldier_frames_pain1[] = {
	{ ai_move, -3 },
	{ ai_move, 4 },
	{ ai_move, 1 },
	{ ai_move, 1 },
	{ ai_move }
};
MMOVE_T(soldier_move_pain1) = { FRAME_pain101, FRAME_pain105, soldier_frames_pain1, soldier_run };

MonsterFrame soldier_frames_pain2[] = {
	{ ai_move, -13 },
	{ ai_move, -1 },
	{ ai_move, 2 },
	{ ai_move, 4 },
	{ ai_move, 2 },
	{ ai_move, 3 },
	{ ai_move, 2 }
};
MMOVE_T(soldier_move_pain2) = { FRAME_pain201, FRAME_pain207, soldier_frames_pain2, soldier_run };

MonsterFrame soldier_frames_pain3[] = {
	{ ai_move, -8 },
	{ ai_move, 10 },
	{ ai_move, -4, monster_footstep },
	{ ai_move, -1 },
	{ ai_move, -3 },
	{ ai_move },
	{ ai_move, 3 },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move, 1 },
	{ ai_move },
	{ ai_move, 1 },
	{ ai_move, 2 },
	{ ai_move, 4 },
	{ ai_move, 3 },
	{ ai_move, 2, monster_footstep }
};
MMOVE_T(soldier_move_pain3) = { FRAME_pain301, FRAME_pain318, soldier_frames_pain3, soldier_run };

MonsterFrame soldier_frames_pain4[] = {
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move, -10 },
	{ ai_move, -6 },
	{ ai_move, 8 },
	{ ai_move, 4 },
	{ ai_move, 1 },
	{ ai_move },
	{ ai_move, 2 },
	{ ai_move, 5 },
	{ ai_move, 2 },
	{ ai_move, -1 },
	{ ai_move, -1 },
	{ ai_move, 3 },
	{ ai_move, 2 },
	{ ai_move }
};
MMOVE_T(soldier_move_pain4) = { FRAME_pain401, FRAME_pain417, soldier_frames_pain4, soldier_run };

static PAIN(soldier_pain) (gentity_t *self, gentity_t *other, float kick, int damage, const MeansOfDeath &mod) -> void {
	float r;
	int	  n;

	monster_done_dodge(self);
	soldier_stop_charge(self);

	// if we're blind firing, this needs to be turned off here
	self->monsterInfo.aiFlags &= ~AI_MANUAL_STEERING;

	if (level.time < self->pain_debounce_time) {
		if ((self->velocity[_Z] > 100) && ((self->monsterInfo.active_move == &soldier_move_pain1) || (self->monsterInfo.active_move == &soldier_move_pain2) || (self->monsterInfo.active_move == &soldier_move_pain3))) {
			// PMM - clear duck flag
			if (self->monsterInfo.aiFlags & AI_DUCKED)
				monster_duck_up(self);
			M_SetAnimation(self, &soldier_move_pain4);
			soldierh_hyper_laser_sound_end(self);
		}
		return;
	}

	self->pain_debounce_time = level.time + 3_sec;

	n = self->count | 1;
	if (n == 1)
		gi.sound(self, CHAN_VOICE, sound_pain_light, 1, ATTN_NORM, 0);
	else if (n == 3)
		gi.sound(self, CHAN_VOICE, sound_pain, 1, ATTN_NORM, 0);
	else
		gi.sound(self, CHAN_VOICE, sound_pain_ss, 1, ATTN_NORM, 0);

	if (self->velocity[_Z] > 100) {
		// PMM - clear duck flag
		if (self->monsterInfo.aiFlags & AI_DUCKED)
			monster_duck_up(self);
		M_SetAnimation(self, &soldier_move_pain4);
		soldierh_hyper_laser_sound_end(self);
		return;
	}

	if (!M_ShouldReactToPain(self, mod))
		return; // no pain anims in nightmare

	r = frandom();

	if (r < 0.33f)
		M_SetAnimation(self, &soldier_move_pain1);
	else if (r < 0.66f)
		M_SetAnimation(self, &soldier_move_pain2);
	else
		M_SetAnimation(self, &soldier_move_pain3);

	// PMM - clear duck flag
	if (self->monsterInfo.aiFlags & AI_DUCKED)
		monster_duck_up(self);
	soldierh_hyper_laser_sound_end(self);
}

MONSTERINFO_SETSKIN(soldier_setskin) (gentity_t *self) -> void {
	if (self->health < (self->maxHealth / 2))
		self->s.skinNum |= 1;
	else
		self->s.skinNum &= ~1;
}

//
// ATTACK
//

constexpr MonsterMuzzleFlashID blaster_flash[] = { MZ2_SOLDIER_BLASTER_1, MZ2_SOLDIER_BLASTER_2, MZ2_SOLDIER_BLASTER_3, MZ2_SOLDIER_BLASTER_4, MZ2_SOLDIER_BLASTER_5, MZ2_SOLDIER_BLASTER_6, MZ2_SOLDIER_BLASTER_7, MZ2_SOLDIER_BLASTER_8, MZ2_SOLDIER_BLASTER_9 };
constexpr MonsterMuzzleFlashID shotgun_flash[] = { MZ2_SOLDIER_SHOTGUN_1, MZ2_SOLDIER_SHOTGUN_2, MZ2_SOLDIER_SHOTGUN_3, MZ2_SOLDIER_SHOTGUN_4, MZ2_SOLDIER_SHOTGUN_5, MZ2_SOLDIER_SHOTGUN_6, MZ2_SOLDIER_SHOTGUN_7, MZ2_SOLDIER_SHOTGUN_8, MZ2_SOLDIER_SHOTGUN_9 };
constexpr MonsterMuzzleFlashID machinegun_flash[] = { MZ2_SOLDIER_MACHINEGUN_1, MZ2_SOLDIER_MACHINEGUN_2, MZ2_SOLDIER_MACHINEGUN_3, MZ2_SOLDIER_MACHINEGUN_4, MZ2_SOLDIER_MACHINEGUN_5, MZ2_SOLDIER_MACHINEGUN_6, MZ2_SOLDIER_MACHINEGUN_7, MZ2_SOLDIER_MACHINEGUN_8, MZ2_SOLDIER_MACHINEGUN_9 };

static void soldier_fire_vanilla(gentity_t *self, int flash_number, bool angle_limited) {
	Vector3					 start;
	Vector3					 forward, right, up;
	Vector3					 aim;
	Vector3					 dir;
	Vector3					 end;
	float					 r, u;
	MonsterMuzzleFlashID flash_index;
	Vector3					 aim_norm;
	float					 angle;
	Vector3					 aim_good;

	if (self->count < 2)
		flash_index = blaster_flash[flash_number];
	else if (self->count < 4)
		flash_index = shotgun_flash[flash_number];
	else
		flash_index = machinegun_flash[flash_number];

	AngleVectors(self->s.angles, forward, right, nullptr);
	start = M_ProjectFlashSource(self, monster_flash_offset[flash_index], forward, right);

	if (flash_number == 5 || flash_number == 6) // he's dead
	{
		if (self->spawnFlags.has(SPAWNFLAG_MONSTER_CORPSE))
			return;

		aim = forward;
	} else {
		if ((!self->enemy) || (!self->enemy->inUse)) {
			self->monsterInfo.aiFlags &= ~AI_HOLD_FRAME;
			return;
		}

		if (self->monsterInfo.attackState == MonsterAttackState::Blind)
			end = self->monsterInfo.blind_fire_target;
		else
			end = self->enemy->s.origin;
		
		end[2] += self->enemy->viewHeight;
		aim = end - start;
		aim_good = end;
		
		if (angle_limited) {
			aim_norm = aim;
			aim_norm.normalize();
			angle = aim_norm.dot(forward);
			if (angle < 0.5f) // ~25 degree angle
			{
				if (level.time >= self->monsterInfo.fireWait)
					self->monsterInfo.aiFlags &= ~AI_HOLD_FRAME;
				else
					self->monsterInfo.aiFlags |= AI_HOLD_FRAME;

				return;
			}
		}

		dir = VectorToAngles(aim);
		AngleVectors(dir, forward, right, up);

		r = crandom() * 1000;
		u = crandom() * 500;

		end = start + (forward * 8192);
		end += (right * r);
		end += (up * u);

		aim = end - start;
		aim.normalize();
	}

	if (self->count <= 1) {
		monster_fire_blaster(self, start, aim, 5, 600, flash_index, EF_BLASTER);
	} else if (self->count <= 3) {
		monster_fire_shotgun(self, start, aim, 2, 1, 1500, 750, 9, flash_index);
		// [Paril-KEX] indicates to soldier that he must cock
		self->dmg = 1;
	} else {
		// PMM - changed to wait from pauseTime to not interfere with dodge code
		if (!(self->monsterInfo.aiFlags & AI_HOLD_FRAME))
			self->monsterInfo.fireWait = level.time + random_time(300_ms, 1.1_sec);

		monster_fire_bullet(self, start, aim, 2, 4, DEFAULT_BULLET_HSPREAD, DEFAULT_BULLET_VSPREAD, flash_index);

		if (level.time >= self->monsterInfo.fireWait)
			self->monsterInfo.aiFlags &= ~AI_HOLD_FRAME;
		else
			self->monsterInfo.aiFlags |= AI_HOLD_FRAME;
	}
}

static PRETHINK(soldierh_laser_update) (gentity_t *laser) -> void {
	gentity_t *self = laser->owner;

	Vector3 forward, right, up;
	Vector3 start;
	Vector3 tempvec;

	AngleVectors(self->s.angles, forward, right, up);
	start = self->s.origin;
	tempvec = monster_flash_offset[self->splashDamage];
	start += (forward * tempvec[0]);
	start += (right * tempvec[1]);
	start += (up * (tempvec[2] + 6));

	if (!self->deadFlag)
		PredictAim(self, self->enemy, start, 0, false, frandom(0.1f, 0.2f), &forward, nullptr);

	laser->s.origin = start;
	laser->moveDir = forward;
	gi.linkEntity(laser);
	dabeam_update(laser, false);
}

static void soldierh_laserbeam(gentity_t *self, int flash_index) {
	self->splashDamage = flash_index;
	monster_fire_dabeam(self, 1, false, soldierh_laser_update);
}

constexpr MonsterMuzzleFlashID ripper_flash[] = { MZ2_SOLDIER_RIPPER_1, MZ2_SOLDIER_RIPPER_2, MZ2_SOLDIER_RIPPER_3, MZ2_SOLDIER_RIPPER_4, MZ2_SOLDIER_RIPPER_5, MZ2_SOLDIER_RIPPER_6, MZ2_SOLDIER_RIPPER_7, MZ2_SOLDIER_RIPPER_8, MZ2_SOLDIER_RIPPER_9 };
constexpr MonsterMuzzleFlashID hyper_flash[] = { MZ2_SOLDIER_HYPERGUN_1, MZ2_SOLDIER_HYPERGUN_2, MZ2_SOLDIER_HYPERGUN_3, MZ2_SOLDIER_HYPERGUN_4, MZ2_SOLDIER_HYPERGUN_5, MZ2_SOLDIER_HYPERGUN_6, MZ2_SOLDIER_HYPERGUN_7, MZ2_SOLDIER_HYPERGUN_8, MZ2_SOLDIER_HYPERGUN_9 };

static void soldier_fire_xatrix(gentity_t *self, int flash_number, bool angle_limited) {
	Vector3					 start;
	Vector3					 forward, right, up;
	Vector3					 aim;
	Vector3					 dir;
	Vector3					 end;
	float					 r, u;
	MonsterMuzzleFlashID flash_index;
	Vector3					 aim_norm;
	float					 angle;
	Vector3					 aim_good;

	if (self->count < 2)
		flash_index = ripper_flash[flash_number]; // ripper
	else if (self->count < 4)
		flash_index = hyper_flash[flash_number]; // hyperblaster
	else
		flash_index = machinegun_flash[flash_number]; // laserbeam

	AngleVectors(self->s.angles, forward, right, nullptr);
	start = M_ProjectFlashSource(self, monster_flash_offset[flash_index], forward, right);

	if (flash_number == 5 || flash_number == 6) {
		if (self->spawnFlags.has(SPAWNFLAG_MONSTER_CORPSE))
			return;

		aim = forward;
	} else {
		// [Paril-KEX] no enemy = no fire
		if ((!self->enemy) || (!self->enemy->inUse)) {
			self->monsterInfo.aiFlags &= ~AI_HOLD_FRAME;
			return;
		}

		if (self->monsterInfo.attackState == MonsterAttackState::Blind)
			end = self->monsterInfo.blind_fire_target;
		else
			end = self->enemy->s.origin;
		
		end[2] += self->enemy->viewHeight;

		aim = end - start;
		aim_good = end;

		if (angle_limited) {
			aim_norm = aim;
			aim_norm.normalize();
			angle = aim_norm.dot(forward);

			if (angle < 0.5f) // ~25 degree angle
			{
				if (level.time >= self->monsterInfo.fireWait)
					self->monsterInfo.aiFlags &= ~AI_HOLD_FRAME;
				else
					self->monsterInfo.aiFlags |= AI_HOLD_FRAME;

				return;
			}
		}

		dir = VectorToAngles(aim);
		AngleVectors(dir, forward, right, up);

		r = crandom() * 100;
		u = crandom() * 50;
		end = start + (forward * 8192);
		end += (right * r);
		end += (up * u);

		aim = end - start;
		aim.normalize();
	}

	if (self->count <= 1) {
		monster_fire_ionripper(self, start, aim, 5, 600, flash_index, EF_IONRIPPER);
	} else if (self->count <= 3) {
		monster_fire_blueblaster(self, start, aim, 1, 600, flash_index, EF_BLUEHYPERBLASTER);
	} else {
		// PMM - changed to wait from pauseTime to not interfere with dodge code
		if (!(self->monsterInfo.aiFlags & AI_HOLD_FRAME))
			self->monsterInfo.fireWait = level.time + random_time(300_ms, 1.1_sec);

		soldierh_laserbeam(self, flash_index);

		if (level.time >= self->monsterInfo.fireWait)
			self->monsterInfo.aiFlags &= ~AI_HOLD_FRAME;
		else
			self->monsterInfo.aiFlags |= AI_HOLD_FRAME;
	}
}

static void soldier_fire(gentity_t *self, int flash_number, bool angle_limited) {
	if (self->style == 1)
		soldier_fire_xatrix(self, flash_number, angle_limited);
	else
		soldier_fire_vanilla(self, flash_number, angle_limited);
}

// ATTACK1 (blaster/shotgun)

static void soldier_fire1(gentity_t *self) {
	soldier_fire(self, 0, false);
}

static void soldier_attack1_refire1(gentity_t *self) {
	// [Paril-KEX]
	if (self->count <= 0)
		self->monsterInfo.nextFrame = FRAME_attack110;

	if (self->monsterInfo.aiFlags & AI_MANUAL_STEERING) {
		self->monsterInfo.aiFlags &= ~AI_MANUAL_STEERING;
		return;
	}

	if (!self->enemy)
		return;

	if (self->count > 1)
		return;

	if (self->enemy->health <= 0)
		return;

	if (((frandom() < 0.5f) && visible(self, self->enemy)) || (range_to(self, self->enemy) <= RANGE_MELEE))
		self->monsterInfo.nextFrame = FRAME_attack102;
	else
		self->monsterInfo.nextFrame = FRAME_attack110;
}

static void soldier_attack1_refire2(gentity_t *self) {
	if (!self->enemy)
		return;

	if (self->count < 2)
		return;

	if (self->enemy->health <= 0)
		return;

	if (((self->splashDamage || frandom() < 0.5f) && visible(self, self->enemy)) || (range_to(self, self->enemy) <= RANGE_MELEE)) {
		self->monsterInfo.nextFrame = FRAME_attack102;
		self->splashDamage = 0;
	}
}

static void soldier_attack1_shotgun_check(gentity_t *self) {
	if (self->dmg) {
		self->monsterInfo.nextFrame = FRAME_attack106;
		// [Paril-KEX] indicate that we should force a refire
		self->splashDamage = 1;
	}
}

static void soldier_blind_check(gentity_t *self) {
	if (self->monsterInfo.aiFlags & AI_MANUAL_STEERING) {
		Vector3 aim = self->monsterInfo.blind_fire_target - self->s.origin;
		self->ideal_yaw = vectoyaw(aim);
	}
}

MonsterFrame soldier_frames_attack1[] = {
	{ ai_charge, 0, soldier_blind_check },
	{ ai_charge, 0, soldier_attack1_shotgun_check },
	{ ai_charge, 0, soldier_fire1 },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge, 0, soldier_attack1_refire1 },
	{ ai_charge },
	{ ai_charge, 0, soldier_cock },
	{ ai_charge, 0, soldier_attack1_refire2 },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge }
};
MMOVE_T(soldier_move_attack1) = { FRAME_attack101, FRAME_attack112, soldier_frames_attack1, soldier_run };

// ATTACK1 (blaster/shotgun)
static void soldierh_hyper_refire1(gentity_t *self) {
	if (!self->enemy)
		return;

	if (self->count >= 2 && self->count < 4) {
		if (frandom() < 0.7f && visible(self, self->enemy))
			self->s.frame = FRAME_attack103;
	}
}

static void soldierh_hyperripper1(gentity_t *self) {
	if (self->count < 4)
		soldier_fire(self, 0, false);
}

MonsterFrame soldierh_frames_attack1[] = {
	{ ai_charge, 0, soldier_blind_check },
	{ ai_charge, 0, soldierh_hyper_laser_sound_start },
	{ ai_charge, 0, soldier_fire1 },
	{ ai_charge, 0, soldierh_hyperripper1 },
	{ ai_charge, 0, soldierh_hyperripper1 },
	{ ai_charge, 0, soldier_attack1_refire1 },
	{ ai_charge, 0, soldierh_hyper_refire1 },
	{ ai_charge, 0, soldier_cock },
	{ ai_charge, 0, soldier_attack1_refire2 },
	{ ai_charge, 0, soldierh_hyper_laser_sound_end },
	{ ai_charge },
	{ ai_charge }
};
MMOVE_T(soldierh_move_attack1) = { FRAME_attack101, FRAME_attack112, soldierh_frames_attack1, soldier_run };

// ATTACK2 (blaster/shotgun)

static void soldier_fire2(gentity_t *self) {
	soldier_fire(self, 1, false);
}

static void soldier_attack2_refire1(gentity_t *self) {
	if (self->count <= 0)
		self->monsterInfo.nextFrame = FRAME_attack216;

	if (!self->enemy)
		return;

	if (self->count > 1)
		return;

	if (self->enemy->health <= 0)
		return;

	if (((frandom() < 0.5f) && visible(self, self->enemy)) || (range_to(self, self->enemy) <= RANGE_MELEE))
		self->monsterInfo.nextFrame = FRAME_attack204;
}

static void soldier_attack2_refire2(gentity_t *self) {
	if (!self->enemy)
		return;

	if (self->count < 2)
		return;

	if (self->enemy->health <= 0)
		return;

	if (((self->splashDamage || frandom() < 0.5f) && visible(self, self->enemy)) || ((self->style == 0 || self->count < 4) && (range_to(self, self->enemy) <= RANGE_MELEE))) {
		self->monsterInfo.nextFrame = FRAME_attack204;
		self->splashDamage = 0;
	}
}

static void soldier_attack2_shotgun_check(gentity_t *self) {
	if (self->dmg) {
		self->monsterInfo.nextFrame = FRAME_attack210;
		// [Paril-KEX] indicate that we should force a refire
		self->splashDamage = 1;
	}
}

MonsterFrame soldier_frames_attack2[] = {
	{ ai_charge },
	{ ai_charge },
	{ ai_charge, 0, soldier_attack2_shotgun_check },
	{ ai_charge },
	{ ai_charge, 0, soldier_fire2 },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge, 0, soldier_attack2_refire1 },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge, 0, soldier_cock },
	{ ai_charge },
	{ ai_charge, 0, soldier_attack2_refire2 },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge }
};
MMOVE_T(soldier_move_attack2) = { FRAME_attack201, FRAME_attack218, soldier_frames_attack2, soldier_run };

static void soldierh_hyper_refire2(gentity_t *self) {
	if (!self->enemy)
		return;

	if (self->count < 2)
		return;
	else if (self->count < 4) {
		if (frandom() < 0.7f && visible(self, self->enemy))
			self->s.frame = FRAME_attack205;
	}
}

static void soldierh_hyperripper2(gentity_t *self) {
	if (self->count < 4)
		soldier_fire(self, 1, false);
}

MonsterFrame soldierh_frames_attack2[] = {
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge, 0, soldierh_hyper_laser_sound_start },
	{ ai_charge, 0, soldier_fire2 },
	{ ai_charge, 0, soldierh_hyperripper2 },
	{ ai_charge, 0, soldierh_hyperripper2 },
	{ ai_charge, 0, soldier_attack2_refire1 },
	{ ai_charge, 0, soldierh_hyper_refire2 },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge, 0, soldier_cock },
	{ ai_charge },
	{ ai_charge, 0, soldier_attack2_refire2 },
	{ ai_charge, 0, soldierh_hyper_laser_sound_end },
	{ ai_charge },
	{ ai_charge }
};
MMOVE_T(soldierh_move_attack2) = { FRAME_attack201, FRAME_attack218, soldierh_frames_attack2, soldier_run };

// ATTACK3 (duck and shoot)
static void soldier_fire3(gentity_t *self) {
	soldier_fire(self, 2, false);
}

static void soldierh_hyperripper3(gentity_t *self) {
	if (self->s.skinNum >= 6 && self->count < 4)
		soldier_fire(self, 2, false);
}

static void soldier_attack3_refire(gentity_t *self) {
	if (self->dmg)
		monster_duck_hold(self);
	else if ((level.time + 400_ms) < self->monsterInfo.duck_wait_time)
		self->monsterInfo.nextFrame = FRAME_attack303;
}

MonsterFrame soldier_frames_attack3[] = {
	{ ai_charge, 0, monster_duck_down },
	{ ai_charge, 0, soldierh_hyper_laser_sound_start },
	{ ai_charge, 0, soldier_fire3 },
	{ ai_charge, 0, soldierh_hyperripper3 },
	{ ai_charge, 0, soldierh_hyperripper3 },
	{ ai_charge, 0, soldier_attack3_refire },
	{ ai_charge, 0, monster_duck_up },
	{ ai_charge, 0, soldierh_hyper_laser_sound_end },
	{ ai_charge }
};
MMOVE_T(soldier_move_attack3) = { FRAME_attack301, FRAME_attack309, soldier_frames_attack3, soldier_run };

// ATTACK4 (machinegun)

static void soldier_fire4(gentity_t *self) {
	soldier_fire(self, 3, false);
}

MonsterFrame soldier_frames_attack4[] = {
	{ ai_charge },
	{ ai_charge, 0, soldierh_hyper_laser_sound_start },
	{ ai_charge, 0, soldier_fire4 },
	{ ai_charge, 0, soldierh_hyper_laser_sound_end },
	{ ai_charge },
	{ ai_charge }
};
MMOVE_T(soldier_move_attack4) = { FRAME_attack401, FRAME_attack406, soldier_frames_attack4, soldier_run };

// ATTACK6 (run & shoot)

static void soldier_fire8(gentity_t *self) {
	soldier_fire(self, 7, true);
}

static void soldier_attack6_refire1(gentity_t *self) {
	// PMM - make sure dodge & charge bits are cleared
	monster_done_dodge(self);
	soldier_stop_charge(self);

	if (!self->enemy)
		return;

	if (self->count > 1)
		return;

	if (self->enemy->health <= 0 ||
		range_to(self, self->enemy) < RANGE_NEAR ||
		!visible(self, self->enemy)) // don't endlessly run into walls
	{
		soldier_run(self);
		return;
	}

	if (frandom() < 0.25f)
		self->monsterInfo.nextFrame = FRAME_runs03;
	else
		soldier_run(self);
}

static void soldier_attack6_refire2(gentity_t *self) {
	// PMM - make sure dodge & charge bits are cleared
	monster_done_dodge(self);
	soldier_stop_charge(self);

	if (!self->enemy || self->count <= 0)
		return;

	if (self->enemy->health <= 0 ||
		(!self->splashDamage && range_to(self, self->enemy) < RANGE_NEAR) ||
		!visible(self, self->enemy)) // don't endlessly run into walls
	{
		soldierh_hyper_laser_sound_end(self);
		return;
	}

	if (self->splashDamage || frandom() < 0.25f) {
		self->monsterInfo.nextFrame = FRAME_runs03;
		self->splashDamage = 0;
	}
}

static void soldier_attack6_shotgun_check(gentity_t *self) {
	if (self->dmg) {
		self->monsterInfo.nextFrame = FRAME_runs09;
		// [Paril-KEX] indicate that we should force a refire
		self->splashDamage = 1;
	}
}

static void soldierh_hyperripper8(gentity_t *self) {
	if (self->s.skinNum >= 6 && self->count < 4)
		soldier_fire(self, 7, true);
}

MonsterFrame soldier_frames_attack6[] = {
	{ ai_run, 10, soldier_start_charge },
	{ ai_run, 4, soldier_attack6_shotgun_check },
	{ ai_run, 12, soldierh_hyper_laser_sound_start },
	{ ai_run, 11, [](gentity_t *self) { soldier_fire8(self); monster_footstep(self); } },
	{ ai_run, 13, [](gentity_t *self) { soldierh_hyperripper8(self); monster_done_dodge(self); } },
	{ ai_run, 18, soldierh_hyperripper8 },
	{ ai_run, 15, monster_footstep },
	{ ai_run, 14, soldier_attack6_refire1 },
	{ ai_run, 11 },
	{ ai_run, 8, monster_footstep },
	{ ai_run, 11, soldier_cock },
	{ ai_run, 12 },
	{ ai_run, 12, monster_footstep },
	{ ai_run, 17, soldier_attack6_refire2 }
};
MMOVE_T(soldier_move_attack6) = { FRAME_runs01, FRAME_runs14, soldier_frames_attack6, soldier_run, 0.65f };

MONSTERINFO_ATTACK(soldier_attack) (gentity_t *self) -> void {
	float r, chance;

	monster_done_dodge(self);

	// blindFire!
	if (self->monsterInfo.attackState == MonsterAttackState::Blind) {
		// setup shot probabilities
		if (self->monsterInfo.blind_fire_delay < 1_sec)
			chance = 1.0f;
		else if (self->monsterInfo.blind_fire_delay < 7.5_sec)
			chance = 0.4f;
		else
			chance = 0.1f;

		r = frandom();

		// minimum of 4.1 seconds, plus 0-3, after the shots are done
		self->monsterInfo.blind_fire_delay += 4.1_sec + random_time(3_sec);

		// don't shoot at the origin
		if (!self->monsterInfo.blind_fire_target)
			return;

		// don't shoot if the dice say not to
		if (r > chance)
			return;

		// turn on manual steering to signal both manual steering and blindFire
		self->monsterInfo.aiFlags |= AI_MANUAL_STEERING;

		if (self->style == 1)
			M_SetAnimation(self, &soldierh_move_attack1);
		else
			M_SetAnimation(self, &soldier_move_attack1);
		self->monsterInfo.attackFinished = level.time + random_time(1.5_sec, 2.5_sec);
		return;
	}

	// PMM - added this so the soldiers now run toward you and shoot instead of just stopping and shooting
	r = frandom();

	// nb: run-shoot not limited by `M_CheckClearShot` since they will be far enough
	// away that it doesn't matter

	if ((!(self->monsterInfo.aiFlags & (AI_BLOCKED | AI_STAND_GROUND))) &&
		(r < 0.25f &&
			(self->count <= 3)) &&
		(range_to(self, self->enemy) >= (RANGE_NEAR * 0.5f))) {
		M_SetAnimation(self, &soldier_move_attack6);
	} else {
		if (self->count < 4) {
			bool attack1_possible;

			// [Paril-KEX] shotgun guard only uses attack2 at close range
			if ((!self->style && self->count >= 2 && self->count <= 3) && range_to(self, self->enemy) <= (RANGE_NEAR * 0.65f))
				attack1_possible = false;
			else
				attack1_possible = M_CheckClearShot(self, monster_flash_offset[MZ2_SOLDIER_BLASTER_1]);

			bool attack2_possible = M_CheckClearShot(self, monster_flash_offset[MZ2_SOLDIER_BLASTER_2]);

			if (attack1_possible && (!attack2_possible || frandom() < 0.5f)) {
				if (self->style == 1)
					M_SetAnimation(self, &soldierh_move_attack1);
				else
					M_SetAnimation(self, &soldier_move_attack1);
			} else if (attack2_possible) {
				if (self->style == 1)
					M_SetAnimation(self, &soldierh_move_attack2);
				else
					M_SetAnimation(self, &soldier_move_attack2);
			}
		} else if (M_CheckClearShot(self, monster_flash_offset[MZ2_SOLDIER_MACHINEGUN_4])) {
			M_SetAnimation(self, &soldier_move_attack4);
		}
	}
}

//
// SIGHT
//

MONSTERINFO_SIGHT(soldier_sight) (gentity_t *self, gentity_t *other) -> void {
	if (frandom() < 0.5f)
		gi.sound(self, CHAN_VOICE, sound_sight1, 1, ATTN_NORM, 0);
	else
		gi.sound(self, CHAN_VOICE, sound_sight2, 1, ATTN_NORM, 0);

	if (self->enemy && (range_to(self, self->enemy) >= RANGE_NEAR) &&
		visible(self, self->enemy) // Paril: don't run-shoot if we can't see them
		) {
		if (self->style == 1 || frandom() > 0.75f) {
			// don't use run+shoot for machinegun/laser because
			// the animation is a bit weird
			if (self->count < 4)
				M_SetAnimation(self, &soldier_move_attack6);
			else if (M_CheckClearShot(self, monster_flash_offset[MZ2_SOLDIER_MACHINEGUN_4]))
				M_SetAnimation(self, &soldier_move_attack4);
		}
	}
}

//
// DUCK
//
MonsterFrame soldier_frames_duck[] = {
	{ ai_move, 5, monster_duck_down },
	{ ai_move, -1, monster_duck_hold },
	{ ai_move, 1 },
	{ ai_move, 0, monster_duck_up },
	{ ai_move, 5 }
};
MMOVE_T(soldier_move_duck) = { FRAME_duck01, FRAME_duck05, soldier_frames_duck, soldier_run };

extern const MonsterMove soldier_move_trip;

static void soldier_stand_up(gentity_t *self) {
	soldierh_hyper_laser_sound_end(self);
	M_SetAnimation(self, &soldier_move_trip, false);
	self->monsterInfo.nextFrame = FRAME_runt08;
}

static bool soldier_prone_shoot_ok(gentity_t *self) {
	if (!self->enemy || !self->enemy->inUse)
		return false;

	Vector3 fwd;
	AngleVectors(self->s.angles, fwd, nullptr, nullptr);

	Vector3 diff = self->enemy->s.origin - self->s.origin;
	diff.z = 0;
	diff.normalize();

	float v = fwd.dot(diff);

	if (v < 0.80f)
		return false;

	return true;
}

static void ai_soldier_move(gentity_t *self, float dist) {
	ai_move(self, dist);

	if (!soldier_prone_shoot_ok(self)) {
		soldier_stand_up(self);
		return;
	}
}

static void soldier_fire5(gentity_t *self) {
	soldier_fire(self, 8, true);
}

static void soldierh_hyperripper5(gentity_t *self) {
	if (self->style && self->count < 4)
		soldier_fire(self, 8, true);
}

MonsterFrame soldier_frames_attack5[] = {
	{ ai_move, 18, monster_duck_down },
	{ ai_move, 11, monster_footstep },
	{ ai_move, 0, monster_footstep },
	{ ai_soldier_move },
	{ ai_soldier_move, 0, soldierh_hyper_laser_sound_start },
	{ ai_soldier_move, 0, soldier_fire5 },
	{ ai_soldier_move, 0, soldierh_hyperripper5 },
	{ ai_soldier_move, 0, soldierh_hyperripper5 },
};
MMOVE_T(soldier_move_attack5) = { FRAME_attack501, FRAME_attack508, soldier_frames_attack5, soldier_stand_up };

static void monster_check_prone(gentity_t *self) {
	// we're a shotgun guard waiting to cock
	if (!self->style && self->count >= 2 && self->count <= 3 && self->dmg)
		return;

	// not going to shoot at this angle
	if (!soldier_prone_shoot_ok(self))
		return;

	M_SetAnimation(self, &soldier_move_attack5, false);
}

MonsterFrame soldier_frames_trip[] = {
	{ ai_move, 10 },
	{ ai_move, 2, monster_check_prone },
	{ ai_move, 18, monster_duck_down },
	{ ai_move, 11, monster_footstep },
	{ ai_move, 9 },
	{ ai_move, -11, monster_footstep },
	{ ai_move, -2 },
	{ ai_move, 0 },
	{ ai_move, 6 },
	{ ai_move, -5 },
	{ ai_move, 0 },
	{ ai_move, 1 },
	{ ai_move, 0, monster_footstep },
	{ ai_move, 0, monster_duck_up },
	{ ai_move, 3 },
	{ ai_move, 2, monster_footstep },
	{ ai_move, -1 },
	{ ai_move, 2 },
	{ ai_move, 0 },
};
MMOVE_T(soldier_move_trip) = { FRAME_runt01, FRAME_runt19, soldier_frames_trip, soldier_run };

// pmm - blocking code

MONSTERINFO_BLOCKED(soldier_blocked) (gentity_t *self, float dist) -> bool {
	// don't do anything if you're dodging
	if ((self->monsterInfo.aiFlags & AI_DODGING) || (self->monsterInfo.aiFlags & AI_DUCKED))
		return false;

	return blocked_checkplat(self, dist);
}

//
// DEATH
//

static void soldier_fire6(gentity_t *self) {
	soldier_fire(self, 5, false);

	if (self->dmg)
		self->monsterInfo.nextFrame = FRAME_death126;
}

static void soldier_fire7(gentity_t *self) {
	soldier_fire(self, 6, false);
}

static void soldier_dead(gentity_t *self) {
	self->mins = { -16, -16, -24 };
	self->maxs = { 16, 16, -8 };
	monster_dead(self);
}

static void soldier_death_shrink(gentity_t *self) {
	self->svFlags |= SVF_DEADMONSTER;
	self->maxs[2] = 0;
	gi.linkEntity(self);
}

MonsterFrame soldier_frames_death1[] = {
	{ ai_move },
	{ ai_move, -10 },
	{ ai_move, -10 },
	{ ai_move, -10, soldier_death_shrink },
	{ ai_move, -5 },
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

	{ ai_move, 0, soldierh_hyper_laser_sound_start },
	{ ai_move, 0, soldier_fire6 },
	{ ai_move },
	{ ai_move },
	{ ai_move, 0, soldier_fire7 },
	{ ai_move, 0, soldierh_hyper_laser_sound_end },
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
MMOVE_T(soldier_move_death1) = { FRAME_death101, FRAME_death136, soldier_frames_death1, soldier_dead };

MonsterFrame soldier_frames_death2[] = {
	{ ai_move, -5 },
	{ ai_move, -5 },
	{ ai_move, -5 },
	{ ai_move, 0, soldier_death_shrink },
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
MMOVE_T(soldier_move_death2) = { FRAME_death201, FRAME_death235, soldier_frames_death2, soldier_dead };

MonsterFrame soldier_frames_death3[] = {
	{ ai_move, -5 },
	{ ai_move, -5 },
	{ ai_move, -5 },
	{ ai_move, 0, soldier_death_shrink },
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
};
MMOVE_T(soldier_move_death3) = { FRAME_death301, FRAME_death345, soldier_frames_death3, soldier_dead };

MonsterFrame soldier_frames_death4[] = {
	{ ai_move },
	{ ai_move },
	{ ai_move, 1.5f },
	{ ai_move, 2.5f },
	{ ai_move, -1.5f },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move, -0.5f },
	{ ai_move },

	{ ai_move },
	{ ai_move, 4.0f },
	{ ai_move, 4.0f },
	{ ai_move, 8.0f, soldier_death_shrink },
	{ ai_move, 8.0f },
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
	{ ai_move, 5.5f },

	{ ai_move, 2.5f },
	{ ai_move, -2.0f },
	{ ai_move, -2.0f }
};
MMOVE_T(soldier_move_death4) = { FRAME_death401, FRAME_death453, soldier_frames_death4, soldier_dead };

MonsterFrame soldier_frames_death5[] = {
	{ ai_move, -5 },
	{ ai_move, -5 },
	{ ai_move, -5 },
	{ ai_move },
	{ ai_move },
	{ ai_move, 0, soldier_death_shrink },
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
MMOVE_T(soldier_move_death5) = { FRAME_death501, FRAME_death524, soldier_frames_death5, soldier_dead };

MonsterFrame soldier_frames_death6[] = {
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move, 0, soldier_death_shrink },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move }
};
MMOVE_T(soldier_move_death6) = { FRAME_death601, FRAME_death610, soldier_frames_death6, soldier_dead };

static DIE(soldier_die) (gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, const Vector3 &point, const MeansOfDeath &mod) -> void {
	int n;

	soldierh_hyper_laser_sound_end(self);

	// check for gib
	if (M_CheckGib(self, mod)) {
		gi.sound(self, CHAN_VOICE, gi.soundIndex("misc/udeath.wav"), 1, ATTN_NORM, 0);

		self->s.skinNum /= 2;

		if (self->beam) {
			FreeEntity(self->beam);
			self->beam = nullptr;
		}

		ThrowGibs(self, damage, {
			{ 3, "models/objects/gibs/sm_meat/tris.md2" },
			{ "models/objects/gibs/bone2/tris.md2" },
			{ "models/objects/gibs/bone/tris.md2" },
			{ "models/monsters/soldier/gibs/arm.md2", GIB_SKINNED },
			{ "models/monsters/soldier/gibs/gun.md2", GIB_SKINNED | GIB_UPRIGHT },
			{ "models/monsters/soldier/gibs/chest.md2", GIB_SKINNED },
			{ "models/monsters/soldier/gibs/head.md2", GIB_HEAD | GIB_SKINNED }
			});
		self->deadFlag = true;
		return;
	}

	if (self->deadFlag)
		return;

	// regular death
	self->deadFlag = true;
	self->takeDamage = true;

	n = self->count | 1;

	if (n == 1)
		gi.sound(self, CHAN_VOICE, sound_death_light, 1, ATTN_NORM, 0);
	else if (n == 3)
		gi.sound(self, CHAN_VOICE, sound_death, 1, ATTN_NORM, 0);
	else // (n == 5)
		gi.sound(self, CHAN_VOICE, sound_death_ss, 1, ATTN_NORM, 0);

	if (std::fabs((self->s.origin[_Z] + self->viewHeight) - point[2]) <= 4 &&
		self->velocity.z < 65.f) {
		// head shot
		M_SetAnimation(self, &soldier_move_death3);
		return;
	}

	// if we die while on the ground, do a quicker death4
	if (self->monsterInfo.active_move == &soldier_move_trip ||
		self->monsterInfo.active_move == &soldier_move_attack5) {
		M_SetAnimation(self, &soldier_move_death4);
		self->monsterInfo.nextFrame = FRAME_death413;
		soldier_death_shrink(self);
		return;
	}

	// only do the spin-death if we have enough velocity to justify it
	if (self->velocity.z > 65.f || self->velocity.length() > 150.f)
		n = irandom(5);
	else
		n = irandom(4);

	if (n == 0)
		M_SetAnimation(self, &soldier_move_death1);
	else if (n == 1)
		M_SetAnimation(self, &soldier_move_death2);
	else if (n == 2)
		M_SetAnimation(self, &soldier_move_death4);
	else if (n == 3)
		M_SetAnimation(self, &soldier_move_death5);
	else
		M_SetAnimation(self, &soldier_move_death6);
}

//
// NEW DODGE CODE
//

MONSTERINFO_SIDESTEP(soldier_sidestep) (gentity_t *self) -> bool {
	// don't sideStep during trip or up pain
	if (self->monsterInfo.active_move == &soldier_move_trip ||
		self->monsterInfo.active_move == &soldier_move_attack5 ||
		self->monsterInfo.active_move == &soldier_move_pain4)
		return false;

	if (self->count <= 3) {
		if (self->monsterInfo.active_move != &soldier_move_attack6) {
			M_SetAnimation(self, &soldier_move_attack6);
			soldierh_hyper_laser_sound_end(self);
		}
	} else {
		if (self->monsterInfo.active_move != &soldier_move_start_run &&
			self->monsterInfo.active_move != &soldier_move_run) {
			M_SetAnimation(self, &soldier_move_start_run);
			soldierh_hyper_laser_sound_end(self);
		}
	}

	return true;
}

MONSTERINFO_DUCK(soldier_duck) (gentity_t *self, GameTime eta) -> bool {
	self->monsterInfo.aiFlags &= ~AI_HOLD_FRAME;

	if (self->monsterInfo.active_move == &soldier_move_attack6) {
		M_SetAnimation(self, &soldier_move_trip);
	} else if (self->dmg || brandom()) {
		M_SetAnimation(self, &soldier_move_duck);
	} else {
		M_SetAnimation(self, &soldier_move_attack3);
	}

	soldierh_hyper_laser_sound_end(self);
	return true;
}

void soldier_blind(gentity_t *self);

MonsterFrame soldier_frames_blind[] = {
	{ ai_move, 0, soldier_idle },
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
MMOVE_T(soldier_move_blind) = { FRAME_stand101, FRAME_stand130, soldier_frames_blind, soldier_blind };

MONSTERINFO_STAND(soldier_blind) (gentity_t *self) -> void {
	M_SetAnimation(self, &soldier_move_blind);
}

//
// SPAWN
//

constexpr SpawnFlags SPAWNFLAG_SOLDIER_BLIND = 8_spawnflag;

static void monster_soldier_x(gentity_t *self) {
	self->s.modelIndex = gi.modelIndex("models/monsters/soldier/tris.md2");
	self->monsterInfo.scale = MODEL_SCALE;
	self->mins = { -16, -16, -24 };
	self->maxs = { 16, 16, 32 };
	self->moveType = MoveType::Step;
	self->solid = SOLID_BBOX;

	sound_idle.assign("soldier/solidle1.wav");
	sound_sight1.assign("soldier/solsght1.wav");
	sound_sight2.assign("soldier/solsrch1.wav");
	sound_cock.assign("infantry/infatck3.wav");

	gi.modelIndex("models/monsters/soldier/gibs/head.md2");
	gi.modelIndex("models/monsters/soldier/gibs/gun.md2");
	gi.modelIndex("models/monsters/soldier/gibs/arm.md2");
	gi.modelIndex("models/monsters/soldier/gibs/chest.md2");

	self->mass = 100;

	self->pain = soldier_pain;
	self->die = soldier_die;

	self->monsterInfo.stand = soldier_stand;
	self->monsterInfo.walk = soldier_walk;
	self->monsterInfo.run = soldier_run;
	self->monsterInfo.dodge = M_MonsterDodge;
	self->monsterInfo.attack = soldier_attack;
	self->monsterInfo.melee = nullptr;
	self->monsterInfo.sight = soldier_sight;
	self->monsterInfo.setSkin = soldier_setskin;

	self->monsterInfo.blocked = soldier_blocked;
	self->monsterInfo.duck = soldier_duck;
	self->monsterInfo.unDuck = monster_duck_up;
	self->monsterInfo.sideStep = soldier_sidestep;

	if (self->spawnFlags.has(SPAWNFLAG_SOLDIER_BLIND)) // blind
		self->monsterInfo.stand = soldier_blind;

	gi.linkEntity(self);

	self->monsterInfo.stand(self);

	walkmonster_start(self);
}

/*QUAKED monster_soldier_light (1 .5 0) (-16 -16 -24) (16 16 32) AMBUSH TRIGGER_SPAWN SIGHT x CORPSE x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
 */
void SP_monster_soldier_light(gentity_t *self) {
	if (!M_AllowSpawn(self)) {
		FreeEntity(self);
		return;
	}

	monster_soldier_x(self);

	sound_pain_light.assign("soldier/solpain2.wav");
	sound_death_light.assign("soldier/soldeth2.wav");
	gi.modelIndex("models/objects/laser/tris.md2");
	gi.soundIndex("misc/lasfly.wav");
	gi.soundIndex("soldier/solatck2.wav");

	self->s.skinNum = 0;
	self->count = self->s.skinNum;
	self->health = self->maxHealth = 20 * st.health_multiplier;
	self->gibHealth = -30;

	// PMM - blindFire
	self->monsterInfo.blindFire = true;
}

/*QUAKED monster_soldier (1 .5 0) (-16 -16 -24) (16 16 32) AMBUSH TRIGGER_SPAWN SIGHT x CORPSE x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
 */
void SP_monster_soldier(gentity_t *self) {
	if (!M_AllowSpawn(self)) {
		FreeEntity(self);
		return;
	}

	monster_soldier_x(self);

	sound_pain.assign("soldier/solpain1.wav");
	sound_death.assign("soldier/soldeth1.wav");
	gi.soundIndex("soldier/solatck1.wav");

	self->s.skinNum = 2;
	self->count = self->s.skinNum;
	self->health = self->maxHealth = 30 * st.health_multiplier;
	self->gibHealth = -30;
}

/*QUAKED monster_soldier_ss (1 .5 0) (-16 -16 -24) (16 16 32) AMBUSH TRIGGER_SPAWN SIGHT x CORPSE x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
 */
void SP_monster_soldier_ss(gentity_t *self) {
	if (!M_AllowSpawn(self)) {
		FreeEntity(self);
		return;
	}

	monster_soldier_x(self);

	sound_pain_ss.assign("soldier/solpain3.wav");
	sound_death_ss.assign("soldier/soldeth3.wav");
	gi.soundIndex("soldier/solatck3.wav");

	self->s.skinNum = 4;
	self->count = self->s.skinNum;
	self->health = self->maxHealth = 40 * st.health_multiplier;
	self->gibHealth = -30;
}

//
// SPAWN
//

static void monster_soldier_h(gentity_t *self) {
	monster_soldier_x(self);
	self->style = 1;
}

/*QUAKED monster_soldier_ripper (1 .5 0) (-16 -16 -24) (16 16 32) AMBUSH TRIGGER_SPAWN SIGHT x CORPSE x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
 */
void SP_monster_soldier_ripper(gentity_t *self) {
	if (!M_AllowSpawn(self)) {
		FreeEntity(self);
		return;
	}

	monster_soldier_h(self);

	sound_pain_light.assign("soldier/solpain2.wav");
	sound_death_light.assign("soldier/soldeth2.wav");

	gi.modelIndex("models/objects/boomrang/tris.md2");
	gi.soundIndex("misc/lasfly.wav");
	gi.soundIndex("soldier/solatck2.wav");

	self->s.skinNum = 6;
	self->count = self->s.skinNum - 6;
	self->health = self->maxHealth = 50 * st.health_multiplier;
	self->gibHealth = -30;

	self->monsterInfo.blindFire = true;
}

/*QUAKED monster_soldier_hypergun (1 .5 0) (-16 -16 -24) (16 16 32) AMBUSH TRIGGER_SPAWN SIGHT x CORPSE x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
 */
void SP_monster_soldier_hypergun(gentity_t *self) {
	if (!M_AllowSpawn(self)) {
		FreeEntity(self);
		return;
	}

	monster_soldier_h(self);

	gi.modelIndex("models/objects/laser/tris.md2");
	sound_pain.assign("soldier/solpain1.wav");
	sound_death.assign("soldier/soldeth1.wav");
	gi.soundIndex("soldier/solatck1.wav");
	gi.soundIndex("weapons/hyprbd1a.wav");
	gi.soundIndex("weapons/hyprbl1a.wav");

	self->s.skinNum = 8;
	self->count = self->s.skinNum - 6;
	self->health = self->maxHealth = 60 * st.health_multiplier;
	self->gibHealth = -30;

	// PMM - blindFire
	self->monsterInfo.blindFire = true;
}

/*QUAKED monster_soldier_lasergun (1 .5 0) (-16 -16 -24) (16 16 32) AMBUSH TRIGGER_SPAWN SIGHT x CORPSE x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
 */
void SP_monster_soldier_lasergun(gentity_t *self) {
	if (!M_AllowSpawn(self)) {
		FreeEntity(self);
		return;
	}

	monster_soldier_h(self);

	sound_pain_ss.assign("soldier/solpain3.wav");
	sound_death_ss.assign("soldier/soldeth3.wav");
	gi.soundIndex("soldier/solatck3.wav");

	self->s.skinNum = 10;
	self->count = self->s.skinNum - 6;
	self->health = self->maxHealth = 70 * st.health_multiplier;
	self->gibHealth = -30;
}
