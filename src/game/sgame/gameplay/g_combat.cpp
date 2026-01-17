/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

g_combat.cpp (Game Combat) This file is central to all damage and combat
interactions in the game. It contains the core `Damage` function, which is the
final authority for processing everything from weapon hits to environmental
hazards. Key Responsibilities: - Damage Application: The `Damage` function
calculates final damage after considering armor, powerups, friendly fire
settings, and god mode. - Armor and Protection: Implements the logic for both
standard armor (`CheckArmor`) and power armor (`CheckPowerArmor`) to absorb
damage. - Knockback: `ApplyKnockback` calculates and applies the physical
pushback effect from weapon impacts. - Entity Death: The `Killed` function is
called when an entity's health is reduced to zero, triggering its death
sequence. - Radius Damage: Implements `RadiusDamage` for explosions, calculating
damage falloff and checking line-of-sight to affected entities.*/

#include "../g_local.hpp"
#include "freezetag_damage.hpp"

#include <cassert>

/*
============
CanDamage

Returns true if the inflictor can directly damage the target.  Used for
explosions and melee attacks.
============
*/
bool CanDamage(gentity_t *targ, gentity_t *inflictor) {
  Vector3 dest;
  trace_t trace;

  // bmodels need special checking because their origin is 0,0,0
  Vector3 inflictor_center;

  if (inflictor->linked)
    inflictor_center = (inflictor->absMin + inflictor->absMax) * 0.5f;
  else
    inflictor_center = inflictor->s.origin;

  if (targ->solid == SOLID_BSP) {
    dest = closest_point_to_box(inflictor_center, targ->absMin, targ->absMax);

    trace = gi.traceLine(inflictor_center, dest, inflictor, MASK_SOLID);
    if (trace.fraction == 1.0f)
      return true;
  }

  Vector3 targ_center;

  if (targ->linked)
    targ_center = (targ->absMin + targ->absMax) * 0.5f;
  else
    targ_center = targ->s.origin;

  trace = gi.traceLine(inflictor_center, targ_center, inflictor, MASK_SOLID);
  if (trace.fraction == 1.0f)
    return true;

  dest = targ_center;
  dest[0] += 15.0f;
  dest[1] += 15.0f;
  trace = gi.traceLine(inflictor_center, dest, inflictor, MASK_SOLID);
  if (trace.fraction == 1.0f)
    return true;

  dest = targ_center;
  dest[0] += 15.0f;
  dest[1] -= 15.0f;
  trace = gi.traceLine(inflictor_center, dest, inflictor, MASK_SOLID);
  if (trace.fraction == 1.0f)
    return true;

  dest = targ_center;
  dest[0] -= 15.0f;
  dest[1] += 15.0f;
  trace = gi.traceLine(inflictor_center, dest, inflictor, MASK_SOLID);
  if (trace.fraction == 1.0f)
    return true;

  dest = targ_center;
  dest[0] -= 15.0f;
  dest[1] -= 15.0f;
  trace = gi.traceLine(inflictor_center, dest, inflictor, MASK_SOLID);
  if (trace.fraction == 1.0f)
    return true;

  return false;
}

/*
============
Killed
============
*/
void Killed(gentity_t *targ, gentity_t *inflictor, gentity_t *attacker,
            int damage, const Vector3 &point, MeansOfDeath mod) {
  if (targ->health < -999)
    targ->health = -999;

  // [Paril-KEX]
  if ((targ->svFlags & SVF_MONSTER) && targ->monsterInfo.aiFlags & AI_MEDIC) {
    if (targ->enemy && targ->enemy->inUse &&
        (targ->enemy->svFlags & SVF_MONSTER)) // god, I hope so
      M_CleanupHealTarget(targ->enemy);

    // clean up self
    targ->monsterInfo.aiFlags &= ~AI_MEDIC;
  }

  targ->enemy = attacker;
  targ->lastMOD = mod;

  // [Paril-KEX] monsters call die in their damage handler
  if (targ->svFlags & SVF_MONSTER)
    return;

  targ->die(targ, inflictor, attacker, damage, point, mod);

  if (targ->monsterInfo.setSkin)
    targ->monsterInfo.setSkin(targ);
}

/*
================
SpawnDamage
================
*/
void SpawnDamage(int type, const Vector3 &origin, const Vector3 &normal,
                 int damage) {
  if (damage > 255)
    damage = 255;
  gi.WriteByte(svc_temp_entity);
  gi.WriteByte(type);
  gi.WritePosition(origin);
  gi.WriteDir(normal);
  gi.multicast(origin, MULTICAST_PVS, false);
}

