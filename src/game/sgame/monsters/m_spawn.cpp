/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

============================================================================== TARBABY (Spawn) -
Ionized behaviour port
==============================================================================*/

#include "../g_local.hpp"
#include "m_spawn.hpp"

constexpr SpawnFlags SPAWNFLAG_HELLSPAWN_BABY = 8_spawnflag;

static cached_soundIndex soundDeath;
static cached_soundIndex soundHit;
static cached_soundIndex soundLand;
static cached_soundIndex soundSight;

static void tarbaby_stand(gentity_t* self);
static void tarbaby_takeoff(gentity_t* self);
static void tarbaby_check_landing(gentity_t* self);
static void tarbaby_fly(gentity_t* self);
static void tarbaby_mitosis(gentity_t* self);
static void tarbaby_dead(gentity_t* self);

MONSTERINFO_SIGHT(tarbaby_sight) (gentity_t* self, gentity_t* other) -> void {
        gi.sound(self, CHAN_VOICE, soundSight, 1, ATTN_NORM, 0);
}

static MonsterFrame tarbaby_frames_stand[] = {
        { ai_stand }
};
MMOVE_T(tarbaby_move_stand) = { FRAME_walk1, FRAME_walk1, tarbaby_frames_stand, tarbaby_stand };

MONSTERINFO_STAND(tarbaby_stand) (gentity_t* self) -> void {
        M_SetAnimation(self, &tarbaby_move_stand);
}

static MonsterFrame tarbaby_frames_walk[] = {
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
        { ai_walk, 2 },
        { ai_walk, 2 },
        { ai_walk, 2 },
        { ai_walk, 2 },
        { ai_walk, 2 },
        { ai_walk, 2 },
        { ai_walk, 2 },
        { ai_walk, 2 },
        { ai_walk, 2 },
        { ai_walk, 2 },
        { ai_walk, 2 },
        { ai_walk, 2 },
        { ai_walk, 2 },
        { ai_walk, 2 },
        { ai_walk, 2 }
};
MMOVE_T(tarbaby_move_walk) = { FRAME_walk1, FRAME_walk25, tarbaby_frames_walk, nullptr };

MONSTERINFO_WALK(tarbaby_walk) (gentity_t* self) -> void {
        M_SetAnimation(self, &tarbaby_move_walk);
}

static MonsterFrame tarbaby_frames_run[] = {
        { ai_run },
        { ai_run },
        { ai_run },
        { ai_run },
        { ai_run },
        { ai_run },
        { ai_run },
        { ai_run },
        { ai_run },
        { ai_run },
        { ai_run, 2 },
        { ai_run, 2 },
        { ai_run, 2 },
        { ai_run, 2 },
        { ai_run, 2 },
        { ai_run, 2 },
        { ai_run, 2 },
        { ai_run, 2 },
        { ai_run, 2 },
        { ai_run, 2 },
        { ai_run, 2 },
        { ai_run, 2 },
        { ai_run, 2 },
        { ai_run, 2 },
        { ai_run, 2 }
};
MMOVE_T(tarbaby_move_run) = { FRAME_run1, FRAME_run25, tarbaby_frames_run, nullptr };

MONSTERINFO_RUN(tarbaby_run) (gentity_t* self) -> void {
        M_SetAnimation(self, &tarbaby_move_run);
}

static MonsterFrame tarbaby_frames_fly[] = {
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge, 0, tarbaby_check_landing }
};
MMOVE_T(tarbaby_move_fly) = { FRAME_fly1, FRAME_fly4, tarbaby_frames_fly, tarbaby_fly };

static void tarbaby_fly(gentity_t* self) {
        M_SetAnimation(self, &tarbaby_move_fly);
}

static MonsterFrame tarbaby_frames_jump[] = {
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge, 0, tarbaby_takeoff },
        { ai_charge }
};
MMOVE_T(tarbaby_move_jump) = { FRAME_jump1, FRAME_jump6, tarbaby_frames_jump, tarbaby_fly };

TOUCH(tarbaby_jump_touch) (gentity_t* self, gentity_t* other, const trace_t& tr, bool otherTouchingSelf) -> void {
        (void)tr;
        (void)otherTouchingSelf;

        if (self->health <= 0) {
                self->touch = nullptr;
                return;
        }

        if (self->style == 1 && other && other->takeDamage) {
                if (self->velocity.length() > 400.0f) {
                        Vector3 normal = self->velocity;
                        if (normal.normalize() == 0.0f)
                                normal = { 0.0f, 0.0f, 1.0f };

                        Vector3 point = self->s.origin + normal * self->maxs[_X];
                        const int damage = irandom(10, 21);

                        Damage(other, self, self, self->velocity, point, normal, damage, damage, DamageFlags::Normal, ModID::Unknown);
                        gi.sound(self, CHAN_VOICE, soundHit, 1, ATTN_NORM, 0);
                        self->style = 0;
                }
        } else if (!other || !other->takeDamage) {
                gi.sound(self, CHAN_VOICE, soundLand, 1, ATTN_NORM, 0);
        }

        if (!M_CheckBottom(self)) {
                if (self->groundEntity) {
                        self->moveType = MoveType::Step;
                        self->monsterInfo.aiFlags &= ~AI_DUCKED;
                        M_SetAnimation(self, &tarbaby_move_run);
                        self->touch = nullptr;
                }
                return;
        }

        self->touch = nullptr;
        self->moveType = MoveType::Step;
        self->monsterInfo.aiFlags &= ~AI_DUCKED;
        M_SetAnimation(self, &tarbaby_move_jump);
}

