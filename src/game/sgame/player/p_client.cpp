/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

p_client.cpp (Player Client) This file manages the lifecycle and state of a
player connected to the server. It handles everything from the initial
connection and spawning into the world to death, respawning, and disconnection.
Key Responsibilities: - Client Lifecycle: Implements `ClientConnect`,
`ClientBegin`, and `ClientDisconnect` to manage a player's session. - Spawning
and Respawning: Contains the logic for `ClientSpawn` and `ClientRespawn`,
including selecting a spawn point (`SelectSpawnPoint`) and putting the player
into the game world. - Per-Frame Updates: The `ClientThink` function is the main
entry point for processing a player's user commands each frame, triggering
movement and actions. - Death and Intermission: Handles player death
(`player_die`) and moving the client to the intermission state at the end of a
match. - State Management: Initializes and maintains the `gclient_t` struct,
which holds all of a player's game-related state.*/

#include "../bots/bot_includes.hpp"
#include "../client/client_session_service_impl.hpp"
#include "../client/client_stats_service.hpp"
#include "../commands/commands.hpp"
#include "../g_local.hpp"
#include "../gameplay/client_config.hpp"
#include "../gameplay/g_headhunters.hpp"
#include "../gameplay/g_proball.hpp"
#include "../monsters/m_player.hpp"
#include "p_client_shared.hpp"
#include "team_join_capacity.hpp"


#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace worr::server::client {

/*
=============
GetClientSessionService

Provides p_client.cpp with access to the lazily constructed client session
service so legacy entry points can delegate to the shared implementation.
=============
*/
namespace {
struct ClientSessionServiceDependencies {
  local_game_import_t *gi;
  GameLocals *game;
  LevelLocals *level;
  ClientConfigStore *configStore;
  ClientStatsService *statsService;
};

ClientSessionServiceDependencies g_clientSessionServiceDependencies{
    &gi, &game, &level, nullptr, nullptr};
std::unique_ptr<ClientSessionServiceImpl> g_clientSessionServiceInstance;

/*
=============
        EnsureClientSessionServiceDependencies

Initializes any missing dependency pointers with the default globals so the
lazy construction path can't dereference null values when tests override only a
subset of the references.
=============
*/
void EnsureClientSessionServiceDependencies() {
  if (!g_clientSessionServiceDependencies.gi)
    g_clientSessionServiceDependencies.gi = &gi;
  if (!g_clientSessionServiceDependencies.game)
    g_clientSessionServiceDependencies.game = &game;
  if (!g_clientSessionServiceDependencies.level)
    g_clientSessionServiceDependencies.level = &level;
  if (!g_clientSessionServiceDependencies.configStore)
    g_clientSessionServiceDependencies.configStore = &GetClientConfigStore();
  if (!g_clientSessionServiceDependencies.statsService)
    g_clientSessionServiceDependencies.statsService = &GetClientStatsService();
}
} // namespace

/*
=============
InitializeClientSessionService

Sets the dependencies used when lazily constructing the client session
service. Tests can replace the references prior to invoking any legacy entry
points.
=============
*/
void InitializeClientSessionService(local_game_import_t &giRef,
                                    GameLocals &gameRef, LevelLocals &levelRef,
                                    ClientConfigStore &configStoreRef,
                                    ClientStatsService &statsServiceRef) {
  g_clientSessionServiceDependencies.gi = &giRef;
  g_clientSessionServiceDependencies.game = &gameRef;
  g_clientSessionServiceDependencies.level = &levelRef;
  g_clientSessionServiceDependencies.configStore = &configStoreRef;
  g_clientSessionServiceDependencies.statsService = &statsServiceRef;
  g_clientSessionServiceInstance.reset();
}

/*
=============
InitializeClientSessionService

Convenience overload that wires the service up to the default client config and
stats services when tests or bootstrapping code don't need to supply mocks.
=============
*/
void InitializeClientSessionService(local_game_import_t &giRef,
                                    GameLocals &gameRef,
                                    LevelLocals &levelRef) {
  InitializeClientSessionService(giRef, gameRef, levelRef,
                                 GetClientConfigStore(),
                                 GetClientStatsService());
}

/*
=============
GetClientSessionService

Provides p_client.cpp with access to the lazily constructed client session
service so legacy entry points can delegate to the shared implementation.
=============
*/
ClientSessionServiceImpl &GetClientSessionService() {
  if (!g_clientSessionServiceInstance) {
    EnsureClientSessionServiceDependencies();
    g_clientSessionServiceInstance = std::make_unique<ClientSessionServiceImpl>(
        *g_clientSessionServiceDependencies.gi,
        *g_clientSessionServiceDependencies.game,
        *g_clientSessionServiceDependencies.level,
        *g_clientSessionServiceDependencies.configStore,
        *g_clientSessionServiceDependencies.statsService);
  }
  return *g_clientSessionServiceInstance;
}

} // namespace worr::server::client

namespace {
constexpr char kObituaryMetaStart = '\x1e';
constexpr char kObituaryMetaSep = '\x1f';
constexpr const char kObituaryMetaTag[] = "OBIT";

constexpr const char *kObitKeyExpiration = "obit_expiration";
constexpr const char *kObitKeySelfPlasmaGun = "obit_self_plasmagun";
constexpr const char *kObitKeyKillPlasmaGun = "obit_kill_plasmagun";
constexpr const char *kObitKeyKillPlasmaGunSplash = "obit_kill_plasmagun_splash";
constexpr const char *kObitKeyKillThunderbolt = "obit_kill_thunderbolt";
constexpr const char *kObitKeyKillThunderboltDischarge =
    "obit_kill_thunderbolt_discharge";
constexpr const char *kObitKeySelfThunderboltDischarge =
    "obit_self_thunderbolt_discharge";
constexpr const char *kObitKeySelfTesla = "obit_self_tesla";

std::string BuildObituaryLocBase(const char *key, const char *base) {
  std::string out;
  out.push_back(kObituaryMetaStart);
  out.append(kObituaryMetaTag);
  out.push_back(kObituaryMetaSep);
  if (key)
    out.append(key);
  out.push_back(kObituaryMetaSep);
  if (base)
    out.append(base);
  return out;
}

void BroadcastObituaryPrint(const char *key, const char *base,
                            const char *victim, const char *killer) {
  const std::string decorated = BuildObituaryLocBase(key, base);
  if (killer && *killer) {
    gi.LocBroadcast_Print(PRINT_MEDIUM, decorated.c_str(), victim, killer);
  } else {
    gi.LocBroadcast_Print(PRINT_MEDIUM, decorated.c_str(), victim);
  }
}
} // namespace

/*
=============
ClientSetReadyStatus

Defers the ready state updates to the session service so the legacy logic can
gradually migrate out of this translation unit.
=============
*/
void ClientSetReadyStatus(gentity_t &ent, bool state, bool toggle) {
  const auto result =
      worr::server::client::GetClientSessionService().OnReadyToggled(
          &ent, state, toggle);

  if (result == worr::server::client::ReadyResult::AlreadySet) {
    gi.LocClient_Print(&ent, PRINT_HIGH, "$g_sgame_auto_992cb0dca638",
                       state ? "" : "NOT ");
  }
}

/*
=============
ClientSetReadyStatus

Validates legacy nullable entry points and emits a diagnostic when invoked with
invalid data before delegating to the non-nullable overload.
=============
*/
void ClientSetReadyStatus(gentity_t *ent, bool state, bool toggle) {
  if (!ent) {
    gi.Com_PrintFmt("{}: called with nullptr ent.\n", __FUNCTION__);
    return;
  }

  ClientSetReadyStatus(*ent, state, toggle);
}

/*QUAKED info_player_start (1 0 0) (-16 -16 -24) (16 16 32) x x x x x x x x
NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP The normal starting point for a
level.

"noBots" will prevent bots from using this spot.
"noHumans" will prevent humans from using this spot.
*/
/*
=============
info_player_start_drop

Prepare an info_player_start point to drop safely onto moving platforms when
spawning on N64 maps.
=============
*/
static void info_player_start_drop(gentity_t *self) {
  self->solid = SOLID_TRIGGER;
  self->moveType = MoveType::Toss;
  self->mins = PLAYER_MINS;
  self->maxs = PLAYER_MAXS;
  gi.linkEntity(self);
}

/*
=============
SP_info_player_start

Entry point for info_player_start entities.
=============
*/
void SP_info_player_start(gentity_t *self) {
  auto &sessionService = worr::server::client::GetClientSessionService();
  sessionService.PrepareSpawnPoint(self, true, info_player_start_drop);
  sessionService.ApplySpawnFlags(self);
}

/*QUAKED info_player_deathmatch (1 0 1) (-16 -16 -24) (16 16 32) INITIAL x x x x
x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP A potential spawning position
for deathmatch games.

The first time a player enters the game, they will be at an 'INITIAL' spot.
Targets will be fired when someone spawns in on them.
"noBots" will prevent bots from using this spot.
"noHumans" will prevent humans from using this spot.
*/
void SP_info_player_deathmatch(gentity_t *self) {
  if (!deathmatch->integer) {
    FreeEntity(self);
    return;
  }
  // Paril-KEX N64 doesn't display these
  if (level.isN64)
    return;

  CreateSpawnPad(self);

  worr::server::client::GetClientSessionService().ApplySpawnFlags(self);
}

/*QUAKED info_player_team_red (1 0 0) (-16 -16 -24) (16 16 32) x x x x x x x x
NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP A potential Red Team spawning
position for CTF games.
*/
void SP_info_player_team_red(gentity_t *self) {}

/*QUAKED info_player_team_blue (0 0 1) (-16 -16 -24) (16 16 32) x x x x x x x x
NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP A potential Blue Team spawning
position for CTF games.
*/
void SP_info_player_team_blue(gentity_t *self) {}

/*QUAKED info_player_coop (1 0 1) (-16 -16 -24) (16 16 32) x x x x x x x x
NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP A potential spawning position for
coop games.
*/
void SP_info_player_coop(gentity_t *self) {
  if (!coop->integer) {
    FreeEntity(self);
    return;
  }

  SP_info_player_start(self);
}

/*QUAKED info_player_coop_lava (1 0 1) (-16 -16 -24) (16 16 32) x x x x x x x x
NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP A potential spawning position for
coop games on rmine2 where lava level needs to be checked.
*/
void SP_info_player_coop_lava(gentity_t *self) {
  if (!coop->integer) {
    FreeEntity(self);
    return;
  }

  worr::server::client::GetClientSessionService().PrepareSpawnPoint(self);
}

/*QUAKED info_player_intermission (1 0 1) (-16 -16 -24) (16 16 32) x x x x x x x
x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP The deathmatch intermission point
will be at one of these Use 'angles' instead of 'angle', so you can set pitch or
roll as well as yaw.  'pitch yaw roll'
*/
void SP_info_player_intermission(gentity_t *ent) {}

/*QUAKED info_ctf_teleport_destination (0.5 0.5 0.5) (-16 -16 -24) (16 16 32) x
x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP Point
trigger_teleports at these.
*/
void SP_info_ctf_teleport_destination(gentity_t *ent) {
  ent->s.origin[_Z] += 16;
}

// [Paril-KEX] whether instanced items should be used or not
bool P_UseCoopInstancedItems() {
  // squad respawn forces instanced items on, since we don't
  // want players to need to backtrack just to get their stuff.
  return g_coop_instanced_items->integer || g_coop_squad_respawn->integer;
}

//=======================================================================

/*
===============
PushAward
===============
*/
void PushAward(gentity_t *ent, PlayerMedal medal) {
  if (!ent || !ent->client)
    return;
  constexpr int MAX_QUEUED_AWARDS = 8;

  struct MedalInfo {
    std::string_view soundKeyFirst;
    std::string_view soundKeyRepeat;
  };
  static constexpr std::array<MedalInfo,
                              static_cast<std::size_t>(PlayerMedal::Total)>
      medalTable = {{{"", ""}, // None
                     {"first_excellent", "excellent1"},
                     {"", "humiliation1"},
                     {"", "impressive1"},
                     {"", "rampage1"},
                     {"", "first_frag"},
                     {"", "defense1"},
                     {"", "assist1"},
                     {"", ""}, // Captures
                     {"", "holy_shit"}}};

  auto &cl = *ent->client;
  auto idx = static_cast<std::size_t>(medal);
  auto &info = medalTable[idx];

  auto &count = cl.pers.match.medalCount[idx];
  ++count;

  std::string_view key = (count == 1 && !info.soundKeyFirst.empty())
                             ? info.soundKeyFirst
                             : info.soundKeyRepeat;

  if (cl.sess.is_a_bot)
    return;

  int soundIdx = 0;
  if (!key.empty()) {
    const std::string path = G_Fmt("vo/{}.wav", key).data();
    soundIdx = gi.soundIndex(path.c_str());
  }

  auto &queue = cl.pers.awardQueue;
  if (queue.queueSize < MAX_QUEUED_AWARDS) {
    const int slot = queue.queueSize++;
    queue.soundIndex[slot] = soundIdx;
    queue.medal[slot] = medal;
    queue.count[slot] = static_cast<int>(count);

    // If no sound is playing, start immediately
    if (queue.queueSize == 1) {
      queue.nextPlayTime = level.time;
      queue.playIndex = 0;
    }
  }
}
//=======================================================================

/*
===============
P_AccumulateMatchPlayTime

Accumulates the client's active match play segment into their persistent total.
===============
*/
void worr::server::client::P_AccumulateMatchPlayTime(gclient_t *cl,
                                                     int64_t now) {
  if (!cl)
    return;

  if (cl->sess.playStartRealTime <= 0)
    return;

  if (now <= cl->sess.playStartRealTime)
    return;

  cl->resp.totalMatchPlayRealTime += now - cl->sess.playStartRealTime;
  cl->sess.playStartRealTime = now;
}

/*
===============
P_SaveGhostSlot

Caches the player's state for reconnects if they have met the minimum
real-time participation threshold.
===============
*/
void P_SaveGhostSlot(gentity_t *ent) {
  if (!ent || !ent->client)
    return;

  if (ent == host)
    return;

  gclient_t *cl = ent->client;

  if (!cl)
    return;

  if (level.matchState != MatchState::In_Progress)
    return;

  const int64_t minGhostSlotPlayTimeMs =
      (g_ghost_min_play_time)
          ? std::max<int64_t>(
                0, static_cast<int64_t>(g_ghost_min_play_time->value * 1000.0f))
          : (60 * 1000);

  if (cl->resp.totalMatchPlayRealTime < minGhostSlotPlayTimeMs)
    return;

  const char *socialID = cl->sess.socialID;
  if (!socialID || !*socialID)
    return;

  // Find existing ghost slot or first free one
  Ghosts *slot = nullptr;
  for (auto &g : level.ghosts) {
    if (Q_strcasecmp(g.socialID, socialID) == 0) {
      slot = &g;
      break;
    }
    if (!*g.socialID && !slot)
      slot = &g;
  }

  if (!slot)
    return; // No available slot

  // Store name and social ID
  Q_strlcpy(slot->netName, cl->sess.netName, sizeof(slot->netName));
  Q_strlcpy(slot->socialID, socialID, sizeof(slot->socialID));

  // Store inventory and stats
  slot->inventory = cl->pers.inventory;
  slot->ammoMax = cl->pers.ammoMax;
  slot->match = cl->pers.match;
  slot->weapon = cl->pers.weapon;
  slot->lastWeapon = cl->pers.lastWeapon;
  slot->team = cl->sess.team;
  slot->score = cl->resp.score;
  slot->skillRating = cl->sess.skillRating;
  slot->skillRatingChange = cl->sess.skillRatingChange;
  slot->origin = ent->s.origin;
  slot->angles = ent->s.angles;
  slot->totalMatchPlayRealTime = cl->resp.totalMatchPlayRealTime;
}

/*
===============
P_RestoreFromGhostSlot
===============
*/
void P_RestoreFromGhostSlot(gentity_t *ent) {
  if (!ent || !ent->client)
    return;

  gclient_t *cl = ent->client;

  if (cl->sess.socialID[0] == '\0')
    return;

  const char *socialID = cl->sess.socialID;

  for (auto &g : level.ghosts) {
    if (Q_strcasecmp(g.socialID, socialID) != 0)
      continue;

    // Restore inventory and stats
    cl->pers.inventory = g.inventory;
    cl->pers.ammoMax = g.ammoMax;
    cl->pers.match = g.match;
    cl->pers.weapon = g.weapon;
    cl->pers.lastWeapon = g.lastWeapon;
    cl->sess.team = g.team;
    cl->ps.teamID = static_cast<int>(cl->sess.team);
    cl->resp.score = g.score;
    cl->sess.skillRating = g.skillRating;
    cl->sess.skillRatingChange = g.skillRatingChange;
    cl->resp.totalMatchPlayRealTime = g.totalMatchPlayRealTime;

    cl->resp.hasPendingGhostSpawn = true;
    cl->resp.pendingGhostOrigin = g.origin;
    cl->resp.pendingGhostAngles = g.angles;

    gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_07394ac22fea");

    // Clear the ghost slot
    g = Ghosts{};
    return;
  }
}

//=======================================================================

//=======================================================================
// PLAYER CONFIGS
//=======================================================================
namespace {
constexpr const char *PLAYER_CONFIG_DIRECTORY = "baseq2/pcfg";
}

/*
===============
PCfg_BuildConfigPath

Sanitizes the client's social ID and builds the legacy config filepath.
===============
*/
static bool PCfg_BuildConfigPath(const char *caller, const gentity_t *ent,
                                 std::string &path,
                                 std::string &sanitizedSocialID,
                                 const char *operation) {
  if (!ent || !ent->client)
    return false;

  const std::string originalSocialID = ent->client->sess.socialID;
  sanitizedSocialID = SanitizeSocialID(originalSocialID);
  if (sanitizedSocialID.empty()) {
    if (gi.Com_Print) {
      gi.Com_PrintFmt("WARNING: {}: refusing to {} player config for invalid "
                      "social ID '{}'\n",
                      caller, operation, originalSocialID.c_str());
    }
    return false;
  }
  if (sanitizedSocialID != originalSocialID && gi.Com_Print) {
    gi.Com_PrintFmt("WARNING: {}: sanitized social ID '{}' to '{}' for player "
                    "config filename\n",
                    caller, originalSocialID.c_str(),
                    sanitizedSocialID.c_str());
  }
  path = G_Fmt("{}/{}.cfg", PLAYER_CONFIG_DIRECTORY, sanitizedSocialID).data();
  return true;
}