/*
===============
CheckPowerArmor
Absorb incoming damage using power armor (player or monster).
Returns the amount of damage absorbed by power armor.
===============
*/
static int CheckPowerArmor(gentity_t *ent, const Vector3 &point,
                           const Vector3 &normal, int damage,
                           DamageFlags dFlags) {
  if (damage <= 0 || ent->health <= 0)
    return 0;

  if (static_cast<int>(dFlags &
                       (DamageFlags::NoArmor | DamageFlags::NoPowerArmor)))
    return 0;

  gclient_t *client = ent->client;

  item_id_t powerArmorType;
  int *power;

  if (client) {
    powerArmorType = PowerArmorType(ent);
    power = &client->pers.inventory[IT_AMMO_CELLS];
  } else if (ent->svFlags & SVF_MONSTER) {
    powerArmorType = ent->monsterInfo.powerArmorType;
    power = &ent->monsterInfo.powerArmorPower;
  } else {
    return 0;
  }

  if (powerArmorType == IT_NULL || *power <= 0)
    return 0;

  int damagePerCell;
  if (powerArmorType == IT_POWER_SCREEN) {
    // only works if damage point is in front
    Vector3 forward;
    AngleVectors(ent->s.angles, forward, nullptr, nullptr);

    Vector3 vec = point - ent->s.origin;
    vec.normalize();

    const float dot = vec.dot(forward);
    if (dot <= 0.3f)
      return 0;

    damagePerCell = 1;
    damage = damage / 3;
  } else {
    // power armor is weaker in DM
    damagePerCell = deathmatch->integer ? 1 : 2;
    damage = (2 * damage) / 3;
  }

  // ensure tiny hits still consume/absorb at least 1
  damage = std::max(1, damage);

  int save = (*power) * damagePerCell;
  if (save <= 0)
    return 0;

  // energy damage is more effective against power armor
  if (static_cast<int>(dFlags & DamageFlags::Energy))
    save = std::max(1, save / 2);

  if (save > damage)
    save = damage;

  int power_used = (save / damagePerCell);
  if (static_cast<int>(dFlags & DamageFlags::Energy))
    power_used *= 2;

  power_used = std::max(damagePerCell, std::max(1, power_used));

  SpawnDamage(TE_SCREEN_SPARKS, point, normal, save);
  ent->powerArmorTime = level.time + 200_ms;

  *power = std::max(0, *power - power_used);

  // power armor turn-off states
  if (ent->client) {
    CheckPowerArmorState(ent);
  } else if (*power == 0) {
    gi.sound(ent, CHAN_AUTO, gi.soundIndex("misc/mon_power2.wav"), 1.0f,
             ATTN_NORM, 0.0f);

    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_POWER_SPLASH);
    gi.WriteEntity(ent);
    gi.WriteByte((powerArmorType == IT_POWER_SCREEN) ? 1 : 0);
    gi.multicast(ent->s.origin, MULTICAST_PHS, false);
  }

  return save;
}

/*
===============
CheckArmor
Absorb incoming damage using regular armor.
Returns the amount of damage absorbed by armor.
===============
*/
static int CheckArmor(gentity_t *ent, const Vector3 &point,
                      const Vector3 &normal, int damage, int tempEvent,
                      DamageFlags dFlags) {
  if (damage <= 0)
    return 0;

  if (static_cast<int>(dFlags &
                       (DamageFlags::NoArmor | DamageFlags::NoRegularArmor)))
    return 0;

  const item_id_t index = ArmorIndex(ent);
  if (!index)
    return 0;

  const Item *armor = GetItemByIndex(index);
  int save;

  if (static_cast<int>(dFlags & DamageFlags::Energy))
    save = static_cast<int>(std::ceil(
        armor_stats[game.ruleset][armor->quantity].energy_protection * damage));
  else
    save = static_cast<int>(std::ceil(
        armor_stats[game.ruleset][armor->quantity].normal_protection * damage));

  int *power = ent->client ? &ent->client->pers.inventory[index]
                           : &ent->monsterInfo.armor_power;

  save = std::min(save, *power);
  if (save <= 0)
    return 0;

  *power -= save;

  if (!ent->client && ent->monsterInfo.armor_power <= 0)
    ent->monsterInfo.armorType = IT_NULL;

  SpawnDamage(tempEvent, point, normal, save);
  return save;
}

/*
===============
M_ReactToDamage
Simple monster reaction/retargeting logic when damaged.
===============
*/
static void M_ReactToDamage(gentity_t *targ, gentity_t *attacker,
                            gentity_t *inflictor) {
  if (!((attacker && attacker->client) ||
        (attacker && (attacker->svFlags & SVF_MONSTER))))
    return;

  // special-case: tesla mines
  if (inflictor && !strcmp(inflictor->className, "tesla_mine")) {
    if ((MarkTeslaArea(targ, inflictor) || brandom()) &&
        (!targ->enemy || !targ->enemy->className ||
         strcmp(targ->enemy->className, "tesla_mine"))) {
      TargetTesla(targ, inflictor);
    }
    return;
  }

  if (attacker == targ || attacker == targ->enemy)
    return;

  // good guys do not get angry at players or other good guys
  if (targ->monsterInfo.aiFlags & AI_GOOD_GUY) {
    if (attacker->client || (attacker->monsterInfo.aiFlags & AI_GOOD_GUY))
      return;
  }

  // ignore damage briefly if target_anger recently forced anger
  if (targ->enemy && (targ->monsterInfo.aiFlags & AI_TARGET_ANGER)) {
    if (targ->enemy->inUse) {
      const float pct = static_cast<float>(targ->health) /
                        static_cast<float>(targ->maxHealth);
      if (pct > 0.33f)
        return;
    }
    targ->monsterInfo.aiFlags &= ~AI_TARGET_ANGER;
  }

  // throttle reaction retargeting
  if (targ->monsterInfo.react_to_damage_time > level.time)
    return;

  // medics prefer to keep healing unless hurt enough
  if (targ->enemy && (targ->monsterInfo.aiFlags & AI_MEDIC)) {
    const float pct =
        static_cast<float>(targ->health) / static_cast<float>(targ->maxHealth);
    if (targ->enemy->inUse && pct > 0.25f)
      return;

    M_CleanupHealTarget(targ->enemy);
    targ->monsterInfo.aiFlags &= ~AI_MEDIC;
  }

  targ->monsterInfo.react_to_damage_time =
      level.time + random_time(3_sec, 5_sec);

  // attacker is a client: strongly prefer them
  if (attacker->client) {
    targ->monsterInfo.aiFlags &= ~AI_SOUND_TARGET;

    if (targ->enemy != attacker) {
      if (targ->enemy && targ->enemy->client) {
        if (visible(targ, targ->enemy)) {
          targ->oldEnemy = attacker;
          return;
        }
        targ->oldEnemy = targ->enemy;
      }

      if ((targ->svFlags & SVF_MONSTER) &&
          (targ->monsterInfo.aiFlags & AI_MEDIC)) {
        if (targ->enemy && targ->enemy->inUse &&
            (targ->enemy->svFlags & SVF_MONSTER))
          M_CleanupHealTarget(targ->enemy);
        targ->monsterInfo.aiFlags &= ~AI_MEDIC;
      }

      targ->enemy = attacker;
      if (!(targ->monsterInfo.aiFlags & AI_DUCKED))
        FoundTarget(targ);
    }
    return;
  }

  // if they meant to shoot us, or we are similar bases and neither ignores
  // shots, retaliate
  const bool bases_match = ((targ->flags & (FL_FLY | FL_SWIM)) ==
                            (attacker->flags & (FL_FLY | FL_SWIM)));
  const bool kinds_differ = (strcmp(targ->className, attacker->className) != 0);
  const bool can_reciprocate =
      !(attacker->monsterInfo.aiFlags & AI_IGNORE_SHOTS) &&
      !(targ->monsterInfo.aiFlags & AI_IGNORE_SHOTS);

  if (attacker->enemy == targ ||
      (bases_match && kinds_differ && can_reciprocate)) {
    if (targ->enemy != attacker) {
      if ((targ->svFlags & SVF_MONSTER) &&
          (targ->monsterInfo.aiFlags & AI_MEDIC)) {
        if (targ->enemy && targ->enemy->inUse &&
            (targ->enemy->svFlags & SVF_MONSTER))
          M_CleanupHealTarget(targ->enemy);
        targ->monsterInfo.aiFlags &= ~AI_MEDIC;
      }

      if (targ->enemy && targ->enemy->client)
        targ->oldEnemy = targ->enemy;

      targ->enemy = attacker;
      if (!(targ->monsterInfo.aiFlags & AI_DUCKED))
        FoundTarget(targ);
    }
  }
  // help our buddy (attack who attacker is attacking), unless that would target
  // us
  else if (attacker->enemy && attacker->enemy != targ &&
           targ->enemy != attacker->enemy) {
    if ((targ->svFlags & SVF_MONSTER) &&
        (targ->monsterInfo.aiFlags & AI_MEDIC)) {
      if (targ->enemy && targ->enemy->inUse &&
          (targ->enemy->svFlags & SVF_MONSTER))
        M_CleanupHealTarget(targ->enemy);
      targ->monsterInfo.aiFlags &= ~AI_MEDIC;
    }

    if (targ->enemy && targ->enemy->client)
      targ->oldEnemy = targ->enemy;

    targ->enemy = attacker->enemy;
    if (!(targ->monsterInfo.aiFlags & AI_DUCKED))
      FoundTarget(targ);
  }
}

