/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

============================================================================== OGRE (Quake 1) -
WOR port - Melee: chainsaw swipe (short reach, refires if still in range) - Ranged: arcing
grenade lob (mid/long range, clear-shot check) - Two pain sets, two death sets - Sight, search,
idle sounds - Uses monster muzzle flash handling like m_gunner.cpp
==============================================================================*/

#include "../g_local.hpp"
#include "m_ogre.hpp"
#include "m_flash.hpp"

#include <cstring>

/*
===============
Spawnflags
===============
*/
constexpr SpawnFlags SPAWNFLAG_OGRE_NOGRENADE = 8_spawnflag;

void ogre_attack(gentity_t* self);

/*
===============
Sounds
===============
*/
static cached_soundIndex snd_melee_swing;
static cached_soundIndex snd_melee_hit;
static cached_soundIndex snd_grenade;
static cached_soundIndex snd_pain1;
static cached_soundIndex snd_death;
static cached_soundIndex snd_idle;
static cached_soundIndex snd_idle2;
static cached_soundIndex snd_sight;
static cached_soundIndex snd_search;

/*
===============
ogre_idlesound
===============
*/
static void ogre_idlesound(gentity_t* self) {
	gi.sound(self, CHAN_VOICE, frandom() > 0.6f ? snd_idle : snd_idle2, 1, ATTN_IDLE, 0);
}

/*
===============
ogre_sight
===============
*/
MONSTERINFO_SIGHT(ogre_sight) (gentity_t* self, gentity_t* other) -> void {
	gi.sound(self, CHAN_VOICE, snd_sight, 1, ATTN_NORM, 0);
}

/*
===============
ogre_search
===============
*/
MONSTERINFO_SEARCH(ogre_search) (gentity_t* self) -> void {
	gi.sound(self, CHAN_VOICE, snd_search, 1, ATTN_IDLE, 0);
}

/*
===============
ogre_stand
===============
*/
static MonsterFrame ogre_frames_stand[] = {
	{ ai_stand },{ ai_stand },{ ai_stand },{ ai_stand },{ ai_stand },
	{ ai_stand },{ ai_stand },{ ai_stand },{ ai_stand },{ ai_stand }
};
MMOVE_T(ogre_move_stand) = { FRAME_stand01, FRAME_stand10, ogre_frames_stand, nullptr };

MONSTERINFO_STAND(ogre_stand) (gentity_t* self) -> void {
	M_SetAnimation(self, &ogre_move_stand);
}

/*
===============
ogre_idle
===============
*/
static void ogre_idle_loop(gentity_t* self) {
	if (frandom() < 0.66f)
		self->monsterInfo.nextFrame = FRAME_idle03;
}

static MonsterFrame ogre_frames_idle[] = {
	{ ai_stand, 0, ogre_idlesound },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand, 0, ogre_idle_loop },
	{ ai_stand },
	{ ai_stand }
};
MMOVE_T(ogre_move_idle) = { FRAME_idle01, FRAME_idle08, ogre_frames_idle, ogre_stand };

MONSTERINFO_IDLE(ogre_idle) (gentity_t* self) -> void {
	M_SetAnimation(self, &ogre_move_idle);
}

/*
===============
ogre_walk
===============
*/
static MonsterFrame ogre_frames_walk[] = {
	{ ai_walk, 4 },{ ai_walk, 5 },{ ai_walk, 6 },{ ai_walk, 4 },
	{ ai_walk, 5 },{ ai_walk, 6 },{ ai_walk, 4 },{ ai_walk, 5 }
};
MMOVE_T(ogre_move_walk) = { FRAME_walk01, FRAME_walk08, ogre_frames_walk, nullptr };

MONSTERINFO_WALK(ogre_walk) (gentity_t* self) -> void {
	M_SetAnimation(self, &ogre_move_walk);
}

