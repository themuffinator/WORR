/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

m_knight.cpp implementation.*/

#include "../g_local.hpp"
#include "m_knight.hpp"

constexpr SpawnFlags SPAWNFLAG_KNIGHT_NOJUMPING = 8_spawnflag;
constexpr SpawnFlags SPAWNFLAG_KNIGHT_KNEEL = 16_spawnflag;
constexpr SpawnFlags SPAWNFLAG_STATUE_STATIONARY = 32_spawnflag;

static cached_soundIndex s_idle;
static cached_soundIndex s_sight;
static cached_soundIndex s_pain;
static cached_soundIndex s_death;
static cached_soundIndex s_sword_hit;
static cached_soundIndex s_sword_miss;
static cached_soundIndex s_sword_swing;

static constexpr int KNIGHT_BASE_HEALTH = 75;
static constexpr int KNIGHT_BASE_GIB_HEALTH = -40;
static constexpr int KNIGHT_BASE_MASS = 120;
static constexpr int KNIGHT_STATUE_HEALTH = 125;
static constexpr int KNIGHT_STATUE_GIB_HEALTH = -100;
static constexpr int KNIGHT_STATUE_MASS = 175;
static constexpr float KNIGHT_MODEL_SCALE = 1.0f;

void knight_stand(gentity_t* self);
void knight_walk(gentity_t* self);
void knight_run(gentity_t* self);

/*
Helpers
*/
static void knight_idle(gentity_t* self) {
    if (frandom() < 0.2f)
        gi.sound(self, CHAN_VOICE, s_idle, 1, ATTN_IDLE, 0);
}

static void knight_search_idle_sound(gentity_t* self) {
    gi.sound(self, CHAN_VOICE, s_idle, 1, ATTN_IDLE, 0);
}

static void knight_step(gentity_t* self) {
    monster_footstep(self);
}

MONSTERINFO_SIGHT(knight_sight) (gentity_t* self, gentity_t* other) -> void {
        gi.sound(self, CHAN_VOICE, s_sight, 1, ATTN_NORM, 0);
}

MONSTERINFO_SEARCH(knight_search) (gentity_t* self) -> void {
        knight_search_idle_sound(self);
}

static void knight_sword_sound(gentity_t* self) {
        gi.sound(self, CHAN_WEAPON, s_sword_hit, 1, ATTN_NORM, 0);
}

static void knight_check_dist(gentity_t* self) {
        if (!self->enemy || !self->enemy->inUse || self->enemy->health <= 0)
                return;

        if (range_to(self, self->enemy) <= RANGE_MELEE) {
                self->monsterInfo.nextFrame = FRAME_attackb1;
        }
        else if (frandom() > 0.6f) {
                self->monsterInfo.nextFrame = FRAME_runattack1;
        }
        else {
                self->monsterInfo.nextFrame = FRAME_run1;
        }
}

static void knight_melee_hit(gentity_t* self) {
        Vector3 aim = { MELEE_DISTANCE, self->mins[0], -4.0f };

        if (fire_hit(self, aim, irandom(15, 25), 200)) {
		// Successful strike
		gi.sound(self, CHAN_WEAPON, s_sword_hit, 1, ATTN_NORM, 0);
	}
	else {
		// Missed swing
		gi.sound(self, CHAN_WEAPON, s_sword_miss, 1, ATTN_NORM, 0);
		self->monsterInfo.melee_debounce_time = level.time + 1.5_sec;
	}
}

/*
Attack sequence
*/
static void knight_run_attack_hit(gentity_t* self) {
        Vector3 aim = { MELEE_DISTANCE, self->mins[0], 2.0f };

        if (fire_hit(self, aim, irandom(15, 25), 200)) {
                gi.sound(self, CHAN_WEAPON, s_sword_hit, 1, ATTN_NORM, 0);
        }
        else {
                gi.sound(self, CHAN_WEAPON, s_sword_miss, 1, ATTN_NORM, 0);
                self->monsterInfo.melee_debounce_time = level.time + 1.5_sec;
        }
}

static MonsterFrame frames_attack_run[] = {
        { ai_charge, 20 },
        { ai_charge, 20, knight_sword_sound },
        { ai_charge, 13 },
        { ai_charge, 7 },
        { ai_charge, 16 },
        { ai_charge, 20, knight_run_attack_hit },
        { ai_charge, 14 },
        { ai_charge, 14 },
        { ai_charge, 14 },
        { ai_charge, 14 },
        { ai_charge, 6, knight_check_dist }
};
void knight_run(gentity_t* self);
MMOVE_T(knight_move_attack_run) = { FRAME_runattack1, FRAME_runattack11, frames_attack_run, knight_run };

MONSTERINFO_ATTACK(knight_attack_run) (gentity_t* self) -> void {
        M_SetAnimation(self, &knight_move_attack_run);
}

static MonsterFrame frames_attack[] = {
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge, 0, knight_melee_hit },
        { ai_charge },
        { ai_charge },
        { ai_charge, 0, knight_check_dist }
};
MMOVE_T(knight_move_attack) = { FRAME_attackb1, FRAME_attackb10, frames_attack, knight_run };

