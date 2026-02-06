/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

g_spawn_points.cpp (Game Player Spawning) This file handles the logic for
finding and managing player spawn points. It scans the map for `info_player_*`
entities at level start, categorizes them by team or gametype, and provides the
logic for selecting an appropriate spawn point for a player entering the game or
respawning. Key Responsibilities: - Spawn Point Registration:
`G_LocateSpawnSpots` is called at the beginning of a map to find all
`info_player_*` entities and register them in the appropriate lists (e.g., FFA,
Team Red, Team Blue). - Modernized Structure: Implements a clean, vector-based
system for storing spawn points, replacing the original engine's flat array and
making the code more robust and easier to manage. - Intermission Point:
Specifically identifies the `info_player_intermission` spot and calculates the
correct camera angle for the end-of-level sequence. - Legacy Compatibility:
Provides a function to flatten the new vector-based system back into the
old-style flat array to maintain compatibility with older parts of the codebase
that have not yet been migrated.*/

#include "../../bgame/logger.hpp"
#include "../g_local.hpp"
#include "g_headhunters.hpp"
#include <algorithm>
#include <cstring>
#include <format>


/*
===============
LogEntityLabel

Return a concise label for spawn logging that includes entity number and
classname.
===============
*/
static std::string LogEntityLabel(const gentity_t *ent) {
  const char *class_name = (ent && ent->className) ? ent->className : "<unset>";
  return std::format("#{} ({})", ent ? ent->s.number : -1, class_name);
}

// -----------------------------------------------------------------------------
// Modern spawn registration
// -----------------------------------------------------------------------------

// Assumptions:
// - g_entities is a contiguous array of size globals.numEntities
// - gentity_t::className is a valid C-string when inUse == true
// - Team is an enum { Free, Red, Blue }
// - Vector3 is your math type (with -, normalized(), etc.)
// - VectorToAngles(Vector3) returns Euler angles as Vector3
// - PickTarget(const char*) returns a gentity_t* or nullptr
// - level has a 'spawn' field of type SpawnLists (added below) and an optional
//   legacy flat array 'spawnSpots' for compatibility, plus counters.
// - NUM_SPAWN_SPOTS is a legacy cap; vectors below remove that constraint.
// - ent->count == 1 means not an initial spawn (you had this convention).
// -----------------------------------------------------------------------------

/*
===============
AsciiToLower
===============
*/
static inline char AsciiToLower(char c) {
  if (c >= 'A' && c <= 'Z')
    return static_cast<char>(c - 'A' + 'a');
  return c;
}

/*
===============
IEquals
===============
*/
static bool IEquals(std::string_view a, std::string_view b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (AsciiToLower(a[i]) != AsciiToLower(b[i]))
      return false;
  }
  return true;
}

/*
===============
IStartsWith
===============
*/
static bool IStartsWith(std::string_view text, std::string_view prefix) {
  if (text.size() < prefix.size())
    return false;
  for (size_t i = 0; i < prefix.size(); ++i) {
    if (AsciiToLower(text[i]) != AsciiToLower(prefix[i]))
      return false;
  }
  return true;
}

/*
===============
SpawnSuffixFromClassname

Returns the suffix after "info_player_" if present, otherwise empty.
Examples:
  "info_player_deathmatch" -> "deathmatch"
  "info_player_team_red"   -> "team_red"
  "info_player_intermission"->"intermission"
===============
*/
static std::string_view SpawnSuffixFromClassname(std::string_view classname) {
  constexpr std::string_view kPrefix = "info_player_";
  if (!IStartsWith(classname, kPrefix))
    return {};
  return classname.substr(kPrefix.size());
}

/*
===============
RegisterSpawn

Assigns team and "count" convention, then pushes to the right list.
Returns true if registered.
===============
*/
static bool RegisterSpawn(gentity_t *ent, std::string_view suffix) {
  if (suffix.empty())
    return false;

  // intermission is unique and not added to lists
  if (IEquals(suffix, "intermission")) {
    if (!level.spawn.intermission) {
      level.spawn.intermission = ent;
      ent->fteam = Team::Free;
      // Intermission view handling is finalized in FinalizeIntermissionView
      worr::Logf(worr::LogLevel::Trace, "{}: registered intermission at {}",
                 __FUNCTION__, LogEntityLabel(ent));
    }
    return true;
  }

  // Single-player / coop start points are registered for fallbacks
  if (IEquals(suffix, "start") || IEquals(suffix, "coop") ||
      IEquals(suffix, "coop_lava")) {
    ent->fteam = Team::Free;
    level.spawn.ffa.push_back(ent);
    worr::Logf(worr::LogLevel::Trace, "{}: registered coop/solo spawn {}",
               __FUNCTION__, LogEntityLabel(ent));
    return true;
  }

  // Deathmatch (FFA)
  if (IEquals(suffix, "deathmatch")) {
    ent->fteam = Team::Free;
    ent->count = 1; // not an initial spawn point
    level.spawn.ffa.push_back(ent);
    worr::Logf(worr::LogLevel::Trace, "{}: registered FFA spawn {}",
               __FUNCTION__, LogEntityLabel(ent));
    return true;
  }

  // Team Red
  if (IEquals(suffix, "team_red")) {
    ent->fteam = Team::Red;
    ent->count = 1;
    level.spawn.red.push_back(ent);
    worr::Logf(worr::LogLevel::Trace, "{}: registered Red spawn {}",
               __FUNCTION__, LogEntityLabel(ent));
    return true;
  }

  // Team Blue
  if (IEquals(suffix, "team_blue")) {
    ent->fteam = Team::Blue;
    ent->count = 1;
    level.spawn.blue.push_back(ent);
    worr::Logf(worr::LogLevel::Trace, "{}: registered Blue spawn {}",
               __FUNCTION__, LogEntityLabel(ent));
    return true;
  }

  return false;
}

/*
===============
FinalizeIntermissionView

Sets level.intermission.origin/angles using intermission entity. If it targets
another entity, faces that target; otherwise uses the intermission's own angles.
===============
*/
static void FinalizeIntermissionView() {
  gentity_t *inter = level.spawn.intermission;
  if (!inter)
    return;

  // Always anchor the intermission origin to the entity itself.
  level.intermission.origin = inter->s.origin;

  if (inter->target) {
    if (gentity_t *tgt = PickTarget(inter->target)) {
      const Vector3 dir = (tgt->s.origin - inter->s.origin).normalized();
      level.intermission.angles = VectorToAngles(dir);
      return;
    }
  }

  // Fallback: use mapper-specified angles from the intermission spot.
  level.intermission.angles = inter->s.angles;
}

/*
===============
G_SpawnSpots_FlattenLegacy

Optional: fills your legacy flat array/counters from the new vectors.
Keeps ordering: FFA first, then Red, then Blue. Intermission remains separate.
===============
*/
static void G_SpawnSpots_FlattenLegacy() {
  level.spawnSpots.fill(nullptr);

  int n = 0;
  auto push_all = [&](const std::vector<gentity_t *> &v) {
    for (gentity_t *e : v) {
      if (n >= static_cast<int>(level.spawnSpots.size()))
        return;
      level.spawnSpots[static_cast<size_t>(n++)] = e;
    }
  };

  push_all(level.spawn.ffa);
  push_all(level.spawn.red);
  push_all(level.spawn.blue);

  if (level.spawn.intermission) {
    level.spawnSpots[SPAWN_SPOT_INTERMISSION] = level.spawn.intermission;
  }
}

