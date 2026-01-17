/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

============================================================================== QUAKE ZOMBIE
(Ionized port) ==============================================================================*/

#include <algorithm>

#include "../g_local.hpp"
#include "m_zombie.hpp"

void monster_think(gentity_t* self);
void zombie_run(gentity_t* self);
void zombie_resurrect_think(gentity_t* self);
void SP_misc_zombie_crucified(gentity_t* self);

namespace {

	constexpr int ZSTATE_NORMAL = 0;
	constexpr int ZSTATE_FEIGNDEAD = 1;

	static cached_soundIndex sound_idleC;
	static cached_soundIndex sound_idle;
	static cached_soundIndex sound_idle2;
	static cached_soundIndex sound_pain;
	static cached_soundIndex sound_pain2;
	static cached_soundIndex sound_fall;
	static cached_soundIndex sound_gib;
	static cached_soundIndex sound_shot;
	static cached_soundIndex sound_gib_hit;
	static cached_soundIndex sound_gib_miss;

	constexpr Vector3 zombie_alive_mins{ -16.f, -16.f, -24.f };
	constexpr Vector3 zombie_alive_maxs{ 16.f, 16.f, 32.f };
	constexpr Vector3 zombie_corpse_mins{ -16.f, -16.f, -8.f };
	constexpr Vector3 zombie_corpse_maxs{ 16.f, 16.f, 0.f };

	extern const MonsterMove zombie_move_pain5;

	static void zombie_halve_health(gentity_t* self) {
		self->maxHealth = std::max(1, self->maxHealth / 2);
		self->health = self->maxHealth;
	}

	static void zombie_fall(gentity_t* self) {
		gi.sound(self, CHAN_VOICE, sound_fall, 1, ATTN_NORM, 0);
		self->health = self->maxHealth;
	}

	TOUCH(zombie_gib_touch) (gentity_t* ent, gentity_t* other, const trace_t& tr, bool otherTouchingSelf) -> void {
		if (other == ent->owner)
			return;

		if (tr.surface && (tr.surface->flags & SURF_SKY)) {
			FreeEntity(ent);
			return;
		}

		if (other->takeDamage) {
			Vector3 dir = other->s.origin - ent->s.origin;
			Damage(other, ent, ent->owner, dir, ent->s.origin, tr.plane.normal, ent->dmg, ent->dmg, DamageFlags::Normal, MeansOfDeath{ ModID::Gekk });
			gi.sound(ent, CHAN_WEAPON | CHAN_RELIABLE, sound_gib_hit, 1.0f, ATTN_NORM, 0);
		}
		else {
			gi.sound(ent, CHAN_WEAPON | CHAN_RELIABLE, sound_gib_miss, 1.0f, ATTN_NORM, 0);

			gi.WriteByte(svc_temp_entity);
			gi.WriteByte(TE_BLOOD);
			gi.WritePosition(ent->s.origin);
			gi.WriteDir(tr.plane.normal);
			gi.multicast(ent->s.origin, MULTICAST_PVS, false);
		}

		ent->touch = nullptr;
		ent->nextThink = level.time + 3_sec;
		ent->think = FreeEntity;
	}

	static gentity_t* zombie_fire_gib_projectile(gentity_t* self, const Vector3& start, const Vector3& aimDir, int damage, int speed, float rightAdjust, float upAdjust) {
		Vector3 dir = VectorToAngles(aimDir);
		Vector3 forward, right, up;
		AngleVectors(dir, forward, right, up);

		gentity_t* gib = Spawn();
		gib->s.origin = start;
		gib->velocity = aimDir * speed;

		if (upAdjust != 0.f) {
			float gravityAdjustment = level.gravity / 800.f;
			gib->velocity += up * upAdjust * gravityAdjustment;
		}

		if (rightAdjust != 0.f)
			gib->velocity += right * rightAdjust;

		gib->moveType = MoveType::Bounce;
		gib->clipMask = MASK_PROJECTILE;
		if (self->client && !G_ShouldPlayersCollide(true))
			gib->clipMask &= ~CONTENTS_PLAYER;
		gib->solid = SOLID_BBOX;
		gib->svFlags |= SVF_PROJECTILE;
		gib->flags |= FL_DODGE;
		gib->s.effects |= EF_GIB;
		gib->speed = speed;
		gib->mins = { -6.f, -6.f, -6.f };
		gib->maxs = { 6.f, 6.f, 6.f };
		gib->aVelocity = { crandom() * 360.f, crandom() * 360.f, crandom() * 360.f };
		gib->s.modelIndex = gi.modelIndex("models/proj/zomgib/tris.md2");
		gib->owner = self;
		gib->touch = zombie_gib_touch;
		gib->nextThink = level.time + 2500_ms;
		gib->think = FreeEntity;
		gib->dmg = damage;
		gib->className = "zombie_gib";

		gi.linkEntity(gib);
		return gib;
	}