MONSTERINFO_MELEE(knight_melee) (gentity_t* self) -> void {
        M_SetAnimation(self, &knight_move_attack);
}

/*
Stand / Walk / Run
*/
void knight_stand(gentity_t* self);

static MonsterFrame frames_stand[] = {
        { ai_stand, 0, knight_idle },
        { ai_stand }, { ai_stand }, { ai_stand },
        { ai_stand }, { ai_stand }, { ai_stand }, { ai_stand }, { ai_stand }
};
MMOVE_T(knight_move_stand) = { FRAME_stand1, FRAME_stand9, frames_stand, knight_stand };

static MonsterFrame frames_stationary[] = {
        { ai_stand }
};
MMOVE_T(knight_move_stationary) = { FRAME_stand1, FRAME_stand1, frames_stationary, nullptr };

static MonsterFrame frames_kneel[] = {
        { ai_stand }
};
MMOVE_T(knight_move_kneel) = { FRAME_kneel5, FRAME_kneel5, frames_kneel, nullptr };

MONSTERINFO_STAND(knight_stand) (gentity_t* self) -> void {
        if (self->monsterInfo.active_move == &knight_move_stationary) {
                M_SetAnimation(self, &knight_move_stationary);
        }
        else if (self->monsterInfo.active_move == &knight_move_kneel) {
                M_SetAnimation(self, &knight_move_kneel);
        }
        else {
                M_SetAnimation(self, &knight_move_stand);
        }
}

void knight_walk(gentity_t* self);
static MonsterFrame frames_walk[] = {
        { ai_walk, 3, knight_step }, { ai_walk, 2 }, { ai_walk, 3 }, { ai_walk, 4 },
        { ai_walk, 3 }, { ai_walk, 3 }, { ai_walk, 3 }, { ai_walk, 4 },
        { ai_walk, 3 }, { ai_walk, 3 }, { ai_walk, 2 }, { ai_walk, 3 },
        { ai_walk, 4 }, { ai_walk, 3 }, { ai_walk, 3 }, { ai_walk, 3 }
};
MMOVE_T(knight_move_walk) = { FRAME_walk1, FRAME_walk16, frames_walk, nullptr };
MONSTERINFO_WALK(knight_walk) (gentity_t* self) -> void {
        M_SetAnimation(self, &knight_move_walk);
}

static MonsterFrame frames_run[] = {
        { ai_run, 16, knight_step },
        { ai_run, 20 },
        { ai_run, 13 },
        { ai_run, 7, knight_step },
        { ai_run, 16 },
        { ai_run, 20 },
        { ai_run, 14 },
        { ai_run, 6, knight_check_dist }
};
MMOVE_T(knight_move_run) = { FRAME_run1, FRAME_run8, frames_run, nullptr };
MONSTERINFO_RUN(knight_run) (gentity_t* self) -> void {
        if (self->monsterInfo.active_move == &knight_move_stationary ||
                self->monsterInfo.active_move == &knight_move_kneel) {
                M_SetAnimation(self, &knight_move_run);
                return;
        }

        if (self->monsterInfo.aiFlags & AI_STAND_GROUND) {
                M_SetAnimation(self, &knight_move_stand);
                return;
        }

        M_SetAnimation(self, &knight_move_run);
}

static MonsterFrame knight_frames_pain1[] = {
        { ai_move },
        { ai_move },
        { ai_move }
};
MMOVE_T(knight_move_pain1) = { FRAME_pain1, FRAME_pain3, knight_frames_pain1, knight_run };

static MonsterFrame knight_frames_pain2[] = {
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
MMOVE_T(knight_move_pain2) = { FRAME_painb1, FRAME_painb11, knight_frames_pain2, knight_run };

/*
Pain
*/
static PAIN(knight_pain) (gentity_t* self, gentity_t* other, float kick, int damage,
        const MeansOfDeath& mod) -> void {
        if (level.time < self->pain_debounce_time)
                return;
        self->pain_debounce_time = level.time + 1_sec;

        if (!M_ShouldReactToPain(self, mod))
                return;

        gi.sound(self, CHAN_VOICE, s_pain, 1, ATTN_NORM, 0);

        if (frandom() < 0.5f)
                M_SetAnimation(self, &knight_move_pain1);
        else
                M_SetAnimation(self, &knight_move_pain2);
}

MONSTERINFO_SETSKIN(knight_setskin) (gentity_t* self) -> void {
        if (self->health < (self->maxHealth / 2))
                self->s.skinNum |= 1;
        else
                self->s.skinNum &= ~1;
}

static void knight_shrink(gentity_t* self) {
        self->maxs[2] = 0;
        self->svFlags |= SVF_DEADMONSTER;
        gi.linkEntity(self);
}

static MonsterFrame knight_frames_death1[] = {
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move, 0, knight_shrink },
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move }
};
MMOVE_T(knight_move_death1) = { FRAME_death1, FRAME_death10, knight_frames_death1, monster_dead };

