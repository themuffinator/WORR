/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

fixbot.c*/

#include "../g_local.hpp"
#include "m_fixbot.hpp"
#include "m_flash.hpp"

bool infront(gentity_t *self, gentity_t *other);
bool FindTarget(gentity_t *self);

static cached_soundIndex sound_pain1;
static cached_soundIndex sound_die;
static cached_soundIndex sound_weld1;
static cached_soundIndex sound_weld2;
static cached_soundIndex sound_weld3;

void fixbot_run(gentity_t *self);
void fixbot_attack(gentity_t *self);
void fixbot_dead(gentity_t *self);
void fixbot_fire_blaster(gentity_t *self);
void fixbot_fire_welder(gentity_t *self);

void use_scanner(gentity_t *self);
void change_to_roam(gentity_t *self);
void fly_vertical(gentity_t *self);

void fixbot_stand(gentity_t *self);

extern const MonsterMove fixbot_move_forward;
extern const MonsterMove fixbot_move_stand;
extern const MonsterMove fixbot_move_stand2;
extern const MonsterMove fixbot_move_roamgoal;

extern const MonsterMove fixbot_move_weld_start;
extern const MonsterMove fixbot_move_weld;
extern const MonsterMove fixbot_move_weld_end;
extern const MonsterMove fixbot_move_takeoff;
extern const MonsterMove fixbot_move_landing;
extern const MonsterMove fixbot_move_turn;

void roam_goal(gentity_t *self);

// [Paril-KEX] clean up bot goals if we get interrupted
static THINK(bot_goal_check) (gentity_t *self) -> void {
	if (!self->owner || !self->owner->inUse || self->owner->goalEntity != self) {
		FreeEntity(self);
		return;
	}

	self->nextThink = level.time + 1_ms;
}

void ED_CallSpawn(gentity_t *ent);

static gentity_t *fixbot_FindDeadMonster(gentity_t *self) {
	gentity_t *ent = nullptr;
	gentity_t *best = nullptr;

	while ((ent = FindRadius(ent, self->s.origin, 1024)) != nullptr) {
		if (ent == self)
			continue;
		if (!(ent->svFlags & SVF_MONSTER))
			continue;
		if (ent->monsterInfo.aiFlags & AI_GOOD_GUY)
			continue;
		// check to make sure we haven't bailed on this guy already
		if ((ent->monsterInfo.badMedic1 == self) || (ent->monsterInfo.badMedic2 == self))
			continue;
		if (ent->monsterInfo.healer)
			// FIXME - this is correcting a bug that is somewhere else
			// if the healer is a monster, and it's in medic mode .. continue .. otherwise
			//   we will override the healer, if it passes all the other tests
			if ((ent->monsterInfo.healer->inUse) && (ent->monsterInfo.healer->health > 0) &&
				(ent->monsterInfo.healer->svFlags & SVF_MONSTER) && (ent->monsterInfo.healer->monsterInfo.aiFlags & AI_MEDIC))
				continue;
		if (ent->health > 0)
			continue;
		if ((ent->nextThink) && (ent->think != monster_dead_think))
			continue;
		if (!visible(self, ent))
			continue;
		if (!best) {
			best = ent;
			continue;
		}
		if (ent->maxHealth <= best->maxHealth)
			continue;
		best = ent;
	}

	return best;
}

static void fixbot_set_fly_parameters(gentity_t *self, bool heal, bool weld) {
	self->monsterInfo.fly_position_time = 0_ms;
	self->monsterInfo.fly_acceleration = 5.f;
	self->monsterInfo.fly_speed = 110.f;
	self->monsterInfo.fly_buzzard = false;

	if (heal) {
		self->monsterInfo.fly_min_distance = 100.f;
		self->monsterInfo.fly_max_distance = 100.f;
		self->monsterInfo.fly_thrusters = true;
	} else if (weld) {
		self->monsterInfo.fly_min_distance = 24.f;
		self->monsterInfo.fly_max_distance = 24.f;
	} else {
		// timid bot
		self->monsterInfo.fly_min_distance = 300.f;
		self->monsterInfo.fly_max_distance = 500.f;
	}
}

static int fixbot_search(gentity_t *self) {
	gentity_t *ent;

	if (!self->enemy) {
		ent = fixbot_FindDeadMonster(self);
		if (ent) {
			self->oldEnemy = self->enemy;
			self->enemy = ent;
			self->enemy->monsterInfo.healer = self;
			self->monsterInfo.aiFlags |= AI_MEDIC;
			FoundTarget(self);
			fixbot_set_fly_parameters(self, true, false);
			return (1);
		}
	}
	return (0);
}

