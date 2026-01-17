/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

============================================================================== FIEND (Demon) -
WOR port - Melee claws at close range - Mid/long pounce (leap) that deals on-impact damage - Two
pain sets, two death sets - Sight/search/idle barks
==============================================================================*/

#include "../g_local.hpp"
#include "m_fiend.hpp"

/*
===============
Spawnflags
===============
*/
constexpr SpawnFlags SPAWNFLAG_FIEND_NOJUMPING = 8_spawnflag;

/*
===============
Sounds
===============
*/
static cached_soundIndex sound_swing;
static cached_soundIndex sound_hit;
static cached_soundIndex sound_jump;
static cached_soundIndex sound_death;
static cached_soundIndex sound_idle1;
static cached_soundIndex sound_idle2;
static cached_soundIndex sound_search;
static cached_soundIndex sound_pain;
static cached_soundIndex sound_sight1;
static cached_soundIndex sound_sight2;
static cached_soundIndex sound_land;

/*
===============
fiend_sight
===============
*/
MONSTERINFO_SIGHT(fiend_sight) (gentity_t* self, gentity_t* other) -> void {
        if (frandom() > 0.5f)
                gi.sound(self, CHAN_VOICE, sound_sight1, 1, ATTN_IDLE, 0);
        else
                gi.sound(self, CHAN_VOICE, sound_sight2, 1, ATTN_IDLE, 0);
}

/*
===============
fiend_search
===============
*/
MONSTERINFO_SEARCH(fiend_search) (gentity_t* self) -> void {
        gi.sound(self, CHAN_VOICE, sound_search, 1, ATTN_IDLE, 0);
}

/*
===============
fiend_idle
===============
*/
MONSTERINFO_IDLE(fiend_idle) (gentity_t* self) -> void {
        if (frandom() > 0.5f)
                gi.sound(self, CHAN_VOICE, sound_idle1, 1, ATTN_IDLE, 0);
        else
                gi.sound(self, CHAN_VOICE, sound_idle2, 1, ATTN_IDLE, 0);
}

/*
===============
fiend_stand
===============
*/
static MonsterFrame fiend_frames_stand[] = {
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
MMOVE_T(fiend_move_stand) = { FRAME_stand01, FRAME_stand13, fiend_frames_stand, nullptr };

MONSTERINFO_STAND(fiend_stand) (gentity_t* self) -> void {
	M_SetAnimation(self, &fiend_move_stand);
}

/*
===============
fiend_walk
===============
*/
static MonsterFrame fiend_frames_walk[] = {
        { ai_walk, 8 },
        { ai_walk, 6 },
        { ai_walk, 6 },
        { ai_walk, 7 },
        { ai_walk, 4 },
        { ai_walk, 6 },
        { ai_walk, 10 },
        { ai_walk, 10 }
};
MMOVE_T(fiend_move_walk) = { FRAME_walk01, FRAME_walk08, fiend_frames_walk, nullptr };

MONSTERINFO_WALK(fiend_walk) (gentity_t* self) -> void {
	M_SetAnimation(self, &fiend_move_walk);
}

/*
===============
fiend_run
===============
*/
static MonsterFrame fiend_frames_run[] = {
        { ai_run, 20 },
        { ai_run, 15 },
        { ai_run, 36 },
        { ai_run, 20 },
        { ai_run, 15 },
        { ai_run, 36 }
};
MMOVE_T(fiend_move_run) = { FRAME_run01, FRAME_run06, fiend_frames_run, nullptr };

MONSTERINFO_RUN(fiend_run) (gentity_t* self) -> void {
	if (self->monsterInfo.aiFlags & AI_STAND_GROUND)
		M_SetAnimation(self, &fiend_move_stand);
	else
		M_SetAnimation(self, &fiend_move_run);
}

/*
===============
fiend_hit_left
===============
*/
static void fiend_hit_left(gentity_t* self) {
        Vector3 aim = { MELEE_DISTANCE, self->mins[0], 8.0f };

        if (fire_hit(self, aim, irandom(5, 15), 100)) {
                gi.sound(self, CHAN_WEAPON, sound_hit, 1, ATTN_NORM, 0);
        } else {
                gi.sound(self, CHAN_WEAPON, sound_swing, 1, ATTN_NORM, 0);
                self->monsterInfo.melee_debounce_time = level.time + 1.5_sec;
        }
}

/*
===============
fiend_hit_right
===============
*/
static void fiend_hit_right(gentity_t* self) {
        Vector3 aim = { MELEE_DISTANCE, self->maxs[0], 8.0f };

        if (fire_hit(self, aim, irandom(5, 15), 100)) {
                gi.sound(self, CHAN_WEAPON, sound_hit, 1, ATTN_NORM, 0);
        } else {
                gi.sound(self, CHAN_WEAPON, sound_swing, 1, ATTN_NORM, 0);
                self->monsterInfo.melee_debounce_time = level.time + 1.5_sec;
        }
}

/*
===============
fiend_check_refire
===============
*/
static void fiend_check_refire(gentity_t* self) {
        if (!self->enemy || !self->enemy->inUse || self->enemy->health <= 0)
                return;

        if (self->monsterInfo.melee_debounce_time <= level.time &&
                (frandom() < 0.5f || range_to(self, self->enemy) <= RANGE_MELEE)) {
                self->monsterInfo.nextFrame = FRAME_attacka01;
        }
}

/*
===============
fiend_melee
===============
*/
static MonsterFrame fiend_frames_attack[] = {
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge, 0, fiend_hit_left },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge, 0, fiend_hit_right },
        { ai_charge },
        { ai_charge },
        { ai_charge, 0, fiend_check_refire }
};
MMOVE_T(fiend_move_attack) = { FRAME_attacka01, FRAME_attacka15, fiend_frames_attack, fiend_run };

