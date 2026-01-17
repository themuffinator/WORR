/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

g_weapon.c*/

#include "../../bgame/weapon_pref_utils.hpp"
#include "../g_local.hpp"
#include "../gameplay/g_proball.hpp"
#include "../monsters/m_player.hpp"


#include <array>
#include <cmath>

bool isQuad = false;
bool isHaste = false;
player_muzzle_t isSilenced = MZ_NONE;
uint8_t damageMultiplier = 1;

/*
================
InfiniteAmmoOn
================
*/
bool InfiniteAmmoOn(Item *item) {
  if (item && item->flags & IF_NO_INFINITE_AMMO)
    return false;

  return g_infiniteAmmo->integer ||
         (deathmatch->integer && (g_instaGib->integer || g_nadeFest->integer));
}

/*
================
PlayerDamageModifier
================
*/
uint8_t PlayerDamageModifier(gentity_t *ent) {
  isQuad = false;
  damageMultiplier = 0;

  struct PowerupCheck {
    PowerupTimer timer;
    uint8_t multiplier;
  };

  constexpr std::array<PowerupCheck, 2> kDamagePowerups{
      {{PowerupTimer::QuadDamage, 4}, {PowerupTimer::DoubleDamage, 2}}};

  for (const auto &[timer, mult] : kDamagePowerups) {
    if (ent->client->PowerupTimer(timer) > level.time) {
      damageMultiplier += mult;
      isQuad = true;
    }
  }

  if (ent->client->pers.inventory[IT_TECH_POWER_AMP])
    damageMultiplier += 2;

  damageMultiplier = std::max<uint8_t>(1, damageMultiplier);
  return damageMultiplier;
}

/*
================
P_CurrentKickFactor

[Paril-KEX] kicks in vanilla take place over 2 10hz server
frames; this is to mimic that visual behavior on any tickrate.
================
*/
static inline float P_CurrentKickFactor(gentity_t *ent) {
  if (ent->client->kick.time < level.time)
    return 0.f;

  return (ent->client->kick.time - level.time).seconds() /
         ent->client->kick.total.seconds();
}

/*
================
P_CurrentKickAngles
================
*/
Vector3 P_CurrentKickAngles(gentity_t *ent) {
  return ent->client->kick.angles * P_CurrentKickFactor(ent);
}

/*
================
P_CurrentKickOrigin
================
*/
Vector3 P_CurrentKickOrigin(gentity_t *ent) {
  return ent->client->kick.origin * P_CurrentKickFactor(ent);
}

/*
================
P_AddWeaponKick
================
*/
void P_AddWeaponKick(gentity_t *ent, const Vector3 &origin,
                     const Vector3 &angles) {
  ent->client->kick.origin = origin;
  ent->client->kick.angles = angles;
  ent->client->kick.total = 200_ms;
  ent->client->kick.time = level.time + ent->client->kick.total;
}

/*
================
P_ProjectSource

Projects the weapon muzzle position and direction from the player's view.
================
*/
void P_ProjectSource(gentity_t *ent, const Vector3 &angles, Vector3 distance,
                     Vector3 &result_start, Vector3 &result_dir) {
  // Adjust distance based on projection settings or handedness
  if (g_weaponProjection->integer > 0) {
    // Horizontally centralize the weapon projection
    distance[_Y] = 0;
    if (g_weaponProjection->integer > 1)
      // Vertically centralize the weapon projection, too
      distance[_Z] = 0;
  } else {
    switch (ent->client->pers.hand) {
    case Handedness::Left:
      distance[_Y] *= -1;
      break;
    case Handedness::Center:
      distance[_Y] = 0;
      break;
    default:
      break;
    }
  }

  Vector3 forward, right, up;
  AngleVectors(angles, forward, right, up);

  Vector3 eye_pos =
      ent->s.origin + Vector3{0, 0, static_cast<float>(ent->viewHeight)};
  result_start = G_ProjectSource2(eye_pos, distance, forward, right, up);

  Vector3 end = eye_pos + forward * 8192.0f;

  contents_t mask = MASK_PROJECTILE & ~CONTENTS_DEADMONSTER;
  if (!G_ShouldPlayersCollide(true))
    mask &= ~CONTENTS_PLAYER;

  trace_t tr = gi.traceLine(eye_pos, end, ent, mask);

  bool closeToTarget = (tr.fraction * 8192.0f) < 128.0f;
  bool hitEntity =
      tr.startSolid || (tr.contents & (CONTENTS_MONSTER | CONTENTS_PLAYER));

  // Use raw forward if we hit something close (e.g., monster/player)
  if (hitEntity && closeToTarget)
    result_dir = forward;
  else
    result_dir = (tr.endPos - result_start).normalized();
}

/*
===============
G_PlayerNoise

Each player can have two noise objects:
- myNoise: personal sounds (jumping, pain, firing)
- myNoise2: impact sounds (bullet wall impacts)

These allow AI to move toward noise origins to locate players.
===============
*/
void G_PlayerNoise(gentity_t *who, const Vector3 &where, PlayerNoise type) {
  if (type == PlayerNoise::Weapon) {
    auto &silencerShots =
        who->client->PowerupCount(PowerupCount::SilencerShots);
    if (silencerShots) {
      who->client->invisibility_fade_time =
          level.time + (INVISIBILITY_TIME / 5);
      silencerShots--;
      return;
    }

    who->client->invisibility_fade_time = level.time + INVISIBILITY_TIME;

    auto &spawnProtection =
        who->client->PowerupTimer(PowerupTimer::SpawnProtection);
    if (spawnProtection > level.time)
      spawnProtection = 0_sec;
  }

  if (deathmatch->integer || (who->flags & FL_NOTARGET))
    return;

  if (type == PlayerNoise::Self &&
      (who->client->landmark_free_fall ||
       who->client->landmark_noise_time >= level.time))
    return;

  if (who->flags & FL_DISGUISED) {
    if (type == PlayerNoise::Weapon) {
      level.campaign.disguiseViolator = who;
      level.campaign.disguiseViolationTime = level.time + 500_ms;
    }
    return;
  }

  // Create noise entities if not yet created
  if (!who->myNoise) {
    auto createNoise = [who]() {
      gentity_t *noise = Spawn();
      noise->className = "player_noise";
      noise->mins = {-8, -8, -8};
      noise->maxs = {8, 8, 8};
      noise->owner = who;
      noise->svFlags = SVF_NOCLIENT;
      return noise;
    };

    who->myNoise = createNoise();
    who->myNoise2 = createNoise();
  }

  // Select appropriate noise entity
  gentity_t *noise = (type == PlayerNoise::Self || type == PlayerNoise::Weapon)
                         ? who->myNoise
                         : who->myNoise2;

  // Update client's sound entity refs
  if (type == PlayerNoise::Self || type == PlayerNoise::Weapon) {
    who->client->sound_entity = noise;
    who->client->sound_entity_time = level.time;
  } else {
    who->client->sound2_entity = noise;
    who->client->sound2_entity_time = level.time;
  }

  // Position and activate noise entity
  noise->s.origin = where;
  noise->absMin = where - noise->maxs;
  noise->absMax = where + noise->maxs;
  noise->teleportTime = level.time;

  gi.linkEntity(noise);
}

/*
================
G_WeaponShouldStay
================
*/
static inline bool G_WeaponShouldStay() {
  if (deathmatch->integer)
    return match_weaponsStay->integer;
  else if (coop->integer)
    return !P_UseCoopInstancedItems();

  return false;
}

/*
================
Pickup_Weapon
================
*/
void G_CheckAutoSwitch(gentity_t *ent, Item *item, bool is_new);

bool Pickup_Weapon(gentity_t *ent, gentity_t *other) {
  const item_id_t index = ent->item->id;

  // Respect weapon stay logic unless the weapon was dropped
  if (G_WeaponShouldStay() && other->client->pers.inventory[index]) {
    if (!(ent->spawnFlags &
          (SPAWNFLAG_ITEM_DROPPED | SPAWNFLAG_ITEM_DROPPED_PLAYER)))
      return false;
  }

  const bool is_new = !other->client->pers.inventory[index];

  // Only give ammo if not a dropped player weapon or count is specified
  if (!(ent->spawnFlags & SPAWNFLAG_ITEM_DROPPED) || ent->count) {
    if (ent->item->ammo) {
      Item *ammo = GetItemByIndex(ent->item->ammo);
      if (ammo) {
        if (InfiniteAmmoOn(ammo)) {
          Add_Ammo(other, ammo, AMMO_INFINITE);
        } else {
          int count = 0;

          if (RS(Quake3Arena)) {
            count = ent->count ? ent->count
                               : (ammo->id == IT_AMMO_GRENADES ||
                                          ammo->id == IT_AMMO_ROCKETS ||
                                          ammo->id == IT_AMMO_SLUGS
                                      ? 10
                                      : ammo->quantity);

            if (other->client->pers.inventory[ammo->id] < count)
              count -= other->client->pers.inventory[ammo->id];
            else
              count = 1;
          } else {
            if (ammo && InfiniteAmmoOn(ent->item)) {
              count = AMMO_INFINITE;
            } else if (ent->count) {
              count = ent->count;
            } else {
              count = ammo->quantity;
            }
          }

          Add_Ammo(other, ammo, count);
        }
      }
    }

    // Handle respawn logic
    if (!(ent->spawnFlags & SPAWNFLAG_ITEM_DROPPED_PLAYER)) {
      if (deathmatch->integer) {
        if (match_weaponsStay->integer)
          ent->flags |= FL_RESPAWN;

        SetRespawn(ent, GameTime::from_sec(g_weapon_respawn_time->integer),
                   !match_weaponsStay->integer);
      }
      if (coop->integer)
        ent->flags |= FL_RESPAWN;
    }
  }

  // Increment inventory and consider auto-switch
  other->client->pers.inventory[index]++;
  G_CheckAutoSwitch(other, ent->item, is_new);

  return true;
}

/*
================
Weapon_RunThink
================
*/
static void Weapon_RunThink(gentity_t *ent) {
  // call active weapon think routine
  if (!ent->client->pers.weapon->weaponThink)
    return;

  PlayerDamageModifier(ent);

  isHaste = ent->client->PowerupTimer(PowerupTimer::Haste) > level.time;
  isSilenced = ent->client->PowerupCount(PowerupCount::SilencerShots)
                   ? MZ_SILENCED
                   : MZ_NONE;

  ent->client->pers.weapon->weaponThink(ent);
}

/*
===============
Change_Weapon

The old weapon has been fully holstered; equip the new one.
===============
*/
void Change_Weapon(gentity_t *ent) {
  auto *client = ent->client;

  // Don't allow holstering unless switching is instant or in frenzy mode
  if (ent->health > 0 && !g_instantWeaponSwitch->integer &&
      !g_frenzy->integer &&
      ((client->latchedButtons | client->buttons) & BUTTON_HOLSTER)) {
    return;
  }

  // Drop held grenade if active
  if (client->grenadeTime) {
    client->weaponSound = 0;
    Weapon_RunThink(ent);
    client->grenadeTime = 0_ms;
  }

  if (client->pers.weapon) {
    client->pers.lastWeapon = client->pers.weapon;

    // Play switch sound only when changing weapons and quick switch enabled
    if (client->weapon.pending &&
        client->weapon.pending != client->pers.weapon) {
      if (g_quickWeaponSwitch->integer || g_instantWeaponSwitch->integer)
        gi.sound(ent, CHAN_WEAPON, gi.soundIndex("weapons/change.wav"), 1,
                 ATTN_NORM, 0);
    }
  }

  client->pers.weapon = client->weapon.pending;
  client->weapon.pending = nullptr;

  // Update model skin if applicable
  if (ent->s.modelIndex == MODELINDEX_PLAYER)
    P_AssignClientSkinNum(ent);

  if (!client->pers.weapon) {
    // No weapon: hide model
    client->ps.gunIndex = 0;
    client->ps.gunSkin = 0;
    return;
  }

  // Begin weapon animation
  client->weaponState = WeaponState::Activating;
  client->ps.gunFrame = 0;
  client->ps.gunIndex = gi.modelIndex(client->pers.weapon->viewModel);
  client->ps.gunSkin = 0;
  client->weaponSound = 0;

  // Apply transition animation
  client->anim.priority = ANIM_PAIN;
  if (client->ps.pmove.pmFlags & PMF_DUCKED) {
    ent->s.frame = FRAME_crpain1;
    client->anim.end = FRAME_crpain4;
  } else {
    ent->s.frame = FRAME_pain301;
    client->anim.end = FRAME_pain304;
  }
  client->anim.time = 0_ms;

  // Apply immediate think if switching is instant
  if (g_instantWeaponSwitch->integer || g_frenzy->integer)
    Weapon_RunThink(ent);
}

constexpr std::array<item_id_t, 22> weaponPriorityList{
    {IT_WEAPON_DISRUPTOR,   IT_WEAPON_BFG,          IT_WEAPON_RAILGUN,
     IT_WEAPON_THUNDERBOLT, IT_WEAPON_PLASMABEAM,   IT_WEAPON_PLASMAGUN,
     IT_WEAPON_IONRIPPER,   IT_WEAPON_HYPERBLASTER, IT_WEAPON_ETF_RIFLE,
     IT_WEAPON_CHAINGUN,    IT_WEAPON_MACHINEGUN,   IT_WEAPON_SSHOTGUN,
     IT_WEAPON_SHOTGUN,     IT_WEAPON_PHALANX,      IT_WEAPON_RLAUNCHER,
     IT_WEAPON_GLAUNCHER,   IT_WEAPON_PROXLAUNCHER, IT_AMMO_GRENADES,
     IT_AMMO_TRAP,          IT_AMMO_TESLA,          IT_WEAPON_BLASTER,
     IT_WEAPON_CHAINFIST}};

static item_id_t weaponIndexToItemID(Weapon weaponIndex) {
  switch (weaponIndex) {
    using enum Weapon;
  case Blaster:
    return IT_WEAPON_BLASTER;
  case Chainfist:
    return IT_WEAPON_CHAINFIST;
  case Shotgun:
    return IT_WEAPON_SHOTGUN;
  case SuperShotgun:
    return IT_WEAPON_SSHOTGUN;
  case Machinegun:
    return IT_WEAPON_MACHINEGUN;
  case ETFRifle:
    return IT_WEAPON_ETF_RIFLE;
  case Chaingun:
    return IT_WEAPON_CHAINGUN;
  case HandGrenades:
    return IT_AMMO_GRENADES;
  case Trap:
    return IT_AMMO_TRAP;
  case TeslaMine:
    return IT_AMMO_TESLA;
  case GrenadeLauncher:
    return IT_WEAPON_GLAUNCHER;
  case ProxLauncher:
    return IT_WEAPON_PROXLAUNCHER;
  case RocketLauncher:
    return IT_WEAPON_RLAUNCHER;
  case HyperBlaster:
    return IT_WEAPON_HYPERBLASTER;
  case IonRipper:
    return IT_WEAPON_IONRIPPER;
  case PlasmaGun:
    return IT_WEAPON_PLASMAGUN;
  case PlasmaBeam:
    return IT_WEAPON_PLASMABEAM;
  case Thunderbolt:
    return IT_WEAPON_THUNDERBOLT;
  case Railgun:
    return IT_WEAPON_RAILGUN;
  case Phalanx:
    return IT_WEAPON_PHALANX;
  case BFG10K:
    return IT_WEAPON_BFG;
  case Disruptor:
    return IT_WEAPON_DISRUPTOR;
  default:
    return IT_NULL;
  }
}

