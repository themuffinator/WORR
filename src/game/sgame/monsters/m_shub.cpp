/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

============================================================================== OLD ONE
(Shub-Niggurath) - Stationary commander-style boss that periodically spawns reinforcements. -
Normally invulnerable; special triggers briefly expose a vulnerability window or the player can
telefrag the boss via classic Quake exits. - Ported from Ionized Quake's m_oldone implementation
and adapted for the WORR reinforcement & vulnerability systems.
==============================================================================*/

#include "../g_local.hpp"
#include "m_shub.hpp"
#include "m_flash.hpp"

constexpr Vector3 OLDONE_MINS = { -160.0f, -128.0f, -24.0f };
constexpr Vector3 OLDONE_MAXS = { 160.0f, 128.0f, 256.0f };
constexpr int32_t OLDONE_HEALTH = 40000;
constexpr int32_t OLDONE_GIB_HEALTH = -50;
constexpr int32_t OLDONE_MASS = 3000;
constexpr float OLDONE_IDLE_VOX_COOLDOWN = 5.0f;
constexpr const char* OLDONE_DEFAULT_REINFORCEMENTS =
        "monster_scrag 1;monster_fiend 3;monster_vore 5;monster_shambler 6";
constexpr int32_t OLDONE_DEFAULT_MONSTER_SLOTS = 6;

static cached_soundIndex sound_idle;
static cached_soundIndex sound_sight;
static cached_soundIndex sound_death;
static cached_soundIndex sound_pop;
static cached_soundIndex sound_spawn;
static cached_soundIndex sound_pain;

bool infront(gentity_t* self, gentity_t* other);
bool inback(gentity_t* self, gentity_t* other);
bool below(gentity_t* self, gentity_t* other);
void FoundTarget(gentity_t* self);
std::array<uint8_t, MAX_REINFORCEMENTS> M_PickReinforcements(gentity_t* self, int32_t& num_chosen, int32_t max_slots);
void M_SetupReinforcements(const char* reinforcements, reinforcement_list_t& list);

static void oldone_run(gentity_t* self);
static void oldone_idle_think(gentity_t* self);
static void oldone_spawn(gentity_t* self);
static void oldone_prep_spawn(gentity_t* self);
static void oldone_spawn_check(gentity_t* self);
static void oldone_ready_spawn(gentity_t* self);
static void oldone_start_spawn(gentity_t* self);
static void oldone_attack(gentity_t* self);

// -----------------------------------------------------------------------------
// Idle & sight
// -----------------------------------------------------------------------------

MONSTERINFO_IDLE(oldone_idle) (gentity_t* self) -> void {
        gi.sound(self, CHAN_VOICE, sound_idle, 1, ATTN_NORM, 0);
}

MONSTERINFO_SIGHT(oldone_sight) (gentity_t* self, gentity_t* other) -> void {
        (void)other;
        gi.sound(self, CHAN_VOICE, sound_sight, 1, ATTN_NORM, 0);
}

static void oldone_idle_think(gentity_t* self) {
        if ((self->monsterInfo.aiFlags & AI_OLDONE_VULNERABLE) && level.time >= self->teleportTime) {
                self->monsterInfo.aiFlags &= ~AI_OLDONE_VULNERABLE;
        }

        if (self->fly_sound_debounce_time <= level.time) {
                self->fly_sound_debounce_time = level.time + GameTime::from_sec(OLDONE_IDLE_VOX_COOLDOWN);
                gi.sound(self, CHAN_VOICE, sound_idle, 1, ATTN_IDLE, 0);
        }
}

// -----------------------------------------------------------------------------
// Stand/Walk/Run frames (loop through 46-frame idle cycle)
// -----------------------------------------------------------------------------

static MonsterFrame oldone_frames_stand[] = {
        { ai_stand, 0, oldone_idle_think },
        { ai_stand }, { ai_stand }, { ai_stand }, { ai_stand },
        { ai_stand }, { ai_stand }, { ai_stand }, { ai_stand }, { ai_stand },
        { ai_stand }, { ai_stand }, { ai_stand }, { ai_stand }, { ai_stand },
        { ai_stand }, { ai_stand }, { ai_stand }, { ai_stand }, { ai_stand },
        { ai_stand }, { ai_stand }, { ai_stand }, { ai_stand }, { ai_stand },
        { ai_stand }, { ai_stand }, { ai_stand }, { ai_stand }, { ai_stand },
        { ai_stand }, { ai_stand }, { ai_stand }, { ai_stand }, { ai_stand },
        { ai_stand }, { ai_stand }, { ai_stand }, { ai_stand }, { ai_stand },
        { ai_stand }, { ai_stand }, { ai_stand }, { ai_stand }, { ai_stand },
        { ai_stand }
};
MMOVE_T(oldone_move_stand) = { FRAME_old1, FRAME_old46, oldone_frames_stand, nullptr };

