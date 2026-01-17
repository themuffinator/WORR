/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

============================================================================== ENFORCER (Quake
1) - WOR Port (rewritten) Core behavior: - Mid-tier humanoid with a fast "laser" (implemented
via blaster plumbing) - Two-shot volley with short cadence; moderate mobility - Simple pain
reactions and standard gib/regular death handling
==============================================================================*/

#include "../g_local.hpp"
#include "m_enforcer.hpp"
#include "m_flash.hpp"

// -----------------------------------------------------------------------------
// Tunables and constants
// -----------------------------------------------------------------------------
static constexpr Vector3 ENFORCER_MINS = { -16.0f, -16.0f, -24.0f };
static constexpr Vector3 ENFORCER_MAXS = { 16.0f,  16.0f,  32.0f };
static constexpr Vector3 ENFORCER_CORPSE_MAXS = { 16.0f, 16.0f, -8.0f };
static constexpr int     ENFORCER_HEALTH = 80;
static constexpr int     ENFORCER_GIBHEALTH = -40;
static constexpr int     ENFORCER_MASS = 200;

// Classic Q1-ish muzzle offset: forward 30, right 8.5, up 16
// Laser parameters (using blaster plumbing for visuals/hit behavior)
static constexpr int   ENFORCER_LASER_DAMAGE = 15;
static constexpr float ENFORCER_LASER_SPEED = 600.0f;
static constexpr GameTime ENFORCER_ROF_GATE = 1_sec; // volley cooldown

// -----------------------------------------------------------------------------
// Sounds
// -----------------------------------------------------------------------------
static cached_soundIndex s_idle;
static cached_soundIndex s_sight1;
static cached_soundIndex s_sight2;
static cached_soundIndex s_sight3;
static cached_soundIndex s_sight4;
static cached_soundIndex s_pain1;
static cached_soundIndex s_pain2;
static cached_soundIndex s_death;
static cached_soundIndex s_gib;
static cached_soundIndex s_fire;

// -----------------------------------------------------------------------------
// Small helpers
// -----------------------------------------------------------------------------

/*
===============
enforcer_idle
===============
*/
static void enforcer_idle(gentity_t* self) {
        if (frandom() < 0.15f) {
                gi.sound(self, CHAN_VOICE, s_idle, 1, ATTN_IDLE, 0);
        }
}

/*
===============
enforcer_footstep
===============
*/
static void enforcer_footstep(gentity_t* self) {
	monster_footstep(self);
}

/*
===============
enforcer_sight
===============
*/
MONSTERINFO_SIGHT(enforcer_sight) (gentity_t* self, gentity_t* other) -> void {
        if (frandom() < 0.15f) {
                gi.sound(self, CHAN_VOICE, s_sight1, 1, ATTN_NORM, 0);
        } else if (frandom() < 0.5f) {
                gi.sound(self, CHAN_VOICE, s_sight2, 1, ATTN_NORM, 0);
        } else if (frandom() < 0.7f) {
                gi.sound(self, CHAN_VOICE, s_sight3, 1, ATTN_NORM, 0);
        }
        else {
                gi.sound(self, CHAN_VOICE, s_sight4, 1, ATTN_NORM, 0);
        }
}

MONSTERINFO_SEARCH(enforcer_search) (gentity_t* self) -> void {
        gi.sound(self, CHAN_VOICE, s_idle, 1, ATTN_IDLE, 0);
}

/*
===============
enforcer_setskin
===============
*/
MONSTERINFO_SETSKIN(enforcer_setskin) (gentity_t* self) -> void {
        if (self->health < (self->maxHealth / 2))
                self->s.skinNum |= 1;
        else
                self->s.skinNum &= ~1;
}

// -----------------------------------------------------------------------------
// Attack
// -----------------------------------------------------------------------------

