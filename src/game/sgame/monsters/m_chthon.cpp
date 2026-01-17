/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

============================================================================== CHTHON (Quake 1
Boss) - WOR Port Behavior overview - Immobile boss that lobs lava balls at enemies on a timer. -
Normally invulnerable; a target_chthon_lightning can strike Chthon, dealing a big hit and
briefly making him vulnerable. - Can only be killed while vulnerable (or by telefrag) and only
by energy weapons during that window.
==============================================================================*/

#include "../g_local.hpp"
#include "m_chthon.hpp"
#include "m_flash.hpp"
#include "q1_support.hpp"

// -----------------------------------------------------------------------------
// Tunables
// -----------------------------------------------------------------------------
static constexpr Vector3  CHTHON_MINS = { -128.0f, -128.0f, -24.0f };
static constexpr Vector3  CHTHON_MAXS = { 128.0f,  128.0f, 226.0f };
static constexpr int      CHTHON_BASE_HEALTH = 3000;
static constexpr int      CHTHON_LAVAMAN_HEALTH = 1500;
static constexpr int      CHTHON_GIBHEALTH = -150;
static constexpr int      CHTHON_MASS = 1000;
static constexpr int      CHTHON_PROJECTILE_DAMAGE = 100;
static constexpr int      CHTHON_PROJECTILE_DAMAGE_LAVAMAN = 40;
static constexpr int      CHTHON_PROJECTILE_SPEED = 750;
static constexpr GameTime CHTHON_ATTACK_PERIOD = 2_sec;
static constexpr GameTime CHTHON_PAIN_COOLDOWN = 6_sec;
static constexpr float    CHTHON_PROJECTILE_SIDE_ADJUST = 10.0f;
static constexpr float    CHTHON_RANDOM_HEAD_CHANCE = 0.33f;
static constexpr float    CHTHON_LEAD_CHANCE = 0.35f;

// Sounds
static cached_soundIndex s_idle;
static cached_soundIndex s_sight;
static cached_soundIndex s_pain;
static cached_soundIndex s_death;
static cached_soundIndex s_throw;
static cached_soundIndex s_rise;

// -----------------------------------------------------------------------------
// Fwd decls
// -----------------------------------------------------------------------------
static void chthon_precache();
static void chthon_start(gentity_t* self);
static void chthon_check_attack(gentity_t* self);
static void chthon_attack(gentity_t* self);
static void chthon_attack_left(gentity_t* self);
static void chthon_attack_right(gentity_t* self);
static void chthon_rise(gentity_t* self);
static void chthon_rise_sound(gentity_t* self);
static void chthon_sight_sound(gentity_t* self);
static void chthon_sight_sound2(gentity_t* self);
static void chthon_idle(gentity_t* self);
static void chthon_gib(gentity_t* self);
static void chthon_dead(gentity_t* self);
static void chthon_think(gentity_t* self);
static bool chthon_is_energy_mod(const MeansOfDeath& mod);

// -----------------------------------------------------------------------------
// Utility helpers
// -----------------------------------------------------------------------------
static bool chthon_is_energy_mod(const MeansOfDeath& mod) {
        switch (mod.id) {
        case ModID::Blaster:
        case ModID::Blaster2:
        case ModID::BlueBlaster:
        case ModID::HyperBlaster:
        case ModID::Laser:
        case ModID::BFG10K_Laser:
        case ModID::BFG10K_Blast:
        case ModID::BFG10K_Effect:
        case ModID::IonRipper:
        case ModID::PlasmaGun:
        case ModID::PlasmaGun_Splash:
        case ModID::Phalanx:
        case ModID::Disruptor:
        case ModID::ETFRifle:
        case ModID::PlasmaBeam:
        case ModID::TeslaMine:
        case ModID::Tracker:
        case ModID::DefenderSphere:
        case ModID::Thunderbolt:
        case ModID::Thunderbolt_Discharge:
                return true;
        default:
                return false;
        }
}

static bool chthon_is_lavaman(const gentity_t* self) {
        return (self->className && !Q_strcasecmp(self->className, "monster_lavaman"));
}

static int chthon_base_skin(const gentity_t* self) {
        return chthon_is_lavaman(self) ? 2 : 0;
}

static void chthon_idle(gentity_t* self) {
        if (!s_idle)
                return;
        if (frandom() < 0.1f)
                gi.sound(self, CHAN_VOICE, s_idle, 1, ATTN_IDLE, 0);
}