/*
===============
G_LocateSpawnSpots
===============
*/
void G_LocateSpawnSpots() {
  level.spawn.Clear();

  // Scan entity list once.
  for (uint32_t i = 0; i < globals.numEntities; ++i) {
    gentity_t *ent = &g_entities[i];
    if (!ent || !ent->inUse || !ent->className)
      continue;

    std::string_view cls = ent->className;
    const std::string_view suffix = SpawnSuffixFromClassname(cls);
    if (suffix.empty())
      continue;

    RegisterSpawn(ent, suffix);
  }

  // Ensure intermission view is set if present.
  FinalizeIntermissionView();

  // Optional: keep legacy fields in sync while you migrate call sites.
  G_SpawnSpots_FlattenLegacy();

  const size_t ffa_count = level.spawn.ffa.size();
  const size_t red_count = level.spawn.red.size();
  const size_t blue_count = level.spawn.blue.size();
  const size_t total_count = ffa_count + red_count + blue_count;
  worr::Logf(worr::LogLevel::Debug,
             "{}: spawn spot totals -> ffa:{} red:{} blue:{} intermission:{}",
             __FUNCTION__, ffa_count, red_count, blue_count,
             level.spawn.intermission ? 1 : 0);
  worr::Logf(worr::LogLevel::Trace, "{}: processed {} spawn points this map",
             __FUNCTION__, total_count);
}

// ==============================================================================

/*
===============
IsProxMine
===============
*/
static inline bool IsProxMine(const gentity_t *e) {
  return e && e->className && strcmp(e->className, "prox_mine") == 0;
}

/*
===============
IsTeslaMine
===============
*/
static inline bool IsTeslaMine(const gentity_t *e) {
  return e && e->className && strncmp(e->className, "tesla", 5) == 0;
}

/*
===============
IsTrap
===============
*/
static inline bool IsTrap(const gentity_t *e) {
  return e && e->className && strncmp(e->className, "food_cube_trap", 14) == 0;
}

/*
===============
SpawnPointHasNearbyMines
Returns true if any mine or trap is within 'radius' of 'origin'
===============
*/
static bool SpawnPointHasNearbyMines(const Vector3 &origin, float radius) {
  gentity_t *it = nullptr;
  while ((it = FindRadius(it, origin, radius)) != nullptr) {
    if (!it->className)
      continue;
    if (IsProxMine(it) || IsTeslaMine(it) || IsTrap(it))
      return true;
  }
  return false;
}

/*
===============
Square
===============
*/
[[maybe_unused]] static inline float Square(float x) { return x * x; }

/*
===============
SpawnEye
Small z-lift for visibility checks.
===============
*/
static inline Vector3 SpawnEye(const Vector3 &p) {
  return p + Vector3{0.0f, 0.0f, 16.0f};
}

/*
===============
IsEnemy
===============
*/
static inline bool IsEnemy(const gentity_t *a, const gentity_t *b) {
  if (!a || !b || !a->client || !b->client)
    return true;
  if (!Teams())
    return true;
  return a->client->sess.team != b->client->sess.team;
}

/*
===============
AnyDirectEnemyLoS
Returns true if any enemy has an unobstructed line-of-sight to the spot.
A range std::clamp avoids expensive full-map traces; tune MAX_LOS_DIST as
needed.
===============
*/
static bool AnyDirectEnemyLoS(const gentity_t *requester, const Vector3 &spot,
                              float maxDist) {
  if (!requester || !requester->client) {
    return false;
  }

  const Vector3 toCheck = SpawnEye(spot);

  for (auto ec : active_clients()) {
    if (ec->health <= 0 || !IsEnemy(requester, ec))
      continue;

    const Vector3 from = SpawnEye(ec->s.origin);
    const Vector3 delta = toCheck - from;
    const float dist = delta.length();
    if (dist > maxDist)
      continue;

    trace_t tr = gi.trace(from, PLAYER_MINS, PLAYER_MAXS, toCheck, nullptr,
                          MASK_SOLID & ~CONTENTS_PLAYER);
    if (tr.fraction == 1.0f) {
      // Direct, unobstructed line-of-sight
      return true;
    }
  }
  return false;
}

/*
===============
G_UnsafeSpawnPosition
Returns a blocking entity if the given spot is unsafe (solid or a player),
otherwise returns nullptr. Optionally ignores players when check_players ==
false. Attempts a tiny Z nudge and a generic un-stuck fix for map quirks.
===============
*/
static gentity_t *G_UnsafeSpawnPosition(Vector3 spot, bool check_players,
                                        const gentity_t *ignore = nullptr) {
  contents_t mask = MASK_PLAYERSOLID;
  if (!check_players) {
    mask &= ~CONTENTS_PLAYER;
  }

  gentity_t *ignoreEnt = const_cast<gentity_t *>(ignore);
  trace_t tr = gi.trace(spot, PLAYER_MINS, PLAYER_MAXS, spot, ignoreEnt, mask);

  // If embedded in non-client brush, try a tiny vertical nudge
  if (tr.startSolid && (!tr.ent || !tr.ent->client)) {
    spot[2] += 1.0f;
    tr = gi.trace(spot, PLAYER_MINS, PLAYER_MAXS, spot, ignoreEnt, mask);
  }

  // If still embedded in non-client geometry, try the generic un-stuck helper
  if (tr.startSolid && (!tr.ent || !tr.ent->client)) {
    const StuckResult fix = G_FixStuckObject_Generic(
        spot, PLAYER_MINS, PLAYER_MAXS,
        [mask, ignoreEnt](const Vector3 &start, const Vector3 &mins,
                          const Vector3 &maxs, const Vector3 &end) {
          return gi.trace(start, mins, maxs, end, ignoreEnt, mask);
        });

    if (fix == StuckResult::NoGoodPosition) {
      return tr.ent; // could be world or brush entity; report the blocker
    }

    tr = gi.trace(spot, PLAYER_MINS, PLAYER_MAXS, spot, ignoreEnt, mask);
    if (tr.startSolid && (!tr.ent || !tr.ent->client)) {
      return tr.ent;
    }
  }

  // Clear? then safe
  if (tr.fraction == 1.0f) {
    return nullptr;
  }

  // Any hit is unsafe; prefer the blocking entity, or world if missing
  return tr.ent ? tr.ent : world;
}

/*
===============
SpotIsClearOfSolidsAndPlayers
Fast occupancy/solid check using your unstuck helper for gnarly brushes.
===============
*/
static bool SpotIsClearOfSolidsAndPlayers(const Vector3 &p, bool check_players,
                                          const gentity_t *ignore = nullptr) {
  return G_UnsafeSpawnPosition(p, check_players, ignore) == nullptr;
}

/*
===============
SpotIsSafe
Telefrag/solid guard, replacing the ad-hoc AABB overlap check.
===============
*/
static bool SpotIsSafe(gentity_t *spot, bool check_players = true) {
  if (!spot)
    return false;
  // Match the actual spawn Z offset to avoid false negatives.
  float zlift = 1.0f; // base lift applied in PutClientOnSpawnPoint
  if (deathmatch->integer) {
    zlift += match_allowSpawnPads->integer ? 9.0f : 1.0f;
  }
  const Vector3 p = spot->s.origin + Vector3{0.0f, 0.0f, zlift};
  return SpotIsClearOfSolidsAndPlayers(p, check_players, spot);
}