/*
===============
OnSameTeam
True if both entities are clients on the same team (with special rules).
===============
*/
bool OnSameTeam(gentity_t *ent1, gentity_t *ent2) {
  // monsters are not teammates (current rules)
  if (!ent1 || !ent2 || !ent1->client || !ent2->client)
    return false;

  // never considered same team as self
  if (ent1 == ent2)
    return false;

  // QuadHog special: if either has quad, do not consider teammates
  if (g_quadhog->integer) {
    if (ent1->client->PowerupTimer(PowerupTimer::QuadDamage) > level.time ||
        ent2->client->PowerupTimer(PowerupTimer::QuadDamage) > level.time)
      return false;
    return true;
  }

  // Coop: all clients are treated as teammates
  if (CooperativeModeOn())
    return true;

  // Team modes
  if (Teams() && ent1->client && ent2->client)
    return ent1->client->sess.team == ent2->client->sess.team;

  return false;
}

/*
===============
CheckTeamDamage
Respect friendly fire setting; true means "do not deal damage".
===============
*/
bool CheckTeamDamage(gentity_t *targ, gentity_t *attacker) {
  // if ff enabled, allow damage
  if (g_friendlyFireScale->value > 0.0f)
    return false;

  return OnSameTeam(targ, attacker);
}

/*
===============
ApplyKnockback (Improved)
Adds knockback impulse to a target when damaged.
This improved version creates more natural arcs by blending the knockback
direction with an upward vector, and gives special consideration to ground-based
hits to make actions like rocket jumps feel more consistent and powerful.
===============
*/
static void ApplyKnockback(gentity_t *targ, const gentity_t *attacker,
                           const Vector3 &dir, int knockback,
                           DamageFlags dFlags) {
  if (!targ || static_cast<int>(dFlags & DamageFlags::NoKnockback) ||
      knockback <= 0)
    return;

  // Skip for non-physical states and noclip
  switch (targ->moveType) {
    using enum MoveType;
  case None:
  case Push:
  case Stop:
  case NoClip:
    return;
  default:
    break;
  }

  // Skip clients that are not in a movable player state
  if (targ->client) {
    const int pm_type =
        targ->client->ps.pmove.pmType; // PM_NORMAL, PM_SPECTATOR,
                                       // PM_INTERMISSION, PM_FREEZE, etc.
    if (pm_type != PM_NORMAL)
      return;
  }

  // Direction must be finite and non-zero
  if (!std::isfinite(dir.x) || !std::isfinite(dir.y) || !std::isfinite(dir.z))
    return;
  const float dirLen = dir.length();
  if (!(dirLen > 0.0f))
    return;
  const Vector3 knock_dir = dir * (1.0f / dirLen);

  // Mass floor and finite knockback scale
  const int clampedMassI = std::max(50, targ->mass);
  const float clampedMass = static_cast<float>(clampedMassI);

  float scale = 1.0f;
  if (g_knockbackScale) {
    scale = g_knockbackScale->value;
    if (!std::isfinite(scale) || scale < 0.0f) // prevent NaN or reversal
      scale = 0.0f;
  }

  const float base = (targ->client && attacker == targ) ? 1200.0f : 1000.0f;

  // Compute impulse with conservative caps
  constexpr float KNOCKBACK_MAX_INPUT =
      10000.0f; // sanity cap on input knockback
  constexpr float IMPULSE_MAX_MAGNITUDE =
      8000.0f; // max linear impulse magnitude (tune to taste)
  const float kb =
      std::clamp(static_cast<float>(knockback), 0.0f, KNOCKBACK_MAX_INPUT);
  float impulseMag = (base * kb / clampedMass) * scale;

  // small arena buff
  if (Game::Has(GameFlags::Arena))
    impulseMag *= 1.125f;

  // --- NEW LOGIC: Create a more natural knockback arc ---

  // Define how much of the knockback should be vertical lift.
  // A higher value creates a higher arc.
  constexpr float UPWARD_BIAS_AIR = 0.35f;
  constexpr float UPWARD_BIAS_GROUND = 0.7f;

  float upward_bias =
      (targ->groundEntity != nullptr) ? UPWARD_BIAS_GROUND : UPWARD_BIAS_AIR;

  // Blend the original knockback direction with a pure 'up' vector.
  // This ensures a predictable upward arc, which feels more natural.
  Vector3 final_dir =
      (knock_dir * (1.0f - upward_bias)) + Vector3(0.0f, 0.0f, upward_bias);

  // Re-normalize the blended vector to ensure consistent impulse magnitude.
  const float finalDirLen = final_dir.length();
  if (finalDirLen > 0.0f) {
    final_dir *= (1.0f / finalDirLen);
  }
  // --- END NEW LOGIC ---

  // Saturate final impulse magnitude
  impulseMag = std::min(impulseMag, IMPULSE_MAX_MAGNITUDE);

  // Apply, but keep velocity finite and within a sensible cap
  if (!std::isfinite(targ->velocity.x) || !std::isfinite(targ->velocity.y) ||
      !std::isfinite(targ->velocity.z))
    targ->velocity = Vector3{0.0f, 0.0f, 0.0f};

  Vector3 newVel = targ->velocity;

  // For ground-based hits (like rocket jumps), resetting vertical velocity
  // makes the upward launch feel much more crisp and responsive.
  if (targ->groundEntity != nullptr) {
    newVel[2] *= 0.5f; // Dampen current z-velocity instead of zeroing to handle
                       // slopes better.
  }

  newVel += final_dir * impulseMag;

  // Optional: clamp to a maximum speed budget so impulses cannot explode
  // numerically
  constexpr float MAX_RESULT_SPEED = 4000.0f; // tune/cvar if desired
  const float newSpeed = newVel.length();
  if (newSpeed > MAX_RESULT_SPEED && newSpeed > 0.0f)
    newVel *= (MAX_RESULT_SPEED / newSpeed);

  // Only commit finite results
  if (std::isfinite(newVel.x) && std::isfinite(newVel.y) &&
      std::isfinite(newVel.z))
    targ->velocity = newVel;

  // Apply pmove knockback time; prefer extending any existing time rather than
  // losing a stronger new hit
  if (targ->client) {
#if 0
		auto& pm = targ->client->ps.pmove;
		const int t = std::clamp(static_cast<int>(kb) * 2, 50, 200);
		pm.pmTime = std::max<uint16_t>(pm.pmTime, static_cast<uint16_t>(t));
		pm.pmFlags |= PMF_TIME_KNOCKBACK;
#endif
    targ->client->ps.pmove.pmTime = 200;
    targ->client->damage.knockback += knockback;
  }
}

