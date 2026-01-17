/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

============================================================================== QUAKE SPIKE MINE
- WOR port Floating kamikaze drone that accelerates toward its target and detonates on close
contact. Ported from the Ionized Quake 1 implementation while matching WOR's helper APIs.
==============================================================================*/

#include "../g_local.hpp"
#include "m_spike.hpp"

static cached_soundIndex s_sight;
static cached_soundIndex s_idle;
static cached_soundIndex s_search;
static cached_soundIndex s_death;
static cached_soundIndex s_pain;

static void spike_run(gentity_t* self);
static void spike_explode(gentity_t* self);
static void spike_finish_explode(gentity_t* self);

/*
===============
spike_precache
===============
*/
static void spike_precache() {
        s_search.assign("spike/search.wav");
        s_death.assign("spike/death.wav");
        s_pain.assign("spike/pain.wav");
        s_idle.assign("spike/idle.wav");
        s_sight.assign("spike/sight.wav");

        gi.modelIndex("models/monsters/spike/head/tris.md2");
        gi.modelIndex("models/monsters/spikeball/tris.md2");
}

/*
===============
spike_sight
===============
*/
MONSTERINFO_SIGHT(spike_sight) (gentity_t* self, gentity_t* other) -> void {
        gi.sound(self, CHAN_VOICE, s_sight, 1, ATTN_NORM, 0);
}

/*
===============
spike_idle
===============
*/
MONSTERINFO_IDLE(spike_idle) (gentity_t* self) -> void {
        if (frandom() < 0.5f)
                gi.sound(self, CHAN_VOICE, s_idle, 1, ATTN_IDLE, 0);
}

/*
===============
spike_search
===============
*/
MONSTERINFO_SEARCH(spike_search) (gentity_t* self) -> void {
        if (frandom() < 0.5f)
                gi.sound(self, CHAN_VOICE, s_search, 1, ATTN_NORM, 0);
}

/*
===============
spike_check
Checks for detonation range while running toward the enemy.
===============
*/
static void spike_check(gentity_t* self) {
        if (!self->inUse)
                return;

        if (!self->enemy || !self->enemy->inUse) {
                spike_explode(self);
                return;
        }

        const Vector3 toEnemy = self->enemy->s.origin - self->s.origin;
        const Vector3 aim = VectorToAngles(toEnemy);
        self->s.angles[PITCH] = aim[PITCH];

        self->goalEntity = self->enemy;

        const float dist = realrange(self, self->enemy);
        if (dist < 90.0f)
                spike_explode(self);
}

/*
===============
spike animations
===============
*/
static MonsterFrame spike_frames_stand[] = {
        { ai_stand }, { ai_stand }, { ai_stand },
        { ai_stand }, { ai_stand }, { ai_stand },
        { ai_stand }, { ai_stand }, { ai_stand }
};
MMOVE_T(spike_move_stand) = { FRAME_spike1, FRAME_spike9, spike_frames_stand, nullptr };

MONSTERINFO_STAND(spike_stand) (gentity_t* self) -> void {
        M_SetAnimation(self, &spike_move_stand);
}

static MonsterFrame spike_frames_walk[] = {
        { ai_walk, 6 }, { ai_walk, 6 }, { ai_walk, 6 },
        { ai_walk, 6 }, { ai_walk, 6 }, { ai_walk, 6 },
        { ai_walk, 6 }, { ai_walk, 6 }, { ai_walk, 6 }
};
MMOVE_T(spike_move_walk) = { FRAME_spike1, FRAME_spike9, spike_frames_walk, nullptr };

MONSTERINFO_WALK(spike_walk) (gentity_t* self) -> void {
        M_SetAnimation(self, &spike_move_walk);
}

static MonsterFrame spike_frames_run[] = {
        { ai_run, 12, spike_check }, { ai_run, 12, spike_check }, { ai_run, 12, spike_check },
        { ai_run, 12, spike_check }, { ai_run, 12, spike_check }, { ai_run, 12, spike_check },
        { ai_run, 12, spike_check }, { ai_run, 12, spike_check }, { ai_run, 12, spike_check }
};
MMOVE_T(spike_move_run) = { FRAME_spike1, FRAME_spike9, spike_frames_run, nullptr };

MONSTERINFO_RUN(spike_run) (gentity_t* self) -> void {
        M_SetAnimation(self, &spike_move_run);
}

MONSTERINFO_MELEE(spike_melee) (gentity_t* self) -> void {
        M_SetAnimation(self, &spike_move_run);
}

/*
===============
spike_pain
===============
*/
static bool spike_pain_is_nailgun(ModID mod) {
        return mod == ModID::ETFRifle;
}

static MonsterFrame spike_frames_pain[] = {
        { ai_move, -1 }, { ai_move, -1 }, { ai_move, -1 },
        { ai_move, -1 }, { ai_move, -1 }, { ai_move, -1 },
        { ai_move, -1 }, { ai_move, -1 }, { ai_move, -1 }
};
MMOVE_T(spike_move_pain) = { FRAME_spike1, FRAME_spike9, spike_frames_pain, spike_run };

