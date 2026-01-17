/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

q1_support.cpp implementation.*/

#include "q1_support.hpp"

#include <cmath>
#include <cstring>

namespace {

	constexpr GameTime TELEPORT_RETURN_DELAY = 1_sec;

        TOUCH(LavaballTouch) (gentity_t* ent, gentity_t* other, const trace_t& tr, bool /*other_touching_self*/) -> void {
                if (other == ent->owner)
                        return;

                if (tr.surface && (tr.surface->flags & SURF_SKY)) {
                        FreeEntity(ent);
			return;
		}

		if (ent->owner && ent->owner->client)
			G_PlayerNoise(ent->owner, ent->s.origin, PlayerNoise::Impact);

		Vector3 origin = ent->s.origin + tr.plane.normal;

		if (other->takeDamage) {
			Damage(other, ent, ent->owner, ent->velocity, ent->s.origin, tr.plane.normal, ent->dmg, ent->dmg,
				DamageFlags::Normal, ModID::RocketLauncher);
		}

		RadiusDamage(ent, ent->owner, static_cast<float>(ent->splashDamage), other, ent->splashRadius, DamageFlags::Normal,
			ModID::RocketLauncher_Splash);

		gi.WriteByte(svc_temp_entity);
		gi.WriteByte(ent->waterLevel ? TE_ROCKET_EXPLOSION_WATER : TE_ROCKET_EXPLOSION);
		gi.WritePosition(origin);
                gi.multicast(ent->s.origin, MULTICAST_PHS, false);

                FreeEntity(ent);
        }

        TOUCH(VorepodTouch) (gentity_t* ent, gentity_t* other, const trace_t& tr, bool /*other_touching_self*/) -> void {
                if (other == ent->owner)
                        return;

                if (tr.surface && (tr.surface->flags & SURF_SKY)) {
			FreeEntity(ent);
			return;
		}

		if (ent->owner && ent->owner->client)
			G_PlayerNoise(ent->owner, ent->s.origin, PlayerNoise::Impact);

		Vector3 origin = ent->s.origin + tr.plane.normal;

		if (other->takeDamage) {
			Damage(other, ent, ent->owner, ent->velocity, ent->s.origin, tr.plane.normal, ent->dmg, ent->dmg,
				DamageFlags::Normal, ModID::RocketLauncher);
		}

		RadiusDamage(ent, ent->owner, static_cast<float>(ent->splashDamage), other, ent->splashRadius, DamageFlags::Normal,
			ModID::RocketLauncher_Splash);

		gi.WriteByte(svc_temp_entity);
		gi.WriteByte(ent->waterLevel ? TE_ROCKET_EXPLOSION_WATER : TE_ROCKET_EXPLOSION);
		gi.WritePosition(origin);
		gi.multicast(ent->s.origin, MULTICAST_PHS, false);

		FreeEntity(ent);
	}

	THINK(VorepodThink) (gentity_t* self) -> void {
		gentity_t* acquire = nullptr;
		float      oldLen = 0.0f;
		float      oldDot = 1.0f;

		Vector3 forward;
		AngleVectors(self->s.angles, forward, nullptr, nullptr);

		if (self->enemy && self->enemy->inUse) {
			acquire = self->enemy;

			if (acquire->health <= 0 || !visible(self, acquire)) {
				self->enemy = acquire = nullptr;
			}
		}

		if (!acquire) {
			gentity_t* target = nullptr;
			while ((target = FindRadius(target, self->s.origin, 1024.0f)) != nullptr) {
				if (target == self->owner)
					continue;
				if (!target->client)
					continue;
				if (target->health <= 0)
					continue;
				if (!visible(self, target))
					continue;

				Vector3 vec = self->s.origin - target->s.origin;
				float len = vec.length();
				if (len <= 0.0f)
					continue;

				Vector3 dir = vec / len;
				float dot = dir.dot(forward);

				if (dot >= oldDot)
					continue;

				if (!acquire || dot < oldDot || len < oldLen) {
					acquire = target;
					oldLen = len;
					oldDot = dot;
				}
			}
		}

		if (acquire) {
			Vector3 desired = (acquire->s.origin - self->s.origin).normalized();
			float   t = self->accel;
			float   d = self->moveDir.dot(desired);

			if (d < 0.45f && d > -0.45f)
				desired = -desired;

			self->moveDir = slerp(self->moveDir, desired, t).normalized();
			self->s.angles = VectorToAngles(self->moveDir);

			if (self->enemy != acquire)
				self->enemy = acquire;
		}
		else {
			self->enemy = nullptr;
		}

		self->velocity = self->moveDir * self->speed;
		self->nextThink = level.time + FRAME_TIME_MS;
	}

