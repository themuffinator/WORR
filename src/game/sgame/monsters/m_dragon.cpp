/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

============================================================================== QUAKE WYVERN /
DRAGON (Ionized port)
==============================================================================*/

#include "../g_local.hpp"
#include "m_dragon.hpp"
#include "q1_support.hpp"

extern const spawn_temp_t& ED_GetSpawnTemp();

namespace {

	constexpr Vector3 DRAGON_MINS = { -72.0f, -48.0f, -32.0f };
	constexpr Vector3 DRAGON_MAXS = { 104.0f,  48.0f,  48.0f };
	constexpr Vector3 DRAGON_DEAD_MINS = { -144.0f, -136.0f, -36.0f };
	constexpr Vector3 DRAGON_DEAD_MAXS = { 88.0f, 128.0f, 24.0f };
	constexpr Vector3 DRAGON_FIRE_OFFSET = { 96.0f, 0.0f, 32.0f };
	constexpr Vector3 DRAGON_CLEARANCE_OFFSET = { 0.0f, 96.0f, 32.0f };

	constexpr int DRAGON_BASE_HEALTH = 3000;
	constexpr int DRAGON_COOP_HEALTH_PER_PLAYER = 500;
	constexpr int DRAGON_BASE_ARMOR = 500;
	constexpr int DRAGON_COOP_ARMOR_PER_PLAYER = 250;
	constexpr int DRAGON_GIB_HEALTH = -500;
	constexpr int DRAGON_MASS = 750;
	constexpr GameTime DRAGON_DEAD_THINK_TIME = 15_sec;

	constexpr float DRAGON_FLY_ACCEL = 20.0f;
	constexpr float DRAGON_FLY_SPEED = 120.0f;
	constexpr float DRAGON_FLY_MIN_DISTANCE = 550.0f;
	constexpr float DRAGON_FLY_MAX_DISTANCE = 750.0f;

	static cached_soundIndex s_sight;
	static cached_soundIndex s_search;
	static cached_soundIndex s_attack;
	static cached_soundIndex s_die;
	static cached_soundIndex s_pain;

	static void dragon_reattack(gentity_t* self);

	static void dragon_precache() {
		gi.modelIndex("models/monsters/dragon/tris.md2");
		gi.modelIndex("models/monsters/dragon/gibs/leg.md2");
		gi.modelIndex("models/monsters/dragon/gibs/wing.md2");
		gi.modelIndex("models/monsters/dragon/gibs/head.md2");

		s_sight.assign("dragon/see.wav");
		s_search.assign("dragon/active.wav");
		s_attack.assign("dragon/attack.wav");
		s_die.assign("dragon/death.wav");
		s_pain.assign("dragon/pain.wav");
	}

	MONSTERINFO_SIGHT(dragon_sight) (gentity_t* self, gentity_t* other) -> void {
		(void)other;
		gi.sound(self, CHAN_VOICE, s_sight, 1, ATTN_NONE, 0);
	}

	MONSTERINFO_SEARCH(dragon_search) (gentity_t* self) -> void {
		gi.sound(self, CHAN_VOICE, s_search, 1, ATTN_NONE, 0);
	}

	static MonsterFrame dragon_frames_hover[] = {
			{ ai_stand }, { ai_stand }, { ai_stand }, { ai_stand }, { ai_stand },
			{ ai_stand }, { ai_stand }, { ai_stand }, { ai_stand }, { ai_stand },
			{ ai_stand }, { ai_stand }, { ai_stand }
	};
	MMOVE_T(dragon_move_hover) = { FRAME_drgfly01, FRAME_drgfly13, dragon_frames_hover, nullptr };

	MONSTERINFO_STAND(dragon_hover) (gentity_t* self) -> void {
		M_SetAnimation(self, &dragon_move_hover);
	}

	static MonsterFrame dragon_frames_walk[] = {
			{ ai_walk, 5 }, { ai_walk, 5 }, { ai_walk, 5 }, { ai_walk, 5 }, { ai_walk, 5 },
			{ ai_walk, 5 }, { ai_walk, 5 }, { ai_walk, 5 }, { ai_walk, 5 }, { ai_walk, 5 },
			{ ai_walk, 5 }, { ai_walk, 5 }, { ai_walk, 5 }
	};
	MMOVE_T(dragon_move_walk) = { FRAME_drgfly01, FRAME_drgfly13, dragon_frames_walk, nullptr };