static void chthon_rise_sound(gentity_t* self) {
        gi.sound(self, CHAN_VOICE, s_rise, 1, ATTN_NORM, 0);
}

static void chthon_sight_sound(gentity_t* self) {
        gi.sound(self, CHAN_VOICE, s_sight, 1, ATTN_NORM, 0);
}

static void chthon_sight_sound2(gentity_t* self) {
        if (frandom() < 0.1f)
                gi.sound(self, CHAN_VOICE, s_sight, 1, ATTN_IDLE, 0);
}

// -----------------------------------------------------------------------------
// Animation state
// -----------------------------------------------------------------------------
static void chthon_stand(gentity_t* self);
static void chthon_walk(gentity_t* self);
static void chthon_run(gentity_t* self);

static MonsterFrame chthon_frames_stand[] = {
        { ai_stand, 0, chthon_idle },
        { ai_stand },
        { ai_stand },
        { ai_stand, 0, chthon_check_attack },
        { ai_stand },
        { ai_stand },
        { ai_stand },
        { ai_stand },
        { ai_stand },
        { ai_stand, 0, chthon_check_attack },
        { ai_stand },
        { ai_stand },
        { ai_stand },
        { ai_stand },
        { ai_stand },
        { ai_stand, 0, chthon_check_attack },
        { ai_stand },
        { ai_stand },
        { ai_stand, 0, chthon_sight_sound },
        { ai_stand },
        { ai_stand },
        { ai_stand },
        { ai_stand, 0, chthon_check_attack },
        { ai_stand },
        { ai_stand },
        { ai_stand },
        { ai_stand },
        { ai_stand },
        { ai_stand },
        { ai_stand, 0, chthon_check_attack },
        { ai_stand }
};
MMOVE_T(chthon_move_stand) = { FRAME_walk01, FRAME_walk31, chthon_frames_stand, chthon_stand };

static MonsterFrame chthon_frames_walk[] = {
        { ai_walk },
        { ai_walk },
        { ai_walk },
        { ai_walk },
        { ai_walk },
        { ai_walk },
        { ai_walk },
        { ai_walk },
        { ai_walk, 0, chthon_sight_sound2 },
        { ai_walk },
        { ai_walk },
        { ai_walk },
        { ai_walk },
        { ai_walk },
        { ai_walk, 0, chthon_check_attack },
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
        { ai_walk },
        { ai_walk },
        { ai_walk },
        { ai_walk },
        { ai_walk },
        { ai_walk, 0, chthon_check_attack }
};
MMOVE_T(chthon_move_walk) = { FRAME_walk01, FRAME_walk31, chthon_frames_walk, chthon_walk };

static MonsterFrame chthon_frames_run[] = {
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge, 0, chthon_sight_sound2 },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge, 0, chthon_check_attack },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge, 0, chthon_check_attack }
};
MMOVE_T(chthon_move_run) = { FRAME_walk01, FRAME_walk31, chthon_frames_run, chthon_run };

static MonsterFrame chthon_frames_rise[] = {
        { ai_move, 0, chthon_rise_sound },
        { ai_move },
        { ai_move, 0, chthon_sight_sound },
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
        { ai_move, 0, chthon_stand }
};
MMOVE_T(chthon_move_rise) = { FRAME_rise01, FRAME_rise17, chthon_frames_rise, nullptr };

static MonsterFrame chthon_frames_shock1[] = {
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move, 0, chthon_check_attack }
};
MMOVE_T(chthon_move_shock1) = { FRAME_shocka01, FRAME_shocka10, chthon_frames_shock1, chthon_walk };

static MonsterFrame chthon_frames_shock2[] = {
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move, 0, chthon_check_attack }
};
MMOVE_T(chthon_move_shock2) = { FRAME_shockb01, FRAME_shockb06, chthon_frames_shock2, chthon_walk };

static MonsterFrame chthon_frames_shock3[] = {
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move, 0, chthon_check_attack }
};
MMOVE_T(chthon_move_shock3) = { FRAME_shockc01, FRAME_shockc10, chthon_frames_shock3, chthon_walk };