constexpr SpawnFlags SPAWNFLAG_INITIAL =
    0x10000_spawnflag; // example value, adjust as needed

/*
===============
FilterInitialSpawns
Keep only INITIAL-flagged spawns when present; otherwise fallback to all.
===============
*/
static std::vector<gentity_t *>
FilterInitialSpawns(const std::vector<gentity_t *> &spawns) {
  std::vector<gentity_t *> flagged;
  flagged.reserve(spawns.size());
  for (auto *s : spawns) {
    if (!s)
      continue;
    if (s->spawnFlags.has(SPAWNFLAG_INITIAL)) {
      flagged.push_back(s);
    }
  }
  return flagged.empty() ? spawns : flagged;
}

/*
================
PlayersRangeFromSpot

Returns the distance to the nearest player from the given spot
muffmode: excludes current client
================
*/
static float PlayersRangeFromSpot(gentity_t *ent, gentity_t *spot) {
  float bestplayerdistance;
  Vector3 v;
  float playerdistance;

  bestplayerdistance = 9999999;

  for (auto ec : active_clients()) {
    if (ec->health <= 0 || ec->client->eliminated)
      continue;
#if 0
		if (ent != nullptr)
			if (ec == ent)
				continue;
#endif
    v = spot->s.origin - ec->s.origin;
    playerdistance = v.length();

    if (playerdistance < bestplayerdistance)
      bestplayerdistance = playerdistance;
  }

  return bestplayerdistance;
}

/*
===============
FilterEligibleSpawns
General screening: not blocked, not too close to avoid_point, no nearby mines,
not too close to other players, not directly in enemy LoS.
force_spawn still checks geometry but ignores player collisions and soft
filters.
===============
*/
static std::vector<gentity_t *>
FilterEligibleSpawns(const std::vector<gentity_t *> &spawns,
                     const Vector3 &avoid_point, bool force_spawn,
                     gentity_t *entForTeamLogic, // may be null
                     bool respectAvoidPoint) {
  constexpr float MIN_AVOID_DIST =
      192.0f; // distance from avoid_point (e.g. last death)
  constexpr float MIN_PLAYER_RADIUS = 160.0f; // keep away from nearest player
  constexpr float MINE_RADIUS = 196.0f;       // avoid mines/traps around spot
  constexpr float MAX_LOS_DIST =
      2048.0f; // consider LoS threats up to this range

  std::vector<gentity_t *> out;
  out.reserve(spawns.size());

  for (auto *s : spawns) {
    if (!s)
      continue;

    // Hard safety: allow forced spawns to bypass when we're stuck.
    if (!SpotIsSafe(s, !force_spawn))
      continue;

    if (!force_spawn) {
      // Keep away from the avoid point (e.g., last death)
      if (respectAvoidPoint &&
          (s->s.origin - avoid_point).length() < MIN_AVOID_DIST)
        continue;

      // No nearby mines/traps
      if (SpawnPointHasNearbyMines(s->s.origin, MINE_RADIUS))
        continue;

      // Player proximity
      if (PlayersRangeFromSpot(entForTeamLogic, s) < MIN_PLAYER_RADIUS)
        continue;

      // Enemy line-of-sight
      if (AnyDirectEnemyLoS(entForTeamLogic, s->s.origin, MAX_LOS_DIST))
        continue;
    }

    out.push_back(s);
  }

  return out;
}

/*
===============
FilterFallbackSpawns
Lightweight fallback filter: occupancy and minimum distance from avoid_point.
===============
*/
static std::vector<gentity_t *>
FilterFallbackSpawns(const std::vector<gentity_t *> &spawns,
                     const Vector3 &avoid_point) {
  constexpr float MIN_DIST = 192.0f;
  std::vector<gentity_t *> out;
  out.reserve(spawns.size());
  for (auto *s : spawns) {
    if (!s)
      continue;
    if (!SpotIsSafe(s))
      continue;
    if ((s->s.origin - avoid_point).length() < MIN_DIST)
      continue;
    out.push_back(s);
  }
  return out;
}

/*
===============
PickRandomly
===============
*/
template <typename T> static T *PickRandomly(const std::vector<T *> &vec) {
  if (vec.empty())
    return nullptr;
  std::uniform_int_distribution<size_t> dist(0, vec.size() - 1);
  return vec[dist(game.mapRNG)];
}

/*
===============
SelectFromSpawnList
Pick random among all spots within epsilon of the best score.
"scoreFn" must return lower-is-better scores.
===============
*/
static gentity_t *
SelectFromSpawnList(const std::vector<gentity_t *> &spawns,
                    const std::function<float(gentity_t *)> &scoreFn) {
  if (spawns.empty())
    return nullptr;

  float best = scoreFn(spawns[0]);
  for (auto *s : spawns) {
    best = std::min(best, scoreFn(s));
  }

  constexpr float EPS =
      0.05f; // 5 percent tolerance if we use normalized scores
  std::vector<gentity_t *> finalists;
  finalists.reserve(spawns.size());
  for (auto *s : spawns) {
    const float sc = scoreFn(s);
    // treat as tie if within epsilon of best
    if (sc <= best + std::max(EPS, 0.01f * std::abs(best)))
      finalists.push_back(s);
  }

  if (finalists.empty())
    return nullptr;

  return PickRandomly(finalists);
}

/*
===============
CompositeDangerScore
Blend heatmap, enemy LoS risk, player proximity, and avoid_point distance.
Lower is better.

Weights are intentionally conservative; tune at runtime once you graph
HM_Query().
===============
*/
static float CompositeDangerScore(gentity_t *s, gentity_t *ent,
                                  const Vector3 &avoid_point) {
  // Heat (0..1) from your combat heat map (nearby recent combat)
  const float heat =
      HM_DangerAt(s->s.origin); // 0..1 normalized by HM_DangerAt impl
  // Distance to nearest player (larger is safer, so invert)
  const float nearest = std::max(1.0f, PlayersRangeFromSpot(ent, s));
  const float nearPenalty = 1.0f / nearest; // 0..1-ish
  // Enemy LoS risk as binary bump; soft penalty to prefer out-of-sight
  const bool los = AnyDirectEnemyLoS(ent, s->s.origin, 2048.0f);
  const float losPenalty = los ? 0.5f : 0.0f;
  // Avoid-point proximity (e.g., last-death). Closer is worse.
  const float ad = (s->s.origin - avoid_point).length();
  const float avoidPenalty = 1.0f / std::max(1.0f, ad);

  // Mines near spot increase danger
  const bool mines = SpawnPointHasNearbyMines(s->s.origin, 196.0f);
  const float minePenalty = mines ? 0.5f : 0.0f;

  // Weighted sum (lower is better)
  const float score = 0.50f * heat + 0.20f * losPenalty + 0.15f * nearPenalty +
                      0.10f * avoidPenalty + 0.05f * minePenalty;

  return score;
}

