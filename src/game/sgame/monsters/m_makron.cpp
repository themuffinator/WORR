/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

============================================================================== Makron -- Final
Boss ==============================================================================*/

#include "../g_local.hpp"
#include "m_makron.hpp"
#include "m_flash.hpp"

void MakronRailgun(gentity_t *self);
void MakronSaveloc(gentity_t *self);
void MakronHyperblaster(gentity_t *self);
void makron_step_left(gentity_t *self);
void makron_step_right(gentity_t *self);
void makronBFG(gentity_t *self);
void makron_dead(gentity_t *self);

static cached_soundIndex sound_pain4;
static cached_soundIndex sound_pain5;
static cached_soundIndex sound_pain6;
static cached_soundIndex sound_death;
static cached_soundIndex sound_step_left;
static cached_soundIndex sound_step_right;
static cached_soundIndex sound_attack_bfg;
static cached_soundIndex sound_brainsplorch;
static cached_soundIndex sound_prerailgun;
static cached_soundIndex sound_popup;
static cached_soundIndex sound_taunt1;
static cached_soundIndex sound_taunt2;
static cached_soundIndex sound_taunt3;
static cached_soundIndex sound_hit;

static void makron_taunt(gentity_t *self) {
	float r;

	r = frandom();
	if (r <= 0.3f)
		gi.sound(self, CHAN_AUTO, sound_taunt1, 1, ATTN_NONE, 0);
	else if (r <= 0.6f)
		gi.sound(self, CHAN_AUTO, sound_taunt2, 1, ATTN_NONE, 0);
	else
		gi.sound(self, CHAN_AUTO, sound_taunt3, 1, ATTN_NONE, 0);
}

//
// stand
//

MonsterFrame makron_frames_stand[] = {
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand }, // 10
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand }, // 20
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand }, // 30
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand }, // 40
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand }, // 50
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand } // 60
};
MMOVE_T(makron_move_stand) = { FRAME_stand201, FRAME_stand260, makron_frames_stand, nullptr };

MONSTERINFO_STAND(makron_stand) (gentity_t *self) -> void {
	M_SetAnimation(self, &makron_move_stand);
}

MonsterFrame makron_frames_run[] = {
	{ ai_run, 3, makron_step_left },
	{ ai_run, 12 },
	{ ai_run, 8 },
	{ ai_run, 8 },
	{ ai_run, 8, makron_step_right },
	{ ai_run, 6 },
	{ ai_run, 12 },
	{ ai_run, 9 },
	{ ai_run, 6 },
	{ ai_run, 12 }
};
MMOVE_T(makron_move_run) = { FRAME_walk204, FRAME_walk213, makron_frames_run, nullptr };

static void makron_hit(gentity_t *self) {
	gi.sound(self, CHAN_AUTO, sound_hit, 1, ATTN_NONE, 0);
}

static void makron_popup(gentity_t *self) {
	gi.sound(self, CHAN_BODY, sound_popup, 1, ATTN_NONE, 0);
}

void makron_step_left(gentity_t *self) {
	gi.sound(self, CHAN_BODY, sound_step_left, 1, ATTN_NORM, 0);
}

void makron_step_right(gentity_t *self) {
	gi.sound(self, CHAN_BODY, sound_step_right, 1, ATTN_NORM, 0);
}

static void makron_brainsplorch(gentity_t *self) {
	gi.sound(self, CHAN_VOICE, sound_brainsplorch, 1, ATTN_NORM, 0);
}

static void makron_prerailgun(gentity_t *self) {
	gi.sound(self, CHAN_WEAPON, sound_prerailgun, 1, ATTN_NORM, 0);
}