static void landing_goal(gentity_t *self) {
	trace_t	 tr;
	Vector3	 forward, right, up;
	Vector3	 end;
	gentity_t *ent;

	ent = Spawn();
	ent->className = "bot_goal";
	ent->solid = SOLID_BBOX;
	ent->owner = self;
	ent->think = bot_goal_check;
	gi.linkEntity(ent);

	ent->mins = { -32, -32, -24 };
	ent->maxs = { 32, 32, 24 };

	AngleVectors(self->s.angles, forward, right, up);
	end = self->s.origin + (forward * 32);
	end = self->s.origin + (up * -8096);

	tr = gi.trace(self->s.origin, ent->mins, ent->maxs, end, self, MASK_MONSTERSOLID);

	ent->s.origin = tr.endPos;

	self->goalEntity = self->enemy = ent;
	M_SetAnimation(self, &fixbot_move_landing);
}

static void takeoff_goal(gentity_t *self) {
	trace_t	 tr;
	Vector3	 forward, right, up;
	Vector3	 end;
	gentity_t *ent;

	ent = Spawn();
	ent->className = "bot_goal";
	ent->solid = SOLID_BBOX;
	ent->owner = self;
	ent->think = bot_goal_check;
	gi.linkEntity(ent);

	ent->mins = { -32, -32, -24 };
	ent->maxs = { 32, 32, 24 };

	AngleVectors(self->s.angles, forward, right, up);
	end = self->s.origin + (forward * 32);
	end = self->s.origin + (up * 128);

	tr = gi.trace(self->s.origin, ent->mins, ent->maxs, end, self, MASK_MONSTERSOLID);

	ent->s.origin = tr.endPos;

	self->goalEntity = self->enemy = ent;
	M_SetAnimation(self, &fixbot_move_takeoff);
}

void change_to_roam(gentity_t *self) {

	if (fixbot_search(self))
		return;

	M_SetAnimation(self, &fixbot_move_roamgoal);

	if (self->spawnFlags.has(SPAWNFLAG_FIXBOT_LANDING)) {
		landing_goal(self);
		M_SetAnimation(self, &fixbot_move_landing);
		self->spawnFlags &= ~SPAWNFLAG_FIXBOT_LANDING;
		self->spawnFlags = SPAWNFLAG_FIXBOT_WORKING;
	}
	if (self->spawnFlags.has(SPAWNFLAG_FIXBOT_TAKEOFF)) {
		takeoff_goal(self);
		M_SetAnimation(self, &fixbot_move_takeoff);
		self->spawnFlags &= ~SPAWNFLAG_FIXBOT_TAKEOFF;
		self->spawnFlags = SPAWNFLAG_FIXBOT_WORKING;
	}
	if (self->spawnFlags.has(SPAWNFLAG_FIXBOT_FIXIT)) {
		M_SetAnimation(self, &fixbot_move_roamgoal);
		self->spawnFlags &= ~SPAWNFLAG_FIXBOT_FIXIT;
		self->spawnFlags = SPAWNFLAG_FIXBOT_WORKING;
	}
	if (!self->spawnFlags) {
		M_SetAnimation(self, &fixbot_move_stand2);
	}
}

void roam_goal(gentity_t *self) {

	trace_t	 tr;
	Vector3	 forward, right, up;
	Vector3	 end;
	gentity_t *ent;
	Vector3	 dang;
	float	 len, oldlen;
	int		 i;
	Vector3	 vec;
	Vector3	 whichvec{};

	ent = Spawn();
	ent->className = "bot_goal";
	ent->solid = SOLID_BBOX;
	ent->owner = self;
	ent->think = bot_goal_check;
	ent->nextThink = level.time + 1_ms;
	gi.linkEntity(ent);

	oldlen = 0;

	for (i = 0; i < 12; i++) {

		dang = self->s.angles;

		if (i < 6)
			dang[YAW] += 30 * i;
		else
			dang[YAW] -= 30 * (i - 6);

		AngleVectors(dang, forward, right, up);
		end = self->s.origin + (forward * 8192);

		tr = gi.traceLine(self->s.origin, end, self, MASK_PROJECTILE);

		vec = self->s.origin - tr.endPos;
		len = vec.normalize();

		if (len > oldlen) {
			oldlen = len;
			whichvec = tr.endPos;
		}
	}

	ent->s.origin = whichvec;
	self->goalEntity = self->enemy = ent;

	M_SetAnimation(self, &fixbot_move_turn);
}