static void tarbaby_check_landing(gentity_t* self) {
        monster_jump_finished(self);

        if (self->groundEntity) {
                gi.sound(self, CHAN_WEAPON, soundLand, 1, ATTN_NORM, 0);
                self->monsterInfo.attackFinished = 0_ms;

                if (self->monsterInfo.unDuck)
                        self->monsterInfo.unDuck(self);
                else
                        self->monsterInfo.aiFlags &= ~AI_DUCKED;

                self->moveType = MoveType::Step;
                self->style = 0;
        }

        self->count++;
        if (self->count >= 4) {
                M_SetAnimation(self, &tarbaby_move_jump);
                self->monsterInfo.nextFrame = FRAME_jump5;
                tarbaby_takeoff(self);
        }
}

static void tarbaby_takeoff(gentity_t* self) {
        Vector3 forward;

        AngleVectors(self->s.angles, &forward, nullptr, nullptr);

        self->moveType = MoveType::Bounce;
        self->s.origin[_Z] += 1.0f;
        self->velocity = forward * 600.0f;
        self->velocity[_Z] = 200.0f;
        self->groundEntity = nullptr;
        self->monsterInfo.aiFlags |= AI_DUCKED;
        self->monsterInfo.attackFinished = level.time + 3_sec;
        self->count = 0;
        self->style = 1;
        self->touch = tarbaby_jump_touch;
}

MONSTERINFO_ATTACK(tarbaby_jump) (gentity_t* self) -> void {
        if (!self->enemy)
                return;

        M_SetAnimation(self, &tarbaby_move_jump);
}

static void tarbaby_assign_enemy(gentity_t* self, gentity_t* ent) {
        gentity_t* designatedEnemy = nullptr;

        if (!CooperativeModeOn()) {
                designatedEnemy = self->enemy;
        } else {
                designatedEnemy = PickCoopTarget(ent);
                if (designatedEnemy) {
                        if (designatedEnemy == self->enemy) {
                                designatedEnemy = PickCoopTarget(ent);
                                if (!designatedEnemy)
                                        designatedEnemy = self->enemy;
                        }
                } else {
                        designatedEnemy = self->enemy;
                }
        }

        if (designatedEnemy && designatedEnemy->inUse && designatedEnemy->health > 0) {
                ent->enemy = designatedEnemy;
                FoundTarget(ent);
                if (ent->monsterInfo.attack)
                        ent->monsterInfo.attack(ent);
        }
}

static void tarbaby_mitosis(gentity_t* self) {
        Vector3 f, r, u;
        AngleVectors(self->s.angles, &f, &r, &u);

        const Vector3 hbabyMins{ -16.0f, -16.0f, -24.0f };
        const Vector3 hbabyMaxs{ 16.0f, 16.0f, 24.0f };

        for (int i = 0; i < 2; ++i) {
                Vector3 offset{ 32.0f, (i == 0 ? 32.0f : -32.0f), 0.0f };
                Vector3 startPoint = G_ProjectSource2(self->s.origin, offset, f, r, u);
                Vector3 spawnPoint;

                if (!FindSpawnPoint(startPoint, hbabyMins, hbabyMaxs, spawnPoint, 64.0f))
                        continue;

                gentity_t* ent = CreateGroundMonster(spawnPoint, self->s.angles, hbabyMins, hbabyMaxs, "monster_tarbaby", 256.0f);
                if (!ent)
                        continue;

                self->monsterInfo.monster_used++;
                ent->monsterInfo.commander = self;
                ent->monsterInfo.monster_slots = 1;

                ent->nextThink = level.time;
                ent->think(ent);

                const int babyHealth = static_cast<int>(90 * st.health_multiplier);
                ent->health = ent->maxHealth = babyHealth;
                ent->s.skinNum = 1;
                ent->s.scale = 0.7f;
                ent->gibHealth = 0;
                ent->spawnFlags |= SPAWNFLAG_HELLSPAWN_BABY;

                ent->monsterInfo.aiFlags |= AI_DO_NOT_COUNT | AI_IGNORE_SHOTS;

                tarbaby_assign_enemy(self, ent);
        }
}

PAIN(tarbaby_pain) (gentity_t* self, gentity_t* other, float kick, int damage, const MeansOfDeath& mod) -> void {
        (void)other;
        (void)kick;
        (void)damage;
        (void)mod;

        if (skill->integer < 1)
                return;

        if (level.time < self->pain_debounce_time)
                return;

        if (!self->className || strcmp(self->className, "monster_tarbaby_hell") != 0)
                return;

        if (self->spawnFlags.has(SPAWNFLAG_HELLSPAWN_BABY))
                return;

        if (frandom() * static_cast<float>(skill->integer) > 0.75f)
                tarbaby_mitosis(self);

        self->pain_debounce_time = level.time + 3_sec;
}

