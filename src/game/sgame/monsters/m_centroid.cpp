/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

============================================================================== QUAKE CENTROID -
WORR Port ==============================================================================*/

#include "../g_local.hpp"
#include "m_centroid.hpp"

static cached_soundIndex sound_idle;
static cached_soundIndex sound_pain1;
static cached_soundIndex sound_pain2;
static cached_soundIndex sound_sight;
static cached_soundIndex sound_search;
static cached_soundIndex sound_melee1;
static cached_soundIndex sound_melee2;
static cached_soundIndex sound_walk;
static cached_soundIndex sound_fire1;
static cached_soundIndex sound_fire2;
static cached_soundIndex sound_death;

constexpr SpawnFlags SPAWNFLAG_CENTROID_NOJUMPING = 8_spawnflag;

// -----------------------------------------------------------------------------
// Sounds / ambient behaviors
// -----------------------------------------------------------------------------
MONSTERINFO_IDLE(centroid_idle) (gentity_t* self) -> void {
	if (frandom() <= 0.5f)
		gi.sound(self, CHAN_VOICE, sound_idle, 1, ATTN_NORM, 0);
}

MONSTERINFO_SEARCH(centroid_search) (gentity_t* self) -> void {
	if (frandom() <= 0.5f)
		gi.sound(self, CHAN_VOICE, sound_search, 1, ATTN_NORM, 0);
}

MONSTERINFO_SIGHT(centroid_sight) (gentity_t* self, gentity_t* other) -> void {
	gi.sound(self, CHAN_VOICE, sound_sight, 1, ATTN_NORM, 0);
}

// -----------------------------------------------------------------------------
// Stand / Walk / Run
// -----------------------------------------------------------------------------
static MonsterFrame centroid_frames_stand[] = {
		{ ai_stand }, { ai_stand }, { ai_stand }, { ai_stand }, { ai_stand }, { ai_stand },
		{ ai_stand }, { ai_stand }, { ai_stand }, { ai_stand }, { ai_stand }, { ai_stand }
};
MMOVE_T(centroid_move_stand) = { FRAME_stand1, FRAME_stand12, centroid_frames_stand, nullptr };

MONSTERINFO_STAND(centroid_stand) (gentity_t* self) -> void {
	M_SetAnimation(self, &centroid_move_stand);
}

static MonsterFrame centroid_frames_walk[] = {
		{ ai_walk, 8 }, { ai_walk, 8 }, { ai_walk, 8 }, { ai_walk, 8 }, { ai_walk, 8 }, { ai_walk, 8 }
};
MMOVE_T(centroid_move_walk) = { FRAME_walk1, FRAME_walk6, centroid_frames_walk, nullptr };

MONSTERINFO_WALK(centroid_walk) (gentity_t* self) -> void {
	M_SetAnimation(self, &centroid_move_walk);
}

static MonsterFrame centroid_frames_run[] = {
		{ ai_run, 14 }, { ai_run, 14 }, { ai_run, 14 }, { ai_run, 14 }, { ai_run, 14 }, { ai_run, 14, monster_done_dodge }
};
MMOVE_T(centroid_move_run) = { FRAME_walk1, FRAME_walk6, centroid_frames_run, nullptr };

static MonsterFrame centroid_frames_run2[] = {
		{ ai_run, 28 }, { ai_run, 24 }, { ai_run, 24 }, { ai_run, 24 }, { ai_run, 22 }, { ai_run, 19, monster_done_dodge }
};
MMOVE_T(centroid_move_run2) = { FRAME_walk1, FRAME_walk6, centroid_frames_run2, nullptr };

MONSTERINFO_RUN(centroid_run) (gentity_t* self) -> void {
	monster_done_dodge(self);

	if (self->monsterInfo.aiFlags & AI_STAND_GROUND) {
		M_SetAnimation(self, &centroid_move_stand);
		return;
	}

	M_SetAnimation(self, &centroid_move_run);
}

// -----------------------------------------------------------------------------
// Ranged attack helpers
// -----------------------------------------------------------------------------
static void centroid_attack2(gentity_t* self);
static void centroid_reattack(gentity_t* self);

