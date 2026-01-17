/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

============================================================================== VORE (Shalrath) -
WOR port - Ranged: launches a slow homing pod that tracks target and explodes - No melee;
prefers mid/long engagements with clear shot checks - Idle/search/sight barks, two pain sets,
standard death - Uses WOR muzzle-flash system like Chick/Gunner/Ogre
==============================================================================*/

#include "../g_local.hpp"
#include "m_vore.hpp"
#include "m_flash.hpp"
#include "q1_support.hpp"

// -----------------------------------------------------------------------------
// Spawn flags
// -----------------------------------------------------------------------------
constexpr SpawnFlags SPAWNFLAG_VORE_ONROOF = 8_spawnflag;

static bool        vore_ok_to_transition(gentity_t* self);
static void        vore_jump_straightup(gentity_t* self);
static void        vore_jump_wait_land(gentity_t* self);
static void        vore_jump_up(gentity_t* self);
static void        vore_jump_down(gentity_t* self);
static void        vore_jump(gentity_t* self, BlockedJumpResult result);
void               vore_attack(gentity_t* self);
void               vore_run(gentity_t* self);
static inline bool VORE_ON_CEILING(gentity_t* ent) { return ent->gravityVector[_Z] > 0.0f; }
static void        vore_update_skin(gentity_t* self);

static Vector3 VoreFlashOffset(MonsterMuzzleFlashID flash, bool onCeiling) {
        Vector3 offset = monster_flash_offset[flash];
        if (onCeiling)
                offset[_Z] = -offset[_Z];
        return offset;
}

// -----------------------------------------------------------------------------
// Config
// -----------------------------------------------------------------------------
static const int   VORE_POD_DAMAGE = 35;
static const float VORE_POD_SPEED = 550.0f;
// static const float VORE_POD_TURNRATE = 0.045f; // handled in projectile think (g_weapon.cpp) if used
static const float VORE_MIN_RANGE = 160.0f;
static const float VORE_MAX_RANGE = 1024.0f;
static const float VORE_POD_TURN_FRACTION = 0.075f;

// -----------------------------------------------------------------------------
// Ceiling transitions
// -----------------------------------------------------------------------------
static bool vore_ok_to_transition(gentity_t* self) {
        trace_t trace;
        Vector3 pt, start;
        float   maxDist;
        float   margin;
        float   endHeight;

        if (VORE_ON_CEILING(self)) {
                if (!self->groundEntity)
                        return true;

                maxDist = -384.0f;
                margin = self->mins[_Z] - 8.0f;
        } else {
                maxDist = 256.0f;
                margin = self->maxs[_Z] + 8.0f;
        }

        pt = self->s.origin;
        pt[_Z] += maxDist;
        trace = gi.trace(self->s.origin, self->mins, self->maxs, pt, self, MASK_MONSTERSOLID);

        if (trace.fraction == 1.0f || !(trace.contents & CONTENTS_SOLID) || (trace.ent != world)) {
                if (VORE_ON_CEILING(self)) {
                        if (trace.plane.normal[_Z] < 0.9f)
                                return false;
                } else {
                        if (trace.plane.normal[_Z] > -0.9f)
                                return false;
                }
        }

        endHeight = trace.endPos[_Z];

        pt[_X] = self->absMin[_X];
        pt[_Y] = self->absMin[_Y];
        pt[_Z] = trace.endPos[_Z] + margin;
        start = pt;
        start[_Z] = self->s.origin[_Z];
        trace = gi.traceLine(start, pt, self, MASK_MONSTERSOLID);
        if (trace.fraction == 1.0f || !(trace.contents & CONTENTS_SOLID) || (trace.ent != world))
                return false;
        if (std::fabs(endHeight + margin - trace.endPos[_Z]) > 8.0f)
                return false;

        pt[_X] = self->absMax[_X];
        pt[_Y] = self->absMin[_Y];
        start = pt;
        start[_Z] = self->s.origin[_Z];
        trace = gi.traceLine(start, pt, self, MASK_MONSTERSOLID);
        if (trace.fraction == 1.0f || !(trace.contents & CONTENTS_SOLID) || (trace.ent != world))
                return false;
        if (std::fabs(endHeight + margin - trace.endPos[_Z]) > 8.0f)
                return false;

        pt[_X] = self->absMax[_X];
        pt[_Y] = self->absMax[_Y];
        start = pt;
        start[_Z] = self->s.origin[_Z];
        trace = gi.traceLine(start, pt, self, MASK_MONSTERSOLID);
        if (trace.fraction == 1.0f || !(trace.contents & CONTENTS_SOLID) || (trace.ent != world))
                return false;
        if (std::fabs(endHeight + margin - trace.endPos[_Z]) > 8.0f)
                return false;

        pt[_X] = self->absMin[_X];
        pt[_Y] = self->absMax[_Y];
        start = pt;
        start[_Z] = self->s.origin[_Z];
        trace = gi.traceLine(start, pt, self, MASK_MONSTERSOLID);
        if (trace.fraction == 1.0f || !(trace.contents & CONTENTS_SOLID) || (trace.ent != world))
                return false;
        if (std::fabs(endHeight + margin - trace.endPos[_Z]) > 8.0f)
                return false;

        return true;
}