void Client_RebuildWeaponPreferenceOrder(gclient_t &cl) {
  auto &cache = cl.sess.weaponPrefOrder;
  cache.clear();

  std::array<bool, static_cast<size_t>(IT_TOTAL)> seen{};
  auto add_item = [&](item_id_t item) {
    const auto index = static_cast<size_t>(item);
    if (!item || index >= seen.size() || seen[index])
      return;

    seen[index] = true;
    cache.push_back(item);
  };

  for (Weapon weaponIndex : cl.sess.weaponPrefs) {
    const auto weaponEnumIndex = static_cast<size_t>(weaponIndex);
    if (weaponIndex == Weapon::None ||
        weaponEnumIndex >= static_cast<size_t>(Weapon::Total))
      continue;

    add_item(weaponIndexToItemID(weaponIndex));
  }

  for (item_id_t def : weaponPriorityList)
    add_item(def);
}

std::vector<std::string> GetSanitizedWeaponPrefStrings(const gclient_t &cl) {
  std::vector<std::string> result;

  if (cl.sess.weaponPrefs.empty())
    return result;

  std::array<bool, static_cast<size_t>(Weapon::Total)> seen{};
  for (Weapon weaponIndex : cl.sess.weaponPrefs) {
    const auto index = static_cast<size_t>(weaponIndex);
    if (weaponIndex == Weapon::None || index >= seen.size() || seen[index])
      continue;

    seen[index] = true;
    std::string_view abbr = WeaponToAbbreviation(weaponIndex);
    if (!abbr.empty())
      result.emplace_back(abbr);
  }

  return result;
}

/*
=============
BuildEffectiveWeaponPriority
Combines client preferences with default weapon priority list.
=============
*/
/*
=============
GetWeaponPriorityIndex
Returns effective priority index for a weapon based on client preference.
Lower index = higher priority.
=============
*/
[[maybe_unused]] static int GetWeaponPriorityIndex(gclient_t &cl, const std::string &abbr) {
  const auto weapon = ParseWeaponAbbreviation(abbr);
  if (!weapon)
    return 9999; // unknown weapon = lowest priority

  Client_RebuildWeaponPreferenceOrder(cl);
  const auto &order = cl.sess.weaponPrefOrder;

  const item_id_t item = weaponIndexToItemID(*weapon);
  if (!item)
    return 9999;

  for (size_t i = 0; i < order.size(); ++i) {
    if (order[i] == item)
      return static_cast<int>(i);
  }

  return 9999; // not in known weapon list
}

/*
=================
NoAmmoWeaponChange

Automatically switches to the next available weapon when out of ammo.
Optionally plays a "click" sound indicating no ammo.
=================
*/
void NoAmmoWeaponChange(gentity_t *ent, bool playSound) {
  auto *client = ent->client;

  if (playSound && level.time >= client->emptyClickSound) {
    gi.sound(ent, CHAN_WEAPON, gi.soundIndex("weapons/noammo.wav"), 1,
             ATTN_NORM, 0);
    client->emptyClickSound = level.time + 1_sec;
  }

  Client_RebuildWeaponPreferenceOrder(*client);

  auto try_list = [&](const auto &ids) {
    for (item_id_t id : ids) {
      Item *item = GetItemByIndex(id);
      if (!item) {
        gi.Com_ErrorFmt("Invalid fallback weapon ID: {}\n",
                        static_cast<int32_t>(id));
        continue;
      }

      if (client->pers.inventory[item->id] <= 0)
        continue;

      if (item->ammo && client->pers.inventory[item->ammo] < item->quantity)
        continue;

      client->weapon.pending = item;
      return true;
    }

    return false;
  };

  if (client->sess.weaponPrefOrder.empty()) {
    if (try_list(weaponPriorityList))
      return;
  } else if (try_list(client->sess.weaponPrefOrder)) {
    return;
  }
}

/*
================
RemoveAmmo

Reduces the player's ammo count for their current weapon.
Triggers a low ammo warning sound if the threshold is crossed.
================
*/
static void RemoveAmmo(gentity_t *ent, int32_t quantity) {
  auto *cl = ent->client;
  auto *weapon = cl->pers.weapon;

  if (InfiniteAmmoOn(weapon))
    return;

  const int ammoIndex = weapon->ammo;
  const int threshold = weapon->quantityWarn;
  int &ammoCount = cl->pers.inventory[ammoIndex];

  const bool wasAboveWarning = ammoCount > threshold;

  ammoCount -= quantity;

  if (wasAboveWarning && ammoCount <= threshold) {
    gi.localSound(ent, CHAN_AUTO, gi.soundIndex("weapons/lowammo.wav"), 1,
                  ATTN_NORM, 0);
  }

  CheckPowerArmorState(ent);
}

/*
================
Weapon_AnimationTime

Determines the duration of one weapon animation frame based on modifiers
such as quick switching, haste, time acceleration, and frenzy mode.
================
*/
static inline GameTime Weapon_AnimationTime(gentity_t *ent) {
  auto &cl = *ent->client;
  auto &ps = cl.ps;

  // Determine base gunRate
  if ((g_quickWeaponSwitch->integer || g_frenzy->integer) &&
      gi.tickRate >= 20 &&
      (cl.weaponState == WeaponState::Activating ||
       cl.weaponState == WeaponState::Dropping)) {
    ps.gunRate = 20;
  } else {
    ps.gunRate = 10;
  }

  // Apply haste and modifiers if allowed
  if (ps.gunFrame != 0 && (!(cl.pers.weapon->flags & IF_NO_HASTE) ||
                           cl.weaponState != WeaponState::Firing)) {
    if (isHaste)
      ps.gunRate *= 1.5f;
    if (Tech_ApplyTimeAccel(ent))
      ps.gunRate *= 2.0f;
    if (g_frenzy->integer)
      ps.gunRate *= 2.0f;
  }

  // Optimization: encode default rate as 0 for networking
  if (ps.gunRate == 10) {
    ps.gunRate = 0;
    return 100_ms;
  }

  const float ms = (1.0f / ps.gunRate) * 1000.0f;
  return GameTime::from_ms(ms);
}

/*
=================
Think_Weapon

Called by ClientBeginServerFrame and ClientThink.
Handles weapon logic including death handling, animation timing,
and compensating for low tick-rate overflows.
=================
*/
void Think_Weapon(gentity_t *ent) {
  if (!ClientIsPlaying(ent->client) || ent->client->eliminated)
    return;

  // Put away weapon if dead
  if (ent->health < 1) {
    ent->client->weapon.pending = nullptr;
    Change_Weapon(ent);
  }

  // If no active weapon, try switching
  if (!ent->client->pers.weapon) {
    if (ent->client->weapon.pending)
      Change_Weapon(ent);
    return;
  }

  // Run the current weapon's think logic
  Weapon_RunThink(ent);

  // Compensate for missed animations due to fast tick rate (e.g. 33ms vs 50ms)
  if (33_ms < FRAME_TIME_MS) {
    const GameTime animTime = Weapon_AnimationTime(ent);

    if (animTime < FRAME_TIME_MS) {
      const GameTime nextFrameTime = level.time + FRAME_TIME_S;
      int64_t overrunMs =
          (nextFrameTime - ent->client->weapon.thinkTime).milliseconds();

      while (overrunMs > 0) {
        ent->client->weapon.thinkTime -= animTime;
        ent->client->weapon.fireFinished -= animTime;
        Weapon_RunThink(ent);
        overrunMs -= animTime.milliseconds();
      }
    }
  }
}

/*
================
Weapon_AttemptSwitch
================
*/
enum class WeaponSwitch : uint8_t {
  AlreadyUsing,
  NoWeapon,
  NoAmmo,
  NotEnoughAmmo,
  ValidSwitch
};

/*
================
Weapon_AttemptSwitch

Checks whether a weapon can be switched to, considering inventory and ammo.
================
*/
static WeaponSwitch Weapon_AttemptSwitch(gentity_t *ent, Item *item,
                                         bool silent) {
  if (!item)
    return WeaponSwitch::NoWeapon;

  if (ent->client->pers.weapon == item)
    return WeaponSwitch::AlreadyUsing;

  if (ent->client->pers.inventory[item->id] < 1)
    return WeaponSwitch::NoWeapon;

  const bool requiresAmmo =
      item->ammo && !g_select_empty->integer && !(item->flags & IF_AMMO);

  if (requiresAmmo) {
    Item *ammoItem = GetItemByIndex(item->ammo);
    const int ammoCount = ent->client->pers.inventory[item->ammo];

    if (ammoCount <= 0) {
      if (!silent && ammoItem)
        gi.LocClient_Print(ent, PRINT_HIGH, "$g_no_ammo", ammoItem->pickupName,
                           item->pickupNameDefinitive);
      return WeaponSwitch::NoAmmo;
    }

    if (ammoCount < item->quantity) {
      if (!silent && ammoItem)
        gi.LocClient_Print(ent, PRINT_HIGH, "$g_not_enough_ammo",
                           ammoItem->pickupName, item->pickupNameDefinitive);
      return WeaponSwitch::NotEnoughAmmo;
    }
  }

  return WeaponSwitch::ValidSwitch;
}

static inline bool Weapon_IsPartOfChain(Item *item, Item *other) {
  return other && other->chain && item->chain && other->chain == item->chain;
}

/*
================
Use_Weapon

Make the weapon ready if there is ammo
================
*/
void Use_Weapon(gentity_t *ent, Item *item) {
  if (!ent || !ent->client || !item)
    return;

  Item *wanted = nullptr;
  Item *root = nullptr;
  WeaponSwitch result = WeaponSwitch::NoWeapon;

  const bool noChains = ent->client->noWeaponChains;

  // Determine starting point in weapon chain
  if (!noChains && Weapon_IsPartOfChain(item, ent->client->weapon.pending)) {
    root = ent->client->weapon.pending;
    wanted = root->chainNext;
  } else if (!noChains &&
             Weapon_IsPartOfChain(item, ent->client->pers.weapon)) {
    root = ent->client->pers.weapon;
    wanted = root->chainNext;
  } else {
    root = item;
    wanted = item;
  }

  while (true) {
    result = Weapon_AttemptSwitch(ent, wanted, /*force=*/false);
    if (result == WeaponSwitch::ValidSwitch)
      break;

    if (noChains || !wanted || !wanted->chainNext)
      break;

    wanted = wanted->chainNext;
    if (wanted == root)
      break;
  }

  if (result == WeaponSwitch::ValidSwitch) {
    ent->client->weapon.pending = wanted;
  } else if ((result = Weapon_AttemptSwitch(ent, wanted, /*force=*/true)) ==
             WeaponSwitch::NoWeapon) {
    // Only print warning if it wasn't already the active or pending weapon
    if (wanted && wanted != ent->client->pers.weapon &&
        wanted != ent->client->weapon.pending)
      gi.LocClient_Print(ent, PRINT_HIGH, "$g_out_of_item", wanted->pickupName);
  }
}

/*
=========================
Weapon_PowerupSound
=========================
*/
void Weapon_PowerupSound(gentity_t *ent) {
  if (!ent || !ent->client)
    return;

  auto *cl = ent->client;

  // Try power amp first
  if (Tech_ApplyPowerAmpSound(ent))
    return;

  const bool quad = cl->PowerupTimer(PowerupTimer::QuadDamage) > level.time;
  const bool ddamage =
      cl->PowerupTimer(PowerupTimer::DoubleDamage) > level.time;
  const bool haste = cl->PowerupTimer(PowerupTimer::Haste) > level.time;
  const bool canHaste = cl->tech.soundTime < level.time;

  const char *sound = nullptr;

  if (quad && ddamage)
    sound = "ctf/tech2x.wav";
  else if (quad)
    sound = "items/damage3.wav";
  else if (ddamage)
    sound = "misc/ddamage3.wav";
  else if (haste && canHaste) {
    cl->tech.soundTime = level.time + 1_sec;
    sound = "ctf/tech3.wav";
  }

  if (sound)
    gi.sound(ent, CHAN_ITEM, gi.soundIndex(sound), 1, ATTN_NORM, 0);

  Tech_ApplyTimeAccelSound(ent);
}

/*
================
Weapon_CanAnimate
================
*/
static inline bool Weapon_CanAnimate(gentity_t *ent) {
  // VWep animations screw up corpses
  return !ent->deadFlag && ent->s.modelIndex == MODELINDEX_PLAYER;
}

/*
================
Weapon_SetFinished

[Paril-KEX] called when finished to set time until
we're allowed to switch to fire again
================
*/
static inline void Weapon_SetFinished(gentity_t *ent) {
  ent->client->weapon.fireFinished = level.time + Weapon_AnimationTime(ent);
}

/*
=============
Weapon_ForceIdle

Forces an active weapon to stop firing and return to an idle-ready state.
=============
*/
void Weapon_ForceIdle(gentity_t *ent) {
  if (!ent || !ent->client)
    return;

  gclient_t *cl = ent->client;

  cl->latchedButtons &= ~BUTTON_ATTACK;
  cl->buttons &= ~BUTTON_ATTACK;
  cl->weapon.fireBuffered = false;
  cl->weapon.thunk = false;
  if (cl->weapon.thinkTime > level.time)
    cl->weapon.thinkTime = level.time;
  if (cl->weapon.fireFinished > level.time)
    cl->weapon.fireFinished = level.time;

  if (cl->weaponState != WeaponState::Ready)
    cl->weaponState = WeaponState::Ready;

  if (cl->weaponSound)
    cl->weaponSound = 0;

  if (cl->ps.gunFrame < 0)
    cl->ps.gunFrame = 0;

  Weapon_Grapple_DoReset(cl);

  if (cl->grenadeTime) {
    cl->grenadeTime = 0_ms;
    cl->grenadeFinishedTime = level.time;
    cl->grenadeBlewUp = false;
  }
}

/*
===========================
Weapon_HandleDropping
===========================
*/
static inline bool Weapon_HandleDropping(gentity_t *ent,
                                         int FRAME_DEACTIVATE_LAST) {
  if (!ent || !ent->client)
    return false;

  auto *cl = ent->client;

  if (cl->weaponState != WeaponState::Dropping)
    return false;

  if (cl->weapon.thinkTime > level.time)
    return true;

  if (cl->ps.gunFrame == FRAME_DEACTIVATE_LAST) {
    Change_Weapon(ent);
    return true;
  }

  // Trigger reversed pain animation for short deactivate sequences
  if ((FRAME_DEACTIVATE_LAST - cl->ps.gunFrame) == 4) {
    cl->anim.priority = ANIM_ATTACK | ANIM_REVERSED;

    if (cl->ps.pmove.pmFlags & PMF_DUCKED) {
      ent->s.frame = FRAME_crpain4 + 1;
      cl->anim.end = FRAME_crpain1;
    } else {
      ent->s.frame = FRAME_pain304 + 1;
      cl->anim.end = FRAME_pain301;
    }

    cl->anim.time = 0_ms;
  }

  cl->ps.gunFrame++;
  cl->weapon.thinkTime = level.time + Weapon_AnimationTime(ent);

  return true;
}

/*
===========================
Weapon_HandleActivating
===========================
*/
static inline bool Weapon_HandleActivating(gentity_t *ent,
                                           int FRAME_ACTIVATE_LAST,
                                           int FRAME_IDLE_FIRST) {
  if (!ent || !ent->client)
    return false;

  auto *cl = ent->client;

  if (cl->weaponState != WeaponState::Activating)
    return false;

  const bool instantSwitch =
      g_instantWeaponSwitch->integer || g_frenzy->integer;

  if (cl->weapon.thinkTime > level.time && !instantSwitch)
    return false;

  cl->weapon.thinkTime = level.time + Weapon_AnimationTime(ent);

  if (cl->ps.gunFrame == FRAME_ACTIVATE_LAST || instantSwitch) {
    cl->weaponState = WeaponState::Ready;
    cl->ps.gunFrame = FRAME_IDLE_FIRST;
    cl->weapon.fireBuffered = false;

    if (!g_instantWeaponSwitch->integer || g_frenzy->integer)
      Weapon_SetFinished(ent);
    else
      cl->weapon.fireFinished = 0_ms;

    return true;
  }

  cl->ps.gunFrame++;
  return true;
}