static void centroid_fire_inner(gentity_t* self) {
	Vector3 start;
	Vector3 forward, right, up;
	Vector3 aim;
	Vector3 offset_right{ 19.0f, 26.0f, -14.0f };
	Vector3 offset_left{ 19.0f, -26.0f, -14.0f };

	AngleVectors(self->s.angles, forward, right, up);

	start = M_ProjectFlashSource(self, offset_right, forward, right);
	PredictAim(self, self->enemy, start, 600, false, frandom() * 0.3f, &aim, nullptr);
	fire_flechette(self, start, aim, 9, 600, 9 / 2);

	start = M_ProjectFlashSource(self, offset_left, forward, right);
	PredictAim(self, self->enemy, start, 600, false, frandom() * 0.3f, &aim, nullptr);
	fire_flechette(self, start, aim, 9, 600, 9 / 2);

	gi.sound(self, CHAN_WEAPON, sound_fire1, 1, ATTN_NORM, 0);
}

static void centroid_fire_outer(gentity_t* self) {
	Vector3 start;
	Vector3 forward, right, up;
	Vector3 aim;
	Vector3 offset_right{ 19.0f, 38.0f, -14.0f };
	Vector3 offset_left{ 19.0f, -38.0f, -14.0f };

	AngleVectors(self->s.angles, forward, right, up);

	start = M_ProjectFlashSource(self, offset_right, forward, right);
	PredictAim(self, self->enemy, start, 600, false, frandom() * 0.3f, &aim, nullptr);
	fire_flechette(self, start, aim, 9, 600, 9 / 2);

	start = M_ProjectFlashSource(self, offset_left, forward, right);
	PredictAim(self, self->enemy, start, 600, false, frandom() * 0.3f, &aim, nullptr);
	fire_flechette(self, start, aim, 9, 600, 9 / 2);

	gi.sound(self, CHAN_WEAPON, sound_fire1, 1, ATTN_NORM, 0);
}

static void centroid_fire_attack(gentity_t* self) {
	const float roll = frandom();
	if (roll <= 0.25f)
		centroid_fire_outer(self);
	else if (roll <= 0.5f)
		centroid_fire_inner(self);
}

static MonsterFrame centroid_frames_attack1[] = {
		{ ai_charge, 14, centroid_fire_inner },
		{ ai_charge, 14, centroid_fire_outer }
};
MMOVE_T(centroid_move_attack1) = { FRAME_shoot1, FRAME_shoot2, centroid_frames_attack1, centroid_attack2 };

static MonsterFrame centroid_frames_attack2[] = {
		{ ai_charge, 14, centroid_fire_inner },
		{ ai_charge, 14, [](gentity_t* self) { centroid_fire_outer(self); centroid_reattack(self); } }
};
MMOVE_T(centroid_move_attack2) = { FRAME_shoot1, FRAME_shoot2, centroid_frames_attack2, nullptr };

static void centroid_attack2(gentity_t* self) {
	M_SetAnimation(self, &centroid_move_attack2);
}

static void centroid_reattack(gentity_t* self) {
	if (self->enemy && (self->enemy->health > 0) && visible(self, self->enemy) && frandom() <= 0.6f) {
		if (frandom() <= 0.3f)
			M_SetAnimation(self, &centroid_move_attack1);
		else
			M_SetAnimation(self, &centroid_move_run);
	}
	else {
		M_SetAnimation(self, &centroid_move_run);
	}
}

MONSTERINFO_ATTACK(centroid_attack) (gentity_t* self) -> void {
	M_SetAnimation(self, &centroid_move_attack1);
}

// -----------------------------------------------------------------------------
// Melee
// -----------------------------------------------------------------------------
static void centroid_swing(gentity_t* self) {
	gi.sound(self, CHAN_WEAPON, sound_melee1, 1, ATTN_NORM, 0);
}

static void centroid_stinger(gentity_t* self) {
	Vector3 aim{ MELEE_DISTANCE, self->mins[0], 10.0f };
	gi.sound(self, CHAN_WEAPON, sound_melee2, 1, ATTN_NORM, 0);
	fire_hit(self, aim, irandom(10, 40), 100);
}

static MonsterFrame centroid_frames_melee[] = {
		{ ai_charge, 3 },
		{ ai_charge, 3 },
		{ ai_charge, 2 },
		{ ai_charge, 2 },
		{ ai_charge, 3, centroid_swing },
		{ ai_charge, 1 },
		{ ai_charge, 0, centroid_stinger },
		{ ai_charge },
		{ ai_charge },
		{ ai_charge },
		{ ai_charge }
};
MMOVE_T(centroid_move_melee) = { FRAME_sting1, FRAME_sting11, centroid_frames_melee, centroid_run };

MONSTERINFO_MELEE(centroid_melee) (gentity_t* self) -> void {
	M_SetAnimation(self, &centroid_move_melee);
}

