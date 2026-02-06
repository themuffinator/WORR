/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

g_utilities.cpp (Game Utilities) This file is a collection of miscellaneous
utility and helper functions that are used throughout the server-side game
module. It contains common, reusable code that doesn't belong to a more specific
system like items or AI. Key Responsibilities: - Entity Searching: Provides
functions like `FindEntity`, `FindRadius`, and `PickTarget` for locating
specific entities in the game world. - Spawning and Linking: Contains the core
`Spawn` and `FreeEntity` functions that manage the entity lifecycle. - String
and Text Manipulation: Includes functions for formatting strings, such as
`TimeString` for displaying time. - Team and Gametype Logic: Provides helper
functions for team-based checks (`OnSameTeam`, `Teams_TeamName`) and gametype
validation. - World Interaction: Implements `KillBox` for killing entities
within a volume and `TouchTriggers` for activating triggers.*/

#include "../../bgame/weapon_pref_utils.hpp"
#include "../g_local.hpp"
#include "team_balance.hpp"
#include <array>
#include <cctype>
#include <chrono> // get real time
#include <ctime>
#include <string_view>


extern gentity_t *neutralObelisk;

/*
=============
LocalTimeNow

Returns the current local time using a thread-safe conversion across platforms.
=============
*/
std::tm LocalTimeNow() {
  const std::time_t now =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::tm result{};
#ifdef _WIN32
  localtime_s(&result, &now);
#else
  localtime_r(&now, &result);
#endif
  return result;
}

/*
=============
G_ColorResetAfter

Appends a white color escape so subsequent text renders in the default color.
Use for user-provided strings (e.g., player names) when composing messages.
=============
*/
std::string G_ColorResetAfter(std::string_view text) {
  if (text.empty())
    return {};

  if (text.size() >= 2 && text[text.size() - 2] == '^' &&
      text[text.size() - 1] == '7') {
    return std::string(text);
  }

  std::string out;
  out.reserve(text.size() + 2);
  out.append(text);
  out.append("^7");
  return out;
}

std::string G_ColorResetAfter(const char *text) {
  if (!text || !*text)
    return {};
  return G_ColorResetAfter(std::string_view{text});
}

static inline bool G_IsColorEscapeCode(char c) {
  if (c >= '0' && c <= '7')
    return true;
  if (c >= 'a' && c <= 'z')
    return true;
  if (c >= 'A' && c <= 'Z')
    return true;
  return false;
}

size_t G_StripColorEscapes(std::string_view text, char *out, size_t outSize) {
  if (!out || outSize == 0)
    return 0;

  const char *src = text.data();
  size_t remaining = text.size();
  char *cursor = out;
  size_t capacity = outSize;

  while (remaining && capacity > 1) {
    if (remaining >= 2 && src[0] == '^' && G_IsColorEscapeCode(src[1])) {
      src += 2;
      remaining -= 2;
      continue;
    }

    *cursor++ = *src++;
    --capacity;
    --remaining;
  }

  *cursor = '\0';
  return static_cast<size_t>(cursor - out);
}

size_t G_StripColorEscapes(const char *text, char *out, size_t outSize) {
  if (!out || outSize == 0)
    return 0;
  if (!text) {
    out[0] = '\0';
    return 0;
  }
  return G_StripColorEscapes(std::string_view{text}, out, outSize);
}

/*
=========================
CheckArenaValid
=========================
*/
bool CheckArenaValid(int arenaNum) {
  if (arenaNum <= 0)
    return false;
  if (arenaNum > level.arenaTotal)
    return false;
  return true;
}

/*
=========================
ChangeArena
=========================
*/
bool ChangeArena(int newArenaNum) {
  if (!CheckArenaValid(newArenaNum))
    return false;

  level.arenaActive = newArenaNum;

  Match_Reset();
  FindIntermissionPoint();

  return true;
}

/*
=============
FindEntity

Searches all active entities for the next one that validates the given callback.

Searches beginning at the entity after from, or the beginning if nullptr
nullptr will be returned if the end of the list is reached.
=============
*/
gentity_t *FindEntity(gentity_t *from,
                      std::function<bool(gentity_t *e)> matcher) {
  if (!from)
    from = g_entities;
  else
    from++;

  for (; from < &g_entities[globals.numEntities]; from++) {
    if (!from->inUse)
      continue;
    if (matcher(from))
      return from;
  }

  return nullptr;
}

/*
=================
FindRadius

Returns entities that have origins within a spherical area

FindRadius (origin, radius)
=================
*/
gentity_t *FindRadius(gentity_t *from, const Vector3 &org, float rad) {
  Vector3 eorg{};
  int j;

  if (!from)
    from = g_entities;
  else
    from++;
  for (; from < &g_entities[globals.numEntities]; from++) {
    if (!from->inUse)
      continue;
    if (from->solid == SOLID_NOT)
      continue;
    for (j = 0; j < 3; j++)
      eorg[j] =
          org[j] - (from->s.origin[j] + (from->mins[j] + from->maxs[j]) * 0.5f);
    if (eorg.length() > rad)
      continue;
    return from;
  }

  return nullptr;
}

/*
=============
PickTarget

Searches all active entities for the next one that holds
the matching string at fieldofs in the structure.

Searches beginning at the entity after from, or the beginning if nullptr
nullptr will be returned if the end of the list is reached.

=============
*/
constexpr size_t MAXCHOICES = 8;

gentity_t *PickTarget(const char *targetName) {
  gentity_t *choice[MAXCHOICES]{};
  gentity_t *ent = nullptr;
  int num_choices = 0;

  if (!targetName) {
    gi.Com_PrintFmt("{}: called with nullptr targetName.\n", __FUNCTION__);
    return nullptr;
  }

  while (1) {
    ent = G_FindByString<&gentity_t::targetName>(ent, targetName);
    if (!ent)
      break;
    choice[num_choices++] = ent;
    if (num_choices == MAXCHOICES)
      break;
  }

  if (!num_choices) {
    gi.Com_PrintFmt("{}: target {} not found\n", __FUNCTION__, targetName);
    return nullptr;
  }

  return choice[irandom(num_choices)];
}

static THINK(Think_Delay)(gentity_t *ent)->void {
  UseTargets(ent, ent->activator);
  FreeEntity(ent);
}

void PrintActivationMessage(gentity_t *ent, gentity_t *activator,
                            bool coop_global) {
  if (!ent || !ent->message)
    return;

  if (!activator) {
    gi.Com_PrintFmt("{}: activation message suppressed (no activator).\n",
                    __FUNCTION__);
    return;
  }

  if (activator->svFlags & SVF_MONSTER)
    return;

  if (coop_global && coop->integer)
    gi.LocBroadcast_Print(PRINT_CENTER, "$format_passthrough", ent->message);
  else
    gi.LocCenter_Print(activator, "$format_passthrough", ent->message);

  // [Paril-KEX] allow non-noisy centerprints
  if (ent->noiseIndex >= 0) {
    if (ent->noiseIndex)
      gi.sound(activator, CHAN_AUTO, ent->noiseIndex, 1, ATTN_NORM, 0);
    else
      gi.sound(activator, CHAN_AUTO, gi.soundIndex("misc/talk1.wav"), 1,
               ATTN_NORM, 0);
  }
}

void BroadcastFriendlyMessage(Team team, const char *msg) {
  for (auto ce : active_clients()) {
    if (!ClientIsPlaying(ce->client) ||
        (Teams() && ce->client->sess.team == team)) {
      gi.LocClient_Print(
          ce, PRINT_HIGH,
          G_Fmt("{}{}",
                ce->client->sess.team != Team::Spectator ? "[TEAM]: " : "", msg)
              .data());
    }
  }
}

void BroadcastTeamMessage(Team team, print_type_t level, const char *msg) {
  for (auto ce : active_clients()) {
    if (ce->client->sess.team != team)
      continue;

    gi.LocClient_Print(ce, level, msg);
  }
}

void G_MonsterKilled(gentity_t *self);

