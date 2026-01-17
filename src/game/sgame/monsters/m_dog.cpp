/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

============================================================================== dog (Quake 1) -
WOR port Faithful behaviors: - Melee bite (attack frame 4) with short reach. - Mid-range leap
that damages on impact. - Two death sets leading to corpse poses. - Two pain sets.
==============================================================================*/

#include "../g_local.hpp"
#include "m_dog.hpp"

constexpr SpawnFlags SPAWNFLAG_DOG_NOJUMPING = 8_spawnflag;

// Sounds
static cached_soundIndex sound_bite;      // dog/dattack1.wav
static cached_soundIndex sound_death;     // dog/ddeath.wav
static cached_soundIndex sound_pain;      // dog/dpain1.wav
static cached_soundIndex sound_sight;     // dog/dsight.wav
static cached_soundIndex sound_idle;      // dog/idle.wav
static cached_soundIndex sound_launch;    // hound/hlaunch.wav
static cached_soundIndex sound_impact;    // hound/himpact.wav
static cached_soundIndex sound_bitemiss;  // hound/hbite2.wav
static cached_soundIndex sound_jump;      // hound/hjump.wav

/*
===============
dog_sight
===============
*/
MONSTERINFO_SIGHT(dog_sight) (gentity_t* self, gentity_t* other) -> void {
	gi.sound(self, CHAN_VOICE, sound_sight, 1, ATTN_NORM, 0);
}

/*
===============
dog_search
===============
*/
MONSTERINFO_SEARCH(dog_search) (gentity_t* self) -> void {
	gi.sound(self, CHAN_VOICE, sound_idle, 1, ATTN_IDLE, 0);
}

/*
===============
dog_idle
===============
*/
MONSTERINFO_IDLE(dog_idle) (gentity_t* self) -> void {
	gi.sound(self, CHAN_VOICE, sound_idle, 1, ATTN_IDLE, 0);
}

/*
===============
dog_stand
===============
*/
static MonsterFrame dog_frames_stand[] = {
	{ ai_stand }, { ai_stand }, { ai_stand },
	{ ai_stand }, { ai_stand }, { ai_stand },
	{ ai_stand }, { ai_stand }, { ai_stand }
};
MMOVE_T(dog_move_stand) = { FRAME_stand01, FRAME_stand09, dog_frames_stand, nullptr };

MONSTERINFO_STAND(dog_stand) (gentity_t* self) -> void {
	M_SetAnimation(self, &dog_move_stand);
}

/*
===============
dog_walk
===============
*/
static MonsterFrame dog_frames_walk[] = {
	{ ai_walk, 8 }, { ai_walk, 8 }, { ai_walk, 8 }, { ai_walk, 8 },
	{ ai_walk, 8 }, { ai_walk, 8 }, { ai_walk, 8 }, { ai_walk, 8 }
};
MMOVE_T(dog_move_walk) = { FRAME_walk01, FRAME_walk08, dog_frames_walk, nullptr };

MONSTERINFO_WALK(dog_walk) (gentity_t* self) -> void {
	M_SetAnimation(self, &dog_move_walk);
}

/*
===============
dog_run
===============
*/
static MonsterFrame dog_frames_run[] = {
	{ ai_run, 16 }, { ai_run, 32 }, { ai_run, 32 }, { ai_run, 20 },
	{ ai_run, 64 }, { ai_run, 32 }, { ai_run, 16 }, { ai_run, 32 },
	{ ai_run, 32 }, { ai_run, 20 }, { ai_run, 64 }, { ai_run, 32 }
};
MMOVE_T(dog_move_run) = { FRAME_run01, FRAME_run12, dog_frames_run, nullptr };

MONSTERINFO_RUN(dog_run) (gentity_t* self) -> void {
	if (self->monsterInfo.aiFlags & AI_STAND_GROUND)
		M_SetAnimation(self, &dog_move_stand);
	else
		M_SetAnimation(self, &dog_move_run);
}

/*
===============
dog_bite
Melee strike on attack frame 4
===============
*/
static void dog_bite(gentity_t* self) {
	if (!self->enemy || self->enemy->health <= 0)
		return;

	// Short-range bite with Q1-like randomization (~0..24 avg ~12)
	const int damage = static_cast<int>((frandom() + frandom() + frandom()) * 8.0f);
	// Aim slightly across body width like mutant does
	Vector3 aim = { MELEE_DISTANCE, self->mins[0], 8.0f };

        if (fire_hit(self, aim, std::max(damage, 1), 100)) {
                gi.sound(self, CHAN_WEAPON, sound_bite, 1, ATTN_NORM, 0);
        } else {
                gi.sound(self, CHAN_WEAPON, sound_bitemiss, 1, ATTN_NORM, 0);
                // If we whiff at point blank, lightly debounce refire
                self->monsterInfo.melee_debounce_time = level.time + 1_sec;
        }
}