void use_scanner(gentity_t *self) {
	gentity_t *ent = nullptr;

	float  radius = 1024;
	Vector3 vec;

	float len;

	while ((ent = FindRadius(ent, self->s.origin, radius)) != nullptr) {
		if (ent->health >= 100) {
			if (strcmp(ent->className, "object_repair") == 0) {
				if (visible(self, ent)) {
					// remove the old one
					if (strcmp(self->goalEntity->className, "bot_goal") == 0) {
						self->goalEntity->nextThink = level.time + 100_ms;
						self->goalEntity->think = FreeEntity;
					}

					self->goalEntity = self->enemy = ent;

					vec = self->s.origin - self->goalEntity->s.origin;
					len = vec.normalize();

					fixbot_set_fly_parameters(self, false, true);

					if (len < 32) {
						M_SetAnimation(self, &fixbot_move_weld_start);
						return;
					}
					return;
				}
			}
		}
	}

	if (!self->goalEntity) {
		M_SetAnimation(self, &fixbot_move_stand);
		return;
	}

	vec = self->s.origin - self->goalEntity->s.origin;
	len = vec.length();

	if (len < 32) {
		if (strcmp(self->goalEntity->className, "object_repair") == 0) {
			M_SetAnimation(self, &fixbot_move_weld_start);
		} else {
			self->goalEntity->nextThink = level.time + 100_ms;
			self->goalEntity->think = FreeEntity;
			self->goalEntity = self->enemy = nullptr;
			M_SetAnimation(self, &fixbot_move_stand);
		}
		return;
	}

	vec = self->s.origin - self->s.oldOrigin;
	len = vec.length();

	/*
	  bot is stuck get new goalEntity
	*/
	if (len == 0) {
		if (strcmp(self->goalEntity->className, "object_repair") == 0) {
			M_SetAnimation(self, &fixbot_move_stand);
		} else {
			self->goalEntity->nextThink = level.time + 100_ms;
			self->goalEntity->think = FreeEntity;
			self->goalEntity = self->enemy = nullptr;
			M_SetAnimation(self, &fixbot_move_stand);
		}
	}
}

/*
	when the bot has found a landing pad
	it will proceed to its goalEntity
	just above the landing pad and
	decend translated along the z the current
	frames are at 10fps
*/
static void blastoff(gentity_t *self, const Vector3 &start, const Vector3 &aimDir, int damage, int kick, int te_impact, int hSpread, int vSpread) {
	trace_t	   tr;
	Vector3	   dir;
	Vector3	   forward, right, up;
	Vector3	   end;
	float	   r;
	float	   u;
	Vector3	   water_start;
	bool	   water = false;
	contents_t content_mask = MASK_PROJECTILE | MASK_WATER;

	hSpread += (self->s.frame - FRAME_takeoff_01);
	vSpread += (self->s.frame - FRAME_takeoff_01);

	tr = gi.traceLine(self->s.origin, start, self, MASK_PROJECTILE);
	if (!(tr.fraction < 1.0f)) {
		dir = VectorToAngles(aimDir);
		AngleVectors(dir, forward, right, up);

		r = crandom() * hSpread;
		u = crandom() * vSpread;
		end = start + (forward * 8192);
		end += (right * r);
		end += (up * u);

		if (gi.pointContents(start) & MASK_WATER) {
			water = true;
			water_start = start;
			content_mask &= ~MASK_WATER;
		}

		tr = gi.traceLine(start, end, self, content_mask);

		// see if we hit water
		if (tr.contents & MASK_WATER) {
			int color;

			water = true;
			water_start = tr.endPos;

			if (start != tr.endPos) {
				if (tr.contents & CONTENTS_WATER) {
					if (strcmp(tr.surface->name, "*brwater") == 0)
						color = SPLASH_BROWN_WATER;
					else
						color = SPLASH_BLUE_WATER;
				} else if (tr.contents & CONTENTS_SLIME)
					color = SPLASH_SLIME;
				else if (tr.contents & CONTENTS_LAVA)
					color = SPLASH_LAVA;
				else
					color = SPLASH_UNKNOWN;

				if (color != SPLASH_UNKNOWN) {
					gi.WriteByte(svc_temp_entity);
					gi.WriteByte(TE_SPLASH);
					gi.WriteByte(8);
					gi.WritePosition(tr.endPos);
					gi.WriteDir(tr.plane.normal);
					gi.WriteByte(color);
					gi.multicast(tr.endPos, MULTICAST_PVS, false);
				}

				// change bullet's course when it enters water
				dir = end - start;
				dir = VectorToAngles(dir);
				AngleVectors(dir, forward, right, up);
				r = crandom() * hSpread * 2;
				u = crandom() * vSpread * 2;
				end = water_start + (forward * 8192);
				end += (right * r);
				end += (up * u);
			}

			// re-trace ignoring water this time
			tr = gi.traceLine(water_start, end, self, MASK_PROJECTILE);
		}
	}

	// send gun puff / flash
	if (!((tr.surface) && (tr.surface->flags & SURF_SKY))) {
		if (tr.fraction < 1.0f) {
			if (tr.ent->takeDamage) {
				Damage(tr.ent, self, self, aimDir, tr.endPos, tr.plane.normal, damage, kick, DamageFlags::Bullet, ModID::BlastOff);
			} else {
				if (!(tr.surface->flags & SURF_SKY)) {
					gi.WriteByte(svc_temp_entity);
					gi.WriteByte(te_impact);
					gi.WritePosition(tr.endPos);
					gi.WriteDir(tr.plane.normal);
					gi.multicast(tr.endPos, MULTICAST_PVS, false);

					if (self->client)
						G_PlayerNoise(self, tr.endPos, PlayerNoise::Impact);
				}
			}
		}
	}

	// if went through water, determine where the end and make a bubble trail
	if (water) {
		Vector3 pos;

		dir = tr.endPos - water_start;
		dir.normalize();
		pos = tr.endPos + (dir * -2);
		if (gi.pointContents(pos) & MASK_WATER)
			tr.endPos = pos;
		else
			tr = gi.traceLine(pos, water_start, tr.ent, MASK_WATER);

		pos = water_start + tr.endPos;
		pos *= 0.5f;

		gi.WriteByte(svc_temp_entity);
		gi.WriteByte(TE_BUBBLETRAIL);
		gi.WritePosition(water_start);
		gi.WritePosition(tr.endPos);
		gi.multicast(pos, MULTICAST_PVS, false);
	}
}

