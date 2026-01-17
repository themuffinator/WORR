/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

============================================================================== OVERLORD
==============================================================================*/

#include "../g_local.hpp"

#include <algorithm>

#include "m_overlord.hpp"
#include "m_wrath.hpp"
#include "q1_support.hpp"

static cached_soundIndex soundSight;
static cached_soundIndex soundAttack;
static cached_soundIndex soundMelee;
static cached_soundIndex soundDie;
static cached_soundIndex soundPain;
static cached_soundIndex soundAttackSecondary;

constexpr SpawnFlags SPAWNFLAG_OVERLORD_NO_TELEPORT = 8_spawnflag;

// sqrt(64*64*2) + sqrt(16*16*2) => 113.1
constexpr Vector3 kSpawnOffsets[] = {
        { 30.f, 128.f, 0.f },
        { 30.f, -128.f, 0.f }
};

constexpr Vector3 kWrathMins = { -16.f, -16.f, -24.f };
constexpr Vector3 kWrathMaxs = { 16.f, 16.f, 32.f };

constexpr float kBlindfireSpawnChance = 0.40f;

MONSTERINFO_SIGHT(overlord_sight) (gentity_t* self, gentity_t* other) -> void {
        gi.sound(self, CHAN_VOICE, soundSight, 1, ATTN_NORM, 0);
}

static void overlord_attack_sound(gentity_t* self) {
        gi.sound(self, CHAN_VOICE, soundAttack, 1, ATTN_NORM, 0);
}

// -----------------------------------------------------------------------------
// Movement
// -----------------------------------------------------------------------------