/*
===============
ogre_run
===============
*/
static MonsterFrame ogre_frames_run[] = {
	{ ai_run, 12 },{ ai_run, 14 },{ ai_run, 10 },{ ai_run, 12 },
	{ ai_run, 16 },{ ai_run, 10 },{ ai_run, 12 },{ ai_run, 14 }
};
MMOVE_T(ogre_move_run) = { FRAME_run01, FRAME_run08, ogre_frames_run, nullptr };

MONSTERINFO_RUN(ogre_run) (gentity_t* self) -> void {
	if (self->monsterInfo.aiFlags & AI_STAND_GROUND)
		M_SetAnimation(self, &ogre_move_stand);
	else
		M_SetAnimation(self, &ogre_move_run);
}

/*
===============
ogre_melee_hit
===============
*/
static void ogre_melee_hit(gentity_t* self) {
        if (!self->enemy || self->enemy->health <= 0)
                return;

        Vector3 aim = { MELEE_DISTANCE, self->maxs[0], 8 };
        const int dmg = irandom(15, 25);

        if (fire_hit(self, aim, dmg, 100))
                gi.sound(self, CHAN_WEAPON, snd_melee_hit, 1, ATTN_NORM, 0);
        else {
                gi.sound(self, CHAN_WEAPON, snd_melee_swing, 1, ATTN_NORM, 0);
                self->monsterInfo.melee_debounce_time = level.time + 1_sec;
        }
}

/*
===============
ogre_melee
===============
*/
static void ogre_melee_refire(gentity_t* self) {
        if (!self->enemy || !self->enemy->inUse || self->enemy->health <= 0)
                return;

        if ((skill && skill->integer >= 3) || range_to(self, self->enemy) == RANGE_MELEE) {
                self->monsterInfo.nextFrame = FRAME_melee01;
        }
        else {
                self->monsterInfo.melee_debounce_time = level.time + 600_ms;
                ogre_attack(self);
        }
}

static MonsterFrame ogre_frames_melee[] = {
        { ai_charge, 8 },
        { ai_charge, 8 },
        { ai_charge, 0, ogre_melee_hit },
        { ai_charge, 5 },
        { ai_charge, 0, ogre_melee_hit },
        { ai_charge, 6, ogre_melee_refire }
};
MMOVE_T(ogre_move_melee) = { FRAME_melee01, FRAME_melee06, ogre_frames_melee, ogre_run };

MONSTERINFO_MELEE(ogre_check_refire) (gentity_t* self) -> void {
        if (!self->enemy || !self->enemy->inUse || self->enemy->health <= 0)
                return;

        M_SetAnimation(self, &ogre_move_melee);
}

/*
===============
ogre_can_grenade
===============
*/
static bool ogre_can_grenade(gentity_t* self) {
	if (!self->enemy)
		return false;
	if (self->spawnFlags.has(SPAWNFLAG_OGRE_NOGRENADE))
		return false;

	Vector3 start;
	if (!M_CheckClearShot(self, { 0, 0, 32 }, start))
		return false;

	const float d = (self->enemy->s.origin - self->s.origin).length();
	if (d < 160.0f)
		return false;

	if (self->absMin[2] + 192 < self->enemy->absMin[2])
		return false;

	return true;
}

/*
===============
Select Ogre grenade flash + spread
===============
*/
static void ogre_select_grenade_flash(int frame, MonsterMuzzleFlashID& flash_number, float& spread) {
        switch (frame) {
        case FRAME_gren01:
        case FRAME_gren02:
                flash_number = MZ2_GUNCMDR_GRENADE_FRONT_1;
                spread = -0.10f;
                break;

        case FRAME_gren03:
        case FRAME_gren04:
                flash_number = MZ2_GUNCMDR_GRENADE_FRONT_2;
                spread = -0.05f;
                break;

        case FRAME_gren05:
        case FRAME_gren06:
                flash_number = MZ2_GUNCMDR_GRENADE_FRONT_3;
                spread = 0.05f;
                break;

        default: // FRAME_gren07..08 and fallback
                flash_number = MZ2_GUNCMDR_GRENADE_FRONT_2;
                spread = 0.10f;
                break;
        }
}