/*
============================
Weapon_HandleNewWeapon
============================
*/
static inline bool Weapon_HandleNewWeapon(gentity_t *ent,
                                          int FRAME_DEACTIVATE_FIRST,
                                          int FRAME_DEACTIVATE_LAST) {
  if (!ent || !ent->client)
    return false;

  auto *cl = ent->client;
  bool isHolstering = false;

  // Determine holster intent
  if (!g_instantWeaponSwitch->integer || g_frenzy->integer) {
    isHolstering = (cl->latchedButtons | cl->buttons) & BUTTON_HOLSTER;
  }

  // Only allow weapon switch if not firing
  const bool wantsNewWeapon = (cl->weapon.pending || isHolstering);
  if (!wantsNewWeapon || cl->weaponState == WeaponState::Firing)
    return false;

  // Proceed if switch delay expired or instant switching enabled
  if (g_instantWeaponSwitch->integer || g_frenzy->integer ||
      cl->weapon.thinkTime <= level.time) {
    if (!cl->weapon.pending)
      cl->weapon.pending = cl->pers.weapon;

    cl->weaponState = WeaponState::Dropping;

    // Instant switch: no animation
    if (g_instantWeaponSwitch->integer || g_frenzy->integer) {
      Change_Weapon(ent);
      return true;
    }

    cl->ps.gunFrame = FRAME_DEACTIVATE_FIRST;

    // If short deactivation animation, play reversed pain animation
    if ((FRAME_DEACTIVATE_LAST - FRAME_DEACTIVATE_FIRST) < 4) {
      cl->anim.priority = ANIM_ATTACK | ANIM_REVERSED;

      if (cl->ps.pmove.pmFlags & PMF_DUCKED) {
        ent->s.frame = FRAME_crpain4 + 1;
        cl->anim.end = FRAME_crpain1;
      } else {
        ent->s.frame = FRAME_pain304 + 1;
        cl->anim.end = FRAME_pain301;
      }
      cl->anim.time = 0_ms;
    }

    cl->weapon.thinkTime = level.time + Weapon_AnimationTime(ent);
    return true;
  }

  return false;
}

/*
===========================
Weapon_HandleReady
===========================
*/
enum class weaponReadyState { None, Changing, Firing };

static inline weaponReadyState
Weapon_HandleReady(gentity_t *ent, int FRAME_FIRE_FIRST, int FRAME_IDLE_FIRST,
                   int FRAME_IDLE_LAST, const int *pauseFrames) {
  if (!ent || !ent->client || ent->client->weaponState != WeaponState::Ready)
    return weaponReadyState::None;

  auto *cl = ent->client;

  // Determine if player is trying to fire
  bool requestFiring = false;
  if (CombatIsDisabled()) {
    cl->latchedButtons &= ~BUTTON_ATTACK;
  } else {
    requestFiring = cl->weapon.fireBuffered ||
                    ((cl->latchedButtons | cl->buttons) & BUTTON_ATTACK);
  }

  if (requestFiring && cl->weapon.fireFinished <= level.time) {
    cl->latchedButtons &= ~BUTTON_ATTACK;
    cl->weapon.thinkTime = level.time;

    // Has ammo or doesn't need it
    const int ammoIndex = cl->pers.weapon->ammo;
    const bool hasAmmo = (ammoIndex == 0) || (cl->pers.inventory[ammoIndex] >=
                                              cl->pers.weapon->quantity);

    if (hasAmmo) {
      cl->weaponState = WeaponState::Firing;
      cl->lastFiringTime = level.time + COOP_DAMAGE_FIRING_TIME;
      return weaponReadyState::Firing;
    } else {
      NoAmmoWeaponChange(ent, true);
      return weaponReadyState::Changing;
    }
  }

  // Advance idle frames
  if (cl->weapon.thinkTime <= level.time) {
    cl->weapon.thinkTime = level.time + Weapon_AnimationTime(ent);

    if (cl->ps.gunFrame == FRAME_IDLE_LAST) {
      cl->ps.gunFrame = FRAME_IDLE_FIRST;
      return weaponReadyState::Changing;
    }

    // Pause frames
    if (pauseFrames) {
      for (int i = 0; pauseFrames[i]; ++i) {
        if (cl->ps.gunFrame == pauseFrames[i] && irandom(16))
          return weaponReadyState::Changing;
      }
    }

    cl->ps.gunFrame++;
    return weaponReadyState::Changing;
  }

  return weaponReadyState::None;
}

/*
=========================
Weapon_HandleFiring
=========================
*/
static inline void
Weapon_HandleFiring(gentity_t *ent, int FRAME_IDLE_FIRST,
                    const std::function<void()> &fireHandler) {
  if (!ent || !ent->client)
    return;

  auto *cl = ent->client;

  Weapon_SetFinished(ent);

  // Consume buffered fire input
  if (cl->weapon.fireBuffered) {
    cl->buttons |= BUTTON_ATTACK;
    cl->weapon.fireBuffered = false;

    auto &spawnProtection = cl->PowerupTimer(PowerupTimer::SpawnProtection);
    if (spawnProtection > level.time)
      spawnProtection = 0_ms;
  }

  // Execute weapon firing behavior
  fireHandler();

  // If frame reached idle, transition state
  if (cl->ps.gunFrame == FRAME_IDLE_FIRST) {
    cl->weaponState = WeaponState::Ready;
    cl->weapon.fireBuffered = false;
  }

  cl->weapon.thinkTime = level.time + Weapon_AnimationTime(ent);
}

/*
=======================
Weapon_Generic
=======================
*/
void Weapon_Generic(gentity_t *ent, int FRAME_ACTIVATE_LAST,
                    int FRAME_FIRE_LAST, int FRAME_IDLE_LAST,
                    int FRAME_DEACTIVATE_LAST, const int *pauseFrames,
                    const int *fireFrames, void (*fire)(gentity_t *ent)) {
  if (!ent || !ent->client)
    return;

  auto *cl = ent->client;

  const int FRAME_FIRE_FIRST = FRAME_ACTIVATE_LAST + 1;
  const int FRAME_IDLE_FIRST = FRAME_FIRE_LAST + 1;
  const int FRAME_DEACTIVATE_FIRST = FRAME_IDLE_LAST + 1;

  if (!Weapon_CanAnimate(ent))
    return;

  if (Weapon_HandleDropping(ent, FRAME_DEACTIVATE_LAST))
    return;

  if (Weapon_HandleActivating(ent, FRAME_ACTIVATE_LAST, FRAME_IDLE_FIRST))
    return;

  if (Weapon_HandleNewWeapon(ent, FRAME_DEACTIVATE_FIRST,
                             FRAME_DEACTIVATE_LAST))
    return;

  const auto readyState = Weapon_HandleReady(
      ent, FRAME_FIRE_FIRST, FRAME_IDLE_FIRST, FRAME_IDLE_LAST, pauseFrames);

  if (readyState == weaponReadyState::Firing) {
    cl->ps.gunFrame = FRAME_FIRE_FIRST;
    cl->weapon.fireBuffered = false;

    if (cl->weapon.thunk)
      cl->weapon.thinkTime += FRAME_TIME_S;

    cl->weapon.thinkTime += Weapon_AnimationTime(ent);
    Weapon_SetFinished(ent);

    // Play attack animation
    cl->anim.priority = ANIM_ATTACK;
    if (cl->ps.pmove.pmFlags & PMF_DUCKED) {
      ent->s.frame = FRAME_crattak1 - 1;
      cl->anim.end = FRAME_crattak9;
    } else {
      ent->s.frame = FRAME_attack1 - 1;
      cl->anim.end = FRAME_attack8;
    }
    cl->anim.time = 0_ms;

    for (int i = 0; fireFrames[i]; ++i) {
      if (cl->ps.gunFrame == fireFrames[i]) {
        Weapon_PowerupSound(ent);
        fire(ent);
        break;
      }
    }

    return;
  }

  // Handle held firing state
  if (cl->weaponState == WeaponState::Firing &&
      cl->weapon.thinkTime <= level.time) {
    cl->lastFiringTime = level.time + COOP_DAMAGE_FIRING_TIME;
    cl->ps.gunFrame++;

    Weapon_HandleFiring(ent, FRAME_IDLE_FIRST, [&]() {
      for (int i = 0; fireFrames[i]; ++i) {
        if (cl->ps.gunFrame == fireFrames[i]) {
          Weapon_PowerupSound(ent);
          fire(ent);
          break;
        }
      }
    });
  }
}

/*
=======================
Weapon_Repeating
=======================
*/
void Weapon_Repeating(gentity_t *ent, int FRAME_ACTIVATE_LAST,
                      int FRAME_FIRE_LAST, int FRAME_IDLE_LAST,
                      int FRAME_DEACTIVATE_LAST, const int *pauseFrames,
                      void (*fire)(gentity_t *ent)) {
  if (!ent || !ent->client)
    return;

  const int FRAME_FIRE_FIRST = FRAME_ACTIVATE_LAST + 1;
  const int FRAME_IDLE_FIRST = FRAME_FIRE_LAST + 1;
  const int FRAME_DEACTIVATE_FIRST = FRAME_IDLE_LAST + 1;

  if (!Weapon_CanAnimate(ent))
    return;

  if (Weapon_HandleDropping(ent, FRAME_DEACTIVATE_LAST))
    return;

  if (Weapon_HandleActivating(ent, FRAME_ACTIVATE_LAST, FRAME_IDLE_FIRST))
    return;

  if (Weapon_HandleNewWeapon(ent, FRAME_DEACTIVATE_FIRST,
                             FRAME_DEACTIVATE_LAST))
    return;

  if (Weapon_HandleReady(ent, FRAME_FIRE_FIRST, FRAME_IDLE_FIRST,
                         FRAME_IDLE_LAST,
                         pauseFrames) == weaponReadyState::Changing)
    return;

  // Handle firing state
  if (ent->client->weaponState == WeaponState::Firing &&
      ent->client->weapon.thinkTime <= level.time) {
    ent->client->lastFiringTime = level.time + COOP_DAMAGE_FIRING_TIME;

    Weapon_HandleFiring(ent, FRAME_IDLE_FIRST, [&]() { fire(ent); });

    if (ent->client->weapon.thunk)
      ent->client->weapon.thinkTime += FRAME_TIME_S;
  }
}

/*
======================================================================

HAND GRENADES

======================================================================
*/

static void Weapon_HandGrenade_Fire(gentity_t *ent, bool held) {
  if (!ent || !ent->client)
    return;

  int damage = 125;
  float radius = static_cast<float>(damage + 40);

  if (isQuad)
    damage *= damageMultiplier;

  // Clamp vertical angle to prevent backward throws
  Vector3 clampedAngles = {std::max(-62.5f, ent->client->vAngle[PITCH]),
                           ent->client->vAngle[YAW], ent->client->vAngle[ROLL]};

  Vector3 start{}, dir{};
  P_ProjectSource(ent, clampedAngles, {2, 0, -14}, start, dir);

  // Determine grenade throw speed based on hold duration or death fallback
  GameTime timer = ent->client->grenadeTime - level.time;
  const float holdSeconds = GRENADE_TIMER.seconds();
  int speed;

  if (ent->health <= 0) {
    speed = GRENADE_MINSPEED;
  } else {
    const float heldTime = (GRENADE_TIMER - timer).seconds();
    const float maxDelta = (GRENADE_MAXSPEED - GRENADE_MINSPEED) / holdSeconds;
    speed = static_cast<int>(std::min(GRENADE_MINSPEED + heldTime * maxDelta,
                                      static_cast<float>(GRENADE_MAXSPEED)));
  }

  ent->client->grenadeTime = 0_ms;

  fire_handgrenade(ent, start, dir, damage, speed, timer, radius, held);

  ent->client->pers.match.totalShots++;
  ent->client->pers.match
      .totalShotsPerWeapon[static_cast<uint8_t>(Weapon::HandGrenades)]++;
  RemoveAmmo(ent, 1);
}

/*
===================
Throw_Generic
===================
*/
void Throw_Generic(gentity_t *ent, int FRAME_FIRE_LAST, int FRAME_IDLE_LAST,
                   int FRAME_PRIME_SOUND, const char *prime_sound,
                   int FRAME_THROW_HOLD, int FRAME_THROW_FIRE,
                   const int *pauseFrames, int EXPLODE,
                   const char *primed_sound,
                   void (*fire)(gentity_t *ent, bool held),
                   bool extra_idle_frame, item_id_t ammoOverride) {
  if (!ent || !ent->client)
    return;

  auto *cl = ent->client;
  const item_id_t ammoItem =
      (ammoOverride != IT_TOTAL) ? ammoOverride : cl->pers.weapon->ammo;
  const int FRAME_IDLE_FIRST = FRAME_FIRE_LAST + 1;

  // On death: toss held grenade
  if (ent->health <= 0) {
    fire(ent, true);
    return;
  }

  // Weapon change queued
  if (cl->weapon.pending && cl->weaponState == WeaponState::Ready) {
    if (cl->weapon.thinkTime <= level.time) {
      Change_Weapon(ent);
      cl->weapon.thinkTime = level.time + Weapon_AnimationTime(ent);
    }
    return;
  }

  // Weapon is activating
  if (cl->weaponState == WeaponState::Activating) {
    if (cl->weapon.thinkTime <= level.time) {
      cl->weaponState = WeaponState::Ready;
      cl->ps.gunFrame =
          extra_idle_frame ? FRAME_IDLE_LAST + 1 : FRAME_IDLE_FIRST;
      cl->weapon.thinkTime = level.time + Weapon_AnimationTime(ent);
      Weapon_SetFinished(ent);
    }
    return;
  }

  // Weapon ready: listen for throw intent
  if (cl->weaponState == WeaponState::Ready) {
    bool request_firing = false;

    if (CombatIsDisabled()) {
      cl->latchedButtons &= ~BUTTON_ATTACK;
    } else {
      request_firing = cl->weapon.fireBuffered ||
                       ((cl->latchedButtons | cl->buttons) & BUTTON_ATTACK);
    }

    if (request_firing && cl->weapon.fireFinished <= level.time) {
      cl->latchedButtons &= ~BUTTON_ATTACK;

      const bool hasAmmo =
          (ammoItem == IT_NULL) || cl->pers.inventory[ammoItem];

      if (hasAmmo) {
        cl->ps.gunFrame = 1;
        cl->weaponState = WeaponState::Firing;
        cl->grenadeTime = 0_ms;
        cl->weapon.thinkTime = level.time + Weapon_AnimationTime(ent);
      } else {
        NoAmmoWeaponChange(ent, true);
      }
      return;
    }

    // Idle animation progression
    if (cl->weapon.thinkTime <= level.time) {
      cl->weapon.thinkTime = level.time + Weapon_AnimationTime(ent);

      if (cl->ps.gunFrame >= FRAME_IDLE_LAST) {
        cl->ps.gunFrame = FRAME_IDLE_FIRST;
        return;
      }

      if (pauseFrames) {
        for (int i = 0; pauseFrames[i]; ++i) {
          if (cl->ps.gunFrame == pauseFrames[i] && irandom(16))
            return;
        }
      }

      cl->ps.gunFrame++;
    }
    return;
  }

  // Weapon is firing
  if (cl->weaponState == WeaponState::Firing &&
      cl->weapon.thinkTime <= level.time) {
    cl->lastFiringTime = level.time + COOP_DAMAGE_FIRING_TIME;

    if (prime_sound && cl->ps.gunFrame == FRAME_PRIME_SOUND) {
      gi.sound(ent, CHAN_WEAPON, gi.soundIndex(prime_sound), 1, ATTN_NORM, 0);
    }

    // Adjust fuse delay for time effects
    GameTime fuseWait = 1_sec;
    if (Tech_ApplyTimeAccel(ent) || isHaste || g_frenzy->integer)
      fuseWait *= 0.5f;

    // Primed and held state
    if (cl->ps.gunFrame == FRAME_THROW_HOLD) {
      if (!cl->grenadeTime && !cl->grenadeFinishedTime)
        cl->grenadeTime = level.time + GRENADE_TIMER + 200_ms;

      if (primed_sound && !cl->grenadeBlewUp)
        cl->weaponSound = gi.soundIndex(primed_sound);

      // Detonate in hand
      if (EXPLODE && !cl->grenadeBlewUp && level.time >= cl->grenadeTime) {
        Weapon_PowerupSound(ent);
        cl->weaponSound = 0;
        fire(ent, true);
        cl->grenadeBlewUp = true;
        cl->grenadeFinishedTime = level.time + fuseWait;
      }

      // Still holding the button
      if (cl->buttons & BUTTON_ATTACK) {
        cl->weapon.thinkTime = level.time + 1_ms;
        return;
      }

      if (cl->grenadeBlewUp) {
        if (level.time >= cl->grenadeFinishedTime) {
          cl->ps.gunFrame = FRAME_FIRE_LAST;
          cl->grenadeBlewUp = false;
          cl->weapon.thinkTime = level.time + Weapon_AnimationTime(ent);
        }
        return;
      }

      // Normal throw
      cl->ps.gunFrame++;
      Weapon_PowerupSound(ent);
      cl->weaponSound = 0;
      fire(ent, false);

      if (!EXPLODE || !cl->grenadeBlewUp)
        cl->grenadeFinishedTime = level.time + fuseWait;

      // Play throw animation
      if (!ent->deadFlag && ent->s.modelIndex == MODELINDEX_PLAYER &&
          ent->health > 0) {
        if (cl->ps.pmove.pmFlags & PMF_DUCKED) {
          cl->anim.priority = ANIM_ATTACK;
          ent->s.frame = FRAME_crattak1 - 1;
          cl->anim.end = FRAME_crattak3;
        } else {
          cl->anim.priority = ANIM_ATTACK | ANIM_REVERSED;
          ent->s.frame = FRAME_wave08;
          cl->anim.end = FRAME_wave01;
        }
        cl->anim.time = 0_ms;
      }
    }

    cl->weapon.thinkTime = level.time + Weapon_AnimationTime(ent);

    // Delay if not ready to return to idle
    if (cl->ps.gunFrame == FRAME_FIRE_LAST &&
        level.time < cl->grenadeFinishedTime)
      return;

    cl->ps.gunFrame++;

    // Return to idle
    if (cl->ps.gunFrame == FRAME_IDLE_FIRST) {
      cl->grenadeFinishedTime = 0_ms;
      cl->weaponState = WeaponState::Ready;
      cl->weapon.fireBuffered = false;
      Weapon_SetFinished(ent);

      if (extra_idle_frame)
        cl->ps.gunFrame = FRAME_IDLE_LAST + 1;

      // Out of grenades: auto-switch
      if (ammoItem != IT_NULL && !cl->pers.inventory[ammoItem]) {
        NoAmmoWeaponChange(ent, false);
        Change_Weapon(ent);
      }
    }
  }
}

