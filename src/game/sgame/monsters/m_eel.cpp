/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

m_eel.cpp implementation.*/

#include "../g_local.hpp"
#include "m_eel.hpp"
#include "q1_support.hpp"

static cached_soundIndex sound_chomp;
static cached_soundIndex sound_death;
static cached_soundIndex sound_idle;

MONSTERINFO_IDLE(eel_idle) (gentity_t* self) -> void {
        if (frandom() < 0.5f)
                gi.sound(self, CHAN_AUTO, sound_idle, 1, ATTN_IDLE, 0);
}

static MonsterFrame eel_frames_stand[] = {
        { ai_stand },
        { ai_stand },
        { ai_stand },
        { ai_stand },
        { ai_stand },
        { ai_stand }
};
MMOVE_T(eel_move_stand) = { FRAME_eelswim1, FRAME_eelswim6, eel_frames_stand, nullptr };

MONSTERINFO_STAND(eel_stand) (gentity_t* self) -> void {
        M_SetAnimation(self, &eel_move_stand);
}

static MonsterFrame eel_frames_run[] = {
        { ai_run, 9 },
        { ai_run, 9 },
        { ai_run, 9 },
        { ai_run, 9 },
        { ai_run, 9 },
        { ai_run, 9 }
};
MMOVE_T(eel_move_run) = { FRAME_eelswim1, FRAME_eelswim6, eel_frames_run, nullptr };

MONSTERINFO_RUN(eel_run) (gentity_t* self) -> void {
        M_SetAnimation(self, &eel_move_run);
}

static MonsterFrame eel_frames_walk[] = {
        { ai_walk, 6 },
        { ai_walk, 6 },
        { ai_walk, 6 },
        { ai_walk, 6 },
        { ai_walk, 6 },
        { ai_walk, 6 }
};
MMOVE_T(eel_move_walk) = { FRAME_eelswim1, FRAME_eelswim6, eel_frames_walk, nullptr };

MONSTERINFO_WALK(eel_walk) (gentity_t* self) -> void {
        M_SetAnimation(self, &eel_move_walk);
}

static MonsterFrame eel_frames_pain[] = {
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move }
};
MMOVE_T(eel_move_pain) = { FRAME_eeldth1, FRAME_eeldth4, eel_frames_pain, eel_run };

static void eel_skin_fire(gentity_t* self) {
        if (self->s.skinNum < 5)
                ++self->s.skinNum;
}

static void eel_shoot(gentity_t* self) {
        if (!self->enemy || !self->enemy->inUse)
                return;

        Vector3 forward, right;
        AngleVectors(self->s.angles, forward, right, nullptr);
        const Vector3 start = M_ProjectFlashSource(self, vec3_origin, forward, right);

        Vector3 aim;
        PredictAim(self, self->enemy, start, 800.0f, false, frandom() * 0.3f, &aim, nullptr);
        aim += Vector3{ crandom_open() * 0.025f, crandom_open() * 0.025f, crandom_open() * 0.025f };

        gi.sound(self, CHAN_WEAPON, sound_chomp, 1, ATTN_NORM, 0);
        fire_lightning(self, start, aim, 5, 600, EF_PLASMA);
        self->s.skinNum = 0;
}

static MonsterFrame eel_frames_attack1[] = {
        { ai_charge, 0, eel_skin_fire },
        { ai_charge, 0, eel_skin_fire },
        { ai_charge, 0, eel_skin_fire },
        { ai_charge, 0, eel_skin_fire },
        { ai_charge, -1, eel_skin_fire },
        { ai_charge, -2, eel_shoot }
};
MMOVE_T(eel_move_attack1) = { FRAME_eelswim1, FRAME_eelswim6, eel_frames_attack1, eel_run };

static MonsterFrame eel_frames_attack2[] = {
        { ai_charge, 5, eel_skin_fire },
        { ai_charge, 5, eel_skin_fire },
        { ai_charge, 5, eel_skin_fire },
        { ai_charge, 5, eel_skin_fire },
        { ai_charge, 5, eel_skin_fire },
        { ai_charge, 5, eel_shoot }
};
MMOVE_T(eel_move_attack2) = { FRAME_eelswim1, FRAME_eelswim6, eel_frames_attack2, eel_run };

MONSTERINFO_ATTACK(eel_attack) (gentity_t* self) -> void {
        constexpr float missileChance = 0.5f;

        if (frandom() > missileChance) {
                M_SetAnimation(self, &eel_move_attack1);
                self->monsterInfo.attackState = MonsterAttackState::Straight;
        } else {
                if (frandom() <= 0.5f)
                        self->monsterInfo.lefty = !self->monsterInfo.lefty;
                M_SetAnimation(self, &eel_move_attack2);
                self->monsterInfo.attackState = MonsterAttackState::Sliding;
        }
}