/*
===============
SelectTeamSpawnPoint
Select from team list first, fallback to FFA then classic start.
Fixed: pass scoreFn instead of an entity to SelectFromSpawnList.
===============
*/
static gentity_t *SelectTeamSpawnPoint(gentity_t *ent, Team team) {
  const std::vector<gentity_t *> *list = nullptr;
  switch (team) {
  case Team::Red:
    list = &level.spawn.red;
    break;
  case Team::Blue:
    list = &level.spawn.blue;
    break;
  default:
    list = &level.spawn.ffa;
    break;
  }

  auto scoreFn = [ent](gentity_t *s) {
    return CompositeDangerScore(
        s, ent, ent ? ent->client->lastDeathLocation : Vector3{0, 0, 0});
  };

  // [Paril-KEX] Filter for safety first!
  std::vector<gentity_t *> eligible;
  eligible.reserve(list->size());
  for (gentity_t *s : *list) {
    if (SpotIsSafe(s))
      eligible.push_back(s);
  }

  if (gentity_t *spot = SelectFromSpawnList(eligible, scoreFn))
    return spot;

  // Fallback to FFA list if team list fails
  if (list != &level.spawn.ffa) {
    eligible.clear();
    eligible.reserve(level.spawn.ffa.size());
    for (gentity_t *s : level.spawn.ffa) {
      if (SpotIsSafe(s))
        eligible.push_back(s);
    }
    if (gentity_t *spot = SelectFromSpawnList(eligible, scoreFn))
      return spot;
  }
  if (gentity_t *only =
          G_FindByString<&gentity_t::className>(nullptr, "info_player_start"))
    return only;

  return nullptr;
}

/*
===============
SelectAnyTeamSpawnPoint

Fallback picker for team-based maps when the player has no team or
team-specific spawns are unavailable.
===============
*/
static gentity_t *SelectAnyTeamSpawnPoint(gentity_t *ent,
                                          const Vector3 &avoid_point,
                                          bool force_spawn) {
  std::vector<gentity_t *> team_spawns;
  team_spawns.reserve(level.spawn.red.size() + level.spawn.blue.size());
  team_spawns.insert(team_spawns.end(), level.spawn.red.begin(),
                     level.spawn.red.end());
  team_spawns.insert(team_spawns.end(), level.spawn.blue.begin(),
                     level.spawn.blue.end());

  if (team_spawns.empty())
    return nullptr;

  const bool hasAvoidPoint = static_cast<bool>(avoid_point);
  auto eligible = FilterEligibleSpawns(team_spawns, avoid_point, force_spawn,
                                       ent, hasAvoidPoint);

  if (eligible.empty()) {
    auto fallback = FilterFallbackSpawns(team_spawns, avoid_point);
    if (!fallback.empty())
      return PickRandomly(fallback);
    return nullptr;
  }

  auto scoreFn = [ent, avoid_point](gentity_t *s) {
    return CompositeDangerScore(s, ent, avoid_point);
  };

  return SelectFromSpawnList(eligible, scoreFn);
}

// ==============================================================================
// Deathmatch spawn selection
// ==============================================================================

/*
===============
SelectDeathmatchSpawnPoint
===============
*/
select_spawn_result_t
SelectDeathmatchSpawnPoint(gentity_t *ent, Vector3 avoid_point,
                           bool force_spawn, bool fallback_to_ctf_or_start,
                           bool intermission, bool initial) {
  // Intermission: only pick the intermission camera spot
  if (intermission) {
    if (level.spawn.intermission) {
      return {level.spawn.intermission, SelectSpawnFlags::Intermission};
    }
    // No intermission spot available; fall through to normal selection.
  }

  // Initial spawns: prefer INITIAL-flagged points if any exist
  std::vector<gentity_t *> baseList = level.spawn.ffa;
  if (initial) {
    baseList = FilterInitialSpawns(baseList);
  }

  const bool hasAvoidPoint = static_cast<bool>(avoid_point);

  if (!hasAvoidPoint) {
    std::shuffle(baseList.begin(), baseList.end(), game.mapRNG);
  }

  // Screen for eligibility
  auto eligible = FilterEligibleSpawns(baseList, avoid_point, force_spawn, ent,
                                       hasAvoidPoint);

  // If none survived and fallback is allowed, try relaxed fallback set
  if (eligible.empty() && fallback_to_ctf_or_start) {
    auto fb = FilterFallbackSpawns(baseList, avoid_point);
    if (!fb.empty()) {
      auto scoreFn = [ent, avoid_point](gentity_t *s) {
        return CompositeDangerScore(s, ent, avoid_point);
      };
      if (gentity_t *pick = SelectFromSpawnList(fb, scoreFn)) {
        return {pick, SelectSpawnFlags::Fallback};
      }
    }
  }

  // If still none and teams are active, try the team lists
  if (eligible.empty() && Teams() && fallback_to_ctf_or_start) {
    Team team = Team::None;
    if (ent && ent->client)
      team = ent->client->sess.team;

    if (gentity_t *t = SelectTeamSpawnPoint(ent, team))
      return {t, SelectSpawnFlags::Fallback};

    if (gentity_t *t = SelectAnyTeamSpawnPoint(ent, avoid_point, force_spawn))
      return {t, SelectSpawnFlags::Fallback};
  }

  // Final fallback: any FFA spot that is at least not embedded
  if (eligible.empty()) {
    auto loose = FilterFallbackSpawns(level.spawn.ffa, avoid_point);
    if (!loose.empty()) {
      return {PickRandomly(loose), SelectSpawnFlags::Fallback};
    }
    return {nullptr, SelectSpawnFlags::None};
  }

  // Normal case: choose the lowest danger score, random within epsilon
  auto scoreFn = [ent, avoid_point](gentity_t *s) {
    return CompositeDangerScore(s, ent, avoid_point);
  };
  if (gentity_t *pick = SelectFromSpawnList(eligible, scoreFn)) {
    return {pick,
            initial ? SelectSpawnFlags::Initial : SelectSpawnFlags::Normal};
  }

  return {nullptr, SelectSpawnFlags::None};
}

// ==============================================================================
// Single-player and Coop spawn selection
// ==============================================================================

// Forward declarations
static gentity_t *SelectSingleSpawnPoint(gentity_t *ent);

/*
===============
SelectLavaCoopSpawnPoint
Find the highest active lava (func_water with SMART flag and contents water),
then choose the lowest coop-lava spawn that sits above that lava top (with
a small safety margin) and is not too close to other players.
===============
*/
static gentity_t *SelectLavaCoopSpawnPoint(gentity_t *ent) {
  if (!ent) {
    return nullptr;
  }

  // Find highest active lava top
  float highestTopZ = -FLT_MAX;
  gentity_t *highestLava = nullptr;

  for (gentity_t *lava = nullptr; (lava = G_FindByString<&gentity_t::className>(
                                       lava, "func_water")) != nullptr;) {
    // Only consider "smart" volumes that actually have water contents at their
    // center
    if (!lava->spawnFlags.has(SPAWNFLAG_WATER_SMART))
      continue;

    Vector3 center = lava->absMax + lava->absMin;
    center *= 0.5f;

    if ((gi.pointContents(center) & MASK_WATER) == 0)
      continue;

    const float topZ = lava->absMax[2];
    if (topZ > highestTopZ) {
      highestTopZ = topZ;
      highestLava = lava;
    }
  }

  // No lava found
  if (!highestLava) {
    return nullptr;
  }

  // Safety clearance above lava top (bbox margin)
  const float lavaTopThreshold = highestTopZ + 64.0f;

  // Gather coop-lava spawn points
  std::vector<gentity_t *> spawns;
  spawns.reserve(64);
  for (gentity_t *spot = nullptr;
       (spot = G_FindByString<&gentity_t::className>(
            spot, "info_player_coop_lava")) != nullptr;) {
    spawns.push_back(spot);
  }

  if (spawns.empty()) {
    return nullptr;
  }

  // Choose the lowest Z that is above lavaTopThreshold and not too close to
  // players
  gentity_t *best = nullptr;
  float bestZ = FLT_MAX;

  for (gentity_t *s : spawns) {
    const float z = s->s.origin[_Z];
    if (z < lavaTopThreshold)
      continue;

    // Require some clearance from other players
    if (PlayersRangeFromSpot(ent, s) <= 32.0f)
      continue;

    if (z < bestZ) {
      bestZ = z;
      best = s;
    }
  }

  return best;
}