	static void zombie_idle(gentity_t* self) {
		if (frandom() > 0.8f)
			gi.sound(self, CHAN_VOICE, sound_idle, 1, ATTN_IDLE, 0);
	}

	static void zombie_idle2(gentity_t* self) {
		if (frandom() > 0.2f)
			gi.sound(self, CHAN_VOICE, sound_idle, 1, ATTN_IDLE, 0);
		else
			gi.sound(self, CHAN_VOICE, sound_idle2, 1, ATTN_IDLE, 0);
	}

	static void zombie_Cidle(gentity_t* self) {
		if (frandom() < 0.1f)
			gi.sound(self, CHAN_VOICE, sound_idleC, 1, ATTN_IDLE, 0);
	}

	static void zombie_down(gentity_t* self) {
		self->solid = SOLID_NOT;
		self->count = ZSTATE_FEIGNDEAD;
		self->svFlags |= SVF_DEADMONSTER;
		self->mins = zombie_corpse_mins;
		self->maxs = zombie_corpse_maxs;
		zombie_fall(self);
		gi.linkEntity(self);
	}

	static void zombie_idle3(gentity_t* self) {
		gi.sound(self, CHAN_VOICE, sound_idle, 1, ATTN_IDLE, 0);

		self->dmg = 0;
		self->timeStamp = GameTime{};

		GameTime delay = random_time(5_sec, 8_sec);

		trace_t tr = gi.trace(self->s.origin, zombie_alive_mins, zombie_alive_maxs, self->s.origin, self, CONTENTS_PLAYERCLIP | CONTENTS_MONSTER);
		if (tr.ent && tr.ent->solid != SOLID_BSP) {
			self->solid = SOLID_NOT;
			self->count = ZSTATE_FEIGNDEAD;
			self->s.frame = FRAME_paine11;
			delay += 5_sec;
			self->pain_debounce_time = level.time + 3_sec;
			self->mins = zombie_corpse_mins;
			self->maxs = zombie_corpse_maxs;
		}
		else {
			self->solid = SOLID_BBOX;
			self->count = ZSTATE_NORMAL;
			self->mins = zombie_alive_mins;
			self->maxs = zombie_alive_maxs;
		}

		gi.linkEntity(self);

		self->think = zombie_resurrect_think;
		self->nextThink = level.time + delay;
	}

	static void zombie_fallagain(gentity_t* self) {
		if (self->pain_debounce_time > level.time)
			M_SetAnimation(self, &zombie_move_pain5);
	}

	static void zombie_pain1(gentity_t* self) {
		gi.sound(self, CHAN_VOICE, sound_pain, 1, ATTN_NORM, 0);
		zombie_halve_health(self);
	}

	static void zombie_pain2(gentity_t* self) {
		gi.sound(self, CHAN_VOICE, sound_pain2, 1, ATTN_NORM, 0);
		zombie_halve_health(self);
	}

	static void zombie_firegib(gentity_t* self, const Vector3& offset) {
		if (!self->enemy)
			return;

		Vector3 forward, right;
		AngleVectors(self->s.angles, forward, right, nullptr);
		Vector3 start = M_ProjectFlashSource(self, offset, forward, right);

		Vector3 target = self->enemy->s.origin;
		float distance = range_to(self, self->enemy);

		if (distance <= RANGE_MELEE)
			target.z += self->enemy->viewHeight;
		else if (distance <= RANGE_NEAR) {
			target += self->enemy->velocity * -0.04f;
			target.z += self->enemy->viewHeight * 0.9f;
		}
		else if (distance <= RANGE_MID) {
			target += self->enemy->velocity * -0.08f;
			target.z += self->enemy->viewHeight * 0.8f;
		}
		else {
			target += self->enemy->velocity * -0.01f;
			target.z += frandom() * self->enemy->viewHeight;
		}

		Vector3 aim = (target - start).normalized();

		int damage = (strcmp(self->className, "monster_mummy") == 0) ? 40 : 10;

		gi.sound(self, CHAN_WEAPON | CHAN_RELIABLE, sound_shot, 1.0f, ATTN_NORM, 0);

		Vector3 pitched = aim;
		if (!M_CalculatePitchToFire(self, target, start, pitched, 500.f, 2.5f, false))
			pitched = aim;

		zombie_fire_gib_projectile(self, start, pitched, damage, 500, crandom_open() * 10.0f, frandom() * 10.0f);
	}