void Weapon_HandGrenade(gentity_t *ent) {
  constexpr int pauseFrames[] = {29, 34, 39, 48, 0};

  Throw_Generic(ent, 15, 48, 5, "weapons/hgrena1b.wav", 11, 12, pauseFrames,
                true, "weapons/hgrenc1b.wav", Weapon_HandGrenade_Fire, true);

  // [Paril-KEX] skip the duped frame
  if (ent->client->ps.gunFrame == 1)
    ent->client->ps.gunFrame = 2;
}

/*
======================================================================

GRENADE LAUNCHER

======================================================================
*/

static void Weapon_GrenadeLauncher_Fire(gentity_t *ent) {
  if (!ent || !ent->client)
    return;

  int damage;
  float splashRadius;
  int speed;

  if (RS(Quake3Arena)) {
    damage = 100;
    splashRadius = 150.0f;
    speed = 700;
  } else {
    damage = 120;
    splashRadius = static_cast<float>(damage + 40.0f);
    speed = 600;
  }

  if (isQuad)
    damage *= damageMultiplier;

  // Clamp upward angle to avoid backward fire
  Vector3 clampedAngles = {std::max(-62.5f, ent->client->vAngle[PITCH]),
                           ent->client->vAngle[YAW], ent->client->vAngle[ROLL]};

  Vector3 start{}, dir{};
  P_ProjectSource(ent, clampedAngles, {8, 0, -8}, start, dir);

  // Weapon kick
  Vector3 kickOrigin{};
  for (int i = 0; i < 3; ++i)
    kickOrigin[i] = ent->client->vForward[i] * -2.0f;

  Vector3 kickAngles = {-1.0f, 0.0f, 0.0f};
  P_AddWeaponKick(ent, kickOrigin, kickAngles);

  // Fire grenade
  const float bounce = crandom_open() * 10.0f;
  const float fuseVel = 200.0f + crandom_open() * 10.0f;

  fire_grenade(ent, start, dir, damage, speed, 2.5_sec, splashRadius, bounce,
               fuseVel, false);

  // Muzzle flash
  gi.WriteByte(svc_muzzleflash);
  gi.WriteEntity(ent);
  gi.WriteByte(MZ_GRENADE | isSilenced);
  gi.multicast(ent->s.origin, MULTICAST_PVS, false);

  G_PlayerNoise(ent, start, PlayerNoise::Weapon);

  ent->client->pers.match.totalShots++;
  ent->client->pers.match
      .totalShotsPerWeapon[static_cast<uint8_t>(Weapon::GrenadeLauncher)]++;
  RemoveAmmo(ent, 1);
}

void Weapon_GrenadeLauncher(gentity_t *ent) {
  constexpr int pauseFrames[] = {34, 51, 59, 0};
  constexpr int fireFrames[] = {6, 0};

  Weapon_Generic(ent, 5, 16, 59, 64, pauseFrames, fireFrames,
                 Weapon_GrenadeLauncher_Fire);
}

/*
======================================================================

ROCKET LAUNCHER

======================================================================
*/

static void Weapon_RocketLauncher_Fire(gentity_t *ent) {
  if (!ent || !ent->client)
    return;

  constexpr int baseDamage = 100;
  constexpr int baseSplashRadius = 100;

  int damage = baseDamage;
  int splashDamage = baseDamage;
  int splashRadius = baseSplashRadius;
  int speed = 800;

  switch (game.ruleset) {
  case Ruleset::Quake1:
    speed = 1000;
    break;
  case Ruleset::Quake3Arena:
    speed = 900;
    break;
  default:
    speed = 800;
    break;
  }

  if (g_frenzy->integer)
    speed = static_cast<int>(speed * 1.5f);

  if (isQuad) {
    damage *= damageMultiplier;
    splashDamage *= damageMultiplier;
  }

  Vector3 start{}, dir{};
  P_ProjectSource(ent, ent->client->vAngle, {8, 8, -8}, start, dir);
  fire_rocket(ent, start, dir, damage, speed, splashRadius, splashDamage);

  Vector3 kickOrigin{};
  for (int i = 0; i < 3; ++i)
    kickOrigin[i] = ent->client->vForward[i] * -2.0f;

  Vector3 kickAngles = {-1.0f, 0.0f, 0.0f};
  P_AddWeaponKick(ent, kickOrigin, kickAngles);

  // Muzzle flash
  gi.WriteByte(svc_muzzleflash);
  gi.WriteEntity(ent);
  gi.WriteByte(MZ_ROCKET | isSilenced);
  gi.multicast(ent->s.origin, MULTICAST_PVS, false);

  G_PlayerNoise(ent, start, PlayerNoise::Weapon);

  ent->client->pers.match.totalShots++;
  ent->client->pers.match
      .totalShotsPerWeapon[static_cast<uint8_t>(Weapon::RocketLauncher)]++;
  RemoveAmmo(ent, 1);
}

void Weapon_RocketLauncher(gentity_t *ent) {
  constexpr int pauseFrames[] = {25, 33, 42, 50, 0};
  constexpr int fireFrames[] = {5, 0};

  Weapon_Generic(ent, 4, 12, 50, 54, pauseFrames, fireFrames,
                 Weapon_RocketLauncher_Fire);
}

/*
======================================================================

GRAPPLE

======================================================================
*/

// self is grapple, not player
static void Weapon_Grapple_Reset(gentity_t *self) {
  if (!self || !self->owner->client || !self->owner->client->grapple.entity)
    return;

  const float volume =
      self->owner->client->PowerupCount(PowerupCount::SilencerShots) ? 0.2f
                                                                     : 1.0f;
  gi.sound(self->owner, CHAN_WEAPON,
           gi.soundIndex("weapons/grapple/grreset.wav"), volume, ATTN_NORM, 0);

  gclient_t *cl;
  cl = self->owner->client;
  cl->grapple.entity = nullptr;
  cl->grapple.releaseTime = level.time + 1_sec;
  cl->grapple.state = GrappleState::Fly; // we're firing, not on hook
  self->owner->flags &= ~FL_NO_KNOCKBACK;
  FreeEntity(self);
}

void Weapon_Grapple_DoReset(gclient_t *cl) {
  if (cl && cl->grapple.entity)
    Weapon_Grapple_Reset(cl->grapple.entity);
}

static TOUCH(Weapon_Grapple_Touch)(gentity_t *self, gentity_t *other,
                                   const trace_t &tr, bool otherTouchingSelf)
    ->void {
  float volume = 1.0;

  if (other == self->owner)
    return;

  if (self->owner->client->grapple.state != GrappleState::Fly)
    return;

  if (tr.surface && (tr.surface->flags & SURF_SKY)) {
    Weapon_Grapple_Reset(self);
    return;
  }

  self->velocity = {};

  G_PlayerNoise(self->owner, self->s.origin, PlayerNoise::Impact);

  if (other->takeDamage) {
    if (self->dmg)
      Damage(other, self, self->owner, self->velocity, self->s.origin,
             tr.plane.normal, self->dmg, 1,
             DamageFlags::Normal | DamageFlags::StatOnce, ModID::GrapplingHook);
    Weapon_Grapple_Reset(self);
    return;
  }

  self->owner->client->grapple.state = GrappleState::Pull; // we're on hook
  self->enemy = other;

  self->solid = SOLID_NOT;

  if (self->owner->client->PowerupCount(PowerupCount::SilencerShots))
    volume = 0.2f;

  gi.sound(self, CHAN_WEAPON, gi.soundIndex("weapons/grapple/grhit.wav"),
           volume, ATTN_NORM, 0);
  self->s.sound = gi.soundIndex("weapons/grapple/grpull.wav");

  gi.WriteByte(svc_temp_entity);
  gi.WriteByte(TE_SPARKS);
  gi.WritePosition(self->s.origin);
  gi.WriteDir(tr.plane.normal);
  gi.multicast(self->s.origin, MULTICAST_PVS, false);
}

// draw beam between grapple and self
static void Weapon_Grapple_DrawCable(gentity_t *self) {
  if (self->owner->client->grapple.state == GrappleState::Hang)
    return;

  Vector3 start, dir;
  P_ProjectSource(self->owner, self->owner->client->vAngle, {7, 2, -9}, start,
                  dir);

  gi.WriteByte(svc_temp_entity);
  gi.WriteByte(TE_GRAPPLE_CABLE_2);
  gi.WriteEntity(self->owner);
  gi.WritePosition(start);
  gi.WritePosition(self->s.origin);
  gi.multicast(self->s.origin, MULTICAST_PVS, false);
}

// pull the player toward the grapple
void Weapon_Grapple_Pull(gentity_t *self) {
  Vector3 hookdir, v;
  float vlen;

  if (self->owner->client->pers.weapon &&
      self->owner->client->pers.weapon->id == IT_WEAPON_GRAPPLE &&
      !(self->owner->client->weapon.pending ||
        ((self->owner->client->latchedButtons | self->owner->client->buttons) &
         BUTTON_HOLSTER)) &&
      self->owner->client->weaponState != WeaponState::Firing &&
      self->owner->client->weaponState != WeaponState::Activating) {
    if (!self->owner->client->weapon.pending)
      self->owner->client->weapon.pending = self->owner->client->pers.weapon;

    Weapon_Grapple_Reset(self);
    return;
  }

  if (self->enemy) {
    if (self->enemy->solid == SOLID_NOT) {
      Weapon_Grapple_Reset(self);
      return;
    }
    if (self->enemy->solid == SOLID_BBOX) {
      v = self->enemy->size * 0.5f;
      v += self->enemy->s.origin;
      self->s.origin = v + self->enemy->mins;
      gi.linkEntity(self);
    } else
      self->velocity = self->enemy->velocity;

    if (self->enemy->deadFlag) { // he died
      Weapon_Grapple_Reset(self);
      return;
    }
  }

  Weapon_Grapple_DrawCable(self);

  if (self->owner->client->grapple.state > GrappleState::Fly) {
    // pull player toward grapple
    Vector3 forward, up;

    AngleVectors(self->owner->client->vAngle, forward, nullptr, up);
    v = self->owner->s.origin;
    v[2] += self->owner->viewHeight;
    hookdir = self->s.origin - v;

    vlen = hookdir.length();

    if (self->owner->client->grapple.state == GrappleState::Pull && vlen < 64) {
      self->owner->client->grapple.state = GrappleState::Hang;
      self->s.sound = gi.soundIndex("weapons/grapple/grhang.wav");
    }

    hookdir.normalize();
    hookdir = hookdir * g_grapple_pull_speed->value;
    self->owner->velocity = hookdir;
    self->owner->flags |= FL_NO_KNOCKBACK;
    G_AddGravity(self->owner);
  }
}

static DIE(Weapon_Grapple_Die)(gentity_t *self, gentity_t *other,
                               gentity_t *inflictor, int damage,
                               const Vector3 &point, const MeansOfDeath &mod)
    ->void {
  if (mod.id == ModID::Crushed)
    Weapon_Grapple_Reset(self);
}

static bool Weapon_Grapple_FireHook(gentity_t *self, const Vector3 &start,
                                    const Vector3 &dir, int damage, int speed,
                                    Effect effect) {
  gentity_t *grapple;
  trace_t tr;
  Vector3 normalized = dir.normalized();

  grapple = Spawn();
  grapple->s.origin = start;
  grapple->s.oldOrigin = start;
  grapple->s.angles = VectorToAngles(normalized);
  grapple->velocity = normalized * speed;
  grapple->moveType = MoveType::FlyMissile;
  grapple->clipMask = MASK_PROJECTILE;
  // [Paril-KEX]
  if (self->client && !G_ShouldPlayersCollide(true))
    grapple->clipMask &= ~CONTENTS_PLAYER;
  grapple->solid = SOLID_BBOX;
  grapple->s.effects |= effect;
  grapple->s.modelIndex = gi.modelIndex("models/weapons/grapple/hook/tris.md2");
  grapple->owner = self;
  grapple->touch = Weapon_Grapple_Touch;
  grapple->dmg = damage;
  grapple->flags |= FL_NO_KNOCKBACK | FL_NO_DAMAGE_EFFECTS;
  grapple->takeDamage = true;
  grapple->die = Weapon_Grapple_Die;
  if (self->client) {
    self->client->grapple.entity = grapple;
    self->client->grapple.state =
        GrappleState::Fly; // we're firing, not on hook
  }
  gi.linkEntity(grapple);

  tr = gi.traceLine(self->s.origin, grapple->s.origin, grapple,
                    grapple->clipMask);
  if (tr.fraction < 1.0f) {
    grapple->s.origin = tr.endPos + (tr.plane.normal * 1.f);
    grapple->touch(grapple, tr.ent, tr, false);
    return false;
  }

  grapple->s.sound = gi.soundIndex("weapons/grapple/grfly.wav");

  return true;
}