static MonsterFrame oldone_frames_walk[] = {
        { ai_walk, 0, oldone_idle_think },
        { ai_walk }, { ai_walk }, { ai_walk }, { ai_walk },
        { ai_walk }, { ai_walk }, { ai_walk }, { ai_walk }, { ai_walk },
        { ai_walk }, { ai_walk }, { ai_walk }, { ai_walk }, { ai_walk },
        { ai_walk }, { ai_walk }, { ai_walk }, { ai_walk }, { ai_walk },
        { ai_walk }, { ai_walk }, { ai_walk }, { ai_walk }, { ai_walk },
        { ai_walk }, { ai_walk }, { ai_walk }, { ai_walk }, { ai_walk },
        { ai_walk }, { ai_walk }, { ai_walk }, { ai_walk }, { ai_walk },
        { ai_walk }, { ai_walk }, { ai_walk }, { ai_walk }, { ai_walk },
        { ai_walk }, { ai_walk }, { ai_walk }, { ai_walk }, { ai_walk },
        { ai_walk }
};
MMOVE_T(oldone_move_walk) = { FRAME_old1, FRAME_old46, oldone_frames_walk, nullptr };

static MonsterFrame oldone_frames_run[] = {
        { ai_run, 0, oldone_idle_think },
        { ai_run }, { ai_run }, { ai_run }, { ai_run },
        { ai_run }, { ai_run }, { ai_run }, { ai_run }, { ai_run },
        { ai_run }, { ai_run }, { ai_run }, { ai_run }, { ai_run },
        { ai_run }, { ai_run }, { ai_run }, { ai_run }, { ai_run },
        { ai_run }, { ai_run }, { ai_run }, { ai_run }, { ai_run },
        { ai_run }, { ai_run }, { ai_run }, { ai_run }, { ai_run },
        { ai_run }, { ai_run }, { ai_run }, { ai_run }, { ai_run },
        { ai_run }, { ai_run }, { ai_run }, { ai_run }, { ai_run },
        { ai_run }, { ai_run }, { ai_run }, { ai_run }, { ai_run },
        { ai_run }
};
MMOVE_T(oldone_move_run) = { FRAME_old1, FRAME_old46, oldone_frames_run, oldone_run };

MONSTERINFO_STAND(oldone_stand) (gentity_t* self) -> void {
        self->monsterInfo.aiFlags |= AI_STAND_GROUND;
        M_SetAnimation(self, &oldone_move_stand);
}

MONSTERINFO_WALK(oldone_walk) (gentity_t* self) -> void {
        M_SetAnimation(self, &oldone_move_walk);
}

MONSTERINFO_RUN(oldone_run) (gentity_t* self) -> void {
        M_SetAnimation(self, &oldone_move_run);
}

// -----------------------------------------------------------------------------
// Reinforcement helpers
// -----------------------------------------------------------------------------

static void oldone_coop_check(gentity_t* self) {
        if (self->monsterInfo.fireWait > level.time)
                return;

        std::array<gentity_t*, MAX_SPLIT_PLAYERS> targets{};
        uint32_t num_targets = 0;

        for (auto client : active_clients()) {
                if (!client->inUse || !client->client)
                        continue;
                if (!(inback(self, client) || below(self, client)))
                        continue;

                trace_t tr = gi.traceLine(self->s.origin, client->s.origin, self, MASK_SOLID);
                if (tr.fraction == 1.0f && num_targets < targets.size())
                        targets[num_targets++] = client;
        }

        if (!num_targets)
                return;

        // Select a random target to bias yaw updates while spawning
        self->monsterInfo.fireWait = level.time + 0.5_sec;
        gentity_t* original = self->enemy;
        self->enemy = targets[irandom(num_targets)];
        self->enemy = original;
}