/*
==============================
UseTargets

the global "activator" should be set to the entity that initiated the firing.

If self.delay is set, a DelayedUse entity will be created that will actually
do the SUB_UseTargets after that many seconds have passed.

Centerprints any self.message to the activator.

Search for (string)targetName in all entities that
match (string)self.target and call their .use function

==============================
*/
void UseTargets(gentity_t *ent, gentity_t *activator) {
  gentity_t *t;

  if (!ent)
    return;

  if (CombatIsDisabled())
    return;

  //
  // check for a delay
  //
  if (ent->delay) {
    // create a temp object to fire at a later time
    t = Spawn();
    t->className = "DelayedUse";
    t->nextThink = level.time + GameTime::from_sec(ent->delay);
    t->think = Think_Delay;
    t->activator = activator;
    if (!activator)
      gi.Com_PrintFmt("{}: {} with no activator.\n", __FUNCTION__, *t);
    t->message = ent->message;
    t->target = ent->target;
    t->killTarget = ent->killTarget;
    return;
  }

  //
  // print the message
  //
  PrintActivationMessage(ent, activator, true);

  //
  // kill killtargets
  //
  if (ent->killTarget) {
    t = nullptr;
    while ((t = G_FindByString<&gentity_t::targetName>(t, ent->killTarget))) {
      if (t->teamMaster) {
        // if this entity is part of a chain, cleanly remove it
        if (t->flags & FL_TEAMSLAVE) {
          for (gentity_t *master = t->teamMaster; master;
               master = master->teamChain) {
            if (master->teamChain == t) {
              master->teamChain = t->teamChain;
              break;
            }
          }
        }
        // [Paril-KEX] remove teamMaster too
        else if (t->flags & FL_TEAMMASTER) {
          t->teamMaster->flags &= ~FL_TEAMMASTER;

          gentity_t *new_master = t->teamMaster->teamChain;

          if (new_master) {
            new_master->flags |= FL_TEAMMASTER;
            new_master->flags &= ~FL_TEAMSLAVE;

            for (gentity_t *m = new_master; m; m = m->teamChain)
              m->teamMaster = new_master;
          }
        }
      }

      // [Paril-KEX] if we killTarget a monster, clean up properly
      if (t->svFlags & SVF_MONSTER) {
        if (!t->deadFlag && !(t->monsterInfo.aiFlags & AI_DO_NOT_COUNT) &&
            !t->spawnFlags.has(SPAWNFLAG_MONSTER_CORPSE))
          G_MonsterKilled(t);
      }

      FreeEntity(t);

      if (!ent->inUse) {
        gi.Com_PrintFmt("{}: gentity_t was removed while using killtargets.\n",
                        __FUNCTION__);
        return;
      }
    }
  }

  //
  // fire targets
  //
  if (ent->target) {
    t = nullptr;
    while ((t = G_FindByString<&gentity_t::targetName>(t, ent->target))) {
      // doors fire area portals in a specific way
      if (!Q_strcasecmp(t->className, "func_areaportal") &&
          (!Q_strcasecmp(ent->className, "func_door") ||
           !Q_strcasecmp(ent->className, "func_door_rotating") ||
           !Q_strcasecmp(ent->className, "func_door_secret") ||
           !Q_strcasecmp(ent->className, "func_water")))
        continue;

      if (t == ent) {
        gi.Com_PrintFmt("{}: WARNING: gentity_t used itself.\n", __FUNCTION__);
      } else {
        if (t->use)
          t->use(t, ent, activator);
      }
      if (!ent->inUse) {
        gi.Com_PrintFmt("{}: gentity_t was removed while using targets.\n",
                        __FUNCTION__);
        return;
      }
    }
  }
}

/*
===============
SetMoveDir
===============
*/
void SetMoveDir(Vector3 &angles, Vector3 &moveDir) {
  static Vector3 VEC_UP = {0, -1, 0};
  static Vector3 MOVEDIR_UP = {0, 0, 1};
  static Vector3 VEC_DOWN = {0, -2, 0};
  static Vector3 MOVEDIR_DOWN = {0, 0, -1};

  if (angles == VEC_UP) {
    moveDir = MOVEDIR_UP;
  } else if (angles == VEC_DOWN) {
    moveDir = MOVEDIR_DOWN;
  } else {
    AngleVectors(angles, moveDir, nullptr, nullptr);
  }

  angles = {};
}

/*
===============
CopyString
===============
*/
char *CopyString(const char *in, int32_t tag) {
  if (!in)
    return nullptr;
  const size_t amt = strlen(in) + 1;
  char *const out = static_cast<char *>(gi.TagMalloc(amt, tag));
  Q_strlcpy(out, in, amt);
  return out;
}

/*
=============
ResetGEntity

Clears an entity for reuse while preserving persistent handles.
=============
*/
static void ResetGEntity(gentity_t *e) {
  gclient_t *client = e->client;
  int32_t spawn_count = e->spawn_count;

  std::memset(static_cast<void *>(e), 0, sizeof(*e));

  e->client = client;
  e->spawn_count = spawn_count;
}

/*
=============
InitGEntity

Initializes a game entity to a known default state before use.
=============
*/
void InitGEntity(gentity_t *e) {
  ResetGEntity(e);
  e->inUse = true;
  e->sv.init = false;
  e->className = "noClass";
  e->gravity = 1.0;
  e->s.number = e - g_entities;

  // do this before calling the spawn function so it can be overridden.
  e->gravityVector = {0.0, 0.0, -1.0};
}

/*
=================
Spawn

Either finds a free entity, or allocates a new one.
Try to avoid reusing an entity that was recently freed, because it
can cause the client to think the entity morphed into something else
instead of being removed and recreated, which can cause interpolated
angles and bad trails.
=================
*/
gentity_t *Spawn() {
  gentity_t *e = &g_entities[static_cast<size_t>(game.maxClients) + 1];
  size_t i;

  for (i = static_cast<size_t>(game.maxClients + 1); i < globals.numEntities;
       i++, e++) {
    // the first couple seconds of server time can involve a lot of
    // freeing and allocating, so relax the replacement policy
    if (!e->inUse &&
        (e->freeTime < 2_sec || level.time - e->freeTime > 500_ms)) {
      InitGEntity(e);
      return e;
    }
  }

  if (i == game.maxEntities)
    gi.Com_ErrorFmt("{}: no free entities.", __FUNCTION__);

  globals.numEntities++;
  InitGEntity(e);
  // gi.Com_PrintFmt("{}: total:{}\n", __FUNCTION__, i);
  return e;
}

/*
=================
FreeEntity

Marks the entity as free
=================
*/
THINK(FreeEntity)(gentity_t *ed)->void {
  if (ed == neutralObelisk) {
    neutralObelisk = nullptr;
  }

  // already freed
  if (!ed->inUse)
    return;

  gi.unlinkEntity(ed); // unlink from world

  if ((ed - g_entities) <= (ptrdiff_t)(game.maxClients + BODY_QUEUE_SIZE)) {
#ifdef _DEBUG
    gi.Com_PrintFmt("Tried to free special entity: {}.\n", *ed);
#endif
    return;
  }
  // gi.Com_PrintFmt("{}: removing {}\n", __FUNCTION__, *ed);

  gi.Bot_UnRegisterEntity(ed);

  int32_t id = ed->spawn_count + 1;
  memset(static_cast<void *>(ed), 0, sizeof(*ed));
  ed->s.number = ed - g_entities;
  ed->className = "freed";
  ed->freeTime = level.time;
  ed->inUse = false;
  ed->spawn_count = id;
  ed->sv.init = false;
}

/*
============
TouchTriggers
============
*/

static BoxEntitiesResult_t TouchTriggers_BoxFilter(gentity_t *hit, void *) {
  if (!hit->touch)
    return BoxEntitiesResult_t::Skip;

  return BoxEntitiesResult_t::Keep;
}

void TouchTriggers(gentity_t *ent) {
  int num;
  static gentity_t *touch[MAX_ENTITIES];
  gentity_t *hit;

  if (ent->client && ent->client->eliminated)
    ;
  else
    // dead things don't activate triggers!
    if ((ent->client || (ent->svFlags & SVF_MONSTER)) && (ent->health <= 0))
      return;

  num = gi.BoxEntities(ent->absMin, ent->absMax, touch, MAX_ENTITIES,
                       AREA_TRIGGERS, TouchTriggers_BoxFilter, nullptr);

  // be careful, it is possible to have an entity in this
  // list removed before we get to it (killtriggered)
  for (size_t i = 0; i < num; i++) {
    hit = touch[i];
    if (!hit->inUse)
      continue;
    if (!hit->touch)
      continue;
    if (ent->moveType == MoveType::FreeCam)
      if (!strstr(hit->className, "teleport"))
        continue;

    trace_t tr = null_trace;
    tr.ent = hit;
    hit->touch(hit, ent, tr, true);
  }
}