static MonsterFrame chthon_frames_attack[] = {
        { ai_charge },
        { ai_charge, 0, chthon_sight_sound2 },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge, 0, chthon_attack_left },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge, 0, chthon_attack_right },
        { ai_charge },
        { ai_charge },
        { ai_charge, 0, chthon_check_attack }
};
MMOVE_T(chthon_move_attack) = { FRAME_attack01, FRAME_attack23, chthon_frames_attack, chthon_walk };

static MonsterFrame chthon_frames_death[] = {
        { ai_move, 0, Q1BossExplode },
        { ai_move, 0, Q1BossExplode },
        { ai_move, 0, Q1BossExplode },
        { ai_move, 0, Q1BossExplode },
        { ai_move, 0, Q1BossExplode },
        { ai_move, 0, Q1BossExplode },
        { ai_move, 0, Q1BossExplode },
        { ai_move, 0, Q1BossExplode },
        { ai_move }
};
MMOVE_T(chthon_move_death) = { FRAME_death01, FRAME_death09, chthon_frames_death, chthon_gib };

MONSTERINFO_STAND(chthon_stand) (gentity_t* self) -> void {
        self->monsterInfo.aiFlags |= AI_STAND_GROUND;
        M_SetAnimation(self, &chthon_move_stand);
}

MONSTERINFO_WALK(chthon_walk) (gentity_t* self) -> void {
        self->monsterInfo.aiFlags |= AI_STAND_GROUND;
        M_SetAnimation(self, &chthon_move_walk);
}

MONSTERINFO_RUN(chthon_run) (gentity_t* self) -> void {
        self->monsterInfo.aiFlags |= AI_STAND_GROUND;
        M_SetAnimation(self, &chthon_move_run);
}

[[maybe_unused]] static void chthon_rise(gentity_t* self) {
        self->monsterInfo.aiFlags |= AI_STAND_GROUND;
        M_SetAnimation(self, &chthon_move_rise);
}

MONSTERINFO_ATTACK(chthon_attack) (gentity_t* self) -> void {
        self->monsterInfo.attackFinished = level.time + CHTHON_ATTACK_PERIOD;

        if (self->monsterInfo.attackState == MonsterAttackState::Blind) {
                float chance;
                if (self->monsterInfo.blind_fire_delay < 1_sec)
                        chance = 1.0f;
                else if (self->monsterInfo.blind_fire_delay < 7.5_sec)
                        chance = 0.4f;
                else
                        chance = 0.1f;

                const float roll = frandom();
                self->monsterInfo.blind_fire_delay += random_time(5.5_sec, 6.5_sec);

                if (!self->monsterInfo.blind_fire_target || roll > chance) {
                        self->monsterInfo.aiFlags &= ~AI_MANUAL_STEERING;
                        return;
                }

                self->monsterInfo.aiFlags |= AI_MANUAL_STEERING;
                M_SetAnimation(self, &chthon_move_attack);
                return;
        }

        self->monsterInfo.aiFlags &= ~AI_MANUAL_STEERING;
        M_SetAnimation(self, &chthon_move_attack);
}

MONSTERINFO_SIGHT(chthon_sight) (gentity_t* self, gentity_t* other) -> void {
        (void)other;
        gi.sound(self, CHAN_VOICE, s_sight, 1, ATTN_NORM, 0);
}

MONSTERINFO_CHECKATTACK(chthon_checkattack) (gentity_t* self) -> bool {
        return M_CheckAttack_Base(self, 0.4f, 0.8f, 0.8f, 0.8f, 0.f, 0.f);
}

MONSTERINFO_SETSKIN(chthon_setskin) (gentity_t* self) -> void {
        const int base = chthon_base_skin(self);
        int skin = base;
        if (self->health <= self->maxHealth / 2)
                skin |= 1;
        self->s.skinNum = skin;
}