MonsterFrame makron_frames_walk[] = {
	{ ai_walk, 3, makron_step_left },
	{ ai_walk, 12 },
	{ ai_walk, 8 },
	{ ai_walk, 8 },
	{ ai_walk, 8, makron_step_right },
	{ ai_walk, 6 },
	{ ai_walk, 12 },
	{ ai_walk, 9 },
	{ ai_walk, 6 },
	{ ai_walk, 12 }
};
MMOVE_T(makron_move_walk) = { FRAME_walk204, FRAME_walk213, makron_frames_run, nullptr };

MONSTERINFO_WALK(makron_walk) (gentity_t *self) -> void {
	M_SetAnimation(self, &makron_move_walk);
}

MONSTERINFO_RUN(makron_run) (gentity_t *self) -> void {
	if (self->monsterInfo.aiFlags & AI_STAND_GROUND)
		M_SetAnimation(self, &makron_move_stand);
	else
		M_SetAnimation(self, &makron_move_run);
}

MonsterFrame makron_frames_pain6[] = {
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move }, // 10
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move, 0, makron_popup },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move }, // 20
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move, 0, makron_taunt },
	{ ai_move },
	{ ai_move },
	{ ai_move }
};
MMOVE_T(makron_move_pain6) = { FRAME_pain601, FRAME_pain627, makron_frames_pain6, makron_run };

MonsterFrame makron_frames_pain5[] = {
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move }
};
MMOVE_T(makron_move_pain5) = { FRAME_pain501, FRAME_pain504, makron_frames_pain5, makron_run };

MonsterFrame makron_frames_pain4[] = {
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move }
};
MMOVE_T(makron_move_pain4) = { FRAME_pain401, FRAME_pain404, makron_frames_pain4, makron_run };

/*
---
Makron Torso. This needs to be spawned in
---
*/

static THINK(makron_torso_think) (gentity_t *self) -> void {
	if (++self->s.frame >= 365)
		self->s.frame = 346;

	self->nextThink = level.time + 10_hz;

	if (self->s.angles[PITCH] > 0)
		self->s.angles[PITCH] = max(0.f, self->s.angles[PITCH] - 15);
}

static void makron_torso(gentity_t *ent) {
	ent->s.frame = 346;
	ent->s.modelIndex = gi.modelIndex("models/monsters/boss3/rider/tris.md2");
	ent->s.skinNum = 1;
	ent->think = makron_torso_think;
	ent->nextThink = level.time + 10_hz;
	ent->s.sound = gi.soundIndex("makron/spine.wav");
	ent->moveType = MoveType::Toss;
	ent->s.effects = EF_GIB;
	Vector3 forward, up;
	AngleVectors(ent->s.angles, forward, nullptr, up);
	ent->velocity += (up * 120);
	ent->velocity += (forward * -120);
	ent->s.origin += (forward * -10);
	ent->s.angles[PITCH] = 90;
	ent->aVelocity = {};
	gi.linkEntity(ent);
}

static void makron_spawn_torso(gentity_t *self) {
	gentity_t *tempent = ThrowGib(self, "models/monsters/boss3/rider/tris.md2", 0, GIB_NONE, self->s.scale);
	tempent->s.origin = self->s.origin;
	tempent->s.angles = self->s.angles;
	self->maxs[2] -= tempent->maxs[2];
	tempent->s.origin[_Z] += self->maxs[2] - 15;
	makron_torso(tempent);
}