// [Paril-KEX] scan for projectiles between our movement positions
// to see if we need to collide against them
void G_TouchProjectiles(gentity_t *ent, Vector3 previous_origin) {
  struct skipped_projectile {
    gentity_t *projectile;
    int32_t spawn_count;
  };
  // a bit ugly, but we'll store projectiles we are ignoring here.
  static std::vector<skipped_projectile> skipped;

  while (true) {
    trace_t tr = gi.trace(previous_origin, ent->mins, ent->maxs, ent->s.origin,
                          ent, ent->clipMask | CONTENTS_PROJECTILE);

    if (tr.fraction == 1.0f)
      break;
    else if (!(tr.ent->svFlags & SVF_PROJECTILE))
      break;

    // always skip this projectile since certain conditions may cause the
    // projectile to not disappear immediately
    tr.ent->svFlags &= ~SVF_PROJECTILE;
    skipped.push_back({tr.ent, tr.ent->spawn_count});

    // if we're both players and it's coop, allow the projectile to "pass"
    // through
    if (ent->client && tr.ent->owner && tr.ent->owner->client &&
        !G_ShouldPlayersCollide(true))
      continue;

    G_Impact(ent, tr);
  }

  for (auto &skip : skipped)
    if (skip.projectile->inUse &&
        skip.projectile->spawn_count == skip.spawn_count)
      skip.projectile->svFlags |= SVF_PROJECTILE;

  skipped.clear();
}

/*
==============================================================================

Kill box

==============================================================================
*/

/*
=================
KillBox

Kills all entities that would touch the proposed new positioning
of ent.
=================
*/

static BoxEntitiesResult_t KillBox_BoxFilter(gentity_t *hit, void *) {
  if (!hit->solid || !hit->takeDamage || hit->solid == SOLID_TRIGGER)
    return BoxEntitiesResult_t::Skip;

  return BoxEntitiesResult_t::Keep;
}

bool KillBox(gentity_t *ent, bool from_spawning, ModID mod, bool bsp_clipping) {
  // don't telefrag as spectator or noclip player...
  if (ent->moveType == MoveType::NoClip || ent->moveType == MoveType::FreeCam)
    return true;

  contents_t mask = CONTENTS_MONSTER | CONTENTS_PLAYER;

  // [Paril-KEX] don't gib other players in coop if we're not colliding
  if (from_spawning && ent->client && CooperativeModeOn() &&
      !G_ShouldPlayersCollide(false))
    mask &= ~CONTENTS_PLAYER;

  int i, num;
  static gentity_t *touch[MAX_ENTITIES];
  gentity_t *hit;

  num = gi.BoxEntities(ent->absMin, ent->absMax, touch, MAX_ENTITIES,
                       AREA_SOLID, KillBox_BoxFilter, nullptr);

  for (i = 0; i < num; i++) {
    hit = touch[i];

    if (hit == ent)
      continue;
    else if (!hit->inUse || !hit->takeDamage || !hit->solid ||
             hit->solid == SOLID_TRIGGER || hit->solid == SOLID_BSP)
      continue;
    else if (hit->client && !(mask & CONTENTS_PLAYER))
      continue;

    if ((ent->solid == SOLID_BSP || (ent->svFlags & SVF_HULL)) &&
        bsp_clipping) {
      trace_t clip = gi.clip(ent, hit->s.origin, hit->mins, hit->maxs,
                             hit->s.origin, G_GetClipMask(hit));

      if (clip.fraction == 1.0f)
        continue;
    }

    // [Paril-KEX] don't allow telefragging of friends in coop.
    // the player that is about to be telefragged will have collision
    // disabled until another time.
    if (ent->client && hit->client && CooperativeModeOn()) {
      hit->clipMask &= ~CONTENTS_PLAYER;
      ent->clipMask &= ~CONTENTS_PLAYER;
      continue;
    }

    Damage(hit, ent, ent, vec3_origin, ent->s.origin, vec3_origin, 100000, 0,
           DamageFlags::NoProtection, mod);
  }

  return true; // all clear
}

/*--------------------------------------------------------------------------*/

const char *Teams_TeamName(Team team) {
  switch (team) {
  case Team::Red:
    return "RED";
  case Team::Blue:
    return "BLUE";
  case Team::Spectator:
    return "SPECTATOR";
  case Team::Free:
    return "FREE";
  case Team::None:
  case Team::Total:
    break;
  }
  return "NONE";
}

const char *Teams_OtherTeamName(Team team) {
  switch (team) {
  case Team::Red:
    return "BLUE";
  case Team::Blue:
    return "RED";
  case Team::None:
  case Team::Spectator:
  case Team::Free:
  case Team::Total:
    break;
  }
  return "UNKNOWN";
}

Team Teams_OtherTeam(Team team) {
  switch (team) {
  case Team::Red:
    return Team::Blue;
  case Team::Blue:
    return Team::Red;
  case Team::None:
  case Team::Spectator:
  case Team::Free:
  case Team::Total:
    break;
  }
  return Team::Spectator; // invalid value
}

/*
=================
CleanSkinName
=================
*/
static std::string SanitizeSkinComponent(std::string_view component) {
  std::string out;
  for (char c : component) {
    if (isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') {
      out += c;
    }
  }
  return out;
}

static std::string CleanSkinName(const std::string &in) {
  const size_t slashPos = in.find('/');

  if (slashPos == std::string::npos) {
    std::string clean = SanitizeSkinComponent(in);
    return clean.empty() ? "male" : clean;
  }

  std::string cleanModel =
      SanitizeSkinComponent(std::string_view(in).substr(0, slashPos));
  std::string cleanSkin =
      SanitizeSkinComponent(std::string_view(in).substr(slashPos + 1));

  if (cleanModel.empty()) {
    cleanModel = "male";
  }
  if (cleanSkin.empty()) {
    cleanSkin = "default";
  }

  return cleanModel + "/" + cleanSkin;
}

/*
=================
AssignPlayerSkin
=================
*/

constexpr const char *TEAM_RED_SKIN = "ctf_r";
constexpr const char *TEAM_BLUE_SKIN = "ctf_b";

void AssignPlayerSkin(gentity_t *ent, const std::string &skin) {
  if (!ent || !ent->client)
    return;

  int playernum = ent - g_entities - 1;

  // Sanitize the input skin
  std::string cleanSkin = CleanSkinName(skin);

  std::string_view baseSkin(cleanSkin);
  size_t slashPos = baseSkin.find_first_of('/');

  std::string modelPath;
  if (slashPos != std::string_view::npos) {
    modelPath = std::string(baseSkin.substr(0, slashPos + 1));
  } else {
    modelPath = "male/";
  }

  const char *teamSkin = nullptr;
  switch (ent->client->sess.team) {
  case Team::Red:
    teamSkin = TEAM_RED_SKIN;
    break;
  case Team::Blue:
    teamSkin = TEAM_BLUE_SKIN;
    break;
  default:
    teamSkin = nullptr;
    break;
  }

  std::string finalSkin;
  if (teamSkin) {
    finalSkin = G_Fmt("{}\\{}{}\\default", ent->client->sess.netName, modelPath,
                      teamSkin);
  } else {
    finalSkin = G_Fmt("{}\\{}\\default", ent->client->sess.netName, cleanSkin);
  }

  gi.configString(CS_PLAYERSKINS + playernum, finalSkin.c_str());
}

/*
===================
G_AdjustPlayerScore
===================
*/
void G_AdjustPlayerScore(gclient_t *cl, int32_t offset, bool adjust_team,
                         int32_t team_offset) {
  if (!cl)
    return;

  if (ScoringIsDisabled())
    return;

  if (level.intermission.queued)
    return;

  if (offset || team_offset) {
    cl->resp.score += offset;
    // gi.Com_PrintFmt("G_AdjustPlayerScore: player: {}\n", offset);

    if (adjust_team && team_offset) {
      // G_AdjustTeamScore(cl->sess.team, team_offset);

      if (Teams() && Game::IsNot(GameType::RedRover)) {

        if (cl->sess.team == Team::Red)
          level.teamScores[static_cast<int>(Team::Red)] += offset;
        else if (cl->sess.team == Team::Blue)
          level.teamScores[static_cast<int>(Team::Blue)] += offset;

        // gi.Com_PrintFmt("G_AdjustPlayerScore: team: {}\n", offset);
      }
    }

    CalculateRanks();
  }
}