// -----------------------------------------------------------------------------
// Sounds
// -----------------------------------------------------------------------------
static cached_soundIndex snd_attack;	// not sure what this is for
static cached_soundIndex snd_attack2; // launch
static cached_soundIndex snd_idle;
static cached_soundIndex snd_search;
static cached_soundIndex snd_sight;
static cached_soundIndex snd_pain;
static cached_soundIndex snd_death;

/*
===============
vore_idle
===============
*/
static void vore_idle(gentity_t* self) {
	if (frandom() < 0.5f)
		gi.sound(self, CHAN_VOICE, snd_idle, 1, ATTN_IDLE, 0);
}

/*
===============
vore_search
===============
*/
MONSTERINFO_SEARCH(vore_search) (gentity_t* self) -> void {
	gi.sound(self, CHAN_VOICE, snd_search, 1, ATTN_IDLE, 0);
}

/*
===============
vore_sight
===============
*/
MONSTERINFO_SIGHT(vore_sight) (gentity_t* self, gentity_t* other) -> void {
	gi.sound(self, CHAN_VOICE, snd_sight, 1, ATTN_NORM, 0);
}

/*
===============
vore_stand
===============
*/
static MonsterFrame vore_frames_stand[] = {
	{ ai_stand, 0, vore_idle },
	{ ai_stand },{ ai_stand },{ ai_stand },{ ai_stand },
	{ ai_stand },{ ai_stand },{ ai_stand },{ ai_stand },{ ai_stand }
};

MMOVE_T(vore_move_stand) = { FRAME_stand01, FRAME_stand10, vore_frames_stand, nullptr };
MONSTERINFO_STAND(vore_stand) (gentity_t* self) -> void {
	M_SetAnimation(self, &vore_move_stand);
}

/*
===============
vore_pain
===============
*/
static MonsterFrame vore_frames_pain[] = {
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move }
};
MMOVE_T(vore_move_pain) = { FRAME_pain01, FRAME_pain05, vore_frames_pain, vore_run };

PAIN(vore_pain) (gentity_t* self, gentity_t* other, float kick, int damage, const MeansOfDeath& mod) -> void {
        if (level.time < self->pain_debounce_time)
                return;

        self->pain_debounce_time = level.time + 3_sec;

        if (!M_ShouldReactToPain(self, mod))
                return;

        gi.sound(self, CHAN_VOICE, snd_pain, 1, ATTN_NORM, 0);
        M_SetAnimation(self, &vore_move_pain);
}

/*
===============
vore_select_flash
Map current attack frame to a muzzle flash index and small lateral spread.
===============
*/
static void vore_select_flash(int frame, MonsterMuzzleFlashID& flashNumber, float& spread_rl) {
	switch (frame) {
	case FRAME_attack01:
	case FRAME_attack02:
		flashNumber = MZ2_VORE_POD_1; spread_rl = -0.08f; break;
	case FRAME_attack03:
	case FRAME_attack04:
		flashNumber = MZ2_VORE_POD_2; spread_rl = -0.04f; break;
	case FRAME_attack05:
	case FRAME_attack06:
		flashNumber = MZ2_VORE_POD_3; spread_rl = 0.04f; break;
	default:
		flashNumber = MZ2_VORE_POD_4; spread_rl = 0.08f; break;
	}
}

