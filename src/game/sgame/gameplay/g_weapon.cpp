/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

g_weapon.cpp (Game Weapon Logic) This file contains the server-side
implementation for all player-usable weapons. It is responsible for the
mechanics of firing each weapon, spawning the appropriate projectiles or
performing hitscan traces, and applying damage. Key Responsibilities:
- Firing Functions: Implements the `fire_*` functions (e.g., `fire_rocket`,
`fire_rail`, `fire_shotgun`) that are called when a player attacks. - Projectile
Spawning: Handles the creation and initialization of projectile entities,
setting their velocity, damage, owner, and other properties. - Hitscan Logic:
Performs the trace line calculations for instant-hit weapons like the shotgun
and machinegun. - Damage and Effects: Calls the core `Damage` function to apply
damage to targets and triggers visual and audio effects for weapon fire. -
Weapon State Machine: The `Weapon_Generic` function provides a state machine to
handle the animation sequence of firing a weapon (ready, fire, idle, etc.).*/

#include "../g_local.hpp"
#include "g_proball.hpp"
#include <array>

/*
=============
PlayClientPowerupFireSound

Selects and plays the appropriate powerup fire sound for the owning client.
=============
*/
static void PlayClientPowerupFireSound(gentity_t *self) {
  if (!self->owner || !self->owner->client)
    return;

  gclient_t *cl = self->owner->client;
  const bool quadDamage =
      cl->PowerupTimer(PowerupTimer::QuadDamage) > level.time;
  const bool doubleDamage =
      cl->PowerupTimer(PowerupTimer::DoubleDamage) > level.time;
  const bool haste = cl->PowerupTimer(PowerupTimer::Haste) > level.time;
  const bool canPlayHaste = cl->tech.soundTime < level.time;

  const char *sound = nullptr;

  if (quadDamage && doubleDamage) {
    sound = "ctf/tech2x.wav";
  } else if (quadDamage) {
    sound = "items/damage3.wav";
  } else if (doubleDamage) {
    sound = "misc/ddamage3.wav";
  } else if (haste && canPlayHaste) {
    cl->tech.soundTime = level.time + 1_sec;
    sound = "ctf/tech3.wav";
  }

  if (sound)
    gi.sound(self, CHAN_ITEM, gi.soundIndex(sound), 1, ATTN_NORM, 0);
}

/*
=================
fire_hit

Used for all impact (hit/punch/slash) attacks
=================
*/
bool fire_hit(gentity_t *self, Vector3 aim, int damage, int kick) {
  const float range = distance_between_boxes(
      self->enemy->absMin, self->enemy->absMax, self->absMin, self->absMax);
  if (range > aim[0])
    return false;

  if (!(aim[1] > self->mins[0] && aim[1] < self->maxs[0])) {
    // this is a side hit so adjust the "right" value out to the edge of their
    // bbox
    aim[1] = aim[1] < 0 ? self->enemy->mins[0] : self->enemy->maxs[0];
  }

  Vector3 point = closest_point_to_box(self->s.origin, self->enemy->absMin,
                                       self->enemy->absMax);
  trace_t tr = gi.traceLine(self->s.origin, point, self, MASK_PROJECTILE);

  if (tr.fraction < 1) {
    if (!tr.ent->takeDamage)
      return false;
    // if it will hit any client/monster then hit the one we wanted to hit
    if ((tr.ent->svFlags & SVF_MONSTER) || tr.ent->client)
      tr.ent = self->enemy;
  }

  // check that we can hit the player from the point
  tr = gi.traceLine(point, self->enemy->s.origin, self, MASK_PROJECTILE);

  if (tr.fraction < 1) {
    if (!tr.ent->takeDamage)
      return false;
    // if it will hit any client/monster then hit the one we wanted to hit
    if ((tr.ent->svFlags & SVF_MONSTER) || tr.ent->client)
      tr.ent = self->enemy;
  }

  Vector3 forward{};
  Vector3 right{};
  Vector3 up{};
  AngleVectors(self->s.angles, forward, right, up);
  point = self->s.origin + (forward * range) + (right * aim[1]) + (up * aim[2]);
  const Vector3 dir = point - self->enemy->s.origin;

  Damage(tr.ent, self, self, dir, point, vec3_origin, damage, kick / 2,
         DamageFlags::NoKnockback, ModID::Hit);

  if (!(tr.ent->svFlags & SVF_MONSTER) && !tr.ent->client)
    return false;

  Vector3 knockbackDir = (self->enemy->absMin + self->enemy->absMax) * 0.5f;
  knockbackDir -= point;
  knockbackDir.normalize();
  self->enemy->velocity += knockbackDir * kick;
  if (self->enemy->velocity[_Z] > 0)
    self->enemy->groundEntity = nullptr;
  return true;
}

// helper routine for piercing traces;
// mask = the input mask for finding what to hit
// you can adjust the mask for the re-trace (for water, etc).
// note that you must take care in your pierce callback to mark
// the entities that are being pierced.
/*
=============
pierce_trace

Performs a pierce-aware trace between two points, delegating hit handling to a
callback.
=============
*/
void pierce_trace(const Vector3 &start, const Vector3 &end, gentity_t *ignore,
                  pierce_args_t &pierce, contents_t mask) {
  Vector3 own_start = start;
  Vector3 own_end = end;

  for (int loop_count = MAX_ENTITIES; loop_count > 0; --loop_count) {
    pierce.tr = gi.traceLine(start, own_end, ignore, mask);

    // didn't hit anything, so we're done
    if (!pierce.tr.ent || pierce.tr.fraction == 1.0f)
      return;

    // hit callback said we're done
    if (!pierce.hit(mask, own_end))
      return;

    own_start = pierce.tr.endPos;
  }

  gi.Com_Print("runaway pierce_trace\n");
}

struct fire_lead_pierce_t : pierce_args_t {
  gentity_t *self;
  Vector3 start;
  Vector3 aimDir;
  int damage;
  int kick;
  int hSpread;
  int vSpread;
  MeansOfDeath mod;
  int te_impact;
  contents_t mask;
  bool water = false;
  Vector3 water_start = {};
  gentity_t *chain = nullptr;

  inline fire_lead_pierce_t(gentity_t *self, Vector3 start, Vector3 aimDir,
                            int damage, int kick, int hSpread, int vSpread,
                            MeansOfDeath mod, int te_impact, contents_t mask)
      : pierce_args_t(), self(self), start(start), aimDir(aimDir),
        damage(damage), kick(kick), hSpread(hSpread), vSpread(vSpread),
        mod(mod), te_impact(te_impact), mask(mask) {}

  // we hit an entity; return false to stop the piercing.
  // you can adjust the mask for the re-trace (for water, etc).
  bool hit(contents_t &mask, Vector3 &end) override {
    // see if we hit water
    if (tr.contents & MASK_WATER) {
      int color;

      water = true;
      water_start = tr.endPos;

      // CHECK: is this compare ever true?
      if (te_impact != -1 && start != tr.endPos) {
        if (tr.contents & CONTENTS_WATER) {
          // FIXME: this effectively does nothing..
          if (strcmp(tr.surface->name, "brwater") == 0)
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
        Vector3 dir, forward, right, up;
        dir = end - start;
        dir = VectorToAngles(dir);
        AngleVectors(dir, forward, right, up);
        float r = crandom() * hSpread * 2;
        float u = crandom() * vSpread * 2;
        end = water_start + (forward * 8192);
        end += (right * r);
        end += (up * u);
      }

      // re-trace ignoring water this time
      mask &= ~MASK_WATER;
      return true;
    }

    // did we hit an hurtable entity?
    if (tr.ent->takeDamage) {
      Damage(tr.ent, self, self, aimDir, tr.endPos, tr.plane.normal, damage,
             kick,
             mod.id == ModID::TeslaMine ? DamageFlags::Energy
                                        : DamageFlags::Bullet,
             mod);

      // only deadmonster is pierceable, or actual dead monsters
      // that haven't been made non-solid yet
      if ((tr.ent->svFlags & SVF_DEADMONSTER) ||
          (tr.ent->health <= 0 && (tr.ent->svFlags & SVF_MONSTER))) {
        if (!mark(tr.ent))
          return false;

        return true;
      }
    } else {
      // send gun puff / flash
      // don't mark the sky
      if (te_impact != -1 &&
          !(tr.surface && ((tr.surface->flags & SURF_SKY) ||
                           strncmp(tr.surface->name, "sky", 3) == 0))) {
        gi.WriteByte(svc_temp_entity);
        gi.WriteByte(te_impact);
        gi.WritePosition(tr.endPos);
        gi.WriteDir(tr.plane.normal);
        gi.multicast(tr.endPos, MULTICAST_PVS, false);

        if (self->client)
          G_PlayerNoise(self, tr.endPos, PlayerNoise::Impact);
      }
    }

    // hit a solid, so we're stopping here

    return false;
  }
};

/*
=================
fire_lead

This is an internal support routine used for bullet/pellet based weapons.
=================
*/
static void fire_lead(gentity_t *self, const Vector3 &start,
                      const Vector3 &aimDir, int damage, int kick,
                      int te_impact, int hSpread, int vSpread,
                      MeansOfDeath mod) {
  fire_lead_pierce_t args = {
      self,    start,   aimDir, damage,    kick,
      hSpread, vSpread, mod,    te_impact, MASK_PROJECTILE | MASK_WATER};

  // [Paril-KEX]
  if (self->client && !G_ShouldPlayersCollide(true))
    args.mask &= ~CONTENTS_PLAYER;

  // special case: we started in water.
  if (gi.pointContents(start) & MASK_WATER) {
    args.water = true;
    args.water_start = start;
    args.mask &= ~MASK_WATER;
  }

  // check initial firing position
  pierce_trace(self->s.origin, start, self, args, args.mask);

  // we're clear, so do the second pierce
  if (args.tr.fraction == 1.f) {
    args.restore();

    Vector3 end, dir, forward, right, up;
    dir = VectorToAngles(aimDir);
    AngleVectors(dir, forward, right, up);

    float r = crandom() * hSpread;
    float u = crandom() * vSpread;
    end = start + (forward * 8192);
    end += (right * r);
    end += (up * u);

    pierce_trace(args.tr.endPos, end, self, args, args.mask);
  }

  // if went through water, determine where the end is and make a bubble trail
  if (args.water && te_impact != -1) {
    Vector3 pos, dir;

    dir = args.tr.endPos - args.water_start;
    dir.normalize();
    pos = args.tr.endPos + (dir * -2);
    if (gi.pointContents(pos) & MASK_WATER)
      args.tr.endPos = pos;
    else
      args.tr = gi.traceLine(pos, args.water_start,
                             args.tr.ent != world ? args.tr.ent : nullptr,
                             MASK_WATER);

    pos = args.water_start + args.tr.endPos;
    pos *= 0.5f;

    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_BUBBLETRAIL);
    gi.WritePosition(args.water_start);
    gi.WritePosition(args.tr.endPos);
    gi.multicast(pos, MULTICAST_PVS, false);
  }
}

/*
=================
fire_bullet

Fires a single round.  Used for machinegun and chaingun.  Would be fine for
pistols, rifles, etc....
=================
*/
void fire_bullet(gentity_t *self, const Vector3 &start, const Vector3 &aimDir,
                 int damage, int kick, int hSpread, int vSpread,
                 MeansOfDeath mod) {
  fire_lead(self, start, aimDir, damage, kick,
            mod.id == ModID::TeslaMine ? -1 : TE_GUNSHOT, hSpread, vSpread,
            mod);
}

/*
=================
fire_shotgun

Shoots shotgun pellets.  Used by shotgun and super shotgun.
=================
*/
void fire_shotgun(gentity_t *self, const Vector3 &start, const Vector3 &aimDir,
                  int damage, int kick, int hSpread, int vSpread, int count,
                  MeansOfDeath mod) {
  for (int i = 0; i < count; i++)
    fire_lead(self, start, aimDir, damage, kick, TE_SHOTGUN, hSpread, vSpread,
              mod);
}

/*
=================
fire_blaster

Fires a single blaster bolt.  Used by the blaster and hyper blaster.
=================
*/
TOUCH(blaster_touch)(gentity_t *ent, gentity_t *other, const trace_t &tr,
                     bool otherTouchingSelf)
    ->void {
  Vector3 origin;
  if (other == ent->owner)
    return;

  if (tr.surface && (tr.surface->flags & SURF_SKY)) {
    FreeEntity(ent);
    return;
  }

  // PMM - crash prevention
  if (ent->owner && ent->owner->client)
    G_PlayerNoise(ent->owner, ent->s.origin, PlayerNoise::Impact);

  // calculate position for the explosion entity
  origin = ent->s.origin + tr.plane.normal;

  if (other->takeDamage) {
    Damage(other, ent, ent->owner, ent->velocity, ent->s.origin,
           tr.plane.normal, ent->dmg, 1,
           DamageFlags::Energy | DamageFlags::StatOnce,
           static_cast<ModID>(ent->style));
  } else {
  }

  if (ent->splashDamage)
    RadiusDamage(ent, ent->owner, (float)ent->splashDamage, other,
                 ent->splashRadius, DamageFlags::Energy, ModID::HyperBlaster);

  gi.WriteByte(svc_temp_entity);
  gi.WriteByte((ent->style != static_cast<int32_t>(ModID::BlueBlaster))
                   ? TE_BLASTER
                   : TE_BLUEHYPERBLASTER);
  gi.WritePosition(ent->splashDamage ? origin : ent->s.origin);
  gi.WriteDir(tr.plane.normal);
  gi.multicast(ent->s.origin, MULTICAST_PHS, false);

  FreeEntity(ent);
}

void fire_blaster(gentity_t *self, const Vector3 &start, const Vector3 &dir,
                  int damage, int speed, Effect effect, MeansOfDeath mod,
                  bool altNoise) {
  gentity_t *bolt = Spawn();
  bolt->svFlags = SVF_PROJECTILE;
  bolt->s.origin = start;
  bolt->s.oldOrigin = start;
  bolt->s.angles = VectorToAngles(dir);
  bolt->velocity = dir * speed;
  bolt->moveType = MoveType::FlyMissile;
  bolt->clipMask = MASK_PROJECTILE;

  if (self->client && !G_ShouldPlayersCollide(true))
    bolt->clipMask &= ~CONTENTS_PLAYER;

  bolt->flags |= FL_DODGE;
  bolt->solid = SOLID_BBOX;
  bolt->s.effects |= effect;
  bolt->s.modelIndex = gi.modelIndex("models/objects/laser/tris.md2");
  bolt->s.sound =
      gi.soundIndex(altNoise ? "enforcer/enfire.wav" : "misc/lasfly.wav");
  bolt->owner = self;
  bolt->touch = blaster_touch;
  bolt->style = static_cast<int>(mod.id);

  bolt->nextThink = level.time + 2_sec;
  bolt->think = FreeEntity;
  bolt->dmg = damage;
  if (RS(Quake3Arena) && mod.id == ModID::HyperBlaster) {
    bolt->s.scale = 100;
    bolt->splashRadius = 30; // 20;
    bolt->splashDamage = 20; // 15;
  }
  bolt->className = "bolt";
  gi.linkEntity(bolt);

  trace_t tr =
      gi.traceLine(self->s.origin, bolt->s.origin, bolt, bolt->clipMask);
  if (tr.fraction < 1.0f) {
    bolt->s.origin = tr.endPos + (tr.plane.normal * 1.f);
    bolt->touch(bolt, tr.ent, tr, false);
  }
}

/*
=================
fire_greenblaster

Fires a single green blaster bolt. Used by monsters, generally.
=================
*/
static TOUCH(blaster2_touch)(gentity_t *self, gentity_t *other,
                             const trace_t &tr, bool otherTouchingSelf)
    ->void {
  MeansOfDeath mod;
  int dmgStat;

  if (other == self->owner)
    return;

  if (tr.surface && (tr.surface->flags & SURF_SKY)) {
    FreeEntity(self);
    return;
  }

  if (self->owner && self->owner->client)
    G_PlayerNoise(self->owner, self->s.origin, PlayerNoise::Impact);

  if (other->takeDamage) {
    // the only time players will be firing blaster2 bolts will be from the
    // defender sphere.
    if (self->owner && self->owner->client)
      mod = ModID::DefenderSphere;
    else
      mod = ModID::Blaster2;

    if (self->owner) {
      dmgStat = self->owner->takeDamage;
      self->owner->takeDamage = false;
      if (self->dmg >= 5)
        RadiusDamage(self, self->owner, (float)(self->dmg * 2), other,
                     self->splashRadius, DamageFlags::Energy, ModID::Unknown);
      Damage(other, self, self->owner, self->velocity, self->s.origin,
             tr.plane.normal, self->dmg, 1,
             DamageFlags::Energy | DamageFlags::StatOnce, mod);
      self->owner->takeDamage = dmgStat;
    } else {
      if (self->dmg >= 5)
        RadiusDamage(self, self->owner, (float)(self->dmg * 2), other,
                     self->splashRadius, DamageFlags::Energy, ModID::Unknown);
      Damage(other, self, self->owner, self->velocity, self->s.origin,
             tr.plane.normal, self->dmg, 1,
             DamageFlags::Energy | DamageFlags::StatOnce, mod);
    }
  } else {
    // PMM - yeowch this will get expensive
    if (self->dmg >= 5)
      RadiusDamage(self, self->owner, (float)(self->dmg * 2), self->owner,
                   self->splashRadius, DamageFlags::Energy, ModID::Unknown);

    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_BLASTER2);
    gi.WritePosition(self->s.origin);
    gi.WriteDir(tr.plane.normal);
    gi.multicast(self->s.origin, MULTICAST_PHS, false);
  }

  FreeEntity(self);
}