/*
===================
Horde_AdjustPlayerScore
===================
*/
void Horde_AdjustPlayerScore(gclient_t *cl, int32_t offset) {
  if (Game::IsNot(GameType::Horde))
    return;
  if (!cl || !cl->pers.connected)
    return;

  if (ScoringIsDisabled())
    return;

  G_AdjustPlayerScore(cl, offset, false, 0);
}

/*
===================
G_SetPlayerScore
===================
*/
void G_SetPlayerScore(gclient_t *cl, int32_t value) {
  if (!cl)
    return;

  if (ScoringIsDisabled())
    return;

  cl->resp.score = value;
  CalculateRanks();
}

/*
===================
G_AdjustTeamScore
===================
*/
void G_AdjustTeamScore(Team team, int32_t offset) {
  if (ScoringIsDisabled())
    return;

  if (!Teams() || Game::Is(GameType::RedRover))
    return;

  if (team == Team::Red)
    level.teamScores[static_cast<int>(Team::Red)] += offset;
  else if (team == Team::Blue)
    level.teamScores[static_cast<int>(Team::Blue)] += offset;
  else
    return;
  CalculateRanks();
}

/*
===================
G_SetTeamScore
===================
*/
void G_SetTeamScore(Team team, int32_t value) {
  if (ScoringIsDisabled())
    return;

  if (!Teams() || Game::Is(GameType::RedRover))
    return;

  if (team == Team::Red)
    level.teamScores[static_cast<int>(Team::Red)] = value;
  else if (team == Team::Blue)
    level.teamScores[static_cast<int>(Team::Blue)] = value;
  else
    return;
  CalculateRanks();
}

/*
===================
PlaceString
===================
*/
const char *PlaceString(int rank) {
  static char str[64];

  const char *prefix = "";
  if (rank & RANK_TIED_FLAG) {
    rank &= ~RANK_TIED_FLAG;
    prefix = "Tied for ";
  }

  static constexpr const char *const suffixTable[10] = {
      "th", "st", "nd", "rd", "th", "th", "th", "th", "th", "th"};

  const char *suffix = "th";
  int mod100 = rank % 100;

  if (mod100 < 11 || mod100 > 13) {
    suffix = suffixTable[rank % 10];
  }

  snprintf(str, sizeof(str), "%s%d%s", prefix, rank, suffix);
  str[sizeof(str) - 1] = '\0'; // Force null-termination

  return str;
}

/*
=================
ItemSpawnsEnabled
=================
*/
bool ItemSpawnsEnabled() {
  if (g_no_items->integer)
    return false;
  if (g_instaGib->integer || g_nadeFest->integer)
    return false;
  if (Game::Has(GameFlags::Arena))
    return false;
  return true;
}

/*
=================
LocBuildBoxPoints
=================
*/
static void LocBuildBoxPoints(Vector3 (&p)[8], const Vector3 &org,
                              const Vector3 &mins, const Vector3 &maxs) {
  // Bottom
  p[0] = org + mins;
  p[1] = p[0];
  p[1][0] += (maxs[0] - mins[0]);
  p[2] = p[0];
  p[2][1] += (maxs[1] - mins[1]);
  p[3] = p[0];
  p[3][0] += (maxs[0] - mins[0]);
  p[3][1] += (maxs[1] - mins[1]);
  // Top
  p[4] = org + maxs;
  p[5] = p[4];
  p[5][0] -= (maxs[0] - mins[0]);
  p[6] = p[4];
  p[6][1] -= (maxs[1] - mins[1]);
  p[7] = p[4];
  p[7][0] -= (maxs[0] - mins[0]);
  p[7][1] -= (maxs[1] - mins[1]);
}

/*
=================
LocCanSee
=================
*/
bool LocCanSee(gentity_t *targetEnt, gentity_t *sourceEnt) {
  if (!targetEnt || !sourceEnt)
    return false;

  if (targetEnt->moveType == MoveType::Push)
    return false; // bmodels not supported

  Vector3 targpoints[8];
  LocBuildBoxPoints(targpoints, targetEnt->s.origin, targetEnt->mins,
                    targetEnt->maxs);

  Vector3 viewpoint = sourceEnt->s.origin;
  viewpoint[2] += sourceEnt->viewHeight;

  for (int i = 0; i < 8; i++) {
    trace_t trace = gi.traceLine(viewpoint, targpoints[i], sourceEnt,
                                 CONTENTS_MIST | MASK_WATER | MASK_SOLID);
    if (trace.fraction == 1.0f)
      return true; // Early exit if any point is visible
  }

  return false;
}

/*
=================
Teams
=================
*/
bool Teams() {
  return Game::Has(GameFlags::Teams);
  // return Game::Is(GameType::CaptureTheFlag) ||
  // Game::Is(GameType::TeamDeathmatch) || Game::Is(GameType::FreezeTag) ||
  // Game::Is(GameType::ClanArena) || Game::Is(GameType::CaptureStrike) ||
  // Game::Is(GameType::RedRover);
}

/*
=================
TimeString
=================
*/
const char *TimeString(int msec, bool showMilliseconds, bool state) {
  static char timeString[32];

  if (state) {
    if (level.matchState < MatchState::Countdown)
      return "WARMUP";
    if (level.intermission.queued || level.intermission.time)
      return "MATCH END";
  }

  int absMs = (msec >= 0) ? msec : -msec;
  int totalSeconds = absMs / 1000;
  int milliseconds = absMs % 1000;

  int hours = totalSeconds / 3600;
  int mins = (totalSeconds % 3600) / 60;
  int seconds = totalSeconds % 60;

  if (showMilliseconds) {
    if (hours > 0) {
      snprintf(timeString, sizeof(timeString), "%s%d:%02d:%02d.%03d",
               (msec < 0 ? "-" : ""), hours, mins, seconds, milliseconds);
    } else {
      snprintf(timeString, sizeof(timeString), "%s%02d:%02d.%03d",
               (msec < 0 ? "-" : ""), mins, seconds, milliseconds);
    }
  } else {
    if (hours > 0) {
      snprintf(timeString, sizeof(timeString), "%s%d:%02d:%02d",
               (msec < 0 ? "-" : ""), hours, mins, seconds);
    } else {
      snprintf(timeString, sizeof(timeString), "%s%02d:%02d",
               (msec < 0 ? "-" : ""), mins, seconds);
    }
  }

  timeString[sizeof(timeString) - 1] = '\0'; // Force null-termination
  return timeString;
}

/*
=================
StringToTeamNum
=================
*/
Team StringToTeamNum(const char *in) {
  struct {
    const char *name;
    Team team;
  } mappings[] = {
      {"spectator", Team::Spectator},
      {"s", Team::Spectator},
      {"auto", Team::None}, // special case handled separately
      {"a", Team::None},    // special case handled separately
      {"blue", Team::Blue},
      {"b", Team::Blue},
      {"red", Team::Red},
      {"r", Team::Red},
      {"free", Team::Free},
      {"f", Team::Free},
  };

  if (!in || !*in) {
    gi.Com_Print("StringToTeamNum: Team::None returned early.\n");
    return Team::None;
  }
  for (const auto &map : mappings) {
    if (!Q_strcasecmp(in, map.name)) {
      if (map.team == Team::None) {
        return PickTeam(-1); // 'auto' special case
      }
      if (!Teams()) {
        // Only allow free-for-all team if not in team mode
        if (map.team == Team::Free || map.team == Team::Spectator) {
          return map.team;
        }
        // Ignore team picks if no teams
        return Team::None;
      }
      if (map.team == Team::Free) {
        return PickTeam(-1);
      }
      // Normal team return
      return map.team;
    }
  }
  return Team::None;
}

/*
=================
InAMatch
=================
*/
bool InAMatch() {
  if (!deathmatch->integer)
    return false;
  if (level.intermission.queued)
    return false;
  if (level.matchState == MatchState::In_Progress)
    return true;

  return false;
}

/*
=================
CombatIsDisabled
=================
*/
bool CombatIsDisabled() {
  if (!deathmatch->integer)
    return false;
  if (level.intermission.queued)
    return true;
  if (level.intermission.time)
    return true;
  if (level.matchState == MatchState::Countdown)
    return true;
  if (Game::Has(GameFlags::Rounds) &&
      level.matchState == MatchState::In_Progress) {
    // added round ended to allow gibbing etc. at end of rounds
    // scoring to be explicitly disabled during this time
    if (level.roundState == RoundState::Countdown &&
        (Game::IsNot(GameType::Horde)))
      return true;
  }
  if (level.timeoutActive)
    return true;
  return false;
}

