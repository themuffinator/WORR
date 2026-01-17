/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

============================================================================== hover
==============================================================================*/

#include "../g_local.hpp"
#include "m_hover.hpp"
#include "m_flash.hpp"

static cached_soundIndex sound_pain1;
static cached_soundIndex sound_pain2;
static cached_soundIndex sound_death1;
static cached_soundIndex sound_death2;
static cached_soundIndex sound_sight;
static cached_soundIndex sound_search1;
static cached_soundIndex sound_search2;

// daedalus sounds
static cached_soundIndex daed_sound_pain1;
static cached_soundIndex daed_sound_pain2;
static cached_soundIndex daed_sound_death1;
static cached_soundIndex daed_sound_death2;
static cached_soundIndex daed_sound_sight;
static cached_soundIndex daed_sound_search1;
static cached_soundIndex daed_sound_search2;

MONSTERINFO_SIGHT(hover_sight) (gentity_t *self, gentity_t *other) -> void {
	// PMM - daedalus sounds
	if (self->mass < 225)
		gi.sound(self, CHAN_VOICE, sound_sight, 1, ATTN_NORM, 0);
	else
		gi.sound(self, CHAN_VOICE, daed_sound_sight, 1, ATTN_NORM, 0);
}

MONSTERINFO_SEARCH(hover_search) (gentity_t *self) -> void {
	// PMM - daedalus sounds
	if (self->mass < 225) {
		if (frandom() < 0.5f)
			gi.sound(self, CHAN_VOICE, sound_search1, 1, ATTN_NORM, 0);
		else
			gi.sound(self, CHAN_VOICE, sound_search2, 1, ATTN_NORM, 0);
	} else {
		if (frandom() < 0.5f)
			gi.sound(self, CHAN_VOICE, daed_sound_search1, 1, ATTN_NORM, 0);
		else
			gi.sound(self, CHAN_VOICE, daed_sound_search2, 1, ATTN_NORM, 0);
	}
}

void hover_run(gentity_t *self);
void hover_dead(gentity_t *self);
void hover_attack(gentity_t *self);
void hover_reattack(gentity_t *self);
void hover_fire_blaster(gentity_t *self);

MonsterFrame hover_frames_stand[] = {
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
	{ ai_stand }
};
MMOVE_T(hover_move_stand) = { FRAME_stand01, FRAME_stand30, hover_frames_stand, nullptr };