void fly_vertical(gentity_t *self) {
	int	   i;
	Vector3 v;
	Vector3 forward, right, up;
	Vector3 start;
	Vector3 tempvec;

	v = self->goalEntity->s.origin - self->s.origin;
	self->ideal_yaw = vectoyaw(v);
	M_ChangeYaw(self);

	if (self->s.frame == FRAME_landing_58 || self->s.frame == FRAME_takeoff_16) {
		self->goalEntity->nextThink = level.time + 100_ms;
		self->goalEntity->think = FreeEntity;
		M_SetAnimation(self, &fixbot_move_stand);
		self->goalEntity = self->enemy = nullptr;
	}

	// kick up some particles
	tempvec = self->s.angles;
	tempvec[PITCH] += 90;

	AngleVectors(tempvec, forward, right, up);
	start = self->s.origin;

	for (i = 0; i < 10; i++)
		blastoff(self, start, forward, 2, 1, TE_SHOTGUN, DEFAULT_SHOTGUN_HSPREAD, DEFAULT_SHOTGUN_VSPREAD);

	// needs sound
}

static void fly_vertical2(gentity_t *self) {
	Vector3 v;
	float  len;

	v = self->goalEntity->s.origin - self->s.origin;
	len = v.length();
	self->ideal_yaw = vectoyaw(v);
	M_ChangeYaw(self);

	if (len < 32) {
		self->goalEntity->nextThink = level.time + 100_ms;
		self->goalEntity->think = FreeEntity;
		M_SetAnimation(self, &fixbot_move_stand);
		self->goalEntity = self->enemy = nullptr;
	}

	// needs sound
}

MonsterFrame fixbot_frames_landing[] = {
	{ ai_move },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },

	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },

	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },

	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },

	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },

	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 },
	{ ai_move, 0, fly_vertical2 }
};
MMOVE_T(fixbot_move_landing) = { FRAME_landing_01, FRAME_landing_58, fixbot_frames_landing, nullptr };

/*
	generic ambient stand
*/
MonsterFrame fixbot_frames_stand[] = {
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
	{ ai_move, 0, change_to_roam }

};
MMOVE_T(fixbot_move_stand) = { FRAME_ambient_01, FRAME_ambient_19, fixbot_frames_stand, nullptr };

MonsterFrame fixbot_frames_stand2[] = {
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
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand },
	{ ai_stand, 0, change_to_roam }
};
MMOVE_T(fixbot_move_stand2) = { FRAME_ambient_01, FRAME_ambient_19, fixbot_frames_stand2, nullptr };

/*
	generic frame to move bot
*/
MonsterFrame fixbot_frames_roamgoal[] = {
	{ ai_move, 0, roam_goal }
};
MMOVE_T(fixbot_move_roamgoal) = { FRAME_freeze_01, FRAME_freeze_01, fixbot_frames_roamgoal, nullptr };

static void ai_facing(gentity_t *self, float dist) {
	if (!self->goalEntity) {
		fixbot_stand(self);
		return;
	}

	Vector3 v;

	if (infront(self, self->goalEntity))
		M_SetAnimation(self, &fixbot_move_forward);
	else {
		v = self->goalEntity->s.origin - self->s.origin;
		self->ideal_yaw = vectoyaw(v);
		M_ChangeYaw(self);
	}
};

MonsterFrame fixbot_frames_turn[] = {
	{ ai_facing }
};
MMOVE_T(fixbot_move_turn) = { FRAME_freeze_01, FRAME_freeze_01, fixbot_frames_turn, nullptr };

[[maybe_unused]] static void go_roam(gentity_t *self) {
	M_SetAnimation(self, &fixbot_move_stand);
}