static void oldone_spawn(gentity_t* self) {
        if (self->monsterInfo.chosen_reinforcements[0] == 255)
                return;

        Vector3 forward, right;
        Vector3 offset = { 225.0f, 0.0f, -58.0f };
        AngleVectors(self->s.angles, forward, right, nullptr);
        Vector3 startpoint = M_ProjectFlashSource(self, offset, forward, right);

        auto& reinforcement =
                self->monsterInfo.reinforcements.reinforcements[self->monsterInfo.chosen_reinforcements[0]];

        Vector3 spawnpoint;
        if (!FindSpawnPoint(startpoint, reinforcement.mins, reinforcement.maxs, spawnpoint, 32, false))
                return;

        gentity_t* ent = CreateFlyMonster(spawnpoint, self->s.angles, reinforcement.mins, reinforcement.maxs,
                reinforcement.className);
        if (!ent)
                return;

        gi.sound(self, CHAN_BODY, sound_spawn, 1, ATTN_NONE, 0);

        if (ent->think) {
                ent->nextThink = level.time;
                ent->think(ent);
        }

        ent->monsterInfo.aiFlags |= AI_SPAWNED_OLDONE | AI_DO_NOT_COUNT | AI_IGNORE_SHOTS;
        ent->monsterInfo.commander = self;
        ent->monsterInfo.monster_slots = reinforcement.strength;
        self->monsterInfo.monster_used += reinforcement.strength;

        if (self->enemy && self->enemy->inUse && self->enemy->health > 0) {
                ent->enemy = self->enemy;
                FoundTarget(ent);
        }
}

static void oldone_prep_spawn(gentity_t* self) {
        oldone_coop_check(self);
        self->monsterInfo.aiFlags |= AI_MANUAL_STEERING;
        self->timeStamp = level.time;
        self->yawSpeed = 10.0f;
}

static void oldone_spawn_check(gentity_t* self) {
        oldone_coop_check(self);
        oldone_spawn(self);

        if (level.time > self->timeStamp + 2.0_sec) {
                self->monsterInfo.aiFlags &= ~AI_MANUAL_STEERING;
        } else {
                self->monsterInfo.nextFrame = FRAME_old8;
        }
}

static void oldone_ready_spawn(gentity_t* self) {
        oldone_coop_check(self);

        float current_yaw = anglemod(self->s.angles[YAW]);
        if (std::fabs(current_yaw - self->ideal_yaw) > 0.1f) {
                self->monsterInfo.aiFlags |= AI_HOLD_FRAME;
                self->timeStamp += FRAME_TIME_S;
                return;
        }

        self->monsterInfo.aiFlags &= ~AI_HOLD_FRAME;

        int32_t num_summoned;
        self->monsterInfo.chosen_reinforcements = M_PickReinforcements(self, num_summoned, 1);
        if (!num_summoned)
                return;

        auto& reinforcement =
                self->monsterInfo.reinforcements.reinforcements[self->monsterInfo.chosen_reinforcements[0]];

        Vector3 forward, right;
        Vector3 offset = { 105.0f, 0.0f, -58.0f };
        AngleVectors(self->s.angles, forward, right, nullptr);
        Vector3 startpoint = M_ProjectFlashSource(self, offset, forward, right);

        Vector3 spawnpoint;
        if (FindSpawnPoint(startpoint, reinforcement.mins, reinforcement.maxs, spawnpoint, 32, false)) {
                float radius = (reinforcement.maxs - reinforcement.mins).length() * 0.5f;
                SpawnGrow_Spawn(spawnpoint + (reinforcement.mins + reinforcement.maxs), radius, radius * 2.0f);
        }
}

static void oldone_start_spawn(gentity_t* self) {
        oldone_coop_check(self);

        if (!self->enemy)
                return;

        int mytime = static_cast<int>(((level.time - self->timeStamp) / 0.5f).seconds());
        Vector3 temp = self->enemy->s.origin - self->s.origin;
        float enemy_yaw = vectoyaw(temp);

        if (mytime == 0)
                self->ideal_yaw = anglemod(enemy_yaw - 30.0f);
        else if (mytime == 1)
                self->ideal_yaw = anglemod(enemy_yaw);
        else if (mytime == 2)
                self->ideal_yaw = anglemod(enemy_yaw + 30.0f);
}

static MonsterFrame oldone_frames_spawn[] = {
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge, 0, oldone_prep_spawn },
        { ai_charge, 0, oldone_start_spawn },
        { ai_charge, 0, oldone_ready_spawn },
        { ai_charge },
        { ai_charge },
        { ai_charge, 0, oldone_spawn_check },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge }
};
MMOVE_T(oldone_move_spawn) = { FRAME_old1, FRAME_old18, oldone_frames_spawn, nullptr };

// -----------------------------------------------------------------------------
// Attack logic
// -----------------------------------------------------------------------------