	MONSTERINFO_WALK(dragon_walk) (gentity_t* self) -> void {
		M_SetAnimation(self, &dragon_move_walk);
	}

	static MonsterFrame dragon_frames_run[] = {
			{ ai_run, 10 }, { ai_run, 10 }, { ai_run, 10 }, { ai_run, 10 }, { ai_run, 10 },
			{ ai_run, 10 }, { ai_run, 10 }, { ai_run, 10 }, { ai_run, 10 }, { ai_run, 10 },
			{ ai_run, 10 }, { ai_run, 10 }, { ai_run, 10 }
	};
	MMOVE_T(dragon_move_run) = { FRAME_drgfly01, FRAME_drgfly13, dragon_frames_run, nullptr };

	MONSTERINFO_RUN(dragon_run) (gentity_t* self) -> void {
		M_SetAnimation(self, &dragon_move_run);
	}

	static MonsterFrame dragon_frames_pain1[] = {
			{ ai_move }, { ai_move }, { ai_move }
	};
	MMOVE_T(dragon_move_pain1) = { FRAME_drgpan1a, FRAME_drgpan1c, dragon_frames_pain1, dragon_run };

	static MonsterFrame dragon_frames_pain2[] = {
			{ ai_move }, { ai_move }, { ai_move }
	};
	MMOVE_T(dragon_move_pain2) = { FRAME_drgpan2a, FRAME_drgpan2c, dragon_frames_pain2, dragon_run };

	static MonsterFrame dragon_frames_pain3[] = {
			{ ai_move }, { ai_move }, { ai_move }
	};
	MMOVE_T(dragon_move_pain3) = { FRAME_drgpan3a, FRAME_drgpan3c, dragon_frames_pain3, dragon_run };

	static MonsterFrame dragon_frames_pain4[] = {
			{ ai_move }, { ai_move }, { ai_move }
	};
	MMOVE_T(dragon_move_pain4) = { FRAME_drgpan4a, FRAME_drgpan4c, dragon_frames_pain4, dragon_run };

	static MonsterFrame dragon_frames_pain5[] = {
			{ ai_move }, { ai_move }, { ai_move }
	};
	MMOVE_T(dragon_move_pain5) = { FRAME_drgpan5a, FRAME_drgpan5c, dragon_frames_pain5, dragon_run };

	static MonsterFrame dragon_frames_pain6[] = {
			{ ai_move }, { ai_move }, { ai_move }
	};
	MMOVE_T(dragon_move_pain6) = { FRAME_drgpan6a, FRAME_drgpan6c, dragon_frames_pain6, dragon_run };

	static PAIN(dragon_pain) (gentity_t* self, gentity_t* other, float kick, int damage, const MeansOfDeath& mod) -> void {
		(void)other;
		(void)kick;

		if (level.time < self->pain_debounce_time)
			return;

		self->pain_debounce_time = level.time + 3_sec;

		if (!M_ShouldReactToPain(self, mod))
			return;

		gi.sound(self, CHAN_VOICE, s_pain, 1, ATTN_NORM, 0);

		if (damage < 30)
			M_SetAnimation(self, &dragon_move_pain1);
		else if (damage < 60) {
			if (frandom() >= 0.5f)
				M_SetAnimation(self, &dragon_move_pain2);
			else
				M_SetAnimation(self, &dragon_move_pain3);
		}
		else if (damage > 120) {
			M_SetAnimation(self, &dragon_move_pain6);
		}
		else {
			if (frandom() >= 0.5f)
				M_SetAnimation(self, &dragon_move_pain4);
			else
				M_SetAnimation(self, &dragon_move_pain5);
		}
	}

	MONSTERINFO_SETSKIN(dragon_setskin) (gentity_t* self) -> void {
		if (self->health < (self->maxHealth / 2))
			self->s.skinNum |= 1;
		else
			self->s.skinNum &= ~1;
	}