/*
	takeoff
*/
MonsterFrame fixbot_frames_takeoff[] = {
	{ ai_move, 0.01f, fly_vertical },
	{ ai_move, 0.01f, fly_vertical },
	{ ai_move, 0.01f, fly_vertical },
	{ ai_move, 0.01f, fly_vertical },
	{ ai_move, 0.01f, fly_vertical },
	{ ai_move, 0.01f, fly_vertical },
	{ ai_move, 0.01f, fly_vertical },
	{ ai_move, 0.01f, fly_vertical },
	{ ai_move, 0.01f, fly_vertical },
	{ ai_move, 0.01f, fly_vertical },

	{ ai_move, 0.01f, fly_vertical },
	{ ai_move, 0.01f, fly_vertical },
	{ ai_move, 0.01f, fly_vertical },
	{ ai_move, 0.01f, fly_vertical },
	{ ai_move, 0.01f, fly_vertical },
	{ ai_move, 0.01f, fly_vertical }
};
MMOVE_T(fixbot_move_takeoff) = { FRAME_takeoff_01, FRAME_takeoff_16, fixbot_frames_takeoff, nullptr };

/* findout what this is */
MonsterFrame fixbot_frames_paina[] = {
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move }
};
MMOVE_T(fixbot_move_paina) = { FRAME_paina_01, FRAME_paina_06, fixbot_frames_paina, fixbot_run };

/* findout what this is */
MonsterFrame fixbot_frames_painb[] = {
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move },
	{ ai_move }
};
MMOVE_T(fixbot_move_painb) = { FRAME_painb_01, FRAME_painb_08, fixbot_frames_painb, fixbot_run };

/*
	backup from pain
	call a generic painsound
	some spark effects
*/
MonsterFrame fixbot_frames_pain3[] = {
	{ ai_move, -1 }
};
MMOVE_T(fixbot_move_pain3) = { FRAME_freeze_01, FRAME_freeze_01, fixbot_frames_pain3, fixbot_run };

void M_MoveToGoal(gentity_t *ent, float dist);

static void ai_movetogoal(gentity_t *self, float dist) {
	M_MoveToGoal(self, dist);
}
/*

*/
MonsterFrame fixbot_frames_forward[] = {
	{ ai_movetogoal, 5, use_scanner }
};
MMOVE_T(fixbot_move_forward) = { FRAME_freeze_01, FRAME_freeze_01, fixbot_frames_forward, nullptr };

/*

*/
MonsterFrame fixbot_frames_walk[] = {
	{ ai_walk, 5 }
};
MMOVE_T(fixbot_move_walk) = { FRAME_freeze_01, FRAME_freeze_01, fixbot_frames_walk, nullptr };

/*

*/
MonsterFrame fixbot_frames_run[] = {
	{ ai_run, 10 }
};
MMOVE_T(fixbot_move_run) = { FRAME_freeze_01, FRAME_freeze_01, fixbot_frames_run, nullptr };

//
MonsterFrame fixbot_frames_start_attack[] = {
	{ ai_charge }
};
MMOVE_T(fixbot_move_start_attack) = { FRAME_freeze_01, FRAME_freeze_01, fixbot_frames_start_attack, fixbot_attack };

void abortHeal(gentity_t *self, bool change_frame, bool gib, bool mark);

PRETHINK(fixbot_laser_update) (gentity_t *laser) -> void {
	gentity_t *self = laser->owner;

	Vector3 start, dir;
	AngleVectors(self->s.angles, dir, nullptr, nullptr);
	start = self->s.origin + (dir * 16);

	if (self->enemy && self->health > 0) {
		Vector3 point;
		point = (self->enemy->absMin + self->enemy->absMax) * 0.5f;
		if (self->monsterInfo.aiFlags & AI_MEDIC)
			point[0] += std::sinf(level.time.seconds()) * 8;
		dir = point - self->s.origin;
		dir.normalize();
	}

	laser->s.origin = start;
	laser->moveDir = dir;
	gi.linkEntity(laser);
	dabeam_update(laser, true);
}