/*
===============
ogre_grenade_fire
===============
*/
static void ogre_grenade_fire(gentity_t* self, bool multiGrenade) {
        if (!self->enemy || !self->enemy->inUse)
                return;

	Vector3 forward, right, up;
	AngleVectors(self->s.angles, forward, right, up);

	// pick flash + spread based on current frame
	MonsterMuzzleFlashID flash_number;
	float spread;
	ogre_select_grenade_flash(self->s.frame, flash_number, spread);

	// base muzzle origin from flash offsets
	Vector3 start = M_ProjectFlashSource(self, monster_flash_offset[flash_number], forward, right);

	// target (enemy or blindFire)
	Vector3 target = (self->monsterInfo.aiFlags & AI_MANUAL_STEERING && self->monsterInfo.blind_fire_target)
		? self->monsterInfo.blind_fire_target
		: self->enemy->s.origin;

	// initial aim is forward + right * spread
	Vector3 aim = forward + (right * spread);

	// add pitch bias if distance is far
	Vector3 toTarget = target - self->s.origin;
	float dist = toTarget.length();
	float pitch = 0.0f;

	if ((dist > 512.0f) && (std::fabs(toTarget[2]) < 64.0f)) {
		pitch = (dist - 512.0f) / 1024.0f;
		pitch = std::clamp(pitch, -0.5f, 0.4f);
	}
	aim += (up * pitch);

	// if CalculatePitchToFire fails, fall back to predictive lob
	if (!M_CalculatePitchToFire(self, target, start, aim, 600, 2.5f, false)) {
		Vector3 lead;
		PredictAim(self, self->enemy, start, 0, true, 0.0f, &lead, nullptr);
		lead[2] += 0.2f; // upward bias
		aim = lead.normalized();
	}

	gi.sound(self, CHAN_WEAPON, snd_grenade, 1, ATTN_NORM, 0);

	// randomize lateral/up velocity offsets like gunner does
	const float rightAdjust = crandom_open() * 10.0f;
	const float upAdjust = frandom() * 10.0f;

        if (multiGrenade)
                monster_fire_multigrenade(self, start, aim, 40, 600, flash_number, rightAdjust, upAdjust);
        else
                monster_fire_grenade(self, start, aim, 40, 600, flash_number, rightAdjust, upAdjust);
}

/*
===============
ogre_attack_grenade
===============
*/
static void ogre_flak_fire(gentity_t* self) {
        if (!self->enemy || !self->enemy->inUse)
                return;

        Vector3 forward, right;
        AngleVectors(self->s.angles, forward, right, nullptr);

        MonsterMuzzleFlashID flash_number;
        switch (self->s.frame) {
        case FRAME_gren03:
                flash_number = MZ2_GUNCMDR_GRENADE_FRONT_1;
                break;
        case FRAME_gren05:
                flash_number = MZ2_GUNCMDR_GRENADE_FRONT_3;
                break;
        default:
                flash_number = MZ2_GUNCMDR_GRENADE_FRONT_2;
                break;
        }

        Vector3 start = M_ProjectFlashSource(self, monster_flash_offset[flash_number], forward, right);

        Vector3 aim;
        PredictAim(self, self->enemy, start, 0, true, -0.2f, &aim, nullptr);
        monster_fire_flakcannon(self, start, aim, 4, 800, 500, 500, 5, flash_number);
}

static void ogre_fire(gentity_t* self) {
        const bool isMarksman = self->className && strcmp(self->className, "monster_ogre_marksman") == 0;
        const bool isMultiGrenade = self->className && strcmp(self->className, "monster_ogre_multigrenade") == 0;

        if (isMarksman)
                ogre_flak_fire(self);
        else
                ogre_grenade_fire(self, isMultiGrenade);
}