/*
===============
dog_melee
===============
*/
static MonsterFrame dog_frames_attack[] = {
	{ ai_charge, 10 },
	{ ai_charge, 10 },
	{ ai_charge, 10 },
	{ ai_charge,  0, dog_bite }, // bite here
	{ ai_charge, 10 },
	{ ai_charge, 10 },
	{ ai_charge, 10 },
	{ ai_charge, 10 }
};
MMOVE_T(dog_move_attack) = { FRAME_attack01, FRAME_attack08, dog_frames_attack, dog_run };

MONSTERINFO_MELEE(dog_melee) (gentity_t* self) -> void {
	M_SetAnimation(self, &dog_move_attack);
}

/*
===============
dog_jump_touch
Deals damage during a leap if moving fast and close.
===============
*/
static TOUCH(dog_jump_touch) (gentity_t* self, gentity_t* other, const trace_t& tr, bool /*otherTouchingSelf*/) -> void {
        if (self->health <= 0) {
                self->touch = nullptr;
                return;
        }

        if (self->style == 1 && other->takeDamage) {
                // Only if we are actually impacting with speed
                if (self->velocity.length() > 400) {
                        const Vector3 dir = self->velocity.normalized();
                        const Vector3 point = self->s.origin + (dir * self->maxs[0]);
                        const int damage = irandom(20, 25);
                        Damage(other, self, self, self->velocity, point, dir, damage, damage, DamageFlags::Normal, ModID::Unknown);
                        gi.sound(self, CHAN_WEAPON, sound_impact, 1, ATTN_NORM, 0);
                        self->style = 0;
                }
        }

        if (!M_CheckBottom(self)) {
                if (self->groundEntity) {
                        self->monsterInfo.nextFrame = FRAME_attack04;
                        self->touch = nullptr;
                }
                return;
        }

        self->touch = nullptr;
}

/*
===============
dog_jump_takeoff
===============
*/
static void dog_jump_takeoff(gentity_t* self) {
        Vector3 forward;
        AngleVectors(self->s.angles, forward, nullptr, nullptr);

        gi.sound(self, CHAN_WEAPON, sound_launch, 1, ATTN_NORM, 0);
        gi.sound(self, CHAN_VOICE, sound_jump, 1, ATTN_NORM, 0);
        self->s.origin[_Z] += 1.0f;
        self->velocity = forward * 400.0f;
        self->velocity[_Z] = 200.0f;
        self->groundEntity = nullptr;

        self->monsterInfo.aiFlags |= AI_DUCKED;
        self->monsterInfo.attackFinished = level.time + 3_sec;

        self->style = 1; // in damaging leap
        self->touch = dog_jump_touch;
}

/*
===============
dog_check_landing
===============
*/
static void dog_check_landing(gentity_t* self) {
        monster_jump_finished(self);

        if (self->groundEntity) {
                gi.sound(self, CHAN_WEAPON, sound_impact, 1, ATTN_NORM, 0);
                self->monsterInfo.attackFinished = level.time + random_time(500_ms, 1.5_sec);

                if (self->monsterInfo.unDuck)
                        self->monsterInfo.unDuck(self);

                // Chain to melee if we are close enough after the pounce
                if (self->enemy && range_to(self, self->enemy) <= RANGE_MELEE * 2.0f)
                        self->monsterInfo.melee(self);

                return;
        }

        // Stay in the landing check frames until we land or timeout
        if (level.time > self->monsterInfo.attackFinished)
                self->monsterInfo.nextFrame = FRAME_leap04;
        else
                self->monsterInfo.nextFrame = FRAME_leap05;
}

/*
===============
dog_jump (missile attack)
===============
*/
static MonsterFrame dog_frames_leap[] = {
        { ai_charge, 20 },                  // leap01: face
        { ai_charge, 20, dog_jump_takeoff }, // leap02: takeoff
        { ai_move,   40 },                  // leap03: flight
        { ai_move,   30 },                  // leap04
        { ai_move,   30, dog_check_landing }, // leap05: poll landing
        { ai_move,    0 },                  // leap06
        { ai_move,    0 },                  // leap07
        { ai_move,    0 },                  // leap08
        { ai_move,    0 }                   // leap09
};
MMOVE_T(dog_move_leap) = { FRAME_leap01, FRAME_leap09, dog_frames_leap, dog_run };

