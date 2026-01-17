/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

============================================================================== BERSERK
==============================================================================*/

#include "../g_local.hpp"
#include "m_berserk.hpp"

constexpr SpawnFlags SPAWNFLAG_BERSERK_NOJUMPING = 8_spawnflag;

static cached_soundIndex sound_pain;
static cached_soundIndex sound_die;
static cached_soundIndex sound_idle;
static cached_soundIndex sound_idle2;
static cached_soundIndex sound_punch;
static cached_soundIndex sound_sight;
static cached_soundIndex sound_search;
static cached_soundIndex sound_thud;
static cached_soundIndex sound_explod;
static cached_soundIndex sound_jump;

MONSTERINFO_SIGHT(berserk_sight) (gentity_t* self, gentity_t* other) -> void {
	gi.sound(self, CHAN_VOICE, sound_sight, 1, ATTN_NORM, 0);
}

MONSTERINFO_SEARCH(berserk_search) (gentity_t* self) -> void {
	if (brandom())
		gi.sound(self, CHAN_VOICE, sound_idle2, 1, ATTN_NORM, 0);
	else
		gi.sound(self, CHAN_VOICE, sound_search, 1, ATTN_NORM, 0);
}

void	 berserk_fidget(gentity_t* self);

MonsterFrame berserk_frames_stand[] = {
	{ ai_stand, 0, berserk_fidget },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand }
};
MMOVE_T(berserk_move_stand) = { FRAME_stand1, FRAME_stand5, berserk_frames_stand, nullptr };

MONSTERINFO_STAND(berserk_stand) (gentity_t* self) -> void {
	M_SetAnimation(self, &berserk_move_stand);
}

MonsterFrame berserk_frames_stand_fidget[] = {
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
MMOVE_T(berserk_move_stand_fidget) = { FRAME_standb1, FRAME_standb20, berserk_frames_stand_fidget, berserk_stand };

void berserk_fidget(gentity_t* self) {
	if (self->monsterInfo.aiFlags & AI_STAND_GROUND)
		return;
	else if (self->enemy)
		return;
	if (frandom() > 0.15f)
		return;

	M_SetAnimation(self, &berserk_move_stand_fidget);
	gi.sound(self, CHAN_WEAPON, sound_idle, 1, ATTN_IDLE, 0);
}

MonsterFrame berserk_frames_walk[] = {
	{ ai_walk, 9.1f },
	{ ai_walk, 6.3f },
	{ ai_walk, 4.9f },
	{ ai_walk, 6.7f, monster_footstep },
	{ ai_walk, 6.0f },
	{ ai_walk, 8.2f },
	{ ai_walk, 7.2f },
	{ ai_walk, 6.1f },
	{ ai_walk, 4.9f },
	{ ai_walk, 4.7f, monster_footstep },
	{ ai_walk, 4.7f }
};
MMOVE_T(berserk_move_walk) = { FRAME_walkc1, FRAME_walkc11, berserk_frames_walk, nullptr };

MONSTERINFO_WALK(berserk_walk) (gentity_t* self) -> void {
	M_SetAnimation(self, &berserk_move_walk);
}

MonsterFrame berserk_frames_run1[] = {
	{ ai_run, 21 },
	{ ai_run, 11, monster_footstep },
	{ ai_run, 21 },
	{ ai_run, 25, monster_done_dodge },
	{ ai_run, 18, monster_footstep },
	{ ai_run, 19 }
};
MMOVE_T(berserk_move_run1) = { FRAME_run1, FRAME_run6, berserk_frames_run1, nullptr };

MONSTERINFO_RUN(berserk_run) (gentity_t* self) -> void {
	monster_done_dodge(self);

	if (self->monsterInfo.aiFlags & AI_STAND_GROUND)
		M_SetAnimation(self, &berserk_move_stand);
	else
		M_SetAnimation(self, &berserk_move_run1);
}

/*
=============
berserk_attack_spike

Executes the spike melee strike and sets a debounce timer on a miss.
=============
*/
static void berserk_attack_spike(gentity_t* self) {
	static const Vector3 aim{ MELEE_DISTANCE, 0, -24 };

	if (!fire_hit(self, aim, irandom(5, 11), 80)) //	Faster attack -- upwards and backwards
		self->monsterInfo.melee_debounce_time = level.time + 1.2_sec;
}

/*
=============
berserk_swing

Plays the swing sound for the berserk melee attack.
=============
*/
static void berserk_swing(gentity_t* self) {
	gi.sound(self, CHAN_WEAPON, sound_punch, 1, ATTN_NORM, 0);
}

MonsterFrame berserk_frames_attack_spike[] = {
	{ ai_charge },
	{ ai_charge },
	{ ai_charge, 0, berserk_swing },
	{ ai_charge, 0, berserk_attack_spike },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge }
};
MMOVE_T(berserk_move_attack_spike) = { FRAME_att_c1, FRAME_att_c8, berserk_frames_attack_spike, berserk_run };