/*
===============
AddInflictedClientDamage
Accumulates damage numbers for HUD and directional indicator logic.
===============
*/
static inline void AddInflictedClientDamage(
    gclient_t *targCl, const Vector3 &point, const gentity_t *attacker,
    const gentity_t *inflictor, int32_t take, int32_t powerArmorSave,
    int32_t armorSave, DamageFlags dFlags, int32_t knockback) {
  if (!targCl)
    return;

  // aggregate totals for this frame
  targCl->damage.powerArmor += powerArmorSave;
  targCl->damage.armor += armorSave;
  targCl->damage.blood += take;
  targCl->damage.knockback += knockback;
  targCl->damage.origin = point;
  targCl->last_damage_time = level.time + COOP_DAMAGE_RESPAWN_TIME;

  // indicator creation filters
  if (static_cast<int>(dFlags & DamageFlags::NoIndicator) ||
      ((take | powerArmorSave | armorSave) == 0))
    return;
  if (!attacker || !inflictor)
    return;
  if (inflictor == world || attacker == world)
    return;

  // find merge target
  damage_indicator_t *indicator = nullptr;
  for (size_t i = 0; i < targCl->numDamageIndicators; ++i) {
    auto &di = targCl->damageIndicators[i];
    if ((point - di.from).length() < 32.0f) {
      indicator = &di;
      break;
    }
  }

  // allocate new one if needed
  if (!indicator && targCl->numDamageIndicators < MAX_DAMAGE_INDICATORS) {
    indicator = &targCl->damageIndicators[targCl->numDamageIndicators++];
    indicator->from = static_cast<int>(dFlags & DamageFlags::Radius)
                          ? inflictor->s.origin
                          : attacker->s.origin;
    indicator->health = 0;
    indicator->armor = 0;
    indicator->power = 0;
  }

  // accumulate
  if (indicator) {
    indicator->health += take;
    indicator->power += powerArmorSave;
    indicator->armor += armorSave;
  }
}