/*
===============
SelectCoopSpawnPoint
Enhanced: uses heat map, LOS, proximity, and mines, with lava-coop support.
Prefers safe coop starts; falls back to SP start, then FFA list.
===============
*/
static gentity_t *SelectCoopSpawnPoint(gentity_t *ent) {
  if (!ent)
    return nullptr;

  // Prefer map-provided lava-safe coop spawns when available.
  if (gentity_t *lava =
          SelectLavaCoopSpawnPoint(ent)) // optional; present in this file
    return lava;

  // Gather coop starts
  std::vector<gentity_t *> coopSpots;
  for (gentity_t *s = nullptr; (s = G_FindByString<&gentity_t::className>(
                                    s, "info_player_coop")) != nullptr;) {
    if (s->inUse)
      coopSpots.push_back(s);
  }

  // Fallback: classic single-player start
  if (coopSpots.empty()) {
    if (gentity_t *start =
            G_FindByString<&gentity_t::className>(nullptr, "info_player_start"))
      return SpotIsSafe(start) ? start : nullptr;
  }

  // If still nothing, consider FFA list to keep players flowing
  if (coopSpots.empty() && !level.spawn.ffa.empty())
    coopSpots = level.spawn.ffa;

  if (coopSpots.empty())
    return nullptr;

  // Safety-screen the set
  const Vector3 avoid_point =
      (ent && ent->client) ? ent->client->lastDeathLocation : Vector3{0, 0, 0};
  const bool hasAvoidPoint = static_cast<bool>(avoid_point);
  auto eligible = FilterEligibleSpawns(
      coopSpots, avoid_point, /*force_spawn=*/false, ent, hasAvoidPoint);
  if (eligible.empty())
    eligible = FilterFallbackSpawns(coopSpots, avoid_point);

  if (eligible.empty()) {
    // Deterministic last-ditch so we never hard-fail coop
    const int clientNum = static_cast<int>(ent - g_entities);
    return coopSpots[static_cast<size_t>(clientNum %
                                         static_cast<int>(coopSpots.size()))];
  }

  // Score by heat + LOS + proximity + avoid_point + mines
  auto scoreFn = [ent, avoid_point](gentity_t *s) {
    return CompositeDangerScore(s, ent, avoid_point);
  };
  if (gentity_t *pick = SelectFromSpawnList(eligible, scoreFn))
    return pick;

  return nullptr;
}

/*
===============
SelectCoopFallbackSpawnPoint

Provides a more permissive coop spawn search when the primary pass fails.
===============
*/
static gentity_t *SelectCoopFallbackSpawnPoint(gentity_t *ent) {
  // Last-resort single-player start without safety checks
  if (gentity_t *start = SelectSingleSpawnPoint(ent))
    return start;

  // Final attempt: reuse any registered FFA spawn to keep players moving
  if (!level.spawn.ffa.empty())
    return level.spawn.ffa.front();

  return nullptr;
}

/*
===============
TryLandmarkSpawn

Attempt to place a client relative to a landmark from a previous map. Safely
no-ops if no client data is available or if the landmark cannot be resolved.
===============
*/
static bool TryLandmarkSpawn(gentity_t *ent, Vector3 &origin, Vector3 &angles) {
  if (!ent || !ent->client || !ent->client->landmark_name ||
      !strlen(ent->client->landmark_name)) {
    return false;
  }

  gentity_t *landmark = PickTarget(ent->client->landmark_name);
  if (!landmark) {
    return false;
  }

  Vector3 originalOrigin = origin;
  Vector3 spot_origin = origin;
  Vector3 originalAngles = angles;
  origin = ent->client->landmark_rel_pos;

  // rotate our relative landmark into our new landmark's frame of reference
  origin =
      RotatePointAroundVector({1, 0, 0}, origin, landmark->s.angles[PITCH]);
  origin = RotatePointAroundVector({0, 1, 0}, origin, landmark->s.angles[ROLL]);
  origin = RotatePointAroundVector({0, 0, 1}, origin, landmark->s.angles[YAW]);

  origin += landmark->s.origin;

  // Preserve the player's relative view when transitioning between maps.
  // The rerelease originally summed the previous view angles with the
  // landmark orientation to provide a seamless experience.
  angles = ent->client->oldViewAngles + landmark->s.angles;

  if (landmark->spawnFlags.has(SPAWNFLAG_LANDMARK_KEEP_Z))
    origin[_Z] = spot_origin[2];

  // sometimes, landmark spawns can cause slight inconsistencies in collision;
  // we'll do a bit of tracing to make sure the bbox is clear
  if (G_FixStuckObject_Generic(origin, PLAYER_MINS, PLAYER_MAXS,
                               [ent](const Vector3 &start, const Vector3 &mins,
                                     const Vector3 &maxs, const Vector3 &end) {
                                 return gi.trace(start, mins, maxs, end, ent,
                                                 MASK_PLAYERSOLID &
                                                     ~CONTENTS_PLAYER);
                               }) == StuckResult::NoGoodPosition) {
    origin = originalOrigin;
    angles = originalAngles;
    return false;
  }

  ent->s.origin = origin;

  // rotate the velocity that we grabbed from the map
  if (ent->velocity) {
    ent->velocity = RotatePointAroundVector({1, 0, 0}, ent->velocity,
                                            landmark->s.angles[PITCH]);
    ent->velocity = RotatePointAroundVector({0, 1, 0}, ent->velocity,
                                            landmark->s.angles[ROLL]);
    ent->velocity = RotatePointAroundVector({0, 0, 1}, ent->velocity,
                                            landmark->s.angles[YAW]);
  }

  return true;
}

/*
===============
SelectSingleSpawnPoint
Chooses a single-player start. Honors game.spawnPoint (targetname) if present.
Prefers a start without a targetName if no explicit targetname match is found.
Falls back to any start if needed.
===============
*/
static gentity_t *SelectSingleSpawnPoint(gentity_t *ent) {
  gentity_t *spot = nullptr;

  // First pass: exact targetname match if game.spawnPoint is set
  while ((spot = G_FindByString<&gentity_t::className>(
              spot, "info_player_start")) != nullptr) {
    if (!game.spawnPoint[0] && !spot->targetName)
      break;

    if (!game.spawnPoint[0] || !spot->targetName)
      continue;

    if (Q_strcasecmp(game.spawnPoint.data(), spot->targetName) == 0)
      break;
  }

  if (!spot) {
    // Second pass: any start with no targetName
    while ((spot = G_FindByString<&gentity_t::className>(
                spot, "info_player_start")) != nullptr) {
      if (!spot->targetName)
        return spot;
    }
  }

  // Final fallback: any start at all
  if (!spot) {
    return G_FindByString<&gentity_t::className>(spot, "info_player_start");
  }

  return spot;
}