MonsterFrame overlord_frames_stand[] = {
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
MMOVE_T(overlord_move_stand) = { FRAME_s_wtwk01, FRAME_s_wtwk15, overlord_frames_stand, nullptr };

MONSTERINFO_STAND(overlord_stand) (gentity_t* self) -> void {
        M_SetAnimation(self, &overlord_move_stand);
}

MonsterFrame overlord_frames_walk[] = {
        { ai_walk, 5 },
        { ai_walk, 5 },
        { ai_walk, 5 },
        { ai_walk, 5 },
        { ai_walk, 5 },
        { ai_walk, 5 },
        { ai_walk, 5 },
        { ai_walk, 5 },
        { ai_walk, 5 },
        { ai_walk, 5 },

        { ai_walk, 5 },
        { ai_walk, 5 },
        { ai_walk, 5 },
        { ai_walk, 5 },
        { ai_walk, 5 }
};
MMOVE_T(overlord_move_walk) = { FRAME_s_wtwk01, FRAME_s_wtwk15, overlord_frames_walk, nullptr };

MONSTERINFO_WALK(overlord_walk) (gentity_t* self) -> void {
        M_SetAnimation(self, &overlord_move_walk);
}

MonsterFrame overlord_frames_run[] = {
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
MMOVE_T(overlord_move_run) = { FRAME_s_wtwk01, FRAME_s_wtwk15, overlord_frames_run, nullptr };

MONSTERINFO_RUN(overlord_run) (gentity_t* self) -> void {
        M_SetAnimation(self, &overlord_move_run);
}

// -----------------------------------------------------------------------------
// Teleport helpers
// -----------------------------------------------------------------------------

static void overlord_try_teleport(gentity_t* self) {
        if (self->spawnFlags.has(SPAWNFLAG_OVERLORD_NO_TELEPORT))
                return;

        float chance = 0.f;
        switch (skill->integer) {
        case 0:
        case 1:
                chance = frandom() * 2.f;
                break;
        case 2:
                chance = frandom() * 3.f;
                break;
        case 3:
                chance = frandom() * 4.f;
                break;
        default:
                chance = frandom() * 2.f;
                break;
        }

        if (chance <= 1.f)
                return;

        TryRandomTeleportPosition(self, 128.f);
}

static void overlord_surprise(gentity_t* self) {
        if (!self->enemy || !self->enemy->inUse || self->enemy->health <= 0)
                return;

        gentity_t* target = self->enemy;

        Vector3 targetAngles = target->s.angles;
        targetAngles[PITCH] = 0.f;

        Vector3 forward;
        AngleVectors(targetAngles, forward, nullptr, nullptr);

        constexpr float kBackDistance = 64.f;
        constexpr float kHeightOffset = 32.f;

        Vector3 behindPos = target->s.origin + (forward * -kBackDistance);
        behindPos[_Z] += kHeightOffset;

        trace_t tr = gi.trace(behindPos, self->mins, self->maxs, behindPos, self, MASK_MONSTERSOLID);
        if (tr.startSolid || tr.allSolid || tr.fraction < 1.f)
                return;

        gi.WriteByte(svc_temp_entity);
        gi.WriteByte(TE_TELEPORT_EFFECT);
        gi.WritePosition(self->s.origin);
        gi.multicast(self->s.origin, MULTICAST_PVS, false);

        self->s.origin = behindPos;
        self->s.oldOrigin = behindPos;
        gi.linkEntity(self);

        Vector3 dir = target->s.origin - self->s.origin;
        self->s.angles = VectorToAngles(dir);

        gi.WriteByte(svc_temp_entity);
        gi.WriteByte(TE_TELEPORT_EFFECT);
        gi.WritePosition(self->s.origin);
        gi.multicast(self->s.origin, MULTICAST_PVS, false);
}

static void overlord_spawn(gentity_t* self) {
        Vector3 forward;
        Vector3 right;
        Vector3 up;
        AngleVectors(self->s.angles, forward, right, up);

        for (const Vector3& offset : kSpawnOffsets) {
                const Vector3 startPoint = G_ProjectSource2(self->s.origin, offset, forward, right, up);

                Vector3 spawnPoint;
                if (!FindSpawnPoint(startPoint, kWrathMins, kWrathMaxs, spawnPoint, 64.f))
                        continue;

                gentity_t* minion = CreateFlyMonster(spawnPoint, self->s.angles, kWrathMins, kWrathMaxs, "monster_wrath");
                if (!minion)
                        continue;

                ++self->monsterInfo.monster_used;
                minion->monsterInfo.commander = self;
                minion->monsterInfo.monster_slots = 1;

                minion->nextThink = level.time;
                if (minion->think)
                        minion->think(minion);

                minion->monsterInfo.aiFlags |= AI_SPAWNED_OVERLORD | AI_DO_NOT_COUNT | AI_IGNORE_SHOTS;

                gentity_t* designatedEnemy = nullptr;
                if (!coop->integer) {
                        designatedEnemy = self->enemy;
                } else {
                        designatedEnemy = PickCoopTarget(minion);
                        if (designatedEnemy && designatedEnemy == self->enemy) {
                                designatedEnemy = PickCoopTarget(minion);
                        }

                        if (!designatedEnemy)
                                designatedEnemy = self->enemy;
                }

                if (designatedEnemy && designatedEnemy->inUse && designatedEnemy->health > 0) {
                        minion->enemy = designatedEnemy;
                        FoundTarget(minion);
                        minion->monsterInfo.attack(minion);
                }
        }
}

static void overlord_ready_spawn(gentity_t* self) {
        Vector3 forward;
        Vector3 right;
        Vector3 up;
        AngleVectors(self->s.angles, forward, right, up);

        for (const Vector3& offset : kSpawnOffsets) {
                const Vector3 startPoint = G_ProjectSource2(self->s.origin, offset, forward, right, up);

                Vector3 spawnPoint;
                if (!FindSpawnPoint(startPoint, kWrathMins, kWrathMaxs, spawnPoint, 0.f, false))
                        continue;

                const float radius = (kWrathMaxs - kWrathMins).length() * 0.5f;
                SpawnGrow_Spawn(spawnPoint + (kWrathMins + kWrathMaxs), radius, radius * 2.f);
        }
}

static void overlord_calculate_slots(gentity_t* self) {
        switch (skill->integer) {
        case 0:
        case 1:
                self->monsterInfo.monster_slots = 2;
                break;
        case 2:
                self->monsterInfo.monster_slots = 3;
                break;
        case 3:
                self->monsterInfo.monster_slots = 4;
                break;
        default:
                self->monsterInfo.monster_slots = 2;
                break;
        }

        if (coop->integer) {
                const int32_t slots = self->monsterInfo.monster_slots + (skill->integer * (CountPlayers() - 1));
                self->monsterInfo.monster_slots = std::min<int32_t>(6, slots);
        }
}

static void overlord_fire(gentity_t* self) {
        if (!self->enemy || !self->enemy->inUse)
                return;

        constexpr int damage = 20;
        constexpr int rocketSpeed = 400;

        const bool blindfire = (self->monsterInfo.aiFlags & AI_MANUAL_STEERING) != 0;

        Vector3 forward;
        Vector3 right;
        AngleVectors(self->s.angles, forward, right, nullptr);

        const Vector3 muzzleOffset = { 0.f, 0.f, 10.f };
        const Vector3 start = M_ProjectFlashSource(self, muzzleOffset, forward, right);

        Vector3 target = blindfire ? self->monsterInfo.blind_fire_target : self->enemy->s.origin;
        Vector3 aimPoint = target;
        Vector3 dir;

        if (blindfire) {
                dir = aimPoint - start;
        } else if (frandom() < 0.33f || start[_Z] < self->enemy->absMin[_Z]) {
                aimPoint[_Z] += self->enemy->viewHeight;
                dir = aimPoint - start;
        } else {
                aimPoint[_Z] = self->enemy->absMin[_Z] + 1.f;
                dir = aimPoint - start;
        }

        if (!blindfire && frandom() < 0.35f)
                PredictAim(self, self->enemy, start, static_cast<float>(rocketSpeed), false, 0.f, &dir, &aimPoint);

        dir.normalize();

        const trace_t trace = gi.traceLine(start, aimPoint, self, MASK_PROJECTILE);

        if (blindfire) {
                auto tryFire = [&](const Vector3& fireTarget) {
                        Vector3 localDir = fireTarget - start;
                        localDir.normalize();

                        trace_t tr = gi.traceLine(start, fireTarget, self, MASK_PROJECTILE);
                        if (tr.startSolid || tr.allSolid || tr.fraction < 0.5f)
                                return false;

                        fire_vorepod(self, start, localDir, damage, rocketSpeed, static_cast<float>(damage), damage, 0.075f, 1);
                        return true;
                };

                if (!tryFire(aimPoint)) {
                        if (!tryFire(target + (right * -10.f)))
                                tryFire(target + (right * 10.f));
                }
        } else {
                if (trace.fraction > 0.5f || !trace.ent || trace.ent->solid != SOLID_BSP)
                        fire_vorepod(self, start, dir, damage, rocketSpeed, static_cast<float>(damage), damage, 0.015f, 1);
        }

        gi.sound(self, CHAN_WEAPON | CHAN_RELIABLE, soundAttackSecondary, 1, ATTN_NORM, 0);
}

static void overlord_hit_left(gentity_t* self) {
        Vector3 aim = { MELEE_DISTANCE, self->mins[0], 8.f };
        if (fire_hit(self, aim, irandom(20, 30), 400)) {
                gi.sound(self, CHAN_WEAPON, soundMelee, 1, ATTN_NORM, 0);
        } else {
                gi.sound(self, CHAN_WEAPON, soundMelee, 1, ATTN_NORM, 0);
                self->monsterInfo.melee_debounce_time = level.time + 1.5_sec;
        }
}

static void overlord_hit_right(gentity_t* self) {
        Vector3 aim = { MELEE_DISTANCE, self->maxs[0], 8.f };
        if (fire_hit(self, aim, irandom(20, 30), 400)) {
                gi.sound(self, CHAN_WEAPON, soundMelee, 1, ATTN_NORM, 0);
        } else {
                gi.sound(self, CHAN_WEAPON, soundMelee, 1, ATTN_NORM, 0);
                self->monsterInfo.melee_debounce_time = level.time + 1.5_sec;
        }
}

static void overlord_hit_right_hard(gentity_t* self) {
        Vector3 aim = { MELEE_DISTANCE, self->maxs[0], 8.f };
        if (fire_hit(self, aim, irandom(40, 60), 400)) {
                gi.sound(self, CHAN_WEAPON, soundMelee, 1, ATTN_NORM, 0);
        } else {
                gi.sound(self, CHAN_WEAPON, soundMelee, 1, ATTN_NORM, 0);
                self->monsterInfo.melee_debounce_time = level.time + 1.5_sec;
        }
}

MonsterFrame overlord_frames_melee1[] = {
        { ai_charge },
        { ai_charge },
        { ai_charge, 0, overlord_attack_sound },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge, 0, overlord_hit_left },
        { ai_charge },
        { ai_charge },
        { ai_charge, 0, overlord_try_teleport }
};
MMOVE_T(overlord_move_melee1) = { FRAME_s_wtaa01, FRAME_s_wtaa10, overlord_frames_melee1, overlord_run };

MonsterFrame overlord_frames_melee2[] = {
        { ai_charge },
        { ai_charge },
        { ai_charge, 0, overlord_attack_sound },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge, 0, overlord_hit_right },
        { ai_charge },
        { ai_charge },

        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge, 0, overlord_try_teleport }
};
MMOVE_T(overlord_move_melee2) = { FRAME_s_wtab01, FRAME_s_wtab14, overlord_frames_melee2, overlord_run };