/*
===============
ApplyDamage
Subtract health, spawn effects, handle spheres, and kill target if needed.
Returns true if the target died.
===============
*/
static bool ApplyDamage(gentity_t *targ, gentity_t *inflictor,
                        gentity_t *attacker, gclient_t *targCl, int take,
                        int knockback, const Vector3 &point,
                        const Vector3 &normal, const MeansOfDeath &mod,
                        int tempEvent, bool &sphereNotified) {
  if (take <= 0)
    return false;

  // visual damage effects
  if (!(targ->flags & FL_NO_DAMAGE_EFFECTS)) {
    if (targ->flags & FL_MECHANICAL) {
      SpawnDamage(TE_ELECTRIC_SPARKS, point, normal, take);
    } else if ((targ->svFlags & SVF_MONSTER) || targCl) {
      if (strcmp(targ->className, "monster_gekk") == 0) {
        SpawnDamage(TE_GREENBLOOD, point, normal, take);
      } else if (mod.id == ModID::Chainfist) {
        SpawnDamage(TE_MOREBLOOD, point, normal, 255);
      } else {
        SpawnDamage(TE_BLOOD, point, normal, take);
      }
    } else {
      SpawnDamage(tempEvent, point, normal, take);
    }
  }

  // apply to health (unless game-wide combat disabled)
  if (!targ->client || (targ->client && !CombatIsDisabled())) {
    HM_AddEvent(point, static_cast<float>(take));
    targ->health -= take;

    // consume health bonus first
    if (targ->client && targ->client->pers.healthBonus) {
      targ->client->pers.healthBonus =
          std::max(0, targ->client->pers.healthBonus - take);
      if (targ->health <= 0 && targ->client->pers.healthBonus > 0)
        targ->client->pers.healthBonus = 0;
    }
  }

  // notify owned sphere AI early
  if (targCl && targCl->ownedSphere) {
    sphereNotified = true;
    if (targCl->ownedSphere->pain)
      targCl->ownedSphere->pain(targCl->ownedSphere, attacker, 0, 0, mod);
  }

  // death handling
  if (targ->health <= 0) {
    if ((targ->svFlags & SVF_MONSTER) || targCl) {
      targ->flags |= FL_ALIVE_KNOCKBACK_ONLY;
      targ->dead_time = level.time;

      if (targ->flags & FL_IMMORTAL) {
        targ->health = 1;
      } else if (g_instaGib->integer && attacker && attacker->client &&
                 targCl && mod.id == ModID::Railgun) {
        targ->health = GIB_HEALTH;
      }

      // freeze tag: do not gib unless thawing logic demands
      if (Game::Is(GameType::FreezeTag) && mod.id != ModID::Thaw &&
          targ->health <= targ->gibHealth && attacker && attacker->client) {
        targ->health = targ->gibHealth + 1;
      }
    }

    // record monster damage meta
    {
      auto &dmg = targ->monsterInfo.damage;
      dmg.blood += take;
      dmg.attacker = attacker;
      dmg.inflictor = inflictor;
      dmg.origin = point;
      dmg.mod = mod;
      dmg.knockback += knockback;
    }

    Killed(targ, inflictor, attacker, take, point, mod);
    return true;
  }

  return false;
}

static void CheckDamageProtection(gentity_t *targ, gclient_t *targCl,
                                  const gentity_t *attacker, int &take,
                                  int &save, int damage, DamageFlags dFlags,
                                  const Vector3 &point, const Vector3 &normal,
                                  const MeansOfDeath &mod, int tempEvent) {
  DamageProtectionContext ctx{};
  ctx.hasClient = (targCl != nullptr);
  ctx.combatDisabled = CombatIsDisabled();
  ctx.proBall = Game::Is(GameType::ProBall);
  ctx.selfDamageDisabled = !g_selfDamage->integer || Game::Is(GameType::None);
  ctx.isSelfDamage = (attacker != nullptr) && (targ == attacker);
  ctx.hasBattleSuit =
      (targCl && targCl->PowerupTimer(PowerupTimer::BattleSuit) > level.time);
  ctx.isRadiusDamage = static_cast<int>(dFlags & DamageFlags::Radius);
  ctx.hasGodMode = (targ->flags & FL_GODMODE);
  ctx.isMonster = (targ->svFlags & SVF_MONSTER);
  ctx.monsterInvincibilityTime = targ->monsterInfo.invincibility_time;
  ctx.painDebounceTime = targ->pain_debounce_time;
  ctx.levelTime = level.time;

  (void)point;
  (void)normal;
  (void)tempEvent;

  DamageProtectionResult protection =
      EvaluateDamageProtection(ctx, dFlags, mod);
  if (!protection.prevented)
    return;

  if (protection.playBattleSuitSound) {
    gi.sound(targ, CHAN_AUX, gi.soundIndex("items/protect3.wav"), 1, ATTN_NORM,
             0);
  }

  if (protection.playMonsterSound) {
    gi.sound(targ, CHAN_ITEM, gi.soundIndex("items/protect4.wav"), 1, ATTN_NORM,
             0);
    targ->pain_debounce_time = protection.newPainDebounceTime;
  }

  take = 0;
  save = damage;
}