static void berserk_attack_club(gentity_t* self) {
	Vector3 aim = { MELEE_DISTANCE, self->mins[0], -4 };

	if (!fire_hit(self, aim, irandom(15, 21), 400)) // Slower attack
		self->monsterInfo.melee_debounce_time = level.time + 2.5_sec;
}

MonsterFrame berserk_frames_attack_club[] = {
	{ ai_charge },
	{ ai_charge },
	{ ai_charge, 0, monster_footstep },
	{ ai_charge },
	{ ai_charge, 0, berserk_swing },
	{ ai_charge },
	{ ai_charge, 0, berserk_attack_club },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge }
};
MMOVE_T(berserk_move_attack_club) = { FRAME_att_c9, FRAME_att_c20, berserk_frames_attack_club, berserk_run };


/*
============
RadiusDamage
============
*/
void T_SlamRadiusDamage(Vector3 point, gentity_t* inflictor, gentity_t* attacker, float damage, float kick, gentity_t* ignore, float radius, MeansOfDeath mod) {
	float	 points;
	gentity_t* ent = nullptr;
	Vector3	 v;
	Vector3	 dir;

	while ((ent = FindRadius(ent, inflictor->s.origin, radius * 2.f)) != nullptr) {
		if (ent == ignore)
			continue;
		if (!ent->takeDamage)
			continue;
		if (!CanDamage(ent, inflictor))
			continue;
		// don't hit players in mid air
		if (ent->client && !ent->groundEntity)
			continue;

		v = closest_point_to_box(point, ent->s.origin + ent->mins, ent->s.origin + ent->maxs) - point;

		// calculate contribution amount
		float amount = min(1.f, 1.f - (v.length() / radius));

		// too far away
		if (amount <= 0.f)
			continue;

		amount *= amount;

		// damage & kick are exponentially scaled
		points = max(1.f, damage * amount);

		dir = (ent->s.origin - point).normalized();

		// keep the point at their feet so they always get knocked up
		point[2] = ent->absMin[2];
		Damage(ent, inflictor, attacker, dir, point, dir, (int)points, (int)(kick * amount),
			DamageFlags::Radius, mod);

		if (ent->client)
			ent->velocity.z = max(270.f, ent->velocity.z);
	}
}

static void berserk_attack_slam(gentity_t* self) {
	gi.sound(self, CHAN_WEAPON, sound_thud, 1, ATTN_NORM, 0);
	gi.sound(self, CHAN_AUTO, sound_explod, 0.75f, ATTN_NORM, 0);
	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_BERSERK_SLAM);
	Vector3 f, r, start;
	AngleVectors(self->s.angles, f, r, nullptr);
	start = M_ProjectFlashSource(self, { 20.f, -14.3f, -21.f }, f, r);
	trace_t tr = gi.traceLine(self->s.origin, start, self, MASK_SOLID);
	gi.WritePosition(tr.endPos);
	gi.WriteDir({ 0.f, 0.f, 1.f });
	gi.multicast(tr.endPos, MULTICAST_PHS, false);
	self->gravity = 1.0f;
	self->velocity = {};
	self->flags |= FL_KILL_VELOCITY;

	T_SlamRadiusDamage(tr.endPos, self, self, 8, 300.f, self, 165, ModID::Unknown);
}

static TOUCH(berserk_jump_touch) (gentity_t* self, gentity_t* other, const trace_t& tr, bool otherTouchingSelf) -> void {
	if (self->health <= 0) {
		self->touch = nullptr;
		return;
	}

	if (self->groundEntity) {
		self->s.frame = FRAME_slam18;

		if (self->touch)
			berserk_attack_slam(self);

		self->touch = nullptr;
	}
}

static void berserk_high_gravity(gentity_t* self) {
	if (self->velocity[_Z] < 0)
		self->gravity = 2.25f * (800.f / level.gravity);
	else
		self->gravity = 5.25f * (800.f / level.gravity);
}