	static void zombie_firegib1(gentity_t* self) {
		zombie_firegib(self, { -8.f, -18.f, 30.f });
	}

	static void zombie_firegib2(gentity_t* self) {
		zombie_firegib(self, { -8.f, -18.f, 30.f });
	}

	static void zombie_firegib3(gentity_t* self) {
		zombie_firegib(self, { -8.f, 22.f, 30.f });
	}

	static MonsterFrame zombie_frames_stand[] = {
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
	MMOVE_T(zombie_move_stand) = { FRAME_stand1, FRAME_stand15, zombie_frames_stand, nullptr };

	static MonsterFrame zombie_frames_walk[] = {
			{ ai_walk },
			{ ai_walk, 2 },
			{ ai_walk, 3 },
			{ ai_walk, 2 },
			{ ai_walk, 1 },
			{ ai_walk },
			{ ai_walk },
			{ ai_walk },
			{ ai_walk },
			{ ai_walk },
			{ ai_walk, 2 },
			{ ai_walk, 2 },
			{ ai_walk, 1 },
			{ ai_walk },
			{ ai_walk },
			{ ai_walk },
			{ ai_walk },
			{ ai_walk },
			{ ai_walk, 0, zombie_idle }
	};
	MMOVE_T(zombie_move_walk) = { FRAME_walk1, FRAME_walk19, zombie_frames_walk, nullptr };

	static MonsterFrame zombie_frames_run[] = {
			{ ai_run, 1 },
			{ ai_run, 1 },
			{ ai_run },
			{ ai_run, 1 },
			{ ai_run, 2 },
			{ ai_run, 3 },
			{ ai_run, 4 },
			{ ai_run, 4 },
			{ ai_run, 2 },
			{ ai_run },
			{ ai_run },
			{ ai_run },
			{ ai_run, 2 },
			{ ai_run, 4 },
			{ ai_run, 6 },
			{ ai_run, 7 },
			{ ai_run, 3 },
			{ ai_run, 8, zombie_idle2 }
	};
	MMOVE_T(zombie_move_run) = { FRAME_run1, FRAME_run18, zombie_frames_run, nullptr };

	static MonsterFrame zombie_frames_attack1[] = {
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
			{ ai_charge, 0, zombie_firegib1 }
	};
	MMOVE_T(zombie_move_attack1) = { FRAME_atta1, FRAME_atta13, zombie_frames_attack1, zombie_run };

	static MonsterFrame zombie_frames_attack2[] = {
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
			{ ai_charge, 0, zombie_firegib2 }
	};
	MMOVE_T(zombie_move_attack2) = { FRAME_attb1, FRAME_attb14, zombie_frames_attack2, zombie_run };

	static MonsterFrame zombie_frames_attack3[] = {
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
			{ ai_charge, 0, zombie_firegib3 }
	};
	MMOVE_T(zombie_move_attack3) = { FRAME_attc1, FRAME_attc12, zombie_frames_attack3, zombie_run };

	static MonsterFrame zombie_frames_pain1[] = {
			{ ai_move, 0, zombie_pain1 },
			{ ai_move, 3 },
			{ ai_move, 1 },
			{ ai_move, -1 },
			{ ai_move, -3 },
			{ ai_move, -1 },
			{ ai_move },
			{ ai_move },
			{ ai_move },
			{ ai_move },
			{ ai_move },
			{ ai_move }
	};
	MMOVE_T(zombie_move_pain1) = { FRAME_paina1, FRAME_paina12, zombie_frames_pain1, zombie_run };

	static MonsterFrame zombie_frames_pain2[] = {
			{ ai_move, 0, zombie_pain2 },
			{ ai_move, -2 },
			{ ai_move, -8 },
			{ ai_move, -6 },
			{ ai_move, -2 },
			{ ai_move, 0 },
			{ ai_move },
			{ ai_move },
			{ ai_move },
			{ ai_move, 0, zombie_down },
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
			{ ai_move, 1 },
			{ ai_move },
			{ ai_move }
	};
	MMOVE_T(zombie_move_pain2) = { FRAME_painb1, FRAME_painb28, zombie_frames_pain2, zombie_run };

	static MonsterFrame zombie_frames_pain3[] = {
			{ ai_move, 0, zombie_pain2 },
			{ ai_move },
			{ ai_move, -3 },
			{ ai_move, -1 },
			{ ai_move },
			{ ai_move },
			{ ai_move },
			{ ai_move },
			{ ai_move },
			{ ai_move },
			{ ai_move },
			{ ai_move, 1 },
			{ ai_move, 1 },
			{ ai_move },
			{ ai_move },
			{ ai_move },
			{ ai_move },
			{ ai_move }
	};
	MMOVE_T(zombie_move_pain3) = { FRAME_painc1, FRAME_painc18, zombie_frames_pain3, zombie_run };

	static MonsterFrame zombie_frames_pain4[] = {
			{ ai_move, 0, zombie_pain1 },
			{ ai_move },
			{ ai_move },
			{ ai_move },
			{ ai_move },
			{ ai_move },
			{ ai_move },
			{ ai_move },
			{ ai_move },
			{ ai_move, -1 },
			{ ai_move },
			{ ai_move },
			{ ai_move }
	};
	MMOVE_T(zombie_move_pain4) = { FRAME_paind1, FRAME_paind13, zombie_frames_pain4, zombie_run };

	static MonsterFrame zombie_frames_pain5[] = {
			{ ai_move, 0, zombie_pain2 },
			{ ai_move, -8 },
			{ ai_move, -5 },
			{ ai_move, -3 },
			{ ai_move, -1 },
			{ ai_move, -2 },
			{ ai_move, -1 },
			{ ai_move, -1 },
			{ ai_move, -2 },
			{ ai_move, 0, zombie_down },
			{ ai_move },
			{ ai_move, 0, zombie_idle3 },
			{ ai_move, 0, zombie_fallagain },
			{ ai_move },
			{ ai_move },
			{ ai_move },
			{ ai_move },
			{ ai_move }
	};
	MMOVE_T(zombie_move_pain5) = { FRAME_paine1, FRAME_paine18, zombie_frames_pain5, zombie_run };

	static MonsterFrame zombie_frames_getup[] = {
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
	MMOVE_T(zombie_move_getup) = { FRAME_paine13, FRAME_paine30, zombie_frames_getup, zombie_run };

} // namespace

MONSTERINFO_STAND(zombie_stand) (gentity_t* self) -> void {
	self->solid = SOLID_BBOX;
	self->count = ZSTATE_NORMAL;
	M_SetAnimation(self, &zombie_move_stand);
}

MONSTERINFO_WALK(zombie_walk) (gentity_t* self) -> void {
	self->solid = SOLID_BBOX;
	self->count = ZSTATE_NORMAL;
	M_SetAnimation(self, &zombie_move_walk);
}

MONSTERINFO_RUN(zombie_run) (gentity_t* self) -> void {
	self->solid = SOLID_BBOX;
	self->count = ZSTATE_NORMAL;

	if (self->monsterInfo.aiFlags & AI_STAND_GROUND) {
		M_SetAnimation(self, &zombie_move_stand);
		return;
	}

	M_SetAnimation(self, &zombie_move_run);
}

MONSTERINFO_ATTACK(zombie_attack) (gentity_t* self) -> void {
	if (self->style)
		return;

	self->solid = SOLID_BBOX;
	self->count = ZSTATE_NORMAL;

	float r = frandom();
	if (r < 0.3f)
		M_SetAnimation(self, &zombie_move_attack1);
	else if (r < 0.6f)
		M_SetAnimation(self, &zombie_move_attack2);
	else
		M_SetAnimation(self, &zombie_move_attack3);
}

PAIN(zombie_pain) (gentity_t* self, gentity_t* other, float kick, int damage, const MeansOfDeath& mod) -> void {
	if (self->count == ZSTATE_FEIGNDEAD && self->nextThink > level.time)
		return;

	self->health = self->maxHealth;

	if (self->timeStamp > level.time) {
		self->dmg += damage;
		if (self->dmg > 30) {
			M_SetAnimation(self, &zombie_move_pain5);
			self->pain_debounce_time = level.time + 3_sec;
			self->dmg = 0;
			self->timeStamp = GameTime{};
			return;
		}
	}
	else {
		self->timeStamp = level.time + 1_sec;
		self->dmg = damage;
	}

	if (damage < 15)
		return;

	if (damage >= 25) {
		M_SetAnimation(self, &zombie_move_pain5);
		self->pain_debounce_time = level.time + 3_sec;
		return;
	}

	if (self->pain_debounce_time > level.time)
		return;

	if (!M_ShouldReactToPain(self, mod))
		return;

	float r = frandom();
	if (r < 0.25f) {
		self->pain_debounce_time = level.time + 1_sec;
		M_SetAnimation(self, &zombie_move_pain1);
	}
	else if (r < 0.5f) {
		self->pain_debounce_time = level.time + 1500_ms;
		M_SetAnimation(self, &zombie_move_pain2);
	}
	else if (r < 0.75f) {
		self->pain_debounce_time = level.time + 1100_ms;
		M_SetAnimation(self, &zombie_move_pain3);
	}
	else {
		self->pain_debounce_time = level.time + 1_sec;
		M_SetAnimation(self, &zombie_move_pain4);
	}
}

MONSTERINFO_SIGHT(zombie_sight) (gentity_t* self, gentity_t* other) -> void {
	gi.sound(self, CHAN_VOICE, sound_idle, 1, ATTN_NORM, 0);

	if (!self->style && skill->integer > 0 && self->enemy && range_to(self, self->enemy) >= RANGE_MID)
		zombie_attack(self);
}

MONSTERINFO_SEARCH(zombie_search) (gentity_t* self) -> void {
	gi.sound(self, CHAN_VOICE, sound_idle, 1, ATTN_IDLE, 0);
}

THINK(zombie_resurrect_think) (gentity_t* self) -> void {
	if (!self->inUse || self->health <= self->gibHealth)
		return;

	if (self->count == ZSTATE_FEIGNDEAD) {
		trace_t tr = gi.trace(self->s.origin, zombie_alive_mins, zombie_alive_maxs, self->s.origin, self, CONTENTS_PLAYERCLIP | CONTENTS_MONSTER);
		if (tr.ent && tr.ent->solid != SOLID_BSP) {
			self->nextThink = level.time + 1_sec;
			return;
		}
	}

	self->think = monster_think;
	self->nextThink = level.time + FRAME_TIME_S;
	self->count = ZSTATE_NORMAL;
	self->deadFlag = false;
	self->svFlags &= ~SVF_DEADMONSTER;
	self->solid = SOLID_BBOX;
	self->mins = zombie_alive_mins;
	self->maxs = zombie_alive_maxs;
	self->health = self->maxHealth;
	self->pain_debounce_time = level.time + 1_sec;

	gi.linkEntity(self);
	gi.sound(self, CHAN_VOICE, sound_idle, 1, ATTN_NORM, 0);
	M_SetAnimation(self, &zombie_move_getup);
}

DIE(zombie_die) (gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, const Vector3& point, const MeansOfDeath& mod) -> void {
	if (M_CheckGib(self, mod)) {
		gi.sound(self, CHAN_VOICE, sound_gib, 1, ATTN_NORM, 0);
		ThrowGibs(self, damage, {
				{ 2, "models/objects/gibs/bone/tris.md2" },
				{ 4, "models/objects/gibs/sm_meat/tris.md2" },
				{ "models/monsters/zombie/gibs/head.md2", GIB_HEAD }
			});
		self->deadFlag = true;
		return;
	}

	if (self->count == ZSTATE_FEIGNDEAD) {
		self->nextThink = level.time + random_time(2_sec, 4_sec);
		return;
	}

	self->deadFlag = true;
	self->takeDamage = true;
	M_SetAnimation(self, &zombie_move_pain5);
	self->pain_debounce_time = level.time + 3_sec;
}

MONSTERINFO_CHECKATTACK(mummy_checkattack) (gentity_t* self) -> bool {
	return M_CheckAttack_Base(self, 0.4f, 0.8f, 0.8f, 0.8f, 0.f, 0.f);
}

THINK(misc_zombie_crucified_think) (gentity_t* self) -> void {
	if (++self->s.frame < 198)
		self->nextThink = level.time + FRAME_TIME_S;
	else {
		self->s.frame = 192;
		self->nextThink = level.time + FRAME_TIME_S;
	}

	if (frandom() <= 0.017f)
		zombie_Cidle(self);
}

/*QUAKED misc_zombie_crucified (1 .5 0) (-16 -16 -24) (16 16 32)
model="models/monsters/q1zombie/"
frame="192"
*/
void SP_misc_zombie_crucified(gentity_t* self) {
	sound_idleC.assign("q1zombie/idle_w2.wav");

	self->moveType = MoveType::None;
	self->solid = SOLID_NOT;
	self->mins = zombie_alive_mins;
	self->maxs = zombie_alive_maxs;
	self->s.modelIndex = gi.modelIndex("models/monsters/q1zombie/tris.md2");
	self->s.frame = 192;

	gi.linkEntity(self);

	self->think = misc_zombie_crucified_think;
	self->nextThink = level.time + FRAME_TIME_S;
}

/*QUAKED monster_zombie (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight
"sounds" 1 spawns a crucified zombie
*/
void SP_monster_zombie(gentity_t* self) {
	if (!M_AllowSpawn(self)) {
		FreeEntity(self);
		return;
	}

	if (self->sounds) {
		SP_misc_zombie_crucified(self);
		return;
	}

	if (deathmatch->integer) {
		FreeEntity(self);
		return;
	}

	sound_idleC.assign("zombie/idle_w2.wav");
	sound_idle.assign("zombie/z_idle.wav");
	sound_idle2.assign("zombie/z_idle1.wav");
	sound_pain.assign("zombie/z_pain.wav");
	sound_pain2.assign("zombie/z_pain1.wav");
	sound_fall.assign("zombie/z_fall.wav");
	sound_gib.assign("zombie/z_gib.wav");
	sound_shot.assign("zombie/z_shot1.wav");
	sound_gib_hit.assign("q1zombie/z_hit.wav");
	sound_gib_miss.assign("q1zombie/z_miss.wav");

	gi.modelIndex("models/proj/zomgib/tris.md2");
	gi.modelIndex("models/objects/gibs/bone/tris.md2");
	gi.modelIndex("models/objects/gibs/sm_meat/tris.md2");
	gi.modelIndex("models/monsters/zombie/gibs/head.md2");

	self->moveType = MoveType::Step;
	self->solid = SOLID_BBOX;
	self->s.modelIndex = gi.modelIndex("models/monsters/zombie/tris.md2");
	self->mins = zombie_alive_mins;
	self->maxs = zombie_alive_maxs;

	self->health = self->maxHealth = 60 * st.health_multiplier;
	self->gibHealth = 0;
	self->mass = 100;
	self->count = ZSTATE_NORMAL;

	self->pain = zombie_pain;
	self->die = zombie_die;

	self->monsterInfo.stand = zombie_stand;
	self->monsterInfo.walk = zombie_walk;
	self->monsterInfo.run = zombie_run;
	self->monsterInfo.attack = zombie_attack;
	self->monsterInfo.melee = nullptr;
	self->monsterInfo.sight = zombie_sight;
	self->monsterInfo.search = zombie_search;

	gi.linkEntity(self);

	M_SetAnimation(self, &zombie_move_stand);
	walkmonster_start(self);
	self->monsterInfo.scale = MODEL_SCALE;
}

/*QUAKED monster_mummy (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight
model="models/monsters/zombie/tris.md2"
*/
void SP_monster_mummy(gentity_t* self) {
	SP_monster_zombie(self);

	if (!self->inUse)
		return;

	self->s.skinNum = 1;
	self->health = std::max(1500, 1500 + 1000 * (skill->integer - 1)) * st.health_multiplier;
	if (!st.was_key_specified("armor_type"))
		self->monsterInfo.armorType = IT_ARMOR_BODY;
	if (!st.was_key_specified("armor_power"))
		self->monsterInfo.armor_power = std::max(350, 350 + 100 * (skill->integer - 1));
	if (coop->integer) {
		int additionalPlayers = std::max(0, CountPlayers() - 1);
		self->health += 250 * additionalPlayers;
		self->monsterInfo.armor_power += 100 * additionalPlayers;
	}
	self->maxHealth = self->health;

	self->monsterInfo.checkAttack = mummy_checkattack;
}