// -----------------------------------------------------------------------------
// Dodge / movement tricks
// -----------------------------------------------------------------------------
static void centroid_jump_straightup(gentity_t* self) {
	if (self->deadFlag)
		return;

	if (self->groundEntity) {
		self->velocity[_X] += crandom() * 5.0f;
		self->velocity[_Y] += crandom() * 5.0f;
		self->velocity[_Z] += -400.0f * self->gravityVector[2];
	}
}

static void centroid_jump_wait_land(gentity_t* self) {
	if ((frandom() < 0.4f) && (level.time >= self->monsterInfo.attackFinished)) {
		self->monsterInfo.attackFinished = level.time + 300_ms;
		centroid_fire_attack(self);
	}

	if (self->groundEntity == nullptr) {
		self->gravity = 1.3f;
		self->monsterInfo.nextFrame = self->s.frame;

		if (monster_jump_finished(self)) {
			self->gravity = 1.0f;
			self->monsterInfo.nextFrame = self->s.frame + 1;
		}
	}
	else {
		self->gravity = 1.0f;
		self->monsterInfo.nextFrame = self->s.frame + 1;
	}
}

static MonsterFrame centroid_frames_jump[] = {
		{ ai_move, 1, centroid_jump_straightup },
		{ ai_move, 1, centroid_jump_wait_land },
		{ ai_move, 1 },
		{ ai_move, -1 },
		{ ai_move, -1 },
		{ ai_move, -1 }
};
MMOVE_T(centroid_move_jump) = { FRAME_walk1, FRAME_walk6, centroid_frames_jump, centroid_run };

static void centroid_dodge_jump(gentity_t* self) {
	M_SetAnimation(self, &centroid_move_jump);
}

MONSTERINFO_SIDESTEP(centroid_sidestep) (gentity_t* self) -> bool {
	if (self->monsterInfo.active_move == &centroid_move_jump)
		return false;

	if (self->monsterInfo.active_move != &centroid_move_run || self->monsterInfo.active_move != &centroid_move_run2)
		M_SetAnimation(self, &centroid_move_run2);

	return true;
}

MONSTERINFO_DODGE(centroid_dodge) (gentity_t* self, gentity_t* attacker, GameTime eta, trace_t* tr, bool gravity) -> void {
	if (!self->groundEntity || self->health <= 0)
		return;

	if (frandom() <= 0.66f) {
		centroid_sidestep(self);
		return;
	}

	if (!self->enemy) {
		self->enemy = attacker;
		FoundTarget(self);
		return;
	}

	if ((eta < FRAME_TIME_MS) || (eta > 5_sec))
		return;

	if (self->timeStamp > level.time)
		return;

	self->timeStamp = level.time + random_time(1_sec, 5_sec);
	centroid_dodge_jump(self);
}

MONSTERINFO_BLOCKED(centroid_blocked) (gentity_t* self, float dist) -> bool {
	if (blocked_checkplat(self, dist))
		return true;

	return false;
}

// -----------------------------------------------------------------------------
// Pain / skin state
// -----------------------------------------------------------------------------
static MonsterFrame centroid_frames_pain[] = {
		{ ai_move }, { ai_move }, { ai_move }, { ai_move }, { ai_move }
};
MMOVE_T(centroid_move_pain) = { FRAME_pain1, FRAME_pain5, centroid_frames_pain, centroid_run };

PAIN(centroid_pain) (gentity_t* self, gentity_t* other, float kick, int damage, const MeansOfDeath& mod) -> void {
	monster_done_dodge(self);

	if (level.time < self->pain_debounce_time)
		return;

	self->pain_debounce_time = level.time + 3_sec;

	if (!M_ShouldReactToPain(self, mod))
		return;

	const float r = frandom();
	if (r < 0.5f)
		gi.sound(self, CHAN_VOICE, sound_pain1, 1, ATTN_NORM, 0);
	else
		gi.sound(self, CHAN_VOICE, sound_pain2, 1, ATTN_NORM, 0);

	M_SetAnimation(self, &centroid_move_pain);
}

MONSTERINFO_SETSKIN(centroid_setskin) (gentity_t* self) -> void {
	if (self->health < (self->maxHealth / 2))
		self->s.skinNum |= 1;
	else
		self->s.skinNum &= ~1;
}

// -----------------------------------------------------------------------------
// Death
// -----------------------------------------------------------------------------
static void centroid_dead(gentity_t* self) {
	self->mins = { -16.0f, -16.0f, -24.0f };
	self->maxs = { 16.0f, 16.0f, 8.0f };
	monster_dead(self);
}

static void centroid_shrink(gentity_t* self) {
	self->maxs[2] = 12.0f;
	self->svFlags |= SVF_DEADMONSTER;
	gi.linkEntity(self);
}