/*
===============
PCfg_WriteConfig

Serializes the current client_config_t values to the legacy config file.
===============
*/
void PCfg_WriteConfig(gentity_t *ent) {
  if (!ent || !ent->client)
    return;
  if (ent->svFlags & SVF_BOT)
    return;
  if (!std::strcmp(ent->client->sess.socialID, "me_a_bot"))
    return;

  std::string sanitizedSocialID;
  std::string path;
  if (!PCfg_BuildConfigPath(__FUNCTION__, ent, path, sanitizedSocialID,
                            "write"))
    return;

  std::error_code directory_error;
  if (!std::filesystem::create_directories(PLAYER_CONFIG_DIRECTORY,
                                           directory_error) &&
      directory_error) {
    if (gi.Com_Print) {
      gi.Com_PrintFmt(
          "WARNING: {}: failed to create player config directory \"{}\": {}\n",
          __FUNCTION__, PLAYER_CONFIG_DIRECTORY,
          directory_error.message().c_str());
    }
    return;
  }

  std::unique_ptr<FILE, decltype(&std::fclose)> file(
      std::fopen(path.c_str(), "wb"), &std::fclose);
  if (!file) {
    if (gi.Com_Print) {
      gi.Com_PrintFmt("{}: Cannot save player config: {}\n", __FUNCTION__,
                      path.c_str());
    }
    return;
  }

  const client_config_t &pc = ent->client->sess.pc;
  std::string contents;
  contents.reserve(256);
  contents += "// ";
  contents += ent->client->sess.netName;
  contents += "'s Player Config\n// Generated by WOR\n";

  const auto appendBoolLine = [&contents](std::string_view key, bool value) {
    contents += key;
    contents += ' ';
    contents.push_back(value ? '1' : '0');
    contents.push_back('\n');
  };
  const auto appendIntLine = [&contents](std::string_view key, int value) {
    contents += key;
    contents += ' ';
    contents += std::to_string(value);
    contents.push_back('\n');
  };

  appendBoolLine("show_id", pc.show_id);
  appendBoolLine("show_fragmessages", pc.show_fragmessages);
  appendBoolLine("show_timer", pc.show_timer);
  appendBoolLine("use_eyecam", pc.use_eyecam);
  appendIntLine("killbeep_num", pc.killbeep_num);
  appendBoolLine("follow_killer", pc.follow_killer);
  appendBoolLine("follow_leader", pc.follow_leader);
  appendBoolLine("follow_powerup", pc.follow_powerup);

  const size_t written =
      std::fwrite(contents.data(), 1, contents.size(), file.get());
  if (written != contents.size()) {
    if (gi.Com_Print) {
      gi.Com_PrintFmt(
          "WARNING: {}: short write while saving player config \"{}\"\n",
          __FUNCTION__, path.c_str());
    }
    return;
  }

  gi.Com_PrintFmt("{}: Player config written to: \"{}\"\n", __FUNCTION__,
                  path.c_str());
}

/*
=============
PCfg_TrimView

Removes leading and trailing whitespace from a string view.
=============
*/
static std::string_view PCfg_TrimView(std::string_view text) {
  while (!text.empty() &&
         std::isspace(static_cast<unsigned char>(text.front()))) {
    text.remove_prefix(1);
  }

  while (!text.empty() &&
         std::isspace(static_cast<unsigned char>(text.back()))) {
    text.remove_suffix(1);
  }

  return text;
}

/*
=============
PCfg_ParseInt

Attempts to parse an integer from the supplied string view.
=============
*/
static bool PCfg_ParseInt(std::string_view text, int &outValue) {
  text = PCfg_TrimView(text);
  if (text.empty()) {
    return false;
  }

  const std::string temp(text);
  char *end = nullptr;
  const long parsed = std::strtol(temp.c_str(), &end, 10);
  if (end == temp.c_str() || *end != '\0') {
    return false;
  }

  outValue = static_cast<int>(parsed);
  return true;
}