static void oldone_attack(gentity_t* self) {
        monster_done_dodge(self);
        self->monsterInfo.aiFlags &= ~AI_HOLD_FRAME;

        if (!self->enemy || !self->enemy->inUse)
                return;

        bool enemy_inback = inback(self, self->enemy);
        bool enemy_infront = infront(self, self->enemy);
        bool enemy_below = below(self, self->enemy);

        if (self->bad_area)
                return;

        if (self->monsterInfo.attackState == MonsterAttackState::Blind) {
            M_SetAnimation(self, &oldone_move_spawn);
            return;
        }

        if (!enemy_inback && !enemy_infront && !enemy_below)
                return;

        if (enemy_infront && M_SlotsLeft(self) > 2) {
                if (frandom() <= 0.20f)
                        M_SetAnimation(self, &oldone_move_spawn);
        }
}

// -----------------------------------------------------------------------------
// Pain & death
// -----------------------------------------------------------------------------

static MonsterFrame oldone_frames_pain[] = {
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
MMOVE_T(oldone_move_pain) = { FRAME_shake1, FRAME_shake10, oldone_frames_pain, oldone_run };

static PAIN(oldone_pain) (gentity_t* self, gentity_t* other, float kick, int damage, const MeansOfDeath& mod) -> void {
        (void)other;
        (void)kick;

        const bool vulnerable = !!(self->monsterInfo.aiFlags & AI_OLDONE_VULNERABLE);
        if (!vulnerable) {
                self->health += damage;

                if (level.time < self->pain_debounce_time)
                        return;

                self->pain_debounce_time = level.time + 5_sec;
                if (!M_ShouldReactToPain(self, mod))
                        return;

                if (sound_pain)
                        gi.sound(self, CHAN_VOICE, sound_pain, 1, ATTN_NORM, 0);
                M_SetAnimation(self, &oldone_move_pain);
                return;
        }

        if (!M_ShouldReactToPain(self, mod))
                return;
        if (level.time < self->pain_debounce_time)
                return;

        self->pain_debounce_time = level.time + 1_sec;
        if (sound_pain)
                gi.sound(self, CHAN_VOICE, sound_pain, 1, ATTN_NORM, 0);
        M_SetAnimation(self, &oldone_move_pain);
}

static void oldone_dead(gentity_t* self) {
        self->mins = { -16.0f, -16.0f, -24.0f };
        self->maxs = { 16.0f, 16.0f, -8.0f };
        monster_dead(self);
}

static MonsterFrame oldone_frames_death[] = {
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
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move }
};
MMOVE_T(oldone_move_death) = { FRAME_shake1, FRAME_shake20, oldone_frames_death, oldone_dead };

static DIE(oldone_die)
(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, const Vector3& point, const MeansOfDeath& mod) -> void {
        (void)inflictor;
        (void)attacker;
        (void)damage;
        (void)point;

        const bool telefrag = (mod.id == ModID::Telefragged || mod.id == ModID::Telefrag_Spawn);
        const bool vulnerable = !!(self->monsterInfo.aiFlags & AI_OLDONE_VULNERABLE);

        if (!telefrag && !vulnerable) {
                self->health = std::max(self->health, 1);
                return;
        }

        if (M_CheckGib(self, mod)) {
                gi.sound(self, CHAN_VOICE, gi.soundIndex("misc/udeath.wav"), 1, ATTN_NORM, 0);

                self->s.skinNum /= 2;

                ThrowGibs(self, damage, {
                                { 2, "models/objects/gibs/bone/tris.md2" },
                                { 3, "models/objects/gibs/sm_meat/tris.md2" },
                                { "models/objects/gibs/sm_meat/tris.md2", GIB_SKINNED | GIB_HEAD }
                        });

                self->deadFlag = true;
                return;
        }

        if (self->deadFlag)
                return;

        gi.sound(self, CHAN_VOICE, telefrag ? sound_pop : sound_death, 1, ATTN_NORM, 0);
        self->deadFlag = true;
        self->takeDamage = true;

        M_SetAnimation(self, &oldone_move_death);
}

// -----------------------------------------------------------------------------
// Spawn / precache
// -----------------------------------------------------------------------------

static void oldone_precache() {
        gi.modelIndex("models/monsters/oldone/tris.md2");
        sound_idle.assign("oldone/idle.wav");
        sound_sight.assign("oldone/sight.wav");
        sound_death.assign("oldone/death.wav");
        sound_pop.assign("oldone/pop2.wav");
        sound_spawn.assign("oldone/spawn.wav");
        sound_pain.assign("oldone/pain.wav");
}