/*
===============
vore_walk
===============
*/
static MonsterFrame vore_frames_walk[] = {
	{ ai_walk, 4 },{ ai_walk, 4 },{ ai_walk, 4 },{ ai_walk, 4 },
	{ ai_walk, 4 },{ ai_walk, 4 },{ ai_walk, 4 },{ ai_walk, 4 }
};
MMOVE_T(vore_move_walk) = { FRAME_walk01, FRAME_walk08, vore_frames_walk, nullptr };

MONSTERINFO_WALK(vore_walk) (gentity_t* self) -> void {
	M_SetAnimation(self, &vore_move_walk);
}

/*
===============
vore_run
===============
*/
static MonsterFrame vore_frames_run[] = {
	{ ai_run, 10 },{ ai_run, 12 },{ ai_run, 12 },{ ai_run, 14 },
	{ ai_run, 10 },{ ai_run, 12 },{ ai_run, 12 },{ ai_run, 14 }
};
MMOVE_T(vore_move_run) = { FRAME_run01, FRAME_run08, vore_frames_run, nullptr };

MONSTERINFO_RUN(vore_run) (gentity_t* self) -> void {
	M_SetAnimation(self, &vore_move_run);
}

/*
===============
vore_fire
Fire a tracking pod using gunner-style muzzle flash projection.
Note: Requires MZ2_VORE_POD_1..4 entries and offsets in m_flash.hpp.
===============
*/
static void vore_fire(gentity_t* self) {
	if (!self->enemy || !self->enemy->inUse)
		return;

        Vector3 forward, right, up;
        AngleVectors(self->s.angles, forward, right, up);

        MonsterMuzzleFlashID flashNumber;
        float spread_rl;
        vore_select_flash(self->s.frame, flashNumber, spread_rl);

        const bool onCeiling = VORE_ON_CEILING(self);

        // project muzzle from flash offset like Chick/Gunner/Ogre
        const Vector3 start = M_ProjectFlashSource(self, VoreFlashOffset(flashNumber, onCeiling), forward, right);

	// choose target: blind-fire target if set, otherwise enemy origin
	const Vector3 target = (self->monsterInfo.aiFlags & AI_MANUAL_STEERING && self->monsterInfo.blind_fire_target)
		? self->monsterInfo.blind_fire_target
		: self->enemy->s.origin;

	const Vector3 to = target - start;
	const float   dist = to.length();

	Vector3 aim = forward + (right * spread_rl);
	if (dist > 512.0f)
		aim[2] += 0.06f; // gentle loft at long range

	aim.normalize();

        gi.sound(self, CHAN_WEAPON, snd_attack2, 1, ATTN_NORM, 0);

        // follow Chick’s pattern: call wrapper that handles the muzzleflash
        monster_fire_homing_pod(self, start, aim, VORE_POD_DAMAGE, VORE_POD_SPEED, flashNumber);

	// Launch the tracking pod with Quake 1 behaviour.
        fire_vorepod(self, start, aim, VORE_POD_DAMAGE, static_cast<int>(VORE_POD_SPEED),
                static_cast<float>(VORE_POD_DAMAGE), VORE_POD_DAMAGE, VORE_POD_TURN_FRACTION, 0);
        monster_muzzleflash(self, start, flashNumber);
}

/*
===============
vore_attack
===============
*/
static void vore_attack_snd(gentity_t* self) {
        gi.sound(self, CHAN_AUTO, snd_attack, 1, ATTN_NORM, 0);
}

static MonsterFrame vore_frames_attack[] = {
        { ai_charge, 0, vore_attack_snd },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge },
        { ai_charge, 0, vore_fire },
        { ai_charge }
};
MMOVE_T(vore_move_attack) = { FRAME_attack01, FRAME_attack10, vore_frames_attack, vore_run };