// -----------------------------------------------------------------------------
// Combat helpers
// -----------------------------------------------------------------------------
static void chthon_fire_lava(gentity_t* self, float sideSign) {
        if (!self->enemy || !self->enemy->inUse)
                return;

        const bool blindfire = (self->monsterInfo.aiFlags & AI_MANUAL_STEERING) != 0;

        Vector3 forward, right;
        AngleVectors(self->s.angles, forward, right, nullptr);
        Vector3 offset = monster_flash_offset[MZ2_CHTON_ROCKET_1];
        offset[_Y] *= sideSign;
        Vector3 start = M_ProjectFlashSource(self, offset, forward, right);

        Vector3 target = blindfire ? self->monsterInfo.blind_fire_target : self->enemy->s.origin;
        Vector3 aim = target;
        if (!blindfire) {
            if (frandom() < CHTHON_RANDOM_HEAD_CHANCE || start.z < self->enemy->absMin[2])
                    aim[2] += self->enemy->viewHeight;
            else
                    aim[2] = self->enemy->absMin[2] + 1.0f;
        }

        Vector3 dir = aim - start;
        Vector3 aimPoint = aim;
        if (!blindfire && frandom() < CHTHON_LEAD_CHANCE) {
                PredictAim(self, self->enemy, start, static_cast<float>(CHTHON_PROJECTILE_SPEED), false, 0.f, &dir, &aimPoint);
        }

        dir.normalize();
        trace_t trace = gi.traceLine(start, aimPoint, self, MASK_PROJECTILE);

        auto is_blocked = [](const trace_t& tr) {
                if (tr.startSolid || tr.allSolid)
                        return true;
                if (tr.fraction < 0.5f && tr.ent && tr.ent->solid == SOLID_BSP)
                        return true;
                return false;
        };

        auto try_fire = [&](const Vector3& fireDir) {
                gi.sound(self, CHAN_WEAPON, s_throw, 1, ATTN_NORM, 0);
                int damage = chthon_is_lavaman(self) ? CHTHON_PROJECTILE_DAMAGE_LAVAMAN : CHTHON_PROJECTILE_DAMAGE;
                monster_fire_rocket(self, start, fireDir, damage, CHTHON_PROJECTILE_SPEED, MZ2_CHTON_ROCKET_1);
        };

        if (blindfire) {
                if (!is_blocked(trace)) {
                        try_fire(dir);
                        return;
                }

                // try nudging aim sideways to squeeze a shot out
                for (float adjust : { -CHTHON_PROJECTILE_SIDE_ADJUST, CHTHON_PROJECTILE_SIDE_ADJUST }) {
                        Vector3 nudged = aimPoint + right * (adjust * sideSign);
                        Vector3 nudgedDir = nudged - start;
                        nudgedDir.normalize();
                        trace = gi.traceLine(start, nudged, self, MASK_PROJECTILE);
                        if (!is_blocked(trace)) {
                                try_fire(nudgedDir);
                                return;
                        }
                }
        } else {
                if (!is_blocked(trace)) {
                        try_fire(dir);
                        return;
                }
        }
}

static void chthon_attack_left(gentity_t* self) {
        chthon_fire_lava(self, 1.0f);
}

static void chthon_attack_right(gentity_t* self) {
        chthon_fire_lava(self, -1.0f);
}

static void chthon_check_attack(gentity_t* self) {
        if (!self->enemy || !self->enemy->inUse || self->enemy->health <= 0)
                return;
        if (level.time < self->monsterInfo.attackFinished)
                return;
        self->monsterInfo.attack(self);
}

static THINK(chthon_think) (gentity_t* self) -> void {
        self->think = monster_think;
        monster_think(self);

        if (!self->inUse)
                return;

        if (self->think == monster_think && !self->deadFlag && self->health > 0)
                self->think = chthon_think;
}

// -----------------------------------------------------------------------------
// Pain / death
// -----------------------------------------------------------------------------
static PAIN(chthon_pain) (gentity_t* self, gentity_t* other, float kick, int damage, const MeansOfDeath& mod) -> void {
        (void)other; (void)kick;

        const bool vulnerable = (self->monsterInfo.aiFlags & AI_CHTHON_VULNERABLE) != 0;

        if (!vulnerable && mod.id != ModID::Telefragged && mod.id != ModID::Telefrag_Spawn) {
                if (level.time >= self->pain_debounce_time) {
                        self->pain_debounce_time = level.time + 2_sec;
                        gi.sound(self, CHAN_VOICE, s_pain, 1, ATTN_NORM, 0);
                }
                if (self->health < 50)
                        self->health = 50;
                if (self->monsterInfo.setSkin)
                        self->monsterInfo.setSkin(self);
                return;
        }

        if (vulnerable && !chthon_is_energy_mod(mod)) {
                self->health = std::min(self->health + damage, self->maxHealth);
                if (self->monsterInfo.setSkin)
                        self->monsterInfo.setSkin(self);
                return;
        }

        if (level.time < self->pain_debounce_time)
                return;
        if (!M_ShouldReactToPain(self, mod))
                return;

        self->pain_debounce_time = level.time + CHTHON_PAIN_COOLDOWN;
        gi.sound(self, CHAN_VOICE, s_pain, 1, ATTN_NORM, 0);

        if (damage > 25) {
                if (self->health <= self->maxHealth / 6)
                        M_SetAnimation(self, &chthon_move_shock3);
                else if (self->health <= self->maxHealth / 3)
                        M_SetAnimation(self, &chthon_move_shock2);
                else if (self->health <= self->maxHealth / 2)
                        M_SetAnimation(self, &chthon_move_shock1);
        }

        if (self->monsterInfo.setSkin)
                self->monsterInfo.setSkin(self);
}