/*
===============
Damage
Central damage entry point.
===============
*/
void Damage(gentity_t *targ, gentity_t *inflictor, gentity_t *attacker,
            const Vector3 &dir, const Vector3 &point, const Vector3 &normal,
            int damage, int knockback, DamageFlags dFlags, MeansOfDeath mod) {
  if (!targ || !targ->takeDamage)
    return;

  const int tempEvent = static_cast<int>(dFlags & DamageFlags::Bullet)
                            ? TE_BULLET_SPARKS
                            : TE_SPARKS;

  bool sphereNotified = false;
  gclient_t *targCl = targ->client;

#ifndef NDEBUG
  const int originalHealth = targ->health;
  bool zeroDamageScaleActive = false;
#endif

  // friendly fire scaling/flagging
  if (targ != attacker &&
      !static_cast<int>(dFlags & DamageFlags::NoProtection)) {
    if (OnSameTeam(targ, attacker)) {
      mod.friendly_fire = true;

      // scale out damage if ff disabled (except some specials like nuke)
      if (mod.id != ModID::Nuke)
        damage = static_cast<int>(damage * g_friendlyFireScale->value);
    }
  }

  // easy skill halves damage vs players in SP
  if (skill->integer == 0 && !deathmatch->integer && targCl && damage > 0) {
    damage = std::max(1, damage / 2);
  }

  // global damage scale
  const float scaleValue = (targ->svFlags & SVF_MONSTER)
                               ? ai_damage_scale->value
                               : g_damage_scale->value;
  const float clampedScale = std::max(0.0f, scaleValue);
  const float baseDamage = std::max(0.0f, static_cast<float>(damage));
  const float scaledDamage = baseDamage * clampedScale;
  const bool hadPositiveScaledDamage = scaledDamage > 0.0f;

#ifndef NDEBUG
  zeroDamageScaleActive = (clampedScale == 0.0f) && (baseDamage > 0.0f);
#endif

  damage = static_cast<int>(scaledDamage);
  if (hadPositiveScaledDamage && damage <= 0)
    damage = 1;

  // defender sphere halves damage
  if (damage > 0 && targCl && targCl->ownedSphere &&
      (targCl->ownedSphere->spawnFlags == SF_SPHERE_DEFENDER)) {
    damage = std::max(1, damage / 2);
  }

  // surprise bonus vs monsters (non-radius, first hit)
  if (!static_cast<int>(dFlags & DamageFlags::Radius) &&
      (targ->svFlags & SVF_MONSTER) && attacker && attacker->client &&
      (!targ->enemy || targ->monsterInfo.surprise_time == level.time) &&
      (targ->health > 0)) {
    damage *= 2;
    targ->monsterInfo.surprise_time = level.time;
  }

  // Q3A-style knockback cap
  if (RS(Quake3Arena)) {
    knockback = std::min(damage, 200);
  }

  /*freeze*/
  if (Game::Is(GameType::FreezeTag) && targCl && targCl->eliminated)
    knockback *= 2;
  else
    /*freeze*/
    if ((targ->flags & FL_NO_KNOCKBACK) ||
        ((targ->flags & FL_ALIVE_KNOCKBACK_ONLY) &&
         (!targ->deadFlag || targ->dead_time != level.time))) {
      knockback = 0;
    }

  if (g_instaGib->integer && attacker && attacker->client && targCl &&
      mod.id == ModID::Railgun)
    knockback = 100;

  // compute momentum before self-damage halving
  ApplyKnockback(targ, attacker, dir, knockback, dFlags);

  // always give half damage if hurting self (after knockback calc)
  if (targ == attacker && damage > 0) {
    if (Game::Has(GameFlags::Arena))
      damage = 0;
    else
      damage = std::max(1, damage / 2);
  }

  if (damage <= 0)
    damage = 0;

  int take = damage;
  int save = 0;

#ifndef NDEBUG
  if (zeroDamageScaleActive)
    assert(take == 0);
#endif

  FreezeTagDamageQuery freezeQuery{};
  freezeQuery.freezeTagActive = Game::Is(GameType::FreezeTag);
  freezeQuery.targetEliminated = (targCl && targCl->eliminated);
  freezeQuery.targetThawing = (targCl && targCl->resp.thawer);
  freezeQuery.attackerHasClient = (attacker && attacker->client);
  freezeQuery.modIsThaw = (mod.id == ModID::Thaw);

  const bool freezeSuppressed = FreezeTag_ShouldSuppressDamage(freezeQuery);

  // global get-out clauses
  CheckDamageProtection(targ, targCl, attacker, take, save, damage, dFlags,
                        point, normal, mod, tempEvent);

  if (freezeSuppressed)
    take = 0;

  // vampiric healing
  if (g_vampiric_damage->integer && targ->health > 0 && attacker &&
      attacker != targ && !OnSameTeam(targ, attacker) && take > 0) {
    const int maxHP = std::clamp(g_vampiric_health_max->integer, 100, 9999);
    const int base = std::min(take, targ->health);
    const float pct = std::clamp(g_vampiric_percentile->value, 0.0f, 1.0f);

    const int heal = std::max(1, static_cast<int>(std::ceil(base * pct)));
    attacker->health = std::min(attacker->health + heal, maxHP);
  }

  // team armor protect or normal armor flows
  int armorSave = 0;
  int powerArmorSave = 0;

  if (!freezeSuppressed) {
    if (Teams() && targCl && attacker && attacker->client &&
        targCl->sess.team == attacker->client->sess.team && targ != attacker &&
        g_teamplay_armor_protect->integer) {
      // teammates do not drain armor under protect mode
      powerArmorSave = 0;
      armorSave = 0;
    } else {
      if (!(targ == attacker && Game::Has(GameFlags::Arena) &&
            !g_arenaSelfDmgArmor->integer)) {
        powerArmorSave = CheckPowerArmor(targ, point, normal, take, dFlags);
        take -= powerArmorSave;

        armorSave = CheckArmor(targ, point, normal, take, tempEvent, dFlags);
        take -= armorSave;
      }
    }
  }

  // treat previous "save" like armor for HUD/indicators
  armorSave += save;

  // additional protections and powerups
  if (!freezeSuppressed &&
      !static_cast<int>(dFlags & DamageFlags::NoProtection)) {
    // tech: disruptor shield, etc.
    take = Tech_ApplyDisruptorShield(targ, take);

    // spawn protection
    if (take > 0 && targCl &&
        targCl->PowerupTimer(PowerupTimer::SpawnProtection) > level.time) {
      gi.sound(targ, CHAN_AUX, gi.soundIndex("items/protect3.wav"), 1,
               ATTN_NORM, 0);
      take = 0;
      targCl->pu_time_spawn_protection_blip = level.time + 100_ms;
    }

    // battle suit halves remaining damage
    if (take > 0 && targCl &&
        targCl->PowerupTimer(PowerupTimer::BattleSuit) > level.time) {
      gi.sound(targ, CHAN_AUX, gi.soundIndex("items/protect3.wav"), 1,
               ATTN_NORM, 0);
      take = static_cast<int>(std::ceil(static_cast<float>(take) / 2.0f));
    }

    // empathy shield halves remaining damage and inflicts the same damage to
    // attacker
    if (targCl &&
        targCl->PowerupTimer(PowerupTimer::EmpathyShield) > level.time &&
        take > 0 && attacker && targ != attacker) {
      gi.sound(targ, CHAN_AUX, gi.soundIndex("items/empathy_hit.wav"), 1,
               ATTN_NORM, 0);
      take = static_cast<int>(std::ceil(static_cast<float>(take) / 2.0f));

      Damage(attacker, nullptr, targ, dir, point, normal, take, 0,
             DamageFlags::NoProtection | DamageFlags::NoKnockback |
                 DamageFlags::NoIndicator,
             mod);
    }
  }

  if (!freezeSuppressed)
    CTF_CheckHurtCarrier(targ, attacker);

  // DamageFlags::DestroyArmor: do full damage through armor unless explicitly
  // protected
  if (!freezeSuppressed &&
      static_cast<int>(dFlags & DamageFlags::DestroyArmor)) {
    if (!(targ->flags & FL_GODMODE) &&
        !static_cast<int>(dFlags & DamageFlags::NoProtection) &&
        !(targCl &&
          targCl->PowerupTimer(PowerupTimer::BattleSuit) > level.time)) {
      take = damage;
    }
  }

  // scoring and stat tracking for the attacker (only if target still alive
  // here)
  if (!freezeSuppressed && targ != attacker && attacker && attacker->client &&
      targ->health > 0) {
    int statTake = std::min(take, targ->health);

    // arena damage scoring: +1 score per 100 dmg dealt to enemies
    if (Game::Has(GameFlags::Arena) && !OnSameTeam(targ, attacker)) {
      attacker->client->pers.dmg_scorer +=
          statTake + powerArmorSave + armorSave;

      while (attacker->client->pers.dmg_scorer >= 100) {
        attacker->client->pers.dmg_scorer -= 100;
        G_AdjustPlayerScore(attacker->client, 1, false, 0);
      }
    }

    // team damage accumulation/warning
    if (OnSameTeam(targ, attacker)) {
      attacker->client->pers.dmg_team += statTake + powerArmorSave + armorSave;

      while (attacker->client->pers.dmg_team >= 100) {
        attacker->client->pers.dmg_team -= 100;
        gi.LocClient_Print(
            attacker, PRINT_CENTER,
            "You are on {} Team,\nstop attacking your team mates!\n",
            Teams_TeamName(attacker->client->sess.team));
      }
    }

    // hit markers (skip target_laser)
    if (!((targ->svFlags & SVF_DEADMONSTER) ||
          (targ->flags & FL_NO_DAMAGE_EFFECTS)) &&
        mod.id != ModID::Laser) {
      attacker->client->ps.stats[STAT_HIT_MARKER] +=
          statTake + powerArmorSave + armorSave;
    }

    attacker->client->pers.match.totalDmgDealt +=
        statTake + powerArmorSave + armorSave;
    attacker->client->pers.match.modTotalDmgD[static_cast<int>(mod.id)] +=
        statTake + powerArmorSave + armorSave;

    if (!inflictor || (inflictor && !inflictor->skip)) {
      attacker->client->pers.match.totalHits++;
      attacker->client->pers.match.totalHitsPerWeapon[static_cast<int>(
          modr[static_cast<int>(mod.id)].weapon)]++;

      // skip MG/CG inflictor skip toggle to keep continuous fire sane
      if (inflictor && mod.id != ModID::Machinegun && mod.id != ModID::Chaingun)
        inflictor->skip = true;
    }

    if (targCl) {
      targCl->pers.match.totalDmgReceived +=
          statTake + powerArmorSave + armorSave;
      targCl->pers.match.modTotalDmgR[static_cast<int>(mod.id)] +=
          statTake + powerArmorSave + armorSave;
    }
  }

  // actually apply the damage; can kill
  if (ApplyDamage(targ, inflictor, attacker, targCl, take, knockback, point,
                  normal, mod, tempEvent, sphereNotified))
    return;

#ifndef NDEBUG
  if (zeroDamageScaleActive)
    assert(targ->health == originalHealth);
#endif

  if (Game::Is(GameType::FreezeTag) && !level.intermission.time && targCl &&
      targCl->eliminated && targ->health <= targ->gibHealth &&
      (!attacker || !attacker->client)) {
    FreezeTag_ForceRespawn(targ);
    return;
  }

  // spheres need to know the attacker to retaliate
  if (!sphereNotified && targCl && targCl->ownedSphere &&
      targCl->ownedSphere->pain)
    targCl->ownedSphere->pain(targCl->ownedSphere, attacker, 0, 0, mod);

  if (targCl)
    targCl->last_attacker_time = level.time;

  // pain callbacks / monster reaction and cosmetic updates
  if (targ->svFlags & SVF_MONSTER) {
    if (damage > 0) {
      M_ReactToDamage(targ, attacker, inflictor);

      auto &dmg = targ->monsterInfo.damage;
      dmg.attacker = attacker;
      dmg.inflictor = inflictor;
      dmg.blood += take;
      dmg.origin = point;
      dmg.mod = mod;
      dmg.knockback += knockback;
    }

    if (targ->monsterInfo.setSkin)
      targ->monsterInfo.setSkin(targ);
  } else if (take > 0 && targ->pain) {
    targ->pain(targ, attacker, static_cast<float>(knockback), take, mod);
  }

  // final HUD accumulation
  AddInflictedClientDamage(targCl, point, attacker, inflictor, take,
                           powerArmorSave, armorSave, dFlags, knockback);
}