/*
=================
ItemPickupsAreDisabled
=================
*/
bool ItemPickupsAreDisabled() {
  if (!deathmatch->integer)
    return false;
  if (level.intermission.queued)
    return true;
  if (level.intermission.time)
    return true;
  if (level.matchState == MatchState::Countdown)
    return true;
  return false;
}

/*
=================
ScoringIsDisabled
=================
*/
bool ScoringIsDisabled() {
  if (level.matchState != MatchState::In_Progress)
    return true;
  if (Game::Is(GameType::None))
    return true;
  if (CombatIsDisabled())
    return true;
  if (Game::Has(GameFlags::Rounds) &&
      level.roundState != RoundState::In_Progress)
    return true;
  if (level.intermission.queued)
    return true;
  return false;
}

/*
=================
GametypeStringToIndex
=================
*/
GameType GametypeStringToIndex(std::string_view input) {
  // Iterate through each defined game mode once.
  for (const auto &mode : GAME_MODES) {
    // Use an efficient, case-insensitive comparison that avoids memory
    // allocation.
    if (Game::AreStringsEqualIgnoreCase(input, mode.short_name) ||
        Game::AreStringsEqualIgnoreCase(input, mode.long_name) ||
        Game::AreStringsEqualIgnoreCase(input, mode.spawn_name)) {
      return mode.type;
    }
  }

  // Return a default value if no match is found.
  return GameType::None;
}

/*
=================
GametypeIndexToString
=================
*/
std::string_view GametypeIndexToString(GameType gametype) {
  const auto type_value = static_cast<int>(gametype);

  if (type_value < static_cast<int>(GameType::None) ||
      type_value >= static_cast<int>(GameType::Total))
    return "NONE";

  return Game::GetInfo(type_value).short_name_upper;
}

/*
=================
GametypeOptionList
=================
*/
std::string GametypeOptionList() {
  std::stringstream ss;
  ss << "<";

  // Iterate through all playable gametypes.
  bool first = true;
  for (const auto &info : GAME_MODES) {
    if (info.type == GameType::None)
      continue;
    if (!first)
      ss << "|";
    ss << info.short_name;
    first = false;
  }

  ss << ">";
  return ss.str();
}

/*
=================
BroadcastReadyReminderMessage
=================
*/
void BroadcastReadyReminderMessage() {
  for (auto ec : active_players()) {
    if (!ClientIsPlaying(ec->client))
      continue;
    if (ec->client->sess.is_a_bot)
      continue;
    if (ec->client->pers.readyStatus)
      continue;
    // gi.LocCenter_Print(ec, "$g_sgame_auto_b04903a16097");
    gi.LocCenter_Print(ec, "$g_sgame_auto_f2847a84ee65"
                           "Ready.\n.MATCH IS IN WARMUP\nYou are NOT ready.");
  }
}

/*
=================
TeleporterVelocity
=================
*/
void TeleporterVelocity(gentity_t *ent, gvec3_t angles) {
  float len = ent->velocity.length();
  ent->velocity[_Z] = 0;
  AngleVectors(angles, ent->velocity, NULL, NULL);
  ent->velocity *= len;

  ent->client->ps.pmove.pmTime = 160; // hold time
  ent->client->ps.pmove.pmFlags |= PMF_TIME_KNOCKBACK;
}

/*
=================
TeleportPlayer
=================
*/
void TeleportPlayer(gentity_t *player, Vector3 origin, Vector3 angles) {
  if (!player || !player->client)
    return;

  Weapon_Grapple_DoReset(player->client);

  // unlink to make sure it can't possibly interfere with KillBox
  gi.unlinkEntity(player);

  player->s.origin = origin;
  player->s.oldOrigin = origin;
  player->s.origin[_Z] += 10;

  TeleporterVelocity(player, angles);

  // set angles
  player->client->ps.pmove.deltaAngles =
      angles - player->client->resp.cmdAngles;

  player->s.angles = {};
  player->client->ps.viewAngles = {};
  player->client->vAngle = {};
  AngleVectors(player->client->vAngle, player->client->vForward, nullptr,
               nullptr);

  gi.linkEntity(player);

  // kill anything at the destination
  KillBox(player, !!player->client);

  // destroy nearby mines
  G_ExplodeNearbyMinesSafe(player->s.origin, 202.0f, player);

  // [Paril-KEX] move sphere, if we own it
  if (player->client->ownedSphere) {
    gentity_t *sphere = player->client->ownedSphere;
    sphere->s.origin = player->s.origin;
    sphere->s.origin[_Z] = player->absMax[2];
    sphere->s.angles[YAW] = player->s.angles[YAW];
    gi.linkEntity(sphere);
  }
}

/*
=================
TeleportPlayerToRandomSpawnPoint
=================
*/
void TeleportPlayerToRandomSpawnPoint(gentity_t *ent, bool fx) {
  if (!ent || !ent->client)
    return;

  bool valid_spawn = false;
  Vector3 spawn_origin, spawn_angles;
  bool is_landmark = false;

  valid_spawn =
      SelectSpawnPoint(ent, spawn_origin, spawn_angles, true, is_landmark);

  if (!valid_spawn)
    return;

  TeleportPlayer(ent, spawn_origin, spawn_angles);

  ent->s.event = fx ? EV_PLAYER_TELEPORT : EV_OTHER_TELEPORT;
}

/*
=================
CooperativeModeOn
=================
*/
bool CooperativeModeOn() { return coop->integer || Game::Is(GameType::Horde); }

bool G_LimitedLivesInCoop() {
  return CooperativeModeOn() && g_coop_enable_lives->integer;
}

bool G_LimitedLivesInLMS() {
  return Game::Is(GameType::LastManStanding) ||
         Game::Is(GameType::LastTeamStanding);
}

bool G_LimitedLivesActive() {
  return G_LimitedLivesInCoop() || G_LimitedLivesInLMS();
}

int G_LimitedLivesMax() {
  if (G_LimitedLivesInCoop()) {
    int lives = g_coop_num_lives->integer;
    if (lives < 0)
      lives = 0;
    return lives + 1;
  }

  if (G_LimitedLivesInLMS()) {
    int lives = g_lms_num_lives->integer;
    if (lives < 0)
      lives = 0;
    return lives + 1;
  }

  return 0;
}

/*
=================
ClientEntFromString
=================
*/
gentity_t *ClientEntFromString(const char *in) {
  if (!in)
    return nullptr;

  // check by nick first
  char sanitizedInput[MAX_INFO_STRING] = {};
  G_StripColorEscapes(in, sanitizedInput, sizeof(sanitizedInput));
  if (*sanitizedInput != '\0') {
    for (auto ec : active_clients()) {
      char sanitizedName[MAX_INFO_STRING] = {};
      G_StripColorEscapes(ec->client->sess.netName, sanitizedName,
                          sizeof(sanitizedName));
      if (!strcmp(sanitizedInput, sanitizedName))
        return ec;
    }
  }

  // otherwise check client num
  uint32_t num = strtoul(sanitizedInput, nullptr, 10);
  if (num >= 0 && num < game.maxClients)
    return &g_entities[&game.clients[num] - game.clients + 1];

  return nullptr;
}

/*
=================
RS_IndexFromString
=================
*/
Ruleset RS_IndexFromString(const char *in) {
  if (!in || !*in)
    return Ruleset::None;

  std::string_view input{in};

  for (size_t i = 1; i < Ruleset::Count(); ++i) {
    // Check all aliases
    for (const auto &alias : rs_short_name[i]) {
      if (!alias.empty() && Q_strcasecmp(input.data(), alias.data()) == 0)
        return Ruleset(i);
    }

    // Check long name
    if (rs_long_name[i] && *rs_long_name[i]) {
      if (Q_strcasecmp(input.data(), rs_long_name[i]) == 0)
        return Ruleset(i);
    }
  }

  return Ruleset::None;
}