	static void dragon_gib(gentity_t* self) {
		gi.sound(self, CHAN_VOICE, gi.soundIndex("misc/udeath.wav"), 1, ATTN_NORM, 0);

		self->s.skinNum /= 2;

		ThrowGibs(self, 1000, {
				{ 2, "models/objects/gibs/bone/tris.md2" },
				{ 4, "models/objects/gibs/sm_meat/tris.md2" },
				{ "models/monsters/dragon/gibs/leg.md2" },
				{ "models/monsters/dragon/gibs/wing.md2" },
				{ "models/monsters/dragon/gibs/head.md2", GIB_HEAD }
			});
	}

	static THINK(dragon_deadthink) (gentity_t* self) -> void {
		if (!self->groundEntity && level.time < self->timeStamp) {
			self->nextThink = level.time + FRAME_TIME_S;
			return;
		}
	}

	static void dragon_dead(gentity_t* self) {
		self->mins = DRAGON_DEAD_MINS;
		self->maxs = DRAGON_DEAD_MAXS;
		self->moveType = MoveType::Toss;
		self->think = dragon_deadthink;
		self->nextThink = level.time + FRAME_TIME_S;
		self->timeStamp = level.time + DRAGON_DEAD_THINK_TIME;
		gi.linkEntity(self);

		dragon_gib(self);
	}

	static MonsterFrame dragon_frames_die1[] = {
			{ ai_move, 0, Q1BossExplode },
			{ ai_move }, { ai_move }, { ai_move }, { ai_move },
			{ ai_move }, { ai_move }, { ai_move }, { ai_move }, { ai_move },
			{ ai_move }, { ai_move }, { ai_move }, { ai_move }, { ai_move },
			{ ai_move }, { ai_move }, { ai_move }, { ai_move }, { ai_move },
			{ ai_move }
	};
	MMOVE_T(dragon_move_die1) = { FRAME_drgdth01, FRAME_drgdth21, dragon_frames_die1, dragon_dead };

	static DIE(dragon_die) (gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, const Vector3& point, const MeansOfDeath& mod) -> void {
		(void)inflictor;
		(void)attacker;
		(void)damage;
		(void)point;

		if (M_CheckGib(self, mod)) {
			dragon_gib(self);
			self->deadFlag = true;
			return;
		}

		if (self->deadFlag)
			return;

		self->deadFlag = true;
		self->takeDamage = true;

		gi.sound(self, CHAN_VOICE, s_die, 1, ATTN_NORM, 0);
		M_SetAnimation(self, &dragon_move_die1);
	}