static void fixbot_fire_laser(gentity_t *self) {
	// critter dun got blown up while bein' fixed
	if (!self->enemy || !self->enemy->inUse || self->enemy->health <= self->enemy->gibHealth) {
		M_SetAnimation(self, &fixbot_move_stand);
		self->monsterInfo.aiFlags &= ~AI_MEDIC;
		return;
	}

	monster_fire_dabeam(self, -1, false, fixbot_laser_update);

	if (self->enemy->health > (self->enemy->mass / 10)) {
		Vector3 maxs;
		self->enemy->spawnFlags = SPAWNFLAG_NONE;
		self->enemy->monsterInfo.aiFlags &= AI_STINKY | AI_SPAWNED_MASK;
		self->enemy->target = nullptr;
		self->enemy->targetName = nullptr;
		self->enemy->combatTarget = nullptr;
		self->enemy->deathTarget = nullptr;
		self->enemy->healthTarget = nullptr;
		self->enemy->itemTarget = nullptr;
		self->enemy->monsterInfo.healer = self;

		maxs = self->enemy->maxs;
		maxs[2] += 48; // compensate for change when they die

		trace_t tr = gi.trace(self->enemy->s.origin, self->enemy->mins, maxs, self->enemy->s.origin, self->enemy, MASK_MONSTERSOLID);
		if (tr.startSolid || tr.allSolid) {
			abortHeal(self, false, true, false);
			return;
		} else if (tr.ent != world) {
			abortHeal(self, false, true, false);
			return;
		} else {
			self->enemy->monsterInfo.aiFlags |= AI_IGNORE_SHOTS | AI_DO_NOT_COUNT;

			// backup & restore health stuff, because of multipliers
			int32_t old_max_health = self->enemy->maxHealth;
			item_id_t old_power_armor_type = self->enemy->monsterInfo.initialPowerArmorType;
			int32_t old_power_armor_power = self->enemy->monsterInfo.max_power_armor_power;
			int32_t old_base_health = self->enemy->monsterInfo.base_health;
			int32_t old_health_scaling = self->enemy->monsterInfo.health_scaling;
			auto reinforcements = self->enemy->monsterInfo.reinforcements;
			int32_t monster_slots = self->enemy->monsterInfo.monster_slots;
			int32_t monster_used = self->enemy->monsterInfo.monster_used;
			int32_t old_gib_health = self->enemy->gibHealth;

			st = {};
			st.keys_specified.emplace("reinforcements");
			st.reinforcements = "";

			ED_CallSpawn(self->enemy);

			self->enemy->monsterInfo.reinforcements = reinforcements;
			self->enemy->monsterInfo.monster_slots = monster_slots;
			self->enemy->monsterInfo.monster_used = monster_used;

			self->enemy->gibHealth = old_gib_health / 2;
			self->enemy->health = self->enemy->maxHealth = old_max_health;
			self->enemy->monsterInfo.powerArmorPower = self->enemy->monsterInfo.max_power_armor_power = old_power_armor_power;
			self->enemy->monsterInfo.powerArmorType = self->enemy->monsterInfo.initialPowerArmorType = old_power_armor_type;
			self->enemy->monsterInfo.base_health = old_base_health;
			self->enemy->monsterInfo.health_scaling = old_health_scaling;

			if (self->enemy->monsterInfo.setSkin)
				self->enemy->monsterInfo.setSkin(self->enemy);

			if (self->enemy->think) {
				self->enemy->nextThink = level.time;
				self->enemy->think(self->enemy);
			}
			self->enemy->monsterInfo.aiFlags &= ~AI_RESURRECTING;
			self->enemy->monsterInfo.aiFlags |= AI_IGNORE_SHOTS | AI_DO_NOT_COUNT;
			// turn off flies
			self->enemy->s.effects &= ~EF_FLIES;
			self->enemy->monsterInfo.healer = nullptr;

			// clean up target, if we have one and it's legit
			if (self->enemy && self->enemy->inUse) {
				M_CleanupHealTarget(self->enemy);

				if ((self->oldEnemy) && (self->oldEnemy->inUse) && (self->oldEnemy->health > 0)) {
					self->enemy->enemy = self->oldEnemy;
					FoundTarget(self->enemy);
				} else {
					self->enemy->enemy = nullptr;
					if (!FindTarget(self->enemy)) {
						// no valid enemy, so stop acting
						self->enemy->monsterInfo.pauseTime = HOLD_FOREVER;
						self->enemy->monsterInfo.stand(self->enemy);
					}
					self->enemy = nullptr;
					self->oldEnemy = nullptr;
					if (!FindTarget(self)) {
						// no valid enemy, so stop acting
						self->monsterInfo.pauseTime = HOLD_FOREVER;
						self->monsterInfo.stand(self);
						return;
					}
				}
			}
		}

		M_SetAnimation(self, &fixbot_move_stand);
	} else
		self->enemy->monsterInfo.aiFlags |= AI_RESURRECTING;
}

MonsterFrame fixbot_frames_laserattack[] = {
	{ ai_charge, 0, fixbot_fire_laser },
	{ ai_charge, 0, fixbot_fire_laser },
	{ ai_charge, 0, fixbot_fire_laser },
	{ ai_charge, 0, fixbot_fire_laser },
	{ ai_charge, 0, fixbot_fire_laser },
	{ ai_charge, 0, fixbot_fire_laser }
};
MMOVE_T(fixbot_move_laserattack) = { FRAME_shoot_01, FRAME_shoot_06, fixbot_frames_laserattack, nullptr };