/*
===============
enforcer_fire

Ionized-style volley shot:

- Projects the muzzle using the soldier blaster flash offset
- Leads targets via PredictAim (or blind-fire anchor when applicable)
- Fires a blaster bolt that mirrors the Ionized behaviour
===============
*/
static void enforcer_fire(gentity_t* self) {
        if (!self->enemy || !self->enemy->inUse) {
                return;
        }

        Vector3 forward;
        Vector3 right;
        Vector3 start;
        Vector3 aim;
        constexpr MonsterMuzzleFlashID flash_index = MZ2_SOLDIER_BLASTER_1;

        AngleVectors(self->s.angles, forward, right, nullptr);
        start = M_ProjectFlashSource(self, monster_flash_offset[flash_index], forward, right);

        if (self->monsterInfo.attackState == MonsterAttackState::Blind) {
                aim = self->monsterInfo.blind_fire_target - start;
                if (aim.lengthSquared() > 0.0f)
                        aim.normalize();
                else
                        aim = forward;
        }
        else {
                PredictAim(self, self->enemy, start, ENFORCER_LASER_SPEED, true, -0.2f, &aim, nullptr);
        }

        if (aim.lengthSquared() == 0.0f)
                aim = forward;

        gi.sound(self, CHAN_WEAPON | CHAN_RELIABLE, s_fire, 1, ATTN_NORM, 0);
        monster_fire_blaster(self, start, aim, ENFORCER_LASER_DAMAGE, ENFORCER_LASER_SPEED, flash_index, EF_BLASTER);
}

static void enforcer_secondfire(gentity_t* self) {
        if ((self->monsterInfo.aiFlags & AI_ENFORCER_SECOND_VOLLEY) == 0) {
                self->monsterInfo.nextFrame = FRAME_attack06;
                self->monsterInfo.aiFlags |= AI_ENFORCER_SECOND_VOLLEY;
        }
        else {
                self->monsterInfo.aiFlags &= ~AI_ENFORCER_SECOND_VOLLEY;
        }
}

/*
===============
enforcer_attack_end
===============
*/
static void enforcer_attack_end(gentity_t* self) {
        self->monsterInfo.attackFinished = level.time + ENFORCER_ROF_GATE;
        self->monsterInfo.aiFlags &= ~AI_ENFORCER_SECOND_VOLLEY;
        // return to run/stand next think
}

/*
===============
enforcer_attack
===============
*/
void enforcer_attack(gentity_t* self);

// Two-shot volley with short holds between fires
static MonsterFrame enforcer_frames_attack[] = {
        { ai_charge, 0 },
        { ai_charge, 1 },
        { ai_charge, 1 },
        { ai_charge, 0 },
        { ai_charge, 0 },
        { ai_charge, 0, enforcer_fire },
        { ai_charge, 0 },
        { ai_charge, 0 },
        { ai_charge, 0, enforcer_secondfire },
        { ai_charge, 0 }
};
MMOVE_T(enforcer_move_attack) = { FRAME_attack01, FRAME_attack10, enforcer_frames_attack, enforcer_attack_end };

/*
===============
enforcer_attack
===============
*/
MONSTERINFO_ATTACK(enforcer_attack) (gentity_t* self) -> void {
        if (level.time < self->monsterInfo.attackFinished)
                return;

        self->monsterInfo.aiFlags &= ~AI_ENFORCER_SECOND_VOLLEY;
        M_SetAnimation(self, &enforcer_move_attack);
}

// -----------------------------------------------------------------------------
// Stand / Fidget
// -----------------------------------------------------------------------------

/*
===============
enforcer_fidget
===============
*/
static void enforcer_fidget(gentity_t* self) {
	if (self->monsterInfo.aiFlags & AI_STAND_GROUND)
		return;
	if (self->enemy)
		return;
	if (frandom() <= 0.05f)
		enforcer_idle(self);
}

/*
===============
enforcer_stand
===============
*/
void enforcer_stand(gentity_t* self);

static MonsterFrame enforcer_frames_stand[] = {
	{ ai_stand, 0, enforcer_idle },
	{ ai_stand }, { ai_stand }, { ai_stand },
	{ ai_stand }, { ai_stand }, { ai_stand, 0, enforcer_fidget }
};
MMOVE_T(enforcer_move_stand) = { FRAME_stand01, FRAME_stand07, enforcer_frames_stand, nullptr };

/*
===============
enforcer_stand
===============
*/
MONSTERINFO_STAND(enforcer_stand) (gentity_t* self) -> void {
	M_SetAnimation(self, &enforcer_move_stand);
}

// -----------------------------------------------------------------------------
// Walk
// -----------------------------------------------------------------------------

/*
===============
enforcer_walk
===============
*/
void enforcer_walk(gentity_t* self);