static void berserk_jump_takeoff(gentity_t* self) {
	Vector3 forward;

	if (!self->enemy)
		return;

	// immediately turn to where we need to go
	float length = (self->s.origin - self->enemy->s.origin).length();
	float fwd_speed = length * 1.95f;
	Vector3 dir;
	PredictAim(self, self->enemy, self->s.origin, fwd_speed, false, 0.f, &dir, nullptr);
	self->s.angles[YAW] = vectoyaw(dir);
	AngleVectors(self->s.angles, forward, nullptr, nullptr);
	self->s.origin[_Z] += 1;
	self->velocity = forward * fwd_speed;
	self->velocity[_Z] = 450;
	self->groundEntity = nullptr;
	self->monsterInfo.aiFlags |= AI_DUCKED;
	self->monsterInfo.attackFinished = level.time + 3_sec;
	self->touch = berserk_jump_touch;
	berserk_high_gravity(self);
}

static void berserk_check_landing(gentity_t* self) {
	berserk_high_gravity(self);

	if (self->groundEntity) {
		self->monsterInfo.attackFinished = 0_ms;
		self->monsterInfo.unDuck(self);
		self->s.frame = FRAME_slam18;
		if (self->touch) {
			berserk_attack_slam(self);
			self->touch = nullptr;
		}
		self->flags &= ~FL_KILL_VELOCITY;
		return;
	}

	if (level.time > self->monsterInfo.attackFinished)
		self->monsterInfo.nextFrame = FRAME_slam3;
	else
		self->monsterInfo.nextFrame = FRAME_slam5;
}

MonsterFrame berserk_frames_attack_strike[] = {
	{ ai_charge },
	{ ai_charge },
	{ ai_move, 0, berserk_jump_takeoff },
	{ ai_move, 0, berserk_high_gravity },
	{ ai_move, 0, berserk_check_landing },
	{ ai_move, 0, monster_footstep },
	{ ai_move },
	{ ai_move, 0, monster_footstep },
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
	{ ai_move, 0, monster_footstep }
};
MMOVE_T(berserk_move_attack_strike) = { FRAME_slam1, FRAME_slam23, berserk_frames_attack_strike, berserk_run };

extern const MonsterMove berserk_move_run_attack1;

MONSTERINFO_MELEE(berserk_melee) (gentity_t* self) -> void {
	if (self->monsterInfo.melee_debounce_time > level.time)
		return;
	// if we're *almost* ready to land down the hammer from run-attack
	// don't switch us
	else if (self->monsterInfo.active_move == &berserk_move_run_attack1 && self->s.frame >= FRAME_r_att13) {
		self->monsterInfo.attackState = MonsterAttackState::Straight;
		self->monsterInfo.attackFinished = 0_ms;
		return;
	}

	monster_done_dodge(self);

	if (brandom())
		M_SetAnimation(self, &berserk_move_attack_spike);
	else
		M_SetAnimation(self, &berserk_move_attack_club);
}

static void berserk_run_attack_speed(gentity_t* self) {
	if (self->enemy && range_to(self, self->enemy) < MELEE_DISTANCE) {
		self->monsterInfo.nextFrame = self->s.frame + 6;
		monster_done_dodge(self);
	}
}

static void berserk_run_swing(gentity_t* self) {
	berserk_swing(self);
	self->monsterInfo.melee_debounce_time = level.time + 0.6_sec;

	if (self->monsterInfo.attackState == MonsterAttackState::Sliding) {
		self->monsterInfo.attackState = MonsterAttackState::Straight;
		monster_done_dodge(self);
	}
}

MonsterFrame berserk_frames_run_attack1[] = {
	{ ai_run, 21, berserk_run_attack_speed },
	{ ai_run, 11, [](gentity_t* self) { berserk_run_attack_speed(self); monster_footstep(self); } },
	{ ai_run, 21, berserk_run_attack_speed },
	{ ai_run, 25, [](gentity_t* self) { berserk_run_attack_speed(self); monster_done_dodge(self); } },
	{ ai_run, 18, [](gentity_t* self) { berserk_run_attack_speed(self); monster_footstep(self); } },
	{ ai_run, 19, berserk_run_attack_speed },
	{ ai_run, 21 },
	{ ai_run, 11, monster_footstep },
	{ ai_run, 21 },
	{ ai_run, 25 },
	{ ai_run, 18, monster_footstep },
	{ ai_run, 19 },
	{ ai_run, 21, berserk_run_swing },
	{ ai_run, 11, monster_footstep },
	{ ai_run, 21 },
	{ ai_run, 25 },
	{ ai_run, 18, monster_footstep },
	{ ai_run, 19, berserk_attack_club }
};
MMOVE_T(berserk_move_run_attack1) = { FRAME_r_att1, FRAME_r_att18, berserk_frames_run_attack1, berserk_run };