MonsterFrame hover_frames_pain3[] = {
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
MMOVE_T(hover_move_pain3) = { FRAME_pain301, FRAME_pain309, hover_frames_pain3, hover_run };

MonsterFrame hover_frames_pain2[] = {
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
MMOVE_T(hover_move_pain2) = { FRAME_pain201, FRAME_pain212, hover_frames_pain2, hover_run };

MonsterFrame hover_frames_pain1[] = {
	{ ai_move },
	{ ai_move },
	{ ai_move, 2 },
	{ ai_move, -8 },
	{ ai_move, -4 },
	{ ai_move, -6 },
	{ ai_move, -4 },
	{ ai_move, -3 },
	{ ai_move, 1 },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move, 3 },
	{ ai_move, 1 },
	{ ai_move },
	{ ai_move, 2 },
	{ ai_move, 3 },
	{ ai_move, 2 },
	{ ai_move, 7 },
	{ ai_move, 1 },
	{ ai_move },
	{ ai_move },
	{ ai_move, 2 },
	{ ai_move },
	{ ai_move },
	{ ai_move, 5 },
	{ ai_move, 3 },
	{ ai_move, 4 }
};
MMOVE_T(hover_move_pain1) = { FRAME_pain101, FRAME_pain128, hover_frames_pain1, hover_run };

MonsterFrame hover_frames_walk[] = {
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
MMOVE_T(hover_move_walk) = { FRAME_forwrd01, FRAME_forwrd35, hover_frames_walk, nullptr };

MonsterFrame hover_frames_run[] = {
	{ ai_run, 10 },
	{ ai_run, 10 },
	{ ai_run, 10 },
	{ ai_run, 10 },
	{ ai_run, 10 },
	{ ai_run, 10 },
	{ ai_run, 10 },
	{ ai_run, 10 },
	{ ai_run, 10 },
	{ ai_run, 10 },
	{ ai_run, 10 },
	{ ai_run, 10 },
	{ ai_run, 10 },
	{ ai_run, 10 },
	{ ai_run, 10 },
	{ ai_run, 10 },
	{ ai_run, 10 },
	{ ai_run, 10 },
	{ ai_run, 10 },
	{ ai_run, 10 },
	{ ai_run, 10 },
	{ ai_run, 10 },
	{ ai_run, 10 },
	{ ai_run, 10 },
	{ ai_run, 10 },
	{ ai_run, 10 },
	{ ai_run, 10 },
	{ ai_run, 10 },
	{ ai_run, 10 },
	{ ai_run, 10 },
	{ ai_run, 10 },
	{ ai_run, 10 },
	{ ai_run, 10 },
	{ ai_run, 10 },
	{ ai_run, 10 }
};
MMOVE_T(hover_move_run) = { FRAME_forwrd01, FRAME_forwrd35, hover_frames_run, nullptr };

static void hover_gib(gentity_t *self) {
	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_EXPLOSION1);
	gi.WritePosition(self->s.origin);
	gi.multicast(self->s.origin, MULTICAST_PHS, false);

	self->s.skinNum /= 2;

	ThrowGibs(self, 150, {
		{ 2, "models/objects/gibs/sm_meat/tris.md2" },
		{ 2, "models/objects/gibs/sm_metal/tris.md2", GIB_METALLIC },
		{ "models/monsters/hover/gibs/chest.md2", GIB_SKINNED },
		{ 2, "models/monsters/hover/gibs/ring.md2", GIB_SKINNED | GIB_METALLIC },
		{ 2, "models/monsters/hover/gibs/foot.md2", GIB_SKINNED },
		{ "models/monsters/hover/gibs/head.md2", GIB_SKINNED | GIB_HEAD },
		});
}

static THINK(hover_deadthink) (gentity_t *self) -> void {
	if (!self->groundEntity && level.time < self->timeStamp) {
		self->nextThink = level.time + FRAME_TIME_S;
		return;
	}

	hover_gib(self);
}

static void hover_dying(gentity_t *self) {
	if (self->groundEntity) {
		hover_deadthink(self);
		return;
	}

	if (brandom())
		return;

	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_PLAIN_EXPLOSION);
	gi.WritePosition(self->s.origin);
	gi.multicast(self->s.origin, MULTICAST_PHS, false);

	if (brandom())
		ThrowGibs(self, 120, {
			{ "models/objects/gibs/sm_meat/tris.md2" }
			});
	else
		ThrowGibs(self, 120, {
			{ "models/objects/gibs/sm_metal/tris.md2", GIB_METALLIC }
			});
}

MonsterFrame hover_frames_death1[] = {
	{ ai_move },
	{ ai_move, 0.f, hover_dying },
	{ ai_move },
	{ ai_move, 0.f, hover_dying },
	{ ai_move },
	{ ai_move, 0.f, hover_dying },
	{ ai_move, -10, hover_dying },
	{ ai_move, 3 },
	{ ai_move, 5, hover_dying },
	{ ai_move, 4, hover_dying },
	{ ai_move, 7 }
};
MMOVE_T(hover_move_death1) = { FRAME_death101, FRAME_death111, hover_frames_death1, hover_dead };

MonsterFrame hover_frames_start_attack[] = {
	{ ai_charge, 1 },
	{ ai_charge, 1 },
	{ ai_charge, 1 }
};
MMOVE_T(hover_move_start_attack) = { FRAME_attack101, FRAME_attack103, hover_frames_start_attack, hover_attack };