static MonsterFrame centroid_frames_death[] = {
		{ ai_move },
		{ ai_move },
		{ ai_move, 0, centroid_shrink },
		{ ai_move },
		{ ai_move, 0, monster_footstep }
};
MMOVE_T(centroid_move_death) = { FRAME_death1, FRAME_death5, centroid_frames_death, centroid_dead };

DIE(centroid_die) (gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, const Vector3& point, const MeansOfDeath& mod) -> void {
	if (M_CheckGib(self, mod)) {
		gi.sound(self, CHAN_VOICE, gi.soundIndex("misc/udeath.wav"), 1, ATTN_NORM, 0);

		self->s.skinNum /= 2;

		ThrowGibs(self, damage, {
				{ 2, "models/objects/gibs/bone/tris.md2" },
				{ 3, "models/objects/gibs/sm_meat/tris.md2" },
				{ 2, "models/objects/gibs/sm_metal/tris.md2" },
				{ "models/monsters/centroid/gibs/head.md2", GIB_HEAD | GIB_SKINNED }
			});
		self->deadFlag = true;
		return;
	}

	if (self->deadFlag)
		return;

	self->deadFlag = true;
	self->takeDamage = true;

	M_SetAnimation(self, &centroid_move_death);
	gi.sound(self, CHAN_VOICE, sound_death, 1, ATTN_NORM, 0);
}

// -----------------------------------------------------------------------------
// Spawn
// -----------------------------------------------------------------------------
/*QUAKED monster_centroid (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight NoJumping */
void SP_monster_centroid(gentity_t* self) {
	if (!M_AllowSpawn(self)) {
		FreeEntity(self);
		return;
	}

	self->moveType = MoveType::Step;
	self->solid = SOLID_BBOX;
	self->s.modelIndex = gi.modelIndex("models/monsters/centroid/tris.md2");

	sound_idle.assign("centroid/idle1.wav");
	sound_pain1.assign("centroid/pain.wav");
	sound_pain2.assign("centroid/pain2.wav");
	sound_sight.assign("centroid/sight.wav");
	sound_search.assign("centroid/sight.wav");
	sound_melee1.assign("centroid/tailswing.wav");
	sound_melee2.assign("centroid/tailswing.wav");
	sound_walk.assign("centroid/walk.wav");
	sound_fire1.assign("weapons/nail1.wav");
	sound_fire2.assign("guncmdr/gcdratck2.wav");
	sound_death.assign("centroid/pain2.wav");

	gi.modelIndex("models/monsters/centroid/gibs/head.md2");
	gi.modelIndex("models/monsters/centroid/gibs/chest.md2");
	gi.modelIndex("models/monsters/centroid/gibs/gun.md2");
	gi.modelIndex("models/monsters/centroid/gibs/arm.md2");
	gi.modelIndex("models/monsters/centroid/gibs/foot.md2");

	self->mins = { -16.0f, -16.0f, -24.0f };
	self->maxs = { 16.0f, 16.0f, 32.0f };

	self->health = self->maxHealth = 250 * st.health_multiplier;
	if (!st.was_key_specified("power_type"))
		self->monsterInfo.armorType = IT_ARMOR_COMBAT;
	if (!st.was_key_specified("power_power"))
		self->monsterInfo.armor_power = 100;
	self->gibHealth = -65;
	self->mass = 400;

	self->pain = centroid_pain;
	self->die = centroid_die;

	self->monsterInfo.combatStyle = CombatStyle::Mixed;

	self->monsterInfo.search = centroid_search;
	self->monsterInfo.sight = centroid_sight;
	self->monsterInfo.idle = centroid_idle;

	self->monsterInfo.stand = centroid_stand;
	self->monsterInfo.walk = centroid_walk;
	self->monsterInfo.run = centroid_run;

	self->monsterInfo.dodge = M_MonsterDodge;
	self->monsterInfo.unDuck = monster_duck_up;
	self->monsterInfo.sideStep = centroid_sidestep;
	self->monsterInfo.blocked = centroid_blocked;

	self->monsterInfo.attack = centroid_attack;
	self->monsterInfo.melee = centroid_melee;
	self->monsterInfo.setSkin = centroid_setskin;

	self->monsterInfo.canJump = !self->spawnFlags.has(SPAWNFLAG_CENTROID_NOJUMPING);

	gi.linkEntity(self);

	M_SetAnimation(self, &centroid_move_stand);
	self->monsterInfo.scale = MODEL_SCALE;

	walkmonster_start(self);
}