/*
===============
AnnouncerSound
===============
*/
void AnnouncerSound(gentity_t *announcer, std::string_view soundKey) {
  if (soundKey.empty())
    return;

  if (!deathmatch || !deathmatch->integer)
    return;

  const std::string path = G_Fmt("vo/{}.wav", soundKey).data();
  const int idx = gi.soundIndex(path.c_str());

  constexpr soundchan_t SOUND_FLAGS =
      static_cast<soundchan_t>(CHAN_RELIABLE | CHAN_NO_PHS_ADD | CHAN_AUX);
  constexpr float VOLUME = 1.0f;
  constexpr float ATTENUATION = ATTN_NONE;
  constexpr uint32_t DUPE_KEY = 0;
  const float TIME_OFFSET = 0;
  constexpr GameTime ANNOUNCER_GAP = 750_ms;
  const GameTime nextTime = level.time + ANNOUNCER_GAP;

  if (announcer == world) {
    gi.positionedSound(world->s.origin, world, SOUND_FLAGS, idx, VOLUME,
                       ATTENUATION, TIME_OFFSET);
    for (auto *targetEnt : active_clients()) {
      if (!targetEnt->client)
        continue;
      if (targetEnt->client->pers.announcerNextTime < nextTime)
        targetEnt->client->pers.announcerNextTime = nextTime;
    }
    return;
  }

  for (auto *targetEnt : active_clients()) {
    auto *cl = targetEnt->client;
    bool hear = false;

    if (!ClientIsPlaying(cl)) {
      hear = (cl->follow.target == announcer);
    } else {
      hear = (targetEnt == announcer && !cl->sess.is_a_bot);
    }

    if (!hear)
      continue;

    gi.localSound(targetEnt, SOUND_FLAGS, idx, VOLUME, ATTENUATION, TIME_OFFSET,
                  DUPE_KEY);
    if (cl->pers.announcerNextTime < nextTime)
      cl->pers.announcerNextTime = nextTime;
  }
}

/*
===============
CreateSpawnPad
===============
*/
void CreateSpawnPad(gentity_t *ent) {
  if (level.no_dm_spawnpads || level.arenaTotal)
    return;

  if (notRS(Quake2))
    return;

  if (level.isN64)
    return;

  if (!match_allowSpawnPads->integer)
    return;

  if (!ItemSpawnsEnabled() || Game::Is(GameType::Horde))
    return;

  gi.setModel(ent, "models/objects/dmspot/tris.md2");
  ent->s.skinNum = 0;
  ent->solid = SOLID_BBOX;
  ent->clipMask |= MASK_SOLID;

  ent->mins = {-32, -32, -24};
  ent->maxs = {32, 32, -16};
  gi.linkEntity(ent);
}

/*
===============
LogAccuracyHit
===============
*/
bool LogAccuracyHit(gentity_t *target, gentity_t *attacker) {
  if (!target->takeDamage)
    return false;

  if (target == attacker)
    return false;

  if (!attacker->client)
    return false;

  if (deathmatch->integer && !target->client)
    return false;

  if (target->health <= 0)
    return false;

  if (OnSameTeam(target, attacker))
    return false;

  return true;
}

/*
==================
G_LogEvent
==================
*/
void G_LogEvent(std::string str) {
  MatchEvent ev;

  if (level.matchState < MatchState::Countdown) {
    return;
  }
  if (str.empty()) {
    gi.Com_ErrorFmt("{}: empty event string.", __FUNCTION__);
    return;
  }
  ev.time = level.time - level.levelStartTime;
  ev.eventStr = str;
  try {
    std::lock_guard<std::mutex> logGuard(level.matchLogMutex);
    if (!level.match.eventLog.capacity()) {
      level.match.eventLog.reserve(2048);
    }
    level.match.eventLog.push_back(std::move(ev));
  } catch (const std::exception &e) {
    gi.Com_ErrorFmt("eventLog push_back failed: {}", e.what());
  }
}

void ApplyGravityLotto() {
  if (!g_gravity_lotto || !g_gravity_lotto->integer)
    return;

  // Random gravity between 100 and 2000
  constexpr int kGravityMin = 100;
  constexpr int kGravityMax = 2000;
  const int newGravity = irandom(kGravityMin, kGravityMax + 1);

  gi.cvarSet("g_gravity", G_Fmt("{}", newGravity).data());

  // Announce to all players
  gi.LocBroadcast_Print(PRINT_HIGH, "$g_sgame_auto_6e168f11d447",
                        newGravity);
}

/*
==================
GT_SetLongName
==================
*/
void GT_SetLongName() {
  struct {
    cvar_t *cvar;
    const char *prefix;
  } suffixModes[] = {
      {g_instaGib, "Insta"},   {g_vampiric_damage, "Vampiric"},
      {g_frenzy, "Frenzy"},    {g_nadeFest, "NadeFest"},
      {g_quadhog, "Quad Hog"}, {g_gravity_lotto, "Gravity Lotto"},
  };

  const char *prefix = nullptr;
  const char *base = nullptr;
  bool useShort = false;

  if (!deathmatch->integer) {
    base = coop->integer ? "Co-op" : "Single Player";
  } else {
    for (const auto &mod : suffixModes) {
      if (mod.cvar && mod.cvar->integer) {
        prefix = mod.prefix;
        useShort = true;
        break;
      }
    }

    if (g_gametype->integer >= 0 &&
        g_gametype->integer < static_cast<int>(GameType::Total)) {
      base = useShort ? Game::GetCurrentInfo().short_name_upper.data()
                      : Game::GetCurrentInfo().long_name.data();
    } else {
      base = "Unknown";
    }
  }

  std::string longName;

  if (prefix && base) {
    if (std::strcmp(base, "FFA") == 0 && std::strcmp(prefix, "Insta") == 0) {
      longName = "InstaGib";
    } else {
      longName = fmt::format("{}-{}", prefix, base);
    }
  } else if (base) {
    longName = base;
  } else {
    longName = "Unknown Gametype";
  }

  // Safely copy to level.gametype_name
  std::strncpy(level.gametype_name.data(), longName.c_str(),
               level.gametype_name.size() - 1);
  level.gametype_name.back() = '\0';
}

/*
=================
HandleLeadChanges

Detect changes in individual player rank
=================
*/
static void HandleLeadChanges() {
  for (auto ec : active_players()) {
    gclient_t *cl = ec->client;
    int newRank = cl->pers.currentRank;
    int previousRank = cl->pers.previousRank;

    bool newTied = (newRank & RANK_TIED_FLAG);
    bool oldTied = (previousRank & RANK_TIED_FLAG);

    newRank &= ~RANK_TIED_FLAG;
    previousRank &= ~RANK_TIED_FLAG;

    if (newRank == previousRank)
      continue;

    if (newRank == 0) {
      // Now in first place
      if (previousRank != 0 || oldTied != newTied)
        AnnouncerSound(ec, newTied ? "lead_tied" : "lead_taken");

      // Update followers
      for (auto spec : active_clients()) {
        if (!ClientIsPlaying(spec->client) &&
            spec->client->sess.pc.follow_leader &&
            spec->client->follow.target != ec) {
          spec->client->follow.queuedTarget = ec;
          spec->client->follow.queuedTime = level.time;
        }
      }
    } else if (previousRank == 0) {
      // Lost lead
      AnnouncerSound(ec, "lead_lost");
    }
  }
}

/*
=================
HandleTeamLeadChanges

Detect changes in team lead state
=================
*/
static void HandleTeamLeadChanges() {
  int previousRank = 2; // 2 = tied, 0 = red leads, 1 = blue leads
  int newRank = 2;

  if (level.teamOldScores[static_cast<int>(Team::Red)] >
      level.teamOldScores[static_cast<int>(Team::Blue)])
    previousRank = 0;
  else if (level.teamOldScores[static_cast<int>(Team::Blue)] >
           level.teamOldScores[static_cast<int>(Team::Red)])
    previousRank = 1;

  if (level.teamScores[static_cast<int>(Team::Red)] >
      level.teamScores[static_cast<int>(Team::Blue)])
    newRank = 0;
  else if (level.teamScores[static_cast<int>(Team::Blue)] >
           level.teamScores[static_cast<int>(Team::Red)])
    newRank = 1;

  if (previousRank != newRank) {
    if (previousRank == 2 && newRank != 2) {
      // A team just took the lead
      AnnouncerSound(world, newRank == 0 ? "red_leads" : "blue_leads");
    } else if (previousRank != 2 && newRank == 2) {
      // Now tied
      AnnouncerSound(world, "teams_tied");
    }
  }

  // Update old scores for next comparison
  level.teamOldScores[static_cast<int>(Team::Red)] =
      level.teamScores[static_cast<int>(Team::Red)];
  level.teamOldScores[static_cast<int>(Team::Blue)] =
      level.teamScores[static_cast<int>(Team::Blue)];
}