/*
	need to get forward translation data
	for the charge attack
*/
MonsterFrame fixbot_frames_attack2[] = {
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

	{ ai_charge, -10 },
	{ ai_charge, -10 },
	{ ai_charge, -10 },
	{ ai_charge, -10 },
	{ ai_charge, -10 },
	{ ai_charge, -10 },
	{ ai_charge, -10 },
	{ ai_charge, -10 },
	{ ai_charge, -10 },
	{ ai_charge, -10 },

	{ ai_charge, 0, fixbot_fire_blaster },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },
	{ ai_charge },

	{ ai_charge }
};
MMOVE_T(fixbot_move_attack2) = { FRAME_charging_01, FRAME_charging_31, fixbot_frames_attack2, fixbot_run };

static void weldstate(gentity_t *self) {
	if (self->s.frame == FRAME_weldstart_10)
		M_SetAnimation(self, &fixbot_move_weld);
	else if (self->goalEntity && self->s.frame == FRAME_weldmiddle_07) {
		if (self->goalEntity->health <= 0) {
			self->enemy->owner = nullptr;
			M_SetAnimation(self, &fixbot_move_weld_end);
		} else
			self->goalEntity->health -= 10;
	} else {
		self->goalEntity = self->enemy = nullptr;
		M_SetAnimation(self, &fixbot_move_stand);
	}
}

static void ai_move2(gentity_t *self, float dist) {
	if (!self->goalEntity) {
		fixbot_stand(self);
		return;
	}

	Vector3 v;

	M_walkmove(self, self->s.angles[YAW], dist);

	v = self->goalEntity->s.origin - self->s.origin;
	self->ideal_yaw = vectoyaw(v);
	M_ChangeYaw(self);
};

MonsterFrame fixbot_frames_weld_start[] = {
	{ ai_move2, 0 },
	{ ai_move2, 0 },
	{ ai_move2, 0 },
	{ ai_move2, 0 },
	{ ai_move2, 0 },
	{ ai_move2, 0 },
	{ ai_move2, 0 },
	{ ai_move2, 0 },
	{ ai_move2, 0 },
	{ ai_move2, 0, weldstate }
};
MMOVE_T(fixbot_move_weld_start) = { FRAME_weldstart_01, FRAME_weldstart_10, fixbot_frames_weld_start, nullptr };

MonsterFrame fixbot_frames_weld[] = {
	{ ai_move2, 0, fixbot_fire_welder },
	{ ai_move2, 0, fixbot_fire_welder },
	{ ai_move2, 0, fixbot_fire_welder },
	{ ai_move2, 0, fixbot_fire_welder },
	{ ai_move2, 0, fixbot_fire_welder },
	{ ai_move2, 0, fixbot_fire_welder },
	{ ai_move2, 0, weldstate }
};
MMOVE_T(fixbot_move_weld) = { FRAME_weldmiddle_01, FRAME_weldmiddle_07, fixbot_frames_weld, nullptr };

MonsterFrame fixbot_frames_weld_end[] = {
	{ ai_move2, -2 },
	{ ai_move2, -2 },
	{ ai_move2, -2 },
	{ ai_move2, -2 },
	{ ai_move2, -2 },
	{ ai_move2, -2 },
	{ ai_move2, -2, weldstate }
};
MMOVE_T(fixbot_move_weld_end) = { FRAME_weldend_01, FRAME_weldend_07, fixbot_frames_weld_end, nullptr };

void fixbot_fire_welder(gentity_t *self) {
	Vector3 start;
	Vector3 forward, right, up;
	Vector3 end;
	Vector3 dir;
	Vector3 vec{};
	float  r;

	if (!self->enemy)
		return;

	vec[0] = 24.0;
	vec[1] = -0.8f;
	vec[2] = -10.0;

	AngleVectors(self->s.angles, forward, right, up);
	start = M_ProjectFlashSource(self, vec, forward, right);

	end = self->enemy->s.origin;

	dir = end - start;

	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_WELDING_SPARKS);
	gi.WriteByte(10);
	gi.WritePosition(start);
	gi.WriteDir(vec3_origin);
	gi.WriteByte(irandom(0xe0, 0xe8));
	gi.multicast(self->s.origin, MULTICAST_PVS, false);

	if (frandom() > 0.8f) {
		r = frandom();

		if (r < 0.33f)
			gi.sound(self, CHAN_VOICE, sound_weld1, 1, ATTN_IDLE, 0);
		else if (r < 0.66f)
			gi.sound(self, CHAN_VOICE, sound_weld2, 1, ATTN_IDLE, 0);
		else
			gi.sound(self, CHAN_VOICE, sound_weld3, 1, ATTN_IDLE, 0);
	}
}

void fixbot_fire_blaster(gentity_t *self) {
	Vector3 start;
	Vector3 forward, right, up;
	Vector3 end;
	Vector3 dir;

	if (!visible(self, self->enemy)) {
		M_SetAnimation(self, &fixbot_move_run);
	}

	AngleVectors(self->s.angles, forward, right, up);
	start = M_ProjectFlashSource(self, monster_flash_offset[MZ2_HOVER_BLASTER_1], forward, right);

	end = self->enemy->s.origin;
	end[2] += self->enemy->viewHeight;
	dir = end - start;
	dir.normalize();

	monster_fire_blaster(self, start, dir, 15, 1000, MZ2_HOVER_BLASTER_1, EF_BLASTER);
}