static void tarbaby_dead(gentity_t* self) {
        RadiusDamage(self, self, 120.0f, self, 150.0f, DamageFlags::Normal, ModID::Explosives);

        gi.WriteByte(svc_temp_entity);
        gi.WriteByte(TE_EXPLOSION1);
        gi.WritePosition(self->s.origin);
        gi.multicast(self->s.origin, MULTICAST_PHS, false);

        self->touch = nullptr;
        self->takeDamage = false;
        self->solid = SOLID_NOT;
        self->svFlags |= SVF_DEADMONSTER;

        ThrowGibs(self, 500, {
                { 1, "models/objects/gibs/sm_meat/tris.md2", GIB_HEAD }
        });

        self->think = FreeEntity;
        self->nextThink = level.time + 0.1_sec;
}

static MonsterFrame tarbaby_frames_explode[] = {
        { ai_move }
};
MMOVE_T(tarbaby_move_explode) = { FRAME_exp, FRAME_exp, tarbaby_frames_explode, tarbaby_dead };

DIE(tarbaby_die) (gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, const Vector3& point, const MeansOfDeath& mod) -> void {
        (void)inflictor;
        (void)attacker;
        (void)damage;
        (void)point;
        (void)mod;

        if (self->deadFlag)
                return;

        gi.sound(self, CHAN_VOICE, soundDeath, 1, ATTN_NORM, 0);
        self->deadFlag = true;
        self->takeDamage = false;
        self->monsterInfo.aiFlags &= ~AI_DUCKED;
        self->moveType = MoveType::Toss;
        self->touch = nullptr;

        M_SetAnimation(self, &tarbaby_move_explode);
}

MONSTERINFO_CHECKATTACK(tarbaby_checkattack) (gentity_t* self) -> bool {
        if (!self->enemy || self->enemy->health <= 0)
                return false;

        if (self->absMin[_Z] + 128.0f < self->enemy->absMin[_Z])
                return false;

        const Vector3 diff = self->enemy->s.origin - self->s.origin;
        const float   dist = diff.length();

        if (dist < 64.0f) {
                self->monsterInfo.attackState = MonsterAttackState::Missile;
                return true;
        }

        if (dist <= 320.0f && visible(self, self->enemy)) {
                self->monsterInfo.attackState = MonsterAttackState::Missile;
                return true;
        }

        return false;
}

void SP_monster_spawn(gentity_t* self) {
        if (!M_AllowSpawn(self)) {
                FreeEntity(self);
                return;
        }

        soundDeath.assign("tarbaby/death1.wav");
        soundHit.assign("tarbaby/hit1.wav");
        soundLand.assign("tarbaby/land1.wav");
        soundSight.assign("tarbaby/sight1.wav");

        self->moveType = MoveType::Step;
        self->solid = SOLID_BBOX;
        self->s.modelIndex = gi.modelIndex("models/monsters/tarbaby/tris.md2");
        self->mins = { -16.0f, -16.0f, -24.0f };
        self->maxs = { 16.0f, 16.0f, 24.0f };

        const bool isHellspawn = self->className && strcmp(self->className, "monster_tarbaby_hell") == 0;

        if (isHellspawn) {
                if (!self->spawnFlags.has(SPAWNFLAG_HELLSPAWN_BABY)) {
                        self->health = self->maxHealth = static_cast<int>(150 * st.health_multiplier);
                        self->s.skinNum = 2;
                } else {
                        self->health = self->maxHealth = static_cast<int>(90 * st.health_multiplier);
                        self->s.skinNum = 1;
                        if (self->s.scale == 0.0f)
                                self->s.scale = 0.8f;
                }
        } else {
                self->health = self->maxHealth = static_cast<int>(120 * st.health_multiplier);
                self->s.skinNum = 0;
        }

        self->gibHealth = 0;
        self->mass = 100;
        self->style = 0;
        self->count = 0;

        self->pain = tarbaby_pain;
        self->die = tarbaby_die;

        self->monsterInfo.stand = tarbaby_stand;
        self->monsterInfo.walk = tarbaby_walk;
        self->monsterInfo.run = tarbaby_run;
        self->monsterInfo.dodge = nullptr;
        self->monsterInfo.attack = tarbaby_jump;
        self->monsterInfo.melee = nullptr;
        self->monsterInfo.sight = tarbaby_sight;
        self->monsterInfo.search = nullptr;
        self->monsterInfo.checkAttack = tarbaby_checkattack;

        gi.linkEntity(self);

        M_SetAnimation(self, &tarbaby_move_stand);
        self->monsterInfo.scale = MODEL_SCALE;
        self->monsterInfo.combatStyle = CombatStyle::Melee;
        self->monsterInfo.canJump = true;

        walkmonster_start(self);
}