MONSTERINFO_MELEE(fiend_melee) (gentity_t* self) -> void {
        M_SetAnimation(self, &fiend_move_attack);
}

/*
===============
fiend_jump_touch
Deals damage during a pounce when impacting with speed.
===============
*/
static TOUCH(fiend_jump_touch) (gentity_t* self, gentity_t* other, const trace_t& tr, bool /*otherTouchingSelf*/) -> void {
        if (self->health <= 0) {
                self->touch = nullptr;
                return;
        }

        if (self->style == 1 && other->takeDamage) {
                if (self->velocity.length() > 30.0f) {
                        const Vector3 normal = self->velocity.normalized();
                        const Vector3 point = self->s.origin + (normal * self->maxs[0]);
                        const int damage = irandom(40, 50);
                        Damage(other, self, self, self->velocity, point, normal, damage, damage, DamageFlags::Normal, ModID::Unknown);
                        self->style = 0;
                }
        }

        if (!M_CheckBottom(self)) {
                if (self->groundEntity) {
                        self->monsterInfo.nextFrame = FRAME_attacka01;
                        self->touch = nullptr;
                }
                return;
        }

        self->touch = nullptr;
}

/*
===============
fiend_jump_takeoff
===============
*/
static void fiend_jump_takeoff(gentity_t* self) {
        Vector3 forward;
        AngleVectors(self->s.angles, forward, nullptr, nullptr);

        gi.sound(self, CHAN_VOICE, sound_jump, 1, ATTN_NORM, 0);

        self->s.origin[_Z] += 1.0f;
        self->velocity = forward * 425.0f;
        self->velocity[_Z] = 160.0f;
        self->groundEntity = nullptr;

        self->monsterInfo.aiFlags |= AI_DUCKED;
        self->monsterInfo.attackFinished = level.time + 3_sec;

        self->style = 1;
        self->touch = fiend_jump_touch;
}

/*
===============
fiend_check_landing
===============
*/
static void fiend_check_landing(gentity_t* self) {
        monster_jump_finished(self);

        self->owner = nullptr;

        if (self->groundEntity) {
                gi.sound(self, CHAN_WEAPON, sound_land, 1, ATTN_NORM, 0);
                self->monsterInfo.attackFinished = level.time + random_time(500_ms, 1.5_sec);

                if (self->monsterInfo.unDuck)
                        self->monsterInfo.unDuck(self);

                if (self->enemy && range_to(self, self->enemy) <= RANGE_MELEE * 2.0f)
                        self->monsterInfo.melee(self);

                return;
        }

        if (level.time > self->monsterInfo.attackFinished)
                self->monsterInfo.nextFrame = FRAME_attacka01;
        else
                self->monsterInfo.nextFrame = FRAME_attacka08;
}

/*
===============
fiend_jump (missile attack)
===============
*/
static MonsterFrame fiend_frames_leap[] = {
        { ai_charge, 5 },
        { ai_charge, 5 },
        { ai_charge, 5, fiend_jump_takeoff },
        { ai_charge, 10 },
        { ai_charge, 10 },
        { ai_charge, 15 },
        { ai_charge, 15 },
        { ai_charge, 10 },
        { ai_charge, 10 },
        { ai_charge, 5 },
        { ai_charge, 5, fiend_check_landing },
        { ai_charge, 5 }
};
MMOVE_T(fiend_move_leap) = { FRAME_leap01, FRAME_leap12, fiend_frames_leap, fiend_run };