MonsterFrame hover_frames_attack1[] = {
	{ ai_charge, -10, hover_fire_blaster },
	{ ai_charge, -10, hover_fire_blaster },
	{ ai_charge, 0, hover_reattack },
};
MMOVE_T(hover_move_attack1) = { FRAME_attack104, FRAME_attack106, hover_frames_attack1, nullptr };

MonsterFrame hover_frames_end_attack[] = {
	{ ai_charge, 1 },
	{ ai_charge, 1 }
};
MMOVE_T(hover_move_end_attack) = { FRAME_attack107, FRAME_attack108, hover_frames_end_attack, hover_run };

// circle strafing code
MonsterFrame hover_frames_attack2[] = {
	{ ai_charge, 10, hover_fire_blaster },
	{ ai_charge, 10, hover_fire_blaster },
	{ ai_charge, 10, hover_reattack },
};
MMOVE_T(hover_move_attack2) = { FRAME_attack104, FRAME_attack106, hover_frames_attack2, nullptr };

void hover_reattack(gentity_t *self) {
	if (self->enemy->health > 0)
		if (visible(self, self->enemy))
			if (frandom() <= 0.6f) {
				if (self->monsterInfo.attackState == MonsterAttackState::Straight) {
					M_SetAnimation(self, &hover_move_attack1);
					return;
				} else if (self->monsterInfo.attackState == MonsterAttackState::Sliding) {
					M_SetAnimation(self, &hover_move_attack2);
					return;
				} else
					gi.Com_PrintFmt("hover_reattack: unexpected state {}\n", (int32_t)self->monsterInfo.attackState);
			}
	M_SetAnimation(self, &hover_move_end_attack);
}

void hover_fire_blaster(gentity_t *self) {
	Vector3	  start;
	Vector3	  forward, right;
	Vector3	  end;
	Vector3	  dir;

	if (!self->enemy || !self->enemy->inUse)
		return;

	AngleVectors(self->s.angles, forward, right, nullptr);
	Vector3 o = monster_flash_offset[(self->s.frame & 1) ? MZ2_HOVER_BLASTER_2 : MZ2_HOVER_BLASTER_1];
	start = M_ProjectFlashSource(self, o, forward, right);

	end = self->enemy->s.origin;
	end[2] += self->enemy->viewHeight;
	dir = end - start;
	dir.normalize();

	// PGM	- daedalus fires blaster2
	if (self->mass < 200)
		monster_fire_blaster(self, start, dir, 1, 1000, (self->s.frame & 1) ? MZ2_HOVER_BLASTER_2 : MZ2_HOVER_BLASTER_1, (self->s.frame % 4) ? EF_NONE : EF_HYPERBLASTER);
	else
		monster_fire_blaster2(self, start, dir, 1, 1000, (self->s.frame & 1) ? MZ2_DAEDALUS_BLASTER_2 : MZ2_DAEDALUS_BLASTER, (self->s.frame % 4) ? EF_NONE : EF_BLASTER);
}

MONSTERINFO_STAND(hover_stand) (gentity_t *self) -> void {
	M_SetAnimation(self, &hover_move_stand);
}

MONSTERINFO_RUN(hover_run) (gentity_t *self) -> void {
	if (self->monsterInfo.aiFlags & AI_STAND_GROUND)
		M_SetAnimation(self, &hover_move_stand);
	else
		M_SetAnimation(self, &hover_move_run);
}

MONSTERINFO_WALK(hover_walk) (gentity_t *self) -> void {
	M_SetAnimation(self, &hover_move_walk);
}

MONSTERINFO_ATTACK(hover_start_attack) (gentity_t *self) -> void {
	M_SetAnimation(self, &hover_move_start_attack);
}