void fire_greenblaster(gentity_t *self, const Vector3 &start,
                       const Vector3 &dir, int damage, int speed, Effect effect,
                       bool hyper) {
  gentity_t *bolt;
  trace_t tr;

  bolt = Spawn();
  bolt->svFlags |= SVF_PROJECTILE;
  bolt->s.origin = start;
  bolt->s.oldOrigin = start;
  bolt->s.angles = VectorToAngles(dir);
  bolt->velocity = dir * speed;
  bolt->moveType = MoveType::FlyMissile;
  bolt->clipMask = MASK_PROJECTILE;
  // [Paril-KEX]
  if (self->client && !G_ShouldPlayersCollide(true))
    bolt->clipMask &= ~CONTENTS_PLAYER;
  bolt->flags |= FL_DODGE;
  bolt->solid = SOLID_BBOX;
  bolt->s.effects |= effect;
  bolt->s.modelIndex = gi.modelIndex("models/objects/laser/tris.md2");
  bolt->owner = self;
  bolt->touch = blaster2_touch;
  if (effect)
    bolt->s.effects |= EF_TRACKER;
  bolt->splashRadius = 128;
  bolt->s.skinNum = 2;
  bolt->s.scale = 2.5f;

  bolt->nextThink = level.time + 2_sec;
  bolt->think = FreeEntity;
  bolt->dmg = damage;
  bolt->className = "bolt";
  gi.linkEntity(bolt);

  tr = gi.traceLine(self->s.origin, bolt->s.origin, bolt, bolt->clipMask);
  if (tr.fraction < 1.0f) {
    bolt->s.origin = tr.endPos + (tr.plane.normal * 1.f);
    bolt->touch(bolt, tr.ent, tr, false);
  }
}

/*
=================
fire_blueblaster
=================
*/
void fire_blueblaster(gentity_t *self, const Vector3 &start, const Vector3 &dir,
                      int damage, int speed, Effect effect) {
  gentity_t *bolt;
  trace_t tr;

  bolt = Spawn();
  bolt->svFlags |= SVF_PROJECTILE;
  bolt->s.origin = start;
  bolt->s.oldOrigin = start;
  bolt->s.angles = VectorToAngles(dir);
  bolt->velocity = dir * speed;
  bolt->moveType = MoveType::FlyMissile;
  bolt->clipMask = MASK_PROJECTILE;
  // [Paril-KEX]
  if (self->client && !G_ShouldPlayersCollide(true))
    bolt->clipMask &= ~CONTENTS_PLAYER;
  bolt->flags |= FL_DODGE;
  bolt->solid = SOLID_BBOX;
  bolt->s.effects |= effect;
  bolt->s.modelIndex = gi.modelIndex("models/objects/laser/tris.md2");
  bolt->s.sound = gi.soundIndex("misc/lasfly.wav");
  bolt->s.skinNum = 1;
  bolt->owner = self;
  bolt->touch = blaster_touch;
  bolt->style = static_cast<int32_t>(ModID::BlueBlaster);

  bolt->nextThink = level.time + 2_sec;
  bolt->think = FreeEntity;
  bolt->dmg = damage;
  bolt->className = "bolt";
  gi.linkEntity(bolt);

  tr = gi.traceLine(self->s.origin, bolt->s.origin, bolt, bolt->clipMask);
  if (tr.fraction < 1.0f) {
    bolt->s.origin = tr.endPos + (tr.plane.normal * 1.f);
    bolt->touch(bolt, tr.ent, tr, false);
  }
}

constexpr SpawnFlags SPAWNFLAG_GRENADE_HAND = 1_spawnflag;
constexpr SpawnFlags SPAWNFLAG_GRENADE_HELD = 2_spawnflag;

/*
=================
fire_grenade
=================
*/
static THINK(Grenade_Explode)(gentity_t *ent)->void {
  // Cache victim pointer before any damage logic potentially frees it.
  gentity_t *victim = ent->enemy;
  Vector3 origin;

  MeansOfDeath mod;

  if (ent->spawnFlags.has(SPAWNFLAG_GRENADE_HELD))
    mod = ModID::HandGrenade_Held;
  else if (ent->spawnFlags.has(SPAWNFLAG_GRENADE_HAND))
    mod = ModID::HandGrenade_Splash;
  else
    mod = ModID::GrenadeLauncher_Splash;

  if (victim && victim->inUse) {
    float points;
    Vector3 v, dir;
    const Vector3 victim_origin = victim->s.origin;
    v = victim->mins + victim->maxs;
    v = victim_origin + (v * 0.5f);
    v = ent->s.origin - v;
    points = ent->dmg - 0.5f * v.length();
    dir = victim_origin - ent->s.origin;

    Damage(victim, ent, ent->owner, dir, ent->s.origin, vec3_origin,
           (int)points, (int)points,
           DamageFlags::Radius | DamageFlags::StatOnce, mod);
  } else {
    victim = nullptr;
    ent->enemy = nullptr;
  }

  RadiusDamage(ent, ent->owner, (float)ent->dmg, victim, ent->splashRadius,
               DamageFlags::Normal | DamageFlags::StatOnce, mod);

  origin = ent->s.origin + (ent->velocity * -0.02f);
  gi.WriteByte(svc_temp_entity);
  if (ent->waterLevel) {
    if (ent->groundEntity)
      gi.WriteByte(TE_GRENADE_EXPLOSION_WATER);
    else
      gi.WriteByte(TE_ROCKET_EXPLOSION_WATER);
  } else {
    if (ent->groundEntity)
      gi.WriteByte(TE_GRENADE_EXPLOSION);
    else
      gi.WriteByte(TE_ROCKET_EXPLOSION);
  }
  gi.WritePosition(origin);
  gi.multicast(ent->s.origin, MULTICAST_PHS, false);

  FreeEntity(ent);
}

static TOUCH(Grenade_Touch)(gentity_t *ent, gentity_t *other, const trace_t &tr,
                            bool otherTouchingSelf)
    ->void {
  if (other == ent->owner)
    return;

  if (tr.surface && (tr.surface->flags & SURF_SKY)) {
    FreeEntity(ent);
    return;
  }

  if (!other->takeDamage) {
    if (ent->spawnFlags.has(SPAWNFLAG_GRENADE_HAND)) {
      if (frandom() > 0.5f)
        gi.sound(ent, CHAN_VOICE, gi.soundIndex("weapons/hgrenb1a.wav"), 1,
                 ATTN_NORM, 0);
      else
        gi.sound(ent, CHAN_VOICE, gi.soundIndex("weapons/hgrenb2a.wav"), 1,
                 ATTN_NORM, 0);
    } else {
      gi.sound(ent, CHAN_VOICE, gi.soundIndex("weapons/grenlb1b.wav"), 1,
               ATTN_NORM, 0);
    }
    return;
  }

  if (Game::Is(GameType::ProBall)) {
    if ((tr.contents & CONTENTS_LAVA) || (tr.contents & CONTENTS_SLIME)) {
      FreeEntity(ent);
      return;
    }
    if (other->client) {
      other->client->pers.inventory[IT_BALL] = 1;
    }
  }

  ent->enemy = other;
  Grenade_Explode(ent);
}

static THINK(Grenade4_Think)(gentity_t *self)->void {
  if (level.time >= self->timeStamp) {
    Grenade_Explode(self);
    return;
  }

  if (self->velocity) {
    float p = self->s.angles.x;
    float z = self->s.angles.z;
    float speed_frac = std::clamp(
        self->velocity.lengthSquared() / (self->speed * self->speed), 0.f, 1.f);
    self->s.angles = VectorToAngles(self->velocity);
    self->s.angles.x = LerpAngle(p, self->s.angles.x, speed_frac);
    self->s.angles.z = z + (gi.frameTimeSec * 360 * speed_frac);
  }

  self->nextThink = level.time + FRAME_TIME_S;
}

void fire_grenade(gentity_t *self, const Vector3 &start, const Vector3 &aimDir,
                  int damage, int speed, GameTime timer, float splashRadius,
                  float rightAdjust, float upAdjust, bool monster) {
  gentity_t *grenade;
  Vector3 dir;
  Vector3 forward, right, up;

  dir = VectorToAngles(aimDir);
  AngleVectors(dir, forward, right, up);

  grenade = Spawn();
  grenade->s.origin = start;
  grenade->velocity = aimDir * speed;

  if (upAdjust) {
    float gravityAdjustment = level.gravity / 800.f;
    grenade->velocity += up * upAdjust * gravityAdjustment;
  }

  if (rightAdjust)
    grenade->velocity += right * rightAdjust;

  grenade->moveType = MoveType::Bounce;
  grenade->clipMask = MASK_PROJECTILE;
  // [Paril-KEX]
  if (self->client && !G_ShouldPlayersCollide(true))
    grenade->clipMask &= ~CONTENTS_PLAYER;
  grenade->solid = SOLID_BBOX;
  grenade->svFlags |= SVF_PROJECTILE;
  grenade->flags |= (FL_DODGE | FL_TRAP);
  grenade->s.effects |= EF_GRENADE;
  grenade->speed = speed;
  grenade->s.scale = 1.25f;

  if (monster) {
    grenade->aVelocity = {crandom() * 360, crandom() * 360, crandom() * 360};
    grenade->s.modelIndex = gi.modelIndex("models/objects/grenade/tris.md2");
    grenade->nextThink = level.time + timer;
    grenade->think = Grenade_Explode;
    grenade->s.effects |= EF_GRENADE_LIGHT;
  } else {
    grenade->s.modelIndex = gi.modelIndex("models/objects/grenade4/tris.md2");
    grenade->s.angles = VectorToAngles(grenade->velocity);
    grenade->nextThink = level.time + FRAME_TIME_S;
    grenade->timeStamp = level.time + timer;
    grenade->think = Grenade4_Think;
    grenade->s.renderFX |= RF_MINLIGHT;
  }
  grenade->owner = self;
  grenade->touch = Grenade_Touch;
  grenade->dmg = damage;
  grenade->splashRadius = splashRadius;
  grenade->className = "grenade";

  gi.linkEntity(grenade);
}

void fire_handgrenade(gentity_t *self, const Vector3 &start,
                      const Vector3 &aimDir, int damage, int speed,
                      GameTime timer, float splashRadius, bool held) {
  gentity_t *grenade;
  Vector3 dir;
  Vector3 forward, right, up;

  dir = VectorToAngles(aimDir);
  AngleVectors(dir, forward, right, up);

  grenade = Spawn();
  grenade->s.origin = start;
  grenade->velocity = aimDir * speed;

  float gravityAdjustment = level.gravity / 800.f;

  grenade->velocity += up * (200 + crandom() * 10.0f) * gravityAdjustment;
  grenade->velocity += right * (crandom() * 10.0f);

  grenade->aVelocity = {crandom() * 360, crandom() * 360, crandom() * 360};
  grenade->moveType = MoveType::Bounce;
  grenade->clipMask = MASK_PROJECTILE;
  // [Paril-KEX]
  if (self->client && !G_ShouldPlayersCollide(true))
    grenade->clipMask &= ~CONTENTS_PLAYER;

  grenade->flags |= (FL_DODGE | FL_TRAP);

  if (Game::Is(GameType::ProBall)) {
    Item *it = GetItemByIndex(IT_BALL);
    if (it)
      Drop_Item(self, it);
    // return;
    /*
    grenade->s.effects |= EF_GRENADE | EF_COLOR_SHELL;
    grenade->s.renderFX |= RF_GLOW | RF_NO_LOD | RF_IR_VISIBLE | RF_SHELL_RED |
    RF_SHELL_GREEN; grenade->s.modelIndex =
    gi.modelIndex("models/items/ammo/grenades/medium/tris.md2");
    grenade->s.scale = 4.0f;
    grenade->mins = { -15, -15, -15 };
    grenade->maxs = { 15, 15, 15 };
    grenade->moveType = MoveType::Toss;
    grenade->solid = SOLID_TRIGGER;
    */
  } else {
    grenade->solid = SOLID_BBOX;
    grenade->svFlags |= SVF_PROJECTILE;

    grenade->s.effects |= EF_GRENADE;
    grenade->s.modelIndex = gi.modelIndex("models/objects/grenade3/tris.md2");
    grenade->s.scale = 1.25f;
  }

  grenade->owner = self;
  grenade->touch = Grenade_Touch;
  grenade->nextThink = level.time + timer;
  grenade->think = Grenade_Explode;
  grenade->dmg = damage;
  grenade->splashRadius = splashRadius;
  grenade->className = "hand_grenade";
  grenade->spawnFlags = SPAWNFLAG_GRENADE_HAND;
  if (held)
    grenade->spawnFlags |= SPAWNFLAG_GRENADE_HELD;
  grenade->s.sound = gi.soundIndex("weapons/hgrenc1b.wav");

  if (timer <= 0_ms)
    Grenade_Explode(grenade);
  else {
    gi.sound(self, CHAN_WEAPON, gi.soundIndex("weapons/hgrent1a.wav"), 1,
             ATTN_NORM, 0);
    gi.linkEntity(grenade);
  }
}

/*
=================
fire_rocket
=================
*/
static TOUCH(rocket_touch)(gentity_t *ent, gentity_t *other, const trace_t &tr,
                           bool otherTouchingSelf)
    ->void {
  Vector3 origin;
  if (other == ent->owner)
    return;

  if (tr.surface && (tr.surface->flags & SURF_SKY)) {
    FreeEntity(ent);
    return;
  }

  if (ent->owner->client)
    G_PlayerNoise(ent->owner, ent->s.origin, PlayerNoise::Impact);

  // calculate position for the explosion entity
  origin = ent->s.origin + tr.plane.normal;

  if (other->takeDamage) {
    Damage(other, ent, ent->owner, ent->velocity, ent->s.origin,
           tr.plane.normal, ent->dmg, 50,
           DamageFlags::Normal | DamageFlags::StatOnce, ModID::RocketLauncher);
  } else {
    // don't throw any debris in net games
    if (!deathmatch->integer && !coop->integer) {
      if (tr.surface && !(tr.surface->flags & (SURF_WARP | SURF_TRANS33 |
                                               SURF_TRANS66 | SURF_FLOWING))) {
        ThrowGibs(ent, 2,
                  {{(size_t)irandom(5), "models/objects/debris2/tris.md2",
                    GIB_METALLIC | GIB_DEBRIS}});
      }
    }
  }

  RadiusDamage(ent, ent->owner, (float)ent->splashDamage, other,
               ent->splashRadius, DamageFlags::Normal,
               ModID::RocketLauncher_Splash);

  gi.WriteByte(svc_temp_entity);
  if (ent->waterLevel)
    gi.WriteByte(TE_ROCKET_EXPLOSION_WATER);
  else
    gi.WriteByte(TE_ROCKET_EXPLOSION);
  gi.WritePosition(origin);
  gi.multicast(ent->s.origin, MULTICAST_PHS, false);

  FreeEntity(ent);
}

gentity_t *fire_rocket(gentity_t *self, const Vector3 &start,
                       const Vector3 &dir, int damage, int speed,
                       float splashRadius, int splashDamage) {
  gentity_t *rocket;

  rocket = Spawn();
  rocket->s.origin = start;
  rocket->s.angles = VectorToAngles(dir);
  rocket->velocity = dir * speed;
  rocket->moveType = MoveType::FlyMissile;
  rocket->svFlags |= SVF_PROJECTILE;
  rocket->flags |= FL_DODGE;
  rocket->clipMask = MASK_PROJECTILE;
  // [Paril-KEX]
  if (self->client && !G_ShouldPlayersCollide(true))
    rocket->clipMask &= ~CONTENTS_PLAYER;
  rocket->solid = SOLID_BBOX;
  rocket->s.effects |= EF_ROCKET;
  rocket->s.modelIndex = gi.modelIndex("models/objects/rocket/tris.md2");
  rocket->owner = self;
  rocket->touch = rocket_touch;
  rocket->nextThink = level.time + GameTime::from_sec(8000.f / speed);
  rocket->think = FreeEntity;
  rocket->dmg = damage;
  rocket->splashDamage = splashDamage;
  rocket->splashRadius = splashRadius;
  rocket->s.sound = gi.soundIndex("weapons/rockfly.wav");
  rocket->className = "rocket";

  gi.linkEntity(rocket);

  return rocket;
}

using search_callback_t = decltype(game_import_t::inPVS);

static bool binary_positional_search_r(const Vector3 &viewer,
                                       const Vector3 &start, const Vector3 &end,
                                       search_callback_t cb,
                                       int32_t split_num) {
  // check half-way point
  Vector3 mid = (start + end) * 0.5f;

  if (cb(viewer, mid, true))
    return true;

  // no more splits
  if (!split_num)
    return false;

  // recursively check both sides
  return binary_positional_search_r(viewer, start, mid, cb, split_num - 1) ||
         binary_positional_search_r(viewer, mid, end, cb, split_num - 1);
}

// [Paril-KEX] simple binary search through a line to see if any points along
// the line (in a binary split) pass the callback
static bool binary_positional_search(const Vector3 &viewer,
                                     const Vector3 &start, const Vector3 &end,
                                     search_callback_t cb, int32_t num_splits) {
  // check start/end first
  if (cb(viewer, start, true) || cb(viewer, end, true))
    return true;

  // recursive split
  return binary_positional_search_r(viewer, start, end, cb, num_splits);
}

struct fire_rail_pierce_t : pierce_args_t {
  gentity_t *self;
  Vector3 aimDir;
  int damage;
  int kick;
  bool water = false;