static bool SelectSpectatorSpawnPoint(Vector3 &origin, Vector3 &angles);

// ==============================================================================

/*
===============
SelectSpawnPoint

Chooses a player start, deathmatch start, coop start, etc
===============
*/
bool SelectSpawnPoint(gentity_t *ent, Vector3 &origin, Vector3 &angles,
                      bool force_spawn, bool &landmark) {
  landmark = false;

  gentity_t *spot = nullptr;

  // Deathmatch
  if (deathmatch->integer) {
    const bool has_client = ent && ent->client;
    const bool wants_player_spawn =
        has_client && ClientIsPlaying(ent->client) && !ent->client->eliminated;

    if (has_client && !ClientIsPlaying(ent->client)) {
      if (SelectSpectatorSpawnPoint(origin, angles)) {
        angles[ROLL] = 0.0f;
        return true;
      }
    }

    // Team spawns first when in team modes for active players only
    if (Teams() && wants_player_spawn) {
      spot = SelectTeamSpawnPoint(ent, ent->client ? ent->client->sess.team
                                                   : Team::Free);
    }

    // FFA spawns if no team spot was chosen
    if (!spot) {
      const Vector3 avoid_point = (ent && ent->client)
                                      ? ent->client->lastDeathLocation
                                      : Vector3{0, 0, 0};
      const bool intermission = static_cast<bool>(level.intermission.time);
      const bool has_any_spawns = !level.spawn.ffa.empty() ||
                                  !level.spawn.red.empty() ||
                                  !level.spawn.blue.empty();
      select_spawn_result_t result = SelectDeathmatchSpawnPoint(
          ent, avoid_point, force_spawn, true, intermission, false);

      if (!result.spot && !force_spawn) {
        // Fall back to a forced spawn if all spots are temporarily blocked.
        result = SelectDeathmatchSpawnPoint(ent, avoid_point, true, true,
                                            intermission, false);
      }

      if (!result.spot) {
        if (level.time <= level.entityReloadGraceUntil) {
          if (g_verbose->integer) {
            const GameTime remaining =
                level.entityReloadGraceUntil - level.time;
            gi.Com_PrintFmt("{}: waiting for spawn points after entity reload "
                            "({} ms remaining)\n",
                            __FUNCTION__, remaining.milliseconds());
          }

          return false;
        }

        if (!has_any_spawns) {
          gi.Com_PrintFmt("{}: no spawn points defined for this map\n",
                          __FUNCTION__);
          return false;
        }
        if (g_verbose->integer) {
          gi.Com_PrintFmt("{}: no valid spawn points; will retry\n",
                          __FUNCTION__);
        }
        return false;
      }

      spot = result.spot;
    }

    if (!spot) {
      return false;
    }

    // Validate spot one more time before dereferencing
    if (!spot || !spot->inUse) {
      gi.Com_PrintFmt("{}: selected spawn point is invalid\n", __FUNCTION__);
      return false;
    }

    // Place slightly above pad if allowed
    const float zlift = match_allowSpawnPads->integer ? 9.0f : 1.0f;
    origin = spot->s.origin + gvec3_t{0.0f, 0.0f, zlift};
    angles = spot->s.angles;

    // Ensure no roll; optionally zero pitch if desired
    angles[ROLL] = 0.0f;
    // if (ent && ent->client && ClientIsPlaying(ent->client)) angles[PITCH] =
    // 0.0f;

    return true;
  }

  // Coop
  if (coop->integer) {
    // Prefer open spots first
    spot = SelectCoopSpawnPoint(ent);
    if (!spot) {
      spot = SelectCoopFallbackSpawnPoint(ent);
    }

    // No open spot yet: during intermission, spawn at intermission camera
    if (!spot) {
      if (level.intermission.time) {
        origin = level.intermission.origin;
        angles = level.intermission.angles;
        return true;
      }
      return false;
    }
  }
  // Single player
  else {
    spot = SelectSingleSpawnPoint(ent);

    // In SP, hard fallback to 0,0,0 if a spot cannot be found
    if (!spot) {
      gi.Com_PrintFmt("Couldn't find spawn point {}\n", game.spawnPoint.data());
      origin = {0.0f, 0.0f, 0.0f};
      angles = {0.0f, 0.0f, 0.0f};
      return true;
    }
  }
  // Common placement (Coop/SP)
  origin = spot->s.origin;
  angles = spot->s.angles;

  // Landmark support
  if (TryLandmarkSpawn(ent, origin, angles)) {
    landmark = true;
  } else {
    angles[ROLL] = 0.0f;
  }

  return true;
}

/*
===============
SelectSpectatorSpawnPoint
Uses the intermission camera if available.
===============
*/
static bool SelectSpectatorSpawnPoint(Vector3 &origin, Vector3 &angles) {
  if (!level.spawn.intermission)
    return false;

  FindIntermissionPoint();
  origin = level.intermission.origin;
  angles = level.intermission.angles;
  return true;
}

// ==============================================================================
// Client spawning
// ==============================================================================

static inline void PutClientOnSpawnPoint(gentity_t *ent,
                                         const Vector3 &spawnOrigin,
                                         const Vector3 &spawnAngles) {
  gclient_t *cl = ent->client;

  cl->ps.pmove.origin = spawnOrigin;

  ent->s.origin = spawnOrigin;
  if (!cl->coopRespawn.useSquad)
    ent->s.origin[_Z] += 1; // make sure off ground
  ent->s.oldOrigin = ent->s.origin;

  // set the delta angle
  cl->ps.pmove.deltaAngles = spawnAngles - cl->resp.cmdAngles;

  ent->s.angles = spawnAngles;
  // ent->s.angles[PITCH] /= 3;		//muff: why??

  cl->ps.viewAngles = ent->s.angles;
  cl->vAngle = ent->s.angles;

  cl->oldViewAngles = ent->s.angles;

  AngleVectors(cl->vAngle, cl->vForward, nullptr, nullptr);
}

/*
===========
MoveClientToFreeCam
============
*/
void MoveClientToFreeCam(gentity_t *ent) {
  ent->moveType = MoveType::FreeCam;
  ent->solid = SOLID_NOT;
  ent->svFlags |= SVF_NOCLIENT;
  ent->client->ps.gunIndex = 0;
  ent->client->ps.gunSkin = 0;

  ent->client->ps.stats[STAT_SHOW_STATUSBAR] = 0;

  ent->takeDamage = false;
  ent->s.modelIndex = 0;
  ent->s.modelIndex2 = 0;
  ent->s.modelIndex3 = 0;
  ent->s.effects = EF_NONE;
  ent->client->ps.damageBlend[3] = ent->client->ps.screenBlend[3] = 0;
  ent->client->ps.rdFlags = RDF_NONE;
  ent->s.sound = 0;

  gi.linkEntity(ent);
}