/*
=============
PCfg_ParseBool

Attempts to parse a boolean from the supplied string view.
=============
*/
static bool PCfg_ParseBool(std::string_view text, bool &outValue) {
  text = PCfg_TrimView(text);
  if (text.empty()) {
    return false;
  }

  std::string lowered(text);
  std::transform(
      lowered.begin(), lowered.end(), lowered.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

  if (lowered == "1" || lowered == "true" || lowered == "yes" ||
      lowered == "on") {
    outValue = true;
    return true;
  }

  if (lowered == "0" || lowered == "false" || lowered == "no" ||
      lowered == "off") {
    outValue = false;
    return true;
  }

  return false;
}

/*
=============
PCfg_ApplyConfigLine

Parses and applies a single key/value pair from the legacy player config.
=============
*/
static void PCfg_ApplyConfigLine(gentity_t *ent, std::string_view line) {
  line = PCfg_TrimView(line);
  if (line.empty()) {
    return;
  }

  if (line.size() >= 2 && line[0] == '/' && line[1] == '/') {
    return;
  }

  if (line.front() == '#') {
    return;
  }

  const size_t separator = line.find_first_of(" 	");
  if (separator == std::string_view::npos) {
    return;
  }

  const std::string_view key = line.substr(0, separator);
  const std::string_view value = PCfg_TrimView(line.substr(separator + 1));

  if (key == "show_id") {
    bool parsed = false;
    if (PCfg_ParseBool(value, parsed)) {
      ent->client->sess.pc.show_id = parsed;
    }
    return;
  }

  if (key == "show_fragmessages") {
    bool parsed = false;
    if (PCfg_ParseBool(value, parsed)) {
      ent->client->sess.pc.show_fragmessages = parsed;
    }
    return;
  }

  if (key == "show_timer") {
    bool parsed = false;
    if (PCfg_ParseBool(value, parsed)) {
      ent->client->sess.pc.show_timer = parsed;
    }
    return;
  }

  if (key == "killbeep_num") {
    int parsed = 0;
    if (PCfg_ParseInt(value, parsed)) {
      if (parsed < 0) {
        parsed = 0;
      }
      if (parsed > 4) {
        parsed = 4;
      }
      ent->client->sess.pc.killbeep_num = parsed;
    }
    return;
  }
}

/*
=============
PCfg_ParseConfigBuffer

Parses the legacy player configuration buffer and applies known settings.
=============
*/
static void PCfg_ParseConfigBuffer(gentity_t *ent, const char *buffer) {
  if (!buffer || !*buffer) {
    return;
  }

  const char *cursor = buffer;
  while (*cursor) {
    const char *line_start = cursor;
    while (*cursor && *cursor != '\n' && *cursor != '\r') {
      ++cursor;
    }

    const std::string_view line(line_start,
                                static_cast<size_t>(cursor - line_start));
    PCfg_ApplyConfigLine(ent, line);

    while (*cursor == '\n' || *cursor == '\r') {
      ++cursor;
    }
  }
}

/*
=============
PCfg_ClientInitPConfig

Initializes the player configuration by loading an existing config file or
generating a default when none is present.
=============
*/
void PCfg_ClientInitPConfig(gentity_t *ent) {
  bool file_exists = false;
  bool cfg_valid = true;
  bool directory_ready = true;

  if (!ent->client)
    return;
  if (ent->svFlags & SVF_BOT)
    return;

  // load up file
  std::string sanitizedSocialID;
  std::string path;
  if (!PCfg_BuildConfigPath(__FUNCTION__, ent, path, sanitizedSocialID,
                            "read")) {
    return;
  }

  std::error_code directory_error;
  if (!std::filesystem::create_directories(PLAYER_CONFIG_DIRECTORY,
                                           directory_error) &&
      directory_error) {
    directory_ready = false;
    if (gi.Com_Print) {
      gi.Com_PrintFmt(
          "WARNING: {}: failed to create player config directory \"{}\": {}\n",
          __FUNCTION__, PLAYER_CONFIG_DIRECTORY,
          directory_error.message().c_str());
    }
  }

  std::unique_ptr<FILE, decltype(&std::fclose)> file(fopen(path.c_str(), "rb"),
                                                     &std::fclose);
  char *buffer = nullptr;
  if (file != nullptr) {
    size_t length;
    size_t read_length = 0;

    fseek(file.get(), 0, SEEK_END);
    length = static_cast<size_t>(ftell(file.get()));
    fseek(file.get(), 0, SEEK_SET);

    if (length > 0x40000) {
      cfg_valid = false;
    }
    if (cfg_valid) {
      buffer = static_cast<char *>(gi.TagMalloc(length + 1, TAG_LEVEL));
      if (!buffer) {
        cfg_valid = false;
      } else {
        if (length > 0) {
          read_length = fread(buffer, 1, length, file.get());

          if (length != read_length) {
            cfg_valid = false;
          }
        }
        buffer[read_length] = '\0';
      }
    }
    file_exists = true;

    if (!cfg_valid || !buffer) {
      if (buffer) {
        gi.TagFree(buffer);
      }
      gi.Com_PrintFmt("{}: Player config load error for \"{}\", discarding.\n",
                      __FUNCTION__, path.c_str());
      return;
    }

    PCfg_ParseConfigBuffer(ent, buffer);
    gi.TagFree(buffer);
  }

  // save file if it doesn't exist
  if (!file_exists) {
    if (directory_ready) {
      PCfg_WriteConfig(ent);
    } else {
      gi.Com_PrintFmt("{}: Cannot save player config: {}\n", __FUNCTION__,
                      path.c_str());
    }
  } else {
    // gi.Com_PrintFmt("{}: Player config not saved as file already exists:
    // \"{}\"\n", __FUNCTION__, path.c_str());
  }
}

//=======================================================================
struct MonsterListInfo {
  std::string_view class_name;
  std::string_view display_name;
};

constexpr std::array<MonsterListInfo, 57> MONSTER_INFO = {
    {{"monster_arachnid", "Arachnid"},
     {"monster_army", "Grunt"},
     {"monster_berserk", "Berserker"},
     {"monster_boss", "Chton"},
     {"monster_boss2", "Hornet"},
     {"monster_boss5", "Super Tank"},
     {"monster_brain", "Brains"},
     {"monster_carrier", "Carrier"},
     {"monster_chick", "Iron Maiden"},
     {"monster_chick_heat", "Iron Maiden"},
     {"monster_daedalus", "Daedalus"},
     {"monster_demon1", "Fiend"},
     {"monster_dog", "Rottweiler"},
     {"monster_enforcer", "Enforcer"},
     {"monster_fish", "Rotfish"},
     {"monster_fixbot", "Fixbot"},
     {"monster_flipper", "Barracuda Shark"},
     {"monster_floater", "Technician"},
     {"monster_flyer", "Flyer"},
     {"monster_gekk", "Gekk"},
     {"monster_gladb", "Gladiator"},
     {"monster_gladiator", "Gladiator"},
     {"monster_guardian", "Guardian"},
     {"monster_guncmdr", "Gunner Commander"},
     {"monster_gunner", "Gunner"},
     {"monster_hell_knight", "Hell Knight"},
     {"monster_hover", "Icarus"},
     {"monster_infantry", "Infantry"},
     {"monster_jorg", "Jorg"},
     {"monster_kamikaze", "Kamikaze"},
     {"monster_knight", "Knight"},
     {"monster_makron", "Makron"},
     {"monster_medic", "Medic"},
     {"monster_medic_commander", "Medic Commander"},
     {"monster_mutant", "Mutant"},
     {"monster_ogre", "Ogre"},
     {"monster_ogre_marksman", "Ogre Marksman"},
     {"monster_oldone", "Shub-Niggurath"},
     {"monster_parasite", "Parasite"},
     {"monster_shalrath", "Vore"},
     {"monster_shambler", "Shambler"},
     {"monster_soldier", "Machinegun Guard"},
     {"monster_soldier_hypergun", "Hypergun Guard"},
     {"monster_soldier_lasergun", "Laser Guard"},
     {"monster_soldier_light", "Light Guard"},
     {"monster_soldier_ripper", "Ripper Guard"},
     {"monster_soldier_ss", "Shotgun Guard"},
     {"monster_stalker", "Stalker"},
     {"monster_supertank", "Super Tank"},
     {"monster_tank", "Tank"},
     {"monster_tank_commander", "Tank Commander"},
     {"monster_tarbaby", "Spawn"},
     {"monster_turret", "Turret"},
     {"monster_widow", "Black Widow"},
     {"monster_widow2", "Black Widow"},
     {"monster_wizard", "Scrag"},
     {"monster_zombie", "Zombie"}}};

static std::optional<std::string_view>
GetMonsterDisplayName(std::string_view class_name) {
  for (const auto &monster : MONSTER_INFO) {
    if (Q_strcasecmp(class_name.data(), monster.class_name.data()) == 0) {
      return monster.display_name; // Return the found name
    }
  }
  return std::nullopt;
}

[[maybe_unused]] static bool IsVowel(const char c) {
  if (c == 'A' || c == 'a' || c == 'E' || c == 'e' || c == 'I' || c == 'i' ||
      c == 'O' || c == 'o' || c == 'U' || c == 'u')
    return true;
  return false;
}

static void ClientObituary(gentity_t *victim, gentity_t *inflictor,
                           gentity_t *attacker, MeansOfDeath mod) {
  std::string base{};
  const char *obit_key = nullptr;

  if (!victim || !victim->client)
    return;

  if (attacker && CooperativeModeOn() && attacker->client)
    mod.friendly_fire = true;

  using enum ModID;

  if (mod.id == Silent)
    return;

  int killStreakCount = victim->client->killStreakCount;
  victim->client->killStreakCount = 0;

  switch (mod.id) {
  case Suicide:
    base = "{} suicides.\n";
    obit_key = "$g_mod_generic_suicide";
    break;
  case Expiration:
    base = "{} ran out of blood.\n";
    obit_key = kObitKeyExpiration;
    break;
  case FallDamage:
    base = "{} cratered.\n";
    obit_key = "$g_mod_generic_falling";
    break;
  case Crushed:
    base = "{} was squished.\n";
    obit_key = "$g_mod_generic_crush";
    break;
  case Drowning:
    base = "{} sank like a rock.\n";
    obit_key = "$g_mod_generic_water";
    break;
  case Slime:
    base = "{} melted.\n";
    obit_key = "$g_mod_generic_slime";
    break;
  case Lava:
    base = "{} does a back flip into the lava.\n";
    obit_key = "$g_mod_generic_lava";
    break;
  case Explosives:
  case Barrel:
    base = "{} blew up.\n";
    obit_key = "$g_mod_generic_explosive";
    break;
  case ExitLevel:
    base = "{} found a way out.\n";
    obit_key = "$g_mod_generic_exit";
    break;
  case Laser:
    base = "{} saw the light.\n";
    obit_key = "$g_mod_generic_laser";
    break;
  case ShooterBlaster:
    base = "{} got blasted.\n";
    obit_key = "$g_mod_generic_blaster";
    break;
  case Bomb:
  case Splash:
  case Hurt:
    base = "{} was in the wrong place.\n";
    obit_key = "$g_mod_generic_hurt";
    break;
  default:
    // base = nullptr;
    break;
  }

  if (base.empty() && attacker == victim) {
    switch (mod.id) {
    case HandGrenade_Held:
      base = "{} tried to put the pin back in.\n";
      obit_key = "$g_mod_self_held_grenade";
      break;
    case HandGrenade_Splash:
    case GrenadeLauncher_Splash:
      base = "{} tripped on their own grenade.\n";
      obit_key = "$g_mod_self_grenade_splash";
      break;
    case RocketLauncher_Splash:
      base = "{} blew themselves up.\n";
      obit_key = "$g_mod_self_rocket_splash";
      break;
    case BFG10K_Blast:
      base = "{} should have used a smaller gun.\n";
      obit_key = "$g_mod_self_bfg_blast";
      break;
    case Trap:
      base = "{} was sucked into their own trap.\n";
      obit_key = "$g_mod_self_trap";
      break;
    case Thunderbolt_Discharge:
      base = "{} had a fatal discharge.\n";
      obit_key = kObitKeySelfThunderboltDischarge;
      break;
    case PlasmaGun:
    case PlasmaGun_Splash:
      base = "{} was dissolved by their own Plasma Gun.\n";
      obit_key = kObitKeySelfPlasmaGun;
      break;
    case Doppelganger_Explode:
      base = "{} was fooled by their own doppelganger.\n";
      obit_key = "$g_mod_self_dopple_explode";
      break;
    case Expiration:
      base = "{} ran out of blood.\n";
      obit_key = kObitKeyExpiration;
      break;
    case TeslaMine:
      base = "{} got zapped by their own tesla mine.\n";
      obit_key = kObitKeySelfTesla;
      break;
    default:
      base = "{} killed themselves.\n";
      obit_key = "$g_mod_self_default";
      break;
    }
  }

  // send generic/victim
  if (!base.empty()) {
    BroadcastObituaryPrint(obit_key, base.c_str(),
                           victim->client->sess.netName, nullptr);

    {
      std::string small;
      small = fmt::format("{}", victim->client->sess.netName);
      G_LogEvent(small);
    }

    victim->enemy = nullptr;
    return;
  }

  // has a killer
  victim->enemy = attacker;

  if (!attacker)
    return;

  if (attacker->svFlags & SVF_MONSTER) {
    if (auto monster_name = GetMonsterDisplayName(attacker->className)) {
      const std::string message =
          fmt::format("{} was killed by a {}.\n", victim->client->sess.netName,
                      *monster_name);

      // Broadcast the message to all clients and write it to the server log.
      gi.LocBroadcast_Print(PRINT_MEDIUM, message.c_str());
      G_LogEvent(message);

      victim->enemy = nullptr;
    }
    return;
  }

  if (!attacker->client)
    return;
  switch (mod.id) {
  case Blaster:
    base = "{} was blasted by {}.\n";
    obit_key = "$g_mod_kill_blaster";
    break;
  case Shotgun:
    base = "{} was gunned down by {}.\n";
    obit_key = "$g_mod_kill_shotgun";
    break;
  case SuperShotgun:
    base = "{} was blown away by {}'s Super Shotgun.\n";
    obit_key = "$g_mod_kill_sshotgun";
    break;
  case Machinegun:
    base = "{} was machinegunned by {}.\n";
    obit_key = "$g_mod_kill_machinegun";
    break;
  case Chaingun:
    base = "{} was cut in half by {}'s Chaingun.\n";
    obit_key = "$g_mod_kill_chaingun";
    break;
  case GrenadeLauncher:
    base = "{} was popped by {}'s grenade.\n";
    obit_key = "$g_mod_kill_grenade";
    break;
  case GrenadeLauncher_Splash:
    base = "{} was shredded by {}'s shrapnel.\n";
    obit_key = "$g_mod_kill_grenade_splash";
    break;
  case RocketLauncher:
    base = "{} ate {}'s rocket.\n";
    obit_key = "$g_mod_kill_rocket";
    break;
  case RocketLauncher_Splash:
    base = "{} almost dodged {}'s rocket.\n";
    obit_key = "$g_mod_kill_rocket_splash";
    break;
  case HyperBlaster:
    base = "{} was melted by {}'s HyperBlaster.\n";
    obit_key = "$g_mod_kill_hyperblaster";
    break;
  case Railgun:
    base = "{} was railed by {}.\n";
    obit_key = "$g_mod_kill_railgun";
    break;
  case BFG10K_Laser:
    base = "{} saw the pretty lights from {}'s BFG.\n";
    obit_key = "$g_mod_kill_bfg_laser";
    break;
  case BFG10K_Blast:
    base = "{} was disintegrated by {}'s BFG blast.\n";
    obit_key = "$g_mod_kill_bfg_blast";
    break;
  case BFG10K_Effect:
    base = "{} couldn't hide from {}'s BFG.\n";
    obit_key = "$g_mod_kill_bfg_effect";
    break;
  case HandGrenade:
    base = "{} caught {}'s handgrenade.\n";
    obit_key = "$g_mod_kill_handgrenade";
    break;
  case HandGrenade_Splash:
    base = "{} didn't see {}'s handgrenade.\n";
    obit_key = "$g_mod_kill_handgrenade_splash";
    break;
  case HandGrenade_Held:
    base = "{} feels {}'s pain.\n";
    obit_key = "$g_mod_kill_held_grenade";
    break;
  case Telefragged:
  case Telefrag_Spawn:
    base = "{} tried to invade {}'s personal space.\n";
    obit_key = "$g_mod_kill_telefrag";
    break;
  case IonRipper:
    base = "{} ripped to shreds by {}'s ripper gun.\n";
    obit_key = "$g_mod_kill_ripper";
    break;
  case Phalanx:
    base = "{} was evaporated by {}.\n";
    obit_key = "$g_mod_kill_phalanx";
    break;
  case Trap:
    base = "{} was caught in {}'s trap.\n";
    obit_key = "$g_mod_kill_trap";
    break;
  case Chainfist:
    base = "{} was shredded by {}'s ripsaw.\n";
    obit_key = "$g_mod_kill_chainfist";
    break;
  case Disruptor:
    base = "{} lost his grip courtesy of {}'s Disintegrator.\n";
    obit_key = "$g_mod_kill_disintegrator";
    break;
  case ETFRifle:
    base = "{} was perforated by {}.\n";
    obit_key = "$g_mod_kill_etf_rifle";
    break;
  case PlasmaGun:
    base = "{} was melted by {}'s Plasma Gun.\n";
    obit_key = kObitKeyKillPlasmaGun;
    break;
  case PlasmaGun_Splash:
    base = "{} was splashed by {}'s Plasma Gun.\n";
    obit_key = kObitKeyKillPlasmaGunSplash;
    break;
  case PlasmaBeam:
    base = "{} was scorched by {}'s Plasma Beam.\n";
    obit_key = "$g_mod_kill_heatbeam";
    break;
  case Thunderbolt:
    base = "{} accepts {}'s shaft.\n";
    obit_key = kObitKeyKillThunderbolt;
    break;
  case Thunderbolt_Discharge:
    base = "{} accepts {}'s discharge.\n";
    obit_key = kObitKeyKillThunderboltDischarge;
    break;
  case TeslaMine:
    base = "{} was enlightened by {}'s tesla mine.\n";
    obit_key = "$g_mod_kill_tesla";
    break;
  case ProxMine:
    base = "{} got too close to {}'s proximity mine.\n";
    obit_key = "$g_mod_kill_prox";
    break;
  case Nuke:
    base = "{} was nuked by {}'s antimatter bomb.\n";
    obit_key = "$g_mod_kill_nuke";
    break;
  case VengeanceSphere:
    base = "{} was purged by {}'s Vengeance Sphere.\n";
    obit_key = "$g_mod_kill_vengeance_sphere";
    break;
  case DefenderSphere:
    base = "{} had a blast with {}'s Defender Sphere.\n";
    obit_key = "$g_mod_kill_defender_sphere";
    break;
  case HunterSphere:
    base = "{} was hunted down by {}'s Hunter Sphere.\n";
    obit_key = "$g_mod_kill_hunter_sphere";
    break;
  case Tracker:
    base = "{} was annihilated by {}'s Disruptor.\n";
    obit_key = "$g_mod_kill_tracker";
    break;
  case Doppelganger_Explode:
    base = "{} was tricked by {}'s Doppelganger.\n";
    obit_key = "$g_mod_kill_dopple_explode";
    break;
  case Doppelganger_Vengeance:
    base = "{} was purged by {}'s Doppelganger.\n";
    obit_key = "$g_mod_kill_dopple_vengeance";
    break;
  case Doppelganger_Hunter:
    base = "{} was hunted down by {}'s Doppelganger.\n";
    obit_key = "$g_mod_kill_dopple_hunter";
    break;
  case GrapplingHook:
    base = "{} was caught by {}'s grapple.\n";
    obit_key = "$g_mod_kill_grapple";
    break;
  default:
    base = "{} was killed by {}.\n";
    obit_key = "$g_mod_kill_generic";
    break;
  }

  BroadcastObituaryPrint(obit_key, base.c_str(),
                         victim->client->sess.netName,
                         attacker->client->sess.netName);
  if (!base.empty()) {
    std::string small = fmt::vformat(
        base, fmt::make_format_args(victim->client->sess.netName,
                                    attacker->client->sess.netName));
    G_LogEvent(small);
  }

  if (Teams()) {
    // if at start and same team, clear.
    // [Paril-KEX] moved here so it's not an outlier in player_die.
    if (mod.id == Telefrag_Spawn && victim->client->resp.ctf_state < 2 &&
        victim->client->sess.team == attacker->client->sess.team) {
      victim->client->resp.ctf_state = 0;
      return;
    }
  }

  // frag messages
  if (deathmatch->integer && victim != attacker && victim->client &&
      attacker->client) {
    if (!(victim->svFlags & SVF_BOT)) {
      if (level.matchState == MatchState::Warmup_ReadyUp) {
        BroadcastReadyReminderMessage();
      } else {
        if (Game::Has(GameFlags::Rounds | GameFlags::Elimination) &&
            level.roundState == RoundState::In_Progress) {
          gi.LocClient_Print(
              victim, PRINT_CENTER,
              "$g_sgame_auto_24516e7280f1",
              attacker->client->sess.netName);
        } else if (Game::Is(GameType::FreezeTag) &&
                   level.roundState == RoundState::In_Progress) {
          bool last_standing = true;
          if ((victim->client->sess.team == Team::Red &&
               level.pop.num_living_red > 1) ||
              (victim->client->sess.team == Team::Blue &&
               level.pop.num_living_blue > 1))
            last_standing = false;
          gi.LocClient_Print(victim, PRINT_CENTER, "$g_sgame_auto_46b053398cf6",
                             attacker->client->sess.netName,
                             last_standing ? ""
                                           : "\nYou will respawn once thawed.");
        } else {
          gi.LocClient_Print(victim, PRINT_CENTER, "$g_sgame_auto_ddeb5b9cd278",
                             Game::Is(GameType::FreezeTag) ? "frozen"
                                                           : "fragged",
                             attacker->client->sess.netName);
        }
      }
    }
    if (!(attacker->svFlags & SVF_BOT)) {
      if (Teams() && OnSameTeam(victim, attacker)) {
        gi.LocClient_Print(attacker, PRINT_CENTER,
                           "$g_sgame_auto_e1384014eedb",
                           victim->client->sess.netName);
      } else {
        if (level.matchState == MatchState::Warmup_ReadyUp) {
          BroadcastReadyReminderMessage();
        } else if (attacker->client->killStreakCount &&
                   !(attacker->client->killStreakCount % 10)) {
          gi.LocBroadcast_Print(PRINT_CENTER,
                                "$g_sgame_auto_5ea9ca9e55c0",
                                attacker->client->sess.netName,
                                attacker->client->killStreakCount);
          PushAward(attacker, PlayerMedal::Rampage);
        } else if (killStreakCount >= 10) {
          gi.LocBroadcast_Print(
              PRINT_CENTER, "$g_sgame_auto_d037994285e0",
              attacker->client->sess.netName, victim->client->sess.netName);
        } else if (Teams() || level.matchState != MatchState::In_Progress) {
          if (attacker->client->sess.pc.show_fragmessages)
            gi.LocClient_Print(attacker, PRINT_CENTER, "$g_sgame_auto_161a2768fd20",
                               Game::Is(GameType::FreezeTag) ? "froze"
                                                             : "fragged",
                               victim->client->sess.netName);
        } else {
          if (attacker->client->sess.pc.show_fragmessages)
            gi.LocClient_Print(
                attacker, PRINT_CENTER, "$g_sgame_auto_e41afbaf7793",
                Game::Is(GameType::FreezeTag) ? "froze" : "fragged",
                victim->client->sess.netName,
                PlaceString(attacker->client->pers.currentRank + 1),
                attacker->client->resp.score);
        }
      }
      if (attacker->client->sess.pc.killbeep_num > 0 &&
          attacker->client->sess.pc.killbeep_num < 5) {
        const char *sb[5] = {"", "nav_editor/select_node.wav",
                             "misc/comp_up.wav", "insane/insane7.wav",
                             "nav_editor/finish_node_move.wav"};
        gi.localSound(attacker, CHAN_AUTO,
                      gi.soundIndex(sb[attacker->client->sess.pc.killbeep_num]),
                      1, ATTN_NONE, 0);
      }
    }
  }

  if (base.size())
    return;

  BroadcastObituaryPrint("$g_mod_generic_died", "$g_mod_generic_died",
                         victim->client->sess.netName, nullptr);
}

/*
=================
TossClientItems

Toss the weapon, tech, CTF flag and powerups for the killed player
=================
*/
void TossClientItems(gentity_t *self) {
  if (!deathmatch->integer)
    return;

  if (Game::Has(GameFlags::Arena))
    return;

  if (!ClientIsPlaying(self->client))
    return;

  if (!self->client->sess.initialised)
    return;

  // don't drop anything when combat is disabled
  if (CombatIsDisabled())
    return;

  Item *wp;
  gentity_t *drop;
  bool quad, doubled, haste, protection, invis, regen;

  if (RS(Quake1)) {
    Drop_Backpack(self);
  } else {
    // drop weapon
    wp = self->client->pers.weapon;
    if (wp) {
      if (g_instaGib->integer)
        wp = nullptr;
      else if (g_nadeFest->integer)
        wp = nullptr;
      else if (!self->client->pers.inventory[self->client->pers.weapon->ammo])
        wp = nullptr;
      else if (!wp->drop)
        wp = nullptr;
      else if (RS(Quake3Arena) && wp->id == IT_WEAPON_MACHINEGUN)
        wp = nullptr;
      else if (RS(Quake1) && wp->id == IT_WEAPON_SHOTGUN)
        wp = nullptr;

      if (wp) {
        self->client->vAngle[YAW] = 0.0;
        drop = Drop_Item(self, wp);
        drop->spawnFlags |= SPAWNFLAG_ITEM_DROPPED_PLAYER;
        drop->spawnFlags &= ~SPAWNFLAG_ITEM_DROPPED;
        drop->svFlags &= ~SVF_INSTANCED;
      }
    }
  }

  // drop tech
  Tech_DeadDrop(self);

  // drop CTF flags
  CTF_DeadDropFlag(self);

  // drop powerup
  quad = (self->client->PowerupTimer(PowerupTimer::QuadDamage) >
          (level.time + 1_sec));
  haste =
      (self->client->PowerupTimer(PowerupTimer::Haste) > (level.time + 1_sec));
  doubled = (self->client->PowerupTimer(PowerupTimer::DoubleDamage) >
             (level.time + 1_sec));
  protection = (self->client->PowerupTimer(PowerupTimer::BattleSuit) >
                (level.time + 1_sec));
  invis = (self->client->PowerupTimer(PowerupTimer::Invisibility) >
           (level.time + 1_sec));
  regen = (self->client->PowerupTimer(PowerupTimer::Regeneration) >
           (level.time + 1_sec));

  if (!match_powerupDrops->integer) {
    quad = doubled = haste = protection = invis = regen = false;
  }

  if (quad) {
    self->client->vAngle[YAW] += 45.0;
    drop = Drop_Item(self, GetItemByIndex(IT_POWERUP_QUAD));
    drop->spawnFlags |= SPAWNFLAG_ITEM_DROPPED_PLAYER;
    drop->spawnFlags &= ~SPAWNFLAG_ITEM_DROPPED;
    drop->svFlags &= ~SVF_INSTANCED;

    drop->touch = Touch_Item;
    auto &quadTime = self->client->PowerupTimer(PowerupTimer::QuadDamage);
    drop->nextThink = quadTime;
    drop->think = g_quadhog->integer ? QuadHog_DoReset : FreeEntity;

    if (g_quadhog->integer) {
      drop->s.renderFX |= RF_SHELL_BLUE;
      drop->s.effects |= EF_COLOR_SHELL;
    }

    // decide how many seconds it has left
    drop->count = quadTime.seconds<int>() - level.time.seconds<int>();
    if (drop->count < 1) {
      drop->count = 1;
    }
  }

  if (haste) {
    self->client->vAngle[YAW] += 45;
    drop = Drop_Item(self, GetItemByIndex(IT_POWERUP_HASTE));
    drop->spawnFlags |= SPAWNFLAG_ITEM_DROPPED_PLAYER;
    drop->spawnFlags &= ~SPAWNFLAG_ITEM_DROPPED;
    drop->svFlags &= ~SVF_INSTANCED;

    drop->touch = Touch_Item;
    auto &hasteTime = self->client->PowerupTimer(PowerupTimer::Haste);
    drop->nextThink = hasteTime;
    drop->think = FreeEntity;

    // decide how many seconds it has left
    drop->count = hasteTime.seconds<int>() - level.time.seconds<int>();
    if (drop->count < 1) {
      drop->count = 1;
    }
  }

  if (protection) {
    self->client->vAngle[YAW] += 45;
    drop = Drop_Item(self, GetItemByIndex(IT_POWERUP_BATTLESUIT));
    drop->spawnFlags |= SPAWNFLAG_ITEM_DROPPED_PLAYER;
    drop->spawnFlags &= ~SPAWNFLAG_ITEM_DROPPED;
    drop->svFlags &= ~SVF_INSTANCED;

    drop->touch = Touch_Item;
    auto &battleSuitTime = self->client->PowerupTimer(PowerupTimer::BattleSuit);
    drop->nextThink = battleSuitTime;
    drop->think = FreeEntity;

    // decide how many seconds it has left
    drop->count = battleSuitTime.seconds<int>() - level.time.seconds<int>();
    if (drop->count < 1) {
      drop->count = 1;
    }
  }

  if (regen) {
    self->client->vAngle[YAW] += 45;
    drop = Drop_Item(self, GetItemByIndex(IT_POWERUP_REGEN));
    drop->spawnFlags |= SPAWNFLAG_ITEM_DROPPED_PLAYER;
    drop->spawnFlags &= ~SPAWNFLAG_ITEM_DROPPED;
    drop->svFlags &= ~SVF_INSTANCED;

    drop->touch = Touch_Item;
    auto &regenTime = self->client->PowerupTimer(PowerupTimer::Regeneration);
    drop->nextThink = regenTime;
    drop->think = FreeEntity;

    // decide how many seconds it has left
    drop->count = regenTime.seconds<int>() - level.time.seconds<int>();
    if (drop->count < 1) {
      drop->count = 1;
    }
  }

  if (invis) {
    self->client->vAngle[YAW] += 45;
    drop = Drop_Item(self, GetItemByIndex(IT_POWERUP_INVISIBILITY));
    drop->spawnFlags |= SPAWNFLAG_ITEM_DROPPED_PLAYER;
    drop->spawnFlags &= ~SPAWNFLAG_ITEM_DROPPED;
    drop->svFlags &= ~SVF_INSTANCED;

    drop->touch = Touch_Item;
    auto &invisTime = self->client->PowerupTimer(PowerupTimer::Invisibility);
    drop->nextThink = invisTime;
    drop->think = FreeEntity;

    // decide how many seconds it has left
    drop->count = invisTime.seconds<int>() - level.time.seconds<int>();
    if (drop->count < 1) {
      drop->count = 1;
    }
  }

  if (doubled) {
    self->client->vAngle[YAW] += 45;
    drop = Drop_Item(self, GetItemByIndex(IT_POWERUP_DOUBLE));
    drop->spawnFlags |= SPAWNFLAG_ITEM_DROPPED_PLAYER;
    drop->spawnFlags &= ~SPAWNFLAG_ITEM_DROPPED;
    drop->svFlags &= ~SVF_INSTANCED;

    drop->touch = Touch_Item;
    auto &doubleTime = self->client->PowerupTimer(PowerupTimer::DoubleDamage);
    drop->nextThink = doubleTime;
    drop->think = FreeEntity;

    // decide how many seconds it has left
    drop->count = doubleTime.seconds<int>() - level.time.seconds<int>();
    if (drop->count < 1) {
      drop->count = 1;
    }
  }

  self->client->vAngle[YAW] = 0.0;
}

/*
==================
LookAtKiller
==================
*/
void LookAtKiller(gentity_t *self, gentity_t *inflictor, gentity_t *attacker) {
  Vector3 dir;

  if (attacker && attacker != world && attacker != self) {
    dir = attacker->s.origin - self->s.origin;
  } else if (inflictor && inflictor != world && inflictor != self) {
    dir = inflictor->s.origin - self->s.origin;
  } else {
    self->client->killerYaw = self->s.angles[YAW];
    return;
  }

  // PMM - fixed to correct for pitch of 0
  if (dir[0])
    self->client->killerYaw = 180 / PIf * atan2f(dir[1], dir[0]);
  else if (dir[1] > 0)
    self->client->killerYaw = 90;
  else if (dir[1] < 0)
    self->client->killerYaw = 270;
  else
    self->client->killerYaw = 0;
}

/*
================
Match_CanScore
================
*/
[[maybe_unused]] static bool Match_CanScore() {
  if (level.intermission.queued)
    return false;

  switch (level.matchState) {
    using enum MatchState;
  case None:
    return false;
  case Initial_Delay:
  case Warmup_Default:
  case Warmup_ReadyUp:
  case Countdown:
  case Ended:
    return false;
  case In_Progress:
    break;
  }

  return true;
}

/*
==================
G_LogDeathEvent
==================
*/
static void G_LogDeathEvent(gentity_t *victim, gentity_t *attacker,
                            MeansOfDeath mod) {
  MatchDeathEvent ev;

  if (level.matchState != MatchState::In_Progress) {
    return;
  }
  if (!victim || !victim->client) {
    gi.Com_PrintFmt("{}: Invalid victim for death log\n", __FUNCTION__);
    return;
  }

  ev.time = level.time - level.levelStartTime;
  ev.victim.name.assign(victim->client->sess.netName);
  ev.victim.id.assign(victim->client->sess.socialID);
  if (attacker && attacker->client && attacker != &g_entities[0]) {
    ev.attacker.name.assign(attacker->client->sess.netName);
    ev.attacker.id.assign(attacker->client->sess.socialID);
  } else {
    ev.attacker.name = "Environment";
    ev.attacker.id = "0";
  }
  ev.mod = mod;

  try {
    std::lock_guard<std::mutex> logGuard(level.matchLogMutex);
    if (!level.match.deathLog.capacity()) {
      level.match.deathLog.reserve(2048);
    }
    level.match.deathLog.push_back(std::move(ev));
  } catch (const std::exception &e) {
    gi.Com_ErrorFmt("deathLog push_back failed: {}", e.what());
  }
}

/*
==================
PushDeathStats
==================
*/
static void PushDeathStats(gentity_t *victim, gentity_t *attacker,
                           const MeansOfDeath &mod) {
  auto now = level.time;
  auto &glob = level.match;
  auto *vcl = victim->client;
  auto &vSess = vcl->pers.match;
  bool isSuicide = (attacker == victim);
  bool validKill =
      (attacker && attacker->client && !isSuicide && !mod.friendly_fire);

  // -- handle a valid non-suicide kill --
  if (validKill) {
    auto *acl = attacker->client;
    auto &aSess = acl->pers.match;

    if (glob.totalKills == 0) {
      PushAward(attacker, PlayerMedal::First_Frag);
    }

    if (attacker->health > 0) {
      ++acl->killStreakCount;
    }

    if (Game::Has(GameFlags::Frags)) {
      G_AdjustPlayerScore(acl, 1,
                          Game::Is(GameType::TeamDeathmatch) ||
                              Game::Is(GameType::Domination),
                          1);
    }

    ++aSess.totalKills;
    ++aSess.modTotalKills[static_cast<int>(mod.id)];
    ++glob.totalKills;
    ++glob.modKills[static_cast<int>(mod.id)];
    if (now - victim->client->respawnMaxTime < 1_sec) {
      ++glob.totalSpawnKills;
      ++acl->pers.match.totalSpawnKills;
    }

    if (OnSameTeam(attacker, victim)) {
      ++glob.totalTeamKills;
      ++acl->pers.match.totalTeamKills;
    }

    if (acl->pers.lastFragTime && acl->pers.lastFragTime + 2_sec > now) {
      PushAward(attacker, PlayerMedal::Excellent);
    }
    acl->pers.lastFragTime = now;

    if (mod.id == ModID::Blaster || mod.id == ModID::Chainfist) {
      PushAward(attacker, PlayerMedal::Humiliation);
    }
  }

  // -- always record the victim's death --
  ++vSess.totalDeaths;
  ++glob.totalDeaths;
  ++glob.modDeaths[static_cast<int>(mod.id)];
  ++vSess.modTotalDeaths[static_cast<int>(mod.id)];

  if (isSuicide) {
    ++vSess.totalSuicides;
  } else if (now - victim->client->respawnMaxTime < 1_sec) {
    ++vSess.totalSpawnDeaths;
  }

  // -- penalty / follow-killer logic --
  bool inPlay = (level.matchState == MatchState::In_Progress);

  if (inPlay && attacker && attacker->client) {
    // attacker killed themselves or hit a teammate?
    if (isSuicide || mod.friendly_fire) {
      if (!mod.no_point_loss) {
        G_AdjustPlayerScore(attacker->client, -1,
                            Game::Is(GameType::TeamDeathmatch) ||
                                Game::Is(GameType::Domination),
                            -1);
      }
      attacker->client->killStreakCount = 0;
    } else {
      // queue any spectators who want to follow the killer
      for (auto ec : active_clients()) {
        if (!ClientIsPlaying(ec->client) && ec->client->sess.pc.follow_killer) {
          ec->client->follow.queuedTarget = attacker;
          ec->client->follow.queuedTime = now;
        }
      }
    }
  } else {
    // penalty to the victim
    if (!mod.no_point_loss) {
      G_AdjustPlayerScore(victim->client, -1,
                          Game::Is(GameType::TeamDeathmatch) ||
                              Game::Is(GameType::Domination),
                          -1);
    }
  }
}

/*
==================
GibPlayer
==================
*/
static void GibPlayer(gentity_t *self, int damage) {
  if (self->flags & FL_NOGIB) {
    return;
  }

  // 1) udeath sound
  gi.sound(self, CHAN_BODY, gi.soundIndex("misc/udeath.wav"), 1, ATTN_NORM, 0);

  // 2) meatier gibs at deeper overkills (deathmatch only)
  struct GibStage {
    int threshold;
    int count;
  };
  static constexpr std::array<GibStage, 3> gibStages{
      {{-300, 16}, {-200, 12}, {-100, 10}}};
  if (deathmatch->integer) {
    for (const auto &stage : gibStages) {
      if (self->health < stage.threshold) {
        ThrowGibs(self, damage,
                  {{size_t(stage.count), "models/objects/gibs/sm_meat/tris.md2",
                    GIB_NONE}});
      }
    }
  }

  // 3) always toss some small meat chunks
  ThrowGibs(self, damage,
            {{size_t(8), "models/objects/gibs/sm_meat/tris.md2", GIB_NONE}});

  // 4) calculate a 'severity' from 1 (just under -40) up to 4 (really deep
  // overkill)
  int overkill = GIB_HEALTH - self->health; // e.g. -40 - (-100) = 60
  int severity = (overkill > 0) ? (overkill / 40) + 1 : 1;
  severity = std::min(severity, 4);

  // 5) random leg gibs (up to 2)
  {
    int maxLegs = std::min(severity, 2);
    int legCount = irandom(maxLegs + 1); // uniform [0..maxLegs]
    if (legCount > 0) {
      ThrowGibs(
          self, damage,
          {{size_t(legCount), "models/objects/gibs/leg/tris.md2", GIB_NONE}});
    }
  }

  // 6) random bone gibs (up to 4)
  {
    int maxBones = std::min(severity * 2, 4);
    int boneCount = irandom(maxBones + 1);
    if (boneCount > 0) {
      ThrowGibs(
          self, damage,
          {{size_t(boneCount), "models/objects/gibs/bone/tris.md2", GIB_NONE}});
    }
  }

  // 7) random forearm bones (up to 2)
  {
    int maxBone2 = std::min(severity, 2);
    int bone2Count = irandom(maxBone2 + 1);
    if (bone2Count > 0) {
      ThrowGibs(self, damage,
                {{size_t(bone2Count), "models/objects/gibs/bone2/tris.md2",
                  GIB_NONE}});
    }
  }

  // 8) random arm bones (up to 2)
  {
    int maxArms = std::min(severity, 2);
    int armCount = irandom(maxArms + 1);
    if (armCount > 0) {
      ThrowGibs(
          self, damage,
          {{size_t(armCount), "models/objects/gibs/arm/tris.md2", GIB_NONE}});
    }
  }
}

bool FreezeTag_IsActive() {
  return Game::Is(GameType::FreezeTag) && !level.intermission.time;
}

bool FreezeTag_IsFrozen(const gentity_t *ent) {
  return FreezeTag_IsActive() && ent && ent->client && ent->client->eliminated;
}

static GameTime FreezeTag_Duration() {
  if (!g_frozen_time)
    return 0_ms;

  return GameTime::from_sec(std::max(0.0f, g_frozen_time->value));
}

static void FreezeTag_ResetState(gclient_t *cl) {
  if (!cl)
    return;

  cl->freeze.frozenTime = 0_ms;
  cl->freeze.thawTime = 0_ms;
  cl->freeze.holdDeadline = 0_ms;
  cl->resp.thawer = nullptr;
}

static void FreezeTag_StartFrozenState(gentity_t *ent) {
  if (!ent || !ent->client)
    return;

  gclient_t *cl = ent->client;

  cl->eliminated = true;
  cl->resp.thawer = nullptr;
  cl->freeze.frozenTime = level.time;
  cl->freeze.holdDeadline = 0_ms;

  const GameTime thawDuration = FreezeTag_Duration();

  if (thawDuration > 0_ms) {
    cl->freeze.thawTime = level.time + thawDuration;
    cl->respawnMinTime = cl->freeze.thawTime;
    cl->respawnMaxTime = cl->freeze.thawTime;
  } else {
    cl->freeze.thawTime = 0_ms;
    const GameTime hold = level.time + 86400_sec;
    cl->respawnMinTime = hold;
    cl->respawnMaxTime = hold;
  }
}

static bool FreezeTag_CanThawTarget(gentity_t *thawer, gentity_t *frozen) {
  if (!FreezeTag_IsActive())
    return false;

  if (!thawer || !thawer->client || !frozen || !frozen->client)
    return false;

  if (!ClientIsPlaying(thawer->client) || thawer->client->eliminated)
    return false;

  if (!ClientIsPlaying(frozen->client) || !frozen->client->eliminated)
    return false;

  if (!Teams() || thawer->client->sess.team != frozen->client->sess.team)
    return false;

  if (frozen->client->resp.thawer && frozen->client->resp.thawer != thawer)
    return false;

  return true;
}

/*
=============
FreezeTag_FindFrozenTarget

Locates the best frozen teammate within range of the thawer, prioritizing
line-of-sight and directional alignment.
=============
*/
gentity_t *worr::server::client::FreezeTag_FindFrozenTarget(gentity_t *thawer) {
  if (!FreezeTag_IsActive() || !thawer || !thawer->client)
    return nullptr;

  constexpr float THAW_RANGE = 96.0f;

  Vector3 forward;
  AngleVectors(thawer->client->vAngle, forward, nullptr, nullptr);

  Vector3 eyeOrigin =
      thawer->s.origin + thawer->client->ps.viewOffset +
      Vector3{0, 0, static_cast<float>(thawer->client->ps.pmove.viewHeight)};

  trace_t tr = gi.traceLine(eyeOrigin, eyeOrigin + forward * THAW_RANGE, thawer,
                            MASK_SHOT);
  if (tr.ent && FreezeTag_CanThawTarget(thawer, tr.ent))
    return tr.ent;

  gentity_t *best = nullptr;
  float bestDot = 0.0f;

  for (gentity_t *candidate : active_clients()) {
    if (!FreezeTag_CanThawTarget(thawer, candidate))
      continue;

    Vector3 toTarget = candidate->s.origin - thawer->s.origin;
    const float distance = toTarget.length();
    if (distance > THAW_RANGE)
      continue;

    Vector3 dir = toTarget.normalized();
    const float dot = dir.dot(forward);
    if (dot < 0.35f)
      continue;

    if (gi.traceLine(eyeOrigin, candidate->s.origin, thawer, MASK_SHOT)
            .fraction != 1.0f)
      continue;

    if (!best || dot > bestDot) {
      best = candidate;
      bestDot = dot;
    }
  }

  return best;
}

static constexpr GameTime FREEZETAG_THAW_HOLD_DURATION = 3_sec;
static constexpr float FREEZETAG_THAW_RANGE = MELEE_DISTANCE;

/*
=============
FreezeTag_IsValidThawHelper

Checks if a thawer is eligible to thaw the frozen teammate based on distance,
team alignment, and active state.
=============
*/
bool worr::server::client::FreezeTag_IsValidThawHelper(gentity_t *thawer,
                                                       gentity_t *frozen) {
  if (!FreezeTag_IsActive())
    return false;

  if (!thawer || !thawer->client || !frozen || !frozen->client)
    return false;

  if (thawer == frozen)
    return false;

  if (!ClientIsPlaying(thawer->client) || thawer->client->eliminated)
    return false;

  if (!ClientIsPlaying(frozen->client) || !frozen->client->eliminated)
    return false;

  if (!Teams() || thawer->client->sess.team != frozen->client->sess.team)
    return false;

  const Vector3 delta = frozen->s.origin - thawer->s.origin;
  return delta.length() <= FREEZETAG_THAW_RANGE;
}

static gentity_t *FreezeTag_FindNearbyThawer(gentity_t *frozen) {
  gentity_t *best = nullptr;
  float bestDistance = 0.0f;

  for (gentity_t *candidate : active_clients()) {
    if (!worr::server::client::FreezeTag_IsValidThawHelper(candidate, frozen))
      continue;

    const float distance = (frozen->s.origin - candidate->s.origin).length();

    if (!best || distance < bestDistance) {
      best = candidate;
      bestDistance = distance;
    }
  }

  return best;
}

static void FreezeTag_StopThawHold(gentity_t *frozen, bool notify) {
  if (!frozen || !frozen->client)
    return;

  gclient_t *fcl = frozen->client;
  gentity_t *thawer = fcl->resp.thawer;

  if (notify && thawer && thawer->client) {
    gi.LocClient_Print(thawer, PRINT_CENTER, "$g_sgame_auto_0979fa106e6c",
                       fcl->sess.netName);
    gi.LocClient_Print(frozen, PRINT_CENTER, "$g_sgame_auto_311f822919dd",
                       thawer->client->sess.netName);
  }

  fcl->resp.thawer = nullptr;
  fcl->freeze.holdDeadline = 0_ms;
}

/*
=============
FreezeTag_StartThawHold

Begins the timed thaw-hold interaction between the thawer and frozen player.
=============
*/
void worr::server::client::FreezeTag_StartThawHold(gentity_t *thawer,
                                                   gentity_t *frozen) {
  if (!frozen || !frozen->client || !thawer || !thawer->client)
    return;

  gclient_t *fcl = frozen->client;

  fcl->resp.thawer = thawer;
  fcl->freeze.holdDeadline = level.time + FREEZETAG_THAW_HOLD_DURATION;

  gi.sound(frozen, CHAN_AUTO, gi.soundIndex("world/steam.wav"), 1, ATTN_NORM,
           0);
  gi.LocClient_Print(thawer, PRINT_CENTER, "$g_sgame_auto_01aa97dfc24a",
                     fcl->sess.netName);
  gi.LocClient_Print(frozen, PRINT_CENTER, "$g_sgame_auto_fdb011c6e37e",
                     thawer->client->sess.netName);
}

/*
=============
FreezeTag_ThawPlayer

Handles thaw completion, scoring, and respawn reset for a frozen teammate.
=============
*/
void worr::server::client::FreezeTag_ThawPlayer(gentity_t *thawer,
                                                gentity_t *frozen,
                                                bool awardScore,
                                                bool autoThaw) {
  if (!frozen || !frozen->client || !FreezeTag_IsFrozen(frozen))
    return;

  gclient_t *fcl = frozen->client;

  if (thawer == frozen)
    thawer = nullptr;

  fcl->resp.thawer = thawer;

  if (thawer && thawer->client && awardScore) {
    ++thawer->client->resp.thawed;
    G_AdjustPlayerScore(thawer->client, 1, false, 0);
    gi.LocClient_Print(thawer, PRINT_CENTER, "$g_sgame_auto_d2e00686ef3e",
                       frozen->client->sess.netName);
  }

  if (thawer && thawer->client) {
    gi.LocClient_Print(frozen, PRINT_CENTER, "$g_sgame_auto_b5a10a49253a",
                       thawer->client->sess.netName);
  } else if (autoThaw) {
    gi.LocClient_Print(frozen, PRINT_CENTER, "$g_sgame_auto_6b9b4499a953");
  }

  MeansOfDeath thawMod{ModID::Thaw, false};
  frozen->lastMOD = thawMod;

  if (frozen->health > frozen->gibHealth)
    frozen->health = frozen->gibHealth - 1;

  GibPlayer(frozen, 400);
  ThrowClientHead(frozen, 400);

  fcl->freeze.thawTime = 0_ms;
  fcl->freeze.frozenTime = 0_ms;
  fcl->freeze.holdDeadline = 0_ms;
  fcl->eliminated = false;
  fcl->respawnMinTime = level.time;
  fcl->respawnMaxTime = level.time;

  ClientRespawn(frozen);
}

/*
=============
FreezeTag_UpdateThawHold

Maintains thaw progress, auto-selecting helpers or completing the thaw when the
hold timer elapses.
=============
*/
bool worr::server::client::FreezeTag_UpdateThawHold(gentity_t *frozen) {
  if (!FreezeTag_IsActive() || !frozen || !frozen->client ||
      !frozen->client->eliminated)
    return false;

  gclient_t *fcl = frozen->client;
  gentity_t *thawer = fcl->resp.thawer;

  if (thawer) {
    if (!FreezeTag_IsValidThawHelper(thawer, frozen)) {
      FreezeTag_StopThawHold(frozen, true);
    } else if (fcl->freeze.holdDeadline &&
               level.time >= fcl->freeze.holdDeadline) {
      FreezeTag_ThawPlayer(thawer, frozen, true, false);
      return true;
    }
  }

  if (!fcl->resp.thawer) {
    gentity_t *helper = FreezeTag_FindNearbyThawer(frozen);

    if (helper) {
      FreezeTag_StartThawHold(helper, frozen);
    } else {
      fcl->freeze.holdDeadline = 0_ms;
    }
  }

  return false;
}

void FreezeTag_ForceRespawn(gentity_t *ent) {
  if (!FreezeTag_IsFrozen(ent))
    return;

  worr::server::client::FreezeTag_ThawPlayer(nullptr, ent, false, true);
}

/*
==================
player_die
==================
*/
DIE(player_die)(gentity_t *self, gentity_t *inflictor, gentity_t *attacker,
                int damage, const Vector3 &point, const MeansOfDeath &mod)
    ->void {
  if (self->client->ps.pmove.pmType == PM_DEAD)
    return;

  if (level.intermission.time)
    return;

  PlayerTrail_Destroy(self);

  self->aVelocity = {};

  self->takeDamage = true;
  self->moveType = MoveType::Toss;

  self->s.modelIndex2 = 0; // remove linked weapon model
  self->s.modelIndex3 = 0; // remove linked ctf flag

  self->s.angles[PITCH] = 0;
  self->s.angles[ROLL] = 0;

  self->s.sound = 0;
  self->client->weaponSound = 0;

  self->maxs[2] = -8;

  self->svFlags |= SVF_DEADMONSTER;
  self->svFlags &= ~SVF_INSTANCED;
  self->s.instanceBits = 0;

  if (!self->deadFlag) {

    if (deathmatch->integer) {
      if (match_playerRespawnMinDelay->value) {
        self->client->respawnMinTime =
            (level.time +
             GameTime::from_sec(match_playerRespawnMinDelay->value));
      } else {
        self->client->respawnMinTime = level.time;
      }

      if (match_forceRespawnTime->value) {
        self->client->respawnMaxTime =
            (level.time + GameTime::from_sec(match_forceRespawnTime->value));
      } else {
        self->client->respawnMaxTime = (level.time + 1_sec);
      }
    }

    PushDeathStats(self, attacker, mod);

    LookAtKiller(self, inflictor, attacker);

    self->client->deathView.active = true;
    self->client->deathView.startTime = level.time;
    self->client->deathView.startOffset = self->client->ps.viewOffset;

    self->client->ps.pmove.pmType = PM_DEAD;
    ClientObituary(self, inflictor, attacker, mod);

    CTF_ScoreBonuses(self, inflictor, attacker);
    ProBall::HandleCarrierDeath(self);
    Harvester_HandlePlayerDeath(self);
    HeadHunters::DropHeads(self, attacker);
    TossClientItems(self);
    Weapon_Grapple_DoReset(self->client);

    if (deathmatch->integer && !self->client->showScores)
      Commands::Help(self, CommandArgs{}); // show scores

    if (coop->integer && !P_UseCoopInstancedItems()) {
      // clear inventory
      // this is kind of ugly, but it's how we want to handle keys in coop
      for (int n = 0; n < IT_TOTAL; n++) {
        if (itemList[n].flags & IF_KEY)
          self->client->resp.coopRespawn.inventory[n] =
              self->client->pers.inventory[n];
        self->client->pers.inventory[n] = 0;
      }
    }
  }

  // remove powerups
  self->client->PowerupTimer(PowerupTimer::QuadDamage) = 0_ms;
  self->client->PowerupTimer(PowerupTimer::Haste) = 0_ms;
  self->client->PowerupTimer(PowerupTimer::DoubleDamage) = 0_ms;
  self->client->PowerupTimer(PowerupTimer::BattleSuit) = 0_ms;
  self->client->PowerupTimer(PowerupTimer::Invisibility) = 0_ms;
  self->client->PowerupTimer(PowerupTimer::Regeneration) = 0_ms;
  self->client->PowerupTimer(PowerupTimer::Rebreather) = 0_ms;
  self->client->PowerupTimer(PowerupTimer::EnviroSuit) = 0_ms;
  self->flags &= ~FL_POWER_ARMOR;

  self->client->lastDeathLocation = self->s.origin;

  // add damage event to heatmap
  HM_AddEvent(self->s.origin, 50.0f);

  // clear inventory
  if (Teams())
    self->client->pers.inventory.fill(0);

  // if there's a sphere around, let it know the player died.
  // vengeance and hunter will die if they're not attacking,
  // defender should always die
  if (self->client->ownedSphere) {
    gentity_t *sphere;

    sphere = self->client->ownedSphere;
    sphere->die(sphere, self, self, 0, vec3_origin, mod);
  }

  // if we've been killed by the tracker, GIB!
  if (mod.id == ModID::Tracker) {
    self->health = -100;
    damage = 400;
  }

  if (FreezeTag_IsActive() && self->client->eliminated) {
    self->s.effects |= EF_COLOR_SHELL;
    self->s.renderFX |= (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE);
  } else {
    self->s.effects = EF_NONE;
    self->s.renderFX = RF_NONE;
  }

  // make sure no trackers are still hurting us.
  if (self->client->trackerPainTime) {
    RemoveAttackingPainDaemons(self);
  }

  // if we got obliterated by the nuke, don't gib
  if ((self->health < -80) && (mod.id == ModID::Nuke))
    self->flags |= FL_NOGIB;

  if (self->health < GIB_HEALTH) {
    GibPlayer(self, damage);

    // clear the "no-gib" flag in case it was set
    self->flags &= ~FL_NOGIB;

    ThrowClientHead(self, damage);

    // lock in a "dead" animation frame so we don't play the normal death anim
    self->client->anim.priority = ANIM_DEATH;
    self->client->anim.end = 0;
    self->takeDamage = false;

  } else {
    // --- normal death animation & sound ---
    if (!self->deadFlag) {
      // Freeze-mode gets a single static pose
      if (Game::Is(GameType::FreezeTag)) {
        self->s.frame = FRAME_crstnd01 - 1;
        self->client->anim.end = self->s.frame;
      } else {
        // pick one of the death animations
        self->client->anim.priority = ANIM_DEATH;
        bool ducked = (self->client->ps.pmove.pmFlags & PMF_DUCKED) != 0;

        if (ducked) {
          self->s.frame = FRAME_crdeath1 - 1;
          self->client->anim.end = FRAME_crdeath5;
        } else {
          static constexpr std::array<std::pair<int, int>, 3> deathRanges = {
              {{FRAME_death101, FRAME_death106},
               {FRAME_death201, FRAME_death206},
               {FRAME_death301, FRAME_death308}}};

          const auto &[start, end] = deathRanges[irandom(3)];

          self->s.frame = start - 1;
          self->client->anim.end = end;
        }
      }

      // play one of four death cries
      static constexpr std::array<const char *, 4> death_sounds = {
          "*death1.wav", "*death2.wav", "*death3.wav", "*death4.wav"};
      gi.sound(self, CHAN_VOICE, gi.soundIndex(random_element(death_sounds)), 1,
               ATTN_NORM, 0);

      self->client->anim.time = 0_ms;
    }
  }

  if (!self->deadFlag) {
    if (G_LimitedLivesInCoop()) {
      if (self->client->pers.lives > 0) {
        self->client->pers.lives--;
        self->client->pers.limitedLivesStash = self->client->pers.lives;
        self->client->pers.limitedLivesPersist = true;
        if (self->client->resp.coopRespawn.lives > 0)
          self->client->resp.coopRespawn.lives--;
      }

      bool allPlayersDead = true;

      for (auto player : active_clients())
        if (player->health > 0 || (!level.campaign.deadly_kill_box &&
                                   player->client->pers.lives > 0)) {
          allPlayersDead = false;
          break;
        }

      if (allPlayersDead) {
        level.campaign.coopLevelRestartTime = level.time + 5_sec;

        for (auto player : active_clients())
          gi.LocCenter_Print(player, "$g_coop_lose");
      }

      if (!level.campaign.coopLevelRestartTime)
        self->client->respawnMaxTime = level.time + 3_sec;
    } else if (G_LimitedLivesInLMS()) {
      if (self->client->pers.lives > 0) {
        self->client->pers.lives--;
        self->client->pers.limitedLivesStash = self->client->pers.lives;
        self->client->pers.limitedLivesPersist = true;

        if (self->client->pers.lives > 0)
          self->client->coopRespawnState = CoopRespawn::None;
      }

      if (self->client->pers.lives == 0) {
        self->client->eliminated = true;
        self->client->coopRespawnState = CoopRespawn::NoLives;
        CalculateRanks();
      }
    }
  }

  if (FreezeTag_IsActive()) {
    FreezeTag_StartFrozenState(self);
  } else {
    FreezeTag_ResetState(self->client);
  }

  G_LogDeathEvent(self, attacker, mod);

  self->deadFlag = true;

  gi.linkEntity(self);
}

//=======================================================================

#include <string>
// #include <sstream>

/*
===========================
Player_GiveStartItems
===========================
*/
static void Player_GiveStartItems(gentity_t *ent, const char *input) {
  const char *token;
  while ((token = COM_ParseEx(&input, ";")) && *token) {
    char token_copy[MAX_TOKEN_CHARS];
    Q_strlcpy(token_copy, token, sizeof(token_copy));

    const char *cursor = token_copy;
    const char *item_name = COM_Parse(&cursor);
    if (!item_name || !*item_name)
      continue;

    Item *item = FindItemByClassname(item_name);
    if (!item || !item->pickup) {
      gi.Com_PrintFmt("Invalid g_start_item entry: '{}'\n", item_name);
      continue;
    }

    int32_t count = 1;
    if (cursor && *cursor) {
      const char *count_str = COM_Parse(&cursor);
      if (count_str && *count_str)
        count = std::clamp(strtol(count_str, nullptr, 10), 0L, 999L);
    }

    if (count == 0) {
      ent->client->pers.inventory[item->id] = 0;
      continue;
    }

    const int item_id = static_cast<int>(item->id);
    if (item_id >= MAX_ITEMS) {
      gi.Com_PrintFmt("Item '{}' has invalid ID {}\n", item_name, item_id);
      continue;
    }

    gentity_t *dummy = Spawn();
    dummy->item = item;
    dummy->count = count;
    dummy->spawnFlags |= SPAWNFLAG_ITEM_DROPPED;
    item->pickup(dummy, ent);
    FreeEntity(dummy);
  }
}

/*
==============
InitClientPersistant

This is only called when the game first initializes in single player,
but is called after each death and level change in deathmatch
==============
*/
void InitClientPersistant(gentity_t *ent, gclient_t *client) {
  // backup & restore userInfo
  char userInfo[MAX_INFO_STRING];
  Q_strlcpy(userInfo, client->pers.userInfo, sizeof(userInfo));

  const int savedCurrentRank = client->pers.currentRank;
  const int savedPreviousRank = client->pers.previousRank;

  client->pers = client_persistant_t{};

  ClientUserinfoChanged(ent, userInfo);

  client->pers.currentRank = savedCurrentRank;
  client->pers.previousRank = savedPreviousRank;

  client->pers.health = 100;
  client->pers.maxHealth = 100;

  // don't give us weapons if we shouldn't have any
  if (ClientIsPlaying(client)) {
    // in coop, if there's already a player in the game and we're new,
    // steal their loadout. this would fix a potential softlock where a new
    // player may not have weapons at all.
    bool taken_loadout = false;

    int health, armor;
    gitem_armor_t armorType = armor_stats[game.ruleset][Armor::Jacket];

    if (Game::Has(GameFlags::Arena)) {
      health = std::clamp(g_arenaStartingHealth->integer, 1, 9999);
      armor = std::clamp(g_arenaStartingArmor->integer, 0, 999);
    } else {
      health = std::clamp(g_starting_health->integer, 1, 9999);
      armor = std::clamp(g_starting_armor->integer, 0, 999);
    }

    if (armor > armor_stats[game.ruleset][Armor::Jacket].max_count) {
      if (armor > armor_stats[game.ruleset][Armor::Combat].max_count)
        armorType = armor_stats[game.ruleset][Armor::Body];
      else
        armorType = armor_stats[game.ruleset][Armor::Combat];
    }

    client->pers.health = client->pers.maxHealth = health;

    if (deathmatch->integer) {
      int bonus = RS(Quake3Arena) ? 25 : g_starting_health_bonus->integer;
      if (!(Game::Has(GameFlags::Arena)) && bonus > 0) {
        client->pers.health += bonus;
        if (!(RS(Quake3Arena))) {
          client->pers.healthBonus = bonus;
        }
        ent->client->timeResidual = level.time;
      }
    }

    if (armorType.base_count ==
        armor_stats[game.ruleset][Armor::Jacket].base_count)
      client->pers.inventory[IT_ARMOR_JACKET] = armor;
    else if (armorType.base_count ==
             armor_stats[game.ruleset][Armor::Combat].base_count)
      client->pers.inventory[IT_ARMOR_COMBAT] = armor;
    else if (armorType.base_count ==
             armor_stats[game.ruleset][Armor::Body].base_count)
      client->pers.inventory[IT_ARMOR_BODY] = armor;

    if (coop->integer) {
      for (auto player : active_clients()) {
        if (player == ent || !player->client->pers.spawned ||
            !ClientIsPlaying(player->client) ||
            player->moveType == MoveType::NoClip ||
            player->moveType == MoveType::FreeCam)
          continue;

        client->pers.inventory = player->client->pers.inventory;
        client->pers.ammoMax = player->client->pers.ammoMax;
        client->pers.powerCubes = player->client->pers.powerCubes;
        taken_loadout = true;
        break;
      }
    }

    if (Game::Is(GameType::ProBall)) {
      client->pers.inventory[IT_WEAPON_CHAINFIST] = 1;
    } else if (!taken_loadout) {
      if (g_instaGib->integer) {
        client->pers.inventory[IT_WEAPON_RAILGUN] = 1;
        client->pers.inventory[IT_AMMO_SLUGS] = AMMO_INFINITE;
      } else if (g_nadeFest->integer) {
        client->pers.inventory[IT_AMMO_GRENADES] = AMMO_INFINITE;
      } else if (Game::Has(GameFlags::Arena)) {
        client->pers.ammoMax.fill(50);
        client->pers.ammoMax[static_cast<int>(AmmoID::Shells)] = 50;
        client->pers.ammoMax[static_cast<int>(AmmoID::Bullets)] = 300;
        client->pers.ammoMax[static_cast<int>(AmmoID::Grenades)] = 50;
        client->pers.ammoMax[static_cast<int>(AmmoID::Rockets)] = 50;
        client->pers.ammoMax[static_cast<int>(AmmoID::Cells)] = 200;
        client->pers.ammoMax[static_cast<int>(AmmoID::Slugs)] = 25;
        /*
        client->pers.ammoMax[AmmoID::Traps] = 5;
        client->pers.ammoMax[AmmoID::Flechettes] = 200;
        client->pers.ammoMax[AmmoID::Rounds] = 12;
        client->pers.ammoMax[AmmoID::TeslaMines] = 5;
        */
        client->pers.inventory[IT_AMMO_SHELLS] = 50;
        if (!(RS(Quake1))) {
          client->pers.inventory[IT_AMMO_BULLETS] = 200;
          client->pers.inventory[IT_AMMO_GRENADES] = 50;
        }
        client->pers.inventory[IT_AMMO_ROCKETS] = 50;
        client->pers.inventory[IT_AMMO_CELLS] = 200;
        if (!(RS(Quake1)))
          client->pers.inventory[IT_AMMO_SLUGS] = 50;

        client->pers.inventory[IT_WEAPON_BLASTER] = 1;
        client->pers.inventory[IT_WEAPON_SHOTGUN] = 1;
        if (!(RS(Quake3Arena)))
          client->pers.inventory[IT_WEAPON_SSHOTGUN] = 1;
        if (!(RS(Quake1))) {
          client->pers.inventory[IT_WEAPON_MACHINEGUN] = 1;
          client->pers.inventory[IT_WEAPON_CHAINGUN] = 1;
        }
        client->pers.inventory[IT_WEAPON_GLAUNCHER] = 1;
        client->pers.inventory[IT_WEAPON_RLAUNCHER] = 1;
        client->pers.inventory[IT_WEAPON_HYPERBLASTER] = 1;
        client->pers.inventory[IT_WEAPON_PLASMAGUN] = 1;
        client->pers.inventory[IT_WEAPON_PLASMABEAM] = 1;
        if (!(RS(Quake1)))
          client->pers.inventory[IT_WEAPON_RAILGUN] = 1;
      } else {
        if (RS(Quake3Arena)) {
          client->pers.ammoMax.fill(200);
          client->pers.ammoMax[static_cast<int>(AmmoID::Bullets)] = 200;
          client->pers.ammoMax[static_cast<int>(AmmoID::Shells)] = 200;
          client->pers.ammoMax[static_cast<int>(AmmoID::Cells)] = 200;

          client->pers.ammoMax[static_cast<int>(AmmoID::Traps)] = 200;
          client->pers.ammoMax[static_cast<int>(AmmoID::Flechettes)] = 200;
          client->pers.ammoMax[static_cast<int>(AmmoID::Rounds)] = 200;
          client->pers.ammoMax[static_cast<int>(AmmoID::TeslaMines)] = 200;

          client->pers.inventory[IT_WEAPON_CHAINFIST] = 1;
          client->pers.inventory[IT_WEAPON_MACHINEGUN] = 1;
          client->pers.inventory[IT_AMMO_BULLETS] =
              (Game::Is(GameType::TeamDeathmatch) ||
               Game::Is(GameType::Domination))
                  ? 50
                  : 100;
        } else if (RS(Quake1)) {
          client->pers.ammoMax.fill(200);
          client->pers.ammoMax[static_cast<int>(AmmoID::Bullets)] = 200;
          client->pers.ammoMax[static_cast<int>(AmmoID::Shells)] = 200;
          client->pers.ammoMax[static_cast<int>(AmmoID::Cells)] = 200;

          client->pers.ammoMax[static_cast<int>(AmmoID::Traps)] = 200;
          client->pers.ammoMax[static_cast<int>(AmmoID::Flechettes)] = 200;
          client->pers.ammoMax[static_cast<int>(AmmoID::Rounds)] = 200;
          client->pers.ammoMax[static_cast<int>(AmmoID::TeslaMines)] = 200;

          client->pers.inventory[IT_WEAPON_CHAINFIST] = 1;
          client->pers.inventory[IT_WEAPON_SHOTGUN] = 1;
          client->pers.inventory[IT_AMMO_SHELLS] = 10;
        } else {
          // fill with 50s, since it's our most common value
          client->pers.ammoMax.fill(50);
          client->pers.ammoMax[static_cast<int>(AmmoID::Bullets)] = 200;
          client->pers.ammoMax[static_cast<int>(AmmoID::Shells)] = 100;
          client->pers.ammoMax[static_cast<int>(AmmoID::Cells)] = 200;

          client->pers.ammoMax[static_cast<int>(AmmoID::Traps)] = 5;
          client->pers.ammoMax[static_cast<int>(AmmoID::Flechettes)] = 200;
          client->pers.ammoMax[static_cast<int>(AmmoID::Rounds)] = 12;
          client->pers.ammoMax[static_cast<int>(AmmoID::TeslaMines)] = 5;

          client->pers.inventory[IT_WEAPON_BLASTER] = 1;
        }

        if (deathmatch->integer) {
          if (level.matchState < MatchState::In_Progress) {
            for (size_t i = FIRST_WEAPON; i < LAST_WEAPON; i++) {
              if (!level.weaponCount[i - FIRST_WEAPON])
                continue;

              if (!itemList[i].ammo)
                continue;

              client->pers.inventory[i] = 1;

              Item *ammo = GetItemByIndex(itemList[i].ammo);
              if (ammo)
                Add_Ammo(&g_entities[client - game.clients + 1], ammo,
                         InfiniteAmmoOn(ammo) ? AMMO_INFINITE
                                              : ammo->quantity * 2);

              // gi.Com_PrintFmt("wp={} wc={} am={} q={}\n", i,
              // level.weaponCount[i - FIRST_WEAPON], itemList[i].ammo,
              // InfiniteAmmoOn(ammo) ? AMMO_INFINITE : ammo->quantity * 2);
            }
          }
        }
      }

      if (*g_start_items->string)
        Player_GiveStartItems(ent, g_start_items->string);
      if (level.start_items && *level.start_items)
        Player_GiveStartItems(ent, level.start_items);

      if (!deathmatch->integer || level.matchState < MatchState::In_Progress)
        // compass also used for ready status toggling in deathmatch
        client->pers.inventory[IT_COMPASS] = 1;

      bool give_grapple =
          (!strcmp(g_allow_grapple->string, "auto"))
              ? (Game::Has(GameFlags::CTF) ? !level.no_grapple : 0)
              : (g_allow_grapple->integer > 0 && !g_grapple_offhand->integer);
      if (give_grapple)
        client->pers.inventory[IT_WEAPON_GRAPPLE] = 1;
    }

    NoAmmoWeaponChange(ent, false);

    client->pers.weapon = client->weapon.pending;
    if (client->weapon.pending)
      client->pers.selectedItem = client->weapon.pending->id;
    client->weapon.pending = nullptr;
    client->pers.lastWeapon = client->pers.weapon;
  }

  client->pers.limitedLivesPersist = false;
  client->pers.limitedLivesStash = 0;
  if (G_LimitedLivesActive()) {
    client->pers.lives = G_LimitedLivesMax();
    client->pers.limitedLivesStash = client->pers.lives;
  }

  if (ent->client->pers.autoshield >= AUTO_SHIELD_AUTO)
    ent->flags |= FL_WANTS_POWER_ARMOR;

  client->pers.connected = true;
  client->pers.spawned = true;

  P_RestoreFromGhostSlot(ent);
}

/*
=============
InitClientResp

Resets the client's respawn snapshot, optionally preserving match stats for
active marathon legs.
=============
*/
void worr::server::client::InitClientResp(gclient_t *cl) {
  const bool preserveScore = game.marathon.active && game.marathon.legIndex > 0;
  const int32_t savedScore = preserveScore ? cl->resp.score : 0;
  const int64_t savedPlayTime =
      preserveScore ? cl->resp.totalMatchPlayRealTime : 0;

  cl->resp = client_respawn_t{};

  cl->resp.enterTime = level.time;
  cl->resp.coopRespawn = cl->pers;

  if (preserveScore) {
    cl->resp.score = savedScore;
    cl->resp.totalMatchPlayRealTime = savedPlayTime;
  }
}

/*
==================
SaveClientData

Some information that should be persistant, like health,
is still stored in the entity structure, so it needs to
be mirrored out to the client structure before all the
gentities are wiped.
==================
*/
void SaveClientData() {
  gentity_t *ent;

  for (size_t i = 0; i < game.maxClients; i++) {
    ent = &g_entities[1 + i];
    if (!ent->inUse)
      continue;
    game.clients[i].pers.health = ent->health;
    game.clients[i].pers.maxHealth = ent->maxHealth;
    game.clients[i].pers.saved_flags =
        (ent->flags & (FL_FLASHLIGHT | FL_GODMODE | FL_NOTARGET |
                       FL_POWER_ARMOR | FL_WANTS_POWER_ARMOR));
    if (coop->integer)
      game.clients[i].pers.score = ent->client->resp.score;
  }
}

void FetchClientEntData(gentity_t *ent) {
  ent->health = ent->client->pers.health;
  ent->maxHealth = ent->client->pers.maxHealth;
  ent->flags |= ent->client->pers.saved_flags;
  if (coop->integer)
    G_SetPlayerScore(ent->client, ent->client->pers.score);
}

//======================================================================

void InitBodyQue() {
  gentity_t *ent;

  level.bodyQue = 0;
  for (size_t i = 0; i < BODY_QUEUE_SIZE; i++) {
    ent = Spawn();
    ent->className = "bodyque";
  }
}

static DIE(body_die)(gentity_t *self, gentity_t *inflictor, gentity_t *attacker,
                     int damage, const Vector3 &point, const MeansOfDeath &mod)
    ->void {
  if (self->s.modelIndex == MODELINDEX_PLAYER &&
      self->health < self->gibHealth) {
    gi.sound(self, CHAN_BODY, gi.soundIndex("misc/udeath.wav"), 1, ATTN_NORM,
             0);
    ThrowGibs(self, damage, {{4, "models/objects/gibs/sm_meat/tris.md2"}});
    self->s.origin[_Z] -= 48;
    ThrowClientHead(self, damage);
  }

  if (mod.id == ModID::Crushed) {
    // prevent explosion singularities
    self->svFlags = SVF_NOCLIENT;
    self->takeDamage = false;
    self->solid = SOLID_NOT;
    self->moveType = MoveType::NoClip;
    gi.linkEntity(self);
  }
}

/*
=============
BodySink

After sitting around for x seconds, fall into the ground and disappear
=============
*/
static THINK(BodySink)(gentity_t *ent)->void {
  if (!ent->linked)
    return;

  if (level.time > ent->timeStamp) {
    ent->svFlags = SVF_NOCLIENT;
    ent->takeDamage = false;
    ent->solid = SOLID_NOT;
    ent->moveType = MoveType::NoClip;

    // the body ques are never actually freed, they are just unlinked
    gi.unlinkEntity(ent);
    return;
  }
  ent->nextThink = level.time + 50_ms;
  ent->s.origin[_Z] -= 0.5;
  gi.linkEntity(ent);
}

void CopyToBodyQue(gentity_t *ent) {
  // if we were completely removed, don't bother with a body
  if (!ent->s.modelIndex)
    return;

  gentity_t *body;
  bool frozen = FreezeTag_IsActive() && ent->client && ent->client->eliminated;

  // grab a body que and cycle to the next one
  body = &g_entities[game.maxClients + level.bodyQue + 1];
  level.bodyQue = (static_cast<size_t>(level.bodyQue) + 1u) % BODY_QUEUE_SIZE;

  // FIXME: send an effect on the removed body

  gi.unlinkEntity(ent);

  gi.unlinkEntity(body);
  body->s = ent->s;
  body->s.number = body - g_entities;
  body->s.skinNum = ent->s.skinNum & 0xFF; // only copy the client #

  if (frozen) {
    body->s.effects |= EF_COLOR_SHELL;
    body->s.renderFX |= (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE);
  } else {
    body->s.effects = EF_NONE;
    body->s.renderFX = RF_NONE;
  }

  body->svFlags = ent->svFlags;
  body->absMin = ent->absMin;
  body->absMax = ent->absMax;
  body->size = ent->size;
  body->solid = ent->solid;
  body->clipMask = ent->clipMask;
  body->owner = ent->owner;
  body->moveType = ent->moveType;
  body->health = ent->health;
  body->gibHealth = ent->gibHealth;
  body->s.event = EV_OTHER_TELEPORT;
  body->velocity = ent->velocity;
  body->aVelocity = ent->aVelocity;
  body->groundEntity = ent->groundEntity;
  body->groundEntity_linkCount = ent->groundEntity_linkCount;

  if (ent->takeDamage) {
    body->mins = ent->mins;
    body->maxs = ent->maxs;
  } else
    body->mins = body->maxs = {};

  if (CORPSE_SINK_TIME > 0_ms && Game::IsNot(GameType::FreezeTag)) {
    body->timeStamp = level.time + CORPSE_SINK_TIME + 1.5_sec;
    body->nextThink = level.time + CORPSE_SINK_TIME;
    body->think = BodySink;
  }

  body->die = body_die;
  body->takeDamage = true;

  gi.linkEntity(body);
}

void G_PostRespawn(gentity_t *self) {
  if (self->svFlags & SVF_NOCLIENT)
    return;

  // add a teleportation effect
  self->s.event = EV_PLAYER_TELEPORT;

  // hold in place briefly
  self->client->ps.pmove.pmFlags |= PMF_TIME_KNOCKBACK;
  self->client->ps.pmove.pmTime = 112;

  self->client->respawnMinTime = 0_ms;
  self->client->respawnMaxTime = level.time;

  if (deathmatch->integer && level.matchState == MatchState::Warmup_ReadyUp)
    BroadcastReadyReminderMessage();
}

/*
=============
ClientCompleteSpawn

Wraps ClientSpawn so the shared post-spawn logic (freeze state resets, respawn
timers, etc.) is always applied once regardless of the spawn origin.
=============
*/
void worr::server::client::ClientCompleteSpawn(gentity_t *ent) {
  // [Paril-KEX] Check for safe spawn availability before spawning
  if (deathmatch->integer) {
    select_spawn_result_t result =
        SelectDeathmatchSpawnPoint(ent, ent->client->lastDeathLocation, false,
                                   true, false, !ent->client->sess.inGame);

    if (!result.spot) {
      // No safe spawn found - delay allowed
      ent->client->respawnMinTime = level.time + GameTime::from_sec(2);
      ent->client->awaitingRespawn = true;
      gi.LocClient_Print(
          ent, PRINT_CENTER,
          "$g_sgame_auto_f11403053ddc");
      return;
    }
  }

  ent->client->awaitingRespawn = false;
  ClientSpawn(ent);
  G_PostRespawn(ent);
}

void ClientRespawn(gentity_t *ent) {
  if (!ent || !ent->client)
    return;

  HeadHunters::DropHeads(ent, nullptr);
  HeadHunters::ResetPlayerState(ent->client);

  ent->client->deathView = {};

  if (FreezeTag_IsActive() && ent && ent->client && ent->client->eliminated &&
      !level.intermission.time) {
    const bool gibbed = ent->health <= ent->gibHealth;
    if (!ent->client->resp.thawer && !gibbed)
      return;

    ent->client->eliminated = false;
  }

  if (deathmatch->integer || coop->integer) {
    // spectators don't leave bodies
    if (ClientIsPlaying(ent->client))
      CopyToBodyQue(ent);
    ent->svFlags &= ~SVF_NOCLIENT;

    if (Game::Is(GameType::RedRover) &&
        level.matchState == MatchState::In_Progress) {
      ent->client->sess.team = Teams_OtherTeam(ent->client->sess.team);
      ent->client->ps.teamID = static_cast<int>(ent->client->sess.team);
      AssignPlayerSkin(ent, ent->client->sess.skinName);
    }

    worr::server::client::ClientCompleteSpawn(ent);
    Harvester_OnClientSpawn(ent);

    if (FreezeTag_IsActive())
      FreezeTag_ResetState(ent->client);
    return;
  }

  // restart the entire server
  gi.AddCommandString("menu_loadgame\n");
}

//==============================================================

// [Paril-KEX]
// skinNum was historically used to pack data
// so we're going to build onto that.
void P_AssignClientSkinNum(gentity_t *ent) {
  if (ent->s.modelIndex != 255)
    return;

  player_skinnum_t packed{};

  packed.clientNum = ent->client - game.clients;
  if (ent->client->pers.weapon)
    packed.viewWeaponIndex =
        ent->client->pers.weapon->viewWeaponIndex - level.viewWeaponOffset + 1;
  else
    packed.viewWeaponIndex = 0;
  packed.viewHeight =
      ent->client->ps.viewOffset.z + ent->client->ps.pmove.viewHeight;

  if (CooperativeModeOn())
    packed.teamIndex = 1; // all players are teamed in coop
  else if (Teams())
    packed.teamIndex = static_cast<int>(ent->client->sess.team);
  else
    packed.teamIndex = 0;

  packed.poiIcon = ent->deadFlag ? 1 : 0;

  ent->s.skinNum = packed.skinNum;
}

// [Paril-KEX] send player level POI
void P_SendLevelPOI(gentity_t *ent) {
  if (!level.poi.valid)
    return;

  gi.WriteByte(svc_poi);
  gi.WriteShort(POI_OBJECTIVE);
  gi.WriteShort(10000);
  gi.WritePosition(ent->client->compass.poiLocation);
  gi.WriteShort(ent->client->compass.poiImage);
  gi.WriteByte(208);
  gi.WriteByte(POI_FLAG_NONE);
  gi.unicast(ent, true);
}

// [Paril-KEX] force the fog transition on the given player,
// optionally instantaneously (ignore any transition time)
void P_ForceFogTransition(gentity_t *ent, bool instant) {
  // sanity check; if we're not changing the values, don't bother
  if (ent->client->fog == ent->client->pers.wanted_fog &&
      ent->client->heightfog == ent->client->pers.wanted_heightfog)
    return;

  svc_fog_data_t fog{};

  // check regular fog
  if (ent->client->pers.wanted_fog[0] != ent->client->fog[0] ||
      ent->client->pers.wanted_fog[4] != ent->client->fog[4]) {
    fog.bits |= svc_fog_data_t::BIT_DENSITY;
    fog.density = ent->client->pers.wanted_fog[0];
    fog.skyfactor = ent->client->pers.wanted_fog[4] * 255.f;
  }
  if (ent->client->pers.wanted_fog[1] != ent->client->fog[1]) {
    fog.bits |= svc_fog_data_t::BIT_R;
    fog.red = ent->client->pers.wanted_fog[1] * 255.f;
  }
  if (ent->client->pers.wanted_fog[2] != ent->client->fog[2]) {
    fog.bits |= svc_fog_data_t::BIT_G;
    fog.green = ent->client->pers.wanted_fog[2] * 255.f;
  }
  if (ent->client->pers.wanted_fog[3] != ent->client->fog[3]) {
    fog.bits |= svc_fog_data_t::BIT_B;
    fog.blue = ent->client->pers.wanted_fog[3] * 255.f;
  }

  if (!instant && ent->client->pers.fog_transition_time) {
    fog.bits |= svc_fog_data_t::BIT_TIME;
    fog.time =
        std::clamp(ent->client->pers.fog_transition_time.milliseconds(),
                   (int64_t)0, (int64_t)std::numeric_limits<uint16_t>::max());
  }

  // check heightfog stuff
  auto &hf = ent->client->heightfog;
  const auto &wanted_hf = ent->client->pers.wanted_heightfog;

  if (hf.falloff != wanted_hf.falloff) {
    fog.bits |= svc_fog_data_t::BIT_HEIGHTFOG_FALLOFF;
    if (!wanted_hf.falloff)
      fog.hf_falloff = 0;
    else
      fog.hf_falloff = wanted_hf.falloff;
  }
  if (hf.density != wanted_hf.density) {
    fog.bits |= svc_fog_data_t::BIT_HEIGHTFOG_DENSITY;

    if (!wanted_hf.density)
      fog.hf_density = 0;
    else
      fog.hf_density = wanted_hf.density;
  }

  if (hf.start[0] != wanted_hf.start[0]) {
    fog.bits |= svc_fog_data_t::BIT_HEIGHTFOG_START_R;
    fog.hf_start_r = wanted_hf.start[0] * 255.f;
  }
  if (hf.start[1] != wanted_hf.start[1]) {
    fog.bits |= svc_fog_data_t::BIT_HEIGHTFOG_START_G;
    fog.hf_start_g = wanted_hf.start[1] * 255.f;
  }
  if (hf.start[2] != wanted_hf.start[2]) {
    fog.bits |= svc_fog_data_t::BIT_HEIGHTFOG_START_B;
    fog.hf_start_b = wanted_hf.start[2] * 255.f;
  }
  if (hf.start[3] != wanted_hf.start[3]) {
    fog.bits |= svc_fog_data_t::BIT_HEIGHTFOG_START_DIST;
    fog.hf_start_dist = wanted_hf.start[3];
  }

  if (hf.end[0] != wanted_hf.end[0]) {
    fog.bits |= svc_fog_data_t::BIT_HEIGHTFOG_END_R;
    fog.hf_end_r = wanted_hf.end[0] * 255.f;
  }
  if (hf.end[1] != wanted_hf.end[1]) {
    fog.bits |= svc_fog_data_t::BIT_HEIGHTFOG_END_G;
    fog.hf_end_g = wanted_hf.end[1] * 255.f;
  }
  if (hf.end[2] != wanted_hf.end[2]) {
    fog.bits |= svc_fog_data_t::BIT_HEIGHTFOG_END_B;
    fog.hf_end_b = wanted_hf.end[2] * 255.f;
  }
  if (hf.end[3] != wanted_hf.end[3]) {
    fog.bits |= svc_fog_data_t::BIT_HEIGHTFOG_END_DIST;
    fog.hf_end_dist = wanted_hf.end[3];
  }

  if (fog.bits & 0xFF00)
    fog.bits |= svc_fog_data_t::BIT_MORE_BITS;

  gi.WriteByte(svc_fog);

  if (fog.bits & svc_fog_data_t::BIT_MORE_BITS)
    gi.WriteShort(fog.bits);
  else
    gi.WriteByte(fog.bits);

  if (fog.bits & svc_fog_data_t::BIT_DENSITY) {
    gi.WriteFloat(fog.density);
    gi.WriteByte(fog.skyfactor);
  }
  if (fog.bits & svc_fog_data_t::BIT_R)
    gi.WriteByte(fog.red);
  if (fog.bits & svc_fog_data_t::BIT_G)
    gi.WriteByte(fog.green);
  if (fog.bits & svc_fog_data_t::BIT_B)
    gi.WriteByte(fog.blue);
  if (fog.bits & svc_fog_data_t::BIT_TIME)
    gi.WriteShort(fog.time);

  if (fog.bits & svc_fog_data_t::BIT_HEIGHTFOG_FALLOFF)
    gi.WriteFloat(fog.hf_falloff);
  if (fog.bits & svc_fog_data_t::BIT_HEIGHTFOG_DENSITY)
    gi.WriteFloat(fog.hf_density);

  if (fog.bits & svc_fog_data_t::BIT_HEIGHTFOG_START_R)
    gi.WriteByte(fog.hf_start_r);
  if (fog.bits & svc_fog_data_t::BIT_HEIGHTFOG_START_G)
    gi.WriteByte(fog.hf_start_g);
  if (fog.bits & svc_fog_data_t::BIT_HEIGHTFOG_START_B)
    gi.WriteByte(fog.hf_start_b);
  if (fog.bits & svc_fog_data_t::BIT_HEIGHTFOG_START_DIST)
    gi.WriteLong(fog.hf_start_dist);

  if (fog.bits & svc_fog_data_t::BIT_HEIGHTFOG_END_R)
    gi.WriteByte(fog.hf_end_r);
  if (fog.bits & svc_fog_data_t::BIT_HEIGHTFOG_END_G)
    gi.WriteByte(fog.hf_end_g);
  if (fog.bits & svc_fog_data_t::BIT_HEIGHTFOG_END_B)
    gi.WriteByte(fog.hf_end_b);
  if (fog.bits & svc_fog_data_t::BIT_HEIGHTFOG_END_DIST)
    gi.WriteLong(fog.hf_end_dist);

  gi.unicast(ent, true);

  ent->client->fog = ent->client->pers.wanted_fog;
  hf = wanted_hf;
}

/*
===========
InitPlayerTeam
============
*/
bool InitPlayerTeam(gentity_t *ent) {
  // Non-deathmatch (e.g. single-player or coop) - everyone plays
  if (!deathmatch->integer) {
    ent->client->sess.team = Team::Free;
    ent->client->ps.teamID = static_cast<int>(ent->client->sess.team);
    ent->client->ps.stats[STAT_SHOW_STATUSBAR] = 1;
    return true;
  }

  if (Tournament_IsActive()) {
    if (Tournament_IsParticipant(ent->client)) {
      Team locked = Tournament_AssignedTeam(ent->client);
      if (locked == Team::Spectator || locked == Team::None)
        locked = Team::Spectator;
      SetTeam(ent, locked, false, true, true);
      return true;
    }

    ent->client->sess.team = Team::Spectator;
    ent->client->ps.teamID = static_cast<int>(ent->client->sess.team);
    MoveClientToFreeCam(ent);
    return false;
  }

  // If we've already been placed on a team, do nothing
  if (ent->client->sess.team != Team::None) {
    return true;
  }

  bool matchLocked =
      ((level.matchState >= MatchState::Countdown) && match_lock->integer);

  if (!matchLocked) {
    if (ent == host) {
      if (g_owner_auto_join->integer) {
        SetTeam(ent, PickTeam(-1), false, false, false);
        return true;
      }
    } else {
      if (match_forceJoin->integer || match_autoJoin->integer) {
        SetTeam(ent, PickTeam(-1), false, false, false);
        return true;
      }
      if ((ent->svFlags & SVF_BOT) || ent->client->sess.is_a_bot) {
        SetTeam(ent, PickTeam(-1), false, false, false);
        return true;
      }
    }
  }

  // Otherwise start as spectator
  ent->client->sess.team = Team::Spectator;
  ent->client->ps.teamID = static_cast<int>(ent->client->sess.team);
  MoveClientToFreeCam(ent);

  ent->client->initialMenu.frozen = true;
  ent->client->initialMenu.hostSetupDone = false;
  ent->client->initialMenu.shown = false;
  ent->client->initialMenu.dmWelcomeActive = false;
  ent->client->initialMenu.dmJoinActive = false;
  if (!ent->client->initialMenu.shown)
    ent->client->initialMenu.delay = level.time + 10_hz;

  return false;
}

/*
=====================
ClientBeginDeathmatch

A client has just connected to the server in
deathmatch mode, so clear everything out before starting them.
=====================
*/
void worr::server::client::ClientBeginDeathmatch(gentity_t *ent) {
  InitGEntity(ent);
  HeadHunters::ResetPlayerState(ent->client);

  // make sure we have a known default
  ent->svFlags |= SVF_PLAYER;

  InitClientResp(ent->client);

  // locate ent at a spawn point
  ClientCompleteSpawn(ent);

  if (level.intermission.time) {
    MoveClientToIntermission(ent);
  } else {
    if (!(ent->svFlags & SVF_NOCLIENT)) {
      // send effect
      gi.WriteByte(svc_muzzleflash);
      gi.WriteEntity(ent);
      gi.WriteByte(MZ_LOGIN);
      gi.multicast(ent->s.origin, MULTICAST_PVS, false);
    }
  }

  // gi.LocBroadcast_Print(PRINT_HIGH, "$g_entered_game",
  // ent->client->sess.netName);

  // make sure all view stuff is valid
  ClientEndServerFrame(ent);
}

/*
===============
G_SetLevelEntry
===============
*/
void worr::server::client::G_SetLevelEntry() {
  if (deathmatch->integer)
    return;

  if (level.campaign.hub_map)
    return;

  LevelEntry *foundEntry = nullptr;
  int32_t highest_order = 0;

  for (size_t i = 0; i < MAX_LEVELS_PER_UNIT; ++i) {
    LevelEntry *entry = &game.levelEntries[i];

    highest_order = std::max(highest_order, entry->visit_order);

    if (!std::strcmp(entry->mapName.data(), level.mapName.data()) ||
        !entry->mapName[0]) {
      foundEntry = entry;
      break;
    }
  }

  if (!foundEntry) {
    gi.Com_PrintFmt(
        "WARNING: more than {} maps in unit, can't track the rest\n",
        MAX_LEVELS_PER_UNIT);
    return;
  }

  level.entry = foundEntry;
  Q_strlcpy(level.entry->mapName.data(), level.mapName.data(),
            level.entry->mapName.size());

  if (!level.entry->longMapName[0]) {
    Q_strlcpy(level.entry->longMapName.data(), level.longName.data(),
              level.entry->longMapName.size());
    level.entry->visit_order = highest_order + 1;

    if (g_coop_enable_lives->integer) {
      const int maxLives = g_coop_num_lives->integer + 1;
      for (size_t i = 0; i < game.maxClients; ++i) {
        gclient_t *cl = &game.clients[i];
        cl->pers.lives = std::min(maxLives, cl->pers.lives + 1);
        cl->pers.limitedLivesStash = cl->pers.lives;
        cl->pers.limitedLivesPersist = true;
      }
    }
  }

  gentity_t *changelevel = nullptr;
  while ((changelevel = G_FindByString<&gentity_t::className>(
              changelevel, "target_changelevel")) != nullptr) {
    if (!changelevel->map[0])
      continue;

    if (std::strchr(changelevel->map.data(), '*'))
      continue;

    const char *levelName = std::strchr(changelevel->map.data(), '+');
    if (levelName)
      ++levelName;
    else
      levelName = changelevel->map.data();

    if (std::strstr(levelName, ".cin") || std::strstr(levelName, ".pcx"))
      continue;

    size_t levelLength;
    if (const char *spawnpoint = std::strchr(levelName, '$'))
      levelLength = static_cast<size_t>(spawnpoint - levelName);
    else
      levelLength = std::strlen(levelName);

    LevelEntry *slot = nullptr;
    for (size_t i = 0; i < MAX_LEVELS_PER_UNIT; ++i) {
      LevelEntry *entry = &game.levelEntries[i];

      if (!entry->mapName[0] ||
          std::strncmp(entry->mapName.data(), levelName, levelLength) == 0) {
        slot = entry;
        break;
      }
    }

    if (!slot) {
      gi.Com_PrintFmt(
          "WARNING: more than {} maps in unit, can't track the rest\n",
          MAX_LEVELS_PER_UNIT);
      return;
    }

    const size_t copyLength = std::min(levelLength + 1, slot->mapName.size());
    Q_strlcpy(slot->mapName.data(), levelName, copyLength);
  }
}

/*
=================
ClientIsPlaying
=================
*/
bool ClientIsPlaying(gclient_t *cl) {
  if (!cl)
    return false;

  if (!deathmatch->integer)
    return true;

  return !(cl->sess.team == Team::None || cl->sess.team == Team::Spectator);
}

/*
=================
BroadcastTeamChange
Let everyone know about a team change
=================
*/
void BroadcastTeamChange(gentity_t *ent, Team old_team, bool inactive,
                         bool silent) {
  if (!deathmatch->integer || !ent || !ent->client)
    return;
  if (!Game::Has(GameFlags::OneVOne) && ent->client->sess.team == old_team)
    return;
  if (silent)
    return;
  std::string s, t;
  char name[MAX_INFO_VALUE] = {};
  gi.Info_ValueForKey(ent->client->pers.userInfo, "name", name, sizeof(name));
  const std::string_view playerName = name;
  const uint16_t skill = ent->client->sess.skillRating;
  const auto team = ent->client->sess.team;
  switch (team) {
  case Team::Free:
    s = std::format(".{} joined the battle.\n", playerName);
    if (skill > 0.f) {
      t = std::format(".You have joined the game.\nYour Skill Rating: {}",
                      skill);
    } else {
      t = ".You have joined the game.";
    }
    break;
  case Team::Spectator:
    if (inactive) {
      s = std::format(".{} is inactive,\nmoved to spectators.\n", playerName);
      t = "You are inactive and have been\nmoved to spectators.";
    } else if (Game::Has(GameFlags::OneVOne) && ent->client->sess.matchQueued) {
      s = std::format(".{} is in the queue to play.\n", playerName);
      t = ".You are in the queue to play.";
    } else {
      s = std::format(".{} joined the spectators.\n", playerName);
      t = ".You are now spectating.";
    }
    break;
  case Team::Red:
  case Team::Blue: {
    std::string_view teamName = Teams_TeamName(team);
    s = std::format(".{} joined the {} Team.\n", playerName, teamName);
    if (skill > 0.f) {
      t = std::format(".You have joined the {} Team.\nYour Skill Rating: {}",
                      teamName, skill);
    } else {
      t = std::format(".You have joined the {} Team.\n", teamName);
    }
    break;
  }
  default:
    break;
  }
  if (!s.empty()) {
    for (auto ec : active_clients()) {
      if (ec == ent || (ec->svFlags & SVF_BOT))
        continue;
      gi.LocClient_Print(ec, PRINT_CENTER, s.c_str());
    }
  }
  if (warmup_doReadyUp->integer &&
      level.matchState == MatchState::Warmup_ReadyUp) {
    BroadcastReadyReminderMessage();
  } else if (!t.empty()) {
    std::string msg = std::format("%bind:inven:Toggles Menu%{}", t);
    gi.LocClient_Print(ent, PRINT_CENTER, msg.c_str());
  }
}

/*
=============
NextDuelQueueTicket

Generates the next ticket number for ordering players in the duel queue.
=============
*/
static uint64_t NextDuelQueueTicket() {
  static uint64_t nextTicket = 1;
  return nextTicket++;
}

bool SetTeam(gentity_t *ent, Team desired_team, bool inactive, bool force,
             bool silent) {
  if (!ent || !ent->client)
    return false;

  gclient_t *cl = ent->client;
  const bool wasInitialised = cl->sess.initialised;
  const Team old_team = cl->sess.team;
  const bool wasPlaying = ClientIsPlaying(cl);
  const bool duel = Game::Has(GameFlags::OneVOne);
  const bool duelQueueAllowed =
      duel && g_allow_duel_queue && g_allow_duel_queue->integer &&
      !Tournament_IsActive();
  const int clientNum = static_cast<int>(cl - game.clients);
  const bool isBot = (ent->svFlags & SVF_BOT) || cl->sess.is_a_bot;

  if (Tournament_IsActive() && !force) {
    if (!Tournament_IsParticipant(cl)) {
      if (!silent)
        gi.LocClient_Print(ent, PRINT_HIGH,
                           "$g_sgame_auto_397418db7edb");
      return false;
    }

    if (desired_team != Team::Spectator) {
      Team locked = Tournament_AssignedTeam(cl);
      if (locked == Team::None)
        locked = Team::Spectator;
      desired_team = locked;
    }
  }

  if (!force && cl->sess.queuedTeam != Team::None &&
      desired_team != Team::Spectator) {
    if (!silent)
      gi.LocClient_Print(
          ent, PRINT_HIGH,
          "$g_sgame_auto_3fdd679681cc");
    return false;
  }

  if (!force && cl->resp.teamDelayTime > level.time) {
    gi.LocClient_Print(ent, PRINT_HIGH,
                       "$g_sgame_auto_d2f12db97932");
    return false;
  }

  if (!force && !isBot && FreezeTag_IsFrozen(ent)) {
    gi.LocClient_Print(ent, PRINT_HIGH, "$g_cant_change_teams");
    return false;
  }

  Team target = desired_team;
  bool requestQueue = duelQueueAllowed && desired_team == Team::None;

  if (duel && desired_team == Team::None && !duelQueueAllowed)
    target = Team::Spectator;

  if (!deathmatch->integer) {
    target = (desired_team == Team::Spectator) ? Team::Spectator : Team::Free;
  } else if (!requestQueue) {
    if (target == Team::None)
      target = PickTeam(clientNum);
    if (!Teams()) {
      if (target != Team::Spectator)
        target = Team::Free;
    } else {
      if (target == Team::Free || target == Team::None)
        target = PickTeam(clientNum);
      if (target != Team::Spectator && target != Team::Red &&
          target != Team::Blue)
        target = PickTeam(clientNum);
    }
  }

  bool joinPlaying = (target != Team::Spectator);
  const bool matchLocked =
      match_lock->integer && level.matchState >= MatchState::Countdown;

  if (joinPlaying && !requestQueue && !force) {
    if (matchLocked && !wasPlaying) {
      if (duel && duelQueueAllowed) {
        target = Team::Spectator;
        joinPlaying = false;
        requestQueue = true;
      } else {
        if (!silent)
          gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_eb439676ee76");
        return false;
      }
    }
  }

  if (joinPlaying) {
    const TeamJoinCapacityAction capacityAction = EvaluateTeamJoinCapacity(
        joinPlaying, requestQueue, force, wasPlaying, duel, duelQueueAllowed,
        !cl->sess.is_a_bot, level.pop.num_playing_human_clients,
        maxplayers->integer);

    switch (capacityAction) {
    case TeamJoinCapacityAction::Allow:
      break;
    case TeamJoinCapacityAction::QueueForDuel:
      target = Team::Spectator;
      joinPlaying = false;
      requestQueue = true;
      break;
    case TeamJoinCapacityAction::Deny:
      if (!silent)
        gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_22f98ab91aa9");
      return false;
    }
  }

  if (joinPlaying && !requestQueue && duel && !force && !wasPlaying) {
    int playingClients = 0;
    for (auto ec : active_clients()) {
      if (ec && ec->client && ClientIsPlaying(ec->client))
        ++playingClients;
    }
    if (playingClients >= 2) {
      target = Team::Spectator;
      joinPlaying = false;
      requestQueue = duelQueueAllowed;
    }
  }

  if (requestQueue)
    target = Team::Spectator;

  const bool queueNow = duelQueueAllowed && requestQueue;
  const bool spectatorInactive = (target == Team::Spectator) && inactive;
  const bool changedTeam = (target != old_team);
  const bool changedQueue = (queueNow != cl->sess.matchQueued);
  const bool changedInactive = (spectatorInactive != cl->sess.inactiveStatus);

  if (!changedTeam && !changedQueue && !changedInactive)
    return false;

  if (changedTeam)
    Harvester_HandleTeamChange(ent);

  if (IsUiMenuOpen(cl))
    CloseActiveMenu(ent);

  const int64_t now = GetCurrentRealTimeMillis();

  if (target == Team::Spectator) {
    if (wasPlaying) {
      CTF_DeadDropFlag(ent);
      ProBall::DropBall(ent, nullptr, false);
      Tech_DeadDrop(ent);
      Weapon_Grapple_DoReset(cl);
      worr::server::client::P_AccumulateMatchPlayTime(cl, now);
      cl->sess.playEndRealTime = now;
    }
    cl->sess.team = Team::Spectator;
    cl->ps.teamID = static_cast<int>(cl->sess.team);
    if (changedTeam || changedQueue)
      cl->sess.teamJoinTime = level.time;
    cl->sess.matchQueued = queueNow;
    if (queueNow) {
      if (changedQueue || cl->sess.duelQueueTicket == 0)
        cl->sess.duelQueueTicket = NextDuelQueueTicket();
    } else {
      cl->sess.duelQueueTicket = 0;
    }
    cl->sess.inactiveStatus = spectatorInactive;
    cl->sess.inactivityWarning = false;
    cl->sess.inactivityTime = 0_sec;
    cl->sess.inGame = false;
    cl->sess.initialised = true;
    cl->pers.readyStatus = false;
    if (G_LimitedLivesActive()) {
      cl->pers.limitedLivesStash = cl->pers.lives;
      cl->pers.limitedLivesPersist = true;
    }
    cl->pers.spawned = false;

    cl->buttons = BUTTON_NONE;
    cl->oldButtons = BUTTON_NONE;
    cl->latchedButtons = BUTTON_NONE;

    cl->weapon.fireFinished = 0_ms;
    cl->weapon.thinkTime = 0_ms;
    cl->weapon.fireBuffered = false;
    cl->weapon.pending = nullptr;

    cl->ps.pmove.pmFlags = PMF_NONE;
    cl->ps.pmove.pmTime = 0;
    cl->ps.damageBlend = {};
    cl->ps.screenBlend = {};
    cl->ps.rdFlags = RDF_NONE;

    cl->damage = {};
    cl->kick = {};
    cl->feedback = {};

    cl->respawnMinTime = 0_ms;
    cl->respawnMaxTime = level.time;
    cl->respawn_timeout = 0_ms;
    cl->pers.teamState = {};

    FreeFollower(ent);
    MoveClientToFreeCam(ent);
    if (level.spawn.intermission) {
      FindIntermissionPoint();
      const Vector3 interOrigin = level.intermission.origin;
      const Vector3 interAngles = level.intermission.angles;

      cl->ps.pmove.origin = interOrigin;
      ent->s.origin = interOrigin;
      ent->s.oldOrigin = interOrigin;

      cl->ps.pmove.deltaAngles = interAngles - cl->resp.cmdAngles;

      ent->s.angles = interAngles;
      cl->ps.viewAngles = interAngles;
      cl->vAngle = interAngles;
      cl->oldViewAngles = interAngles;

      AngleVectors(cl->vAngle, cl->vForward, nullptr, nullptr);
      gi.linkEntity(ent);
    }
    FreeClientFollowers(ent);
  } else {
    cl->sess.team = target;
    cl->ps.teamID = static_cast<int>(cl->sess.team);
    if (Teams())
      AssignPlayerSkin(ent, cl->sess.skinName);
    cl->sess.matchQueued = false;
    cl->sess.duelQueueTicket = 0;
    cl->sess.inactiveStatus = false;
    cl->sess.inactivityWarning = false;
    cl->sess.inGame = true;
    cl->sess.initialised = true;
    cl->sess.teamJoinTime = level.time;
    cl->pers.spawned = true;
    cl->pers.readyStatus = false;

    GameTime timeout = GameTime::from_sec(g_inactivity->integer);
    if (timeout && timeout < 15_sec)
      timeout = 15_sec;
    cl->sess.inactivityTime = timeout ? level.time + timeout : 0_sec;

    if (!wasPlaying)
      cl->sess.playStartRealTime = now;
    cl->sess.playEndRealTime = 0;

    cl->buttons = BUTTON_NONE;
    cl->oldButtons = BUTTON_NONE;
    cl->latchedButtons = BUTTON_NONE;

    cl->weapon.fireBuffered = false;
    cl->weapon.pending = nullptr;

    cl->ps.pmove.pmFlags = PMF_NONE;
    cl->ps.pmove.pmTime = 0;

    FreeFollower(ent);
    ClientRespawn(ent);
  }

  BroadcastTeamChange(ent, old_team, spectatorInactive, silent);
  CalculateRanks();
  ClientUpdateFollowers(ent);

  if (cl->initialMenu.frozen) {
    cl->initialMenu.frozen = false;
    cl->initialMenu.shown = true;
    cl->initialMenu.delay = 0_sec;
    cl->initialMenu.hostSetupDone = true;
    cl->initialMenu.dmWelcomeActive = false;
    cl->initialMenu.dmJoinActive = false;
  }

  if (!force && wasInitialised && changedTeam)
    cl->resp.teamDelayTime = level.time + 5_sec;

  return true;
}
/*
=============
ClientBegin

Routes ClientBegin through the ClientSessionServiceImpl so all entry points
share the same lifecycle management.
=============
*/
void ClientBegin(gentity_t *ent) {
  auto &service = worr::server::client::GetClientSessionService();
  service.ClientBegin(gi, game, level, ent);
}

/*
================
P_GetLobbyUserNum
================
*/
unsigned int P_GetLobbyUserNum(const gentity_t *player) {
  unsigned int playerNum = 0;
  if (player > g_entities && player < g_entities + MAX_ENTITIES) {
    playerNum = (player - g_entities) - 1;
    if (playerNum >= MAX_CLIENTS) {
      playerNum = 0;
    }
  }
  return playerNum;
}

/*
================
G_EncodedPlayerName

Gets a token version of the players "name" to be decoded on the client.
================
*/
std::string G_EncodedPlayerName(gentity_t *player) {
  unsigned int playernum = P_GetLobbyUserNum(player);
  return std::string("##P") + std::to_string(playernum);
}

/*
================
ClientUserinfoChanged

Routes userinfo updates through the ClientSessionServiceImpl to keep legacy
state transitions centralized.
================
*/
void ClientUserinfoChanged(gentity_t *ent, const char *userInfo) {
  auto &service = worr::server::client::GetClientSessionService();
  service.ClientUserinfoChanged(gi, game, level, ent, userInfo);
}

static inline bool IsSlotIgnored(gentity_t *slot, gentity_t **ignore,
                                 size_t num_ignore) {
  for (size_t i = 0; i < num_ignore; i++)
    if (slot == ignore[i])
      return true;

  return false;
}

static inline gentity_t *ClientChooseSlot_Any(gentity_t **ignore,
                                              size_t num_ignore) {
  for (size_t i = 0; i < game.maxClients; i++)
    if (!IsSlotIgnored(globals.gentities + i + 1, ignore, num_ignore) &&
        !game.clients[i].pers.connected)
      return globals.gentities + i + 1;

  return nullptr;
}

static inline gentity_t *ClientChooseSlot_Coop(const char *userInfo,
                                               const char *socialID, bool isBot,
                                               gentity_t **ignore,
                                               size_t num_ignore) {
  char name[MAX_INFO_VALUE] = {0};
  gi.Info_ValueForKey(userInfo, "name", name, sizeof(name));

  // the host should always occupy slot 0, some systems rely on this
  // (CHECK: is this true? is it just bots?)
  {
    size_t num_players = 0;

    for (size_t i = 0; i < game.maxClients; i++)
      if (IsSlotIgnored(globals.gentities + i + 1, ignore, num_ignore) ||
          game.clients[i].pers.connected)
        num_players++;

    if (!num_players) {
      gi.Com_PrintFmt("coop slot {} is host {}+{}\n", 1, name, socialID);
      return globals.gentities + 1;
    }
  }

  // grab matches from players that we have connected
  using match_type_t = int32_t;
  enum {
    SLOT_MATCH_USERNAME,
    SLOT_MATCH_SOCIAL,
    SLOT_MATCH_BOTH,

    SLOT_MATCH_TYPES
  };

  struct {
    gentity_t *slot = nullptr;
    size_t total = 0;
  } matches[SLOT_MATCH_TYPES];

  for (size_t i = 0; i < game.maxClients; i++) {
    if (IsSlotIgnored(globals.gentities + i + 1, ignore, num_ignore) ||
        game.clients[i].pers.connected)
      continue;

    char check_name[MAX_INFO_VALUE] = {0};
    gi.Info_ValueForKey(game.clients[i].pers.userInfo, "name", check_name,
                        sizeof(check_name));

    bool username_match =
        game.clients[i].pers.userInfo[0] && !strcmp(check_name, name);

    bool social_match = socialID && game.clients[i].sess.socialID[0] &&
                        !strcmp(game.clients[i].sess.socialID, socialID);

    match_type_t type = (match_type_t)0;

    if (username_match)
      type |= SLOT_MATCH_USERNAME;
    if (social_match)
      type |= SLOT_MATCH_SOCIAL;

    if (!type)
      continue;

    matches[type].slot = globals.gentities + i + 1;
    matches[type].total++;
  }

  // pick matches in descending order, only if the total matches
  // is 1 in the particular set; this will prefer to pick
  // social+username matches first, then social, then username last.
  for (int32_t i = 2; i >= 0; i--) {
    if (matches[i].total == 1) {
      gi.Com_PrintFmt("coop slot {} restored for {}+{}\n",
                      (ptrdiff_t)(matches[i].slot - globals.gentities), name,
                      socialID);

      // spawn us a ghost now since we're gonna spawn eventually
      if (!matches[i].slot->inUse) {
        matches[i].slot->s.modelIndex = MODELINDEX_PLAYER;
        matches[i].slot->solid = SOLID_BBOX;

        InitGEntity(matches[i].slot);
        matches[i].slot->className = "player";
        worr::server::client::InitClientResp(matches[i].slot->client);
        matches[i].slot->client->coopRespawn.spawnBegin = true;
        ClientSpawn(matches[i].slot);
        matches[i].slot->client->coopRespawn.spawnBegin = false;

        // make sure we have a known default
        matches[i].slot->svFlags |= SVF_PLAYER;

        matches[i].slot->sv.init = false;
        matches[i].slot->className = "player";
        matches[i].slot->client->pers.connected = true;
        matches[i].slot->client->pers.spawned = true;
        P_AssignClientSkinNum(matches[i].slot);
        gi.linkEntity(matches[i].slot);
      }

      return matches[i].slot;
    }
  }

  // in the case where we can't find a match, we're probably a new
  // player, so pick a slot that hasn't been occupied yet
  for (size_t i = 0; i < game.maxClients; i++)
    if (!IsSlotIgnored(globals.gentities + i + 1, ignore, num_ignore) &&
        !game.clients[i].pers.userInfo[0]) {
      gi.Com_PrintFmt("coop slot {} issuing new for {}+{}\n", i + 1, name,
                      socialID);
      return globals.gentities + i + 1;
    }

  // all slots have some player data in them, we're forced to replace one.
  gentity_t *any_slot = ClientChooseSlot_Any(ignore, num_ignore);

  gi.Com_PrintFmt("coop slot {} any slot for {}+{}\n",
                  !any_slot ? -1 : (ptrdiff_t)(any_slot - globals.gentities),
                  name, socialID);

  return any_slot;
}

// [Paril-KEX] for coop, we want to try to ensure that players will always get
// their proper slot back when they connect.
gentity_t *ClientChooseSlot(const char *userInfo, const char *socialID,
                            bool isBot, gentity_t **ignore, size_t num_ignore,
                            bool cinematic) {
  // coop and non-bots is the only thing that we need to do special behavior on
  if (!cinematic && coop->integer && !isBot)
    return ClientChooseSlot_Coop(userInfo, socialID, isBot, ignore, num_ignore);

  // just find any free slot
  return ClientChooseSlot_Any(ignore, num_ignore);
}

/*
===========
ClientConnect

Relays the legacy ClientConnect logic through the session service so the
new entry point can gradually replace this procedural implementation.
============
*/
bool ClientConnect(gentity_t *ent, char *userInfo, const char *socialID,
                   bool isBot) {
  auto &service = worr::server::client::GetClientSessionService();
  return service.ClientConnect(gi, game, level, ent, userInfo, socialID, isBot);
}

/*
===========
ClientDisconnect

Called when a player drops from the server.
Will not be called between levels.
============
*/
void ClientDisconnect(gentity_t *ent) {
  auto &service = worr::server::client::GetClientSessionService();
  const auto result = service.ClientDisconnect(gi, game, level, ent);
  if (result == worr::server::client::DisconnectResult::InvalidEntity)
    return;
}

//==============================================================

[[maybe_unused]] static trace_t G_PM_Clip(const Vector3 &start, const Vector3 *mins,
                         const Vector3 *maxs, const Vector3 &end,
                         contents_t mask) {
  return gi.game_import_t::clip(world, start, mins, maxs, end, mask);
}

bool G_ShouldPlayersCollide(bool weaponry) {
  if (g_disable_player_collision->integer)
    return false; // only for debugging.

  // always collide on dm
  if (!CooperativeModeOn())
    return true;

  // weaponry collides if friendly fire is enabled
  if (weaponry && g_friendlyFireScale->integer > 0.0)
    return true;

  // check collision cvar
  return g_coop_player_collision->integer;
}

/*
=================
P_FallingDamage

Paril-KEX: this is moved here and now reacts directly
to ClientThink rather than being delayed.
=================
*/
[[maybe_unused]] static void P_FallingDamage(gentity_t *ent, const PMove &pm) {
  int damage;
  Vector3 dir;

  // dead stuff can't crater
  if (ent->health <= 0 || ent->deadFlag)
    return;

  if (ent->s.modelIndex != MODELINDEX_PLAYER)
    return; // not in the player model

  if (ent->moveType == MoveType::NoClip || ent->moveType == MoveType::FreeCam)
    return;

  // never take falling damage if completely underwater
  if (pm.waterLevel == WATER_UNDER)
    return;

  //  never take damage if just release grapple or on grapple
  if (ent->client->grapple.releaseTime >= level.time ||
      (ent->client->grapple.entity &&
       ent->client->grapple.state > GrappleState::Fly))
    return;

  float delta = pm.impactDelta;

  delta = delta * delta * 0.0001f;

  if (pm.waterLevel == WATER_WAIST)
    delta *= 0.25f;
  if (pm.waterLevel == WATER_FEET)
    delta *= 0.5f;

  if (delta < 1.0f)
    return;

  // restart footstep timer
  ent->client->feedback.bobTime = 0;

  if (ent->client->landmark_free_fall) {
    delta = min(30.f, delta);
    ent->client->landmark_free_fall = false;
    ent->client->landmark_noise_time = level.time + 100_ms;
  }

  if (delta < 15) {
    if (!(pm.s.pmFlags & PMF_ON_LADDER))
      ent->s.event = EV_FOOTSTEP;
    return;
  }

  ent->client->feedback.fallValue = delta * 0.5f;
  if (ent->client->feedback.fallValue > 40)
    ent->client->feedback.fallValue = 40;
  ent->client->feedback.fallTime = level.time + FALL_TIME();

  int med_min = RS(Quake3Arena) ? 40 : 30;
  int far_min = RS(Quake3Arena) ? 61 : 55;

  if (delta > med_min) {
    if (delta >= far_min)
      ent->s.event = EV_FALL_FAR;
    else
      ent->s.event = EV_FALL_MEDIUM;
    if (g_fallingDamage->integer && !Game::Has(GameFlags::Arena)) {
      const int healthBefore = ent->health;
      const int feedbackBefore =
          ent->client ? (ent->client->damage.blood + ent->client->damage.armor +
                         ent->client->damage.powerArmor)
                      : 0;

      if (RS(Quake3Arena))
        damage = ent->s.event == EV_FALL_FAR ? 10 : 5;
      else {
        damage = (int)((delta - 30) / 3); // 2 originally
        if (damage < 1)
          damage = 1;
      }
      dir = {0, 0, 1};

      Damage(ent, world, world, dir, ent->s.origin, vec3_origin, damage, 0,
             DamageFlags::Normal, ModID::FallDamage);

      if (ent->client) {
        const int feedbackAfter = ent->client->damage.blood +
                                  ent->client->damage.armor +
                                  ent->client->damage.powerArmor;
        const int healthDelta = healthBefore - ent->health;
        if (healthDelta > 0 && ent->health > 0 &&
            feedbackAfter == feedbackBefore) {
          // Ensure fall damage generates HUD feedback even if damage tracking
          // misses it.
          ent->client->damage.blood += healthDelta;
          ent->client->damage.origin = ent->s.origin;
          ent->client->last_damage_time = level.time + COOP_DAMAGE_RESPAWN_TIME;
        }
      }
    }
  } else
    ent->s.event = EV_FALL_SHORT;

  // Paril: falling damage noises alert monsters
  if (ent->health)
    G_PlayerNoise(ent, pm.s.origin, PlayerNoise::Self);
}

/*
=========================
PrintModifierIntro

Displays the intro text for the active game modifier.
Only one modifier should be active at a time.
=========================
*/
void worr::server::client::PrintModifierIntro(gentity_t *ent) {
  if (!ent || !ent->client)
    return;

  if (g_quadhog->integer) {
    gi.LocClient_Print(
        ent, PRINT_CENTER,
        "$g_sgame_auto_56bc30d874d4");
  } else if (g_vampiric_damage->integer) {
    gi.LocClient_Print(
        ent, PRINT_CENTER,
        "$g_sgame_auto_1581815c80f0");
  } else if (g_frenzy->integer) {
    gi.LocClient_Print(
        ent, PRINT_CENTER,
        "$g_sgame_auto_3bd95624502e");
  } else if (g_gravity_lotto && g_gravity_lotto->integer) {
    gi.LocClient_Print(ent, PRINT_CENTER,
                       "$g_sgame_auto_63e47ec21ba1",
                       g_gravity->integer);
  } else if (g_nadeFest->integer) {
    gi.LocClient_Print(ent, PRINT_CENTER, "$g_sgame_auto_cd1341266e56");
  } else if (g_instaGib->integer) {
    gi.LocClient_Print(ent, PRINT_CENTER, "$g_sgame_auto_d761dbbb03b3");
  }
}

/*
==============
ClientThink

This will be called once for each client frame, which will
usually be a couple times for each server frame.
==============
*/
void ClientThink(gentity_t *ent, usercmd_t *ucmd) {
  auto &service = worr::server::client::GetClientSessionService();
  service.ClientThink(gi, game, level, ent, ucmd);
}

void OpenJoinMenu(gentity_t *ent);

// active monsters
struct active_monsters_filter_t {
  inline bool operator()(gentity_t *ent) const {
    return (ent->inUse && (ent->svFlags & SVF_MONSTER) && ent->health > 0);
  }
};

static inline entity_iterable_t<active_monsters_filter_t> active_monsters() {
  return entity_iterable_t<active_monsters_filter_t>{
      game.maxClients + (uint32_t)BODY_QUEUE_SIZE + 1U};
}

static inline bool G_MonstersSearchingFor(gentity_t *player) {
  for (auto ent : active_monsters()) {
    // check for *any* player target
    if (player == nullptr && ent->enemy && !ent->enemy->client)
      continue;
    // they're not targeting us, so who cares
    else if (player != nullptr && ent->enemy != player)
      continue;

    // they lost sight of us
    if ((ent->monsterInfo.aiFlags & AI_LOST_SIGHT) &&
        level.time > ent->monsterInfo.trailTime + 5_sec)
      continue;

    // no sir
    return true;
  }

  // yes sir
  return false;
}

/*
===============
G_FindRespawnSpot

Attempts to find a valid respawn spot near the given player.
Returns true and fills `spot` if successful.
===============
*/
static inline bool G_FindRespawnSpot(gentity_t *player, Vector3 &spot) {
  constexpr std::array<float, 5> yawOffsets{{0, 90, 45, -45, -90}};
  constexpr float backDistance = 128.0f;
  constexpr float upDistance = 128.0f;
  constexpr float viewHeight = static_cast<float>(DEFAULT_VIEWHEIGHT);
  constexpr contents_t solidMask =
      MASK_PLAYERSOLID | CONTENTS_LAVA | CONTENTS_SLIME;

  // Sanity check: make sure player isn't already stuck
  if (gi.trace(player->s.origin, PLAYER_MINS, PLAYER_MAXS, player->s.origin,
               player, MASK_PLAYERSOLID)
          .startSolid)
    return false;

  for (float yawOffset : yawOffsets) {
    Vector3 yawAngles = {0, player->s.angles[YAW] + 180.0f + yawOffset, 0};

    // Step 1: Try moving up first
    Vector3 start = player->s.origin;
    Vector3 end = start + Vector3{0, 0, upDistance};
    trace_t tr =
        gi.trace(start, PLAYER_MINS, PLAYER_MAXS, end, player, solidMask);
    if (tr.startSolid || tr.allSolid ||
        (tr.contents & (CONTENTS_LAVA | CONTENTS_SLIME)))
      continue;

    // Step 2: Then move backwards from that elevated point
    Vector3 forward;
    AngleVectors(yawAngles, forward, nullptr, nullptr);
    start = tr.endPos;
    end = start + forward * backDistance;
    tr = gi.trace(start, PLAYER_MINS, PLAYER_MAXS, end, player, solidMask);
    if (tr.startSolid || tr.allSolid ||
        (tr.contents & (CONTENTS_LAVA | CONTENTS_SLIME)))
      continue;

    // Step 3: Now cast downward to find solid ground
    start = tr.endPos;
    end = start - Vector3{0, 0, upDistance * 4};
    tr = gi.trace(start, PLAYER_MINS, PLAYER_MAXS, end, player, solidMask);
    if (tr.startSolid || tr.allSolid || tr.fraction == 1.0f ||
        tr.ent != world || tr.plane.normal.z < 0.7f)
      continue;

    // Avoid liquids
    if (gi.pointContents(tr.endPos + Vector3{0, 0, viewHeight}) & MASK_WATER)
      continue;

    // Height delta check
    float zDelta = std::fabs(player->s.origin[_Z] - tr.endPos[2]);
    float stepLimit = (player->s.origin[_Z] < 0 ? STEPSIZE_BELOW : STEPSIZE);
    if (zDelta > stepLimit * 4.0f)
      continue;

    // If stepped up/down, ensure visibility
    if (zDelta > stepLimit) {
      if (gi.traceLine(player->s.origin, tr.endPos, player, solidMask)
              .fraction != 1.0f)
        continue;
      if (gi.traceLine(player->s.origin + Vector3{0, 0, viewHeight},
                       tr.endPos + Vector3{0, 0, viewHeight}, player, solidMask)
              .fraction != 1.0f)
        continue;
    }

    spot = tr.endPos;
    return true;
  }

  return false;
}

/*
==============================
G_FindSquadRespawnTarget

Scans for a valid living player who is not in combat or danger
and has a suitable spawn spot nearby. Returns the player and spot.
==============================
*/
static inline std::tuple<gentity_t *, Vector3> G_FindSquadRespawnTarget() {
  const bool anyMonstersSearching = G_MonstersSearchingFor(nullptr);

  for (gentity_t *player : active_clients()) {
    auto *cl = player->client;

    // Skip invalid candidates
    if (player->deadFlag)
      continue;

    using enum CoopRespawn;

    if (cl->last_damage_time >= level.time) {
      cl->coopRespawnState = InCombat;
      continue;
    }
    if (G_MonstersSearchingFor(player)) {
      cl->coopRespawnState = InCombat;
      continue;
    }
    if (anyMonstersSearching && cl->lastFiringTime >= level.time) {
      cl->coopRespawnState = InCombat;
      continue;
    }
    if (player->groundEntity != world) {
      cl->coopRespawnState = BadArea;
      continue;
    }
    if (player->waterLevel >= WATER_UNDER) {
      cl->coopRespawnState = BadArea;
      continue;
    }

    Vector3 spot;
    if (!G_FindRespawnSpot(player, spot)) {
      cl->coopRespawnState = Blocked;
      continue;
    }

    return {player, spot};
  }

  return {nullptr, vec3_origin};
}

enum respawn_state_t {
  RESPAWN_NONE,     // invalid state
  RESPAWN_SPECTATE, // move to spectator
  RESPAWN_SQUAD,    // move to good squad point
  RESPAWN_START     // move to start of map
};

// [Paril-KEX] return false to fall back to click-to-respawn behavior.
// note that this is only called if they are allowed to respawn (not
// restarting the level due to all being dead)
bool G_LimitedLivesRespawn(gentity_t *ent) {
  if (CooperativeModeOn()) {
    const bool limitedLives = G_LimitedLivesInCoop();
    const bool allowSquadRespawn =
        coop->integer && g_coop_squad_respawn->integer;

    if (!allowSquadRespawn && !limitedLives)
      return false;

    respawn_state_t state = RESPAWN_NONE;

    if (limitedLives && ent->client->pers.lives == 0) {
      state = RESPAWN_SPECTATE;
      ent->client->coopRespawnState = CoopRespawn::NoLives;
    }

    if (state == RESPAWN_NONE) {
      if (allowSquadRespawn) {
        bool allDead = true;

        for (auto player : active_clients()) {
          if (player->health > 0) {
            allDead = false;
            break;
          }
        }

        if (allDead)
          state = RESPAWN_START;
        else {
          auto [good_player, good_spot] = G_FindSquadRespawnTarget();

          if (good_player) {
            state = RESPAWN_SQUAD;

            ent->client->coopRespawn.squadOrigin = good_spot;
            ent->client->coopRespawn.squadAngles = good_player->s.angles;
            ent->client->coopRespawn.squadAngles[ROLL] = 0;

            ent->client->coopRespawn.useSquad = true;
          } else {
            state = RESPAWN_SPECTATE;
          }
        }
      } else
        state = RESPAWN_START;
    }

    if (state == RESPAWN_SQUAD || state == RESPAWN_START) {
      if (P_UseCoopInstancedItems())
        ent->client->pers.health = ent->client->pers.maxHealth = ent->maxHealth;

      ClientRespawn(ent);

      ent->client->latchedButtons = BUTTON_NONE;
      ent->client->coopRespawn.useSquad = false;
    } else if (state == RESPAWN_SPECTATE) {
      if (!static_cast<int>(ent->client->coopRespawnState))
        ent->client->coopRespawnState = CoopRespawn::Waiting;

      if (ClientIsPlaying(ent->client)) {
        CopyToBodyQue(ent);
        ent->client->sess.team = Team::Spectator;
        MoveClientToFreeCam(ent);
        gi.linkEntity(ent);
        GetFollowTarget(ent);
      }
    }

    return true;
  }

  if (G_LimitedLivesInLMS()) {
    if (ent->client->pers.lives == 0) {
      ent->client->eliminated = true;
      ent->client->coopRespawnState = CoopRespawn::NoLives;
      if (ClientIsPlaying(ent->client)) {
        CopyToBodyQue(ent);
        MoveClientToFreeCam(ent);
        gi.linkEntity(ent);
        GetFollowTarget(ent);
      }
      return true;
    }
    ent->client->coopRespawnState = CoopRespawn::None;
    return false;
  }

  return false;
}

/*
==============
ClientBeginServerFrame

This will be called once for each server frame, before running
any other entities in the world.
==============
*/
/*
==============
ClientBeginServerFrame

Relays per-frame setup through ClientSessionServiceImpl to maintain consistent
state handling.
==============
*/
void ClientBeginServerFrame(gentity_t *ent) {
  auto &service = worr::server::client::GetClientSessionService();
  service.ClientBeginServerFrame(gi, game, level, ent);
}

/*
==============
RemoveAttackingPainDaemons

This is called to clean up the pain daemons that the disruptor attaches
to clients to damage them.
==============
*/
void RemoveAttackingPainDaemons(gentity_t *self) {
  gentity_t *tracker =
      G_FindByString<&gentity_t::className>(nullptr, "pain daemon");

  while (tracker) {
    if (tracker->enemy == self)
      FreeEntity(tracker);
    tracker = G_FindByString<&gentity_t::className>(tracker, "pain daemon");
  }

  if (self->client)
    self->client->trackerPainTime = 0_ms;
}