MonsterFrame overlord_frames_melee3[] = {
        { ai_charge, 0, overlord_surprise },
        { ai_charge },
        { ai_charge, 0, overlord_attack_sound },
        { ai_charge },
        { ai_charge },
        { ai_charge, 0, overlord_hit_right_hard },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },

        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge, 0, overlord_try_teleport }
};
MMOVE_T(overlord_move_melee3) = { FRAME_s_wtac01, FRAME_s_wtac14, overlord_frames_melee3, overlord_run };

MONSTERINFO_MELEE(overlord_melee) (gentity_t* self) -> void {
        const float roll = frandom();
        if (roll <= 0.20f && skill->integer >= 3) {
                M_SetAnimation(self, &overlord_move_melee3);
        } else if (roll >= 0.60f) {
                M_SetAnimation(self, &overlord_move_melee2);
        } else {
                M_SetAnimation(self, &overlord_move_melee1);
        }
}

MonsterFrame overlord_frames_missile[] = {
        { ai_charge },
        { ai_charge },
        { ai_charge, 0, overlord_attack_sound },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge, -1 },
        { ai_charge, -2 },
        { ai_charge, -3 },
        { ai_charge, -2, overlord_fire },

        { ai_charge, -1 },
        { ai_charge, 0, overlord_try_teleport }
};
MMOVE_T(overlord_move_missile) = { FRAME_s_wtba01, FRAME_s_wtba12, overlord_frames_missile, overlord_run };