	static void dragon_fireball(gentity_t* self) {
		if (!self->enemy || !self->enemy->inUse)
			return;

		Vector3 forward{}, right{};
		AngleVectors(self->s.angles, &forward, &right, nullptr);

		Vector3 start = M_ProjectFlashSource(self, DRAGON_FIRE_OFFSET, forward, right);

		int damage;
		int speed;
		int fireCount;
		if (frandom() > 0.66f) {
			fireCount = (skill->integer > 1) ? 2 : 1;
			damage = 80 + static_cast<int>(frandom() * 20.0f);
			speed = 1250;
			self->style = 1;
		}
		else {
			int maxBursts = std::max(1, skill->integer);
			fireCount = irandom(1, maxBursts);
			damage = 90;
			speed = static_cast<int>(frandom() * 300.0f + 900.0f);
			self->style = 0;
		}

		const bool blindFire = (self->monsterInfo.aiFlags & AI_MANUAL_STEERING) != 0;

		Vector3 target = blindFire ? self->monsterInfo.blind_fire_target : self->enemy->s.origin;
		Vector3 vec = target;
		Vector3 dir;

		if (blindFire) {
			dir = vec - start;
		}
		else if (frandom() < 0.33f || start.z < self->enemy->absMin.z) {
			vec.z += self->enemy->viewHeight;
			dir = vec - start;
		}
		else {
			vec.z = self->enemy->absMin.z + 1.0f;
			dir = vec - start;
		}

		if (!blindFire && frandom() < 0.35f)
			PredictAim(self, self->enemy, start, 750.0f, false, 0.0f, &dir, &vec);

		dir.normalize();

		trace_t trace = gi.traceLine(start, vec, self, MASK_PROJECTILE);

        for (int shot = 0; shot < fireCount; ++shot) {
                if (blindFire) {
                        if (!(trace.startSolid || trace.allSolid || (trace.fraction < 0.5f))) {
                                if (self->style == 1) {
                                        fire_plasmaball(self, start, dir, damage, speed, damage * 2);
                                } else {
                                        [[maybe_unused]] gentity_t* projectile = fire_lavaball(
                                                self, start, dir, damage, speed, static_cast<float>(damage), damage);
                                }
                        } else {
                                vec = target + (right * -10.0f);
                                dir = vec - start;
                                dir.normalize();
                                trace = gi.traceLine(start, vec, self, MASK_PROJECTILE);

                                if (!(trace.startSolid || trace.allSolid || (trace.fraction < 0.5f))) {
                                        if (self->style == 1) {
                                                fire_plasmaball(self, start, dir, damage, speed, damage * 2);
                                        }
                                        else {
                                                [[maybe_unused]] gentity_t* projectile = fire_lavaball(
                                                        self, start, dir, damage, speed, static_cast<float>(damage), damage);
                                        }
                                } else {
                                        vec = target + (right * 10.0f);
                                        dir = vec - start;
                                        dir.normalize();
                                        trace = gi.traceLine(start, vec, self, MASK_PROJECTILE);

                                        if (!(trace.startSolid || trace.allSolid || (trace.fraction < 0.5f))) {
                                                if (self->style == 1) {
                                                        fire_plasmaball(self, start, dir, damage, speed, damage * 2);
                                                }
                                                else {
                                                        [[maybe_unused]] gentity_t* projectile = fire_lavaball(
                                                                self, start, dir, damage, speed, static_cast<float>(damage), damage);
                                                }
                                        }
                                }
                        }
                } else {
                        const float r = crandom() * 1000.0f;
                        vec = start + (forward * 8192.0f) + (right * r);
                        dir = vec - start;
                        dir.normalize();

                        if (trace.fraction > 0.5f || !trace.ent || trace.ent->solid != SOLID_BSP) {
                                if (self->style == 1) {
                                        fire_plasmaball(self, start, dir, damage, speed, damage * 2);
                                }
                                else {
                                        [[maybe_unused]] gentity_t* projectile = fire_lavaball(
                                                self, start, dir, damage, speed, static_cast<float>(damage), damage);
                                }
                        }
                }

			gi.sound(self, CHAN_VOICE, s_attack, 1, ATTN_NORM, 0);
		}
	}

	static void dragon_tail(gentity_t* self) {
		if (fire_hit(self, Vector3{ MELEE_DISTANCE, 0.0f, -32.0f }, static_cast<int>(frandom() * 30.0f) + 30, 400.0f)) {
			if (self->enemy && self->enemy->client && self->enemy->velocity.z < 270.0f)
				self->enemy->velocity.z = 270.0f;
		}
	}

	static MonsterFrame dragon_frames_attack1[] = {
			{ ai_charge, 17 },
			{ ai_charge, 12 },
			{ ai_charge, 7 },
			{ ai_charge, 2 },
			{ ai_charge },
			{ ai_charge, -2 },
			{ ai_charge, -7 },
			{ ai_charge, -12, dragon_fireball },
			{ ai_charge, -7 },
			{ ai_charge, -2, dragon_reattack }
	};
	MMOVE_T(dragon_move_attack1) = { FRAME_drgfir01, FRAME_drgfir10, dragon_frames_attack1, nullptr };

	static void dragon_postfix1(gentity_t* self) {
		M_SetAnimation(self, &dragon_move_run, false);
		self->monsterInfo.nextFrame = FRAME_drgfly05;
	}

	static void dragon_postfix2(gentity_t* self) {
		M_SetAnimation(self, &dragon_move_run, false);
		self->monsterInfo.nextFrame = FRAME_drgfly07;
	}

	static void dragon_postfix3(gentity_t* self) {
		M_SetAnimation(self, &dragon_move_run, false);
		self->monsterInfo.nextFrame = FRAME_drgfly09;
	}

	static void dragon_postfix4(gentity_t* self) {
		M_SetAnimation(self, &dragon_move_run, false);
		self->monsterInfo.nextFrame = FRAME_drgfly11;
	}