PAIN(eel_pain) (gentity_t* self, gentity_t* other, float kick, int damage, const MeansOfDeath& mod) -> void {
        if (level.time < self->pain_debounce_time)
                return;

        self->pain_debounce_time = level.time + 1_sec;

        if (!M_ShouldReactToPain(self, mod))
                return;

        gi.sound(self, CHAN_VOICE, sound_death, 1, ATTN_NORM, 0);
        M_SetAnimation(self, &eel_move_pain);
}

static void eel_dead(gentity_t* self) {
        self->mins = { -16, -16, -8 };
        self->maxs = { 16, 16, 8 };
        monster_dead(self);
}

static void eel_skin_death(gentity_t* self) {
        if (self->s.skinNum > 0)
                --self->s.skinNum;
}

static MonsterFrame eel_frames_death[] = {
        { ai_move },
        { ai_move, 0, eel_skin_death },
        { ai_move },
        { ai_move, 0, eel_skin_death },
        { ai_move },
        { ai_move, 0, eel_skin_death },
        { ai_move },
        { ai_move, 0, eel_skin_death },
        { ai_move },
        { ai_move, 0, eel_skin_death },
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move }
};
MMOVE_T(eel_move_death) = { FRAME_eeldth1, FRAME_eeldth15, eel_frames_death, eel_dead };

MONSTERINFO_SIGHT(eel_sight) (gentity_t* self, gentity_t* other) -> void {
        gi.sound(self, CHAN_VOICE, sound_idle, 1, ATTN_NORM, 0);
}

static DIE(eel_die) (gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, const Vector3& point,
        const MeansOfDeath& mod) -> void {
        if (M_CheckGib(self, mod)) {
                gi.sound(self, CHAN_VOICE, gi.soundIndex("misc/udeath.wav"), 1, ATTN_NORM, 0);
                ThrowGibs(self, damage, {
                        { 2, "models/objects/gibs/bone/tris.md2" },
                        { "models/objects/gibs/sm_meat/tris.md2" },
                        { "models/monsters/eel/gibs/head.md2", GIB_HEAD }
                });
                self->deadFlag = true;
                return;
        }

        if (self->deadFlag)
                return;

        gi.sound(self, CHAN_VOICE, sound_death, 1, ATTN_NORM, 0);
        self->deadFlag = true;
        self->takeDamage = true;
        self->svFlags |= SVF_DEADMONSTER;
        M_SetAnimation(self, &eel_move_death);
}

static void eel_set_swim_parameters(gentity_t* self) {
        self->monsterInfo.fly_thrusters = false;
        self->monsterInfo.fly_acceleration = 30.0f;
        self->monsterInfo.fly_speed = 110.0f;
        self->monsterInfo.fly_min_distance = 10.0f;
        self->monsterInfo.fly_max_distance = 10.0f;
}

/*QUAKED monster_eel (1 .5 0) (-16 -16 -24) (16 16 24) AMBUSH TRIGGER_SPAWN SIGHT
model="models/monsters/eel/tris.md2"*/
void SP_monster_eel(gentity_t* self) {
        if (!M_AllowSpawn(self)) {
                FreeEntity(self);
                return;
        }

        sound_death.assign("eel/death.wav");
        sound_chomp.assign("eel/bite.wav");
        sound_idle.assign("eel/idle.wav");

        self->moveType = MoveType::Step;
        self->solid = SOLID_BBOX;
        self->s.modelIndex = gi.modelIndex("models/monsters/eel/tris.md2");
        self->mins = { -16, -16, -24 };
        self->maxs = { 16, 16, 32 };

        self->health = 90 * st.health_multiplier;
        self->gibHealth = -50;
        self->mass = 100;

        self->pain = eel_pain;
        self->die = eel_die;

        self->monsterInfo.stand = eel_stand;
        self->monsterInfo.walk = eel_walk;
        self->monsterInfo.run = eel_run;
        self->monsterInfo.attack = eel_attack;
        self->monsterInfo.sight = eel_sight;
        self->monsterInfo.idle = eel_idle;

        gi.linkEntity(self);

        M_SetAnimation(self, &eel_move_stand);
        self->monsterInfo.scale = MODEL_SCALE;

        self->monsterInfo.aiFlags |= AI_ALTERNATE_FLY;
        eel_set_swim_parameters(self);

        swimmonster_start(self);
}