static MonsterFrame knight_frames_death2[] = {
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move, 0, knight_shrink },
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move }
};
MMOVE_T(knight_move_death2) = { FRAME_deathb1, FRAME_deathb11, knight_frames_death2, monster_dead };

static DIE(knight_die) (gentity_t* self, gentity_t* inflictor, gentity_t* attacker,
        int damage, const Vector3& point, const MeansOfDeath& mod) -> void {
        const bool isStatue = self->spawnFlags.has(SPAWNFLAG_STATUE_STATIONARY) ||
                (self->className && strcmp(self->className, "monster_statue") == 0);

        if (M_CheckGib(self, mod)) {
                gi.sound(self, CHAN_VOICE, gi.soundIndex("misc/udeath.wav"), 1, ATTN_NORM, 0);

                self->s.skinNum &= ~1;

                if (isStatue) {
                        ThrowGibs(self, damage, {
                                { 2, "models/objects/gibs/bone/tris.md2" },
                                { 4, "models/objects/gibs/sm_meat/tris.md2" },
                                { "models/monsters/knight/gibs/head.md2", GIB_HEAD | GIB_SKINNED | GIB_DEBRIS }
                        });
                }
                else {
                        ThrowGibs(self, damage, {
                                { 2, "models/objects/gibs/bone/tris.md2" },
                                { 4, "models/objects/gibs/sm_meat/tris.md2" },
                                { "models/objects/gibs/chest/tris.md2" },
                                { "models/monsters/knight/gibs/head.md2", GIB_HEAD | GIB_SKINNED }
                        });
                }

                self->deadFlag = true;
                return;
        }

        if (self->deadFlag)
                return;

        gi.sound(self, CHAN_VOICE, s_death, 1, ATTN_NORM, 0);
        self->deadFlag = true;
        self->takeDamage = true;

        if (frandom() < 0.5f)
                M_SetAnimation(self, &knight_move_death1);
        else
                M_SetAnimation(self, &knight_move_death2);
}

static void knight_precache() {
    gi.modelIndex("models/monsters/knight/tris.md2");
    gi.modelIndex("models/monsters/knight/gibs/head.md2");
    gi.modelIndex("models/objects/gibs/chest/tris.md2");

    s_idle.assign("knight/idle.wav");
    s_sight.assign("knight/sight.wav");
    s_pain.assign("knight/pain.wav");
    s_death.assign("knight/death.wav");
    s_sword_hit.assign("knight/sword1.wav");
    s_sword_swing.assign("knight/sword2.wav");
    s_sword_miss.assign("knight/swordmiss.wav");
}

void SP_monster_knight(gentity_t* self) {
        if (!M_AllowSpawn(self)) {
                FreeEntity(self);
                return;
        }

        knight_precache();

        self->s.modelIndex = gi.modelIndex("models/monsters/knight/tris.md2");

        self->mins = { -16, -16, -24 };
        self->maxs = { 16,  16,  32 };
        self->moveType = MoveType::Step;
        self->solid = SOLID_BBOX;
        self->yawSpeed = 15;

        const bool isStatue = self->className && (strcmp(self->className, "monster_statue") == 0);

        if (isStatue) {
                self->health = self->maxHealth = KNIGHT_STATUE_HEALTH * st.health_multiplier;
                self->gibHealth = KNIGHT_STATUE_GIB_HEALTH;
                self->mass = KNIGHT_STATUE_MASS;
        }
        else {
                self->health = self->maxHealth = KNIGHT_BASE_HEALTH * st.health_multiplier;
                self->gibHealth = KNIGHT_BASE_GIB_HEALTH;
                self->mass = KNIGHT_BASE_MASS;
        }

        self->monsterInfo.stand = knight_stand;
        self->monsterInfo.walk = knight_walk;
        self->monsterInfo.run = knight_run;
        self->monsterInfo.attack = knight_attack_run;
        self->monsterInfo.melee = knight_melee;
        self->monsterInfo.sight = knight_sight;
        self->monsterInfo.search = knight_search;
        self->monsterInfo.setSkin = knight_setskin;
        self->pain = knight_pain;
        self->die = knight_die;

        self->monsterInfo.scale = KNIGHT_MODEL_SCALE;
        self->monsterInfo.combatStyle = CombatStyle::Melee;
        self->monsterInfo.canJump = !self->spawnFlags.has(SPAWNFLAG_KNIGHT_NOJUMPING);
        self->monsterInfo.dropHeight = 256;
        self->monsterInfo.jumpHeight = 68;

        knight_setskin(self);

        if (self->spawnFlags.has(SPAWNFLAG_STATUE_STATIONARY)) {
                M_SetAnimation(self, &knight_move_stationary);
        }
        else if (self->spawnFlags.has(SPAWNFLAG_KNIGHT_KNEEL)) {
                M_SetAnimation(self, &knight_move_kneel);
        }
        else {
                M_SetAnimation(self, &knight_move_stand);
        }

        gi.linkEntity(self);
        walkmonster_start(self);
}