/*
============
RadiusDamage
============
*/
#if 0
bool RadiusDamage(gentity_t* inflictor, gentity_t* attacker, float damage, gentity_t* ignore, float radius, DamageFlags dFlags, MeansOfDeath mod) {
	float	 points;
	gentity_t* ent = nullptr;
	Vector3	 v, dir, origin;

	origin = inflictor->linked ? ((inflictor->absMax + inflictor->absMin) * 0.5f) : inflictor->s.origin;

	while ((ent = FindRadius(ent, origin, radius)) != nullptr) {
		if (ent == ignore)
			continue;
		if (!ent->takeDamage)
			continue;

		if (ent->solid == SOLID_BSP && ent->linked)
			v = closest_point_to_box(origin, ent->absMin, ent->absMax);
		else {
			v = ent->mins + ent->maxs;
			v = ent->s.origin + (v * 0.5f);
		}
		v = origin - v;
		points = damage - 0.5f * v.length();
		if (ent == attacker)
			points *= 0.5f;
		if (points > 0) {
			if (CanDamage(ent, inflictor)) {
				dir = (ent->s.origin - origin).normalized();
				// [Paril-KEX] use closest point on bbox to explosion position
				// to spawn damage effect

				float kb = points;
				if (mod.id == ModID::HyperBlaster)
					kb *= 5;

				Damage(ent, inflictor, attacker, dir, closest_point_to_box(origin, ent->absMin, ent->absMax), dir, (int)points, (int)kb, dFlags | DamageFlags::Radius, mod);
			}
		}
	}
}
#endif