MonsterFrame makron_frames_death2[] = {
	{ ai_move, -15 },
	{ ai_move, 3 },
	{ ai_move, -12 },
	{ ai_move, 0, makron_step_left },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move }, // 10
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move, 11 },
	{ ai_move, 12 },
	{ ai_move, 11, makron_step_right },
	{ ai_move },
	{ ai_move }, // 20
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move }, // 30
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move, 5 },
	{ ai_move, 7 },
	{ ai_move, 6, makron_step_left },
	{ ai_move },
	{ ai_move },
	{ ai_move, -1 },
	{ ai_move, 2 }, // 40
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move }, // 50
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move, -6 },
	{ ai_move, -4 },
	{ ai_move, -6, makron_step_right },
	{ ai_move, -4 },
	{ ai_move, -4, makron_step_left },
	{ ai_move },
	{ ai_move }, // 60
	{ ai_move },
	{ ai_move },
	{ ai_move, -2 },
	{ ai_move, -5 },
	{ ai_move, -3, makron_step_right },
	{ ai_move, -8 },
	{ ai_move, -3, makron_step_left },
	{ ai_move, -7 },
	{ ai_move, -4 },
	{ ai_move, -4, makron_step_right }, // 70
	{ ai_move, -6 },
	{ ai_move, -7 },
	{ ai_move, 0, makron_step_left },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move }, // 80
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move, -2 },
	{ ai_move },
	{ ai_move },
	{ ai_move, 2 },
	{ ai_move }, // 90
	{ ai_move, 27, makron_hit },
	{ ai_move, 26 },
	{ ai_move, 0, makron_brainsplorch },
	{ ai_move },
	{ ai_move } // 95
};
MMOVE_T(makron_move_death2) = { FRAME_death201, FRAME_death295, makron_frames_death2, makron_dead };

MonsterFrame makron_frames_sight[] = {
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
MMOVE_T(makron_move_sight) = { FRAME_active01, FRAME_active13, makron_frames_sight, makron_run };

void makronBFG(gentity_t *self) {
	Vector3 forward, right;
	Vector3 start;
	Vector3 dir;
	Vector3 vec;

	AngleVectors(self->s.angles, forward, right, nullptr);
	start = M_ProjectFlashSource(self, monster_flash_offset[MZ2_MAKRON_BFG], forward, right);

	vec = self->enemy->s.origin;
	vec[2] += self->enemy->viewHeight;
	dir = vec - start;
	dir.normalize();
	gi.sound(self, CHAN_VOICE, sound_attack_bfg, 1, ATTN_NORM, 0);
	monster_fire_bfg(self, start, dir, 50, 300, 100, 300, MZ2_MAKRON_BFG);
}

MonsterFrame makron_frames_attack3[] = {
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge, 0, makronBFG },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move }
};
MMOVE_T(makron_move_attack3) = { FRAME_attack301, FRAME_attack308, makron_frames_attack3, makron_run };

MonsterFrame makron_frames_attack4[] = {
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_move, 0, MakronHyperblaster }, // fire
	{ ai_move, 0, MakronHyperblaster }, // fire
	{ ai_move, 0, MakronHyperblaster }, // fire
	{ ai_move, 0, MakronHyperblaster }, // fire
	{ ai_move, 0, MakronHyperblaster }, // fire
	{ ai_move, 0, MakronHyperblaster }, // fire
	{ ai_move, 0, MakronHyperblaster }, // fire
	{ ai_move, 0, MakronHyperblaster }, // fire
	{ ai_move, 0, MakronHyperblaster }, // fire
	{ ai_move, 0, MakronHyperblaster }, // fire
	{ ai_move, 0, MakronHyperblaster }, // fire
	{ ai_move, 0, MakronHyperblaster }, // fire
	{ ai_move, 0, MakronHyperblaster }, // fire
	{ ai_move, 0, MakronHyperblaster }, // fire
	{ ai_move, 0, MakronHyperblaster }, // fire
	{ ai_move, 0, MakronHyperblaster }, // fire
	{ ai_move, 0, MakronHyperblaster }, // fire
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move }
};
MMOVE_T(makron_move_attack4) = { FRAME_attack401, FRAME_attack426, makron_frames_attack4, makron_run };

MonsterFrame makron_frames_attack5[] = {
	{ ai_charge, 0, makron_prerailgun },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge, 0, MakronSaveloc },
	{ ai_move, 0, MakronRailgun }, // Fire railgun
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move }
};
MMOVE_T(makron_move_attack5) = { FRAME_attack501, FRAME_attack516, makron_frames_attack5, makron_run };