  inline fire_rail_pierce_t(gentity_t *self, Vector3 aimDir, int damage,
                            int kick)
      : pierce_args_t(), self(self), aimDir(aimDir), damage(damage),
        kick(kick) {}

  // we hit an entity; return false to stop the piercing.
  // you can adjust the mask for the re-trace (for water, etc).
  bool hit(contents_t &mask, Vector3 &end) override {
    if (tr.contents & (CONTENTS_SLIME | CONTENTS_LAVA)) {
      mask &= ~(CONTENTS_SLIME | CONTENTS_LAVA);
      water = true;
      return true;
    } else {
      self->skip = false;
      // try to kill it first
      if ((tr.ent != self) && (tr.ent->takeDamage))
        Damage(tr.ent, self, self, aimDir, tr.endPos, tr.plane.normal, damage,
               kick, DamageFlags::Energy | DamageFlags::StatOnce,
               ModID::Railgun);

      // dead, so we don't need to care about checking pierce
      if (!tr.ent->inUse || (!tr.ent->solid || tr.ent->solid == SOLID_TRIGGER))
        return true;

      // rail goes through SOLID_BBOX entities (gibs, etc)
      if ((tr.ent->svFlags & SVF_MONSTER) || (tr.ent->client) ||
          (tr.ent->flags & FL_DAMAGEABLE) || (tr.ent->solid == SOLID_BBOX)) {
        if (!mark(tr.ent))
          return false;

        return true;
      }
    }

    return false;
  }
};

// [Paril-KEX] get the current unique unicast key
uint32_t GetUnicastKey() {
  static uint32_t key = 1;

  if (!key)
    return key = 1;

  return key++;
}

/*
=================
fire_rail
=================
*/
void fire_rail(gentity_t *self, const Vector3 &start, const Vector3 &aimDir,
               int damage, int kick) {
  fire_rail_pierce_t args = {self, aimDir, damage, kick};

  contents_t mask = MASK_PROJECTILE | CONTENTS_SLIME | CONTENTS_LAVA;

  // [Paril-KEX]
  if (self->client && !G_ShouldPlayersCollide(true))
    mask &= ~CONTENTS_PLAYER;

  Vector3 end = start + (aimDir * 8192);

  pierce_trace(start, end, self, args, mask);

  uint32_t unicast_key = GetUnicastKey();

  // send gun puff / flash
  // [Paril-KEX] this often makes double noise, so trying
  // a slightly different approach...
  for (auto player : active_clients()) {
    Vector3 org = player->s.origin + player->client->ps.viewOffset +
                  Vector3{0, 0, (float)player->client->ps.pmove.viewHeight};

    if (binary_positional_search(org, start, args.tr.endPos, gi.inPHS, 3)) {
      gi.WriteByte(svc_temp_entity);
      gi.WriteByte((deathmatch->integer && g_instaGib->integer) ? TE_RAILTRAIL2
                                                                : TE_RAILTRAIL);
      gi.WritePosition(start);
      gi.WritePosition(args.tr.endPos);
      gi.unicast(player, false, unicast_key);
    }
  }

  if (g_instaGib->integer && g_instagib_splash->integer) {
    gentity_t *exp;

    exp = Spawn();
    exp->className = "railsplash";
    exp->s.origin = args.tr.endPos;
    exp->s.angles = VectorToAngles(aimDir);
    exp->clipMask = MASK_PROJECTILE;
    exp->owner = self;
    exp->dmg = 180;
    exp->splashDamage = 120;
    exp->splashRadius = 120;

    gi.linkEntity(exp);

    RadiusDamage(exp, exp->owner, exp->dmg, nullptr, exp->splashRadius,
                 DamageFlags::Normal, ModID::Railgun_Splash);

    gi.WriteByte(svc_temp_entity);
    if (exp->waterLevel)
      gi.WriteByte(TE_ROCKET_EXPLOSION_WATER);
    else
      gi.WriteByte(TE_ROCKET_EXPLOSION);
    gi.WritePosition(exp->s.origin);
    gi.multicast(exp->s.origin, MULTICAST_PHS, false);

    FreeEntity(exp);
  }

  if (self->client)
    G_PlayerNoise(self, args.tr.endPos, PlayerNoise::Impact);
}

static Vector3 bfg_laser_pos(Vector3 p, float dist) {
  float theta = frandom(2 * PIf);
  float phi = acos(crandom());

  Vector3 d{sin(phi) * cos(theta), sin(phi) * sin(theta), cos(phi)};

  return p + (d * dist);
}

static THINK(bfg_laser_update)(gentity_t *self)->void {
  if (level.time > self->timeStamp || !self->owner->inUse) {
    FreeEntity(self);
    return;
  }

  self->s.origin = self->owner->s.origin;
  self->nextThink = level.time + 1_ms;
  gi.linkEntity(self);
}

static void bfg_spawn_laser(gentity_t *self) {
  Vector3 end = bfg_laser_pos(self->s.origin, 256);
  trace_t tr = gi.traceLine(self->s.origin, end, self, MASK_OPAQUE);

  if (tr.fraction == 1.0f)
    return;

  gentity_t *laser = Spawn();
  laser->s.frame = 3;
  laser->s.renderFX = RF_BEAM_LIGHTNING;
  laser->moveType = MoveType::None;
  laser->solid = SOLID_NOT;
  laser->s.modelIndex = MODELINDEX_WORLD; // must be non-zero
  laser->s.origin = self->s.origin;
  laser->s.oldOrigin = tr.endPos;
  laser->s.skinNum = 0xD0D0D0D0;
  laser->think = bfg_laser_update;
  laser->nextThink = level.time + 1_ms;
  laser->timeStamp = level.time + 300_ms;
  laser->owner = self;
  gi.linkEntity(laser);
}

/*
=================
fire_bfg
=================
*/
static THINK(bfg_explode)(gentity_t *self)->void {
  gentity_t *ent;
  float points;
  Vector3 v;
  float dist;

  bfg_spawn_laser(self);

  if (self->s.frame == 0) {
    // the BFG effect
    ent = nullptr;
    while ((ent = FindRadius(ent, self->s.origin, self->splashRadius)) !=
           nullptr) {
      if (!ent->takeDamage)
        continue;
      if (ent == self->owner)
        continue;
      if (ent->client && ent->client->eliminated)
        continue;
      if (!CanDamage(ent, self))
        continue;
      if (!CanDamage(ent, self->owner))
        continue;
      // make tesla hurt by bfg
      if (!(ent->svFlags & SVF_MONSTER) && !(ent->flags & FL_DAMAGEABLE) &&
          (!ent->client) && (strcmp(ent->className, "misc_explobox") != 0))
        continue;
      // don't target team mates during teamplay if we can't damage them
      if (CheckTeamDamage(ent, self->owner))
        continue;

      v = ent->mins + ent->maxs;
      v = ent->s.origin + (v * 0.5f);
      Vector3 centroid = v;
      v = self->s.origin - centroid;
      dist = v.length();
      points = self->splashDamage * (1.0f - sqrtf(dist / self->splashRadius));

      Damage(ent, self, self->owner, self->velocity, centroid, vec3_origin,
             (int)points, 0, DamageFlags::Energy | DamageFlags::StatOnce,
             ModID::BFG10K_Effect);

      // Paril: draw BFG lightning laser to enemies
      gi.WriteByte(svc_temp_entity);
      gi.WriteByte(TE_BFG_ZAP);
      gi.WritePosition(self->s.origin);
      gi.WritePosition(centroid);
      gi.multicast(self->s.origin, MULTICAST_PHS, false);
    }
  }

  self->nextThink = level.time + 10_hz;
  self->s.frame++;
  if (self->s.frame == 5)
    self->think = FreeEntity;
}

static TOUCH(bfg_touch)(gentity_t *self, gentity_t *other, const trace_t &tr,
                        bool otherTouchingSelf)
    ->void {
  if (other == self->owner)
    return;

  if (tr.surface && (tr.surface->flags & SURF_SKY)) {
    FreeEntity(self);
    return;
  }

  if (self->owner->client)
    G_PlayerNoise(self->owner, self->s.origin, PlayerNoise::Impact);

  // core explosion - prevents firing it into the wall/floor
  if (other->takeDamage)
    Damage(other, self, self->owner, self->velocity, self->s.origin,
           tr.plane.normal, 200, 0, DamageFlags::Energy, ModID::BFG10K_Blast);
  RadiusDamage(self, self->owner, 200, other, 100,
               DamageFlags::Energy | DamageFlags::StatOnce,
               ModID::BFG10K_Blast);

  gi.sound(self, CHAN_VOICE, gi.soundIndex("weapons/bfg__x1b.wav"), 1,
           ATTN_NORM, 0);
  self->solid = SOLID_NOT;
  self->touch = nullptr;
  self->s.origin += self->velocity * (-1 * gi.frameTimeSec);
  self->velocity = {};
  self->s.modelIndex = gi.modelIndex("sprites/s_bfg3.sp2");
  self->s.frame = 0;
  self->s.sound = 0;
  self->s.effects &= ~EF_ANIM_ALLFAST;
  self->think = bfg_explode;
  self->nextThink = level.time + 10_hz;
  self->enemy = other;

  gi.WriteByte(svc_temp_entity);
  gi.WriteByte(TE_BFG_BIGEXPLOSION);
  gi.WritePosition(self->s.origin);
  gi.multicast(self->s.origin, MULTICAST_PHS, false);
}

struct bfg_laser_pierce_t : pierce_args_t {
  gentity_t *self;
  Vector3 dir;
  int damage;

  inline bfg_laser_pierce_t(gentity_t *self, Vector3 dir, int damage)
      : pierce_args_t(), self(self), dir(dir), damage(damage) {}

  // we hit an entity; return false to stop the piercing.
  // you can adjust the mask for the re-trace (for water, etc).
  bool hit(contents_t &mask, Vector3 &end) override {
    // hurt it if we can
    if ((tr.ent->takeDamage) && !(tr.ent->flags & FL_IMMUNE_LASER) &&
        (tr.ent != self->owner))
      Damage(tr.ent, self, self->owner, dir, tr.endPos, vec3_origin, damage, 1,
             DamageFlags::Energy, ModID::BFG10K_Laser);

    // if we hit something that's not a monster or player we're done
    if (!(tr.ent->svFlags & SVF_MONSTER) && !(tr.ent->flags & FL_DAMAGEABLE) &&
        (!tr.ent->client)) {
      gi.WriteByte(svc_temp_entity);
      gi.WriteByte(TE_LASER_SPARKS);
      gi.WriteByte(4);
      gi.WritePosition(tr.endPos);
      gi.WriteDir(tr.plane.normal);
      gi.WriteByte(self->s.skinNum);
      gi.multicast(tr.endPos, MULTICAST_PVS, false);
      return false;
    }

    if (!mark(tr.ent))
      return false;

    return true;
  }
};

static THINK(bfg_think)(gentity_t *self)->void {
  gentity_t *ent;
  Vector3 point;
  Vector3 dir;
  Vector3 start;
  Vector3 end;
  int dmg;
  trace_t tr;

  dmg = deathmatch->integer ? 5 : 10;

  bfg_spawn_laser(self);

  ent = nullptr;
  while ((ent = FindRadius(ent, self->s.origin, 256)) != nullptr) {
    if (ent == self)
      continue;
    if (ent == self->owner)
      continue;
    if (ent->client && ent->client->eliminated)
      continue;
    if (!ent->takeDamage)
      continue;

    // make tesla hurt by bfg
    if (!(ent->svFlags & SVF_MONSTER) && !(ent->flags & FL_DAMAGEABLE) &&
        (!ent->client) && (strcmp(ent->className, "misc_explobox") != 0))
      continue;
    // don't target team mates during teamplay if we can't damage them
    if (CheckTeamDamage(ent, self->owner))
      continue;

    point = (ent->absMin + ent->absMax) * 0.5f;

    dir = point - self->s.origin;
    dir.normalize();

    start = self->s.origin;
    end = start + (dir * 2048);

    // [Paril-KEX] don't fire a laser if we're blocked by the world
    tr = gi.traceLine(start, point, nullptr, MASK_SOLID);

    if (tr.fraction < 1.0f)
      continue;

    bfg_laser_pierce_t args{self, dir, dmg};

    pierce_trace(start, end, self, args,
                 CONTENTS_SOLID | CONTENTS_MONSTER | CONTENTS_PLAYER |
                     CONTENTS_DEADMONSTER);

    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_BFG_LASER);
    gi.WritePosition(self->s.origin);
    gi.WritePosition(tr.endPos);
    gi.multicast(self->s.origin, MULTICAST_PHS, false);
  }

  self->nextThink = level.time + 10_hz;
}

void fire_bfg(gentity_t *self, const Vector3 &start, const Vector3 &dir,
              int damage, int speed, float splashRadius) {
  gentity_t *bfg;

  bfg = Spawn();
  bfg->s.origin = start;
  bfg->s.angles = VectorToAngles(dir);
  bfg->velocity = dir * speed;
  bfg->moveType = MoveType::FlyMissile;
  bfg->clipMask = MASK_PROJECTILE;
  bfg->svFlags = SVF_PROJECTILE;
  // [Paril-KEX]
  if (self->client && !G_ShouldPlayersCollide(true))
    bfg->clipMask &= ~CONTENTS_PLAYER;
  bfg->solid = SOLID_BBOX;
  bfg->s.effects |= EF_BFG | EF_ANIM_ALLFAST;
  bfg->s.modelIndex = gi.modelIndex("sprites/s_bfg1.sp2");
  bfg->owner = self;
  bfg->touch = bfg_touch;
  bfg->nextThink = level.time + GameTime::from_sec(8000.f / speed);
  bfg->think = FreeEntity;
  bfg->splashDamage = damage;
  bfg->splashRadius = splashRadius;
  bfg->className = "bfg blast";
  bfg->s.sound = gi.soundIndex("weapons/bfg__l1a.wav");

  bfg->think = bfg_think;
  bfg->nextThink = level.time + FRAME_TIME_S;
  bfg->teamMaster = bfg;
  bfg->teamChain = nullptr;

  gi.linkEntity(bfg);
}

static TOUCH(disintegrator_touch)(gentity_t *self, gentity_t *other,
                                  const trace_t &tr, bool otherTouchingSelf)
    ->void {
  gi.WriteByte(svc_temp_entity);
  gi.WriteByte(TE_WIDOWSPLASH);
  gi.WritePosition(self->s.origin - (self->velocity * 0.01f));
  gi.multicast(self->s.origin, MULTICAST_PHS, false);

  FreeEntity(self);

  if (other->svFlags & (SVF_MONSTER | SVF_PLAYER)) {
    other->disintegrator_time += 50_sec;
    other->disintegrator = self->owner;
  }
}

void fire_disintegrator(gentity_t *self, const Vector3 &start,
                        const Vector3 &forward, int speed) {
  gentity_t *bfg;

  bfg = Spawn();
  bfg->s.origin = start;
  bfg->s.angles = VectorToAngles(forward);
  bfg->velocity = forward * speed;
  bfg->moveType = MoveType::FlyMissile;
  bfg->clipMask = MASK_PROJECTILE;
  // [Paril-KEX]
  if (self->client && !G_ShouldPlayersCollide(true))
    bfg->clipMask &= ~CONTENTS_PLAYER;
  bfg->solid = SOLID_BBOX;
  bfg->s.effects |= EF_TAGTRAIL | EF_ANIM_ALL;
  bfg->s.renderFX |= RF_TRANSLUCENT;
  bfg->svFlags |= SVF_PROJECTILE;
  bfg->flags |= FL_DODGE;
  bfg->s.modelIndex = gi.modelIndex("sprites/s_bfg1.sp2");
  bfg->owner = self;
  bfg->touch = disintegrator_touch;
  bfg->nextThink = level.time + GameTime::from_sec(8000.f / speed);
  bfg->think = FreeEntity;
  bfg->className = "disint ball";
  bfg->s.sound = gi.soundIndex("weapons/bfg__l1a.wav");

  gi.linkEntity(bfg);
}

// *************************
//  PLASMA BEAM
// *************************