MONSTERINFO_ATTACK(fiend_jump) (gentity_t* self) -> void {
        M_SetAnimation(self, &fiend_move_leap);
}

static void fiend_jump_down(gentity_t* self) {
        Vector3 forward, up;
        Vector3 right;
        AngleVectors(self->s.angles, &forward, &right, &up);

        self->velocity += forward * 100.0f;
        self->velocity += up * 300.0f;
}

static void fiend_jump_up(gentity_t* self) {
        Vector3 forward, up;
        Vector3 right;
        AngleVectors(self->s.angles, &forward, &right, &up);

        self->velocity += forward * 200.0f;
        self->velocity += up * 450.0f;
}

static void fiend_jump_wait_land(gentity_t* self) {
        if (!monster_jump_finished(self) && self->groundEntity == nullptr) {
                self->monsterInfo.nextFrame = self->s.frame;
                return;
        }

        if (self->groundEntity) {
                gi.sound(self, CHAN_WEAPON, sound_land, 1, ATTN_NORM, 0);
                self->monsterInfo.attackFinished = level.time + random_time(500_ms, 1.5_sec);

                if (self->monsterInfo.unDuck)
                        self->monsterInfo.unDuck(self);

                if (self->enemy && range_to(self, self->enemy) <= RANGE_MELEE * 2.0f)
                        self->monsterInfo.melee(self);
        }

        self->monsterInfo.nextFrame = self->s.frame + 1;
}

static MonsterFrame fiend_frames_jump_up[] = {
        { ai_move, -8 },
        { ai_move },
        { ai_move, -8, fiend_jump_up },
        { ai_move },
        { ai_move },
        { ai_move, 0, fiend_jump_wait_land },
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move }
};
MMOVE_T(fiend_move_jump_up) = { FRAME_leap01, FRAME_leap12, fiend_frames_jump_up, fiend_run };

static MonsterFrame fiend_frames_jump_down[] = {
        { ai_move },
        { ai_move },
        { ai_move, 0, fiend_jump_down },
        { ai_move },
        { ai_move },
        { ai_move, 0, fiend_jump_wait_land },
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move }
};
MMOVE_T(fiend_move_jump_down) = { FRAME_leap01, FRAME_leap12, fiend_frames_jump_down, fiend_run };

static void fiend_jump_updown(gentity_t* self, BlockedJumpResult result) {
        if (!self->enemy)
                return;

        if (result == BlockedJumpResult::Jump_Turn_Up)
                M_SetAnimation(self, &fiend_move_jump_up);
        else
                M_SetAnimation(self, &fiend_move_jump_down);
}

MONSTERINFO_BLOCKED(fiend_blocked) (gentity_t* self, float dist) -> bool {
        if (auto result = blocked_checkjump(self, dist); result != BlockedJumpResult::No_Jump) {
                if (result != BlockedJumpResult::Jump_Turn)
                        fiend_jump_updown(self, result);
                return true;
        }

        if (blocked_checkplat(self, dist))
                return true;

        return false;
}
/*
===============
fiend_check_melee
===============
*/
static bool fiend_check_melee(gentity_t* self) {
	return self->enemy && (range_to(self, self->enemy) <= RANGE_MELEE) && (self->monsterInfo.melee_debounce_time <= level.time);
}

/*
===============
fiend_check_jump
Prefer mid range; avoid huge vertical deltas.
===============
*/
static bool fiend_check_jump(gentity_t* self) {
        if (!self->enemy)
                return false;

        if ((self->monsterInfo.attackFinished >= level.time) || self->spawnFlags.has(SPAWNFLAG_FIEND_NOJUMPING))
                return false;

        const float enemy_height = self->enemy->size[2];

        if (self->absMin[2] > (self->enemy->absMin[2] + (0.75f * enemy_height)))
                return false;

        if (self->absMax[2] < (self->enemy->absMin[2] + (0.25f * enemy_height)))
                return false;

        Vector3 to_enemy = self->s.origin - self->enemy->s.origin;
        to_enemy[2] = 0.0f;
        const float distance = to_enemy.length();

        if (distance < 100.0f && self->monsterInfo.melee_debounce_time <= level.time)
                return false;

        if (distance > 100.0f) {
                if (frandom() < 0.9f)
                        return false;
        }

        return true;
}

/*
===============
fiend_checkattack
===============
*/
MONSTERINFO_CHECKATTACK(fiend_checkattack) (gentity_t* self) -> bool {
        if (!self->enemy || self->enemy->health <= 0)
                return false;

        if (fiend_check_melee(self)) {
                self->monsterInfo.attackState = MonsterAttackState::Melee;
                return true;
        }

        if (fiend_check_jump(self)) {
                self->monsterInfo.attackState = MonsterAttackState::Missile;
                return true;
        }

        return false;
}