void hover_attack(gentity_t *self) {
	float chance = 0.5f;

	if (self->mass > 150) // the daedalus strafes more
		chance += 0.1f;

	if (frandom() > chance) {
		M_SetAnimation(self, &hover_move_attack1);
		self->monsterInfo.attackState = MonsterAttackState::Straight;
	} else // circle strafe
	{
		if (frandom() <= 0.5f) // switch directions
			self->monsterInfo.lefty = !self->monsterInfo.lefty;
		M_SetAnimation(self, &hover_move_attack2);
		self->monsterInfo.attackState = MonsterAttackState::Sliding;
	}
}

static PAIN(hover_pain) (gentity_t *self, gentity_t *other, float kick, int damage, const MeansOfDeath &mod) -> void {
	if (level.time < self->pain_debounce_time)
		return;

	self->pain_debounce_time = level.time + 3_sec;

	float r = frandom();

	if (r < 0.5f) {
		// PMM - daedalus sounds
		if (self->mass < 225)
			gi.sound(self, CHAN_VOICE, sound_pain1, 1, ATTN_NORM, 0);
		else
			gi.sound(self, CHAN_VOICE, daed_sound_pain1, 1, ATTN_NORM, 0);
	} else {
		// PMM - daedalus sounds
		if (self->mass < 225)
			gi.sound(self, CHAN_VOICE, sound_pain2, 1, ATTN_NORM, 0);
		else
			gi.sound(self, CHAN_VOICE, daed_sound_pain2, 1, ATTN_NORM, 0);
	}

	if (!M_ShouldReactToPain(self, mod))
		return; // no pain anims in nightmare

	r = frandom();

	if (damage <= 25) {
		if (r < 0.5f)
			M_SetAnimation(self, &hover_move_pain3);
		else
			M_SetAnimation(self, &hover_move_pain2);
	} else {
		// PGM pain sequence is WAY too long
		if (r < 0.3f)
			M_SetAnimation(self, &hover_move_pain1);
		else
			M_SetAnimation(self, &hover_move_pain2);
	}
}

MONSTERINFO_SETSKIN(hover_setskin) (gentity_t *self) -> void {
	if (self->health < (self->maxHealth / 2))
		self->s.skinNum |= 1; // PGM support for skins 2 & 3.
	else
		self->s.skinNum &= ~1; // PGM support for skins 2 & 3.
}

void hover_dead(gentity_t *self) {
	self->mins = { -16, -16, -24 };
	self->maxs = { 16, 16, -8 };
	self->moveType = MoveType::Toss;
	self->think = hover_deadthink;
	self->nextThink = level.time + FRAME_TIME_S;
	self->timeStamp = level.time + 15_sec;
	gi.linkEntity(self);
}

static DIE(hover_die) (gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, const Vector3 &point, const MeansOfDeath &mod) -> void {
	self->s.effects = EF_NONE;
	self->monsterInfo.powerArmorType = IT_NULL;

	if (M_CheckGib(self, mod)) {
		hover_gib(self);
		return;
	}

	if (self->deadFlag)
		return;

	// regular death
	// PMM - daedalus sounds
	if (self->mass < 225) {
		if (frandom() < 0.5f)
			gi.sound(self, CHAN_VOICE, sound_death1, 1, ATTN_NORM, 0);
		else
			gi.sound(self, CHAN_VOICE, sound_death2, 1, ATTN_NORM, 0);
	} else {
		if (frandom() < 0.5f)
			gi.sound(self, CHAN_VOICE, daed_sound_death1, 1, ATTN_NORM, 0);
		else
			gi.sound(self, CHAN_VOICE, daed_sound_death2, 1, ATTN_NORM, 0);
	}
	self->deadFlag = true;
	self->takeDamage = true;
	M_SetAnimation(self, &hover_move_death1);
}

static void hover_set_fly_parameters(gentity_t *self) {
	self->monsterInfo.fly_thrusters = false;
	self->monsterInfo.fly_acceleration = 20.f;
	self->monsterInfo.fly_speed = 120.f;
	// Icarus prefers to keep its distance, but flies slower than the flyer.
	// he never pins because of this.
	self->monsterInfo.fly_min_distance = 150.f;
	self->monsterInfo.fly_max_distance = 350.f;
}