static void Weapon_Grapple_DoFire(gentity_t *ent, const Vector3 &g_offset,
                                  int damage, Effect effect) {
  float volume = 1.0;

  if (ent->client->grapple.state > GrappleState::Fly)
    return; // it's already out

  Vector3 start, dir;
  P_ProjectSource(ent, ent->client->vAngle, Vector3{24, 8, -8 + 2} + g_offset,
                  start, dir);

  if (ent->client->PowerupCount(PowerupCount::SilencerShots))
    volume = 0.2f;

  if (Weapon_Grapple_FireHook(ent, start, dir, damage,
                              g_grapple_fly_speed->value, effect))
    gi.sound(ent, CHAN_WEAPON, gi.soundIndex("weapons/grapple/grfire.wav"),
             volume, ATTN_NORM, 0);

  G_PlayerNoise(ent, start, PlayerNoise::Weapon);
}

static void Weapon_Grapple_Fire(gentity_t *ent) {
  Weapon_Grapple_DoFire(ent, vec3_origin, g_grapple_damage->integer, EF_NONE);
}

void Weapon_Grapple(gentity_t *ent) {
  constexpr int pauseFrames[] = {10, 18, 27, 0};
  constexpr int fireFrames[] = {6, 0};
  WeaponState prevState;

  // if the the attack button is still down, stay in the firing frame
  if ((ent->client->buttons & (BUTTON_ATTACK | BUTTON_HOLSTER)) &&
      ent->client->weaponState == WeaponState::Firing &&
      ent->client->grapple.entity)
    ent->client->ps.gunFrame = 6;

  if (!(ent->client->buttons & (BUTTON_ATTACK | BUTTON_HOLSTER)) &&
      ent->client->grapple.entity) {
    Weapon_Grapple_Reset(ent->client->grapple.entity);
    if (ent->client->weaponState == WeaponState::Firing)
      ent->client->weaponState = WeaponState::Ready;
  }

  if ((ent->client->weapon.pending ||
       ((ent->client->latchedButtons | ent->client->buttons) &
        BUTTON_HOLSTER)) &&
      ent->client->grapple.state > GrappleState::Fly &&
      ent->client->weaponState == WeaponState::Firing) {
    // he wants to change weapons while grappled
    if (!ent->client->weapon.pending)
      ent->client->weapon.pending = ent->client->pers.weapon;
    ent->client->weaponState = WeaponState::Dropping;
    ent->client->ps.gunFrame = 32;
  }

  prevState = ent->client->weaponState;
  Weapon_Generic(ent, 5, 10, 31, 36, pauseFrames, fireFrames,
                 Weapon_Grapple_Fire);

  // if the the attack button is still down, stay in the firing frame
  if ((ent->client->buttons & (BUTTON_ATTACK | BUTTON_HOLSTER)) &&
      ent->client->weaponState == WeaponState::Firing &&
      ent->client->grapple.entity)
    ent->client->ps.gunFrame = 6;

  // if we just switched back to grapple, immediately go to fire frame
  if (prevState == WeaponState::Activating &&
      ent->client->weaponState == WeaponState::Ready &&
      ent->client->grapple.state > GrappleState::Fly) {
    if (!(ent->client->buttons & (BUTTON_ATTACK | BUTTON_HOLSTER)))
      ent->client->ps.gunFrame = 6;
    else
      ent->client->ps.gunFrame = 5;
    ent->client->weaponState = WeaponState::Firing;
  }
}

/*
======================================================================

OFF-HAND HOOK

======================================================================
*/

static void Weapon_Hook_DoFire(gentity_t *ent, const Vector3 &g_offset,
                               int damage, Effect effect) {
  if (ent->client->grapple.state > GrappleState::Fly)
    return; // it's already out

  Vector3 start, dir;
  P_ProjectSource(ent, ent->client->vAngle, Vector3{24, 0, 0} + g_offset, start,
                  dir);

  if (Weapon_Grapple_FireHook(ent, start, dir, damage,
                              g_grapple_fly_speed->value, effect)) {
    const float volume =
        ent->client->PowerupCount(PowerupCount::SilencerShots) ? 0.2f : 1.0f;
    gi.sound(ent, CHAN_WEAPON, gi.soundIndex("weapons/grapple/grfire.wav"),
             volume, ATTN_NORM, 0);
  }

  G_PlayerNoise(ent, start, PlayerNoise::Weapon);
}

void Weapon_Hook(gentity_t *ent) {
  Weapon_Hook_DoFire(ent, vec3_origin, g_grapple_damage->integer, EF_NONE);
}

/*
======================================================================

BLASTER / HYPERBLASTER

======================================================================
*/

static void Weapon_Blaster_Fire(gentity_t *ent, const Vector3 &g_offset,
                                int damage, bool hyper, Effect effect) {
  if (!ent || !ent->client)
    return;

  if (isQuad)
    damage *= damageMultiplier;

  // Calculate final offset from muzzle
  Vector3 offset = {24.0f + g_offset[0], 8.0f + g_offset[1],
                    -8.0f + g_offset[2]};

  Vector3 start{}, dir{};
  P_ProjectSource(ent, ent->client->vAngle, offset, start, dir);

  // Kick origin
  Vector3 kickOrigin{};
  for (int i = 0; i < 3; ++i)
    kickOrigin[i] = ent->client->vForward[i] * -2.0f;

  // Kick angles
  Vector3 kickAngles{};
  if (hyper) {
    for (int i = 0; i < 3; ++i)
      kickAngles[i] = crandom() * 0.7f;
  } else {
    kickAngles[PITCH] = -1.0f;
    kickAngles[YAW] = 0.0f;
    kickAngles[ROLL] = 0.0f;
  }

  P_AddWeaponKick(ent, kickOrigin, kickAngles);

  // Determine projectile speed
  int speed = 0;
  if (RS(Quake3Arena)) {
    speed = hyper ? 2000 : 2500;
  } else {
    speed = hyper ? 1000 : 1500;
  }

  fire_blaster(ent, start, dir, damage, speed, effect,
               hyper ? ModID::HyperBlaster : ModID::Blaster, false);

  // Muzzle flash
  gi.WriteByte(svc_muzzleflash);
  gi.WriteEntity(ent);
  gi.WriteByte((hyper ? MZ_HYPERBLASTER : MZ_BLASTER) | isSilenced);
  gi.multicast(ent->s.origin, MULTICAST_PVS, false);

  G_PlayerNoise(ent, start, PlayerNoise::Weapon);

  ent->client->pers.match.totalShots++;
  ent->client->pers.match
      .totalShotsPerWeapon[static_cast<uint8_t>(Weapon::Blaster)]++;
}

static void Weapon_Blaster_DoFire(gentity_t *ent) {
  int damage = 15;
  Weapon_Blaster_Fire(ent, vec3_origin, damage, false, EF_BLASTER);
}

void Weapon_Blaster(gentity_t *ent) {
  constexpr int pauseFrames[] = {19, 32, 0};
  constexpr int fireFrames[] = {5, 0};

  Weapon_Generic(ent, 4, 8, 52, 55, pauseFrames, fireFrames,
                 Weapon_Blaster_DoFire);
}

/*
=============
Weapon_HyperBlaster_Fire

Handles the firing logic for the HyperBlaster, including animation, ammo checks,
and shot execution.
=============
*/
static void Weapon_HyperBlaster_Fire(gentity_t *ent) {
  if (!ent || !ent->client)
    return;

  gclient_t *client = ent->client;
  int damage;
  float rotation;

  // Advance or reset gunFrame
  if (client->ps.gunFrame > 20) {
    client->ps.gunFrame = 6;
  } else {
    client->ps.gunFrame++;
  }

  // Loop logic or wind-down sound
  if (client->ps.gunFrame == 12) {
    if ((client->pers.inventory[client->pers.weapon->ammo] > 0) &&
        (client->buttons & BUTTON_ATTACK)) {
      client->ps.gunFrame = 6;
    } else {
      gi.sound(ent, CHAN_AUTO, gi.soundIndex("weapons/hyprbd1a.wav"), 1,
               ATTN_NORM, 0);
    }
  }

  // Weapon sound during firing loop
  if (client->ps.gunFrame >= 6 && client->ps.gunFrame <= 11) {
    client->weaponSound = gi.soundIndex("weapons/hyprbl1a.wav");
  } else {
    client->weaponSound = 0;
  }

  // Firing logic
  const bool isFiring =
      client->weapon.fireBuffered || (client->buttons & BUTTON_ATTACK);

  if (isFiring && client->ps.gunFrame >= 6 && client->ps.gunFrame <= 11) {
    client->weapon.fireBuffered = false;

    if (client->pers.inventory[client->pers.weapon->ammo] < 1) {
      NoAmmoWeaponChange(ent, true);
      return;
    }

    // Calculate rotating offset
    Vector3 offset{};
    rotation = (client->ps.gunFrame - 5) * 2.0f * PIf / 6.0f;
    offset[0] = -4.0f * std::sin(rotation);
    offset[1] = 4.0f * std::cos(rotation);
    offset[2] = 0.0f;

    // Set damage based on ruleset
    if (RS(Quake3Arena)) {
      damage = deathmatch->integer ? 20 : 25;
    } else {
      damage = deathmatch->integer ? 15 : 20;
    }

    const Effect effect =
        (client->ps.gunFrame % 4 == 0) ? EF_HYPERBLASTER : EF_NONE;

    Weapon_Blaster_Fire(ent, offset, damage, true, effect);
    Weapon_PowerupSound(ent);

    client->pers.match.totalShots++;
    client->pers.match
        .totalShotsPerWeapon[static_cast<uint8_t>(Weapon::HyperBlaster)]++;
    RemoveAmmo(ent, 1);

    // Play attack animation
    client->anim.priority = ANIM_ATTACK;
    if (client->ps.pmove.pmFlags & PMF_DUCKED) {
      ent->s.frame = FRAME_crattak1 - (int)(frandom() + 0.25f);
      client->anim.end = FRAME_crattak9;
    } else {
      ent->s.frame = FRAME_attack1 - (int)(frandom() + 0.25f);
      client->anim.end = FRAME_attack8;
    }
    client->anim.time = 0_ms;
  }
}

void Weapon_HyperBlaster(gentity_t *ent) {
  constexpr int pauseFrames[] = {0};

  Weapon_Repeating(ent, 5, 20, 49, 53, pauseFrames, Weapon_HyperBlaster_Fire);
}

/*
======================================================================

MACHINEGUN / CHAINGUN

======================================================================
*/

static void Weapon_Machinegun_Fire(gentity_t *ent) {
  if (!ent || !ent->client)
    return;

  auto &client = *ent->client;
  auto &ps = client.ps;

  int damage = 8;
  int kick = 2;
  int hSpread = DEFAULT_BULLET_HSPREAD;
  int vSpread = DEFAULT_BULLET_VSPREAD;

  if (RS(Quake3Arena)) {
    damage =
        (Game::Is(GameType::TeamDeathmatch) || Game::Is(GameType::Domination))
            ? 5
            : 7;
    hSpread = 200;
    vSpread = 200;
  }

  if (!(client.buttons & BUTTON_ATTACK)) {
    ps.gunFrame = 6;
    return;
  }

  ps.gunFrame = (ps.gunFrame == 4) ? 5 : 4;

  int &ammo = client.pers.inventory[client.pers.weapon->ammo];
  if (ammo < 1) {
    ps.gunFrame = 6;
    NoAmmoWeaponChange(ent, true);
    return;
  }

  if (isQuad) {
    damage *= damageMultiplier;
    kick *= damageMultiplier;
  }

  Vector3 kickOrigin{crandom() * 0.35f, crandom() * 0.35f, crandom() * 0.35f};

  Vector3 kickAngles{crandom() * 0.7f, crandom() * 0.7f, crandom() * 0.7f};

  P_AddWeaponKick(ent, kickOrigin, kickAngles);

  Vector3 start, dir;
  P_ProjectSource(ent, client.vAngle, {0, 0, -8}, start, dir);

  LagCompensate(ent, start, dir);
  fire_bullet(ent, start, dir, damage, kick, hSpread, vSpread,
              ModID::Machinegun);
  UnLagCompensate();

  Weapon_PowerupSound(ent);

  // Muzzle flash
  gi.WriteByte(svc_muzzleflash);
  gi.WriteEntity(ent);
  gi.WriteByte(MZ_MACHINEGUN | isSilenced);
  gi.multicast(ent->s.origin, MULTICAST_PVS, false);

  G_PlayerNoise(ent, start, PlayerNoise::Weapon);

  client.pers.match.totalShots++;
  client.pers.match
      .totalShotsPerWeapon[static_cast<uint8_t>(Weapon::Machinegun)]++;
  RemoveAmmo(ent, 1);

  // Attack animation
  client.anim.priority = ANIM_ATTACK;
  if (ps.pmove.pmFlags & PMF_DUCKED) {
    ent->s.frame = FRAME_crattak1 - static_cast<int>(frandom() + 0.25f);
    client.anim.end = FRAME_crattak9;
  } else {
    ent->s.frame = FRAME_attack1 - static_cast<int>(frandom() + 0.25f);
    client.anim.end = FRAME_attack8;
  }
  client.anim.time = 0_ms;
}

void Weapon_Machinegun(gentity_t *ent) {
  constexpr int pauseFrames[] = {23, 45, 0};

  Weapon_Repeating(ent, 3, 5, 45, 49, pauseFrames, Weapon_Machinegun_Fire);
}