/*
=============
CalculateRanks
=============
*/
void CalculateRanks() {
  if (level.restarted)
    return;

  const bool teams = Teams();
  constexpr uint8_t kNoFollow = 0xFF;

  // Reset counters
  level.pop = {};
  level.follow1 = level.follow2 = kNoFollow;

  level.sortedClients.fill(-1);

  // Phase 1: Gather active clients
  for (auto ec : active_clients()) {
    gclient_t *cl = ec->client;
    int clientNum = cl - game.clients;

    level.sortedClients[level.pop.num_connected_clients++] = clientNum;
    if (cl->sess.consolePlayer)
      level.pop.num_console_clients++;

    if (!ClientIsPlaying(cl)) {
      if (g_allowSpecVote->integer)
        level.pop.num_voting_clients++;
      continue;
    }

    level.pop.num_nonspectator_clients++;
    level.pop.num_playing_clients++;

    if (!cl->sess.is_a_bot) {
      level.pop.num_playing_human_clients++;
      level.pop.num_voting_clients++;
    }

    if (level.follow1 == kNoFollow)
      level.follow1 = clientNum;
    else if (level.follow2 == kNoFollow)
      level.follow2 = clientNum;

    if (teams) {
      const bool eliminatedFromLives = ClientIsEliminatedFromLimitedLives(cl);

      if (cl->sess.team == Team::Red) {
        level.pop.num_playing_red++;
        if (cl->pers.health > 0)
          level.pop.num_living_red++;
        else if (cl->eliminated || eliminatedFromLives)
          level.pop.num_eliminated_red++;
      } else {
        level.pop.num_playing_blue++;
        if (cl->pers.health > 0)
          level.pop.num_living_blue++;
        else if (cl->eliminated || eliminatedFromLives)
          level.pop.num_eliminated_blue++;
      }
    }
  }

  // Phase 2: Collect valid and unique client indices
  std::array<uint16_t, MAX_CLIENTS> sorted{}; // or int16_t
  std::bitset<MAX_CLIENTS> used{};
  int sortedCount = 0;

  for (auto ec : active_clients()) {
    // Convert entity number (1..maxClients) to 0-based client index
    int clientNum = ec->s.number - 1; // FIX: was ec->s.number
    if (clientNum < 0 || clientNum >= MAX_CLIENTS)
      continue;

    if (used[clientNum])
      continue;

    used.set(clientNum);
    sorted[sortedCount++] = static_cast<uint16_t>(clientNum);
  }

  // Sort validated list
  std::sort(sorted.begin(), sorted.begin() + sortedCount, [](int a, int b) {
    gclient_t *ca = &game.clients[a];
    gclient_t *cb = &game.clients[b];

    if (!ca->pers.connected)
      return false;
    if (!cb->pers.connected)
      return true;

    bool caPlaying = ClientIsPlaying(ca);
    bool cbPlaying = ClientIsPlaying(cb);

    if (!caPlaying && !cbPlaying) {
      if (ca->sess.matchQueued && cb->sess.matchQueued) {
        const uint64_t caTicket = ca->sess.duelQueueTicket;
        const uint64_t cbTicket = cb->sess.duelQueueTicket;
        if (caTicket && cbTicket && caTicket != cbTicket)
          return caTicket < cbTicket;
        if (ca->sess.teamJoinTime != cb->sess.teamJoinTime)
          return ca->sess.teamJoinTime < cb->sess.teamJoinTime;
      }
      if (ca->sess.matchQueued)
        return true;
      if (cb->sess.matchQueued)
        return false;

      return ca->sess.teamJoinTime < cb->sess.teamJoinTime;
    }
    if (!caPlaying)
      return false;
    if (!cbPlaying)
      return true;

    const int scoreA = ClientScoreForStandings(ca);
    const int scoreB = ClientScoreForStandings(cb);

    if (scoreA != scoreB)
      return scoreA > scoreB;

    return ca->sess.teamJoinTime < cb->sess.teamJoinTime;
  });

  // Write back to level.sortedClients
  level.sortedClients.fill(-1);

  for (int i = 0; i < sortedCount; ++i)
    level.sortedClients[i] = sorted[i];

  level.pop.num_connected_clients = sortedCount;

  // Phase 3: Assign ranks
  if (teams && Game::IsNot(GameType::RedRover)) {
    for (size_t i = 0; i < level.pop.num_connected_clients; ++i) {
      gclient_t *cl = &game.clients[level.sortedClients[i]];

      if (level.teamScores[static_cast<int>(Team::Red)] ==
          level.teamScores[static_cast<int>(Team::Blue)])
        cl->pers.currentRank = 2; // tied
      else if (level.teamScores[static_cast<int>(Team::Red)] >
               level.teamScores[static_cast<int>(Team::Blue)])
        cl->pers.currentRank = 0; // red leads
      else
        cl->pers.currentRank = 1; // blue leads
    }
  } else {
    int lastScore = -99999;
    int currentRank = 0;

    for (size_t i = 0; i < level.pop.num_playing_clients; ++i) {
      gclient_t *cl = &game.clients[level.sortedClients[i]];
      cl->pers.previousRank = cl->pers.currentRank;

      const int score = ClientScoreForStandings(cl);

      if (score != lastScore) {
        currentRank = i;
        cl->pers.currentRank = currentRank;
      } else {
        cl->pers.currentRank = currentRank | RANK_TIED_FLAG;
        if (i > 0) {
          game.clients[level.sortedClients[i - 1]].pers.currentRank =
              currentRank | RANK_TIED_FLAG;
        }
      }

      lastScore = score;
    }
  }

  // Phase 4: Handle "no players" time
  if (!level.pop.num_playing_clients && !level.no_players_time)
    level.no_players_time = level.time;
  else if (level.pop.num_playing_clients)
    level.no_players_time = 0_ms;

  level.warmupNoticeTime = level.time;

  const bool has_top_playing_client =
      level.pop.num_playing_clients > 0 && level.sortedClients[0] >= 0;

  // Phase 5: Frag limit warnings
  if (level.matchState == MatchState::In_Progress &&
      Game::Has(GameFlags::Frags) && has_top_playing_client) {
    if (fragLimit->integer > 3) {
      int leadScore =
          ClientScoreForStandings(&game.clients[level.sortedClients[0]]);
      int scoreDiff = fragLimit->integer - leadScore;

      if (scoreDiff > 0 && scoreDiff <= 3 &&
          !level.fragWarning[scoreDiff - 1]) {
        AnnouncerSound(
            world,
            G_Fmt("{}_frag{}", scoreDiff, scoreDiff > 1 ? "s" : "").data());
        level.fragWarning[scoreDiff - 1] = true;
        CheckDMExitRules();
        return;
      }
    }
  }

  // Phase 6: Lead/tied/lost sounds
  if (level.matchState == MatchState::In_Progress) {
    if (!teams && has_top_playing_client &&
        ClientScoreForStandings(&game.clients[level.sortedClients[0]]) > 0) {
      HandleLeadChanges();
    } else if (teams && Game::Has(GameFlags::Frags)) {
      HandleTeamLeadChanges();
    }
  }

  CheckDMExitRules();
}

/*
=============
TimeStamp
=============
*/
std::string TimeStamp() {
  const std::tm ltime = LocalTimeNow();

  return G_Fmt("{}-{:02}-{:02} {:02}:{:02}:{:02}", 1900 + ltime.tm_year,
               ltime.tm_mon + 1, ltime.tm_mday, ltime.tm_hour, ltime.tm_min,
               ltime.tm_sec)
      .data();
}

/*
=============
FileTimeStamp
=============
*/
std::string FileTimeStamp() {
  const std::tm ltime = LocalTimeNow();

  return G_Fmt("{}-{:02}-{:02}_{:02}-{:02}-{:02}", 1900 + ltime.tm_year,
               ltime.tm_mon + 1, ltime.tm_mday, ltime.tm_hour, ltime.tm_min,
               ltime.tm_sec)
      .data();
}

/*
=============
DateStamp
=============
*/
std::string DateStamp() {
  const std::tm ltime = LocalTimeNow();

  return G_Fmt("{}-{:02}-{:02}", 1900 + ltime.tm_year, ltime.tm_mon + 1,
               ltime.tm_mday)
      .data();
}