/*QUAKED monster_hover (1 .5 0) (-16 -16 -24) (16 16 32) AMBUSH TRIGGER_SPAWN SIGHT x CORPSE x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
 */

/*QUAKED monster_daedalus (1 .5 0) (-16 -16 -24) (16 16 32) AMBUSH TRIGGER_SPAWN SIGHT x CORPSE x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
This is the improved icarus monster.
*/
void SP_monster_hover(gentity_t *self) {
	if (!M_AllowSpawn(self)) {
		FreeEntity(self);
		return;
	}

	self->moveType = MoveType::Step;
	self->solid = SOLID_BBOX;
	self->s.modelIndex = gi.modelIndex("models/monsters/hover/tris.md2");

	gi.modelIndex("models/monsters/hover/gibs/chest.md2");
	gi.modelIndex("models/monsters/hover/gibs/foot.md2");
	gi.modelIndex("models/monsters/hover/gibs/head.md2");
	gi.modelIndex("models/monsters/hover/gibs/ring.md2");

	self->mins = { -24, -24, -24 };
	self->maxs = { 24, 24, 32 };

	self->health = 240 * st.health_multiplier;
	self->gibHealth = -100;
	self->mass = 150;

	self->pain = hover_pain;
	self->die = hover_die;

	self->monsterInfo.stand = hover_stand;
	self->monsterInfo.walk = hover_walk;
	self->monsterInfo.run = hover_run;
	self->monsterInfo.attack = hover_start_attack;
	self->monsterInfo.sight = hover_sight;
	self->monsterInfo.search = hover_search;
	self->monsterInfo.setSkin = hover_setskin;

	if (strcmp(self->className, "monster_daedalus") == 0) {
		self->health = 450 * st.health_multiplier;
		self->mass = 225;
		self->yawSpeed = 23;
		if (!st.was_key_specified("powerArmorType"))
			self->monsterInfo.powerArmorType = IT_POWER_SCREEN;
		if (!st.was_key_specified("powerArmorPower"))
			self->monsterInfo.powerArmorPower = 100;

		self->monsterInfo.engineSound = gi.soundIndex("daedalus/daedidle1.wav");
		daed_sound_pain1.assign("daedalus/daedpain1.wav");
		daed_sound_pain2.assign("daedalus/daedpain2.wav");
		daed_sound_death1.assign("daedalus/daeddeth1.wav");
		daed_sound_death2.assign("daedalus/daeddeth2.wav");
		daed_sound_sight.assign("daedalus/daedsght1.wav");
		daed_sound_search1.assign("daedalus/daedsrch1.wav");
		daed_sound_search2.assign("daedalus/daedsrch2.wav");
		gi.soundIndex("tank/tnkatck3.wav");
	} else {
		self->yawSpeed = 18;
		sound_pain1.assign("hover/hovpain1.wav");
		sound_pain2.assign("hover/hovpain2.wav");
		sound_death1.assign("hover/hovdeth1.wav");
		sound_death2.assign("hover/hovdeth2.wav");
		sound_sight.assign("hover/hovsght1.wav");
		sound_search1.assign("hover/hovsrch1.wav");
		sound_search2.assign("hover/hovsrch2.wav");
		gi.soundIndex("hover/hovatck1.wav");

		self->monsterInfo.engineSound = gi.soundIndex("hover/hovidle1.wav");
	}

	gi.linkEntity(self);

	M_SetAnimation(self, &hover_move_stand);
	self->monsterInfo.scale = MODEL_SCALE;

	flymonster_start(self);

	if (strcmp(self->className, "monster_daedalus") == 0)
		self->s.skinNum = 2;

	self->monsterInfo.aiFlags |= AI_ALTERNATE_FLY;
	hover_set_fly_parameters(self);
}