static void chthon_gib(gentity_t* self) {
        gi.WriteByte(svc_temp_entity);
        gi.WriteByte(TE_EXPLOSION1_BIG);
        gi.WritePosition(self->s.origin);
        gi.multicast(self->s.origin, MULTICAST_PHS, false);

        self->s.sound = 0;
        self->svFlags |= SVF_DEADMONSTER;
        self->solid = SOLID_NOT;
        self->takeDamage = false;

        ThrowGibs(self, 500, {
                        { 2, "models/objects/gibs/bone/tris.md2" },
                        { 1, "models/objects/gibs/bone2/tris.md2" },
                        { 4, "models/objects/gibs/sm_meat/tris.md2" },
                        { "models/objects/gibs/sm_meat/tris.md2" },
                        { "models/objects/gibs/head2/tris.md2", GIB_HEAD | GIB_SKINNED }
                });
}

static DIE(chthon_die) (gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, const Vector3& point, const MeansOfDeath& mod) -> void {
	// Chthon is only killable while vulnerable or by telefrag.
	const bool telefrag = (mod.id == ModID::Telefragged);
	const bool vulnerable = (self->monsterInfo.aiFlags & AI_CHTHON_VULNERABLE) != 0;

	if (!telefrag && !vulnerable) {
		// Refuse to die outside the vulnerability window.
		// Play a pain bark and std::clamp very low health so stray hits cannot finish him.
		if (level.time >= self->pain_debounce_time) {
			self->pain_debounce_time = level.time + 1_sec;
			gi.sound(self, CHAN_VOICE, s_pain, 1, ATTN_NORM, 0);
		}
		if (self->health < 50)
			self->health = 50;
		return;
	}

	// Normal monster die structure from here on.

	// check for gib
	if (M_CheckGib(self, mod)) {
		gi.sound(self, CHAN_VOICE, gi.soundIndex("misc/udeath.wav"), 1, ATTN_NORM, 0);

		// Optionally alter skin on gib if your model supports it
		// self->s.skinNum /= 2;

                ThrowGibs(self, damage, {
                        { 3, "models/objects/gibs/bone/tris.md2" },
                        { 4, "models/objects/gibs/sm_meat/tris.md2" },
                        { "models/objects/gibs/head2/tris.md2", GIB_HEAD | GIB_SKINNED }
                });

                self->deadFlag = true;
                return;
        }

        if (self->deadFlag)
                return;

	// regular death
	self->deadFlag = true;
	self->takeDamage = true;

	// Chthon typically has a single death sound; if you have multiple, branch like chick_die.
	gi.sound(self, CHAN_VOICE, s_death, 1, ATTN_NORM, 0);

	// If you have death animations, trigger them here, e.g.:
	// M_SetAnimation(self, &chthon_move_death1);
        // Otherwise, remove the corpse in the standard way.
        Q1BossExplode(self);
        chthon_dead(self);
}

static void chthon_dead(gentity_t* self) {
        monster_dead(self);
}

static void chthon_precache() {
        gi.modelIndex("models/monsters/chthon/tris.md2");

        s_idle.assign("chthon/idle1.wav");
        s_sight.assign("chthon/sight1.wav");
        s_pain.assign("chthon/pain.wav");
        s_death.assign("chthon/death.wav");
        s_throw.assign("chthon/throw.wav");
        s_rise.assign("chthon/out1.wav");

        gi.soundIndex("misc/udeath.wav");
}