void MakronSaveloc(gentity_t *self) {
	self->pos1 = self->enemy->s.origin; // save for aiming the shot
	self->pos1[2] += self->enemy->viewHeight;
};

void MakronRailgun(gentity_t *self) {
	Vector3 start;
	Vector3 dir;
	Vector3 forward, right;

	AngleVectors(self->s.angles, forward, right, nullptr);
	start = M_ProjectFlashSource(self, monster_flash_offset[MZ2_MAKRON_RAILGUN_1], forward, right);

	// calc direction to where we targted
	dir = self->pos1 - start;
	dir.normalize();

	monster_fire_railgun(self, start, dir, 50, 100, MZ2_MAKRON_RAILGUN_1);
}

void MakronHyperblaster(gentity_t *self) {
	Vector3 dir = { 0, 0, 0 };
	Vector3 vec;
	Vector3 start;
	Vector3 forward, right;

	MonsterMuzzleFlashID flash_number = (MonsterMuzzleFlashID)(MZ2_MAKRON_BLASTER_1 + (self->s.frame - FRAME_attack405));

	AngleVectors(self->s.angles, forward, right, nullptr);
	start = M_ProjectFlashSource(self, monster_flash_offset[flash_number], forward, right);

	if (self->enemy) {
		vec = self->enemy->s.origin;
		vec[2] += self->enemy->viewHeight;
		vec -= start;
		vec = VectorToAngles(vec);
		dir[0] = vec[0];
	} else {
		dir[0] = 0;
	}
	if (self->s.frame <= FRAME_attack413)
		dir[1] = self->s.angles[YAW] - 10 * (self->s.frame - FRAME_attack413);
	else
		dir[1] = self->s.angles[YAW] + 10 * (self->s.frame - FRAME_attack421);
	dir[2] = 0;

	AngleVectors(dir, forward, nullptr, nullptr);

	monster_fire_blaster(self, start, forward, 15, 1000, flash_number, EF_BLASTER);
}

static PAIN(makron_pain) (gentity_t *self, gentity_t *other, float kick, int damage, const MeansOfDeath &mod) -> void {
	if (self->monsterInfo.active_move == &makron_move_sight)
		return;

	if (level.time < self->pain_debounce_time)
		return;

	// Lessen the chance of him going into his pain frames
	if (mod.id != ModID::Chainfist && damage <= 25)
		if (frandom() < 0.2f)
			return;

	self->pain_debounce_time = level.time + 3_sec;

	bool do_pain6 = false;

	if (damage <= 40)
		gi.sound(self, CHAN_VOICE, sound_pain4, 1, ATTN_NONE, 0);
	else if (damage <= 110)
		gi.sound(self, CHAN_VOICE, sound_pain5, 1, ATTN_NONE, 0);
	else {
		if (damage <= 150) {
			if (frandom() <= 0.45f) {
				do_pain6 = true;
				gi.sound(self, CHAN_VOICE, sound_pain6, 1, ATTN_NONE, 0);
			}
		} else {
			if (frandom() <= 0.35f) {
				do_pain6 = true;
				gi.sound(self, CHAN_VOICE, sound_pain6, 1, ATTN_NONE, 0);
			}
		}
	}

	if (!M_ShouldReactToPain(self, mod))
		return; // no pain anims in nightmare

	if (damage <= 40)
		M_SetAnimation(self, &makron_move_pain4);
	else if (damage <= 110)
		M_SetAnimation(self, &makron_move_pain5);
	else if (do_pain6)
		M_SetAnimation(self, &makron_move_pain6);
}

MONSTERINFO_SETSKIN(makron_setskin) (gentity_t *self) -> void {
	if (self->health < (self->maxHealth / 2))
		self->s.skinNum = 1;
	else
		self->s.skinNum = 0;
}

MONSTERINFO_SIGHT(makron_sight) (gentity_t *self, gentity_t *other) -> void {
	M_SetAnimation(self, &makron_move_sight);
}