MONSTERINFO_ATTACK(vore_attack) (gentity_t* self) -> void {
        if (!M_CheckClearShot(self, VoreFlashOffset(MZ2_VORE_POD_2, VORE_ON_CEILING(self))))
                return;

        monster_done_dodge(self);

        if (self->monsterInfo.attackState == MonsterAttackState::Blind) {
                float chance;

                if (self->monsterInfo.blind_fire_delay < 1_sec)
                        chance = 1.0f;
                else if (self->monsterInfo.blind_fire_delay < 7.5_sec)
                        chance = 0.4f;
                else
                        chance = 0.1f;

                const float r = frandom();

                self->monsterInfo.blind_fire_delay += random_time(5.5_sec, 6.5_sec);

                if (!self->monsterInfo.blind_fire_target)
                        return;

                if (r > chance)
                        return;

                self->monsterInfo.aiFlags |= AI_MANUAL_STEERING;
                M_SetAnimation(self, &vore_move_attack);
                self->monsterInfo.attackFinished = level.time + random_time(2_sec);
                return;
        }

        M_SetAnimation(self, &vore_move_attack);
}

/*
===============
Vore dodge / jump helpers
===============
*/
static MonsterFrame vore_frames_jump_straightup[] = {
        { ai_move, 1, vore_jump_straightup },
        { ai_move, 1, vore_jump_wait_land },
        { ai_move, -1, monster_footstep },
        { ai_move, -1 }
};
MMOVE_T(vore_move_jump_straightup) = { FRAME_run01, FRAME_run04, vore_frames_jump_straightup, vore_run };

static MonsterFrame vore_frames_jump_up[] = {
        { ai_move, -8 },
        { ai_move, -8 },
        { ai_move, -8 },
        { ai_move, -8 },

        { ai_move, 0, vore_jump_up },
        { ai_move, 0, vore_jump_wait_land },
        { ai_move, 0, monster_footstep }
};
MMOVE_T(vore_move_jump_up) = { FRAME_run01, FRAME_run07, vore_frames_jump_up, vore_run };

static MonsterFrame vore_frames_jump_down[] = {
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move },

        { ai_move, 0, vore_jump_down },
        { ai_move, 0, vore_jump_wait_land },
        { ai_move, 0, monster_footstep }
};
MMOVE_T(vore_move_jump_down) = { FRAME_run01, FRAME_run07, vore_frames_jump_down, vore_run };

static void vore_jump_straightup(gentity_t* self) {
        if (self->deadFlag)
                return;

        if (VORE_ON_CEILING(self)) {
                if (vore_ok_to_transition(self)) {
                        self->gravityVector[_Z] = -1.0f;
                        self->s.angles[ROLL] += 180.0f;
                        if (self->s.angles[ROLL] > 360.0f)
                                self->s.angles[ROLL] -= 360.0f;
                        self->groundEntity = nullptr;
                        vore_update_skin(self);
                }
        } else if (self->groundEntity) {
                self->velocity[_X] += crandom() * 5.0f;
                self->velocity[_Y] += crandom() * 5.0f;
                self->velocity[_Z] += -400.0f * self->gravityVector[_Z];

                if (vore_ok_to_transition(self)) {
                        self->gravityVector[_Z] = 1.0f;
                        self->s.angles[ROLL] = 180.0f;
                        self->groundEntity = nullptr;
                        vore_update_skin(self);
                }
        }
}

static void vore_dodge_jump(gentity_t* self) {
        M_SetAnimation(self, &vore_move_jump_straightup);
}

MONSTERINFO_DODGE(vore_dodge) (gentity_t* self, gentity_t* attacker, GameTime eta, trace_t* /*tr*/, bool /*gravity*/) -> void {
        if (!self->groundEntity || self->health <= 0)
                return;

        if (!self->enemy) {
                self->enemy = attacker;
                FoundTarget(self);
                return;
        }

        if (eta < FRAME_TIME_MS || eta > 5_sec)
                return;

        if (self->timeStamp > level.time)
                return;

        self->timeStamp = level.time + random_time(1_sec, 5_sec);

        vore_dodge_jump(self);
}