static void Weapon_Chaingun_Fire(gentity_t *ent) {
  if (!ent || !ent->client)
    return;

  auto &client = *ent->client;
  auto &ps = client.ps;
  const int damageBase = deathmatch->integer ? 6 : 8;
  int damage = damageBase;
  int kick = 2;

  // Handle gunFrame animation
  if (ps.gunFrame > 31) {
    ps.gunFrame = 5;
    gi.sound(ent, CHAN_AUTO, gi.soundIndex("weapons/chngnu1a.wav"), 1,
             ATTN_IDLE, 0);
  } else if (ps.gunFrame == 14 && !(client.buttons & BUTTON_ATTACK)) {
    ps.gunFrame = 32;
    client.weaponSound = 0;
    return;
  } else if (ps.gunFrame == 21 && (client.buttons & BUTTON_ATTACK) &&
             client.pers.inventory[client.pers.weapon->ammo]) {
    ps.gunFrame = 15;
  } else {
    ps.gunFrame++;
  }

  if (ps.gunFrame == 22) {
    client.weaponSound = 0;
    gi.sound(ent, CHAN_AUTO, gi.soundIndex("weapons/chngnd1a.wav"), 1,
             ATTN_IDLE, 0);
  }

  if (ps.gunFrame < 5 || ps.gunFrame > 21)
    return;

  client.weaponSound = gi.soundIndex("weapons/chngnl1a.wav");

  // Set animation
  client.anim.priority = ANIM_ATTACK;
  if (ps.pmove.pmFlags & PMF_DUCKED) {
    ent->s.frame = FRAME_crattak1 - (ps.gunFrame & 1);
    client.anim.end = FRAME_crattak9;
  } else {
    ent->s.frame = FRAME_attack1 - (ps.gunFrame & 1);
    client.anim.end = FRAME_attack8;
  }
  client.anim.time = 0_ms;

  // Determine number of shots
  int shots = 0;
  if (ps.gunFrame <= 9) {
    shots = 1;
  } else if (ps.gunFrame <= 14) {
    shots = (client.buttons & BUTTON_ATTACK) ? 2 : 1;
  } else {
    shots = 3;
  }

  int &ammo = client.pers.inventory[client.pers.weapon->ammo];
  if (ammo < shots)
    shots = ammo;

  if (shots == 0) {
    NoAmmoWeaponChange(ent, true);
    return;
  }

  if (isQuad) {
    damage *= damageMultiplier;
    kick *= damageMultiplier;
  }

  // Apply weapon kick
  Vector3 kickOrigin{crandom() * 0.35f, crandom() * 0.35f, crandom() * 0.35f};

  Vector3 kickAngles{crandom() * (0.5f + shots * 0.15f),
                     crandom() * (0.5f + shots * 0.15f),
                     crandom() * (0.5f + shots * 0.15f)};

  P_AddWeaponKick(ent, kickOrigin, kickAngles);

  Vector3 start, dir;
  P_ProjectSource(ent, client.vAngle, {0, 0, -8}, start, dir);

  LagCompensate(ent, start, dir);

  for (int i = 0; i < shots; ++i) {
    // Recalculate for each shot
    P_ProjectSource(ent, client.vAngle, {0, 0, -8}, start, dir);

    fire_bullet(ent, start, dir, damage, kick, DEFAULT_BULLET_HSPREAD,
                DEFAULT_BULLET_VSPREAD, ModID::Chaingun);
  }

  UnLagCompensate();

  Weapon_PowerupSound(ent);

  // Muzzle flash
  gi.WriteByte(svc_muzzleflash);
  gi.WriteEntity(ent);
  gi.WriteByte((MZ_CHAINGUN1 + shots - 1) | isSilenced);
  gi.multicast(ent->s.origin, MULTICAST_PVS, false);

  G_PlayerNoise(ent, start, PlayerNoise::Weapon);

  client.pers.match.totalShots += shots;
  client.pers.match
      .totalShotsPerWeapon[static_cast<uint8_t>(Weapon::Chaingun)] += shots;

  RemoveAmmo(ent, shots);
}

void Weapon_Chaingun(gentity_t *ent) {
  constexpr int pauseFrames[] = {38, 43, 51, 61, 0};

  Weapon_Repeating(ent, 4, 31, 61, 64, pauseFrames, Weapon_Chaingun_Fire);
}

/*
======================================================================

SHOTGUN / SUPERSHOTGUN

======================================================================
*/

/*
=============
Weapon_Shotgun_Fire

Fires the shotgun, applying damage, spread, and animations for the player.
=============
*/
static void Weapon_Shotgun_Fire(gentity_t *ent) {
  // Calculate damage and kick
  int damage = RS(Quake3Arena) ? 10 : 4;
  int kick = 4;

  if (isQuad) {
    damage *= damageMultiplier;
    kick *= damageMultiplier;
  }

  const int pelletCount = RS(Quake3Arena) ? 11 : 12;

  // Setup source and direction
  const Vector3 viewOffset = {0.f, 0.f, -8.f};
  Vector3 start, dir;
  P_ProjectSource(ent, ent->client->vAngle, viewOffset, start, dir);

  // Apply weapon kickback
  P_AddWeaponKick(ent, ent->client->vForward * -2.f, {-2.f, 0.f, 0.f});

  // Fire with lag compensation
  LagCompensate(ent, start, dir);
  fire_shotgun(ent, start, dir, damage, kick, 500, 500, pelletCount,
               ModID::Shotgun);
  UnLagCompensate();

  // Muzzle flash
  gi.WriteByte(svc_muzzleflash);
  gi.WriteEntity(ent);
  gi.WriteByte(MZ_SHOTGUN | isSilenced);
  gi.multicast(ent->s.origin, MULTICAST_PVS, false);

  // Weapon noise and stats
  G_PlayerNoise(ent, start, PlayerNoise::Weapon);

  ent->client->pers.match.totalShots += pelletCount;
  ent->client->pers.match
      .totalShotsPerWeapon[static_cast<uint8_t>(Weapon::Shotgun)] +=
      pelletCount;
  RemoveAmmo(ent, 1);
}

void Weapon_Shotgun(gentity_t *ent) {
  constexpr int pauseFrames[] = {22, 28, 34, 0};
  constexpr int fireFrames[] = {8, 0};

  Weapon_Generic(ent, 7, 18, 36, 39, pauseFrames, fireFrames,
                 Weapon_Shotgun_Fire);
}

/*
===========================
Weapon_SuperShotgun_Fire
===========================
*/
static void Weapon_SuperShotgun_Fire(gentity_t *ent) {
  int damage = 6;
  int kick = 6;

  if (isQuad) {
    damage *= damageMultiplier;
    kick *= damageMultiplier;
  }

  // Prepare direction and starting positions
  const Vector3 viewOffset = {0.f, 0.f, -8.f};
  Vector3 start, dir;

  // Central shot uses original angle
  P_ProjectSource(ent, ent->client->vAngle, viewOffset, start, dir);
  LagCompensate(ent, start, dir);

  // First barrel shot (slightly left)
  Vector3 leftAngle = {ent->client->vAngle[PITCH],
                       ent->client->vAngle[YAW] - 5.f,
                       ent->client->vAngle[ROLL]};
  P_ProjectSource(ent, leftAngle, viewOffset, start, dir);
  fire_shotgun(ent, start, dir, damage, kick, DEFAULT_SHOTGUN_HSPREAD,
               DEFAULT_SHOTGUN_VSPREAD, DEFAULT_SSHOTGUN_COUNT / 2,
               ModID::SuperShotgun);

  // Second barrel shot (slightly right)
  Vector3 rightAngle = {ent->client->vAngle[PITCH],
                        ent->client->vAngle[YAW] + 5.f,
                        ent->client->vAngle[ROLL]};
  P_ProjectSource(ent, rightAngle, viewOffset, start, dir);
  fire_shotgun(ent, start, dir, damage, kick, DEFAULT_SHOTGUN_HSPREAD,
               DEFAULT_SHOTGUN_VSPREAD, DEFAULT_SSHOTGUN_COUNT / 2,
               ModID::SuperShotgun);

  UnLagCompensate();

  // Add recoil
  P_AddWeaponKick(ent, ent->client->vForward * -2.f, {-2.f, 0.f, 0.f});

  // Visual and sound effects
  gi.WriteByte(svc_muzzleflash);
  gi.WriteEntity(ent);
  gi.WriteByte(MZ_SSHOTGUN | isSilenced);
  gi.multicast(ent->s.origin, MULTICAST_PVS, false);

  G_PlayerNoise(ent, start, PlayerNoise::Weapon);

  // Stats and ammo
  ent->client->pers.match.totalShots += DEFAULT_SSHOTGUN_COUNT;
  ent->client->pers.match
      .totalShotsPerWeapon[static_cast<uint8_t>(Weapon::SuperShotgun)] +=
      DEFAULT_SSHOTGUN_COUNT;
  RemoveAmmo(ent, 2);
}

void Weapon_SuperShotgun(gentity_t *ent) {
  constexpr int pauseFrames[] = {29, 42, 57, 0};
  constexpr int fireFrames[] = {7, 0};

  Weapon_Generic(ent, 6, 17, 57, 61, pauseFrames, fireFrames,
                 Weapon_SuperShotgun_Fire);
}

/*
======================================================================

RAILGUN

======================================================================
*/

static void Weapon_Railgun_Fire(gentity_t *ent) {
  int damage = deathmatch->integer ? 80 : 150;
  int kick = damage;

  if (isQuad) {
    damage *= damageMultiplier;
    kick *= damageMultiplier;
  }

  Vector3 start, dir;
  P_ProjectSource(ent, ent->client->vAngle, {0.f, 7.f, -8.f}, start, dir);

  LagCompensate(ent, start, dir);
  fire_rail(ent, start, dir, damage, kick);
  UnLagCompensate();

  P_AddWeaponKick(ent, ent->client->vForward * -3.f, {-3.f, 0.f, 0.f});

  // Muzzle flash effect
  gi.WriteByte(svc_muzzleflash);
  gi.WriteEntity(ent);
  gi.WriteByte(MZ_RAILGUN | isSilenced);
  gi.multicast(ent->s.origin, MULTICAST_PVS, false);

  G_PlayerNoise(ent, start, PlayerNoise::Weapon);

  // Stats and ammo tracking
  ent->client->pers.match.totalShots++;
  ent->client->pers.match
      .totalShotsPerWeapon[static_cast<uint8_t>(Weapon::Railgun)]++;
  RemoveAmmo(ent, 1);
}

void Weapon_Railgun(gentity_t *ent) {
  constexpr int pauseFrames[] = {56, 0};
  constexpr int fireFrames[] = {4, 0};

  Weapon_Generic(ent, 3, 18, 56, 61, pauseFrames, fireFrames,
                 Weapon_Railgun_Fire);
}

/*
======================================================================

BFG10K

======================================================================
*/

static void Weapon_BFG_Fire(gentity_t *ent) {
  const bool q3 = RS(Quake3Arena);
  int damage = q3 ? 100 : (deathmatch->integer ? 200 : 500);
  int speed = q3 ? 1000 : 400;
  float radius = q3 ? 120.0f : 1000.0f;
  int ammoNeeded = q3 ? 10 : 50;

  // Show muzzle flash on windup frame only
  if (ent->client->ps.gunFrame == 9) {
    gi.WriteByte(svc_muzzleflash);
    gi.WriteEntity(ent);
    gi.WriteByte(MZ_BFG | isSilenced);
    gi.multicast(ent->s.origin, MULTICAST_PVS, false);
    G_PlayerNoise(ent, ent->s.origin, PlayerNoise::Weapon);
    return;
  }

  // Abort if not enough ammo (could have been drained during windup)
  if (ent->client->pers.inventory[ent->client->pers.weapon->ammo] < ammoNeeded)
    return;

  if (isQuad)
    damage *= damageMultiplier;

  Vector3 start, dir;
  P_ProjectSource(ent, ent->client->vAngle, {8.f, 8.f, -8.f}, start, dir);
  fire_bfg(ent, start, dir, damage, speed, radius);

  // Apply kickback
  if (q3) {
    P_AddWeaponKick(ent, ent->client->vForward * -2.f, {-1.f, 0.f, 0.f});
  } else {
    P_AddWeaponKick(ent, ent->client->vForward * -2.f,
                    {-20.f, 0.f, crandom() * 8.f});
    ent->client->kick.total = DAMAGE_TIME();
    ent->client->kick.time = level.time + ent->client->kick.total;
  }

  // Fire flash
  gi.WriteByte(svc_muzzleflash);
  gi.WriteEntity(ent);
  gi.WriteByte(MZ_BFG2 | isSilenced);
  gi.multicast(ent->s.origin, MULTICAST_PVS, false);

  G_PlayerNoise(ent, start, PlayerNoise::Weapon);

  // Stats and ammo
  ent->client->pers.match.totalShots++;
  ent->client->pers.match
      .totalShotsPerWeapon[static_cast<uint8_t>(Weapon::BFG10K)]++;
  RemoveAmmo(ent, ammoNeeded);
}

void Weapon_BFG(gentity_t *ent) {
  constexpr int pauseFrames[] = {39, 45, 50, 55, 0};
  constexpr int fireFrames[] = {9, 17, 0};
  constexpr int fireFramesQ3A[] = {15, 17, 0};

  Weapon_Generic(ent, 8, 32, 54, 58, pauseFrames,
                 RS(Quake3Arena) ? fireFramesQ3A : fireFrames, Weapon_BFG_Fire);
}

/*
======================================================================

PROX MINES

======================================================================
*/

static void Weapon_ProxLauncher_Fire(gentity_t *ent) {
  // Clamp pitch to avoid backward firing
  const Vector3 launchAngles = {std::max(-62.5f, ent->client->vAngle[PITCH]),
                                ent->client->vAngle[YAW],
                                ent->client->vAngle[ROLL]};

  Vector3 start, dir;
  P_ProjectSource(ent, launchAngles, {8.f, 8.f, -8.f}, start, dir);

  // Apply recoil
  P_AddWeaponKick(ent, ent->client->vForward * -2.f, {-1.f, 0.f, 0.f});

  // Fire prox mine
  fire_prox(ent, start, dir, damageMultiplier, 600);

  // Muzzle flash and sound
  gi.WriteByte(svc_muzzleflash);
  gi.WriteEntity(ent);
  gi.WriteByte(MZ_PROX | isSilenced);
  gi.multicast(ent->s.origin, MULTICAST_PVS, false);

  G_PlayerNoise(ent, start, PlayerNoise::Weapon);

  // Stats and ammo
  ent->client->pers.match.totalShots++;
  ent->client->pers.match
      .totalShotsPerWeapon[static_cast<uint8_t>(Weapon::ProxLauncher)]++;
  RemoveAmmo(ent, 1);
}

void Weapon_ProxLauncher(gentity_t *ent) {
  constexpr int pauseFrames[] = {34, 51, 59, 0};
  constexpr int fireFrames[] = {6, 0};

  Weapon_Generic(ent, 5, 16, 59, 64, pauseFrames, fireFrames,
                 Weapon_ProxLauncher_Fire);
}

/*
======================================================================

TESLA MINES

======================================================================
*/

static void Weapon_Tesla_Fire(gentity_t *ent, bool held) {
  // Determine firing direction with pitch limit
  Vector3 angles = {std::max(-62.5f, ent->client->vAngle[PITCH]),
                    ent->client->vAngle[YAW], ent->client->vAngle[ROLL]};

  Vector3 start, dir;
  P_ProjectSource(ent, angles, {0.f, 0.f, -22.f}, start, dir);

  // Calculate throw speed based on grenade hold time
  const GameTime timer = ent->client->grenadeTime - level.time;
  const float tSec = std::clamp(timer.seconds(), 0.f, GRENADE_TIMER.seconds());
  const float speed =
      (ent->health <= 0)
          ? GRENADE_MINSPEED
          : std::min(GRENADE_MINSPEED +
                         tSec * ((GRENADE_MAXSPEED - GRENADE_MINSPEED) /
                                 GRENADE_TIMER.seconds()),
                     GRENADE_MAXSPEED);

  ent->client->grenadeTime = 0_ms;

  // Fire tesla mine
  fire_tesla(ent, start, dir, damageMultiplier, static_cast<int>(speed));

  // Stats and ammo
  ent->client->pers.match.totalShots++;
  ent->client->pers.match
      .totalShotsPerWeapon[static_cast<uint8_t>(Weapon::TeslaMine)]++;
  RemoveAmmo(ent, 1);
}

void Weapon_Tesla(gentity_t *ent) {
  constexpr int pauseFrames[] = {21, 0};

  Throw_Generic(ent, 8, 32, -1, nullptr, 1, 2, pauseFrames, false, nullptr,
                Weapon_Tesla_Fire, false);
}

/*
======================================================================

CHAINFIST

======================================================================
*/

static void Weapon_Ball_Fire(gentity_t *ent, bool held) {
  if (!ent || !ent->client)
    return;

  if (!Game::Is(GameType::ProBall))
    return;

  if (held) {
    Vector3 dropOrigin =
        ent->s.origin +
        Vector3{0.f, 0.f, static_cast<float>(ent->viewHeight) * 0.4f};
    Ball_Drop(ent, dropOrigin);
    ent->client->grenadeTime = 0_ms;
    return;
  }

  Vector3 angles = {std::max(-62.5f, ent->client->vAngle[PITCH]),
                    ent->client->vAngle[YAW], ent->client->vAngle[ROLL]};

  Vector3 start{}, dir{};
  P_ProjectSource(ent, angles, {2.f, 0.f, -14.f}, start, dir);

  GameTime timer = ent->client->grenadeTime - level.time;
  ent->client->grenadeTime = 0_ms;

  const float holdSeconds = GRENADE_TIMER.seconds();
  const float heldSeconds =
      std::clamp((GRENADE_TIMER - timer).seconds(), 0.f, holdSeconds);
  const float speedStep = (GRENADE_MAXSPEED - GRENADE_MINSPEED) / holdSeconds;
  const float speed = ent->health <= 0
                          ? GRENADE_MINSPEED
                          : std::min(GRENADE_MINSPEED + heldSeconds * speedStep,
                                     GRENADE_MAXSPEED);

  Ball_Launch(ent, start, dir, speed);
}