static MonsterFrame ogre_frames_grenade[] = {
        { ai_charge, 0 },
        { ai_charge, 0 },
        { ai_charge, 0, ogre_fire }, // throw
        { ai_charge, 0 },
        { ai_charge, 0 },
        { ai_charge, 0 },
	{ ai_charge, 0 },
	{ ai_charge, 0 }
};
MMOVE_T(ogre_move_grenade) = { FRAME_gren01, FRAME_gren08, ogre_frames_grenade, ogre_run };

MONSTERINFO_ATTACK(ogre_attack) (gentity_t* self) -> void {
	if (ogre_can_grenade(self))
		M_SetAnimation(self, &ogre_move_grenade);
	else if (self->enemy && range_to(self, self->enemy) <= RANGE_MELEE)
		M_SetAnimation(self, &ogre_move_melee);
}

/*
===============
ogre_checkattack
===============
*/
MONSTERINFO_CHECKATTACK(ogre_checkattack) (gentity_t* self) -> bool {
	if (!self->enemy || self->enemy->health <= 0)
		return false;

	if (range_to(self, self->enemy) <= RANGE_MELEE && self->monsterInfo.melee_debounce_time <= level.time) {
		self->monsterInfo.attackState = MonsterAttackState::Melee;
		return true;
	}

	if (ogre_can_grenade(self)) {
		self->monsterInfo.attackState = MonsterAttackState::Missile;
		return true;
	}

	return false;
}

/*
===============
ogre_pain
===============
*/
static MonsterFrame ogre_frames_painA[] = {
	{ ai_move },{ ai_move },{ ai_move },{ ai_move },{ ai_move }
};
MMOVE_T(ogre_move_painA) = { FRAME_pain01, FRAME_pain05, ogre_frames_painA, ogre_run };

static MonsterFrame ogre_frames_painB[] = {
	{ ai_move },{ ai_move },{ ai_move },{ ai_move },{ ai_move },{ ai_move },{ ai_move }
};
MMOVE_T(ogre_move_painB) = { FRAME_painb01, FRAME_painb07, ogre_frames_painB, ogre_run };

static PAIN(ogre_pain) (gentity_t* self, gentity_t* other, float kick, int damage, const MeansOfDeath& mod) -> void {
	if (level.time < self->pain_debounce_time)
		return;

	self->pain_debounce_time = level.time + 2.0_sec;

	gi.sound(self, CHAN_VOICE, snd_pain1, 1, ATTN_NORM, 0);

	if (!M_ShouldReactToPain(self, mod))
		return;

	if (damage <= 20)
		M_SetAnimation(self, &ogre_move_painA);
	else
		M_SetAnimation(self, &ogre_move_painB);
}

/*
===============
ogre_death helpers
===============
*/
static void ogre_shrink(gentity_t* self) {
	self->maxs[2] = 0;
	self->svFlags |= SVF_DEADMONSTER;
	gi.linkEntity(self);
}

/*
===============
ogre_die
===============
*/
static MonsterFrame ogre_frames_death1[] = {
	{ ai_move },{ ai_move },{ ai_move, 0, ogre_shrink },{ ai_move },
	{ ai_move },{ ai_move },{ ai_move },{ ai_move }
};
MMOVE_T(ogre_move_death1) = { FRAME_death01, FRAME_death08, ogre_frames_death1, monster_dead };

static MonsterFrame ogre_frames_death2[] = {
	{ ai_move },{ ai_move },{ ai_move, 0, ogre_shrink },{ ai_move },
	{ ai_move },{ ai_move },{ ai_move },{ ai_move }
};
MMOVE_T(ogre_move_death2) = { FRAME_deathb01, FRAME_deathb08, ogre_frames_death2, monster_dead };