	TOUCH(FlameTouch) (gentity_t* ent, gentity_t* other, const trace_t& tr, bool /*other_touching_self*/) -> void {
		if (other == ent->owner)
			return;

		if (tr.surface && (tr.surface->flags & SURF_SKY)) {
			FreeEntity(ent);
			return;
		}

		if (ent->owner && ent->owner->client)
			G_PlayerNoise(ent->owner, ent->s.origin, PlayerNoise::Impact);

		if (other->takeDamage) {
			Damage(other, ent, ent->owner, ent->velocity, ent->s.origin, tr.plane.normal, ent->dmg, 1,
				DamageFlags::Energy, static_cast<ModID>(ent->style));
		}
		else {
			gi.WriteByte(svc_temp_entity);
			gi.WriteByte(TE_SPARKS);
			gi.WritePosition(ent->s.origin);
			gi.WriteDir(tr.plane.normal);
			gi.multicast(ent->s.origin, MULTICAST_PHS, false);
		}

		FreeEntity(ent);
	}

TOUCH(ZombieGibTouch) (gentity_t* ent, gentity_t* other, const trace_t& tr, bool /*other_touching_self*/) -> void {
		if (other == ent->owner) {
			return;
		}

		if (tr.surface && (tr.surface->flags & SURF_SKY)) {
			FreeEntity(ent);
			return;
		}

		if (other->takeDamage) {
			Vector3 dir = other->s.origin - ent->s.origin;
			Damage(other, ent, ent->owner ? ent->owner : ent, dir, ent->s.origin, tr.plane.normal, ent->dmg, ent->dmg,
				DamageFlags::Normal, ModID::Gekk);
			gi.sound(ent, static_cast<soundchan_t>(CHAN_RELIABLE | CHAN_WEAPON),
				gi.soundIndex("q1zombie/z_hit.wav"), 1.0f, ATTN_NORM, 0);
		}
		else {
			gi.sound(ent, static_cast<soundchan_t>(CHAN_RELIABLE | CHAN_WEAPON),
				gi.soundIndex("q1zombie/z_miss.wav"), 1.0f, ATTN_NORM, 0);
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

	THINK(PlasmaballExplode) (gentity_t* self) -> void {
		if (self->s.frame == 0) {
			gentity_t* ent = nullptr;
			while ((ent = FindRadius(ent, self->s.origin, self->splashRadius)) != nullptr) {
				if (!ent->takeDamage)
					continue;
				if (ent == self->owner)
					continue;
				if (!CanDamage(ent, self))
					continue;
				if (self->owner && !CanDamage(ent, self->owner))
					continue;
				if (!(ent->svFlags & SVF_MONSTER) && !(ent->flags & FL_DAMAGEABLE) && !ent->client &&
					(!ent->className || std::strcmp(ent->className, "misc_explobox") != 0))
					continue;
				if (self->owner && CheckTeamDamage(ent, self->owner))
					continue;

				Vector3 minsMaxs = ent->mins + ent->maxs;
				Vector3 centroid = ent->s.origin + (minsMaxs * 0.5f);
				Vector3 delta = self->s.origin - centroid;
				float   dist = delta.length();
				if (dist <= 0.0f || self->splashRadius <= 0.0f)
					continue;

				float points = self->splashDamage * (1.0f - std::sqrt(dist / self->splashRadius));
				if (points <= 0.0f)
					continue;

				Damage(ent, self, self->owner, self->velocity, centroid, vec3_origin, static_cast<int>(points), 0,
					DamageFlags::Energy, ModID::BFG10K_Effect);

				gi.WriteByte(svc_temp_entity);
				gi.WriteByte(TE_LIGHTNING);
				gi.WriteEntity(self);
				gi.WriteEntity(world);
				gi.WritePosition(self->s.origin);
				gi.WritePosition(centroid);
				gi.multicast(self->s.origin, MULTICAST_PHS, false);
			}
		}

		self->nextThink = level.time;
		self->think = FreeEntity;
	}

	TOUCH(PlasmaballTouch) (gentity_t* self, gentity_t* other, const trace_t& tr, bool /*other_touching_self*/) -> void {
		if (other == self->owner)
			return;

		if (tr.surface && (tr.surface->flags & SURF_SKY)) {
			FreeEntity(self);
			return;
		}

		if (self->owner && self->owner->client)
			G_PlayerNoise(self->owner, self->s.origin, PlayerNoise::Impact);

		if (other->takeDamage) {
			Damage(other, self, self->owner, self->velocity, self->s.origin, tr.plane.normal, 200, 0,
				DamageFlags::Energy, ModID::BFG10K_Blast);
		}

		RadiusDamage(self, self->owner, 200.0f, other, 100.0f, DamageFlags::Energy, ModID::BFG10K_Blast);

		gi.sound(self, CHAN_VOICE, gi.soundIndex("weapons/bfg__x1b.wav"), 1, ATTN_NORM, 0);
		self->solid = SOLID_NOT;
		self->touch = nullptr;
		self->s.origin += self->velocity * (-gi.frameTimeSec);
		self->velocity = {};
		self->s.frame = 0;
		self->s.sound = 0;
		self->s.effects &= ~EF_ANIM_ALLFAST;
		self->think = PlasmaballExplode;
		self->nextThink = level.time + 10_hz;
		self->enemy = other;

		gi.WriteByte(svc_temp_entity);
		gi.WriteByte(TE_NUKEBLAST);
		gi.WritePosition(self->s.origin);
		gi.multicast(self->s.origin, MULTICAST_PHS, false);
	}

} // namespace

bool TryRandomTeleportPosition(gentity_t* self, float radius) {
	if (!self)
		return false;

	for (int attempt = 0; attempt < 10; ++attempt) {
		Vector3 offset = { crandom() * radius, crandom() * radius, crandom() * (radius * 0.5f) };
		Vector3 target = self->s.origin + offset;

		trace_t solid = gi.trace(self->s.origin, self->mins, self->maxs, target, self, MASK_SOLID);
		if (solid.startSolid || solid.allSolid)
			continue;

		trace_t occ = gi.trace(target, self->mins, self->maxs, target, self, MASK_MONSTERSOLID);
		if (occ.startSolid || occ.allSolid || occ.fraction < 1.0f ||
			(occ.ent && occ.ent != world && occ.ent != self))
			continue;

		gi.WriteByte(svc_temp_entity);
		gi.WriteByte(TE_TELEPORT_EFFECT);
		gi.WritePosition(self->s.origin);
		gi.multicast(self->s.origin, MULTICAST_PVS, false);

		self->monsterInfo.teleportReturnOrigin = self->s.origin;
		self->monsterInfo.teleportReturnTime = level.time + TELEPORT_RETURN_DELAY;
		self->monsterInfo.teleportActive = true;
		self->postThink = CheckTeleportReturn;

		self->s.origin = target;
		self->s.oldOrigin = target;
		gi.linkEntity(self);

		gi.WriteByte(svc_temp_entity);
		gi.WriteByte(TE_TELEPORT_EFFECT);
		gi.WritePosition(self->s.origin);
		gi.multicast(self->s.origin, MULTICAST_PVS, false);

		return true;
	}

	return false;
}

void CheckTeleportReturn(gentity_t* self) {
	if (!self || !self->monsterInfo.teleportActive)
		return;

	if (level.time < self->monsterInfo.teleportReturnTime)
		return;

	bool returnHome = false;
	if (!self->enemy || !self->enemy->inUse)
		returnHome = true;
	else if (!visible(self, self->enemy))
		returnHome = true;

	if (returnHome) {
		gi.WriteByte(svc_temp_entity);
		gi.WriteByte(TE_TELEPORT_EFFECT);
		gi.WritePosition(self->s.origin);
		gi.multicast(self->s.origin, MULTICAST_PVS, false);

		self->s.origin = self->monsterInfo.teleportReturnOrigin;
		self->s.oldOrigin = self->monsterInfo.teleportReturnOrigin;
		gi.linkEntity(self);

		gi.WriteByte(svc_temp_entity);
		gi.WriteByte(TE_TELEPORT_EFFECT);
		gi.WritePosition(self->s.origin);
		gi.multicast(self->s.origin, MULTICAST_PVS, false);
	}

	self->monsterInfo.teleportActive = false;
	self->postThink = nullptr;
}

gentity_t* fire_lavaball(gentity_t* self, const Vector3& start, const Vector3& dir, int damage, int speed,
	float damageRadius, int radiusDamage) {
	gentity_t* lavaball = Spawn();
	lavaball->s.origin = start;
	lavaball->s.oldOrigin = start;
	lavaball->s.angles = VectorToAngles(dir);
	lavaball->s.effects |= EF_FIREBALL;
	lavaball->velocity = dir * speed;
	lavaball->moveType = MoveType::FlyMissile;
	lavaball->svFlags |= SVF_PROJECTILE;
	lavaball->flags |= FL_DODGE;
	lavaball->clipMask = MASK_PROJECTILE;
	if (self && self->client && !G_ShouldPlayersCollide(true))
		lavaball->clipMask &= ~CONTENTS_PLAYER;
	lavaball->solid = SOLID_BBOX;
	lavaball->s.modelIndex = gi.modelIndex("models/objects/gibs/sm_meat/tris.md2");
	lavaball->owner = self;
	lavaball->touch = LavaballTouch;
	lavaball->nextThink = level.time + GameTime::from_sec(8000.f / speed);
	lavaball->think = FreeEntity;
	lavaball->dmg = damage;
	lavaball->splashDamage = radiusDamage;
	lavaball->splashRadius = damageRadius;
	lavaball->className = "lavaball";

	gi.linkEntity(lavaball);
	return lavaball;
}

void fire_vorepod(gentity_t* self, const Vector3& start, const Vector3& dir, int damage, int speed, float damageRadius,
	int radiusDamage, float turnFraction, int skin) {
	gentity_t* pod = Spawn();
	pod->s.origin = start;
	pod->moveDir = dir;
	pod->s.angles = VectorToAngles(dir);
	pod->velocity = dir * speed;
	pod->flags |= FL_DODGE;
	pod->moveType = MoveType::FlyMissile;
	pod->svFlags |= SVF_PROJECTILE;
	pod->clipMask = MASK_PROJECTILE;
	if (self && self->client && !G_ShouldPlayersCollide(true))
		pod->clipMask &= ~CONTENTS_PLAYER;
	pod->solid = SOLID_BBOX;
	pod->s.effects |= EF_TRACKER;
	pod->s.modelIndex = gi.modelIndex("models/proj/pod/tris.md2");
	pod->s.skinNum = skin;
	pod->owner = self;
	pod->touch = VorepodTouch;
	pod->speed = speed;
	pod->accel = turnFraction;
	pod->nextThink = level.time + FRAME_TIME_MS;
	pod->think = VorepodThink;
	pod->dmg = damage;
	pod->splashDamage = radiusDamage;
	pod->splashRadius = damageRadius;

	if (self && self->enemy && self->enemy->inUse && visible(pod, self->enemy))
		pod->enemy = self->enemy;

	gi.linkEntity(pod);
}

gentity_t* fire_flame(gentity_t* self, const Vector3& start, const Vector3& dir, int damage, int speed, ModID mod) {
	gentity_t* flame = Spawn();
	flame->svFlags |= SVF_PROJECTILE;
	flame->s.origin = start;
	flame->s.oldOrigin = start;
	flame->s.angles = VectorToAngles(dir);
	flame->velocity = dir * speed;
	flame->moveType = MoveType::FlyMissile;
	flame->clipMask = MASK_PROJECTILE;
	if (self && self->client && !G_ShouldPlayersCollide(true))
		flame->clipMask &= ~CONTENTS_PLAYER;
	flame->flags |= FL_DODGE;
	flame->solid = SOLID_BBOX;
	flame->s.effects |= EF_IONRIPPER;
	flame->s.modelIndex = gi.modelIndex("models/proj/firebolt/tris.md2");
	flame->s.sound = gi.soundIndex("monsters/hknight/attack1.wav");
	flame->owner = self;
	flame->touch = FlameTouch;
	flame->nextThink = level.time + 2_sec;
	flame->think = FreeEntity;
	flame->dmg = damage;
	flame->style = static_cast<int>(mod);
	flame->className = "flame";

	gi.linkEntity(flame);

	trace_t tr = gi.traceLine(self->s.origin, flame->s.origin, flame, flame->clipMask);
	if (tr.fraction < 1.0f) {
		flame->s.origin = tr.endPos + (tr.plane.normal * 1.0f);
		flame->touch(flame, tr.ent, tr, false);
	}

	return flame;
}

void fire_gib(gentity_t* self, const Vector3& start, const Vector3& aimDir, int damage, int speed, float rightAdjust,
	float upAdjust) {
	gentity_t* gib = Spawn();
	gib->s.origin = start;
	gib->velocity = aimDir * speed;

	Vector3 forward, right, up;
	AngleVectors(VectorToAngles(aimDir), forward, right, up);

	if (upAdjust != 0.0f) {
		float gravityAdjustment = level.gravity / 800.0f;
		gib->velocity += up * (upAdjust * gravityAdjustment);
	}

	if (rightAdjust != 0.0f)
		gib->velocity += right * rightAdjust;

	gib->moveType = MoveType::Bounce;
	gib->clipMask = MASK_PROJECTILE;
	if (self && self->client && !G_ShouldPlayersCollide(true))
		gib->clipMask &= ~CONTENTS_PLAYER;
	gib->solid = SOLID_BBOX;
	gib->svFlags |= SVF_PROJECTILE;
	gib->flags |= FL_DODGE;
	gib->s.effects |= EF_GIB;
	gib->speed = speed;
	gib->mins = { -6, -6, -6 };
	gib->maxs = { 6, 6, 6 };
	gib->aVelocity = { crandom() * 360.0f, crandom() * 360.0f, crandom() * 360.0f };
	gib->s.modelIndex = gi.modelIndex("models/proj/zomgib/tris.md2");
	gib->owner = self;
	gib->touch = ZombieGibTouch;
	gib->nextThink = level.time + 2.5_sec;
	gib->think = FreeEntity;
	gib->dmg = damage;
	gib->className = "gib";

	gi.linkEntity(gib);
}

void blaster_touch(gentity_t* self, gentity_t* other, const trace_t& tr, bool otherTouchingSelf);

void fire_plasmaball(gentity_t* self, const Vector3& start, const Vector3& dir, int damage, int speed, float damageRadius) {
	gentity_t* plasma = Spawn();
	plasma->s.origin = start;
	plasma->s.angles = VectorToAngles(dir);
	plasma->velocity = dir * speed;
	plasma->svFlags |= SVF_PROJECTILE;
	plasma->moveType = MoveType::FlyMissile;
	plasma->clipMask = MASK_PROJECTILE;
	plasma->flags |= FL_DODGE;
	if (self && self->client && !G_ShouldPlayersCollide(true))
		plasma->clipMask &= ~CONTENTS_PLAYER;
	plasma->solid = SOLID_BBOX;
	plasma->s.effects |= EF_PLASMA;
	plasma->s.modelIndex = gi.modelIndex("models/proj/plasma/tris.md2");
	plasma->touch = PlasmaballTouch;
	plasma->owner = self;
	plasma->nextThink = level.time + GameTime::from_sec(8000.f / speed);
	plasma->think = FreeEntity;
	plasma->splashDamage = damage;
	plasma->splashRadius = damageRadius;
	plasma->className = "plasma blast";
	plasma->s.sound = gi.soundIndex("weapons/plasma__l1a.wav");
	plasma->teamMaster = plasma;
	plasma->teamChain = nullptr;

	gi.linkEntity(plasma);
}

void fire_lightning(gentity_t* self, const Vector3& start, const Vector3& dir, int damage, int speed, Effect effect) {
	gentity_t* bolt = Spawn();
	bolt->s.origin = start;
	bolt->s.oldOrigin = start;
	bolt->s.angles = VectorToAngles(dir);
	bolt->velocity = dir * speed;
	bolt->svFlags |= SVF_PROJECTILE;
	bolt->moveType = MoveType::FlyMissile;
	bolt->flags |= FL_DODGE;
	bolt->clipMask = MASK_PROJECTILE;
	if (self && self->client && !G_ShouldPlayersCollide(true))
		bolt->clipMask &= ~CONTENTS_PLAYER;
	bolt->solid = SOLID_BBOX;
	bolt->s.effects |= effect;
	bolt->s.modelIndex = gi.modelIndex("models/proj/lightning/tris.md2");
	bolt->s.skinNum = 1;
	bolt->s.sound = gi.soundIndex("weapons/tesla.wav");
	bolt->owner = self;
	bolt->touch = blaster_touch;
	bolt->nextThink = level.time + 2_sec;
	bolt->think = FreeEntity;
	bolt->dmg = damage;
	bolt->className = "bolt";
	bolt->style = static_cast<int32_t>(ModID::Thunderbolt);

	gi.linkEntity(bolt);

	trace_t tr = gi.traceLine(self->s.origin, bolt->s.origin, bolt, bolt->clipMask);
	if (tr.fraction < 1.0f) {
		bolt->s.origin = tr.endPos + (tr.plane.normal * 1.0f);
		bolt->touch(bolt, tr.ent, tr, false);
	}
}