static void Weapon_ChainFist_Fire(gentity_t *ent) {
  constexpr int32_t CHAINFIST_REACH = 24;

  // Stop attacking when fire is released on certain frames
  const int frame = ent->client->ps.gunFrame;
  if (!(ent->client->buttons & BUTTON_ATTACK)) {
    if (frame == 13 || frame == 23 || frame >= 32) {
      ent->client->ps.gunFrame = 33;
      return;
    }
  }

  // Determine damage
  int damage = deathmatch->integer ? 15 : 7;
  if (isQuad) {
    damage *= damageMultiplier;
  }

  Vector3 start{}, dir{};

  // Pro Ball: throwing the ball while chainfist is equipped
  if (Game::Is(GameType::ProBall) && Ball_PlayerHasBall(ent)) {
    constexpr int pauseFrames[] = {29, 34, 39, 48, 0};
    Throw_Generic(ent, 15, 48, 5, "weapons/hgrena1b.wav", 11, 12, pauseFrames,
                  false, nullptr, Weapon_Ball_Fire, true, IT_BALL);
    return;
  }

  // Fire melee strike
  P_ProjectSource(ent, ent->client->vAngle, {0, 0, -4}, start, dir);

  if (fire_player_melee(ent, start, dir, CHAINFIST_REACH, damage, 100,
                        ModID::Chainfist)) {
    if (ent->client->emptyClickSound < level.time) {
      ent->client->emptyClickSound = level.time + 500_ms;
      gi.sound(ent, CHAN_WEAPON, gi.soundIndex("weapons/sawslice.wav"), 1.f,
               ATTN_NORM, 0.f);
    }
  }

  G_PlayerNoise(ent, start, PlayerNoise::Weapon);

  // Advance animation frame
  ent->client->ps.gunFrame++;

  // Handle firing frame looping
  if (ent->client->buttons & BUTTON_ATTACK) {
    switch (ent->client->ps.gunFrame) {
    case 12:
      ent->client->ps.gunFrame = 14;
      break;
    case 22:
      ent->client->ps.gunFrame = 24;
      break;
    case 32:
      ent->client->ps.gunFrame = 7;
      break;
    }
  }

  // Start attack animation if needed
  if (ent->client->anim.priority != ANIM_ATTACK || frandom() < 0.25f) {
    ent->client->anim.priority = ANIM_ATTACK;
    if (ent->client->ps.pmove.pmFlags & PMF_DUCKED) {
      ent->s.frame = FRAME_crattak1 - 1;
      ent->client->anim.end = FRAME_crattak9;
    } else {
      ent->s.frame = FRAME_attack1 - 1;
      ent->client->anim.end = FRAME_attack8;
    }
    ent->client->anim.time = 0_ms;
  }
}

// this spits out some smoke from the motor. it's a two-stroke, you know.
static void Weapon_ChainFist_smoke(gentity_t *ent) {
  Vector3 tempVec, dir;
  P_ProjectSource(ent, ent->client->vAngle, {8, 8, -4}, tempVec, dir);

  gi.WriteByte(svc_temp_entity);
  gi.WriteByte(TE_CHAINFIST_SMOKE);
  gi.WritePosition(tempVec);
  gi.unicast(ent, 0);
}

void Weapon_ChainFist(gentity_t *ent) {
  constexpr int pauseFrames[] = {0};

  Weapon_Repeating(ent, 4, 32, 57, 60, pauseFrames, Weapon_ChainFist_Fire);

  // smoke on idle sequence
  if (ent->client->ps.gunFrame == 42 && irandom(8)) {
    if ((ent->client->pers.hand != Handedness::Center) && frandom() < 0.4f)
      Weapon_ChainFist_smoke(ent);
  } else if (ent->client->ps.gunFrame == 51 && irandom(8)) {
    if ((ent->client->pers.hand != Handedness::Center) && frandom() < 0.4f)
      Weapon_ChainFist_smoke(ent);
  }

  // set the appropriate weapon sound.
  if (ent->client->weaponState == WeaponState::Firing)
    ent->client->weaponSound = gi.soundIndex("weapons/sawhit.wav");
  else if (ent->client->weaponState == WeaponState::Dropping)
    ent->client->weaponSound = 0;
  else if (ent->client->pers.weapon->id == IT_WEAPON_CHAINFIST)
    ent->client->weaponSound = gi.soundIndex("weapons/sawidle.wav");
}

/*
======================================================================

DISRUPTOR

======================================================================
*/

/*
=============
Weapon_Disruptor_Fire

Fires the Disruptor projectile, handling lag compensation and collision checks.
=============
*/
static void Weapon_Disruptor_Fire(gentity_t *ent) {
  int damage = deathmatch->integer ? 45 : 135;
  if (isQuad) {
    damage *= damageMultiplier;
  }

  const Vector3 kMins = {-16.f, -16.f, -16.f};
  const Vector3 kMaxs = {16.f, 16.f, 16.f};
  const Vector3 kDistance = {24.f, 8.f, -8.f};

  Vector3 start, dir;
  P_ProjectSource(ent, ent->client->vAngle, kDistance, start, dir);

  Vector3 end = start + (dir * 8192.f);
  gentity_t *target = nullptr;
  contents_t mask = MASK_PROJECTILE;

  // Disable player collision if needed
  if (!G_ShouldPlayersCollide(true)) {
    mask &= ~CONTENTS_PLAYER;
  }

  // Lag compensation
  LagCompensate(ent, start, dir);
  trace_t tr = gi.traceLine(start, end, ent, mask);
  UnLagCompensate();

  // Attempt hit from point trace
  if (tr.ent != world && tr.ent->health > 0 &&
      ((tr.ent->svFlags & SVF_MONSTER) || tr.ent->client ||
       (tr.ent->flags & FL_DAMAGEABLE))) {
    target = tr.ent;
  } else {
    // Try expanded bounding box trace
    tr = gi.trace(start, kMins, kMaxs, end, ent, mask);
    if (tr.ent != world && tr.ent->health > 0 &&
        ((tr.ent->svFlags & SVF_MONSTER) || tr.ent->client ||
         (tr.ent->flags & FL_DAMAGEABLE))) {
      target = tr.ent;
    }
  }

  // Recoil
  P_AddWeaponKick(ent, ent->client->vForward * -2.f, {-1.f, 0.f, 0.f});

  // Fire weapon
  fire_disruptor(ent, start, dir, damage, 1000, target);

  // Muzzle flash
  gi.WriteByte(svc_muzzleflash);
  gi.WriteEntity(ent);
  gi.WriteByte(MZ_TRACKER | isSilenced);
  gi.multicast(ent->s.origin, MULTICAST_PVS, false);

  G_PlayerNoise(ent, start, PlayerNoise::Weapon);

  // Stats
  ent->client->pers.match.totalShots++;
  ent->client->pers.match
      .totalShotsPerWeapon[static_cast<uint8_t>(Weapon::Disruptor)]++;
  RemoveAmmo(ent, 1);
}

void Weapon_Disruptor(gentity_t *ent) {
  constexpr int pauseFrames[] = {14, 19, 23, 0};
  constexpr int fireFrames[] = {5, 0};

  Weapon_Generic(ent, 4, 9, 29, 34, pauseFrames, fireFrames,
                 Weapon_Disruptor_Fire);
}

/*
======================================================================

ETF RIFLE

======================================================================
*/

static void Weapon_ETF_Rifle_Fire(gentity_t *ent) {
  const int baseDamage = 10;
  const int baseKick = 3;

  if (!(ent->client->buttons & BUTTON_ATTACK)) {
    ent->client->ps.gunFrame = 8;
    return;
  }

  // Alternate muzzle flashes
  ent->client->ps.gunFrame = (ent->client->ps.gunFrame == 6) ? 7 : 6;

  // Ammo check
  if (ent->client->pers.inventory[ent->client->pers.weapon->ammo] <
      ent->client->pers.weapon->quantity) {
    ent->client->ps.gunFrame = 8;
    NoAmmoWeaponChange(ent, true);
    return;
  }

  // Damage + kick scaling
  int damage = baseDamage;
  int kick = baseKick;
  if (isQuad) {
    damage *= damageMultiplier;
    kick *= damageMultiplier;
  }

  // Weapon kick randomness
  Vector3 kickOrigin{}, kickAngles{};
  for (int i = 0; i < 3; ++i) {
    kickOrigin[i] = crandom() * 0.85f;
    kickAngles[i] = crandom() * 0.85f;
  }
  P_AddWeaponKick(ent, kickOrigin, kickAngles);

  // Firing position offset
  Vector3 offset = (ent->client->ps.gunFrame == 6) ? Vector3{15.f, 8.f, -8.f}
                                                   : Vector3{15.f, 6.f, -8.f};

  // Compute firing start and direction
  Vector3 start, dir;
  P_ProjectSource(ent, ent->client->vAngle + kickAngles, offset, start, dir);
  fire_flechette(ent, start, dir, damage, 1150, kick);

  Weapon_PowerupSound(ent);

  // Muzzle flash
  const int flashType =
      (ent->client->ps.gunFrame == 6) ? MZ_ETF_RIFLE : MZ_ETF_RIFLE_2;
  gi.WriteByte(svc_muzzleflash);
  gi.WriteEntity(ent);
  gi.WriteByte(flashType | isSilenced);
  gi.multicast(ent->s.origin, MULTICAST_PVS, false);

  G_PlayerNoise(ent, start, PlayerNoise::Weapon);

  // Stats tracking
  ent->client->pers.match.totalShots++;
  ent->client->pers.match
      .totalShotsPerWeapon[static_cast<uint8_t>(Weapon::ETFRifle)]++;
  RemoveAmmo(ent, 1);

  // Animation
  ent->client->anim.priority = ANIM_ATTACK;
  if (ent->client->ps.pmove.pmFlags & PMF_DUCKED) {
    ent->s.frame = FRAME_crattak1 - static_cast<int>(frandom() + 0.25f);
    ent->client->anim.end = FRAME_crattak9;
  } else {
    ent->s.frame = FRAME_attack1 - static_cast<int>(frandom() + 0.25f);
    ent->client->anim.end = FRAME_attack8;
  }
  ent->client->anim.time = 0_ms;
}

void Weapon_ETF_Rifle(gentity_t *ent) {
  constexpr int pauseFrames[] = {18, 28, 0};

  Weapon_Repeating(ent, 4, 7, 37, 41, pauseFrames, Weapon_ETF_Rifle_Fire);
}

/*
======================================================================

PLASMA GUN

======================================================================
*/

// v_plasmr.md2 has 52 frames (0..51): activate 0-8, fire 9-42, idle 43-49,
// deactivate 50-51.
constexpr int PLASMAGUN_FRAME_ACTIVATE_LAST = 8;
constexpr int PLASMAGUN_FRAME_FIRE_FIRST = 9;
constexpr int PLASMAGUN_FRAME_FIRE_LAST = 42;
constexpr int PLASMAGUN_FRAME_IDLE_FIRST = 43;
constexpr int PLASMAGUN_FRAME_IDLE_LAST = 49;
[[maybe_unused]] constexpr int PLASMAGUN_FRAME_DEACTIVATE_FIRST = 50;
constexpr int PLASMAGUN_FRAME_DEACTIVATE_LAST = 51;

static void Weapon_PlasmaGun_Fire(gentity_t *ent) {
  if (!ent || !ent->client)
    return;

  auto *cl = ent->client;
  const bool firing = (cl->buttons & BUTTON_ATTACK) && !CombatIsDisabled();
  const bool hasAmmo =
      cl->pers.inventory[cl->pers.weapon->ammo] >= cl->pers.weapon->quantity;

  if (!firing || !hasAmmo) {
    cl->ps.gunFrame = PLASMAGUN_FRAME_IDLE_FIRST;
    cl->weaponSound = 0;
    if (firing && !hasAmmo)
      NoAmmoWeaponChange(ent, true);
    return;
  }

  if (cl->ps.gunFrame < PLASMAGUN_FRAME_FIRE_FIRST ||
      cl->ps.gunFrame > PLASMAGUN_FRAME_FIRE_LAST) {
    cl->ps.gunFrame = PLASMAGUN_FRAME_FIRE_FIRST;
  } else {
    cl->ps.gunFrame++;
    if (cl->ps.gunFrame > PLASMAGUN_FRAME_FIRE_LAST)
      cl->ps.gunFrame = PLASMAGUN_FRAME_FIRE_FIRST;
  }

  int damage = 20;
  int splashDamage = 15;
  const float splashRadius = 20.f;
  int speed = 2000;

  if (isQuad) {
    damage *= damageMultiplier;
    splashDamage *= damageMultiplier;
  }

  Vector3 start{}, dir{};
  P_ProjectSource(ent, cl->vAngle, {24.f, 8.f, -8.f}, start, dir);

  fire_plasmagun(ent, start, dir, damage, speed, splashRadius, splashDamage);

  gi.sound(ent, CHAN_WEAPON, gi.soundIndex("weapons/plsmfire.wav"), 1,
           ATTN_NORM, 0);

  gi.WriteByte(svc_muzzleflash);
  gi.WriteEntity(ent);
  gi.WriteByte(MZ_HYPERBLASTER | isSilenced);
  gi.multicast(ent->s.origin, MULTICAST_PVS, false);

  G_PlayerNoise(ent, start, PlayerNoise::Weapon);
  Weapon_PowerupSound(ent);

  cl->pers.match.totalShots++;
  cl->pers.match.totalShotsPerWeapon[static_cast<uint8_t>(Weapon::PlasmaGun)]++;
  RemoveAmmo(ent, 1);

  cl->anim.priority = ANIM_ATTACK;
  if (cl->ps.pmove.pmFlags & PMF_DUCKED) {
    ent->s.frame = FRAME_crattak1 - static_cast<int>(frandom() + 0.25f);
    cl->anim.end = FRAME_crattak9;
  } else {
    ent->s.frame = FRAME_attack1 - static_cast<int>(frandom() + 0.25f);
    cl->anim.end = FRAME_attack8;
  }
  cl->anim.time = 0_ms;
}

void Weapon_PlasmaGun(gentity_t *ent) {
  constexpr int pauseFrames[] = {0};
  Weapon_Repeating(ent, PLASMAGUN_FRAME_ACTIVATE_LAST,
                   PLASMAGUN_FRAME_FIRE_LAST, PLASMAGUN_FRAME_IDLE_LAST,
                   PLASMAGUN_FRAME_DEACTIVATE_LAST, pauseFrames,
                   Weapon_PlasmaGun_Fire);
}

/*
======================================================================

PLASMA BEAM

======================================================================
*/