MONSTERINFO_ATTACK(berserk_attack) (gentity_t* self) -> void {
	if (self->monsterInfo.melee_debounce_time <= level.time && (range_to(self, self->enemy) < MELEE_DISTANCE))
		berserk_melee(self);
	// only jump if they are far enough away for it to make sense (otherwise
	// it gets annoying to have them keep hopping over and over again)
	else if (!self->spawnFlags.has(SPAWNFLAG_BERSERK_NOJUMPING) && (self->timeStamp < level.time && brandom()) && range_to(self, self->enemy) > 150.f) {
		M_SetAnimation(self, &berserk_move_attack_strike);
		// don't do this for a while, otherwise we just keep doing it
		gi.sound(self, CHAN_WEAPON, sound_jump, 1, ATTN_NORM, 0);
		self->timeStamp = level.time + 5_sec;
	}
	else if (self->monsterInfo.active_move == &berserk_move_run1 && (range_to(self, self->enemy) <= RANGE_NEAR)) {
		M_SetAnimation(self, &berserk_move_run_attack1);
		self->monsterInfo.nextFrame = FRAME_r_att1 + (self->s.frame - FRAME_run1) + 1;
	}
}

MonsterFrame berserk_frames_pain1[] = {
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move }
};
MMOVE_T(berserk_move_pain1) = { FRAME_painc1, FRAME_painc4, berserk_frames_pain1, berserk_run };

MonsterFrame berserk_frames_pain2[] = {
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move, 0, monster_footstep },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move, 0, monster_footstep },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move, 0, monster_footstep }
};
MMOVE_T(berserk_move_pain2) = { FRAME_painb1, FRAME_painb20, berserk_frames_pain2, berserk_run };

extern const MonsterMove berserk_move_jump, berserk_move_jump2;

static PAIN(berserk_pain) (gentity_t* self, gentity_t* other, float kick, int damage, const MeansOfDeath& mod) -> void {
	// if we're jumping, don't pain
	if ((self->monsterInfo.active_move == &berserk_move_jump) ||
		(self->monsterInfo.active_move == &berserk_move_jump2) ||
		(self->monsterInfo.active_move == &berserk_move_attack_strike)) {
		return;
	}

	if (level.time < self->pain_debounce_time)
		return;

	self->pain_debounce_time = level.time + 3_sec;
	gi.sound(self, CHAN_VOICE, sound_pain, 1, ATTN_NORM, 0);

	if (!M_ShouldReactToPain(self, mod))
		return; // no pain anims in nightmare

	monster_done_dodge(self);

	if ((damage <= 50) || (frandom() < 0.5f))
		M_SetAnimation(self, &berserk_move_pain1);
	else
		M_SetAnimation(self, &berserk_move_pain2);
}

MONSTERINFO_SETSKIN(berserk_setskin) (gentity_t* self) -> void {
	if (self->health < (self->maxHealth / 2))
		self->s.skinNum = 1;
	else
		self->s.skinNum = 0;
}

static void berserk_dead(gentity_t* self) {
	self->mins = { -16, -16, -24 };
	self->maxs = { 16, 16, -8 };
	monster_dead(self);
}

static void berserk_shrink(gentity_t* self) {
	self->maxs[2] = 0;
	self->svFlags |= SVF_DEADMONSTER;
	gi.linkEntity(self);
}

MonsterFrame berserk_frames_death1[] = {
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move, 0, monster_footstep },
	{ ai_move },
	{ ai_move, 0, berserk_shrink },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move }
};
MMOVE_T(berserk_move_death1) = { FRAME_death1, FRAME_death13, berserk_frames_death1, berserk_dead };

MonsterFrame berserk_frames_death2[] = {
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move, 0, berserk_shrink },
	{ ai_move, 0, monster_footstep },
	{ ai_move },
	{ ai_move },
	{ ai_move }
};
MMOVE_T(berserk_move_death2) = { FRAME_deathc1, FRAME_deathc8, berserk_frames_death2, berserk_dead };