static MonsterFrame enforcer_frames_walk[] = {
	{ ai_walk, 2 }, { ai_walk, 4 }, { ai_walk, 4 }, { ai_walk, 3 },
	{ ai_walk, 1 }, { ai_walk, 2 }, { ai_walk, 2 }, { ai_walk, 1 },
	{ ai_walk, 2 }, { ai_walk, 4 }, { ai_walk, 4 }, { ai_walk, 1 },
	{ ai_walk, 2 }, { ai_walk, 3 }, { ai_walk, 4 }, { ai_walk, 4 }
};
MMOVE_T(enforcer_move_walk) = { FRAME_walk01, FRAME_walk16, enforcer_frames_walk, nullptr };

/*
===============
enforcer_walk
===============
*/
MONSTERINFO_WALK(enforcer_walk) (gentity_t* self) -> void {
	M_SetAnimation(self, &enforcer_move_walk);
}

// -----------------------------------------------------------------------------
// Run
// -----------------------------------------------------------------------------

/*
===============
enforcer_run
===============
*/
void enforcer_run(gentity_t* self);

static MonsterFrame enforcer_frames_run[] = {
	{ ai_run, 14, enforcer_footstep }, { ai_run, 12 },
	{ ai_run, 16 }, { ai_run, 10, enforcer_footstep },
	{ ai_run, 14 }, { ai_run, 14 },
	{ ai_run, 7 },  { ai_run, 11 }
};
MMOVE_T(enforcer_move_run) = { FRAME_run01, FRAME_run08, enforcer_frames_run, nullptr };

/*
===============
enforcer_run
===============
*/
MONSTERINFO_RUN(enforcer_run) (gentity_t* self) -> void {
	if (self->monsterInfo.aiFlags & AI_STAND_GROUND) {
		M_SetAnimation(self, &enforcer_move_stand);
		return;
	}
	M_SetAnimation(self, &enforcer_move_run);
}

// -----------------------------------------------------------------------------
// Pain
// -----------------------------------------------------------------------------

static MonsterFrame enforcer_pain_light_frames[] = {
        { ai_move }, { ai_move }, { ai_move }, { ai_move }
};
MMOVE_T(enforcer_move_pain_light) = { FRAME_paina01, FRAME_paina04, enforcer_pain_light_frames, enforcer_run };

static MonsterFrame enforcer_pain_medium_frames[] = {
        { ai_move }, { ai_move }, { ai_move }, { ai_move }, { ai_move }
};
MMOVE_T(enforcer_move_pain_medium) = { FRAME_painb01, FRAME_painb05, enforcer_pain_medium_frames, enforcer_run };

static MonsterFrame enforcer_pain_heavy_frames[] = {
        { ai_move }, { ai_move }, { ai_move }, { ai_move },
        { ai_move }, { ai_move }, { ai_move }, { ai_move }
};
MMOVE_T(enforcer_move_pain_heavy) = { FRAME_painc01, FRAME_painc08, enforcer_pain_heavy_frames, enforcer_run };

/*
===============
enforcer_pain
===============
*/
static PAIN(enforcer_pain) (gentity_t* self, gentity_t* other, float kick, int damage, const MeansOfDeath& mod) -> void {
        enforcer_setskin(self);

        if (level.time < self->pain_debounce_time)
                return;

        if (!M_ShouldReactToPain(self, mod))
                return;

        self->pain_debounce_time = level.time + 3_sec;

        if (frandom() < 0.5f)
                gi.sound(self, CHAN_VOICE, s_pain1, 1, ATTN_NORM, 0);
        else
                gi.sound(self, CHAN_VOICE, s_pain2, 1, ATTN_NORM, 0);

        if (damage <= 10)
                M_SetAnimation(self, &enforcer_move_pain_light);
        else if (damage <= 25)
                M_SetAnimation(self, &enforcer_move_pain_medium);
        else
                M_SetAnimation(self, &enforcer_move_pain_heavy);
}

// -----------------------------------------------------------------------------
// Death
// -----------------------------------------------------------------------------

/*
===============
enforcer_dead
===============
*/
static void enforcer_death_shrink(gentity_t* self) {
        self->svFlags |= SVF_DEADMONSTER;
        self->mins = ENFORCER_MINS;
        self->maxs = ENFORCER_CORPSE_MAXS;
        gi.linkEntity(self);
}