static void Weapon_PlasmaBeam_Fire(gentity_t *ent) {
  const bool firing =
      (ent->client->buttons & BUTTON_ATTACK) && !CombatIsDisabled();
  const bool hasAmmo =
      ent->client->pers.inventory[ent->client->pers.weapon->ammo] >=
      ent->client->pers.weapon->quantity;

  if (!firing || !hasAmmo) {
    ent->client->ps.gunFrame = 13;
    ent->client->weaponSound = 0;
    ent->client->ps.gunSkin = 0;

    // Only forcibly exit the fire loop if the player is truly out of ammo
    if (firing && !hasAmmo)
      NoAmmoWeaponChange(ent, true);
    return;
  }

  // Advance gunFrame
  if (ent->client->ps.gunFrame > 12)
    ent->client->ps.gunFrame = 8;
  else
    ent->client->ps.gunFrame++;

  if (ent->client->ps.gunFrame == 12)
    ent->client->ps.gunFrame = 8;

  // Set weapon sound and visual effects
  ent->client->weaponSound = gi.soundIndex("weapons/tesla.wav");
  ent->client->ps.gunSkin = 1;

  // Determine damage and kick
  int damage = 15;
  int kick = 15;

  switch (game.ruleset) {
  case Ruleset::Quake1:
    damage = 30;
    break;
  case Ruleset::Quake3Arena:
    damage = deathmatch->integer ? 8 : 15;
    break;
  default:
    damage = deathmatch->integer ? 8 : 15;
    break;
  }
  kick = damage;

  if (isQuad) {
    damage *= damageMultiplier;
    kick *= damageMultiplier;
  }

  ent->client->kick.time = 0_ms;

  // Fire origin and direction
  Vector3 start, dir;
  P_ProjectSource(ent, ent->client->vAngle, {7.f, 2.f, -3.f}, start, dir);

  // Lag compensation for accurate hits
  LagCompensate(ent, start, dir);
  fire_plasmabeam(ent, start, dir, {2.f, 7.f, -3.f}, damage, kick, false);
  UnLagCompensate();

  Weapon_PowerupSound(ent);

  // Muzzle flash
  gi.WriteByte(svc_muzzleflash);
  gi.WriteEntity(ent);
  gi.WriteByte(MZ_HEATBEAM | isSilenced);
  gi.multicast(ent->s.origin, MULTICAST_PVS, false);

  G_PlayerNoise(ent, start, PlayerNoise::Weapon);

  // Stats tracking
  ent->client->pers.match.totalShots++;
  ent->client->pers.match
      .totalShotsPerWeapon[static_cast<uint8_t>(Weapon::PlasmaBeam)]++;
  RemoveAmmo(ent, RS(Quake1) ? 2 : 1);

  // Animation
  ent->client->anim.priority = ANIM_ATTACK;
  if (ent->client->ps.pmove.pmFlags & PMF_DUCKED) {
    ent->s.frame = FRAME_crattak1 - static_cast<int>(frandom() + 0.25f);
    ent->client->anim.end = FRAME_crattak9;
  } else {
    ent->s.frame = FRAME_attack1 - static_cast<int>(frandom() + 0.25f);
    ent->client->anim.end = FRAME_attack8;
  }
  ent->client->anim.time = 0_ms;
}

void Weapon_PlasmaBeam(gentity_t *ent) {
  constexpr int pauseFrames[] = {35, 0};

  Weapon_Repeating(ent, 8, 12, 42, 47, pauseFrames, Weapon_PlasmaBeam_Fire);
}

/*
======================================================================

THUNDERBOLT

======================================================================
*/

// v_light.md2 has 5 frames (shot1..shot5); use a compact fire loop.
constexpr int TB_FRAME_ACTIVATE_LAST = 0;
constexpr int TB_FRAME_FIRE_LAST = 2;
constexpr int TB_FRAME_IDLE_LAST = 3;
constexpr int TB_FRAME_DEACTIVATE_LAST = 4;
constexpr int TB_FRAME_FIRE_FIRST = TB_FRAME_ACTIVATE_LAST + 1;
constexpr int TB_FRAME_IDLE_FIRST = TB_FRAME_FIRE_LAST + 1;

static void Weapon_Thunderbolt_Fire(gentity_t *ent) {
  const bool firing =
      (ent->client->buttons & BUTTON_ATTACK) && !CombatIsDisabled();
  const bool hasAmmo =
      ent->client->pers.inventory[ent->client->pers.weapon->ammo] >=
      ent->client->pers.weapon->quantity;

  if (!firing || !hasAmmo) {
    ent->client->weaponSound = 0;
    ent->client->ps.gunSkin = 0;
    ent->client->ps.gunFrame = TB_FRAME_IDLE_FIRST;
    ent->client->thunderbolt_sound_time = 0_ms;

    if (firing && !hasAmmo)
      NoAmmoWeaponChange(ent, true);
    return;
  }

  const bool startingFire = (ent->client->ps.gunFrame < TB_FRAME_FIRE_FIRST ||
                             ent->client->ps.gunFrame > TB_FRAME_FIRE_LAST);
  if (startingFire) {
    ent->client->ps.gunFrame = TB_FRAME_FIRE_FIRST;
  } else {
    ent->client->ps.gunFrame++;
    if (ent->client->ps.gunFrame > TB_FRAME_FIRE_LAST)
      ent->client->ps.gunFrame = TB_FRAME_FIRE_FIRST;
  }

  ent->client->weaponSound = 0;
  ent->client->ps.gunSkin = 1;

  // Determine damage and kick
  int damage = 15;
  int kick = 15;

  switch (game.ruleset) {
  case Ruleset::Quake3Arena:
    damage = deathmatch->integer ? 8 : 15;
    break;
  case Ruleset::Quake1:
    damage = 30;
    break;
  default:
    damage = deathmatch->integer ? 8 : 15;
    break;
  }
  kick = damage;

  if (isQuad) {
    damage *= damageMultiplier;
    kick *= damageMultiplier;
  }

  ent->client->kick.time = 0_ms;

  const Vector3 projectionOffset = {7.f, 2.f, -3.f};
  const Vector3 muzzleOffset = {2.f, 7.f, -3.f};
  Vector3 start, dir;
  P_ProjectSource(ent, ent->client->vAngle, projectionOffset, start, dir);

  LagCompensate(ent, start, dir);

  const bool discharged =
      fire_thunderbolt(ent, start, dir, muzzleOffset, damage, kick,
                       ModID::Thunderbolt, static_cast<int>(damageMultiplier));

  UnLagCompensate();

  if (!discharged) {
    if (startingFire) {
      gi.sound(ent, CHAN_WEAPON, gi.soundIndex("weapons/lstart.wav"), 1,
               ATTN_NORM, 0);
      ent->client->thunderbolt_sound_time = level.time + 600_ms;
    }

    if (level.time >= ent->client->thunderbolt_sound_time) {
      gi.sound(ent, CHAN_WEAPON, gi.soundIndex("weapons/lhit.wav"), 1,
               ATTN_NORM, 0);
      ent->client->thunderbolt_sound_time = level.time + 600_ms;
    }
  }

  Weapon_PowerupSound(ent);
#if 0
	gi.WriteByte(svc_muzzleflash);
	gi.WriteEntity(ent);
	gi.WriteByte(MZ_ETF_RIFLE | isSilenced);
	gi.multicast(ent->s.origin, MULTICAST_PVS, false);
#endif
  G_PlayerNoise(ent, start, PlayerNoise::Weapon);

  ent->client->pers.match.totalShots++;
  ent->client->pers.match
      .totalShotsPerWeapon[static_cast<uint8_t>(Weapon::Thunderbolt)]++;
  if (discharged) {
    ent->client->ps.gunFrame = TB_FRAME_IDLE_FIRST;
    ent->client->weaponSound = 0;
    ent->client->ps.gunSkin = 0;
    ent->client->thunderbolt_sound_time = 0_ms;
  } else {
    RemoveAmmo(ent, 1);
  }

  ent->client->anim.priority = ANIM_ATTACK;
  if (ent->client->ps.pmove.pmFlags & PMF_DUCKED) {
    ent->s.frame = FRAME_crattak1 - static_cast<int>(frandom() + 0.25f);
    ent->client->anim.end = FRAME_crattak9;
  } else {
    ent->s.frame = FRAME_attack1 - static_cast<int>(frandom() + 0.25f);
    ent->client->anim.end = FRAME_attack8;
  }
  ent->client->anim.time = 0_ms;
}

/*
==========================
Weapon_Thunderbolt
==========================
*/
void Weapon_Thunderbolt(gentity_t *ent) {
  constexpr int pauseFrames[] = {0};

  Weapon_Repeating(ent, TB_FRAME_ACTIVATE_LAST, TB_FRAME_FIRE_LAST,
                   TB_FRAME_IDLE_LAST, TB_FRAME_DEACTIVATE_LAST, pauseFrames,
                   Weapon_Thunderbolt_Fire);
}

/*
======================================================================

ION RIPPER

======================================================================
*/
/*
=========================
Weapon_IonRipper_Fire
=========================
*/
static void Weapon_IonRipper_Fire(gentity_t *ent) {
  constexpr int kProjectileCount = 15;
  constexpr int kDamage = 10; // 20;
  constexpr float kBaseSpeed = 555.f;
  constexpr float kRandomSpeed = 1800.f;
  constexpr int kHSpread = 500; // horizontal spread in fire_lead style
  constexpr int kVSpread = 500; // vertical spread in fire_lead style
  const Vector3 muzzleOffset = {16.f, 7.f, -8.f};
  constexpr Effect effectFlags = EF_IONRIPPER;
  constexpr int ammoNeeded = 10;

  if (!ent || !ent->client)
    return;

  if (ent->client->pers.inventory[ent->client->pers.weapon->ammo] < ammoNeeded)
    return;

  Vector3 forward, right, up;
  AngleVectors(ent->client->vAngle, forward, right, up);

  Vector3 start;
  P_ProjectSource(ent, ent->client->vAngle, muzzleOffset, start, forward);

  for (int i = 0; i < kProjectileCount; ++i) {
    // Compute bullet-style spread (same as fire_lead)
    float hOffset = crandom() * kHSpread;
    float vOffset = crandom() * kVSpread;

    Vector3 end = start + forward * 8192.f;
    end += right * hOffset;
    end += up * vOffset;

    Vector3 dir = end - start;
    dir.normalize();

    float speed = kBaseSpeed + crandom() * kRandomSpeed;

    fire_ionripper(ent, start, dir, kDamage, speed, effectFlags);
  }

  P_AddWeaponKick(ent, ent->client->vForward * -3.f, {-3.f, 0.f, 0.f});

  gi.WriteByte(svc_muzzleflash);
  gi.WriteEntity(ent);
  gi.WriteByte(MZ_IONRIPPER | isSilenced);
  gi.multicast(ent->s.origin, MULTICAST_PVS, false);

  G_PlayerNoise(ent, start, PlayerNoise::Weapon);

  ent->client->pers.match.totalShots += kProjectileCount;
  ent->client->pers.match
      .totalShotsPerWeapon[static_cast<uint8_t>(Weapon::IonRipper)] +=
      kProjectileCount;

  RemoveAmmo(ent, ammoNeeded);
}

void Weapon_IonRipper(gentity_t *ent) {
  constexpr int pauseFrames[] = {36, 0};
  constexpr int fireFrames[] = {6, 0};

  if (!ent || !ent->client)
    return;

  auto *cl = ent->client;

  // Enforce 1 second minimum delay between shots (in addition to fireFrames)
  if (cl->weapon.thinkTime > level.time)
    return;

  Weapon_Generic(
      ent, 5, 7, 36, 39, pauseFrames, fireFrames, [](gentity_t *ent) {
        ent->client->weapon.thinkTime = level.time + 1_sec; // Custom cooldown
        Weapon_IonRipper_Fire(ent);
      });
}

/*
======================================================================

PHALANX

======================================================================
*/

/*
=============
Weapon_Phalanx_Fire

Launches a Phalanx projectile, adjusting damage for powerups and alternating
barrels.
=============
*/
static void Weapon_Phalanx_Fire(gentity_t *ent) {
  constexpr int baseDamage = 80;
  constexpr float splashRadius = 100.f;
  constexpr int projectileSpeed = 725;
  const Vector3 offset = {0.f, 8.f, -8.f};

  int damage = baseDamage;
  int splashDamage = baseDamage;

  if (isQuad) {
    damage *= damageMultiplier;
    splashDamage *= damageMultiplier;
  }

  const bool isRightBarrel = (ent->client->ps.gunFrame == 8);
  const float yawOffset = isRightBarrel ? -1.5f : 1.5f;
  const int muzzleFlashType = isRightBarrel ? MZ_PHALANX2 : MZ_PHALANX;

  Vector3 firingAngles = {ent->client->vAngle[PITCH],
                          ent->client->vAngle[YAW] + yawOffset,
                          ent->client->vAngle[ROLL]};

  Vector3 start, dir;
  P_ProjectSource(ent, firingAngles, offset, start, dir);

  fire_phalanx(ent, start, dir, damage, projectileSpeed, splashRadius,
               splashDamage);

  // Muzzle flash and sound
  gi.WriteByte(svc_muzzleflash);
  gi.WriteEntity(ent);
  gi.WriteByte(muzzleFlashType | isSilenced);
  gi.multicast(ent->s.origin, MULTICAST_PVS, false);

  if (isRightBarrel) {
    ent->client->pers.match.totalShots += 2;
    ent->client->pers.match
        .totalShotsPerWeapon[static_cast<uint8_t>(Weapon::Phalanx)] += 2;
    RemoveAmmo(ent, 1);
  } else {
    G_PlayerNoise(ent, start, PlayerNoise::Weapon);
  }

  // Add weapon kick
  P_AddWeaponKick(ent, ent->client->vForward * -2.f, {-2.f, 0.f, 0.f});
}

void Weapon_Phalanx(gentity_t *ent) {
  constexpr int pauseFrames[] = {29, 42, 55, 0};
  constexpr int fireFrames[] = {7, 8, 0};

  Weapon_Generic(ent, 5, 20, 58, 63, pauseFrames, fireFrames,
                 Weapon_Phalanx_Fire);
}

/*
======================================================================

TRAP

======================================================================
*/

static void Weapon_Trap_Fire(gentity_t *ent, bool held) {
  constexpr GameTime TRAP_TIMER = 5_sec;
  constexpr float TRAP_MINSPEED = 300.f;
  constexpr float TRAP_MAXSPEED = 700.f;
  constexpr float TRAP_THROW_OFFSET_Z = -8.f;

  Vector3 start, dir;

  // Clamp pitch to avoid backwards throws and eliminate sideways offset
  Vector3 clampedAngles = {std::max(-62.5f, ent->client->vAngle[PITCH]),
                           ent->client->vAngle[YAW], ent->client->vAngle[ROLL]};

  // Calculate projectile start and direction
  P_ProjectSource(ent, clampedAngles, {8, 0, TRAP_THROW_OFFSET_Z}, start, dir);

  // Calculate speed based on how long the trap was held
  GameTime heldTime = ent->client->grenadeTime - level.time;
  float speed = TRAP_MINSPEED;

  if (ent->health > 0) {
    float timeHeldSec =
        std::clamp(heldTime.seconds(), 0.0f, TRAP_TIMER.seconds());
    speed = TRAP_MINSPEED + timeHeldSec * ((TRAP_MAXSPEED - TRAP_MINSPEED) /
                                           TRAP_TIMER.seconds());
  }

  speed = std::min(speed, TRAP_MAXSPEED);
  ent->client->grenadeTime = 0_ms;

  fire_trap(ent, start, dir, static_cast<int>(speed));

  // Track usage stats
  ent->client->pers.match.totalShots++;
  ent->client->pers.match
      .totalShotsPerWeapon[static_cast<uint8_t>(Weapon::Trap)]++;
  RemoveAmmo(ent, 1);
}

void Weapon_Trap(gentity_t *ent) {
  constexpr int pauseFrames[] = {29, 34, 39, 48, 0};

  Throw_Generic(ent, 15, 48, 5, "weapons/trapcock.wav", 11, 12, pauseFrames,
                false, "weapons/traploop.wav", Weapon_Trap_Fire, false);
}