static DIE(berserk_die) (gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, const Vector3& point, const MeansOfDeath& mod) -> void {
	if (M_CheckGib(self, mod)) {
		gi.sound(self, CHAN_VOICE, gi.soundIndex("misc/udeath.wav"), 1, ATTN_NORM, 0);

		self->s.skinNum = 0;

		ThrowGibs(self, damage, {
			{ 2, "models/objects/gibs/bone/tris.md2" },
			{ 3, "models/objects/gibs/sm_meat/tris.md2" },
			{ 1, "models/objects/gibs/gear/tris.md2" },
			{ "models/monsters/berserk/gibs/chest.md2", GIB_SKINNED },
			{ "models/monsters/berserk/gibs/hammer.md2", GIB_SKINNED | GIB_UPRIGHT },
			{ "models/monsters/berserk/gibs/thigh.md2", GIB_SKINNED },
			{ "models/monsters/berserk/gibs/head.md2", GIB_HEAD | GIB_SKINNED }
			});
		self->deadFlag = true;
		return;
	}

	if (self->deadFlag)
		return;

	gi.sound(self, CHAN_VOICE, sound_die, 1, ATTN_NORM, 0);
	self->deadFlag = true;
	self->takeDamage = true;

	if (damage >= 50)
		M_SetAnimation(self, &berserk_move_death1);
	else
		M_SetAnimation(self, &berserk_move_death2);
}

static void berserk_jump_now(gentity_t* self) {
	Vector3 forward, up;

	AngleVectors(self->s.angles, forward, nullptr, up);
	self->velocity += (forward * 100);
	self->velocity += (up * 300);
}

static void berserk_jump2_now(gentity_t* self) {
	Vector3 forward, up;

	AngleVectors(self->s.angles, forward, nullptr, up);
	self->velocity += (forward * 150);
	self->velocity += (up * 400);
}

static void berserk_jump_wait_land(gentity_t* self) {
	if (self->groundEntity == nullptr) {
		self->monsterInfo.nextFrame = self->s.frame;

		if (monster_jump_finished(self))
			self->monsterInfo.nextFrame = self->s.frame + 1;
	}
	else
		self->monsterInfo.nextFrame = self->s.frame + 1;
}

MonsterFrame berserk_frames_jump[] = {
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move, 0, berserk_jump_now },
	{ ai_move },
	{ ai_move },
	{ ai_move, 0, berserk_jump_wait_land },
	{ ai_move },
	{ ai_move }
};
MMOVE_T(berserk_move_jump) = { FRAME_jump1, FRAME_jump9, berserk_frames_jump, berserk_run };

MonsterFrame berserk_frames_jump2[] = {
	{ ai_move, -8 },
	{ ai_move, -4 },
	{ ai_move, -4 },
	{ ai_move, 0, berserk_jump2_now },
	{ ai_move },
	{ ai_move },
	{ ai_move, 0, berserk_jump_wait_land },
	{ ai_move },
	{ ai_move }
};
MMOVE_T(berserk_move_jump2) = { FRAME_jump1, FRAME_jump9, berserk_frames_jump2, berserk_run };

static void berserk_jump(gentity_t* self, BlockedJumpResult result) {
	if (!self->enemy)
		return;

	if (result == BlockedJumpResult::Jump_Turn_Up)
		M_SetAnimation(self, &berserk_move_jump2);
	else
		M_SetAnimation(self, &berserk_move_jump);
}

MONSTERINFO_BLOCKED(berserk_blocked) (gentity_t* self, float dist) -> bool {
	if (auto result = blocked_checkjump(self, dist); result != BlockedJumpResult::No_Jump) {
		if (result != BlockedJumpResult::Jump_Turn)
			berserk_jump(self, result);

		return true;
	}

	if (blocked_checkplat(self, dist))
		return true;

	return false;
}

MONSTERINFO_SIDESTEP(berserk_sidestep) (gentity_t* self) -> bool {
	// if we're jumping or in long pain, don't dodge
	if ((self->monsterInfo.active_move == &berserk_move_jump) ||
		(self->monsterInfo.active_move == &berserk_move_jump2) ||
		(self->monsterInfo.active_move == &berserk_move_attack_strike) ||
		(self->monsterInfo.active_move == &berserk_move_pain2))
		return false;

	if (self->monsterInfo.active_move != &berserk_move_run1)
		M_SetAnimation(self, &berserk_move_run1);

	return true;
}