static void vore_jump_down(gentity_t* self) {
        Vector3 forward, up;

        AngleVectors(self->s.angles, forward, nullptr, up);
        self->velocity += forward * 100.0f;
        self->velocity += up * 300.0f;
}

static void vore_jump_up(gentity_t* self) {
        Vector3 forward, up;

        AngleVectors(self->s.angles, forward, nullptr, up);
        self->velocity += forward * 200.0f;
        self->velocity += up * 450.0f;
}

static void vore_jump_wait_land(gentity_t* self) {
        if (frandom() < 0.4f && level.time >= self->monsterInfo.attackFinished) {
                self->monsterInfo.attackFinished = level.time + 300_ms;
                vore_attack(self);
        }

        if (!self->groundEntity) {
                self->gravity = 1.3f;
                self->monsterInfo.nextFrame = self->s.frame;

                if (monster_jump_finished(self)) {
                        self->gravity = 1.0f;
                        self->monsterInfo.nextFrame = self->s.frame + 1;
                }
        } else {
                self->gravity = 1.0f;
                self->monsterInfo.nextFrame = self->s.frame + 1;
        }
}

static void vore_jump(gentity_t* self, BlockedJumpResult result) {
        if (!self->enemy)
                return;

        if (result == BlockedJumpResult::Jump_Turn_Up)
                M_SetAnimation(self, &vore_move_jump_up);
        else
                M_SetAnimation(self, &vore_move_jump_down);
}

MONSTERINFO_BLOCKED(vore_blocked) (gentity_t* self, float dist) -> bool {
        if (!has_valid_enemy(self))
                return false;

        const bool onCeiling = VORE_ON_CEILING(self);

        if (!onCeiling) {
                if (auto result = blocked_checkjump(self, dist); result != BlockedJumpResult::No_Jump) {
                        if (result != BlockedJumpResult::Jump_Turn)
                                vore_jump(self, result);
                        return true;
                }

                if (blocked_checkplat(self, dist))
                        return true;
        } else {
                if (vore_ok_to_transition(self)) {
                        self->gravityVector[_Z] = -1.0f;
                        self->s.angles[ROLL] += 180.0f;
                        if (self->s.angles[ROLL] > 360.0f)
                                self->s.angles[ROLL] -= 360.0f;
                        self->groundEntity = nullptr;
                        vore_update_skin(self);
                        return true;
                }
        }

        return false;
}

MONSTERINFO_PHYSCHANGED(vore_physics_change) (gentity_t* self) -> void {
        if (VORE_ON_CEILING(self) && !self->groundEntity) {
                self->mins = { -32, -32, -32 };
                self->maxs = { 32, 32, 16 };

                self->gravityVector[_Z] = -1.0f;
                self->s.angles[ROLL] += 180.0f;
                if (self->s.angles[ROLL] > 360.0f)
                        self->s.angles[ROLL] -= 360.0f;
        } else {
                self->mins = { -32, -32, -24 };
                self->maxs = { 32, 32, 32 };
        }

        vore_update_skin(self);
}

static void vore_update_skin(gentity_t* self) {
        const bool damaged = self->health < (self->maxHealth / 2);

        if (damaged)
                self->s.skinNum |= 1;
        else
                self->s.skinNum &= ~1;
}

MONSTERINFO_SETSKIN(vore_setskin) (gentity_t* self) -> void { vore_update_skin(self); }

static void vore_dead(gentity_t* self) {
        self->mins = { -16, -16, -24 };
        self->maxs = { 16, 16, -8 };
        monster_dead(self);
}

static void vore_corpse_shrink(gentity_t* self) {
        self->maxs[_Z] = -4.0f;
        self->svFlags |= SVF_DEADMONSTER;
        gi.linkEntity(self);
}

static MonsterFrame vore_frames_death[] = {
        { ai_move },
        { ai_move },
        { ai_move },
        { ai_move, 0, vore_corpse_shrink },
        { ai_move },
        { ai_move },
        { ai_move }
};
MMOVE_T(vore_move_death) = { FRAME_death01, FRAME_death07, vore_frames_death, vore_dead };