	static void dragon_postfix5(gentity_t* self) {
		M_SetAnimation(self, &dragon_move_run, false);
		self->monsterInfo.nextFrame = FRAME_drgfly13;
	}

	static void dragon_postfix6(gentity_t* self) {
		M_SetAnimation(self, &dragon_move_run, false);
		self->monsterInfo.nextFrame = FRAME_drgfly03;
	}

	static MonsterFrame dragon_frames_fix1[] = {
			{ ai_charge, 17 },
			{ ai_charge, 17, dragon_fireball },
			{ ai_charge, 17, dragon_postfix1 }
	};
	MMOVE_T(dragon_move_fix1) = { FRAME_drgfix1a, FRAME_drgfix1c, dragon_frames_fix1, nullptr };

	static MonsterFrame dragon_frames_fix2[] = {
			{ ai_charge, 17 },
			{ ai_charge, 17, dragon_fireball },
			{ ai_charge, 17, dragon_postfix2 }
	};
	MMOVE_T(dragon_move_fix2) = { FRAME_drgfix2a, FRAME_drgfix2c, dragon_frames_fix2, nullptr };

	static MonsterFrame dragon_frames_fix3[] = {
			{ ai_charge, 17 },
			{ ai_charge, 17, dragon_fireball },
			{ ai_charge, 17, dragon_postfix3 }
	};
	MMOVE_T(dragon_move_fix3) = { FRAME_drgfix3a, FRAME_drgfix3c, dragon_frames_fix3, nullptr };

	static MonsterFrame dragon_frames_fix4[] = {
			{ ai_charge, 17 },
			{ ai_charge, 17, dragon_fireball },
			{ ai_charge, 17, dragon_postfix4 }
	};
	MMOVE_T(dragon_move_fix4) = { FRAME_drgfix4a, FRAME_drgfix4c, dragon_frames_fix4, nullptr };

	static MonsterFrame dragon_frames_fix5[] = {
			{ ai_charge, 17 },
			{ ai_charge, 17, dragon_fireball },
			{ ai_charge, 17, dragon_postfix5 }
	};
	MMOVE_T(dragon_move_fix5) = { FRAME_drgfix5a, FRAME_drgfix5c, dragon_frames_fix5, nullptr };

	static MonsterFrame dragon_frames_fix6[] = {
			{ ai_charge, 17 },
			{ ai_charge, 17, dragon_fireball },
			{ ai_charge, 17 },
			{ ai_charge, 17, dragon_postfix6 }
	};
	MMOVE_T(dragon_move_fix6) = { FRAME_drgfix6a, FRAME_drgfix6d, dragon_frames_fix6, nullptr };

	MONSTERINFO_ATTACK(dragon_attack) (gentity_t* self) -> void {
		if (!M_CheckClearShot(self, DRAGON_CLEARANCE_OFFSET))
			return;

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

			if (!self->monsterInfo.blind_fire_target)
				return;

			if (roll > chance)
				return;

			self->monsterInfo.aiFlags |= AI_MANUAL_STEERING;
			M_SetAnimation(self, &dragon_move_attack1);
			self->monsterInfo.attackFinished = level.time + random_time(2_sec);
			return;
		}