MONSTERINFO_STAND(fixbot_stand) (gentity_t *self) -> void {
	M_SetAnimation(self, &fixbot_move_stand);
}

MONSTERINFO_RUN(fixbot_run) (gentity_t *self) -> void {
	if (self->monsterInfo.aiFlags & AI_STAND_GROUND)
		M_SetAnimation(self, &fixbot_move_stand);
	else
		M_SetAnimation(self, &fixbot_move_run);
}

MONSTERINFO_WALK(fixbot_walk) (gentity_t *self) -> void {
	Vector3 vec;
	float  len;

	if (self->goalEntity && strcmp(self->goalEntity->className, "object_repair") == 0) {
		vec = self->s.origin - self->goalEntity->s.origin;
		len = vec.length();
		if (len < 32) {
			M_SetAnimation(self, &fixbot_move_weld_start);
			return;
		}
	}
	M_SetAnimation(self, &fixbot_move_walk);
}

[[maybe_unused]] static void fixbot_start_attack(gentity_t *self) {
	M_SetAnimation(self, &fixbot_move_start_attack);
}

MONSTERINFO_ATTACK(fixbot_attack) (gentity_t *self) -> void {
	Vector3 vec;
	float  len;

	if (self->monsterInfo.aiFlags & AI_MEDIC) {
		if (!visible(self, self->enemy))
			return;
		vec = self->s.origin - self->enemy->s.origin;
		len = vec.length();
		if (len > 128)
			return;
		else
			M_SetAnimation(self, &fixbot_move_laserattack);
	} else {
		fixbot_set_fly_parameters(self, false, false);
		M_SetAnimation(self, &fixbot_move_attack2);
	}
}

static PAIN(fixbot_pain) (gentity_t *self, gentity_t *other, float kick, int damage, const MeansOfDeath &mod) -> void {
	if (level.time < self->pain_debounce_time)
		return;

	fixbot_set_fly_parameters(self, false, false);
	self->pain_debounce_time = level.time + 3_sec;
	gi.sound(self, CHAN_VOICE, sound_pain1, 1, ATTN_NORM, 0);

	if (damage <= 10)
		M_SetAnimation(self, &fixbot_move_pain3);
	else if (damage <= 25)
		M_SetAnimation(self, &fixbot_move_painb);
	else
		M_SetAnimation(self, &fixbot_move_paina);

	abortHeal(self, false, false, false);
}

void fixbot_dead(gentity_t *self) {
	self->mins = { -16, -16, -24 };
	self->maxs = { 16, 16, -8 };
	self->moveType = MoveType::Toss;
	self->svFlags |= SVF_DEADMONSTER;
	self->nextThink = 0_ms;
	gi.linkEntity(self);
}

static DIE(fixbot_die) (gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, const Vector3 &point, const MeansOfDeath &mod) -> void {
	gi.sound(self, CHAN_VOICE, sound_die, 1, ATTN_NORM, 0);
	BecomeExplosion1(self);

	// shards
}

/*QUAKED monster_fixbot (1 .5 0) (-32 -32 -24) (32 32 24) AMBUSH TRIGGER_SPAWN FIXIT TAKEOFF LANDING x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
 */
void SP_monster_fixbot(gentity_t *self) {
	if (!M_AllowSpawn(self)) {
		FreeEntity(self);
		return;
	}

	sound_pain1.assign("flyer/flypain1.wav");
	sound_die.assign("flyer/flydeth1.wav");

	sound_weld1.assign("misc/welder1.wav");
	sound_weld2.assign("misc/welder2.wav");
	sound_weld3.assign("misc/welder3.wav");

	self->s.modelIndex = gi.modelIndex("models/monsters/fixbot/tris.md2");

	self->mins = { -32, -32, -24 };
	self->maxs = { 32, 32, 24 };

	self->moveType = MoveType::Step;
	self->solid = SOLID_BBOX;

	self->health = 150 * st.health_multiplier;
	self->mass = 150;

	self->pain = fixbot_pain;
	self->die = fixbot_die;

	self->monsterInfo.stand = fixbot_stand;
	self->monsterInfo.walk = fixbot_walk;
	self->monsterInfo.run = fixbot_run;
	self->monsterInfo.attack = fixbot_attack;

	gi.linkEntity(self);

	M_SetAnimation(self, &fixbot_move_stand);
	self->monsterInfo.scale = MODEL_SCALE;
	self->monsterInfo.aiFlags |= AI_ALTERNATE_FLY;
	fixbot_set_fly_parameters(self, false, false);

	flymonster_start(self);
}