PAIN(spike_pain) (gentity_t* self, gentity_t* other, float kick, int damage, const MeansOfDeath& mod) -> void {
        if (spike_pain_is_nailgun(mod.id)) {
                const Vector3 point = other ? other->s.origin : self->s.origin;
                Damage(self, self, self, Vector3{}, point, Vector3{}, self->health + 10, 0, DamageFlags::Normal,
                        MeansOfDeath{ ModID::Explosives });
                return;
        }

        if (level.time < self->pain_debounce_time)
                return;

        self->pain_debounce_time = level.time + 3_sec;

        if (!M_ShouldReactToPain(self, mod))
                return;

        gi.sound(self, CHAN_VOICE, s_pain, 1, ATTN_NORM, 0);
        M_SetAnimation(self, &spike_move_pain);
}

/*
===============
spike_dead
===============
*/
static MonsterFrame spike_frames_explode[] = {
        { ai_move }
};
MMOVE_T(spike_move_explode) = { FRAME_spike1, FRAME_spike1, spike_frames_explode, spike_finish_explode };

static void spike_finish_explode(gentity_t* self) {
        RadiusDamage(self, self, 120.0f, nullptr, 150.0f, DamageFlags::Normal, MeansOfDeath{ ModID::Explosives });

        gi.WriteByte(svc_temp_entity);
        gi.WriteByte(TE_EXPLOSION1);
        gi.WritePosition(self->s.origin);
        gi.multicast(self->s.origin, MULTICAST_PHS, false);

        self->touch = nullptr;
        self->takeDamage = false;
        self->svFlags |= SVF_DEADMONSTER;
        self->solid = SOLID_NOT;
        self->moveType = MoveType::Toss;

        ThrowGibs(self, 500, {
                { "models/objects/gibs/sm_meat/tris.md2", GIB_HEAD }
        });

        self->think = FreeEntity;
        self->nextThink = level.time + 0.1_sec;
        gi.linkEntity(self);
}

static void spike_explode(gentity_t* self) {
        if (self->deadFlag)
                return;

        gi.sound(self, CHAN_VOICE, s_death, 1, ATTN_NORM, 0);
        self->deadFlag = true;

        spike_finish_explode(self);
}

DIE(spike_die) (gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, const Vector3& point,
        const MeansOfDeath& mod) -> void {
        if (self->deadFlag)
                return;

        gi.sound(self, CHAN_VOICE, s_death, 1, ATTN_NORM, 0);
        self->deadFlag = true;

        M_SetAnimation(self, &spike_move_explode);
}

/*
===============
spike_sidestep
===============
*/
MONSTERINFO_SIDESTEP(spike_sidestep) (gentity_t* self) -> bool {
        if (skill->integer <= 2)
                return false;

        if (self->monsterInfo.active_move != &spike_move_run)
                M_SetAnimation(self, &spike_move_run);

        return true;
}

/*
===============
spike_set_fly_parameters
===============
*/
static void spike_set_fly_parameters(gentity_t* self) {
        self->monsterInfo.fly_pinned = false;
        self->monsterInfo.fly_thrusters = true;
        self->monsterInfo.fly_position_time = 0_ms;
        self->monsterInfo.fly_acceleration = 20.0f;
        self->monsterInfo.fly_speed = 210.0f;
        self->monsterInfo.fly_min_distance = 0.0f;
        self->monsterInfo.fly_max_distance = 10.0f;
}

/*
===============
SP_monster_spike
===============
*/
void SP_monster_spike(gentity_t* self) {
        if (!M_AllowSpawn(self)) {
                FreeEntity(self);
                return;
        }

        spike_precache();

        self->className = "monster_spike";
        self->moveType = MoveType::Step;
        self->solid = SOLID_BBOX;
        self->s.modelIndex = gi.modelIndex("models/monsters/spikeball/tris.md2");
        self->mins = { -16.0f, -16.0f, -24.0f };
        self->maxs = { 16.0f, 16.0f, 40.0f };

        self->health = self->maxHealth = static_cast<int>(200 * st.health_multiplier);
        self->gibHealth = -80;
        self->mass = 120;

        self->pain = spike_pain;
        self->die = spike_die;

        self->monsterInfo.aiFlags |= AI_IGNORE_SHOTS;

        self->monsterInfo.stand = spike_stand;
        self->monsterInfo.walk = spike_walk;
        self->monsterInfo.run = spike_run;
        self->monsterInfo.melee = spike_melee;
        self->monsterInfo.attack = nullptr;
        self->monsterInfo.idle = spike_idle;
        self->monsterInfo.search = spike_search;
        self->monsterInfo.sight = spike_sight;
        self->monsterInfo.sideStep = spike_sidestep;

        gi.linkEntity(self);

        M_SetAnimation(self, &spike_move_stand);
        self->monsterInfo.scale = MODEL_SCALE;
        self->monsterInfo.combatStyle = CombatStyle::Melee;

        self->monsterInfo.aiFlags |= AI_ALTERNATE_FLY;
        spike_set_fly_parameters(self);

        flymonster_start(self);
}