MONSTERINFO_ATTACK(makron_attack) (gentity_t *self) -> void {
	float r;

	r = frandom();

	if (r <= 0.3f)
		M_SetAnimation(self, &makron_move_attack3);
	else if (r <= 0.6f)
		M_SetAnimation(self, &makron_move_attack4);
	else
		M_SetAnimation(self, &makron_move_attack5);
}

//
// death
//

void makron_dead(gentity_t *self) {
	self->mins = { -60, -60, 0 };
	self->maxs = { 60, 60, 24 };
	self->moveType = MoveType::Toss;
	self->svFlags |= SVF_DEADMONSTER;
	gi.linkEntity(self);
	monster_dead(self);
}

static DIE(makron_die) (gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, const Vector3 &point, const MeansOfDeath &mod) -> void {
	self->s.sound = 0;

	// check for gib
	if (M_CheckGib(self, mod)) {
		gi.sound(self, CHAN_VOICE, gi.soundIndex("misc/udeath.wav"), 1, ATTN_NORM, 0);
		ThrowGibs(self, damage, {
			{ "models/objects/gibs/sm_meat/tris.md2" },
			{ 4, "models/objects/gibs/sm_metal/tris.md2", GIB_METALLIC },
			{ "models/objects/gibs/gear/tris.md2", GIB_METALLIC | GIB_HEAD }
			});
		self->deadFlag = true;
		return;
	}

	if (self->deadFlag)
		return;

	// regular death
	gi.sound(self, CHAN_VOICE, sound_death, 1, ATTN_NONE, 0);
	self->deadFlag = true;
	self->takeDamage = true;
	self->svFlags |= SVF_DEADMONSTER;

	M_SetAnimation(self, &makron_move_death2);

	makron_spawn_torso(self);

	self->mins = { -60, -60, 0 };
	self->maxs = { 60, 60, 48 };
}

// [Paril-KEX] use generic function
MONSTERINFO_CHECKATTACK(Makron_CheckAttack) (gentity_t *self) -> bool {
	return M_CheckAttack_Base(self, 0.4f, 0.8f, 0.4f, 0.2f, 0.0f, 0.f);
}

//
// monster_makron
//

void MakronPrecache() {
	sound_pain4.assign("makron/pain3.wav");
	sound_pain5.assign("makron/pain2.wav");
	sound_pain6.assign("makron/pain1.wav");
	sound_death.assign("makron/death.wav");
	sound_step_left.assign("makron/step1.wav");
	sound_step_right.assign("makron/step2.wav");
	sound_attack_bfg.assign("makron/bfg_fire.wav");
	sound_brainsplorch.assign("makron/brain1.wav");
	sound_prerailgun.assign("makron/rail_up.wav");
	sound_popup.assign("makron/popup.wav");
	sound_taunt1.assign("makron/voice4.wav");
	sound_taunt2.assign("makron/voice3.wav");
	sound_taunt3.assign("makron/voice.wav");
	sound_hit.assign("makron/bhit.wav");

	gi.modelIndex("models/monsters/boss3/rider/tris.md2");
}

/*QUAKED monster_makron (1 .5 0) (-30 -30 0) (30 30 90) AMBUSH TRIGGER_SPAWN SIGHT x CORPSE x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
 */