MonsterFrame overlord_frames_spawn[] = {
        { ai_charge },
        { ai_charge },
        { ai_charge, 0, overlord_attack_sound },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge, -1, overlord_ready_spawn },
        { ai_charge, -2 },
        { ai_charge, -3 },
        { ai_charge, -2, overlord_spawn },

        { ai_charge, -1 },
        { ai_charge, 0, overlord_try_teleport }
};
MMOVE_T(overlord_move_spawn) = { FRAME_s_wtba01, FRAME_s_wtba12, overlord_frames_spawn, overlord_run };

MONSTERINFO_ATTACK(overlord_attack) (gentity_t* self) -> void {
        const Vector3 offset = { 0.f, 0.f, 10.f };

        bool blocked = false;
        if (self->monsterInfo.aiFlags & AI_BLOCKED) {
                blocked = true;
                self->monsterInfo.aiFlags &= ~AI_BLOCKED;
        }

        if (!M_CheckClearShot(self, offset))
                return;

        overlord_calculate_slots(self);

        if ((self->monsterInfo.attackState == MonsterAttackState::Blind || blocked) && (M_SlotsLeft(self) >= 2)) {
                M_SetAnimation(self, &overlord_move_spawn);
                return;
        }

        if (self->monsterInfo.attackState == MonsterAttackState::Blind) {
                float chance;
                if (self->monsterInfo.blind_fire_delay < 1_sec) {
                        chance = 1.f;
                } else if (self->monsterInfo.blind_fire_delay < 7.5_sec) {
                        chance = 0.4f;
                } else {
                        chance = 0.1f;
                }

                const float roll = frandom();
                self->monsterInfo.blind_fire_delay += random_time(5.5_sec, 6.5_sec);

                if (!self->monsterInfo.blind_fire_target || roll > chance)
                        return;

                self->monsterInfo.aiFlags |= AI_MANUAL_STEERING;

                if (M_SlotsLeft(self) >= 2) {
                        const float spawnRoll = frandom();
                        if (spawnRoll <= kBlindfireSpawnChance) {
                                M_SetAnimation(self, &overlord_move_missile);
                        } else {
                                M_SetAnimation(self, &overlord_move_spawn);
                        }
                } else {
                        M_SetAnimation(self, &overlord_move_missile);
                }

                self->monsterInfo.attackFinished = level.time + random_time(4_sec);
                return;
        }

        const float roll = frandom();
        if (M_SlotsLeft(self) >= 2) {
                if (roll <= 0.20f && skill->integer >= 3) {
                        M_SetAnimation(self, &overlord_move_melee3);
                } else if (roll <= 0.60f) {
                        M_SetAnimation(self, &overlord_move_missile);
                } else {
                        M_SetAnimation(self, &overlord_move_spawn);
                }
        } else {
                if (roll <= 0.20f && skill->integer >= 3) {
                        M_SetAnimation(self, &overlord_move_melee3);
                } else {
                        M_SetAnimation(self, &overlord_move_missile);
                }
        }
}