static void fire_beams(gentity_t *self, const Vector3 &start,
                       const Vector3 &aimDir, const Vector3 &offset, int damage,
                       int kick, int te_beam, int te_impact, MeansOfDeath mod) {
  trace_t tr;
  Vector3 dir;
  Vector3 forward, right, up;
  Vector3 end;
  Vector3 water_start, endpoint;
  bool water = false, underwater = false;
  contents_t content_mask = MASK_PROJECTILE | MASK_WATER;

  // [Paril-KEX]
  if (self->client && !G_ShouldPlayersCollide(true))
    content_mask &= ~CONTENTS_PLAYER;

  dir = VectorToAngles(aimDir);
  AngleVectors(dir, forward, right, up);

  int length = RS(Quake1) ? 600 : 768;
  end = start + (forward * length);

  if (gi.pointContents(start) & MASK_WATER) {
    underwater = true;
    water_start = start;
    content_mask &= ~MASK_WATER;
  }

  tr = gi.traceLine(start, end, self, content_mask);

  // see if we hit water
  if (tr.contents & MASK_WATER) {
    water = true;
    water_start = tr.endPos;

    if (start != tr.endPos) {
      gi.WriteByte(svc_temp_entity);
      gi.WriteByte(TE_HEATBEAM_SPARKS);
      gi.WritePosition(water_start);
      gi.WriteDir(tr.plane.normal);
      gi.multicast(tr.endPos, MULTICAST_PVS, false);
    }
    // re-trace ignoring water this time
    tr = gi.traceLine(water_start, end, self, content_mask & ~MASK_WATER);
  }
  endpoint = tr.endPos;

  // halve the damage if target underwater
  if (water)
    damage = damage / 2;

  // send gun puff / flash
  if (!((tr.surface) && (tr.surface->flags & SURF_SKY))) {
    if (tr.fraction < 1.0f) {
      if (tr.ent->takeDamage) {
        Damage(tr.ent, self, self, aimDir, tr.endPos, tr.plane.normal, damage,
               kick, DamageFlags::Energy, mod);
      } else {
        if ((!water) && !(tr.surface && (tr.surface->flags & SURF_SKY))) {
          // This is the truncated steam entry - uses 1+1+2 extra bytes of data
          gi.WriteByte(svc_temp_entity);
          gi.WriteByte(TE_HEATBEAM_STEAM);
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
  if ((water) || (underwater)) {
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
    gi.WriteByte(TE_BUBBLETRAIL2);
    gi.WritePosition(water_start);
    gi.WritePosition(tr.endPos);
    gi.multicast(pos, MULTICAST_PVS, false);
  }

  gi.WriteByte(svc_temp_entity);
  gi.WriteByte(te_beam);
  gi.WriteEntity(self);
  gi.WritePosition(start);
  gi.WritePosition(!underwater && !water ? tr.endPos : endpoint);
  gi.multicast(self->s.origin, MULTICAST_ALL, false);
}

/*
=================
fire_plasmabeam

Fires a single heat beam.
=================
*/
void fire_plasmabeam(gentity_t *self, const Vector3 &start,
                     const Vector3 &aimDir, const Vector3 &offset, int damage,
                     int kick, bool monster) {
  if (monster)
    fire_beams(self, start, aimDir, offset, damage, kick, TE_MONSTER_HEATBEAM,
               TE_HEATBEAM_SPARKS, ModID::PlasmaBeam);
  else
    fire_beams(self, start, aimDir, offset, damage, kick, TE_HEATBEAM,
               TE_HEATBEAM_SPARKS, ModID::PlasmaBeam);
}

static void SpawnThunderboltBeam(gentity_t *self, const Vector3 &start,
                                 const Vector3 &end) {
  gentity_t *beam = Spawn();
  if (!beam)
    return;

  beam->className = "thunderbolt_beam";
  beam->owner = self;
  beam->moveType = MoveType::None;
  beam->solid = SOLID_NOT;
  beam->s.modelIndex = gi.modelIndex("models/proj/lightning/tris.md2");
  beam->s.renderFX = RF_BEAM;
  beam->s.effects |= EF_ANIM_ALLFAST;
  beam->s.origin = start;
  beam->s.oldOrigin = end;
  beam->think = FreeEntity;
  beam->nextThink = level.time + FRAME_TIME_MS;

  gi.linkEntity(beam);
}

/*
===============
fire_thunderbolt
Q1-style instant lightning beam using RF_BEAM_LIGHTNING and tesla zap effects.
===============
*/
bool fire_thunderbolt(gentity_t *self, const Vector3 &start,
                      const Vector3 &aimDir, const Vector3 &offset, int damage,
                      int kick, MeansOfDeath mod, int damageMultiplier) {
  trace_t tr;
  Vector3 dir;
  Vector3 forward;
  Vector3 end;
  Vector3 water_start, endpoint;
  bool water = false, underwater = false;
  contents_t content_mask = MASK_PROJECTILE | MASK_WATER;

  Vector3 beamStart = start;
  if (self->client) {
    Vector3 beamDir;
    P_ProjectSource(self, self->client->vAngle, offset, beamStart, beamDir);
  }

  // [Paril-KEX]
  if (self->client && !G_ShouldPlayersCollide(true))
    content_mask &= ~CONTENTS_PLAYER;

  dir = VectorToAngles(aimDir);
  AngleVectors(dir, forward, nullptr, nullptr);

  int length = RS(Quake1) ? 600 : (RS(Quake3Arena) ? 768 : 700);
  end = start + (forward * length);

  if (gi.pointContents(start) & MASK_WATER) {
    underwater = true;
    water_start = start;
    content_mask &= ~MASK_WATER;
  }

  // Thunderbolt Discharge: if *actually firing* while underwater
  if (underwater && self->client) {
    const int ammoIdx = self->client->pers.weapon->ammo;
    int cells = self->client->pers.inventory[ammoIdx];
    const int maxCells =
        self->client->pers.ammoMax[static_cast<int>(AmmoID::Cells)];
    const bool infiniteAmmo = InfiniteAmmoOn(self->client->pers.weapon);

    if (cells > maxCells)
      cells = maxCells;

    if (!infiniteAmmo)
      self->client->pers.inventory[ammoIdx] = 0;

    const float dischargeDamage = 35.0f * static_cast<float>(cells) *
                                  static_cast<float>(damageMultiplier);
    if (dischargeDamage > 0.0f) {
      const float dischargeRadius = dischargeDamage + 40.0f;
      RadiusDamage(self, self, dischargeDamage, self, dischargeRadius,
                   DamageFlags::Energy | DamageFlags::StatOnce,
                   ModID::Thunderbolt_Discharge);
      Damage(self, self, self, aimDir, self->s.origin, vec3_origin,
             static_cast<int>(dischargeDamage * 0.5f), 0,
             DamageFlags::Energy | DamageFlags::StatOnce,
             ModID::Thunderbolt_Discharge);
    }

    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_ELECTRIC_SPARKS);
    gi.WritePosition(self->s.origin);
    gi.WriteDir(Vector3{0, 0, 1});
    gi.multicast(self->s.origin, MULTICAST_PVS, false);

    return true;
  }

  tr = gi.traceLine(start, end, self, content_mask);

  // see if we hit water
  if (tr.contents & MASK_WATER) {
    water = true;
    water_start = tr.endPos;

    if (start != tr.endPos) {
      gi.WriteByte(svc_temp_entity);
      gi.WriteByte(TE_ELECTRIC_SPARKS);
      gi.WritePosition(water_start);
      gi.WriteDir(tr.plane.normal);
      gi.multicast(tr.endPos, MULTICAST_PVS, false);
    }
    // re-trace ignoring water this time
    tr = gi.traceLine(water_start, end, self, content_mask & ~MASK_WATER);
  }
  endpoint = tr.endPos;

  // halve the damage if target underwater
  if (water)
    damage = damage / 2;

  gentity_t *hit1 = nullptr;
  gentity_t *hit2 = nullptr;
  auto apply_damage = [&](const trace_t &hit) {
    if (!hit.ent || !hit.ent->takeDamage)
      return;

    if (hit.ent == hit1 || hit.ent == hit2)
      return;

    Damage(hit.ent, self, self, aimDir, hit.endPos, hit.plane.normal, damage,
           kick, DamageFlags::Energy, mod);

    if (!hit1)
      hit1 = hit.ent;
    else
      hit2 = hit.ent;
  };

  apply_damage(tr);

  Vector3 side = {-forward[_Y], forward[_X], 0.0f};
  if (side.length() > 0.1f) {
    side.normalize();
    side *= 16.0f;
    const contents_t damage_mask = content_mask & ~MASK_WATER;
    apply_damage(gi.traceLine(start + side, end + side, self, damage_mask));
    apply_damage(gi.traceLine(start - side, end - side, self, damage_mask));
  }

  if (!((tr.surface) && (tr.surface->flags & SURF_SKY))) {
    if (tr.fraction < 1.0f && !water && (!tr.ent || !tr.ent->takeDamage)) {
      gi.WriteByte(svc_temp_entity);
      gi.WriteByte(TE_ELECTRIC_SPARKS);
      gi.WritePosition(tr.endPos);
      gi.WriteDir(tr.plane.normal);
      gi.multicast(tr.endPos, MULTICAST_PVS, false);

      if (self->client)
        G_PlayerNoise(self, tr.endPos, PlayerNoise::Impact);
    }
  }

  // if went through water, determine where the end and make a bubble trail
  if ((water) || (underwater)) {
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
    gi.WriteByte(TE_BUBBLETRAIL2);
    gi.WritePosition(water_start);
    gi.WritePosition(tr.endPos);
    gi.multicast(pos, MULTICAST_PVS, false);
  }

  SpawnThunderboltBeam(self, beamStart,
                       (!underwater && !water) ? tr.endPos : endpoint);
  return false;
}

/*
=================
fire_disruptor
=================
*/
constexpr DamageFlags DISRUPTOR_DAMAGE_FLAGS =
    (DamageFlags::NoPowerArmor | DamageFlags::Energy |
     DamageFlags::NoKnockback);
constexpr DamageFlags DISRUPTOR_IMPACT_FLAGS =
    (DamageFlags::NoPowerArmor | DamageFlags::Energy);

constexpr GameTime DISRUPTOR_DAMAGE_TIME = 500_ms;

static THINK(disruptor_pain_daemon_think)(gentity_t *self)->void {
  static constexpr Vector3 pain_normal = {0, 0, 1};
  int hurt;

  if (!self->inUse)
    return;

  if ((level.time - self->timeStamp) > DISRUPTOR_DAMAGE_TIME) {
    if (!self->enemy->client)
      self->enemy->s.effects &= ~EF_TRACKERTRAIL;
    FreeEntity(self);
  } else {
    if (self->enemy->health > 0) {
      Vector3 center = (self->enemy->absMax + self->enemy->absMin) * 0.5f;

      Damage(self->enemy, self, self->owner, vec3_origin, center, pain_normal,
             self->dmg, 0, DISRUPTOR_DAMAGE_FLAGS | DamageFlags::StatOnce,
             ModID::Tracker);

      // if we kill the player, we'll be removed.
      if (self->inUse) {
        // if we killed a monster, gib them.
        if (self->enemy->health < 1) {
          if (self->enemy->gibHealth)
            hurt = -self->enemy->gibHealth;
          else
            hurt = 500;

          Damage(self->enemy, self, self->owner, vec3_origin, center,
                 pain_normal, hurt, 0,
                 DISRUPTOR_DAMAGE_FLAGS | DamageFlags::StatOnce,
                 ModID::Tracker);
        }

        self->nextThink = level.time + 10_hz;

        if (self->enemy->client)
          self->enemy->client->trackerPainTime = self->nextThink;
        else
          self->enemy->s.effects |= EF_TRACKERTRAIL;
      }
    } else {
      if (!self->enemy->client)
        self->enemy->s.effects &= ~EF_TRACKERTRAIL;
      FreeEntity(self);
    }
  }
}

static void disruptor_pain_daemon_spawn(gentity_t *owner, gentity_t *enemy,
                                        int damage) {
  gentity_t *daemon;

  if (enemy == nullptr)
    return;

  daemon = Spawn();
  daemon->className = "pain daemon";
  daemon->think = disruptor_pain_daemon_think;
  daemon->nextThink = level.time;
  daemon->timeStamp = level.time;
  daemon->owner = owner;
  daemon->enemy = enemy;
  daemon->dmg = damage;
}

static void tracker_explode(gentity_t *self) {
  gi.WriteByte(svc_temp_entity);
  gi.WriteByte(TE_TRACKER_EXPLOSION);
  gi.WritePosition(self->s.origin);
  gi.multicast(self->s.origin, MULTICAST_PHS, false);

  FreeEntity(self);
}

static TOUCH(disruptor_touch)(gentity_t *self, gentity_t *other,
                              const trace_t &tr, bool otherTouchingSelf)
    ->void {
  float damagetime;

  if (other == self->owner)
    return;

  if (tr.surface && (tr.surface->flags & SURF_SKY)) {
    FreeEntity(self);
    return;
  }

  if (self->client)
    G_PlayerNoise(self->owner, self->s.origin, PlayerNoise::Impact);

  if (other->takeDamage) {
    if ((other->svFlags & SVF_MONSTER) || other->client) {
      if (other->health > 0) // knockback only for living creatures
      {
        // PMM - kickback was times 4 .. reduced to 3
        // now this does no damage, just knockback
        Damage(other, self, self->owner, self->velocity, self->s.origin,
               tr.plane.normal,
               /* self->dmg */ 0, (self->dmg * 3),
               DISRUPTOR_IMPACT_FLAGS | DamageFlags::StatOnce, ModID::Tracker);

        if (!(other->flags & (FL_FLY | FL_SWIM)))
          other->velocity[_Z] += 140;

        damagetime = ((float)self->dmg) * 0.1f;
        damagetime = damagetime / DISRUPTOR_DAMAGE_TIME.seconds();

        disruptor_pain_daemon_spawn(self->owner, other, (int)damagetime);
      } else // lots of damage (almost autogib) for dead bodies
      {
        Damage(other, self, self->owner, self->velocity, self->s.origin,
               tr.plane.normal, self->dmg * 4, (self->dmg * 3),
               DISRUPTOR_IMPACT_FLAGS | DamageFlags::StatOnce, ModID::Tracker);
      }
    } else // full damage in one shot for inanimate objects
    {
      Damage(other, self, self->owner, self->velocity, self->s.origin,
             tr.plane.normal, self->dmg, (self->dmg * 3),
             DISRUPTOR_IMPACT_FLAGS | DamageFlags::StatOnce, ModID::Tracker);
    }
  }

  tracker_explode(self);
  return;
}

static THINK(disruptor_fly)(gentity_t *self)->void {
  Vector3 dest;
  Vector3 dir;
  Vector3 center;

  if ((!self->enemy) || (!self->enemy->inUse) || (self->enemy->health < 1)) {
    tracker_explode(self);
    return;
  }

  // PMM - try to hunt for center of enemy, if possible and not client
  if (self->enemy->client) {
    dest = self->enemy->s.origin;
    dest[2] += self->enemy->viewHeight;
  }
  // paranoia
  else if (!self->enemy->absMin || !self->enemy->absMax) {
    dest = self->enemy->s.origin;
  } else {
    center = (self->enemy->absMin + self->enemy->absMax) * 0.5f;
    dest = center;
  }

  dir = dest - self->s.origin;
  dir.normalize();
  self->s.angles = VectorToAngles(dir);
  self->velocity = dir * self->speed;
  self->monsterInfo.savedGoal = dest;

  self->nextThink = level.time + 10_hz;
}

void fire_disruptor(gentity_t *self, const Vector3 &start, const Vector3 &dir,
                    int damage, int speed, gentity_t *enemy) {
  gentity_t *bolt;
  trace_t tr;

  bolt = Spawn();
  bolt->s.origin = start;
  bolt->s.oldOrigin = start;
  bolt->s.angles = VectorToAngles(dir);
  bolt->velocity = dir * speed;
  bolt->svFlags |= SVF_PROJECTILE;
  bolt->moveType = MoveType::FlyMissile;
  bolt->clipMask = MASK_PROJECTILE;

  // [Paril-KEX]
  if (self->client && !G_ShouldPlayersCollide(true))
    bolt->clipMask &= ~CONTENTS_PLAYER;

  bolt->solid = SOLID_BBOX;
  bolt->speed = (float)speed;
  bolt->s.effects = EF_TRACKER;
  bolt->s.sound = gi.soundIndex("weapons/disrupt.wav");
  bolt->s.modelIndex = gi.modelIndex("models/proj/disintegrator/tris.md2");
  bolt->touch = disruptor_touch;
  bolt->enemy = enemy;
  bolt->owner = self;
  bolt->dmg = damage;
  bolt->className = "tracker";
  gi.linkEntity(bolt);

  if (enemy) {
    bolt->nextThink = level.time + 10_hz;
    bolt->think = disruptor_fly;
  } else {
    bolt->nextThink = level.time + 10_sec;
    bolt->think = FreeEntity;
  }

  tr = gi.traceLine(self->s.origin, bolt->s.origin, bolt, bolt->clipMask);
  if (tr.fraction < 1.0f) {
    bolt->s.origin = tr.endPos + (tr.plane.normal * 1.f);
    bolt->touch(bolt, tr.ent, tr, false);
  }
}

/*
========================
fire_flechette
========================
*/
static TOUCH(flechette_touch)(gentity_t *self, gentity_t *other,
                              const trace_t &tr, bool otherTouchingSelf)
    ->void {
  if (other == self->owner)
    return;

  if (tr.surface && (tr.surface->flags & SURF_SKY)) {
    FreeEntity(self);
    return;
  }

  if (self->client)
    G_PlayerNoise(self->owner, self->s.origin, PlayerNoise::Impact);

  if (other->takeDamage) {
    Damage(other, self, self->owner, self->velocity, self->s.origin,
           tr.plane.normal, self->dmg, (int)self->splashRadius,
           DamageFlags::NoRegularArmor | DamageFlags::StatOnce,
           ModID::ETFRifle);
  } else {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_FLECHETTE);
    gi.WritePosition(self->s.origin);
    gi.WriteDir(tr.plane.normal);
    gi.multicast(self->s.origin, MULTICAST_PHS, false);
  }

  FreeEntity(self);
}

void fire_flechette(gentity_t *self, const Vector3 &start, const Vector3 &dir,
                    int damage, int speed, int kick) {
  gentity_t *flechette;

  flechette = Spawn();
  flechette->s.origin = start;
  flechette->s.oldOrigin = start;
  flechette->s.angles = VectorToAngles(dir);
  flechette->velocity = dir * speed;
  flechette->svFlags |= SVF_PROJECTILE;
  flechette->moveType = MoveType::FlyMissile;
  flechette->clipMask = MASK_PROJECTILE;
  flechette->flags |= FL_DODGE;

  // [Paril-KEX]
  if (self->client && !G_ShouldPlayersCollide(true))
    flechette->clipMask &= ~CONTENTS_PLAYER;

  flechette->solid = SOLID_BBOX;
  flechette->s.renderFX = RF_FULLBRIGHT;
  flechette->s.modelIndex = gi.modelIndex("models/proj/flechette/tris.md2");

  flechette->owner = self;
  flechette->touch = flechette_touch;
  flechette->nextThink = level.time + GameTime::from_sec(8000.f / speed);
  flechette->think = FreeEntity;
  flechette->dmg = damage;
  flechette->splashRadius = (float)kick;

  gi.linkEntity(flechette);

  trace_t tr = gi.traceLine(self->s.origin, flechette->s.origin, flechette,
                            flechette->clipMask);
  if (tr.fraction < 1.0f) {
    flechette->s.origin = tr.endPos + (tr.plane.normal * 1.f);
    flechette->touch(flechette, tr.ent, tr, false);
  }
}

// **************************
// PROX
// **************************

constexpr GameTime PROX_TIME_TO_LIVE = 45_sec;
constexpr GameTime PROX_ARMING_DELAY = 500_ms;
constexpr GameTime PROX_TIME_DELAY = 500_ms;
constexpr float PROX_BOUND_SIZE = 96.0f;
constexpr float PROX_DAMAGE_RADIUS = 192.0f;
constexpr float PROX_TRIGGER_RADIUS =
    PROX_DAMAGE_RADIUS + 10.0f; // match legacy sweep
constexpr int32_t PROX_HEALTH = 20;
constexpr int32_t PROX_DAMAGE = 90;

/*
===============
Prox_Explode
===============
*/
static THINK(Prox_Explode)(gentity_t *ent)->void {
  Vector3 origin;
  gentity_t *owner;

  // free the trigger
  if (ent->teamChain && ent->teamChain->owner == ent)
    FreeEntity(ent->teamChain);

  owner = ent;
  if (ent->teamMaster) {
    owner = ent->teamMaster;
    G_PlayerNoise(owner, ent->s.origin, PlayerNoise::Impact);
  }

  PlayClientPowerupFireSound(ent);

  ent->takeDamage = false;
  RadiusDamage(ent, owner, (float)ent->dmg, ent, PROX_DAMAGE_RADIUS,
               DamageFlags::Normal, ModID::ProxMine);

  origin = ent->s.origin + (ent->velocity * -0.02f);
  gi.WriteByte(svc_temp_entity);
  gi.WriteByte(ent->groundEntity ? TE_GRENADE_EXPLOSION : TE_ROCKET_EXPLOSION);
  gi.WritePosition(origin);
  gi.multicast(ent->s.origin, MULTICAST_PHS, false);

  FreeEntity(ent);
}

/*
===============
prox_die
===============
*/
static DIE(prox_die)(gentity_t *self, gentity_t *inflictor, gentity_t *attacker,
                     int damage, const Vector3 &point, const MeansOfDeath &mod)
    ->void {
  // if set off by another prox, delay a little (chained explosions)
  if (strcmp(inflictor->className, "prox_mine")) {
    self->takeDamage = false;
    Prox_Explode(self);
  } else {
    self->takeDamage = false;
    self->think = Prox_Explode;
    self->nextThink = level.time + FRAME_TIME_S;
  }
}

/*
===============
Prox_TriggerThink
Radial detection after arming delay; triggers owner prox if a valid target is in
range and visible.
===============
*/
static THINK(Prox_TriggerThink)(gentity_t *trigger)->void {
  if (!trigger || !trigger->owner) {
    if (trigger)
      FreeEntity(trigger);
    return;
  }

  gentity_t *prox = trigger->owner;

  // Not armed yet: keep waiting
  if (level.time < trigger->timeStamp) {
    trigger->nextThink = level.time + 100_ms;
    trigger->think = Prox_TriggerThink;
    return;
  }

  // Already scheduled to blow
  if (prox->think == Prox_Explode) {
    FreeEntity(trigger);
    return;
  }

  // Radial sweep for targets
  gentity_t *search = nullptr;
  while ((search = FindRadius(search, trigger->s.origin,
                              PROX_TRIGGER_RADIUS)) != nullptr) {
    if (!search->className)
      continue;

    // teammate avoidance
    if (CheckTeamDamage(prox->teamMaster, search))
      continue;

    // non-DM: only monsters can trigger
    if (!deathmatch->integer && search->client)
      continue;

    // do not set off by owner
    if (search == prox)
      continue;

    // must be a valid candidate
    const bool candidate =
        ((search->svFlags & SVF_MONSTER) ||
         (deathmatch->integer &&
          (search->client ||
           (search->className && !strcmp(search->className, "prox_mine")))))
            ? (search->health > 0)
            : (deathmatch->integer &&
               ((strncmp(search->className, "info_player_", 12) == 0) ||
                (strcmp(search->className, "misc_teleporter_dest") == 0) ||
                (strncmp(search->className, "item_flag_", 10) == 0)));

    if (!candidate)
      continue;

    // require visibility
    if (!visible(search, prox))
      continue;

    // Trigger warning and arm explosion after short delay
    gi.sound(prox, CHAN_VOICE, gi.soundIndex("weapons/proxwarn.wav"), 1.0f,
             ATTN_NORM, 0);
    prox->think = Prox_Explode;
    prox->nextThink = level.time + PROX_TIME_DELAY;

    FreeEntity(trigger);
    return;
  }

  // Keep scanning
  trigger->nextThink = level.time + 100_ms;
  trigger->think = Prox_TriggerThink;
}

/*
===============
prox_seek
===============
*/
static THINK(prox_seek)(gentity_t *ent)->void {
  if (level.time > GameTime::from_sec(ent->wait)) {
    Prox_Explode(ent);
  } else {
    ent->s.frame++;
    if (ent->s.frame > 13)
      ent->s.frame = 9;
    ent->think = prox_seek;
    ent->nextThink = level.time + 10_hz;
  }
}

/*
===============
prox_open
Plays opening anim; when fully open, leaves radial-trigger running.
Respects arming delay by deferring to the trigger entity's timeStamp.
===============
*/
static THINK(prox_open)(gentity_t *ent)->void {
  gentity_t *search = nullptr;

  if (ent->s.frame == 9) { // end of opening animation
    ent->s.sound = 0;

    // let owner pass through in DM
    if (deathmatch->integer)
      ent->owner = nullptr;

    // Optional immediate sweep but only if armed; otherwise, the trigger think
    // handles it.
    if (ent->teamChain && level.time >= ent->teamChain->timeStamp) {
      while ((search = FindRadius(search, ent->s.origin,
                                  PROX_TRIGGER_RADIUS)) != nullptr) {
        if (!search->className)
          continue;

        if (CheckTeamDamage(search, ent->teamMaster))
          continue;

        const bool candidate =
            ((search->svFlags & SVF_MONSTER) ||
             (deathmatch->integer &&
              (search->client ||
               (search->className && !strcmp(search->className, "prox_mine")))))
                ? (search->health > 0)
                : (deathmatch->integer &&
                   ((strncmp(search->className, "info_player_", 12) == 0) ||
                    (strcmp(search->className, "misc_teleporter_dest") == 0) ||
                    (strncmp(search->className, "item_flag_", 10) == 0)));

        if (!candidate)
          continue;

        if (!visible(search, ent))
          continue;

        gi.sound(ent, CHAN_VOICE, gi.soundIndex("weapons/proxwarn.wav"), 1.0f,
                 ATTN_NORM, 0);
        Prox_Explode(ent);
        return;
      }
    }

    if (g_dm_strong_mines->integer)
      ent->wait = (level.time + PROX_TIME_TO_LIVE).seconds();
    else {
      switch (ent->dmg / PROX_DAMAGE) {
      case 1:
        ent->wait = (level.time + PROX_TIME_TO_LIVE).seconds();
        break;
      case 2:
        ent->wait = (level.time + 30_sec).seconds();
        break;
      case 4:
        ent->wait = (level.time + 15_sec).seconds();
        break;
      case 8:
        ent->wait = (level.time + 10_sec).seconds();
        break;
      default:
        ent->wait = (level.time + PROX_TIME_TO_LIVE).seconds();
        break;
      }
    }

    ent->think = prox_seek;
    ent->nextThink = level.time + 200_ms;
  } else {
    if (ent->s.frame == 0)
      gi.sound(ent, CHAN_VOICE, gi.soundIndex("weapons/proxopen.wav"), 1.0f,
               ATTN_NORM, 0);
    ent->s.frame++;
    ent->think = prox_open;
    ent->nextThink = level.time + 10_hz;
  }
}

/*
===============
prox_land
Attach to surface, set up radial trigger with arming delay.
===============
*/
static TOUCH(prox_land)(gentity_t *ent, gentity_t *other, const trace_t &tr,
                        bool otherTouchingSelf)
    ->void {
  gentity_t *trigger;
  Vector3 dir;
  Vector3 forward, right, up;
  MoveType moveType = MoveType::None;
  int stick_ok = 0;
  Vector3 land_point;

  // sky: remove
  if (tr.surface && (tr.surface->flags & SURF_SKY)) {
    FreeEntity(ent);
    return;
  }

  // slime/lava check at contact point
  if (tr.plane.normal) {
    land_point = ent->s.origin + (tr.plane.normal * -10.0f);
    if (gi.pointContents(land_point) & (CONTENTS_SLIME | CONTENTS_LAVA)) {
      Prox_Explode(ent);
      return;
    }
  }

  constexpr float PROX_STOP_EPSILON = 0.1f;

  // invalid plane or hit living/damageable actors => explode (unless owner)
  if (!tr.plane.normal || (other->svFlags & SVF_MONSTER) || other->client ||
      (other->flags & FL_DAMAGEABLE)) {
    if (other != ent->teamMaster)
      Prox_Explode(ent);
    return;
  } else if (other != world) {
    // Evaluate if we can come to rest on this entity
    Vector3 out{};
    float backoff, change;

    if ((other->moveType == MoveType::Push) && (tr.plane.normal[2] > 0.7f))
      stick_ok = 1;

    backoff = ent->velocity.dot(tr.plane.normal) * 1.5f;
    for (int i = 0; i < 3; i++) {
      change = tr.plane.normal[i] * backoff;
      out[i] = ent->velocity[i] - change;
      if (out[i] > -PROX_STOP_EPSILON && out[i] < PROX_STOP_EPSILON)
        out[i] = 0.0f;
    }

    // too much upward motion; keep bouncing
    if (out[2] > 60.0f)
      return;

    moveType = MoveType::Bounce;

    // stick or bail
    if (stick_ok) {
      ent->velocity = {};
      ent->aVelocity = {};
    } else {
      if (tr.plane.normal[2] > 0.7f) {
        Prox_Explode(ent);
        return;
      }
      return;
    }
  } else if (other->s.modelIndex != MODELINDEX_WORLD) {
    return;
  }

  // Orient to surface normal
  dir = VectorToAngles(tr.plane.normal);
  AngleVectors(dir, forward, right, up);

  // second check: if the mine itself is in lava/slime, explode
  if (gi.pointContents(ent->s.origin) & (CONTENTS_LAVA | CONTENTS_SLIME)) {
    Prox_Explode(ent);
    return;
  }

  // Create radial trigger with arming delay
  trigger = Spawn();
  trigger->s.origin = ent->s.origin;
  trigger->mins = {-PROX_BOUND_SIZE, -PROX_BOUND_SIZE, -PROX_BOUND_SIZE};
  trigger->maxs = {PROX_BOUND_SIZE, PROX_BOUND_SIZE, PROX_BOUND_SIZE};
  trigger->moveType = MoveType::None;
  trigger->solid = SOLID_NOT; // no touch; purely radial via think
  trigger->owner = ent;       // back-pointer to the prox
  trigger->className = "prox_trigger";
  trigger->teamMaster = ent;
  trigger->timeStamp = level.time + PROX_ARMING_DELAY; // ARMED after this time
  trigger->think = Prox_TriggerThink;
  trigger->nextThink = level.time + 100_ms;
  gi.linkEntity(trigger);

  // Finalize prox entity
  ent->svFlags &= ~SVF_PROJECTILE;
  ent->velocity = {};
  ent->aVelocity = {};
  dir[PITCH] += 90.0f; // rotate to vertical
  ent->s.angles = dir;
  ent->takeDamage = true;
  ent->moveType = moveType; // either bounce or none, depending on stick
  ent->die = prox_die;
  ent->teamChain = trigger; // link trigger to prox
  ent->health = PROX_HEALTH;
  ent->nextThink = level.time;
  ent->think = prox_open;
  ent->touch = nullptr;
  ent->solid = SOLID_BBOX;

  gi.linkEntity(ent);
}

/*
===============
Prox_Think
===============
*/
static THINK(Prox_Think)(gentity_t *self)->void {
  if (self->timeStamp <= level.time) {
    Prox_Explode(self);
    return;
  }

  self->s.angles = VectorToAngles(self->velocity.normalized());
  self->s.angles[PITCH] -= 90.0f;
  self->nextThink = level.time;
}

/*
===============
fire_prox
===============
*/
void fire_prox(gentity_t *self, const Vector3 &start, const Vector3 &aimDir,
               int prox_damage_multiplier, int speed) {
  gentity_t *prox;
  Vector3 dir;
  Vector3 forward, right, up;

  dir = VectorToAngles(aimDir);
  AngleVectors(dir, forward, right, up);

  prox = Spawn();
  prox->s.origin = start;
  prox->velocity = aimDir * speed;

  const float gravityAdjustment = level.gravity / 800.0f;
  prox->velocity += up * (200.0f + crandom() * 10.0f) * gravityAdjustment;
  prox->velocity += right * (crandom() * 10.0f);

  prox->s.angles = dir;
  prox->s.angles[PITCH] -= 90.0f;
  prox->moveType = MoveType::Bounce;
  prox->solid = SOLID_BBOX;
  prox->svFlags |= SVF_PROJECTILE;
  prox->s.effects |= EF_GRENADE;
  prox->flags |= (FL_DODGE | FL_TRAP);
  prox->clipMask = MASK_PROJECTILE | CONTENTS_LAVA | CONTENTS_SLIME;

  // [Paril-KEX]
  if (self->client && !G_ShouldPlayersCollide(true))
    prox->clipMask &= ~CONTENTS_PLAYER;

  prox->s.renderFX |= RF_IR_VISIBLE;
  prox->mins = {-6, -6, -6};
  prox->maxs = {6, 6, 6};
  prox->s.modelIndex = gi.modelIndex("models/weapons/g_prox/tris.md2");
  prox->owner = self;
  prox->teamMaster = self;
  prox->touch = prox_land;
  prox->think = Prox_Think;
  prox->nextThink = level.time;
  prox->dmg = PROX_DAMAGE * prox_damage_multiplier;
  prox->className = "prox_mine";
  prox->flags |= FL_DAMAGEABLE;
  prox->flags |= FL_MECHANICAL;

  switch (prox_damage_multiplier) {
  case 1:
    prox->timeStamp = level.time + PROX_TIME_TO_LIVE;
    break;
  case 2:
    prox->timeStamp = level.time + 30_sec;
    break;
  case 4:
    prox->timeStamp = level.time + 15_sec;
    break;
  case 8:
    prox->timeStamp = level.time + 10_sec;
    break;
  default:
    prox->timeStamp = level.time + PROX_TIME_TO_LIVE;
    break;
  }

  gi.linkEntity(prox);
}

// *************************
// MELEE WEAPONS
// *************************

struct player_melee_data_t {
  gentity_t *self;
  const Vector3 &start;
  const Vector3 &aim;
  int reach;
};

static BoxEntitiesResult_t fire_player_melee_BoxFilter(gentity_t *check,
                                                       void *data_v) {
  const player_melee_data_t *data = (const player_melee_data_t *)data_v;

  if (!check->inUse || !check->takeDamage || check == data->self)
    return BoxEntitiesResult_t::Skip;

  // check distance
  Vector3 closest_point_to_check =
      closest_point_to_box(data->start, check->s.origin + check->mins,
                           check->s.origin + check->maxs);
  Vector3 closest_point_to_self = closest_point_to_box(
      closest_point_to_check, data->self->s.origin + data->self->mins,
      data->self->s.origin + data->self->maxs);

  Vector3 dir = (closest_point_to_check - closest_point_to_self);
  float len = dir.normalize();

  if (len > data->reach)
    return BoxEntitiesResult_t::Skip;

  // check angle if we aren't intersecting
  Vector3 shrink{2, 2, 2};
  if (!boxes_intersect(check->absMin + shrink, check->absMax - shrink,
                       data->self->absMin + shrink,
                       data->self->absMax - shrink)) {
    dir = (((check->absMin + check->absMax) / 2) - data->start).normalized();

    if (dir.dot(data->aim) < 0.70f)
      return BoxEntitiesResult_t::Skip;
  }

  return BoxEntitiesResult_t::Keep;
}

bool fire_player_melee(gentity_t *self, const Vector3 &start,
                       const Vector3 &aim, int reach, int damage, int kick,
                       MeansOfDeath mod) {
  constexpr size_t MAX_HIT = 4;

  Vector3 reach_vec{float(reach - 1), float(reach - 1), float(reach - 1)};
  gentity_t *targets[MAX_HIT];

  player_melee_data_t data{self, start, aim, reach};

  // find all the things we could maybe hit
  size_t num = gi.BoxEntities(
      self->absMin - reach_vec, self->absMax + reach_vec, targets,
      q_countof(targets), AREA_SOLID, fire_player_melee_BoxFilter, &data);

  if (!num)
    return false;

  bool was_hit = false;

  for (size_t i = 0; i < num; i++) {
    gentity_t *hit = targets[i];

    if (!hit->inUse || !hit->takeDamage)
      continue;
    else if (!CanDamage(self, hit))
      continue;

    // do the damage
    Vector3 closest_point_to_check = closest_point_to_box(
        start, hit->s.origin + hit->mins, hit->s.origin + hit->maxs);

    if (hit->svFlags & SVF_MONSTER)
      hit->pain_debounce_time -= random_time(5_ms, 75_ms);

    bool prevented = false;
    if (Game::Is(GameType::ProBall))
      prevented = ProBall::HandleCarrierHit(hit, self, mod);

    if (!prevented) {
      if (mod.id == ModID::Chainfist)
        Damage(hit, self, self, aim, closest_point_to_check, -aim, damage,
               kick / 2, DamageFlags::DestroyArmor | DamageFlags::NoKnockback,
               mod);
      else
        Damage(hit, self, self, aim, closest_point_to_check, -aim, damage,
               kick / 2, DamageFlags::NoKnockback, mod);
    }

    was_hit = true;
  }

  return was_hit;
}

// *************************
// NUKE
// *************************

constexpr GameTime NUKE_DELAY = 4_sec;
constexpr GameTime NUKE_TIME_TO_LIVE = 6_sec;
constexpr float NUKE_RADIUS = 512;
constexpr int32_t NUKE_DAMAGE = 400;
constexpr GameTime NUKE_QUAKE_TIME = 3_sec;
constexpr float NUKE_QUAKE_STRENGTH = 100;

static THINK(Nuke_Quake)(gentity_t *self)->void {
  uint32_t i;
  gentity_t *e;

  if (self->last_move_time < level.time) {
    gi.positionedSound(self->s.origin, self, CHAN_AUTO, self->noiseIndex, 0.75,
                       ATTN_NONE, 0);
    self->last_move_time = level.time + 500_ms;
  }

  for (i = 1, e = g_entities + i; i < globals.numEntities; i++, e++) {
    if (!e->inUse)
      continue;
    if (!e->client)
      continue;
    if (!e->groundEntity)
      continue;

    e->groundEntity = nullptr;
    e->velocity[_X] += crandom() * 150;
    e->velocity[_Y] += crandom() * 150;
    e->velocity[_Z] = self->speed * (100.0f / e->mass);
  }

  if (level.time < self->timeStamp)
    self->nextThink = level.time + FRAME_TIME_S;
  else
    FreeEntity(self);
}

void Nuke_Explode(gentity_t *ent) {
  float dmg = ent->dmg;
  float splashRadius = ent->splashRadius;

  if (!dmg)
    dmg = 400;

  if (!splashRadius)
    dmg = 512;

  if (ent->teamMaster->client)
    G_PlayerNoise(ent->teamMaster, ent->s.origin, PlayerNoise::Impact);

  RadiusNukeDamage(ent, ent->teamMaster, dmg, ent, splashRadius, ModID::Nuke);

  PlayClientPowerupFireSound(ent);

  gi.sound(ent, CHAN_NO_PHS_ADD | CHAN_VOICE,
           gi.soundIndex("weapons/grenlx1a.wav"), 1, ATTN_NONE, 0);

  gi.WriteByte(svc_temp_entity);
  gi.WriteByte(TE_EXPLOSION1_BIG);
  gi.WritePosition(ent->s.origin);
  gi.multicast(ent->s.origin, MULTICAST_PHS, false);

  gi.WriteByte(svc_temp_entity);
  gi.WriteByte(TE_NUKEBLAST);
  gi.WritePosition(ent->s.origin);
  gi.multicast(ent->s.origin, MULTICAST_ALL, false);

  // become a quake
  ent->svFlags |= SVF_NOCLIENT;
  ent->noiseIndex = gi.soundIndex("world/rumble.wav");
  ent->think = Nuke_Quake;
  ent->speed = NUKE_QUAKE_STRENGTH;
  ent->timeStamp = level.time + NUKE_QUAKE_TIME;
  ent->nextThink = level.time + FRAME_TIME_S;
  ent->last_move_time = 0_ms;
}

static DIE(nuke_die)(gentity_t *self, gentity_t *inflictor, gentity_t *attacker,
                     int damage, const Vector3 &point, const MeansOfDeath &mod)
    ->void {
  self->takeDamage = false;
  if ((attacker) && !(strcmp(attacker->className, "nuke"))) {
    FreeEntity(self);
    return;
  }
  Nuke_Explode(self);
}

static THINK(Nuke_Think)(gentity_t *ent)->void {
  float attenuation, default_atten = 1.8f;
  int nuke_damage_multiplier;
  player_muzzle_t muzzleflash;

  nuke_damage_multiplier = ent->dmg / NUKE_DAMAGE;
  switch (nuke_damage_multiplier) {
  case 1:
    attenuation = default_atten / 1.4f;
    muzzleflash = MZ_NUKE1;
    break;
  case 2:
    attenuation = default_atten / 2.0f;
    muzzleflash = MZ_NUKE2;
    break;
  case 4:
    attenuation = default_atten / 3.0f;
    muzzleflash = MZ_NUKE4;
    break;
  case 8:
    attenuation = default_atten / 5.0f;
    muzzleflash = MZ_NUKE8;
    break;
  default:
    attenuation = default_atten;
    muzzleflash = MZ_NUKE1;
    break;
  }

  if (ent->wait < level.time.seconds())
    Nuke_Explode(ent);
  else if (level.time >= (GameTime::from_sec(ent->wait) - NUKE_TIME_TO_LIVE)) {
    ent->s.frame++;

    if (ent->s.frame > 11)
      ent->s.frame = 6;

    if (gi.pointContents(ent->s.origin) & (CONTENTS_SLIME | CONTENTS_LAVA)) {
      Nuke_Explode(ent);
      return;
    }

    ent->think = Nuke_Think;
    ent->nextThink = level.time + 10_hz;
    ent->health = 1;

    // set the owner to nullptr so the owner can walk through it.  needs to be
    // done here so the owner doesn't get stuck on it while it's opening if
    // fired at point blank wall
    ent->owner = nullptr;

    gi.WriteByte(svc_muzzleflash);
    gi.WriteEntity(ent);
    gi.WriteByte(muzzleflash);
    gi.multicast(ent->s.origin, MULTICAST_PHS, false);

    if (ent->timeStamp <= level.time) {
      if ((GameTime::from_sec(ent->wait) - level.time) <=
          (NUKE_TIME_TO_LIVE / 2.0f)) {
        gi.sound(ent, CHAN_NO_PHS_ADD | CHAN_VOICE,
                 gi.soundIndex("weapons/nukewarn2.wav"), 1, attenuation, 0);
        ent->timeStamp = level.time + 300_ms;
      } else {
        gi.sound(ent, CHAN_NO_PHS_ADD | CHAN_VOICE,
                 gi.soundIndex("weapons/nukewarn2.wav"), 1, attenuation, 0);
        ent->timeStamp = level.time + 500_ms;
      }
    }
  } else {
    if (ent->timeStamp <= level.time) {
      gi.sound(ent, CHAN_NO_PHS_ADD | CHAN_VOICE,
               gi.soundIndex("weapons/nukewarn2.wav"), 1, attenuation, 0);
      ent->timeStamp = level.time + 1_sec;
    }
    ent->nextThink = level.time + FRAME_TIME_S;
  }
}

static TOUCH(nuke_bounce)(gentity_t *ent, gentity_t *other, const trace_t &tr,
                          bool otherTouchingSelf)
    ->void {
  if (tr.surface && tr.surface->id) {
    if (frandom() > 0.5f)
      gi.sound(ent, CHAN_BODY, gi.soundIndex("weapons/hgrenb1a.wav"), 1,
               ATTN_NORM, 0);
    else
      gi.sound(ent, CHAN_BODY, gi.soundIndex("weapons/hgrenb2a.wav"), 1,
               ATTN_NORM, 0);
  }
}

void fire_nuke(gentity_t *self, const Vector3 &start, const Vector3 &aimDir,
               int speed) {
  gentity_t *nuke;
  Vector3 dir;
  Vector3 forward, right, up;
  uint8_t damage_modifier = PlayerDamageModifier(self);

  dir = VectorToAngles(aimDir);
  AngleVectors(dir, forward, right, up);

  nuke = Spawn();
  nuke->s.origin = start;
  nuke->velocity = aimDir * speed;
  nuke->velocity += up * (200 + crandom() * 10.0f);
  nuke->velocity += right * (crandom() * 10.0f);
  nuke->moveType = MoveType::Bounce;
  nuke->clipMask = MASK_PROJECTILE;
  nuke->solid = SOLID_BBOX;
  nuke->s.effects |= EF_GRENADE;
  nuke->s.renderFX |= RF_IR_VISIBLE;
  nuke->mins = {-8, -8, 0};
  nuke->maxs = {8, 8, 16};
  nuke->s.modelIndex = gi.modelIndex("models/weapons/g_nuke/tris.md2");
  nuke->owner = self;
  nuke->teamMaster = self;
  nuke->nextThink = level.time + FRAME_TIME_S;
  nuke->wait = (level.time + NUKE_DELAY + NUKE_TIME_TO_LIVE).seconds();
  nuke->think = Nuke_Think;
  nuke->touch = nuke_bounce;

  nuke->health = 10000;
  nuke->takeDamage = true;
  nuke->flags |= FL_DAMAGEABLE;
  nuke->dmg = NUKE_DAMAGE * damage_modifier;
  if (damage_modifier == 1)
    nuke->splashRadius = NUKE_RADIUS;
  else
    nuke->splashRadius =
        NUKE_RADIUS + NUKE_RADIUS * (0.25f * (float)damage_modifier);
  // this yields 1.0, 1.5, 2.0, 3.0 times radius

  nuke->className = "nuke";
  nuke->die = nuke_die;

  gi.linkEntity(nuke);
}

// *************************
// TESLA
// *************************

constexpr GameTime TESLA_TIME_TO_LIVE = 30_sec;
constexpr float TESLA_DAMAGE_RADIUS = 128;
constexpr int32_t TESLA_DAMAGE = 3;
constexpr int32_t TESLA_KNOCKBACK = 8;

constexpr GameTime TESLA_ACTIVATE_TIME = 3_sec;

constexpr int32_t TESLA_EXPLOSION_DAMAGE_MULT =
    50; // this is the amount the damage is multiplied by for underwater
        // explosions
constexpr float TESLA_EXPLOSION_RADIUS = 200;

static void tesla_remove(gentity_t *self) {
  gentity_t *cur, *next;

  self->takeDamage = false;
  if (self->teamChain) {
    cur = self->teamChain;
    while (cur) {
      next = cur->teamChain;
      FreeEntity(cur);
      cur = next;
    }
  } else if (self->airFinished)
    gi.Com_Print("tesla_mine without a field!\n");

  self->owner = self->teamMaster; // Going away, set the owner correctly.
  // grenade explode does damage to self->enemy
  self->enemy = nullptr;

  // play quad sound if quadded and an underwater explosion
  if ((self->splashRadius) &&
      (self->dmg > (TESLA_DAMAGE * TESLA_EXPLOSION_DAMAGE_MULT)))
    PlayClientPowerupFireSound(self);

  Grenade_Explode(self);
}

static DIE(tesla_die)(gentity_t *self, gentity_t *inflictor,
                      gentity_t *attacker, int damage, const Vector3 &point,
                      const MeansOfDeath &mod)
    ->void {
  tesla_remove(self);
}

static void tesla_blow(gentity_t *self) {
  self->dmg *= TESLA_EXPLOSION_DAMAGE_MULT;
  self->splashRadius = TESLA_EXPLOSION_RADIUS;
  tesla_remove(self);
}

static TOUCH(tesla_zap)(gentity_t *self, gentity_t *other, const trace_t &tr,
                        bool otherTouchingSelf)
    ->void {}

static BoxEntitiesResult_t tesla_think_active_BoxFilter(gentity_t *check,
                                                        void *data) {
  gentity_t *self = (gentity_t *)data;

  if (!check->inUse)
    return BoxEntitiesResult_t::Skip;
  if (check == self)
    return BoxEntitiesResult_t::Skip;
  if (check->health < 1)
    return BoxEntitiesResult_t::Skip;
  // don't hit teammates
  if (check->client) {
    if (!deathmatch->integer)
      return BoxEntitiesResult_t::Skip;
    else if (CheckTeamDamage(check, self->teamMaster))
      return BoxEntitiesResult_t::Skip;
  }
  if (!(check->svFlags & SVF_MONSTER) && !(check->flags & FL_DAMAGEABLE) &&
      !check->client)
    return BoxEntitiesResult_t::Skip;

  // don't hit other teslas in SP/coop
  if (!deathmatch->integer && check->className && (check->flags & FL_TRAP))
    return BoxEntitiesResult_t::Skip;

  return BoxEntitiesResult_t::Keep;
}

static THINK(tesla_think_active)(gentity_t *self)->void {
  size_t num;
  static gentity_t *touch[MAX_ENTITIES];
  gentity_t *hit;
  Vector3 dir, start;
  trace_t tr;

  if (level.time > self->airFinished) {
    tesla_remove(self);
    return;
  }

  if (deathmatch->integer && CombatIsDisabled())
    return;

  start = self->s.origin;
  start[2] += 16;

  num = gi.BoxEntities(self->teamChain->absMin, self->teamChain->absMax, touch,
                       MAX_ENTITIES, AREA_SOLID, tesla_think_active_BoxFilter,
                       self);
  for (size_t i = 0; i < num; i++) {
    // if the tesla died while zapping things, stop zapping.
    if (!(self->inUse))
      break;

    hit = touch[i];
    if (!hit->inUse)
      continue;
    if (hit == self)
      continue;
    if (hit->health < 1)
      continue;
    // don't hit teammates
    if (hit->client) {
      if (!deathmatch->integer)
        continue;
      else if (CheckTeamDamage(hit, self->teamChain->owner))
        continue;
    }
    if (!(hit->svFlags & SVF_MONSTER) && !(hit->flags & FL_DAMAGEABLE) &&
        !hit->client)
      continue;

    tr = gi.traceLine(start, hit->s.origin, self, MASK_PROJECTILE);
    if (tr.fraction == 1 || tr.ent == hit) {
      dir = hit->s.origin - start;

      PlayClientPowerupFireSound(self);

      // PGM - don't do knockback to walking monsters
      if ((hit->svFlags & SVF_MONSTER) && !(hit->flags & (FL_FLY | FL_SWIM)))
        Damage(hit, self, self->teamMaster, dir, tr.endPos, tr.plane.normal,
               self->dmg, 0, DamageFlags::Normal | DamageFlags::StatOnce,
               ModID::TeslaMine);
      else
        Damage(hit, self, self->teamMaster, dir, tr.endPos, tr.plane.normal,
               self->dmg, TESLA_KNOCKBACK,
               DamageFlags::Normal | DamageFlags::StatOnce, ModID::TeslaMine);

      gi.WriteByte(svc_temp_entity);
      gi.WriteByte(TE_LIGHTNING);
      gi.WriteEntity(self); // source entity
      gi.WriteEntity(hit);  // destination entity
      gi.WritePosition(start);
      gi.WritePosition(tr.endPos);
      gi.multicast(start, MULTICAST_PVS, false);
    }
  }

  if (self->inUse) {
    self->think = tesla_think_active;
    self->nextThink = level.time + 10_hz;
  }
}

static THINK(tesla_activate)(gentity_t *self)->void {
  gentity_t *trigger, *search;

  if (gi.pointContents(self->s.origin) &
      (CONTENTS_SLIME | CONTENTS_LAVA | CONTENTS_WATER)) {
    tesla_blow(self);
    return;
  }

  // only check for spawn points in deathmatch
  if (deathmatch->integer) {
    search = nullptr;
    while ((search = FindRadius(search, self->s.origin,
                                1.5f * TESLA_DAMAGE_RADIUS)) != nullptr) {
      // [Paril-KEX] don't allow traps to be placed near flags or teleporters
      // if it's a monster or player with health > 0
      // or it's a player start point
      // and we can see it
      // blow up
      if (search->className &&
          ((deathmatch->integer &&
            ((!strncmp(search->className, "info_player_", 12)) ||
             (!strcmp(search->className, "misc_teleporter_dest")) ||
             (!strncmp(search->className, "item_flag_", 10))))) &&
          (visible(search, self))) {
        BecomeExplosion1(self);
        return;
      }
    }
  }

  trigger = Spawn();
  trigger->s.origin = self->s.origin;
  trigger->mins = {-TESLA_DAMAGE_RADIUS, -TESLA_DAMAGE_RADIUS, self->mins[2]};
  trigger->maxs = {TESLA_DAMAGE_RADIUS, TESLA_DAMAGE_RADIUS,
                   TESLA_DAMAGE_RADIUS};
  trigger->moveType = MoveType::None;
  trigger->solid = SOLID_TRIGGER;
  trigger->owner = self;
  trigger->touch = tesla_zap;
  trigger->className = "tesla trigger";
  // doesn't need to be marked as a teamslave since the move code for bounce
  // looks for teamchains
  gi.linkEntity(trigger);

  self->s.angles = {};

  // set the owner to nullptr so the owner can walk through it.  needs to be
  // done here so the owner doesn't get stuck on it while it's opening if fired
  // at point blank wall
  if (deathmatch->integer)
    self->owner = nullptr;

  self->teamChain = trigger;
  self->think = tesla_think_active;
  self->nextThink = level.time + FRAME_TIME_S;
  self->airFinished = level.time + TESLA_TIME_TO_LIVE;
}

static THINK(tesla_think)(gentity_t *ent)->void {
  if (gi.pointContents(ent->s.origin) & (CONTENTS_SLIME | CONTENTS_LAVA)) {
    tesla_remove(ent);
    return;
  }

  ent->s.angles = {};

  if (!(ent->s.frame))
    gi.sound(ent, CHAN_VOICE, gi.soundIndex("weapons/teslaopen.wav"), 1,
             ATTN_NORM, 0);

  ent->s.frame++;
  if (ent->s.frame > 14) {
    ent->s.frame = 14;
    ent->think = tesla_activate;
    ent->nextThink = level.time + 10_hz;
  } else {
    if (ent->s.frame > 9) {
      if (ent->s.frame == 10) {
        if (ent->owner && ent->owner->client) {
          G_PlayerNoise(ent->owner, ent->s.origin, PlayerNoise::Weapon);
        }
        ent->s.skinNum = 1;
      } else if (ent->s.frame == 12)
        ent->s.skinNum = 2;
      else if (ent->s.frame == 14)
        ent->s.skinNum = 3;
    }
    ent->think = tesla_think;
    ent->nextThink = level.time + 10_hz;
  }
}

/*
=============
tesla_touch

Handle tesla mine contact events and apply related effects.
=============
*/
static TOUCH(tesla_touch)(gentity_t *ent, gentity_t *other, const trace_t &tr,
                          bool otherTouchingSelf)
    ->void {
  if (tr.contents & (CONTENTS_SLIME | CONTENTS_LAVA)) {
    tesla_blow(ent);
    return;
  }

  // Play a random bounce sound if moving
  if (ent->velocity[_X] || ent->velocity[_Y] || ent->velocity[_Z]) {
    const char variant = (frandom() > 0.5f) ? '1' : '2';

    std::array<char, 32> path{};
    std::snprintf(path.data(), path.size(), "weapons/hgrenb%ca.wav", variant);

    gi.sound(ent, CHAN_VOICE, gi.soundIndex(path.data()), 1.0f, ATTN_NORM, 0);
  }
}

void fire_tesla(gentity_t *self, const Vector3 &start, const Vector3 &aimDir,
                int tesla_damage_multiplier, int speed) {
  gentity_t *tesla;
  Vector3 dir;
  Vector3 forward, right, up;

  dir = VectorToAngles(aimDir);
  AngleVectors(dir, forward, right, up);

  tesla = Spawn();
  tesla->s.origin = start;
  tesla->velocity = aimDir * speed;

  float gravityAdjustment = level.gravity / 800.f;

  tesla->velocity += up * (200 + crandom() * 10.0f) * gravityAdjustment;
  tesla->velocity += right * (crandom() * 10.0f);

  tesla->s.angles = {};
  tesla->moveType = MoveType::Bounce;
  tesla->solid = SOLID_BBOX;
  tesla->s.effects |= EF_GRENADE;
  tesla->s.renderFX |= RF_IR_VISIBLE;
  tesla->mins = {-12, -12, 0};
  tesla->maxs = {12, 12, 20};
  tesla->s.modelIndex = gi.modelIndex("models/weapons/g_tesla/tris.md2");

  tesla->owner = self; // PGM - we don't want it owned by self YET.
  tesla->teamMaster = self;

  tesla->wait = (level.time + TESLA_TIME_TO_LIVE).seconds();
  tesla->think = tesla_think;
  tesla->nextThink = level.time + TESLA_ACTIVATE_TIME;

  // blow up on contact with lava & slime code
  tesla->touch = tesla_touch;

  tesla->health = deathmatch->integer ? 20 : 50;

  tesla->takeDamage = true;
  tesla->die = tesla_die;
  tesla->dmg = TESLA_DAMAGE * tesla_damage_multiplier;
  tesla->className = "tesla_mine";
  tesla->flags |= (FL_DAMAGEABLE | FL_TRAP);
  tesla->clipMask = (MASK_PROJECTILE | CONTENTS_SLIME | CONTENTS_LAVA) &
                    ~CONTENTS_DEADMONSTER;

  // [Paril-KEX]
  if (self->client && !G_ShouldPlayersCollide(true))
    tesla->clipMask &= ~CONTENTS_PLAYER;

  tesla->flags |= FL_MECHANICAL;

  gi.linkEntity(tesla);
}

/*
=================
fire_ionripper
=================
*/
static THINK(ionripper_sparks)(gentity_t *self)->void {
  gi.WriteByte(svc_temp_entity);
  gi.WriteByte(TE_WELDING_SPARKS);
  gi.WriteByte(0);
  gi.WritePosition(self->s.origin);
  gi.WriteDir(vec3_origin);
  gi.WriteByte(irandom(0xe4, 0xe8));
  gi.multicast(self->s.origin, MULTICAST_PVS, false);

  FreeEntity(self);
}

static TOUCH(ionripper_touch)(gentity_t *self, gentity_t *other,
                              const trace_t &tr, bool otherTouchingSelf)
    ->void {
  if (other == self->owner)
    return;

  if (tr.surface && (tr.surface->flags & SURF_SKY)) {
    FreeEntity(self);
    return;
  }

  if (self->owner->client)
    G_PlayerNoise(self->owner, self->s.origin, PlayerNoise::Impact);

  if (other->takeDamage) {
    Damage(other, self, self->owner, self->velocity, self->s.origin,
           tr.plane.normal, self->dmg, 1,
           DamageFlags::Energy | DamageFlags::StatOnce, ModID::IonRipper);
  } else {
    return;
  }

  FreeEntity(self);
}

/*
=================
fire_ionripper
=================
*/
void fire_ionripper(gentity_t *self, const Vector3 &start, const Vector3 &dir,
                    int damage, int speed, Effect effect) {
  gentity_t *ion = Spawn();
  trace_t tr;

  ion->s.origin = start;
  ion->s.oldOrigin = start;
  ion->s.angles = VectorToAngles(dir);
  ion->velocity = dir * speed;

  ion->moveType = MoveType::FlyMissile; // MoveType::WallBounce;
  ion->clipMask = MASK_PROJECTILE;

  // Optional: Skip players if friendly fire disabled
  if (self->client && !G_ShouldPlayersCollide(true))
    ion->clipMask &= ~CONTENTS_PLAYER;

  ion->solid = SOLID_BBOX;
  ion->s.effects |= effect;
  ion->svFlags |= SVF_PROJECTILE;
  ion->flags |= FL_DODGE;
  ion->s.renderFX |= RF_FULLBRIGHT;

  // Optional: Use a different model/sound for nailgun-like effect
  ion->s.modelIndex = gi.modelIndex("models/objects/boomrang/tris.md2");
  ion->s.sound = gi.soundIndex("misc/lasfly.wav");

  ion->owner = self;
  ion->touch = ionripper_touch;
  ion->nextThink = level.time + 3_sec;
  ion->think = ionripper_sparks;
  ion->dmg = damage;
  ion->splashRadius = 100;

  gi.linkEntity(ion);

  // Immediate trace to prevent spawning inside walls
  tr = gi.traceLine(self->s.origin, ion->s.origin, ion, ion->clipMask);
  if (tr.fraction < 1.0f) {
    ion->s.origin = tr.endPos + (tr.plane.normal * 1.f);
    ion->touch(ion, tr.ent, tr, false);
  }
}

/*
=================
fire_heat
=================
*/
static THINK(heat_think)(gentity_t *self)->void {
  gentity_t *target = nullptr;
  gentity_t *acquire = nullptr;
  Vector3 vec;
  Vector3 oldang;
  float len;
  float oldlen = 0;
  float dot, olddot = 1;

  Vector3 fwd = AngleVectors(self->s.angles).forward;

  // acquire new target
  while ((target = FindRadius(target, self->s.origin, 1024)) != nullptr) {
    if (self->owner == target)
      continue;
    if (!target->client)
      continue;
    if (target->health <= 0)
      continue;
    if (target->client && target->client->eliminated)
      continue;
    if (!visible(self, target))
      continue;

    vec = self->s.origin - target->s.origin;
    len = vec.length();

    dot = vec.normalized().dot(fwd);

    // targets that require us to turn less are preferred
    if (dot >= olddot)
      continue;

    if (acquire == nullptr || dot < olddot || len < oldlen) {
      acquire = target;
      oldlen = len;
      olddot = dot;
    }
  }

  if (acquire != nullptr) {
    oldang = self->s.angles;
    vec = (acquire->s.origin - self->s.origin).normalized();
    float t = self->accel;

    float d = self->moveDir.dot(vec);

    if (d < 0.45f && d > -0.45f)
      vec = -vec;

    self->moveDir = slerp(self->moveDir, vec, t).normalized();
    self->s.angles = VectorToAngles(self->moveDir);

    if (!self->enemy) {
      gi.sound(self, CHAN_WEAPON, gi.soundIndex("weapons/railgr1a.wav"), 1.f,
               0.25f, 0);
      self->enemy = acquire;
    }
  } else
    self->enemy = nullptr;

  self->velocity = self->moveDir * self->speed;
  self->nextThink = level.time + FRAME_TIME_MS;
}

void fire_heat(gentity_t *self, const Vector3 &start, const Vector3 &dir,
               int damage, int speed, float splashRadius, int splashDamage,
               float turnFraction) {
  gentity_t *heat;

  heat = Spawn();
  heat->s.origin = start;
  heat->moveDir = dir;
  heat->s.angles = VectorToAngles(dir);
  heat->velocity = dir * speed;
  heat->flags |= FL_DODGE;
  heat->moveType = MoveType::FlyMissile;
  heat->svFlags |= SVF_PROJECTILE;
  heat->clipMask = MASK_PROJECTILE;
  heat->solid = SOLID_BBOX;
  heat->s.effects |= EF_ROCKET;
  heat->s.modelIndex = gi.modelIndex("models/objects/rocket/tris.md2");
  heat->owner = self;
  heat->touch = rocket_touch;
  heat->speed = speed;
  heat->accel = turnFraction;

  heat->nextThink = level.time + FRAME_TIME_MS;
  heat->think = heat_think;

  heat->dmg = damage;
  heat->splashDamage = splashDamage;
  heat->splashRadius = splashRadius;
  heat->s.sound = gi.soundIndex("weapons/rockfly.wav");

  gi.linkEntity(heat);
}

/*
=================
fire_plasmagun
=================
*/
static void SpawnPlasmaExplosion(const Vector3 &origin) {
  gentity_t *explosion = Spawn();
  explosion->s.origin = origin;
  explosion->s.modelIndex = gi.modelIndex("sprites/s_pls2.sp2");
  explosion->s.effects |= EF_ANIM_ALLFAST;
  explosion->s.renderFX |= RF_TRANSLUCENT;
  explosion->solid = SOLID_NOT;
  explosion->nextThink = level.time + 500_ms;
  explosion->think = FreeEntity;
  gi.linkEntity(explosion);
}

static TOUCH(plasmagun_touch)(gentity_t *ent, gentity_t *other,
                              const trace_t &tr, bool otherTouchingSelf)
    ->void {
  if (other == ent->owner)
    return;

  if (tr.surface && (tr.surface->flags & SURF_SKY)) {
    FreeEntity(ent);
    return;
  }

  const Vector3 impact = tr.endPos;

  if (ent->owner && ent->owner->client)
    G_PlayerNoise(ent->owner, impact, PlayerNoise::Impact);

  if (other->takeDamage) {
    Damage(other, ent, ent->owner, ent->velocity, impact, tr.plane.normal,
           ent->dmg, 1,
           DamageFlags::Energy | DamageFlags::StatOnce, ModID::PlasmaGun);
  }

  if (ent->splashDamage) {
    // Match Quake 3: center splash on the impact point.
    ent->s.origin = impact;
    gi.linkEntity(ent);
    RadiusDamage(ent, ent->owner, static_cast<float>(ent->splashDamage), other,
                 ent->splashRadius, DamageFlags::Energy,
                 ModID::PlasmaGun_Splash);
  }

  gi.sound(ent, CHAN_WEAPON, gi.soundIndex("weapons/plsmexpl.wav"), 1,
           ATTN_NORM, 0);
  SpawnPlasmaExplosion(impact);

  FreeEntity(ent);
}

void fire_plasmagun(gentity_t *self, const Vector3 &start, const Vector3 &dir,
                    int damage, int speed, float splashRadius,
                    int splashDamage) {
  gentity_t *plasma = Spawn();
  trace_t tr;

  plasma->s.origin = start;
  plasma->s.oldOrigin = start;
  plasma->s.angles = VectorToAngles(dir);
  plasma->velocity = dir * speed;
  plasma->moveType = MoveType::FlyMissile;
  plasma->svFlags |= SVF_PROJECTILE;
  plasma->clipMask = MASK_PROJECTILE;
  if (self->client && !G_ShouldPlayersCollide(true))
    plasma->clipMask &= ~CONTENTS_PLAYER;
  plasma->solid = SOLID_BBOX;
  plasma->flags |= FL_DODGE;
  plasma->s.effects |= EF_PLASMA | EF_ANIM_ALLFAST | EF_BLUEHYPERBLASTER;
  plasma->s.renderFX |= RF_TRANSLUCENT;
  plasma->s.modelIndex = gi.modelIndex("sprites/s_pls1.sp2");
  plasma->s.sound = gi.soundIndex("weapons/plsmhumm.wav");
  plasma->owner = self;
  plasma->touch = plasmagun_touch;
  plasma->nextThink = level.time + GameTime::from_sec(8000.f / speed);
  plasma->think = FreeEntity;
  plasma->dmg = damage;
  plasma->splashDamage = splashDamage;
  plasma->splashRadius = splashRadius;
  plasma->className = "plasma bolt";

  gi.linkEntity(plasma);

  tr = gi.traceLine(self->s.origin, plasma->s.origin, plasma, plasma->clipMask);
  if (tr.fraction < 1.0f) {
    plasma->s.origin = tr.endPos + (tr.plane.normal * 1.f);
    plasma->touch(plasma, tr.ent, tr, false);
  }
}

/*
=================
fire_phalanx
=================
*/
static TOUCH(phalanx_touch)(gentity_t *ent, gentity_t *other, const trace_t &tr,
                            bool otherTouchingSelf)
    ->void {
  Vector3 origin;

  if (other == ent->owner)
    return;

  if (tr.surface && (tr.surface->flags & SURF_SKY)) {
    FreeEntity(ent);
    return;
  }

  if (ent->owner->client)
    G_PlayerNoise(ent->owner, ent->s.origin, PlayerNoise::Impact);

  // calculate position for the explosion entity
  origin = ent->s.origin + (ent->velocity * -0.02f);

  if (other->takeDamage)
    Damage(other, ent, ent->owner, ent->velocity, ent->s.origin,
           tr.plane.normal, ent->dmg, 0, DamageFlags::Energy, ModID::Phalanx);

  RadiusDamage(ent, ent->owner, (float)ent->splashDamage, other,
               ent->splashRadius, DamageFlags::Energy, ModID::Phalanx);

  gi.WriteByte(svc_temp_entity);
  gi.WriteByte(TE_PLASMA_EXPLOSION);
  gi.WritePosition(origin);
  gi.multicast(ent->s.origin, MULTICAST_PHS, false);

  FreeEntity(ent);
}

void fire_phalanx(gentity_t *self, const Vector3 &start, const Vector3 &dir,
                  int damage, int speed, float splashRadius, int splashDamage) {
  gentity_t *phalanx;

  phalanx = Spawn();
  phalanx->s.origin = start;
  phalanx->moveDir = dir;
  phalanx->s.angles = VectorToAngles(dir);
  phalanx->velocity = dir * speed;
  phalanx->moveType = MoveType::FlyMissile;
  phalanx->clipMask = MASK_PROJECTILE;

  // [Paril-KEX]
  if (self->client && !G_ShouldPlayersCollide(true))
    phalanx->clipMask &= ~CONTENTS_PLAYER;

  phalanx->solid = SOLID_BBOX;
  phalanx->svFlags |= SVF_PROJECTILE;
  phalanx->flags |= FL_DODGE;
  phalanx->owner = self;
  phalanx->touch = phalanx_touch;
  phalanx->nextThink = level.time + GameTime::from_sec(8000.f / speed);
  phalanx->think = FreeEntity;
  phalanx->dmg = damage;
  phalanx->splashDamage = splashDamage;
  phalanx->splashRadius = splashRadius;
  phalanx->s.sound = gi.soundIndex("weapons/rockfly.wav");

  phalanx->s.modelIndex = gi.modelIndex("sprites/s_photon.sp2");
  phalanx->s.effects |= EF_PLASMA | EF_ANIM_ALLFAST;

  gi.linkEntity(phalanx);
}

/*
=================
fire_trap
=================
*/
static THINK(Trap_Gib_Think)(gentity_t *ent)->void {
  if (ent->owner->s.frame != 5) {
    FreeEntity(ent);
    return;
  }

  Vector3 forward, right, up;
  Vector3 vec;

  AngleVectors(ent->owner->s.angles, forward, right, up);

  // rotate us around the center
  float degrees = (150.f * gi.frameTimeSec) + ent->owner->delay;
  Vector3 diff = ent->owner->s.origin - ent->s.origin;
  vec = RotatePointAroundVector(up, diff, degrees);
  ent->s.angles[YAW] += degrees;
  Vector3 new_origin = ent->owner->s.origin - vec;

  trace_t tr = gi.traceLine(ent->s.origin, new_origin, ent, MASK_SOLID);
  ent->s.origin = tr.endPos;

  // pull us towards the trap's center
  diff.normalize();
  ent->s.origin += diff * (15.0f * gi.frameTimeSec);

  ent->waterType = gi.pointContents(ent->s.origin);
  if (ent->waterType & MASK_WATER)
    ent->waterLevel = WATER_FEET;

  ent->nextThink = level.time + FRAME_TIME_S;
  gi.linkEntity(ent);
}

static DIE(trap_die)(gentity_t *self, gentity_t *inflictor, gentity_t *attacker,
                     int damage, const Vector3 &point, const MeansOfDeath &mod)
    ->void {
  BecomeExplosion1(self);
}

static void SP_item_foodcube(gentity_t *self) {
  if (deathmatch->integer && !game.map.spawnHealth) {
    FreeEntity(self);
    return;
  }

  SpawnItem(self, GetItemByIndex(IT_FOODCUBE));
  self->spawnFlags |= SPAWNFLAG_ITEM_DROPPED;
}

void SpawnDamage(int type, const Vector3 &origin, const Vector3 &normal,
                 int damage);

static THINK(Trap_Think)(gentity_t *ent)->void {
  gentity_t *target = nullptr;
  gentity_t *best = nullptr;
  Vector3 vec;
  float len;
  float oldlen = 8000;

  if (ent->timeStamp < level.time) {
    BecomeExplosion1(ent);
    // note to self
    // cause explosion damage???
    return;
  }

  ent->nextThink = level.time + 10_hz;

  if (!ent->groundEntity)
    return;

  // ok lets do the blood effect
  if (ent->s.frame > 4) {
    if (ent->s.frame == 5) {
      bool spawn = ent->wait == 64;

      ent->wait -= 2;

      if (spawn)
        gi.sound(ent, CHAN_VOICE, gi.soundIndex("weapons/trapdown.wav"), 1,
                 ATTN_IDLE, 0);

      ent->delay += 2.f;

      if (ent->wait < 19)
        ent->s.frame++;

      return;
    }
    ent->s.frame++;
    if (ent->s.frame == 8) {
      ent->nextThink = level.time + 1_sec;
      ent->think = FreeEntity;
      ent->s.effects &= ~EF_TRAP;

      best = Spawn();
      if (best) {
        best->count = ent->mass;
        best->s.scale = 1.f + ((ent->accel - 100.f) / 300.f) * 1.0f;
        SP_item_foodcube(best);
        best->s.origin = ent->s.origin;
        best->s.origin[_Z] += 24 * best->s.scale;
        best->s.oldOrigin = best->s.origin;
        best->s.angles[YAW] = frandom() * 360;
        best->velocity[_Z] = 400;
        // best->think(best);
        // best->nextThink = 0_ms;
        gi.linkEntity(best);

        gi.sound(best, CHAN_AUTO, gi.soundIndex("misc/fhit3.wav"), 1.f,
                 ATTN_NORM, 0.f);
      }
      return;
    }
    return;
  }

  ent->s.effects &= ~EF_TRAP;
  if (ent->s.frame >= 4) {
    ent->s.effects |= EF_TRAP;
    // set the owner to nullptr so the owner can walk through it.  needs to be
    // done here so the owner doesn't get stuck on it while it's opening if
    // fired at point blank wall
    if (deathmatch->integer)
      ent->owner = nullptr;
  }

  if (ent->s.frame < 4) {
    ent->s.frame++;
    return;
  }

  if (deathmatch->integer && CombatIsDisabled())
    return;

  while ((target = FindRadius(target, ent->s.origin, 256)) != nullptr) {
    if (target == ent)
      continue;

    // [Paril-KEX] don't allow traps to be placed near flags or teleporters
    // if it's a monster or player with health > 0
    // or it's a player start point
    // and we can see it
    // blow up
    if (target->className &&
        ((deathmatch->integer &&
          ((!strncmp(target->className, "info_player_", 12)) ||
           (!strcmp(target->className, "misc_teleporter_dest")) ||
           (!strncmp(target->className, "item_flag_", 10))))) &&
        (visible(target, ent))) {
      BecomeExplosion1(ent);
      return;
    }

    if (!(target->svFlags & SVF_MONSTER) && !target->client)
      continue;
    if (target != ent->teamMaster && CheckTeamDamage(target, ent->teamMaster))
      continue;
    // [Paril-KEX]
    if (!deathmatch->integer && target->client)
      continue;
    if (target->health <= 0)
      continue;
    if (!visible(ent, target))
      continue;
    vec = ent->s.origin - target->s.origin;
    len = vec.length();
    if (!best) {
      best = target;
      oldlen = len;
      continue;
    }
    if (len < oldlen) {
      oldlen = len;
      best = target;
    }
  }

  // pull the enemy in
  if (best) {
    if (best->groundEntity) {
      best->s.origin[_Z] += 1;
      best->groundEntity = nullptr;
    }
    vec = ent->s.origin - best->s.origin;
    len = vec.normalize();

    float max_speed = best->client ? 290.f : 150.f;

    // Ensure clamp bounds are ordered even if max_speed falls below the
    // intended minimum pull speed. This avoids triggering the MSVC debug
    // runtime assert for invalid std::clamp bounds during trap damage
    // handling (seen when certain entities customise their speed).
    const float min_pull_speed = std::min(64.0f, max_speed);
    const float max_pull_speed = std::max(64.0f, max_speed);
    const float pull_speed =
        std::clamp(max_speed - len, min_pull_speed, max_pull_speed);

    best->velocity += (vec * pull_speed);

    ent->s.sound = gi.soundIndex("weapons/trapsuck.wav");

    if (len < 48) {
      if (best->mass < 400) {
        ent->takeDamage = false;
        ent->solid = SOLID_NOT;
        ent->die = nullptr;

        Damage(best, ent, ent->teamMaster, vec3_origin, best->s.origin,
               vec3_origin, 100000, 1,
               DamageFlags::Normal | DamageFlags::StatOnce, ModID::Trap);

        if (best->svFlags & SVF_MONSTER)
          M_ProcessPain(best);

        ent->enemy = best;
        ent->wait = 64;
        ent->s.oldOrigin = ent->s.origin;
        ent->timeStamp = level.time + 30_sec;
        ent->accel = best->mass;
        ent->mass = best->mass / (deathmatch->integer ? 4 : 10);

        // ok spawn the food cube
        ent->s.frame = 5;

        // link up any gibs that this monster may have spawned
        for (size_t i = 0; i < globals.numEntities; i++) {
          gentity_t *e = &g_entities[i];

          if (!e->inUse)
            continue;
          else if (strcmp(e->className, "gib"))
            continue;
          else if ((e->s.origin - ent->s.origin).length() > 128.f)
            continue;

          e->moveType = MoveType::None;
          e->nextThink = level.time + FRAME_TIME_S;
          e->think = Trap_Gib_Think;
          e->owner = ent;
          Trap_Gib_Think(e);
        }
      } else {
        BecomeExplosion1(ent);
        // note to self
        // cause explosion damage???
        return;
      }
    }
  }
}

void fire_trap(gentity_t *self, const Vector3 &start, const Vector3 &aimDir,
               int speed) {
  gentity_t *trap;
  Vector3 dir;
  Vector3 forward, right, up;

  dir = VectorToAngles(aimDir);
  AngleVectors(dir, forward, right, up);

  trap = Spawn();
  trap->s.origin = start;
  trap->velocity = aimDir * speed;

  float gravityAdjustment = level.gravity / 800.f;

  trap->velocity += up * (200 + crandom() * 10.0f) * gravityAdjustment;
  trap->velocity += right * (crandom() * 10.0f);

  trap->aVelocity = {0, 300, 0};
  trap->moveType = MoveType::Bounce;

  trap->solid = SOLID_BBOX;
  trap->takeDamage = true;
  trap->mins = {-4, -4, 0};
  trap->maxs = {4, 4, 8};
  trap->die = trap_die;
  trap->health = 20;
  trap->s.modelIndex = gi.modelIndex("models/weapons/z_trap/tris.md2");
  trap->owner = trap->teamMaster = self;
  trap->nextThink = level.time + 1_sec;
  trap->think = Trap_Think;
  trap->className = "food_cube_trap";
  trap->s.sound = gi.soundIndex("weapons/traploop.wav");

  trap->flags |= (FL_DAMAGEABLE | FL_MECHANICAL | FL_TRAP);
  trap->clipMask = MASK_PROJECTILE & ~CONTENTS_DEADMONSTER;

  // [Paril-KEX]
  if (self->client && !G_ShouldPlayersCollide(true))
    trap->clipMask &= ~CONTENTS_PLAYER;

  gi.linkEntity(trap);

  trap->timeStamp = level.time + 30_sec;
}

// =================================================

/*
===============
fire_vorepod

Shalrath-inspired homing projectile:
- 10s max life
- predictive lead on target
- slightly wider/natural turn rate
===============
*/
namespace {
/*
===============
VorePod_Touch
===============
*/
static TOUCH(VorePod_Touch)(gentity_t *self, gentity_t *other,
                            const trace_t &tr, bool otherTouchingSelf)
    ->void {
  if (other == self->owner)
    return;

  // vanish silently on sky
  if (tr.surface && (tr.surface->flags & SURF_SKY)) {
    FreeEntity(self);
    return;
  }

  // radius + impact damage
  const int radius = self->splashRadius ? self->splashRadius : 40;
  if (radius > 0) {
    RadiusDamage(self, self->owner, self->dmg, nullptr, radius,
                 DamageFlags::Normal, ModID::Tracker);
  }

  if (other->takeDamage && self->dmg > 0) {
    Damage(other, self, self->owner, self->velocity, self->s.origin,
           tr.plane.normal, self->dmg, 1,
           DamageFlags::Energy | DamageFlags::NoKnockback, ModID::Tracker);
  }

  // explosion effect
  gi.WriteByte(svc_temp_entity);
  gi.WriteByte(TE_EXPLOSION1);
  gi.WritePosition(self->s.origin);
  gi.multicast(self->s.origin, MULTICAST_PHS, false);

  FreeEntity(self);
}

/*
===============
VP_PredictLead
===============
*/
static Vector3 VP_PredictLead(const Vector3 &src, float projSpeed,
                              const Vector3 &tgtPos, const Vector3 &tgtVel) {
  const Vector3 r = tgtPos - src;
  const float s = projSpeed;
  const float vv = tgtVel.dot(tgtVel);
  const float rv = r.dot(tgtVel);
  const float rr = r.dot(r);
  const float a = vv - s * s;
  const float b = 2.0f * rv;
  const float c = rr;

  float t;
  if (std::fabs(a) < 1e-6f) {
    t = (s > 1e-3f) ? (rr / (2.0f * s * (sqrtf(rr) + 1e-6f))) : 0.0f;
  } else {
    const float disc = b * b - 4.0f * a * c;
    if (disc < 0.0f)
      return r.normalized();
    const float sqrtDisc = sqrtf(disc);
    const float t1 = (-b + sqrtDisc) / (2.0f * a);
    const float t2 = (-b - sqrtDisc) / (2.0f * a);
    t = (t1 >= 0.0f && t2 >= 0.0f) ? std::min(t1, t2) : std::max(t1, t2);
    if (t < 0.0f)
      return r.normalized();
  }

  const Vector3 lead = tgtPos + tgtVel * t;
  return (lead - src).normalized();
}

/*
===============
VorePod_Home
===============
*/
static THINK(VorePod_Home)(gentity_t *self)->void {
  // expire after 10s
  if (level.time.milliseconds() >= self->wait) {
    FreeEntity(self);
    return;
  }

  gentity_t *enemy = self->enemy;
  if (!enemy || !enemy->inUse || enemy->health <= 0) {
    FreeEntity(self);
    return;
  }

  // target center
  const Vector3 tgtCenter =
      (enemy->absMin + enemy->absMax) * 0.5f + Vector3(0, 0, 10);
  Vector3 tgtVel = enemy->velocity;

  // predictive lead
  Vector3 desiredDir =
      VP_PredictLead(self->s.origin, self->speed, tgtCenter, tgtVel);

  if (self->moveDir.lengthSquared() < 1e-6f)
    self->moveDir = self->velocity.lengthSquared() > 1e-6f
                        ? self->velocity.normalized()
                        : desiredDir;

  // natural wider turn
  const float dot = std::clamp(self->moveDir.dot(desiredDir), -1.0f, 1.0f);
  const float angle = acosf(dot);
  const float turnFrac =
      std::clamp(0.12f + (angle / 3.14159f) * 0.25f, 0.12f, 0.5f);

  self->moveDir = slerp(self->moveDir, desiredDir, turnFrac).normalized();
  self->s.angles = VectorToAngles(self->moveDir);
  self->velocity = self->moveDir * self->speed;
  self->aVelocity = Vector3(300, 300, 300);

  self->nextThink = level.time + 100_ms;
}
} // namespace

/*
===============
fire_vorepod
===============
*/
void fire_homing_pod(gentity_t *self, const Vector3 &start, const Vector3 &dir,
                     int damage, int speed, MonsterMuzzleFlashID flashType) {
#if 0
	// optional muzzleflash
	if (flashType != MZ2_INVALID) {
		gi.WriteByte(svc_muzzleflash2);
		gi.WriteShort(static_cast<int>(self - g_entities));
		gi.WriteByte(static_cast<int>(flashType));
		gi.multicast(self->s.origin, MULTICAST_PVS, false);
	}
#endif
  // spawn projectile
  gentity_t *pod = Spawn();
  pod->className = "vorepod";
  pod->owner = self;
  pod->solid = SOLID_BBOX;
  pod->moveType = MoveType::FlyMissile;
  pod->clipMask = MASK_PROJECTILE;
  pod->svFlags |= SVF_PROJECTILE;
  pod->flags |= FL_DODGE;

  pod->s.modelIndex = gi.modelIndex("models/proj/v_spike/tris.md2");
  pod->mins = pod->maxs = Vector3(0, 0, 0);

  pod->s.origin = start;
  pod->s.oldOrigin = start;

  pod->velocity = dir * float(speed);
  pod->speed = float(speed);
  pod->aVelocity = Vector3(300, 300, 300);
  pod->moveDir = dir;

  pod->enemy = self->enemy;

  pod->dmg = damage;
  pod->splashDamage = damage;
  pod->splashRadius = damage;
  // pod->meansOfDeath = ModID::Tracker;

  pod->wait = level.time.milliseconds() + 10000; // 10s lifetime

  pod->touch = VorePod_Touch;
  pod->think = VorePod_Home;
  pod->nextThink = level.time + 200_ms; // first adjust after 0.2s

  pod->s.effects |= EF_TRACKER | EF_TRACKERTRAIL;
  pod->s.sound = gi.soundIndex("misc/lasfly.wav");

  gi.linkEntity(pod);

  // collision check on spawn
  const trace_t tr =
      gi.traceLine(self->s.origin, pod->s.origin, pod, pod->clipMask);
  if (tr.fraction < 1.0f) {
    pod->s.origin = tr.endPos + tr.plane.normal * 1.0f;
    VorePod_Touch(pod, tr.ent, tr, false);
  }
}

// =================================================

/*
===============
G_ExplodeNearbyMinesSafe
Detonate any nearby mines (prox/tesla) around a point, ensuring 'safe' takes no
damage.

Intended use:
- Call on player spawn or teleporter exit to clear unfair traps.
- If possible, spawn logic should choose an alternate spot first; this is the
fallback.

Returns the number of mines detonated.
===============
*/
int G_ExplodeNearbyMinesSafe(const Vector3 &origin, float radius,
                             gentity_t *safe) {
  // Helper classifiers
  auto IsProxMine = [](const gentity_t *e) -> bool {
    return e && e->className && strcmp(e->className, "prox_mine") == 0;
  };
  auto IsTeslaMine = [](const gentity_t *e) -> bool {
    // Allow "tesla", "tesla_mine", "tesla_trap", etc.
    return e && e->className && strncmp(e->className, "tesla", 5) == 0;
  };
  auto IsTrap = [](const gentity_t *e) -> bool {
    return e && e->className &&
           strncmp(e->className, "food_cube_trap", 14) == 0;
  };

  // Temporarily suppress damage on the spawning/teleporting player for this
  // clear operation. This keeps the change tightly scoped to just these
  // explosions.
  const bool hadSafe = (safe != nullptr);
  const bool restoreSafeDamage = hadSafe && (safe->takeDamage != 0);
  if (restoreSafeDamage)
    safe->takeDamage = false;

  int detonated = 0;
  gentity_t *it = nullptr;

  while ((it = FindRadius(it, origin, radius)) != nullptr) {
    if (!it->inUse || !it->className)
      continue;

    // Only detonating actual mines, not any FL_TRAP generics by default.
    if (IsProxMine(it)) {
      // Prox-specific cleanup and explosion
      // Free trigger if owned by this prox (mirrors Prox_Explode)
      if (it->teamChain && it->teamChain->owner == it)
        FreeEntity(it->teamChain);

      gentity_t *owner = it->teamMaster ? it->teamMaster : it;
      G_PlayerNoise(owner, it->s.origin, PlayerNoise::Impact);
      PlayClientPowerupFireSound(it);

      it->takeDamage = false;

      // Ignore 'safe' for damage application during this detonation
      RadiusDamage(it, owner, (float)it->dmg, safe, PROX_DAMAGE_RADIUS,
                   DamageFlags::Normal, ModID::ProxMine);

      Vector3 origin_fx = it->s.origin + (it->velocity * -0.02f);
      gi.WriteByte(svc_temp_entity);
      gi.WriteByte(it->groundEntity ? TE_GRENADE_EXPLOSION
                                    : TE_ROCKET_EXPLOSION);
      gi.WritePosition(origin_fx);
      gi.multicast(it->s.origin, MULTICAST_PHS, false);

      FreeEntity(it);
      ++detonated;
      continue;
    }

    if (IsTeslaMine(it)) {
      BecomeExplosion1(it);
      continue;
    }

    if (IsTrap(it)) {
      BecomeExplosion1(it);
      continue;
    }
  }

  // Restore damage flag on the player we protected
  if (restoreSafeDamage)
    safe->takeDamage = true;

  return detonated;
}