MONSTERINFO_ATTACK(dog_jump) (gentity_t* self) -> void {
	M_SetAnimation(self, &dog_move_leap);
}

/*
===============
dog_check_melee
===============
*/
static bool dog_check_melee(gentity_t* self) {
	return self->enemy && (range_to(self, self->enemy) <= RANGE_MELEE) && (self->monsterInfo.melee_debounce_time <= level.time);
}

/*
===============
dog_check_jump
Roughly mirrors the Q1 logic: usable when target is mid-range and not too far above.
===============
*/
static bool dog_check_jump(gentity_t* self) {
	if (!self->enemy)
		return false;

	// Do not attempt if we cannot jump, or we just did
        if ((self->monsterInfo.attackFinished >= level.time) || self->spawnFlags.has(SPAWNFLAG_DOG_NOJUMPING))
		return false;

	// Height gate: avoid leaping if enemy is far above our standing reach
	if (self->absMin[2] + 96 < self->enemy->absMin[2]) // conservative standing reach
		return false;

	// Horizontal distance gate: approx 80..150 like Q1
	Vector3 flat = self->enemy->s.origin - self->s.origin;
	flat[2] = 0;
	const float d = flat.length();
	if (d < 80.0f || d > 150.0f)
		return false;

	// Chance gate to avoid spamming
	return brandom();
}

/*
===============
dog_checkattack
===============
*/
MONSTERINFO_CHECKATTACK(dog_checkattack) (gentity_t* self) -> bool {
	if (!self->enemy || self->enemy->health <= 0)
		return false;

	if (dog_check_melee(self)) {
		self->monsterInfo.attackState = MonsterAttackState::Melee;
		return true;
	}

	if (dog_check_jump(self)) {
		self->monsterInfo.attackState = MonsterAttackState::Missile;
		return true;
	}

	return false;
}

/*
===============
dog_pain
===============
*/
static MonsterFrame dog_frames_painA[] = {
	{ ai_move }, { ai_move }, { ai_move }, { ai_move }, { ai_move }, { ai_move }
};
MMOVE_T(dog_move_painA) = { FRAME_pain01, FRAME_pain06, dog_frames_painA, dog_run };

static MonsterFrame dog_frames_painB[] = {
	{ ai_move },          // painb01
	{ ai_move },          // painb02
	{ ai_move, 4 },       // painb03
	{ ai_move, 12 },      // painb04
	{ ai_move, 12 },      // painb05
	{ ai_move, 2 },       // painb06
	{ ai_move },          // painb07
	{ ai_move, 4 },       // painb08
	{ ai_move },          // painb09
	{ ai_move, 10 },      // painb10
	{ ai_move },          // painb11
	{ ai_move },          // painb12
	{ ai_move },          // painb13
	{ ai_move },          // painb14
	{ ai_move },          // painb15
	{ ai_move }           // painb16
};
MMOVE_T(dog_move_painB) = { FRAME_painb01, FRAME_painb16, dog_frames_painB, dog_run };

static PAIN(dog_pain) (gentity_t* self, gentity_t* other, float kick, int damage, const MeansOfDeath& mod) -> void {
        if (level.time < self->pain_debounce_time)
                return;

        self->pain_debounce_time = level.time + 1.5_sec;
        gi.sound(self, CHAN_VOICE, sound_pain, 1, ATTN_NORM, 0);

        if (!M_ShouldReactToPain(self, mod))
                return;

        if (frandom() > 0.5f)
                M_SetAnimation(self, &dog_move_painA);
        else
                M_SetAnimation(self, &dog_move_painB);
}

/*
===============
dog_setskin
===============
*/
MONSTERINFO_SETSKIN(dog_setskin) (gentity_t* self) -> void {
        const int maxHealth = std::max(self->maxHealth, self->health);
        const bool useDamagedSkin = self->health < (maxHealth / 2);

        if (useDamagedSkin)
                self->s.skinNum |= 1;
        else
                self->s.skinNum &= ~1;
}

/*
===============
dog_die
===============
*/
static void dog_shrink(gentity_t* self) {
        if (self->svFlags & SVF_DEADMONSTER)
                return;

        self->maxs[2] = 0;
        self->svFlags |= SVF_DEADMONSTER;
        gi.linkEntity(self);
}