static void oldone_configure(gentity_t* self, const spawn_temp_t& st) {
        self->monsterInfo.stand = oldone_stand;
        self->monsterInfo.walk = oldone_walk;
        self->monsterInfo.run = oldone_run;
        self->monsterInfo.attack = oldone_attack;
        self->monsterInfo.melee = nullptr;
        self->monsterInfo.dodge = nullptr;
        self->monsterInfo.sight = oldone_sight;
        self->monsterInfo.idle = oldone_idle;
        self->monsterInfo.setSkin = nullptr;

        self->pain = oldone_pain;
        self->die = oldone_die;

        self->mins = OLDONE_MINS;
        self->maxs = OLDONE_MAXS;
        self->yawSpeed = 10.0f;
        self->mass = OLDONE_MASS;

        self->health = self->maxHealth = OLDONE_HEALTH * st.health_multiplier;
        self->gibHealth = OLDONE_GIB_HEALTH;
        self->monsterInfo.base_health = self->health;

        if (!st.was_key_specified("armor_type"))
                self->monsterInfo.armorType = IT_ARMOR_BODY;
        if (!st.was_key_specified("armor_power"))
                self->monsterInfo.armor_power = 1000;

        self->svFlags |= SVF_MONSTER;
        self->moveType = MoveType::None;
        self->solid = SOLID_BBOX;
        self->takeDamage = true;
        self->flags |= FL_NO_KNOCKBACK | FL_STATIONARY;

        self->monsterInfo.scale = MODEL_SCALE;
        self->monsterInfo.monster_used = 0;
        self->monsterInfo.fireWait = 0_ms;
        self->monsterInfo.chosen_reinforcements.fill(255);

        self->monsterInfo.aiFlags |= AI_STAND_GROUND;

        const char* reinforcements = OLDONE_DEFAULT_REINFORCEMENTS;
        if (!st.was_key_specified("monster_slots"))
                self->monsterInfo.monster_slots = OLDONE_DEFAULT_MONSTER_SLOTS;
        if (st.was_key_specified("reinforcements"))
                reinforcements = st.reinforcements;

        if (self->monsterInfo.monster_slots && reinforcements && *reinforcements) {
                if (skill->integer)
                        self->monsterInfo.monster_slots += floor(self->monsterInfo.monster_slots * (skill->value / 2.0f));
                if (coop->integer)
                        self->monsterInfo.monster_slots += floor(self->monsterInfo.monster_slots * (skill->value / 2.0f));
                M_SetupReinforcements(reinforcements, self->monsterInfo.reinforcements);
        }

        gi.linkEntity(self);
        M_SetAnimation(self, &oldone_move_stand);
}

/*QUAKED monster_oldone (1 .5 0) (-160 -128 -24) (160 128 256) AMBUSH TRIGGER_SPAWN SIGHT NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Shub-Niggurath. Immobile boss that periodically spawns reinforcements.
*/
void SP_monster_oldone(gentity_t* self) {
        if (!M_AllowSpawn(self)) {
                FreeEntity(self);
                return;
        }

        oldone_precache();

        self->className = "monster_oldone";
        self->s.modelIndex = gi.modelIndex("models/monsters/oldone/tris.md2");

        oldone_configure(self, st);
        stationarymonster_start(self);
}

// -----------------------------------------------------------------------------
// target_oldone_vulnerable: toggles vulnerability for a short duration
// -----------------------------------------------------------------------------

/*QUAKED target_oldone_vulnerable (0 .5 .8) (-8 -8 -8) (8 8 8)
Keys:
- target: name of the Shub to affect. If omitted, applies to all Shubs.
- wait: vulnerability duration in seconds (default 2.0).
When triggered, sets AI_OLDONE_VULNERABLE for the chosen monster(s).
*/
static USE(Use_target_oldone_vulnerable)
(gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
        (void)other;
        (void)activator;

        const float duration = (self->wait > 0.0f) ? self->wait : 2.0f;
        const GameTime until = level.time + GameTime::from_sec(duration);

        for (gentity_t* ent = g_entities; ent < g_entities + globals.numEntities; ++ent) {
                if (!ent->inUse || !ent->className)
                        continue;
                if (strcmp(ent->className, "monster_oldone") != 0)
                        continue;
                if (self->target && (!ent->targetName || strcmp(self->target, ent->targetName) != 0))
                        continue;

                ent->monsterInfo.aiFlags |= AI_OLDONE_VULNERABLE;
                ent->teleportTime = until;
        }
}

void SP_target_oldone_vulnerable(gentity_t* self) {
        self->className = "target_oldone_vulnerable";
        self->use = Use_target_oldone_vulnerable;
}