// -----------------------------------------------------------------------------
// Pain / death
// -----------------------------------------------------------------------------

MonsterFrame overlord_frames_pain1[] = {
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
                { ai_move }
};
MMOVE_T(overlord_move_pain1) = { FRAME_s_wtpa01, FRAME_s_wtpa14, overlord_frames_pain1, overlord_run };

MonsterFrame overlord_frames_pain2[] = {
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
MMOVE_T(overlord_move_pain2) = { FRAME_s_wtpb01, FRAME_s_wtpb11, overlord_frames_pain2, overlord_run };

PAIN(overlord_pain) (gentity_t* self, gentity_t* other, float kick, int damage, const MeansOfDeath& mod) -> void {
        if (level.time < self->pain_debounce_time)
                return;

        self->pain_debounce_time = level.time + 2_sec;

        if (!M_ShouldReactToPain(self, mod))
                return;

        gi.sound(self, CHAN_VOICE, soundPain, 1, ATTN_NORM, 0);

        if (frandom() >= 0.4f)
                M_SetAnimation(self, &overlord_move_pain1);
        else
                M_SetAnimation(self, &overlord_move_pain2);
}

static void overlord_dead(gentity_t* self) {
        RadiusDamage(self, self, 60, nullptr, 105, DamageFlags::Normal, MeansOfDeath{ ModID::Barrel });

        gi.WriteByte(svc_temp_entity);
        gi.WriteByte(TE_EXPLOSION1);
        gi.WritePosition(self->s.origin);
        gi.multicast(self->s.origin, MULTICAST_PHS, false);

        self->s.skinNum /= 2;

        ThrowGibs(self, 55, {
                { 2, "models/objects/gibs/bone/tris.md2" },
                { 2, "models/monsters/overlord/gibs/claw.md2" },
                { 2, "models/monsters/overlord/gibs/arm.md2" },
                { "models/monsters/overlord/gibs/ribs.md2" },
                { "models/monsters/overlord/gibs/bone.md2", GIB_HEAD }
        });

        self->touch = nullptr;
}

MonsterFrame overlord_frames_die[] = {
        { ai_move, 0, Q1BossExplode },
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
        { ai_move },
        { ai_move }
};
MMOVE_T(overlord_move_die) = { FRAME_s_wtdt01, FRAME_s_wtdt17, overlord_frames_die, overlord_dead };

static void overlord_kill_wraths(gentity_t* self) {
        Vector3 damageOrigin = self->s.origin;
        if (self->enemy && self->enemy->inUse)
                damageOrigin = self->enemy->s.origin;

        for (gentity_t* ent = nullptr; (ent = G_FindByString<&gentity_t::className>(ent, "monster_wrath"));) {
                if (ent->inUse && ent->health > 0) {
                        Damage(ent, self, self, vec3_origin, damageOrigin, vec3_origin,
                                ent->health + 1, 0, DamageFlags::NoKnockback, MeansOfDeath{ ModID::Unknown });
                }
        }
}

DIE(overlord_die) (gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, const Vector3& point, const MeansOfDeath& mod) -> void {
        if (self->deadFlag)
                return;

        gi.sound(self, CHAN_VOICE, soundDie, 1, ATTN_NORM, 0);
        self->deadFlag = true;
        self->takeDamage = true;
        overlord_kill_wraths(self);

        M_SetAnimation(self, &overlord_move_die);
}

MONSTERINFO_CHECKATTACK(overlord_checkattack) (gentity_t* self) -> bool {
        if (!self->enemy)
                return false;

        return M_CheckAttack_Base(self, 0.4f, 0.8f, 0.8f, 0.8f, 0.f, 0.f);
}

static void overlord_set_fly_parameters(gentity_t* self) {
        self->monsterInfo.fly_thrusters = false;
        self->monsterInfo.fly_acceleration = 20.f;
        self->monsterInfo.fly_speed = 120.f;
        self->monsterInfo.fly_min_distance = 200.f;
        self->monsterInfo.fly_max_distance = 400.f;
}

/*QUAKED monster_overlord (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight
model="models/monsters/overlord/tris.md2"
*/
void SP_monster_overlord(gentity_t* self) {
        if (!M_AllowSpawn(self)) {
                FreeEntity(self);
                return;
        }

        soundMelee.assign("overlord/smash.wav");
        soundSight.assign("wrath/wsee.wav");
        soundAttack.assign("wrath/watt.wav");
        soundDie.assign("wrath/wdthc.wav");
        soundPain.assign("wrath/wpain.wav");
        soundAttackSecondary.assign("vore/attack2.wav");

        self->moveType = MoveType::Step;
        self->solid = SOLID_BBOX;

        self->s.modelIndex = gi.modelIndex("models/monsters/overlord/tris.md2");
        self->mins = { -16.f, -16.f, -24.f };
        self->maxs = { 16.f, 16.f, 32.f };

        self->health = max(3000, 3000 + 1250 * (skill->integer - 1)) * st.health_multiplier;
        if (!st.was_key_specified("armor_type"))
                self->monsterInfo.armorType = IT_ARMOR_BODY;
        if (!st.was_key_specified("armor_power"))
                self->monsterInfo.armor_power = max(500, 500 + 150 * (skill->integer - 1));
        self->mass = 750;
        if (coop->integer) {
                self->health += (500 * skill->integer) + (500 * (CountPlayers() - 1));
                self->monsterInfo.armor_power += (250 * skill->integer) + (250 * (CountPlayers() - 1));
        }
        self->pain = overlord_pain;
        self->die = overlord_die;

        self->monsterInfo.stand = overlord_stand;
        self->monsterInfo.walk = overlord_walk;
        self->monsterInfo.run = overlord_run;
        self->monsterInfo.attack = overlord_attack;
        self->monsterInfo.melee = overlord_melee;
        self->monsterInfo.sight = overlord_sight;
        self->monsterInfo.search = nullptr;
        self->monsterInfo.checkAttack = overlord_checkattack;

        gi.linkEntity(self);

        M_SetAnimation(self, &overlord_move_stand);
        self->monsterInfo.scale = OVERLORD_MODEL_SCALE;

        self->flags |= FL_FLY;
        if (!self->yawSpeed)
                self->yawSpeed = 10;
        self->viewHeight = 10;

        flymonster_start(self);

        self->monsterInfo.aiFlags |= AI_ALTERNATE_FLY;

        overlord_set_fly_parameters(self);
}