static void chthon_configure(gentity_t* self) {
        self->mins = CHTHON_MINS;
        self->maxs = CHTHON_MAXS;
        self->yawSpeed = 10;
        self->mass = CHTHON_MASS;

        if (chthon_is_lavaman(self)) {
                self->s.skinNum = chthon_base_skin(self);
                self->health = self->maxHealth = CHTHON_LAVAMAN_HEALTH;
        } else {
                self->s.skinNum = chthon_base_skin(self);
                self->health = self->maxHealth = CHTHON_BASE_HEALTH;
        }
        self->gibHealth = CHTHON_GIBHEALTH;
        self->moveType = MoveType::None;
        self->svFlags |= SVF_MONSTER;
        self->takeDamage = true;
        self->monsterInfo.attackFinished = level.time;

        self->monsterInfo.stand = chthon_stand;
        self->monsterInfo.walk = chthon_walk;
        self->monsterInfo.run = chthon_run;
        self->monsterInfo.attack = chthon_attack;
        self->monsterInfo.sight = chthon_sight;
        self->monsterInfo.checkAttack = chthon_checkattack;
        self->monsterInfo.setSkin = chthon_setskin;
        self->monsterInfo.aiFlags |= AI_STAND_GROUND | AI_IGNORE_SHOTS;

        self->pain = chthon_pain;
        self->die = chthon_die;

        gi.linkEntity(self);

        if (self->monsterInfo.setSkin)
                self->monsterInfo.setSkin(self);

        if (!self->spawnFlags.has(SPAWNFLAG_MONSTER_CORPSE))
                M_SetAnimation(self, &chthon_move_rise);
        else
                M_SetAnimation(self, &chthon_move_stand);

        stationarymonster_start(self);

        self->think = chthon_think;
        self->nextThink = level.time + 250_ms;
}

// -----------------------------------------------------------------------------
// Spawn functions
// -----------------------------------------------------------------------------
static void chthon_start(gentity_t* self) {
        chthon_precache();
        self->s.modelIndex = gi.modelIndex("models/monsters/chthon/tris.md2");
        chthon_configure(self);
}

void SP_monster_chthon(gentity_t* self) {
        if (!M_AllowSpawn(self)) {
                FreeEntity(self);
                return;
        }

        self->className = "monster_chthon";
        chthon_start(self);
}

void SP_monster_lavaman(gentity_t* self) {
        if (!M_AllowSpawn(self)) {
                FreeEntity(self);
                return;
        }

        self->className = "monster_lavaman";
        self->s.scale = 0.75f;
        chthon_start(self);
}

void SP_monster_boss(gentity_t* self) {
        SP_monster_chthon(self);
}

// -----------------------------------------------------------------------------
// target_chthon_lightning: applies a big damage hit and brief vulnerability
// -----------------------------------------------------------------------------
static THINK(chthon_clear_vuln_think) (gentity_t* self) -> void {
        self->monsterInfo.aiFlags &= ~AI_CHTHON_VULNERABLE;
        if (self->monsterInfo.setSkin)
                self->monsterInfo.setSkin(self);
        self->think = chthon_think;
        self->nextThink = level.time + 250_ms;
}

static USE(Use_target_chthon_lightning) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
        (void)other;

        const int   lightning_damage = (self->dmg > 0) ? self->dmg : 200;
        const float vuln_seconds = (self->wait > 0.f) ? self->wait : 1.5f;

        for (gentity_t* e = g_entities; e < g_entities + globals.numEntities; ++e) {
                if (!e->inUse || !e->className)
                        continue;
                if (strcmp(e->className, "monster_chthon") != 0 && strcmp(e->className, "monster_lavaman") != 0)
                        continue;
                if (self->target && e->targetName && strcmp(self->target, e->targetName) != 0)
                        continue;

                e->monsterInfo.aiFlags |= AI_CHTHON_VULNERABLE;

                Damage(
                        e,
                        self,
                        activator ? activator : self,
                        Vector3{ 0, 0, 0 },
                        e->s.origin,
                        Vector3{ 0, 0, 0 },
                        lightning_damage,
                        0,
                        DamageFlags::Energy | DamageFlags::NoKnockback,
                        ModID::Laser
                );

                e->think = chthon_clear_vuln_think;
                e->nextThink = level.time + GameTime::from_sec(vuln_seconds);

                if (e->monsterInfo.setSkin)
                        e->monsterInfo.setSkin(e);
        }

        FreeEntity(self);
}

void SP_target_chthon_lightning(gentity_t* self) {
        self->className = "target_chthon_lightning";
        self->use = Use_target_chthon_lightning;
}