/*
===============
fiend_pain
===============
*/
static MonsterFrame fiend_frames_pain[] = {
        { ai_move, 4 },
        { ai_move, -3 },
        { ai_move, -8 },
        { ai_move, -3 },
        { ai_move, 2 },
        { ai_move, 5 }
};
MMOVE_T(fiend_move_pain) = { FRAME_pain01, FRAME_pain06, fiend_frames_pain, fiend_run };

static PAIN(fiend_pain) (gentity_t* self, gentity_t* other, float kick, int damage, const MeansOfDeath& mod) -> void {
        if (level.time < self->pain_debounce_time)
                return;

        self->pain_debounce_time = level.time + 3_sec;

        if (!M_ShouldReactToPain(self, mod))
                return;

        gi.sound(self, CHAN_VOICE, sound_pain, 1, ATTN_NORM, 0);

        M_SetAnimation(self, &fiend_move_pain);
}

/*
===============
fiend_die
===============
*/
static void fiend_shrink(gentity_t* self) {
        self->maxs[2] = 0;
        self->svFlags |= SVF_DEADMONSTER;
        gi.linkEntity(self);
}

static MonsterFrame fiend_frames_death[] = {
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move, 0, fiend_shrink },
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move }
};
MMOVE_T(fiend_move_death) = { FRAME_death01, FRAME_death09, fiend_frames_death, monster_dead };

static DIE(fiend_die) (gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, const Vector3& point, const MeansOfDeath& mod) -> void {
        if (M_CheckGib(self, mod)) {
                gi.sound(self, CHAN_VOICE, gi.soundIndex("misc/udeath.wav"), 1, ATTN_NORM, 0);
                ThrowGibs(self, damage, {
                        { 3, "models/objects/gibs/bone/tris.md2" },
                        { 4, "models/objects/gibs/sm_meat/tris.md2" },
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

        M_SetAnimation(self, &fiend_move_death);
}

/*
===============
fiend_setskin
===============
*/
MONSTERINFO_SETSKIN(fiend_setskin) (gentity_t* self) -> void {
        if (self->health < (self->maxHealth / 2))
                self->s.skinNum |= 1;
        else
                self->s.skinNum &= ~1;
}

/*
===============
SP_monster_fiend
===============
*/
/*QUAKED monster_fiend (1 0 0) (-32 -32 -24) (32 32 48) AMBUSH TRIGGER_SPAWN SIGHT NOJUMPING x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/monsters/fiend/tris.md2"
*/
void SP_monster_fiend(gentity_t* self) {
        if (!M_AllowSpawn(self)) {
                FreeEntity(self);
                return;
        }

        sound_swing.assign("mutant/mutatck1.wav");
        sound_hit.assign("fiend/dhit2.wav");
        sound_jump.assign("fiend/djump.wav");
        sound_death.assign("fiend/ddeath.wav");
        sound_idle1.assign("fiend/idle1.wav");
        sound_idle2.assign("fiend/idle2.wav");
        sound_search.assign("demon/search.wav");
        sound_pain.assign("fiend/dpain1.wav");
        sound_sight1.assign("fiend/sight1.wav");
        sound_sight2.assign("fiend/sight2.wav");
        sound_land.assign("fiend/dland2.wav");

        self->moveType = MoveType::Step;
        self->solid = SOLID_BBOX;
        self->s.modelIndex = gi.modelIndex("models/monsters/fiend/tris.md2");

        self->mins = { -32, -32, -24 };
        self->maxs = { 32,  32,  48 };

        self->health = self->maxHealth = 250 * st.health_multiplier;
        self->gibHealth = -180;
        self->mass = 250;

        self->pain = fiend_pain;
        self->die = fiend_die;

        MonsterInfo* m = &self->monsterInfo;

        m->stand = fiend_stand;
        m->walk = fiend_walk;
        m->run = fiend_run;
        m->attack = fiend_jump;
        m->melee = fiend_melee;
        m->sight = fiend_sight;
        m->search = fiend_search;
        m->idle = fiend_idle;
        m->checkAttack = fiend_checkattack;
        m->blocked = fiend_blocked;
        m->setSkin = fiend_setskin;

        gi.linkEntity(self);

        M_SetAnimation(self, &fiend_move_stand);
        m->scale = MODEL_SCALE;

        m->combatStyle = CombatStyle::Melee;
        m->canJump = !self->spawnFlags.has(SPAWNFLAG_FIEND_NOJUMPING);
        m->dropHeight = 256;
        m->jumpHeight = 68;

        walkmonster_start(self);
}