		if (self->s.frame == FRAME_drgfly01)
			M_SetAnimation(self, &dragon_move_fix1);
		else if (self->s.frame == FRAME_drgfly03)
			M_SetAnimation(self, &dragon_move_fix2);
		else if (self->s.frame == FRAME_drgfly05)
			M_SetAnimation(self, &dragon_move_fix3);
		else if (self->s.frame == FRAME_drgfly07)
			M_SetAnimation(self, &dragon_move_fix4);
		else if (self->s.frame == FRAME_drgfly09)
			M_SetAnimation(self, &dragon_move_fix5);
		else if (self->s.frame == FRAME_drgfly11)
			M_SetAnimation(self, &dragon_move_fix6);
		else
			M_SetAnimation(self, &dragon_move_attack1);
	}

	static void dragon_reattack(gentity_t* self) {
		if (self->enemy && self->enemy->health > 0)
			if (visible(self, self->enemy))
				if (frandom() <= 0.4f) {
					M_SetAnimation(self, &dragon_move_attack1);
					return;
				}

		dragon_run(self);
	}

	static MonsterFrame dragon_frames_melee[] = {
			{ ai_charge, 12 }, { ai_charge, 12 }, { ai_charge, 12 }, { ai_charge, 12 }, { ai_charge, 12 },
			{ ai_charge, 12 }, { ai_charge, 12 }, { ai_charge, 12 }, { ai_charge, 12 }, { ai_charge, 0, dragon_tail },
			{ ai_charge, 10 }, { ai_charge, 10 }, { ai_charge }
	};
	MMOVE_T(dragon_move_melee) = { FRAME_drgslh01, FRAME_drgslh13, dragon_frames_melee, dragon_run };

	MONSTERINFO_MELEE(dragon_melee) (gentity_t* self) -> void {
		M_SetAnimation(self, &dragon_move_melee);
	}

	static void dragon_set_fly_parameters(gentity_t* self) {
		self->monsterInfo.fly_thrusters = false;
		self->monsterInfo.fly_acceleration = DRAGON_FLY_ACCEL;
		self->monsterInfo.fly_speed = DRAGON_FLY_SPEED;
		self->monsterInfo.fly_min_distance = DRAGON_FLY_MIN_DISTANCE;
		self->monsterInfo.fly_max_distance = DRAGON_FLY_MAX_DISTANCE;
	}

	static void dragon_start(gentity_t* self) {
		const spawn_temp_t& st = ED_GetSpawnTemp();

		self->monsterInfo.stand = dragon_hover;
		self->monsterInfo.walk = dragon_walk;
		self->monsterInfo.run = dragon_run;
		self->monsterInfo.attack = dragon_attack;
		self->monsterInfo.melee = dragon_melee;
		self->monsterInfo.sight = dragon_sight;
		self->monsterInfo.search = dragon_search;
		self->monsterInfo.setSkin = dragon_setskin;

		self->pain = dragon_pain;
		self->die = dragon_die;

		self->moveType = MoveType::Step;
		self->solid = SOLID_BBOX;
		self->mins = DRAGON_MINS;
		self->maxs = DRAGON_MAXS;
		self->s.modelIndex = gi.modelIndex("models/monsters/dragon/tris.md2");
		self->s.scale = DRAGON_MODEL_SCALE;

		if (self->yawSpeed == 0)
			self->yawSpeed = 20;

		int baseHealth = std::max(DRAGON_BASE_HEALTH, DRAGON_BASE_HEALTH + 1250 * (skill->integer - 1));
		self->health = self->maxHealth = baseHealth * st.health_multiplier;

		if (!st.was_key_specified("armor_type"))
			self->monsterInfo.armorType = IT_ARMOR_BODY;
		if (!st.was_key_specified("armor_power"))
			self->monsterInfo.armor_power = std::max(DRAGON_BASE_ARMOR, DRAGON_BASE_ARMOR + 150 * (skill->integer - 1));

		self->gibHealth = DRAGON_GIB_HEALTH;
		self->mass = DRAGON_MASS;

		if (coop->integer) {
			int additionalPlayers = std::max(0, CountPlayers() - 1);
			self->health += DRAGON_COOP_HEALTH_PER_PLAYER * (skill->integer + additionalPlayers);
			self->monsterInfo.armor_power += DRAGON_COOP_ARMOR_PER_PLAYER * (skill->integer + additionalPlayers);
			self->maxHealth = self->health;
		}

		self->monsterInfo.scale = DRAGON_MODEL_SCALE;
		self->monsterInfo.fly_pinned = false;
		self->monsterInfo.fly_position_time = 0_ms;

		gi.linkEntity(self);

		M_SetAnimation(self, &dragon_move_hover);
		flymonster_start(self);

		self->monsterInfo.aiFlags |= AI_ALTERNATE_FLY;
		dragon_set_fly_parameters(self);
	}

} // namespace

void SP_monster_dragon(gentity_t* self) {
	if (!M_AllowSpawn(self)) {
		FreeEntity(self);
		return;
	}

	dragon_precache();
	dragon_start(self);
}