MonsterFrame berserk_frames_duck[] = {
	{ ai_move, 0, monster_duck_down },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move, 0, monster_duck_hold },
	{ ai_move },
	{ ai_move },
	{ ai_move, 0, monster_duck_up },
	{ ai_move },
	{ ai_move }
};
MMOVE_T(berserk_move_duck) = { FRAME_duck1, FRAME_duck10, berserk_frames_duck, berserk_run };

MonsterFrame berserk_frames_duck2[] = {
	{ ai_move, 21, monster_duck_down },
	{ ai_move, 28 },
	{ ai_move, 20 },
	{ ai_move, 12, monster_footstep },
	{ ai_move, 7 },
	{ ai_move, 0, monster_footstep },
	{ ai_move },
	{ ai_move, 0, monster_duck_hold },
	{ ai_move, 0 },
	{ ai_move, 0 },
	{ ai_move, 0 },
	{ ai_move, 0 },
	{ ai_move, 0, monster_footstep },
	{ ai_move, 0, monster_duck_up },
	{ ai_move },
	{ ai_move },
	{ ai_move, 0, monster_footstep },
};
MMOVE_T(berserk_move_duck2) = { FRAME_fall2, FRAME_fall18, berserk_frames_duck2, berserk_run };

MONSTERINFO_DUCK(berserk_duck) (gentity_t* self, GameTime eta) -> bool {
	// berserk only dives forward, and very rarely
	if (frandom() >= 0.05f) {
		return false;
	}

	// if we're jumping, don't dodge
	if ((self->monsterInfo.active_move == &berserk_move_jump) ||
		(self->monsterInfo.active_move == &berserk_move_jump2)) {
		return false;
	}

	M_SetAnimation(self, &berserk_move_duck2);

	return true;
}

/*QUAKED monster_berserk (1 .5 0) (-16 -16 -24) (16 16 32) AMBUSH TRIGGER_SPAWN SIGHT x CORPSE x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
 */
void SP_monster_berserk(gentity_t* self) {
	if (!M_AllowSpawn(self)) {
		FreeEntity(self);
		return;
	}

	// pre-caches
	sound_pain.assign("berserk/berpain2.wav");
	sound_die.assign("berserk/berdeth2.wav");
	sound_idle.assign("berserk/beridle1.wav");
	sound_idle2.assign("berserk/idle.wav");
	sound_punch.assign("berserk/attack.wav");
	sound_search.assign("berserk/bersrch1.wav");
	sound_sight.assign("berserk/sight.wav");
	sound_thud.assign("mutant/thud1.wav");
	sound_explod.assign("world/explod2.wav");
	sound_jump.assign("berserk/jump.wav");

	self->s.modelIndex = gi.modelIndex("models/monsters/berserk/tris.md2");

	gi.modelIndex("models/monsters/berserk/gibs/head.md2");
	gi.modelIndex("models/monsters/berserk/gibs/chest.md2");
	gi.modelIndex("models/monsters/berserk/gibs/hammer.md2");
	gi.modelIndex("models/monsters/berserk/gibs/thigh.md2");

	self->mins = { -16, -16, -24 };
	self->maxs = { 16, 16, 32 };
	self->moveType = MoveType::Step;
	self->solid = SOLID_BBOX;

	self->health = 240 * st.health_multiplier;
	self->gibHealth = -60;
	self->mass = 250;

	self->pain = berserk_pain;
	self->die = berserk_die;

	self->monsterInfo.stand = berserk_stand;
	self->monsterInfo.walk = berserk_walk;
	self->monsterInfo.run = berserk_run;
	self->monsterInfo.dodge = M_MonsterDodge;
	self->monsterInfo.duck = berserk_duck;
	self->monsterInfo.unDuck = monster_duck_up;
	self->monsterInfo.sideStep = berserk_sidestep;
	self->monsterInfo.blocked = berserk_blocked;
	self->monsterInfo.attack = berserk_attack;
	self->monsterInfo.melee = berserk_melee;
	self->monsterInfo.sight = berserk_sight;
	self->monsterInfo.search = berserk_search;
	self->monsterInfo.setSkin = berserk_setskin;

	M_SetAnimation(self, &berserk_move_stand);
	self->monsterInfo.scale = MODEL_SCALE;

	self->monsterInfo.combatStyle = CombatStyle::Melee;
	self->monsterInfo.canJump = !self->spawnFlags.has(SPAWNFLAG_BERSERK_NOJUMPING);
	self->monsterInfo.dropHeight = 256;
	self->monsterInfo.jumpHeight = 40;

	gi.linkEntity(self);

	walkmonster_start(self);
}