bool RadiusDamage(gentity_t *inflictor, gentity_t *attacker, float damage,
                  gentity_t *ignore, float radius, DamageFlags dFlags,
                  MeansOfDeath mod) {
  float points;
  gentity_t *ent = nullptr;
  Vector3 v, dir, origin;
  bool hitClient = false;

  if (radius < 1)
    radius = 1;

  origin = inflictor->linked ? ((inflictor->absMax + inflictor->absMin) * 0.5f)
                             : inflictor->s.origin;

  while ((ent = FindRadius(ent, origin, radius)) != nullptr) {
    if (ent == ignore)
      continue;
    if (!ent->takeDamage)
      continue;

    // [Paril-KEX] Q3A-style: calculate distance to the closest point of the
    // bounding box
    Vector3 bmin = ent->absMin;
    Vector3 bmax = ent->absMax;

    // normalize possibly inverted boxes
    if (bmin.x > bmax.x)
      std::swap(bmin.x, bmax.x);
    if (bmin.y > bmax.y)
      std::swap(bmin.y, bmax.y);
    if (bmin.z > bmax.z)
      std::swap(bmin.z, bmax.z);

    v = closest_point_to_box(origin, bmin, bmax);
    v = origin - v;

    if (v.length() >= radius)
      continue;

    points = damage * (1.0f - v.length() / radius);

    if (points > 0) {
      if (CanDamage(ent, inflictor)) {
        if (LogAccuracyHit(ent, attacker))
          hitClient = true;
        dir = (ent->s.origin - origin).normalized();

        // push the center of mass higher than the origin so players
        // get knocked into the air more
        dir[2] += 24.0f;

        const Vector3 hitPoint = closest_point_to_box(origin, bmin, bmax);

        Damage(ent, inflictor, attacker, dir, hitPoint, dir, (int)points,
               (int)points, dFlags | DamageFlags::Radius, mod);
      }
    }
  }
  return hitClient;
}

/*
============
RadiusNukeDamage

Like RadiusDamage, but ignores walls (skips CanDamage check, among others)
// up to KILLZONE radius, do 10,000 points
// after that, do damage linearly out to KILLZONE2 radius
============
*/

void RadiusNukeDamage(gentity_t *inflictor, gentity_t *attacker, float damage,
                      gentity_t *ignore, float radius, MeansOfDeath mod) {
  float points;
  gentity_t *ent = nullptr;
  Vector3 v;
  Vector3 dir;
  float len;
  float killZone, killZone2;
  trace_t tr;
  float dist;

  killZone = radius;
  killZone2 = radius * 2.0f;

  while ((ent = FindRadius(ent, inflictor->s.origin, killZone2)) != nullptr) {
    // ignore nobody
    if (ent == ignore)
      continue;
    if (!ent->takeDamage)
      continue;
    if (!ent->inUse)
      continue;
    if (!(ent->client || (ent->svFlags & SVF_MONSTER) ||
          (ent->flags & FL_DAMAGEABLE)))
      continue;

    v = ent->mins + ent->maxs;
    v = ent->s.origin + (v * 0.5f);
    v = inflictor->s.origin - v;
    len = v.length();
    if (len <= killZone) {
      if (ent->client)
        ent->flags |= FL_NOGIB;
      points = 10000;
    } else if (len <= killZone2)
      points = (damage / killZone) * (killZone2 - len);
    else
      points = 0;

    if (points > 0) {
      if (ent->client)
        ent->client->nukeTime = level.time + 2_sec;
      dir = ent->s.origin - inflictor->s.origin;
      Damage(ent, inflictor, attacker, dir, inflictor->s.origin, vec3_origin,
             (int)points, (int)points, DamageFlags::Radius, mod);
    }
  }
  ent = g_entities + 1; // skip the worldspawn
  // cycle through players
  while (ent) {
    if ((ent->client) && (ent->client->nukeTime != level.time + 2_sec) &&
        (ent->inUse)) {
      tr = gi.traceLine(inflictor->s.origin, ent->s.origin, inflictor,
                        MASK_SOLID);
      if (tr.fraction == 1.0f)
        ent->client->nukeTime = level.time + 2_sec;
      else {
        dist = realrange(ent, inflictor);
        if (dist < 2048)
          ent->client->nukeTime =
              max(ent->client->nukeTime, level.time + 1.5_sec);
        else
          ent->client->nukeTime =
              max(ent->client->nukeTime, level.time + 1_sec);
      }
      ent++;
    } else
      ent = nullptr;
  }
}