void SP_monster_makron(gentity_t *self) {
	if (!M_AllowSpawn(self)) {
		FreeEntity(self);
		return;
	}

	MakronPrecache();

	self->moveType = MoveType::Step;
	self->solid = SOLID_BBOX;
	self->s.modelIndex = gi.modelIndex("models/monsters/boss3/rider/tris.md2");
	self->mins = { -30, -30, 0 };
	self->maxs = { 30, 30, 90 };

	self->health = 3000 * st.health_multiplier;
	self->gibHealth = -2000;
	self->mass = 500;

	self->pain = makron_pain;
	self->die = makron_die;
	self->monsterInfo.stand = makron_stand;
	self->monsterInfo.walk = makron_walk;
	self->monsterInfo.run = makron_run;
	self->monsterInfo.dodge = nullptr;
	self->monsterInfo.attack = makron_attack;
	self->monsterInfo.melee = nullptr;
	self->monsterInfo.sight = makron_sight;
	self->monsterInfo.checkAttack = Makron_CheckAttack;
	self->monsterInfo.setSkin = makron_setskin;

	gi.linkEntity(self);

	//	M_SetAnimation(self, &makron_move_stand);
	M_SetAnimation(self, &makron_move_sight);
	self->monsterInfo.scale = MODEL_SCALE;

	walkmonster_start(self);

	self->monsterInfo.aiFlags |= AI_IGNORE_SHOTS;
}

/*
=================
MakronSpawn

=================
*/
static THINK(MakronSpawn) (gentity_t *self) -> void {
	Vector3	 vec;
	gentity_t *player;

	SP_monster_makron(self);
	self->think(self);

	// jump at player
	if (self->enemy && self->enemy->inUse && self->enemy->health > 0)
		player = self->enemy;
	else
		player = AI_GetSightClient(self);

	if (!player)
		return;

	vec = player->s.origin - self->s.origin;
	self->s.angles[YAW] = vectoyaw(vec);
	vec.normalize();
	self->velocity = vec * 400;
	self->velocity[_Z] = 200;
	self->groundEntity = nullptr;
	self->enemy = player;
	FoundTarget(self);
	self->monsterInfo.sight(self, self->enemy);
	self->s.frame = self->monsterInfo.nextFrame = FRAME_active01; // FIXME: why????
}

/*
=================
MakronToss

Jorg is just about dead, so set up to launch Makron out
=================
*/
void MakronToss(gentity_t *self) {
	gentity_t *ent = Spawn();
	ent->className = "monster_makron";
	ent->target = self->target;
	ent->s.origin = self->s.origin;
	ent->enemy = self->enemy;

	MakronSpawn(ent);

	// [Paril-KEX] set health bar over to Makron when we throw him out
	for (size_t i = 0; i < 2; i++)
		if (level.campaign.health_bar_entities[i] && level.campaign.health_bar_entities[i]->enemy == self)
			level.campaign.health_bar_entities[i]->enemy = ent;
}

USE(Use_Boss3) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_BOSSTPORT);
	gi.WritePosition(self->s.origin);
	gi.multicast(self->s.origin, MULTICAST_PHS, false);

	// just hide, don't kill ent so we can trigger it again
	self->svFlags |= SVF_NOCLIENT;
	self->solid = SOLID_NOT;
}

static THINK(Think_Boss3Stand) (gentity_t* self) -> void {
	if (self->s.frame == FRAME_stand260)
		self->s.frame = FRAME_stand201;
	else
		self->s.frame++;
	self->nextThink = level.time + 10_hz;
}

/*QUAKED monster_boss3_stand (1 .5 0) (-32 -32 0) (32 32 90) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP

Just stands and cycles in one place until targeted, then teleports away.
*/
void SP_monster_boss3_stand(gentity_t* self) {
	if (!M_AllowSpawn(self)) {
		FreeEntity(self);
		return;
	}

	self->moveType = MoveType::Step;
	self->solid = SOLID_BBOX;
	self->model = "models/monsters/boss3/rider/tris.md2";
	self->s.modelIndex = gi.modelIndex(self->model);
	self->s.frame = FRAME_stand201;

	gi.soundIndex("misc/bigtele.wav");

	self->mins = { -32, -32, 0 };
	self->maxs = { 32, 32, 90 };

	self->use = Use_Boss3;
	self->think = Think_Boss3Stand;
	self->nextThink = level.time + FRAME_TIME_S;
	gi.linkEntity(self);
}
