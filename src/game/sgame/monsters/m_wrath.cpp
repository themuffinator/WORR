/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

============================================================================== WRATH
==============================================================================*/

#include "../g_local.hpp"

#include "m_wrath.hpp"
#include "q1_support.hpp"

static cached_soundIndex soundSight;
static cached_soundIndex soundAttack;
static cached_soundIndex soundDie;
static cached_soundIndex soundPain;
static cached_soundIndex soundAttackSecondary;

constexpr Vector3 kWrathMins = { -16.f, -16.f, -24.f };
constexpr Vector3 kWrathMaxs = { 16.f, 16.f, 32.f };

MONSTERINFO_SIGHT(wrath_sight) (gentity_t* self, gentity_t* other) -> void {
	gi.sound(self, CHAN_VOICE, soundSight, 1, ATTN_NORM, 0);
}

static void wrath_attack_sound(gentity_t* self) {
	gi.sound(self, CHAN_VOICE, soundAttack, 1, ATTN_NORM, 0);
}

MonsterFrame wrath_frames_stand[] = {
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
MMOVE_T(wrath_move_stand) = { FRAME_wrthwk01, FRAME_wrthwk12, wrath_frames_stand, nullptr };

MONSTERINFO_STAND(wrath_stand) (gentity_t* self) -> void {
	M_SetAnimation(self, &wrath_move_stand);
}

MonsterFrame wrath_frames_walk[] = {
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
MMOVE_T(wrath_move_walk) = { FRAME_wrthwk01, FRAME_wrthwk12, wrath_frames_walk, nullptr };

MONSTERINFO_WALK(wrath_walk) (gentity_t* self) -> void {
	M_SetAnimation(self, &wrath_move_walk);
}

MonsterFrame wrath_frames_run[] = {
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
MMOVE_T(wrath_move_run) = { FRAME_wrthwk01, FRAME_wrthwk12, wrath_frames_run, nullptr };

MONSTERINFO_RUN(wrath_run) (gentity_t* self) -> void {
	M_SetAnimation(self, &wrath_move_run);
}

MonsterFrame wrath_frames_pain1[] = {
				{ ai_move },
				{ ai_move },
				{ ai_move },
				{ ai_move },
				{ ai_move },

				{ ai_move }
};
MMOVE_T(wrath_move_pain1) = { FRAME_wrthpa01, FRAME_wrthpa06, wrath_frames_pain1, wrath_run };

MonsterFrame wrath_frames_pain2[] = {
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
MMOVE_T(wrath_move_pain2) = { FRAME_wrthpb01, FRAME_wrthpb11, wrath_frames_pain2, wrath_run };

PAIN(wrath_pain) (gentity_t* self, gentity_t* other, float kick, int damage, const MeansOfDeath& mod) -> void {
	if (level.time < self->pain_debounce_time)
		return;

	self->pain_debounce_time = level.time + 2_sec;

	if (!M_ShouldReactToPain(self, mod))
		return;

	gi.sound(self, CHAN_VOICE, soundPain, 1, ATTN_NORM, 0);

	if (frandom() >= 0.4f)
		M_SetAnimation(self, &wrath_move_pain1);
	else
		M_SetAnimation(self, &wrath_move_pain2);
}

static void wrath_dead(gentity_t* self) {
	RadiusDamage(self, self, 60, nullptr, 105, DamageFlags::Normal, MeansOfDeath{ ModID::Barrel });

	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_EXPLOSION1);
	gi.WritePosition(self->s.origin);
	gi.multicast(self->s.origin, MULTICAST_PHS, false);

	self->s.skinNum /= 2;

	ThrowGibs(self, 55, {
			{ 2, "models/objects/gibs/bone/tris.md2" },
			{ 4, "models/monsters/wrath/gibs/claw.md2" },
			{ 4, "models/monsters/wrath/gibs/arm.md2" },
			{ "models/monsters/overlord/gibs/ribs.md2" },
			{ "models/monsters/wrath/gibs/bone.md2", GIB_HEAD }
		});

	self->touch = nullptr;
}

MonsterFrame wrath_frames_die[] = {
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
MMOVE_T(wrath_move_die) = { FRAME_wrthdt01, FRAME_wrthdt15, wrath_frames_die, wrath_dead };

DIE(wrath_die) (gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, const Vector3& point, const MeansOfDeath& mod) -> void {
	if (self->deadFlag)
		return;

	gi.sound(self, CHAN_VOICE, soundDie, 1, ATTN_NORM, 0);
	self->deadFlag = true;
	self->takeDamage = true;

	M_SetAnimation(self, &wrath_move_die);
}

static void wrath_fire(gentity_t* self) {
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
	}
	else if (frandom() < 0.33f || start[_Z] < self->enemy->absMin[_Z]) {
		aimPoint[_Z] += self->enemy->viewHeight;
		dir = aimPoint - start;
	}
	else {
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
	}
	else {
		if (trace.fraction > 0.5f || !trace.ent || trace.ent->solid != SOLID_BSP)
			fire_vorepod(self, start, dir, damage, rocketSpeed, static_cast<float>(damage), damage, 0.15f, 1);
	}

	gi.sound(self, CHAN_WEAPON | CHAN_RELIABLE, soundAttackSecondary, 1, ATTN_NORM, 0);
}

MonsterFrame wrath_frames_attack1[] = {
		{ ai_charge },
		{ ai_charge },
		{ ai_charge, 0, wrath_attack_sound },
		{ ai_charge },
		{ ai_charge },
		{ ai_charge },
		{ ai_charge },
		{ ai_charge },
		{ ai_charge, -1 },
		{ ai_charge, -2 },

		{ ai_charge, -3 },
		{ ai_charge, -2, wrath_fire },
		{ ai_charge, -1 },
		{ ai_charge }
};
MMOVE_T(wrath_move_attack1) = { FRAME_wrthaa01, FRAME_wrthaa14, wrath_frames_attack1, wrath_run };

MonsterFrame wrath_frames_attack2[] = {
		{ ai_charge },
		{ ai_charge },
		{ ai_charge, 0, wrath_attack_sound },
		{ ai_charge },
		{ ai_charge },
		{ ai_charge, -1 },
		{ ai_charge, -2 },
		{ ai_charge, -3 },
		{ ai_charge, -2, wrath_fire },
		{ ai_charge, -1 },

		{ ai_charge },
		{ ai_charge },
		{ ai_charge }
};
MMOVE_T(wrath_move_attack2) = { FRAME_wrthab01, FRAME_wrthab13, wrath_frames_attack2, wrath_run };

MonsterFrame wrath_frames_attack3[] = {
		{ ai_charge },
		{ ai_charge },
		{ ai_charge, 0, wrath_attack_sound },
		{ ai_charge },
		{ ai_charge },
		{ ai_charge },
		{ ai_charge, -1 },
		{ ai_charge, -2 },
		{ ai_charge, -3 },
		{ ai_charge, -2, wrath_fire },

		{ ai_charge, -1 },
		{ ai_charge },
		{ ai_charge },
		{ ai_charge },
		{ ai_charge }
};
MMOVE_T(wrath_move_attack3) = { FRAME_wrthac01, FRAME_wrthac15, wrath_frames_attack3, wrath_run };

MONSTERINFO_ATTACK(wrath_attack) (gentity_t* self) -> void {
	const Vector3 offset = { 0.f, 0.f, 10.f };

	if (!M_CheckClearShot(self, offset))
		return;

	if (self->monsterInfo.attackState == MonsterAttackState::Blind) {
		float chance;
		if (self->monsterInfo.blind_fire_delay < 1_sec) {
			chance = 1.f;
		}
		else if (self->monsterInfo.blind_fire_delay < 7.5_sec) {
			chance = 0.4f;
		}
		else {
			chance = 0.1f;
		}

		const float roll = frandom();
		self->monsterInfo.blind_fire_delay += random_time(5.5_sec, 6.5_sec);

		if (!self->monsterInfo.blind_fire_target || roll > chance)
			return;

		self->monsterInfo.aiFlags |= AI_MANUAL_STEERING;

		const float attackRoll = frandom();
		if (attackRoll > 0.66f) {
			M_SetAnimation(self, &wrath_move_attack3);
		}
		else if (attackRoll > 0.33f) {
			M_SetAnimation(self, &wrath_move_attack2);
		}
		else {
			M_SetAnimation(self, &wrath_move_attack1);
		}

		self->monsterInfo.attackFinished = level.time + random_time(2_sec);
		return;
	}

	const float attackRoll = frandom();
	if (attackRoll > 0.66f) {
		M_SetAnimation(self, &wrath_move_attack3);
	}
	else if (attackRoll > 0.33f) {
		M_SetAnimation(self, &wrath_move_attack2);
	}
	else {
		M_SetAnimation(self, &wrath_move_attack1);
	}
}

static void wrath_set_fly_parameters(gentity_t* self) {
	self->monsterInfo.fly_thrusters = false;
	self->monsterInfo.fly_acceleration = 20.f;
	self->monsterInfo.fly_speed = 120.f;
	self->monsterInfo.fly_min_distance = 200.f;
	self->monsterInfo.fly_max_distance = 400.f;
}

/*QUAKED monster_wrath (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight
model="models/monsters/wrath/tris.md2"
*/
void SP_monster_wrath(gentity_t* self) {
	if (!M_AllowSpawn(self)) {
		FreeEntity(self);
		return;
	}

	soundSight.assign("wrath/wsee.wav");
	soundAttack.assign("wrath/watt.wav");
	soundDie.assign("wrath/wdthc.wav");
	soundPain.assign("wrath/wpain.wav");
	soundAttackSecondary.assign("vore/attack2.wav");

	self->moveType = MoveType::Step;
	self->solid = SOLID_BBOX;

	self->s.modelIndex = gi.modelIndex("models/monsters/wrath/tris.md2");
	self->mins = kWrathMins;
	self->maxs = kWrathMaxs;

	self->health = 400 * st.health_multiplier;
	self->mass = 400;

	self->pain = wrath_pain;
	self->die = wrath_die;

	self->monsterInfo.stand = wrath_stand;
	self->monsterInfo.walk = wrath_walk;
	self->monsterInfo.run = wrath_run;
	self->monsterInfo.attack = wrath_attack;
	self->monsterInfo.melee = nullptr;
	self->monsterInfo.sight = wrath_sight;
	self->monsterInfo.search = nullptr;

	gi.linkEntity(self);

	M_SetAnimation(self, &wrath_move_stand);
	self->monsterInfo.scale = WRATH_MODEL_SCALE;

	self->flags |= FL_FLY;
	if (!self->yawSpeed)
		self->yawSpeed = 10;
	self->viewHeight = 10;

	flymonster_start(self);

	self->monsterInfo.aiFlags |= AI_ALTERNATE_FLY;
	wrath_set_fly_parameters(self);
}