static DIE(ogre_die) (gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, const Vector3& point, const MeansOfDeath& mod) -> void {
	if (M_CheckGib(self, mod)) {
		gi.sound(self, CHAN_VOICE, gi.soundIndex("misc/udeath.wav"), 1, ATTN_NORM, 0);

		ThrowGibs(self, damage, {
			{ 2, "models/objects/gibs/bone/tris.md2" },
			{ 4, "models/objects/gibs/sm_meat/tris.md2" },
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

	if (brandom())
		M_SetAnimation(self, &ogre_move_death1);
	else
		M_SetAnimation(self, &ogre_move_death2);
}

/*
===============
ogre_setskin
===============
*/
MONSTERINFO_SETSKIN(ogre_setskin) (gentity_t* self) -> void {
        int baseSkin = 0;

        if (self->className) {
                if (strcmp(self->className, "monster_ogre_marksman") == 0)
                        baseSkin = 2;
                else if (strcmp(self->className, "monster_ogre_multigrenade") == 0)
                        baseSkin = 4;
        }

        const bool injured = self->maxHealth > 0 && self->health < (self->maxHealth / 2);
        self->s.skinNum = baseSkin + (injured ? 1 : 0);
}

/*
===============
SP_monster_ogre
===============
*/
/*QUAKED monster_ogre (1 0 0) (-24 -24 -24) (24 24 32) AMBUSH TRIGGER_SPAWN SIGHT NOGRENADE x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/monsters/ogre/tris.md2"
*/
void SP_monster_ogre(gentity_t* self) {
	if (!M_AllowSpawn(self)) {
		FreeEntity(self);
		return;
	}

	// sounds
	snd_melee_swing.assign("ogre/ogsawatk.wav");
	snd_melee_hit.assign("ogre/oghit.wav");
	snd_grenade.assign("ogre/ogthrow.wav");
	snd_pain1.assign("ogre/ogpain1.wav");
	snd_death.assign("ogre/ogdth.wav");
	snd_idle.assign("ogre/ogidle.wav");
	snd_idle2.assign("ogre/ogidle2.wav");
	snd_sight.assign("ogre/ogsight.wav");
	snd_search.assign("ogre/ogsearch.wav");

	// model
	self->moveType = MoveType::Step;
	self->solid = SOLID_BBOX;
	self->s.modelIndex = gi.modelIndex("models/monsters/ogre/tris.md2");

	// bounds and stats
	self->mins = { -24, -24, -24 };
	self->maxs = { 24,  24,  32 };

        self->health = 200 * st.health_multiplier;
        self->maxHealth = self->health;
        self->gibHealth = -80;
        self->mass = 300;

        self->item = &itemList[IT_PACK];

        // callbacks
        self->pain = ogre_pain;
        self->die = ogre_die;

	self->monsterInfo.stand = ogre_stand;
	self->monsterInfo.walk = ogre_walk;
	self->monsterInfo.run = ogre_run;
	self->monsterInfo.dodge = nullptr;
        self->monsterInfo.attack = ogre_attack;     // grenade
        self->monsterInfo.melee = ogre_check_refire;      // chainsaw
	self->monsterInfo.sight = ogre_sight;
	self->monsterInfo.search = ogre_search;
	self->monsterInfo.idle = ogre_idle;
	self->monsterInfo.checkAttack = ogre_checkattack;
	self->monsterInfo.blocked = nullptr;
	self->monsterInfo.setSkin = ogre_setskin;

	gi.linkEntity(self);

	M_SetAnimation(self, &ogre_move_stand);

	self->monsterInfo.combatStyle = CombatStyle::Melee;

	self->monsterInfo.scale = OGRE_MODEL_SCALE;
	self->monsterInfo.dropHeight = 192;
	self->monsterInfo.jumpHeight = 0; // does not actively jump

	walkmonster_start(self);
}

/*
===============
SP_monster_ogre_marksman
===============
*/
/*QUAKED monster_ogre_marksman (1 0 0) (-24 -24 -24) (24 24 32) AMBUSH TRIGGER_SPAWN SIGHT NOGRENADE x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/monsters/ogre/tris.md2"
*/
void SP_monster_ogre_marksman(gentity_t* self) {
        SP_monster_ogre(self);
        self->s.skinNum = 2;
}

void SP_monster_ogre_multigrenade(gentity_t* self) {
        SP_monster_ogre(self);
        self->s.skinNum = 4;
}