/*
=============
FormatDuration
=============
*/
std::string FormatDuration(int seconds) {
  int hours = seconds / 3600;
  seconds %= 3600;

  int minutes = seconds / 60;
  seconds %= 60;

  if (hours > 0)
    return fmt::format("{}h {}m {}s", hours, minutes, seconds);
  else if (minutes > 0)
    return fmt::format("{}m {}s", minutes, seconds);
  else
    return fmt::format("{}s", seconds);
}

/*
=============
GetWeaponIndexByAbbrev
Returns Weapon::None if not found.
=============
*/
Weapon GetWeaponIndexByAbbrev(const std::string &abbr) {
  const std::string query = NormalizeWeaponAbbreviation(abbr);
  if (auto weapon = ParseNormalizedWeaponAbbreviation(query)) {
    return *weapon;
  }
  return Weapon::None;
}

/*
=============
GetCurrentRealTimeMillis

Returns the current real-world time in milliseconds since UNIX epoch.
=============
*/
int64_t GetCurrentRealTimeMillis() {
  auto now = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             now.time_since_epoch())
      .count();
}

/*
=============
GetRealTimeSeconds
=============
*/
double GetRealTimeSeconds() {
  auto now = std::chrono::system_clock::now();
  auto duration = now.time_since_epoch();
  return std::chrono::duration<double>(duration).count();
}

/*
=============
Vote_Menu_Active
=============
*/
bool Vote_Menu_Active(gentity_t *ent) {
  if (level.vote.time <= 0_sec)
    return false;

  if (!level.vote.client)
    return false;

  if (ent->client->pers.voted)
    return false;

  if (!g_allowSpecVote->integer && !ClientIsPlaying(ent->client))
    return false;

  return true;
}

bool ReadyConditions(gentity_t *ent, bool admin_cmd) {
  if (level.matchState == MatchState::Warmup_ReadyUp)
    return true;

  const char *reason = admin_cmd ? "You cannot force ready status until "
                                 : "You cannot change your ready status until ";
  switch (level.warmupState) {
  case WarmupState::Too_Few_Players: {
    int minp = Game::Has(GameFlags::OneVOne) ? 2 : minplayers->integer;
    int req = minp - level.pop.num_playing_clients;
    gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_e0c04282a5bb", reason,
                       req, req > 1 ? "s are" : " is");
    break;
  }
  case WarmupState::Teams_Imbalanced:
    gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_fd5213927702", reason);
    break;
  default:
    gi.LocClient_Print(
        ent, PRINT_HIGH,
        "$g_sgame_auto_ebb0ea2e2155");
    break;
  }
  return false;
}

/*
=================
ClientListSortByJoinTime
=================
*/
static int ClientListSortByJoinTime(const void *a, const void *b) {
  int anum, bnum;
  anum = *(const int *)a;
  bnum = *(const int *)b;
  anum = game.clients[anum].sess.teamJoinTime.milliseconds();
  bnum = game.clients[bnum].sess.teamJoinTime.milliseconds();
  if (anum > bnum)
    return -1;
  if (anum < bnum)
    return 1;
  return 0;
}

/*
================
TeamBalance
Balance the teams without shuffling.
Switch last joined player(s) from stacked team.
================
*/
int TeamBalance(bool force) {
  if (!Teams())
    return 0;
  if (Game::Is(GameType::RedRover))
    return 0;
  const bool queueSwap = Game::Has(GameFlags::Rounds | GameFlags::Elimination);
  int delta = abs(level.pop.num_playing_red - level.pop.num_playing_blue);
  if (delta < 2)
    return level.pop.num_playing_red - level.pop.num_playing_blue;
  Team stack_team = level.pop.num_playing_red > level.pop.num_playing_blue
                        ? Team::Red
                        : Team::Blue;
  const Team targetTeam = (stack_team == Team::Red) ? Team::Blue : Team::Red;
  if (queueSwap) {
    int pendingQueued = 0;
    for (auto ec : active_clients()) {
      if (!ec->client || ec->client->sess.team != stack_team)
        continue;
      if (ec->client->sess.queuedTeam == targetTeam)
        pendingQueued++;
    }
    delta -= pendingQueued;
    if (delta < 0)
      delta = 0;
    if (delta < 2)
      return level.pop.num_playing_red - level.pop.num_playing_blue;
  }
  std::array<int, MAX_CLIENTS_KEX> index{};
  size_t count = CollectStackedTeamClients(stack_team, index);
  // sort client num list by join time
  qsort(index.data(), count, sizeof(index[0]), ClientListSortByJoinTime);
  // run through sort list, switching from stack_team until teams are even
  if (count) {
    size_t i;
    int switched = 0;
    gclient_t *cl = nullptr;
    for (i = 0; i < count && delta > 1; i++) {
      cl = &game.clients[index[i]];
      if (!cl)
        continue;
      if (!cl->pers.connected)
        continue;
      if (cl->sess.team != stack_team)
        continue;
      gentity_t *ent = &g_entities[cl - game.clients + 1];
      if (queueSwap) {
        if (cl->sess.queuedTeam == targetTeam)
          continue;
        cl->sess.queuedTeam = targetTeam;
        gi.LocClient_Print(
            ent, PRINT_CENTER,
            G_Fmt(
                "Team balance queued.\nYou will join the {} team next round.\n",
                Teams_TeamName(targetTeam))
                .data());
        gi.LocBroadcast_Print(
            PRINT_HIGH,
            G_Fmt("{} will swap to the {} team when the next round begins.\n",
                  G_ColorResetAfter(cl->pers.netName),
                  Teams_TeamName(targetTeam))
                .data());
      } else {
        cl->sess.team = targetTeam;
        ClientRespawn(ent);
        gi.LocClient_Print(ent, PRINT_CENTER,
                        "$g_sgame_auto_dcd63279a234");
      }
      delta--;
      switched++;
    }
    if (switched) {
      if (queueSwap)
        gi.LocBroadcast_Print(
            PRINT_HIGH,
            "$g_sgame_auto_fb957bc77f51");
      else
        gi.LocBroadcast_Print(PRINT_HIGH, "$g_sgame_auto_ce0b7adee297");
      return switched;
    }
  }
  return 0;
}

/*
=============
ApplyQueuedTeamChange

Moves a client to their queued team assignment, if present.
=============
*/
void ApplyQueuedTeamChange(gentity_t *ent, bool silent) {
  if (!ent || !ent->client)
    return;

  gclient_t *cl = ent->client;
  if (cl->sess.queuedTeam == Team::None)
    return;

  const Team target = cl->sess.queuedTeam;
  cl->sess.queuedTeam = Team::None;

  SetTeam(ent, target, false, true, silent);
}

/*
=============
ApplyQueuedTeamChanges

Applies queued team assignments for all connected clients.
=============
*/
void ApplyQueuedTeamChanges(bool silent) {
  for (auto ec : active_clients())
    ApplyQueuedTeamChange(ec, silent);
}

/*
================
PickTeam
================
*/
Team PickTeam(int ignore_client_num) {
  if (!Teams())
    return Team::Free;
  if (level.pop.num_playing_blue > level.pop.num_playing_red)
    return Team::Red;
  if (level.pop.num_playing_red > level.pop.num_playing_blue)
    return Team::Blue;
  // equal team count, so join the team with the lowest score
  if (level.teamScores[static_cast<int>(Team::Blue)] >
      level.teamScores[static_cast<int>(Team::Red)])
    return Team::Red;
  if (level.teamScores[static_cast<int>(Team::Red)] >
      level.teamScores[static_cast<int>(Team::Blue)])
    return Team::Blue;
  // equal team scores, so join team with lowest total individual scores
  // skip in tdm as it's redundant
  if (Game::IsNot(GameType::TeamDeathmatch)) {
    int iscore_red = 0, iscore_blue = 0;
    for (size_t i = 0; i < game.maxClients; i++) {
      if (i == ignore_client_num)
        continue;
      if (!game.clients[i].pers.connected)
        continue;
      if (game.clients[i].sess.team == Team::Red) {
        iscore_red += game.clients[i].resp.score;
        continue;
      }
      if (game.clients[i].sess.team == Team::Blue) {
        iscore_blue += game.clients[i].resp.score;
        continue;
      }
    }
    if (iscore_blue > iscore_red)
      return Team::Red;
    if (iscore_red > iscore_blue)
      return Team::Blue;
  }
  // otherwise just randomly select a team
  return brandom() ? Team::Red : Team::Blue;
}