/*
===========
ClientSpawn

Previously known as 'PutClientInServer'

Called when a player connects to a server or respawns in
a deathmatch.
============
*/
void ClientSpawn(gentity_t *ent) {
  gclient_t *cl = ent->client;

  if (!cl)
    return;

  HeadHunters::ResetPlayerState(cl);
  Harvester_OnClientSpawn(ent);

  int index = ent - g_entities - 1;
  Vector3 spawnOrigin, spawnAngles;
  client_persistant_t savedPers;
  client_respawn_t savedResp;
  client_session_t savedSess;

  cl->coopRespawnState = CoopRespawn::None;

  if (Game::Has(GameFlags::Rounds | GameFlags::Elimination) &&
      level.matchState == MatchState::In_Progress &&
      Game::IsNot(GameType::Horde)) {
    if (level.roundState == RoundState::In_Progress ||
        level.roundState == RoundState::Ended) {
      cl->eliminated = true;
    } else {
      cl->eliminated = false;
    }
  }
  bool eliminated = ent->client->eliminated;
  int lives = 0;
  if (G_LimitedLivesActive()) {
    if (cl->pers.limitedLivesPersist) {
      lives = cl->pers.limitedLivesStash;
    } else {
      lives = G_LimitedLivesMax();
    }
  }

  // clear velocity now, since landmark may change it
  ent->velocity = {};

  if (cl->landmark_name != nullptr)
    ent->velocity = cl->oldVelocity;

  // find a spawn point
  // do it before setting health back up, so farthest
  // ranging doesn't count this client
  bool valid_spawn = false;
  bool force_spawn = cl->awaitingRespawn && level.time > cl->respawn_timeout;
  bool is_landmark = false;
  bool usedGhostSpawn = false;

  InitPlayerTeam(ent);
  cl->ps.teamID = static_cast<int>(cl->sess.team);

  if (!ClientIsPlaying(cl) || eliminated)
    ent->flags |= FL_NOTARGET;
  else
    ent->flags &= ~FL_NOTARGET;

  if (cl->coopRespawn.useSquad) {
    spawnOrigin = cl->coopRespawn.squadOrigin;
    spawnAngles = cl->coopRespawn.squadAngles;
    spawnAngles[ROLL] = 0.0f;
    valid_spawn = true;
  } else
    valid_spawn = SelectSpawnPoint(ent, spawnOrigin, spawnAngles, force_spawn,
                                   is_landmark);

  // [Paril-KEX] if we didn't get a valid spawn, hold us in
  // limbo for a while until we do get one
  if (!valid_spawn) {
    // only do this once per spawn
    if (!cl->awaitingRespawn) {
      char userInfo[MAX_INFO_STRING];
      Q_strlcpy(userInfo, cl->pers.userInfo, sizeof(userInfo));
      ClientUserinfoChanged(ent, userInfo);

      cl->respawn_timeout = level.time + 3_sec;
    }

    // find a spot to place us
    // SetIntermissionPoint();
    FindIntermissionPoint();

    ent->s.origin = level.intermission.origin;
    ent->client->ps.pmove.origin = level.intermission.origin;
    ent->client->ps.viewAngles = level.intermission.angles;

    cl->awaitingRespawn = true;
    cl->ps.pmove.pmType = PM_FREEZE;
    cl->ps.rdFlags = RDF_NONE;
    ent->deadFlag = false;

    MoveClientToFreeCam(ent);
    gi.linkEntity(ent);

    return;
  }

  cl->resp.ctf_state++;

  bool was_waiting_for_respawn = cl->awaitingRespawn;

  if (cl->awaitingRespawn)
    ent->svFlags &= ~SVF_NOCLIENT;

  cl->awaitingRespawn = false;
  cl->respawn_timeout = 0_ms;

  const bool preserveMarathonInventory =
      deathmatch->integer && game.marathon.active &&
      game.marathon.transitionPending;

  // deathmatch wipes most client data every spawn
  if (deathmatch->integer) {
    if (!preserveMarathonInventory) {
      cl->pers.health = 0;
    } else if (cl->pers.health <= 0) {
      const int fallbackHealth = (cl->pers.maxHealth > 0)
                                     ? cl->pers.maxHealth
                                     : 100;
      cl->pers.health = fallbackHealth;
      if (cl->pers.maxHealth < cl->pers.health)
        cl->pers.maxHealth = cl->pers.health;
    }
    savedResp = cl->resp;
    savedSess = cl->sess;
  } else {
    // [Kex] Maintain user info in singleplayer to keep the player skin.
    char userInfo[MAX_INFO_STRING];
    Q_strlcpy(userInfo, cl->pers.userInfo, sizeof(userInfo));

    if (coop->integer) {
      savedResp = cl->resp;
      savedSess = cl->sess;

      if (!P_UseCoopInstancedItems()) {
        savedResp.coopRespawn.game_help1changed = cl->pers.game_help1changed;
        savedResp.coopRespawn.game_help2changed = cl->pers.game_help2changed;
        savedResp.coopRespawn.helpChanged = cl->pers.helpChanged;
        cl->pers = savedResp.coopRespawn;
      } else {
        // fix weapon
        if (!cl->pers.weapon)
          cl->pers.weapon = cl->pers.lastWeapon;
      }
    }

    ClientUserinfoChanged(ent, userInfo);

    if (coop->integer) {
      if (savedResp.score > cl->pers.score)
        cl->pers.score = savedResp.score;
    } else {
      savedResp = client_respawn_t{};
      savedSess = client_session_t{};
      cl->sess.team = Team::Free;
      cl->ps.teamID = static_cast<int>(cl->sess.team);
    }
  }

  auto savedInitialMenu = cl->initialMenu;

  // clear everything but the persistant data
  savedPers = cl->pers;
  *cl = gclient_t{};
  cl->pers = savedPers;
  cl->resp = savedResp;
  cl->sess = savedSess;
  cl->initialMenu = savedInitialMenu;

  // on a new, fresh spawn (always in DM, clear inventory
  // or new spawns in SP/coop)
  if (!preserveMarathonInventory && cl->pers.health <= 0)
    InitClientPersistant(ent, cl);

  // fix level switch issue
  ent->client->pers.connected = true;

  // slow time will be unset here
  globals.serverFlags &= ~SERVER_FLAG_SLOW_TIME;

  // copy some data from the client to the entity
  FetchClientEntData(ent);

  // clear entity values
  ent->groundEntity = nullptr;
  ent->client = &game.clients[index];
  ent->takeDamage = true;
  ent->moveType = MoveType::Walk;
  ent->viewHeight = DEFAULT_VIEWHEIGHT;
  ent->inUse = true;
  ent->className = "player";
  ent->mass = 200;
  ent->solid = SOLID_BBOX;
  ent->deadFlag = false;
  ent->airFinished = level.time + 12_sec;
  ent->clipMask = MASK_PLAYERSOLID;
  ent->model = "players/male/tris.md2";
  ent->die = player_die;
  ent->waterLevel = WATER_NONE;
  ent->waterType = CONTENTS_NONE;
  ent->flags &= ~(FL_NO_KNOCKBACK | FL_ALIVE_KNOCKBACK_ONLY |
                  FL_NO_DAMAGE_EFFECTS | FL_SAM_RAIMI);
  ent->svFlags &= ~SVF_DEADMONSTER;
  ent->svFlags |= SVF_PLAYER;
  ent->client->pers.last_spawn_time = level.time;
  ent->client->timeResidual = level.time + 1_sec;

  ent->mins = PLAYER_MINS;
  ent->maxs = PLAYER_MAXS;

  ent->client->pers.lives = lives;
  if (G_LimitedLivesActive()) {
    cl->pers.limitedLivesStash = lives;
    cl->pers.limitedLivesPersist = true;
  } else {
    cl->pers.limitedLivesStash = 0;
    cl->pers.limitedLivesPersist = false;
  }
  if (G_LimitedLivesInCoop())
    ent->client->resp.coopRespawn.lives = lives;

  // clear playerstate values
  ent->client->ps = {};

  char val[MAX_INFO_VALUE];
  gi.Info_ValueForKey(ent->client->pers.userInfo, "fov", val, sizeof(val));
  ent->client->ps.fov =
      std::clamp((float)strtoul(val, nullptr, 10), 1.f, 160.f);

  ent->client->ps.pmove.viewHeight = ent->viewHeight;

  if (!G_ShouldPlayersCollide(false))
    ent->clipMask &= ~CONTENTS_PLAYER;

  if (cl->pers.weapon)
    cl->ps.gunIndex = gi.modelIndex(cl->pers.weapon->viewModel);
  else
    cl->ps.gunIndex = 0;
  cl->ps.gunSkin = 0;

  // clear entity state values
  ent->s.effects = EF_NONE;
  ent->s.modelIndex = MODELINDEX_PLAYER;  // will use the skin specified model
  ent->s.modelIndex2 = MODELINDEX_PLAYER; // custom gun model
  // sknum is player num and weapon number
  // weapon number will be added in changeweapon
  P_AssignClientSkinNum(ent);

  CalculateRanks();

  if (cl->resp.hasPendingGhostSpawn) {
    Vector3 ghostOrigin = cl->resp.pendingGhostOrigin;
    Vector3 ghostAngles = cl->resp.pendingGhostAngles;
    gentity_t *blockingEnt =
        G_UnsafeSpawnPosition(ghostOrigin, /*check_players=*/true);
    bool ghostSpotSafe = (blockingEnt == nullptr);

    if (!ghostSpotSafe) {
      gentity_t *geometryBlocker =
          G_UnsafeSpawnPosition(ghostOrigin, /*check_players=*/false);
      if (!geometryBlocker) {
        ghostSpotSafe = true;
      } else {
        blockingEnt = geometryBlocker;
      }
    }

    if (ghostSpotSafe) {
      spawnOrigin = ghostOrigin;
      spawnAngles = ghostAngles;
      usedGhostSpawn = true;
    } else {
      const char *sessionName = cl->sess.netName;
      const char *persistentName = cl->pers.netName;
      const char *playerName = cl->sess.netName[0]   ? sessionName
                               : cl->pers.netName[0] ? persistentName
                                                     : "player";
      const std::string safePlayerName = G_ColorResetAfter(playerName);

      std::string blockerDesc = "solid geometry";
      if (blockingEnt) {
        if (blockingEnt->client) {
          const char *occupantPersist = blockingEnt->client->pers.netName;
          const char *occupantSession = blockingEnt->client->sess.netName;
          const char *occupant = (occupantPersist[0])   ? occupantPersist
                                 : (occupantSession[0]) ? occupantSession
                                                        : nullptr;
          if (occupant && *occupant)
            blockerDesc = G_ColorResetAfter(occupant);
          else
            blockerDesc = "another player";
        } else if (blockingEnt->className) {
          blockerDesc = blockingEnt->className;
        }
      }

      gi.Com_PrintFmt(
          "Ghost respawn for {} denied at ({} {} {}); blocked by {}\n",
          safePlayerName, ghostOrigin[0], ghostOrigin[1], ghostOrigin[2],
          blockerDesc);
    }

    cl->resp.hasPendingGhostSpawn = false;
    cl->resp.pendingGhostOrigin = vec3_origin;
    cl->resp.pendingGhostAngles = vec3_origin;
  }

  ent->s.frame = 0;

  PutClientOnSpawnPoint(ent, spawnOrigin, spawnAngles);

  if (ClientIsPlaying(cl) && !eliminated && !is_landmark &&
      !cl->coopRespawn.useSquad && !usedGhostSpawn) {
    cl->spawnAngleLockAngles = spawnAngles;
    cl->spawnAngleLockUntil = level.time + (FRAME_TIME_MS * 2);
  }

  if (!is_landmark) {
    // When spawning at a map-defined point, persist the mapper-provided
    // orientation for later transitions (e.g., coop level changes) after
    // we've computed the initial delta from the previous command angles.
    cl->resp.cmdAngles = spawnAngles;
  }

  // [Paril-KEX] set up world fog & send it instantly
  ent->client->pers.wanted_fog = {world->fog.density, world->fog.color[0],
                                  world->fog.color[1], world->fog.color[2],
                                  world->fog.sky_factor};
  ent->client->pers.wanted_heightfog = {
      {world->heightfog.start_color[0], world->heightfog.start_color[1],
       world->heightfog.start_color[2], world->heightfog.start_dist},
      {world->heightfog.end_color[0], world->heightfog.end_color[1],
       world->heightfog.end_color[2], world->heightfog.end_dist},
      world->heightfog.falloff,
      world->heightfog.density};
  P_ForceFogTransition(ent, true);

  // spawn as spectator
  if (!ClientIsPlaying(cl) || eliminated) {
    FreeFollower(ent);

    MoveClientToFreeCam(ent);
    ent->client->ps.stats[STAT_SHOW_STATUSBAR] = 0;
    if (!ent->client->initialMenu.shown)
      ent->client->initialMenu.delay = level.time + 10_hz;
    ent->client->eliminated = eliminated;
    gi.linkEntity(ent);
    return;
  }
  ent->client->ps.stats[STAT_SHOW_STATUSBAR] = 1;

  // [Paril-KEX] a bit of a hack, but landmark spawns can sometimes cause
  // intersecting spawns, so we'll do a sanity check here...
  if (ent->client->coopRespawn.spawnBegin) {
    if (coop->integer) {
      gentity_t *collision = G_UnsafeSpawnPosition(ent->s.origin, true);

      if (collision) {
        gi.linkEntity(ent);

        if (collision->client) {
          // we spawned in somebody else, so we're going to change their spawn
          // position
          bool lm = false;
          SelectSpawnPoint(collision, spawnOrigin, spawnAngles, true, lm);
          PutClientOnSpawnPoint(collision, spawnOrigin, spawnAngles);
        }
        // else, no choice but to accept where ever we spawned :(
      }
    }

    // give us one (1) free fall ticket even if
    // we didn't spawn from landmark
    ent->client->landmark_free_fall = true;
  }

  gi.linkEntity(ent);

  if (!KillBox(ent, true, ModID::Telefrag_Spawn)) { // could't spawn in?
  }

  // my tribute to cash's level-specific hacks. I hope I live
  // up to his trailblazing cheese.
  if (!deathmatch->integer &&
      Q_strcasecmp(level.mapName.data(), "rboss") == 0) {
    // if you get on to rboss in single player or coop, ensure
    // the player has the nuke key. (not in DM)
    cl->pers.inventory[IT_KEY_NUKE] = 1;
  }

  // force the current weapon up
  if (Game::Has(GameFlags::Arena) && cl->pers.inventory[IT_WEAPON_RLAUNCHER])
    cl->weapon.pending = &itemList[IT_WEAPON_RLAUNCHER];
  else
    cl->weapon.pending = cl->pers.weapon;
  Change_Weapon(ent);

  if (was_waiting_for_respawn)
    G_PostRespawn(ent);
}