static MonsterFrame dog_frames_death1[] = {
        { ai_move }, { ai_move }, { ai_move }, { ai_move, 0, dog_shrink }, { ai_move },
        { ai_move }, { ai_move }, { ai_move }, { ai_move } // ends on corpse pose 1
};
MMOVE_T(dog_move_death1) = { FRAME_death01, FRAME_death09, dog_frames_death1, monster_dead };

static MonsterFrame dog_frames_death2[] = {
        { ai_move }, { ai_move }, { ai_move }, { ai_move }, { ai_move, 0, dog_shrink },
        { ai_move }, { ai_move }, { ai_move }, { ai_move } // ends on corpse pose 2
};
MMOVE_T(dog_move_death2) = { FRAME_deathb01, FRAME_deathb09, dog_frames_death2, monster_dead };

static DIE(dog_die) (gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, const Vector3& point, const MeansOfDeath& mod) -> void {
	if (M_CheckGib(self, mod)) {
		gi.sound(self, CHAN_VOICE, gi.soundIndex("misc/udeath.wav"), 1, ATTN_NORM, 0);

		ThrowGibs(self, damage, {
			{ 3, "models/objects/gibs/bone/tris.md2" },
			{ 3, "models/objects/gibs/sm_meat/tris.md2" },
			{ "models/objects/gibs/head2/tris.md2", GIB_HEAD }
		});

		self->deadFlag = true;
		return;
	}

	if (self->deadFlag)
		return;

	gi.sound(self, CHAN_VOICE, sound_death, 1, ATTN_NORM, 0);
	self->deadFlag = true;
	self->takeDamage = true;

	if (frandom() > 0.5f)
		M_SetAnimation(self, &dog_move_death1);
	else
		M_SetAnimation(self, &dog_move_death2);
}

/*
===============
SP_monster_dog
===============
*/
/*QUAKED monster_dog (1 0 0) (-32 -32 -24) (32 32 40) AMBUSH TRIGGER_SPAWN SIGHT NOJUMPING x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/monsters/dog/tris.md2"
*/
void SP_monster_dog(gentity_t* self) {
	if (!M_AllowSpawn(self)) {
		FreeEntity(self);
		return;
	}

	// Sounds
        sound_bite.assign("dog/dattack1.wav");
        sound_death.assign("dog/ddeath.wav");
        sound_pain.assign("dog/dpain1.wav");
        sound_sight.assign("dog/dsight.wav");
        sound_idle.assign("dog/idle.wav");
        sound_launch.assign("hound/hlaunch.wav");
        sound_impact.assign("hound/himpact.wav");
        sound_bitemiss.assign("hound/hbite2.wav");
        sound_jump.assign("hound/hjump.wav");

	self->moveType = MoveType::Step;
	self->solid = SOLID_BBOX;

	// Model (adjust path if your build uses a different location/name)
	self->s.modelIndex = gi.modelIndex("models/monsters/dog/tris.md2");

	// Bounds per QC spawn comment
	self->mins = { -32, -32, -24 };
	self->maxs = {  32,  32,  40 };

        self->health = static_cast<int>(25 * st.health_multiplier);
        self->maxHealth = self->health;
        self->gibHealth = -35;
        self->mass = 200;

        self->pain = dog_pain;
        self->die  = dog_die;

	self->monsterInfo.stand        = dog_stand;
	self->monsterInfo.walk         = dog_walk;
	self->monsterInfo.run          = dog_run;
	self->monsterInfo.dodge        = nullptr;
	self->monsterInfo.attack       = dog_jump;   // missile = leap
	self->monsterInfo.melee        = dog_melee;
	self->monsterInfo.sight        = dog_sight;
        self->monsterInfo.search       = dog_search;
        self->monsterInfo.idle         = dog_idle;
        self->monsterInfo.checkAttack  = dog_checkattack;
        self->monsterInfo.blocked      = nullptr;    // default blocked is fine for a small quadruped
        self->monsterInfo.setSkin      = dog_setskin;

        if (self->monsterInfo.setSkin)
                self->monsterInfo.setSkin(self);

	gi.linkEntity(self);

	M_SetAnimation(self, &dog_move_stand);

	self->monsterInfo.combatStyle = CombatStyle::Melee;

	self->monsterInfo.scale      = DOG_MODEL_SCALE;
        self->monsterInfo.canJump   = !(self->spawnFlags.has(SPAWNFLAG_DOG_NOJUMPING));
	self->monsterInfo.dropHeight = 256;
	self->monsterInfo.jumpHeight = 56;

	walkmonster_start(self);
}