static void enforcer_dead(gentity_t* self) {
        self->mins = ENFORCER_MINS;
        self->maxs = ENFORCER_CORPSE_MAXS;
        monster_dead(self);
}

static MonsterFrame enforcer_frames_death[] = {
        { ai_move }, { ai_move }, { ai_move, 0, enforcer_death_shrink }, { ai_move },
        { ai_move }, { ai_move }, { ai_move }, { ai_move }
};
MMOVE_T(enforcer_move_death) = { FRAME_death01, FRAME_death08, enforcer_frames_death, enforcer_dead };

/*
===============
enforcer_die
===============
*/
static DIE(enforcer_die) (gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, const Vector3& point, const MeansOfDeath& mod) -> void {
	// check for gib
        if (M_CheckGib(self, mod)) {
                gi.sound(self, CHAN_VOICE, s_gib, 1, ATTN_NORM, 0);

                // Optional: halve skin like gunner to reflect gore variant if model supports it
                self->s.skinNum /= 2;

		ThrowGibs(self, damage, {
			{ 2, "models/objects/gibs/bone/tris.md2" },
			{ 3, "models/objects/gibs/sm_meat/tris.md2" },
			{ "models/monsters/enforcer/gibs/head.md2", GIB_HEAD }
			});

		self->deadFlag = true;
		return;
	}

	if (self->deadFlag)
		return;

	// regular death
        gi.sound(self, CHAN_VOICE, s_death, 1, ATTN_NORM, 0);
        self->deadFlag = true;
        self->takeDamage = true;

        M_SetAnimation(self, &enforcer_move_death);
}

// -----------------------------------------------------------------------------
// Spawn / Precache
// -----------------------------------------------------------------------------

/*
===============
enforcer_precache
===============
*/
static void enforcer_precache() {
	// Models
	gi.modelIndex("models/monsters/enforcer/tris.md2");
	gi.modelIndex("models/monsters/enforcer/gibs/head.md2"); // for head gib

	// Sounds
        s_idle.assign("enforcer/idle1.wav");
        s_sight1.assign("enforcer/sight1.wav");
        s_sight2.assign("enforcer/sight2.wav");
        s_sight3.assign("enforcer/sight3.wav");
        s_sight4.assign("enforcer/sight4.wav");
        s_pain1.assign("enforcer/pain1.wav");
        s_pain2.assign("enforcer/pain2.wav");
        s_death.assign("enforcer/death1.wav");
        s_gib.assign("q1player/udeath.wav");
        s_fire.assign("enforcer/enfire.wav");
}

/*
===============
enforcer_start
===============
*/
static void enforcer_start(gentity_t* self) {
	self->monsterInfo.stand = enforcer_stand;
	self->monsterInfo.walk = enforcer_walk;
	self->monsterInfo.run = enforcer_run;
        self->monsterInfo.attack = enforcer_attack;
        self->monsterInfo.sight = enforcer_sight;
        self->monsterInfo.search = enforcer_search;
        self->monsterInfo.idle = enforcer_idle;
        self->monsterInfo.setSkin = enforcer_setskin;

        self->pain = enforcer_pain;
        self->die = enforcer_die;

	self->mins = ENFORCER_MINS;
	self->maxs = ENFORCER_MAXS;
	self->s.scale = MODEL_SCALE;
	self->yawSpeed = 15;

	self->health = self->maxHealth = ENFORCER_HEALTH * st.health_multiplier;
	self->gibHealth = ENFORCER_GIBHEALTH;
	self->mass = ENFORCER_MASS;

        gi.linkEntity(self);

        M_SetAnimation(self, &enforcer_move_stand);
        self->monsterInfo.scale = MODEL_SCALE;
        enforcer_setskin(self);

        walkmonster_start(self);
}

/*QUAKED monster_enforcer (1 .5 0) (-16 -16 -24) (16 16 32) AMBUSH TRIGGER_SPAWN SIGHT NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Quake 1 Enforcer armed with a fast "laser" (implemented via blaster projectile).
*/
void SP_monster_enforcer(gentity_t* self) {
	if (!M_AllowSpawn(self)) {
		FreeEntity(self);
		return;
	}

	enforcer_precache();

	self->className = "monster_enforcer";
	self->s.modelIndex = gi.modelIndex("models/monsters/enforcer/tris.md2");

	enforcer_start(self);
}