static DIE(vore_die) (gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, const Vector3& point, const MeansOfDeath& mod) -> void {
        self->moveType = MoveType::Toss;
        self->s.angles[ROLL] = 0.0f;
        self->gravityVector = { 0.0f, 0.0f, -1.0f };

        if (M_CheckGib(self, mod)) {
                gi.sound(self, CHAN_VOICE, gi.soundIndex("misc/udeath.wav"), 1, ATTN_NORM, 0);

                self->s.skinNum /= 2;

                ThrowGibs(self, damage, {
                        { 2, "models/objects/gibs/bone/tris.md2" },
                        { 3, "models/objects/gibs/sm_meat/tris.md2" },
                        { "models/monsters/vore/gibs/head.md2", GIB_SKINNED | GIB_HEAD }
                });

                self->deadFlag = true;
                return;
        }

        if (self->deadFlag)
                return;

        gi.sound(self, CHAN_VOICE, snd_death, 1, ATTN_NORM, 0);
        self->deadFlag = true;
        self->takeDamage = true;

        M_SetAnimation(self, &vore_move_death);
}

/*
===============
vore_checkattack
===============
*/
MONSTERINFO_CHECKATTACK(vore_checkattack) (gentity_t* self) -> bool {
	if (!self->enemy || self->enemy->health <= 0)
		return false;

	// range gating
	const float d = (self->enemy->s.origin - self->s.origin).length();
	if (d < VORE_MIN_RANGE || d > VORE_MAX_RANGE)
		return false;

        // clear shot from a representative flash
        if (!M_CheckClearShot(self, VoreFlashOffset(MZ2_VORE_POD_2, VORE_ON_CEILING(self))))
                return false;

	self->monsterInfo.attackState = MonsterAttackState::Missile;
	return true;
}

/*
===============
SP_monster_vore
===============
*/
void SP_monster_vore(gentity_t* self) {

        if (!M_AllowSpawn(self)) {
                FreeEntity(self);
                return;
        }

	// sounds
	snd_attack.assign("shalrath/attack.wav");	// probably to remove
	snd_attack2.assign("shalrath/attack2.wav");
	snd_idle.assign("shalrath/idle.wav");
	snd_search.assign("shalrath/search.wav");	// remove
	snd_sight.assign("shalrath/sight.wav");
	snd_pain.assign("shalrath/pain.wav");
	snd_death.assign("shalrath/death.wav");

        // model + bbox
        self->moveType = MoveType::Step;
        self->solid = SOLID_BBOX;
        self->s.modelIndex = gi.modelIndex("models/monsters/shalrath/tris.md2");
        self->mins = { -32, -32, -24 };
        self->maxs = { 32, 32, 32 };

        // stats
        self->health = self->maxHealth = 600 * st.health_multiplier;
        self->gibHealth = -90;
        self->mass = 125;

        // think/ai wiring
        self->yawSpeed = 20;

        self->monsterInfo.stand = vore_stand;
        self->monsterInfo.walk = vore_walk;
        self->monsterInfo.run = vore_run;
        self->monsterInfo.attack = vore_attack;
        self->monsterInfo.sight = vore_sight;
        self->monsterInfo.search = vore_search;
        self->monsterInfo.dodge = vore_dodge;
        self->monsterInfo.blocked = vore_blocked;
        self->monsterInfo.setSkin = vore_setskin;
        self->monsterInfo.physicsChange = vore_physics_change;
        self->pain = vore_pain;
        self->die = vore_die;
        self->monsterInfo.checkAttack = vore_checkattack;

        // behavior prefs
        self->monsterInfo.combatStyle = CombatStyle::Ranged;
        self->monsterInfo.dropHeight = 256.0f;
        self->monsterInfo.jumpHeight = 68.0f;
        self->monsterInfo.canJump = true;
        self->monsterInfo.blindFire = true;

        if (self->spawnFlags.has(SPAWNFLAG_VORE_ONROOF)) {
                self->s.angles[ROLL] = 180.0f;
                self->gravityVector[_Z] = 1.0f;
                vore_physics_change(self);
        }

        vore_update_skin(self);

        gi.linkEntity(self);

        M_SetAnimation(self, &vore_move_stand);
        self->monsterInfo.scale = MODEL_SCALE;

        walkmonster_start(self);
}
