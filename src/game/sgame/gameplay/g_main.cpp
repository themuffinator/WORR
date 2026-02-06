/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

g_main.cpp (Game Main) This is the main entry point and central hub for the
server-side game module (game.dll). It is responsible for initializing and
shutting down the game, managing the main game loop, and orchestrating the
high-level logic of a match. Key Responsibilities: - API Bridge: Implements
`GetGameAPI`, which provides the engine with the necessary function pointers to
interact with the game logic. - Initialization: `InitGame` is called once per
server startup to register cvars and initialize global game state.
`SpawnEntities` is called for each map load.
- Game Loop: `G_RunFrame` is the main function called by the engine every server
frame. It drives all entity thinking, physics, and game rule checks. - Match
State Management: Contains the top-level logic for checking game rules (e.g.,
timelimit, fraglimit) and transitioning the game into and out of intermission. -
Cvar Management: Handles the checking and application of various cvars that can
change game behavior on the fly.*/

#include "../../bgame/char_array_utils.hpp"
#include "../../bgame/logger.hpp"
#include "../bots/bot_includes.hpp"
#include "../commands/commands.hpp"
#include "../g_local.hpp"
#include "g_clients.hpp"
#include "g_headhunters.hpp"
#include "g_hud_blob.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <memory>
#include <new>
#include <sstream>
#include <string>
#include <vector>

CHECK_GCLIENT_INTEGRITY;
CHECK_ENTITY_INTEGRITY;

std::tm LocalTimeNow();

constexpr int32_t DEFAULT_GRAPPLE_SPEED = 750;    // speed of grapple in flight
constexpr float DEFAULT_GRAPPLE_PULL_SPEED = 750; // speed player is pulled at

std::mt19937 mt_rand;

GameLocals game;
LevelLocals level;

local_game_import_t gi;
game_import_t base_import;

/*static*/ char local_game_import_t::print_buffer[0x10000];

/*static*/ std::array<char[MAX_INFO_STRING], MAX_LOCALIZATION_ARGS>
    local_game_import_t::buffers;
/*static*/ std::array<const char *, MAX_LOCALIZATION_ARGS>
    local_game_import_t::buffer_ptrs;

game_export_t globals;
spawn_temp_t st;

cached_modelIndex sm_meat_index;
cached_soundIndex snd_fry;

gentity_t *g_entities;

cvar_t *hostname;

cvar_t *deathmatch;
cvar_t *ctf;
cvar_t *teamplay;
cvar_t *g_gametype;
cvar_t *g_practice;

/*
=============
InitServerLogging

Configure shared logging for the server game module.
=============
*/
static void InitServerLogging() {
  base_import = gi;
  const auto print_sink =
      [print_fn = base_import.Com_Print](std::string_view message) {
        print_fn(message.data());
      };
  const auto error_sink =
      [error_fn = base_import.Com_Error](std::string_view message) {
        error_fn(message.data());
      };

  worr::InitLogger("server", print_sink, error_sink);
  gi.Com_Print = worr::LoggerPrint;
}

cvar_t *coop;

cvar_t *skill;
cvar_t *fragLimit;
cvar_t *captureLimit;
cvar_t *timeLimit;
cvar_t *roundLimit;
cvar_t *roundTimeLimit;
cvar_t *mercyLimit;
cvar_t *noPlayersTime;
cvar_t *marathon;
cvar_t *g_marathon_timelimit;
cvar_t *g_marathon_scorelimit;

cvar_t *g_ruleset;

cvar_t *password;
cvar_t *spectatorPassword;
cvar_t *admin_password;
cvar_t *needPass;
cvar_t *filterBan;

static cvar_t *maxclients;
static cvar_t *maxentities;
cvar_t *maxplayers;
cvar_t *minplayers;

cvar_t *ai_allow_dm_spawn;
cvar_t *ai_damage_scale;
cvar_t *ai_model_scale;
cvar_t *ai_movement_disabled;
cvar_t *ai_widow_roof_spawn;
cvar_t *bob_pitch;
cvar_t *bob_roll;
cvar_t *bob_up;
cvar_t *bot_debug_follow_actor;
cvar_t *bot_debug_move_to_point;
cvar_t *flood_msgs;
cvar_t *flood_persecond;
cvar_t *flood_waitdelay;
cvar_t *gun_x, *gun_y, *gun_z;
cvar_t *run_pitch;
cvar_t *run_roll;

cvar_t *g_allowAdmin;
cvar_t *g_allowCustomSkins;
cvar_t *g_allowForfeit;
cvar_t *g_allow_grapple;
cvar_t *g_allow_kill;
cvar_t *g_allow_duel_queue;
cvar_t *g_allowMymap;
cvar_t *g_allowSpecVote;
cvar_t *g_allowTechs;
cvar_t *g_allowVoteMidGame;
cvar_t *g_allowVoting;
cvar_t *g_arenaSelfDmgArmor;
cvar_t *g_arenaStartingArmor;
cvar_t *g_arenaStartingHealth;
cvar_t *g_cheats;
cvar_t *g_ghost_min_play_time;
cvar_t *g_coop_enable_lives;
cvar_t *g_coop_health_scaling;
cvar_t *g_coop_instanced_items;
cvar_t *g_coop_num_lives;
cvar_t *g_coop_player_collision;
cvar_t *g_coop_squad_respawn;
cvar_t *g_lms_num_lives;
cvar_t *g_damage_scale;
cvar_t *g_debug_monster_kills;
cvar_t *g_debug_monster_paths;
cvar_t *g_dedicated;
cvar_t *g_disable_player_collision;
cvar_t *match_startNoHumans;
cvar_t *match_autoJoin;
cvar_t *match_crosshairIDs;
cvar_t *warmup_doReadyUp;
cvar_t *warmup_enabled;
cvar_t *g_dm_exec_level_cfg;
cvar_t *match_forceJoin;
cvar_t *match_doForceRespawn;
cvar_t *match_forceRespawnTime;
cvar_t *match_holdableAdrenaline;
cvar_t *match_instantItems;
cvar_t *owner_intermissionShots;
cvar_t *match_itemsRespawnRate;
cvar_t *g_fallingDamage;
cvar_t *g_selfDamage;
cvar_t *match_doOvertime;
cvar_t *match_powerupDrops;
cvar_t *match_powerupMinPlayerLock;
cvar_t *g_dm_random_items;
cvar_t *g_domination_tick_interval;
cvar_t *g_domination_points_per_tick;
cvar_t *g_domination_capture_bonus;
cvar_t *g_domination_capture_time;
cvar_t *g_domination_neutralize_time;
cvar_t *match_playerRespawnMinDelay;
cvar_t *match_playerRespawnMinDistance;
cvar_t *match_playerRespawnMinDistanceDebug;
cvar_t *match_map_sameLevel;
cvar_t *match_allowSpawnPads;
cvar_t *g_dm_strong_mines;
cvar_t *match_allowTeleporterPads;
cvar_t *match_timeoutLength;
cvar_t *match_weaponsStay;
cvar_t *match_dropCmdFlags;
cvar_t *g_entityOverrideDir;
cvar_t *g_entityOverrideLoad;
cvar_t *g_entityOverrideSave;
cvar_t *g_eyecam;
cvar_t *g_fastDoors;
cvar_t *g_frag_messages;
cvar_t *g_frenzy;
cvar_t *g_friendlyFireScale;
cvar_t *g_frozen_time;
cvar_t *g_grapple_damage;
cvar_t *g_grapple_fly_speed;
cvar_t *g_grapple_offhand;
cvar_t *g_grapple_pull_speed;
cvar_t *g_gravity;
cvar_t *g_gravity_lotto;
cvar_t *g_horde_starting_wave;
cvar_t *g_huntercam;
cvar_t *g_inactivity;
cvar_t *g_infiniteAmmo;
cvar_t *g_instaGib;
cvar_t *g_instagib_splash;
cvar_t *g_instantWeaponSwitch;
cvar_t *g_itemBobbing;
cvar_t *g_knockbackScale;
cvar_t *g_ladderSteps;
cvar_t *g_lagCompensation;
cvar_t *g_level_rulesets;
cvar_t *match_maps_list;
cvar_t *match_maps_listShuffle;
cvar_t *match_lock;
cvar_t *match_setup_active;
cvar_t *match_setup_format;
cvar_t *match_setup_gametype;
cvar_t *match_setup_modifier;
cvar_t *match_setup_maxplayers;
cvar_t *match_setup_length;
cvar_t *match_setup_type;
cvar_t *match_setup_bestof;
cvar_t *g_tourney_cfg;
cvar_t *g_matchstats;
cvar_t *g_maxvelocity;
cvar_t *g_motd_filename;
cvar_t *g_mover_debug;
cvar_t *g_mover_speed_scale;
cvar_t *g_nadeFest;
cvar_t *g_no_armor;
cvar_t *g_mapspawn_no_bfg;
cvar_t *g_mapspawn_no_plasmabeam;
cvar_t *g_no_health;
cvar_t *g_no_items;
cvar_t *g_no_mines;
cvar_t *g_no_nukes;
cvar_t *g_no_powerups;
cvar_t *g_no_spheres;
cvar_t *g_owner_auto_join;
cvar_t *g_owner_push_scores;
cvar_t *g_quadhog;
cvar_t *g_quickWeaponSwitch;
cvar_t *g_rollAngle;
cvar_t *g_rollSpeed;
cvar_t *g_select_empty;
cvar_t *g_showhelp;
cvar_t *g_showmotd;
cvar_t *g_skip_view_modifiers;
cvar_t *g_start_items;
cvar_t *g_starting_health;
cvar_t *g_starting_health_bonus;
cvar_t *g_starting_armor;
cvar_t *g_stopspeed;
cvar_t *g_strict_saves;
cvar_t *g_teamplay_allow_team_pick;
cvar_t *g_teamplay_armor_protect;
cvar_t *g_teamplay_auto_balance;
cvar_t *g_teamplay_force_balance;
cvar_t *g_teamplay_item_drop_notice;
cvar_t *g_vampiric_damage;
cvar_t *g_vampiric_exp_min;
cvar_t *g_vampiric_health_max;
cvar_t *g_vampiric_percentile;
cvar_t *g_verbose;
cvar_t *g_vote_flags;
cvar_t *g_vote_limit;
cvar_t *g_warmup_countdown;
cvar_t *g_warmup_ready_percentage;
cvar_t *g_weaponProjection;
cvar_t *g_weapon_respawn_time;

cvar_t *g_maps_pool_file;
cvar_t *g_maps_cycle_file;
cvar_t *g_maps_selector;
cvar_t *g_maps_mymap;
cvar_t *g_maps_mymap_queue_limit;
cvar_t *g_maps_allow_custom_textures;
cvar_t *g_maps_allow_custom_sounds;

cvar_t *g_statex_enabled;
cvar_t *g_statex_humans_present;
cvar_t *g_statex_export_html;

cvar_t *g_blueTeamName;
cvar_t *g_redTeamName;

cvar_t *bot_name_prefix;

cvar_t *g_autoScreenshotTool;

static cvar_t *g_framesPerFrame;

int ii_duel_header;
int ii_highlight;
int ii_ctf_red_dropped;
int ii_ctf_blue_dropped;
int ii_ctf_red_taken;
int ii_ctf_blue_taken;
int ii_teams_red_default;
int ii_teams_blue_default;
int ii_teams_red_tiny;
int ii_teams_blue_tiny;
int ii_teams_header_red;
int ii_teams_header_blue;
int mi_ctf_red_flag, mi_ctf_blue_flag; // [Paril-KEX]

void ClientThink(gentity_t *ent, usercmd_t *cmd);
gentity_t *ClientChooseSlot(const char *userInfo, const char *socialID,
                            bool isBot, gentity_t **ignore, size_t num_ignore,
                            bool cinematic);
bool ClientConnect(gentity_t *ent, char *userInfo, const char *socialID,
                   bool isBot);
char *WriteGameJson(bool autosave, size_t *out_size);
void ReadGameJson(const char *jsonString);
char *WriteLevelJson(bool transition, size_t *out_size);
void ReadLevelJson(const char *jsonString);
bool CanSave();
void ClientDisconnect(gentity_t *ent);
void ClientBegin(gentity_t *ent);
void ClientCommand(gentity_t *ent);
void G_RunFrame(bool main_loop);
void G_PrepFrame();
void G_InitSave();

#include <chrono>

// =================================================

/*
=============
LoadMotd

Loads the message of the day file after validating the configured filename.
=============
*/
void LoadMotd() {
  const char *rawName = g_motd_filename->string;
  const std::string configuredName = rawName[0] ? rawName : "motd.txt";
  std::string activeGameDir;

  if (gi.cvar) {
    cvar_t *gameCvar = gi.cvar("game", "", CVAR_NOFLAGS);

    if (gameCvar && gameCvar->string && gameCvar->string[0]) {
      activeGameDir = gameCvar->string;
    }
  }

  std::vector<std::filesystem::path> motdRoots;

  if (!activeGameDir.empty()) {
    motdRoots.emplace_back(activeGameDir);
  }

  if (motdRoots.empty() || activeGameDir != "baseq2") {
    motdRoots.emplace_back("baseq2");
  }

  std::string effectiveName = configuredName;
  bool invalidNameReported = false;
  bool loaded = false;

  auto validateAndResolve = [](const std::string &name,
                               const std::filesystem::path &basePath,
                               std::filesystem::path &outPath) -> bool {
    std::filesystem::path relativePath(name);

    if (basePath.empty() || relativePath.empty()) {
      return false;
    }

    if (relativePath.is_absolute() || relativePath.has_root_path()) {
      return false;
    }

    for (const std::filesystem::path &part : relativePath) {
      if (part == "." || part == "..") {
        return false;
      }
    }

    const std::filesystem::path normalizedBase = basePath.lexically_normal();
    const std::filesystem::path candidate =
        (basePath / relativePath).lexically_normal();
    auto candidateIter = candidate.begin();

    for (const std::filesystem::path &basePart : normalizedBase) {
      if (candidateIter == candidate.end() || basePart != *candidateIter) {
        return false;
      }

      ++candidateIter;
    }

    outPath = candidate;
    return true;
  };

  auto loadMotdFile = [&](const std::filesystem::path &resolvedPath) -> bool {
    const std::string resolvedPathString = resolvedPath.string();
    std::unique_ptr<FILE, decltype(&std::fclose)> file(
        fopen(resolvedPathString.c_str(), "rb"), &std::fclose);

    if (!file) {
      if (g_verbose && g_verbose->integer) {
        gi.Com_PrintFmt("{}: MotD file not found: {}\n", __FUNCTION__,
                        resolvedPathString);
      }

      return false;
    }

    bool valid = true;
    std::string contents;

    if (fseek(file.get(), 0, SEEK_END) != 0) {
      valid = false;
    }

    long endPosition = valid ? ftell(file.get()) : -1;

    if (endPosition < 0) {
      valid = false;
    }

    if (valid) {
      if (fseek(file.get(), 0, SEEK_SET) != 0) {
        valid = false;
      }
    }

    if (valid) {
      const std::size_t length = static_cast<std::size_t>(endPosition);

      if (length > 0x40000) {
        gi.Com_PrintFmt("{}: MotD file length exceeds maximum: {}\n",
                        __FUNCTION__, resolvedPathString);
        valid = false;
      }

      else {
        contents.resize(length);

        if (length > 0) {
          const std::size_t readLength =
              fread(contents.data(), 1, length, file.get());

          if (readLength != length) {
            gi.Com_PrintFmt("{}: MotD file read error: {}\n", __FUNCTION__,
                            resolvedPathString);
            valid = false;
          }
        }
      }
    }

    if (valid) {
      game.motd = contents;
      game.motdModificationCount++;

      if (g_verbose && g_verbose->integer) {
        gi.Com_PrintFmt("{}: MotD file verified and loaded: {}\n", __FUNCTION__,
                        resolvedPathString);
      }

      return true;
    }

    gi.Com_PrintFmt("{}: MotD file load error for {}, discarding.\n",
                    __FUNCTION__, resolvedPathString);
    return false;
  };

  while (!loaded) {
    bool validPathFound = false;

    for (const std::filesystem::path &basePath : motdRoots) {
      std::filesystem::path resolvedPath;

      if (!validateAndResolve(effectiveName, basePath, resolvedPath)) {
        continue;
      }

      validPathFound = true;

      if (loadMotdFile(resolvedPath)) {
        loaded = true;
        break;
      }
    }

    if (loaded) {
      break;
    }

    if (!validPathFound) {
      if (!invalidNameReported) {
        gi.Com_PrintFmt("{}: Invalid MotD filename, ignoring: {}\n",
                        __FUNCTION__, effectiveName);
        invalidNameReported = true;
        effectiveName = "motd.txt";
        continue;
      }

      gi.Com_PrintFmt("{}: Default MotD filename failed validation: {}\n",
                      __FUNCTION__, effectiveName);
      return;
    }

    return;
  }
}

int check_ruleset = -1;
static void CheckRuleset() {
  // if (!deathmatch->integer)
  //	return;

  // don't do this if we're forcing a level ruleset
  if (g_level_rulesets->integer)
    return;

  if (game.ruleset && check_ruleset == g_ruleset->modifiedCount)
    return;

  game.ruleset = Ruleset(
      std::clamp(g_ruleset->integer, static_cast<int>(Ruleset::None) + 1,
                 static_cast<int>(Ruleset::RS_NUM_RULESETS) - 1));

  if ((int)game.ruleset != g_ruleset->integer)
    gi.cvarForceSet("g_ruleset", G_Fmt("{}", (int)game.ruleset).data());

  check_ruleset = g_ruleset->modifiedCount;

  const int32_t airAccel = GetRulesetAirAccel(game.ruleset);
  if (pm_config.airAccel != airAccel) {
    pm_config.airAccel = airAccel;
    gi.configString(CS_AIRACCEL, G_Fmt("{}", pm_config.airAccel).data());
  }

  const bool q3Overbounce = RS(Quake3Arena);
  if (pm_config.q3Overbounce != q3Overbounce) {
    pm_config.q3Overbounce = q3Overbounce;
    gi.configString(CONFIG_Q3_OVERBOUNCE, pm_config.q3Overbounce ? "1" : "0");
  }

  gi.LocBroadcast_Print(PRINT_HIGH, "$g_sgame_auto_6d57e94076e2",
                        rs_long_name[(int)game.ruleset]);
}

int gt_teamplay = 0;
int gt_ctf = 0;
int gt_g_gametype = 0;
bool gt_teams_on = false;
GameType gt_check = GameType::None;

/*
=============
MapSupportsGametype

Uses map pool metadata to determine whether the current map supports a
requested gametype.
=============
*/
static bool MapSupportsGametype(GameType gt) {
  const MapEntry *map = game.mapSystem.GetMapEntry(level.mapName.data());
  if (!map)
    return true;

  const GameFlags flags = Game::GetInfo(gt).flags;

  if (HasFlag(flags, GameFlags::CTF))
    return map->preferredCTF;
  if (HasFlag(flags, GameFlags::OneVOne))
    return map->preferredDuel;
  if (HasFlag(flags, GameFlags::Teams))
    return map->preferredTDM;

  return true;
}

/*
============
Match_SetGameType

Applies the pending gametype change and resets the current map from the cached
entity string when possible.
============
*/
static void Match_SetGameType(GameType gt) {
  if (!MapSupportsGametype(gt)) {
    gi.LocBroadcast_Print(PRINT_HIGH, "$g_sgame_auto_5b21f7c73e78",
                          level.mapName.data(), Game::GetInfo(gt).long_name);
    gi.Com_PrintFmt(
        "{}: Map {} incompatible with {}, using {} fallback.\n", __FUNCTION__,
        level.mapName.data(), Game::GetInfo(gt).long_name,
        (g_allowVoting && g_allowVoting->integer) ? "mapvote" : "nextmap");
    gi.AddCommandString(
        (g_allowVoting && g_allowVoting->integer) ? "mapvote\n" : "nextmap\n");
    return;
  }

  gi.cvarForceSet("g_gametype", G_Fmt("{}", (int)gt).data());
  gt_g_gametype = g_gametype->modifiedCount;
  gt_check = gt;
  level.matchReloadedFromEntities = false;

  const bool canReloadEntities = !level.savedEntityString.empty();

  GT_PrecacheAssets();
  GT_SetLongName();
  gi.LocBroadcast_Print(PRINT_CENTER, "$format_passthrough", level.gametype_name.data());

  if (canReloadEntities) {
    Match_Reset();

    if (!level.matchReloadedFromEntities) {
      gi.Com_PrintFmt(
          "{}: Falling back to gamemap {} because map state reload failed.\n",
          __FUNCTION__, level.mapName.data());
      gi.AddCommandString(G_Fmt("gamemap {}\n", level.mapName.data()).data());
    }
    return;
  }

  gi.Com_PrintFmt("{}: Cached entity string missing for {}, reloading map.\n",
                  __FUNCTION__, level.mapName.data());
  gi.AddCommandString(G_Fmt("gamemap {}\n", level.mapName.data()).data());
}

/*
=============
GT_Changes

Synchronizes gametype-related cvars and handles gametype transitions.
=============
*/
static void GT_Changes() {
  if (!deathmatch->integer)
    return;

  // do these checks only once level has initialised
  if (!level.init)
    return;

  bool changed = false, team_reset = false;
  GameType gt = GameType::None;

  if (gt_g_gametype != g_gametype->modifiedCount) {
    const GameType normalized = Game::NormalizeTypeValue(g_gametype->integer);
    if (static_cast<int>(normalized) != g_gametype->integer)
      gi.cvarForceSet("g_gametype",
                      G_Fmt("{}", static_cast<int>(normalized)).data());

    gt = normalized;

    if (gt != gt_check) {
      const auto &gt_info = Game::GetInfo(gt);
      const bool has_teams = HasFlag(gt_info.flags, GameFlags::Teams);
      const bool has_ctf = HasFlag(gt_info.flags, GameFlags::CTF);

      if (teamplay->integer != static_cast<int>(has_teams))
        gi.cvarForceSet("teamplay", has_teams ? "1" : "0");

      if (ctf->integer != static_cast<int>(has_ctf))
        gi.cvarForceSet("ctf", has_ctf ? "1" : "0");

      gt_teamplay = teamplay->modifiedCount;
      gt_ctf = ctf->modifiedCount;
      changed = true;
    }
  }
  if (!changed) {
    if (gt_teamplay != teamplay->modifiedCount) {
      if (teamplay->integer) {
        gt = GameType::TeamDeathmatch;
        if (!teamplay->integer)
          gi.cvarForceSet("teamplay", "1");
        if (ctf->integer)
          gi.cvarForceSet("ctf", "0");
      } else {
        gt = GameType::FreeForAll;
        if (teamplay->integer)
          gi.cvarForceSet("teamplay", "0");
        if (ctf->integer)
          gi.cvarForceSet("ctf", "0");
      }
      changed = true;
      gt_teamplay = teamplay->modifiedCount;
      gt_ctf = ctf->modifiedCount;
    }
    if (gt_ctf != ctf->modifiedCount) {
      if (ctf->integer) {
        gt = GameType::CaptureTheFlag;
        if (teamplay->integer)
          gi.cvarForceSet("teamplay", "0");
        if (!ctf->integer)
          gi.cvarForceSet("ctf", "1");
      } else {
        gt = GameType::TeamDeathmatch;
        if (!teamplay->integer)
          gi.cvarForceSet("teamplay", "1");
        if (ctf->integer)
          gi.cvarForceSet("ctf", "0");
      }
      changed = true;
      gt_teamplay = teamplay->modifiedCount;
      gt_ctf = ctf->modifiedCount;
    }
  }

  if (!changed)
    return;

  // gi.Com_PrintFmt("GAMETYPE = {}\n", (int)gt);

  if (gt_teams_on != Teams()) {
    team_reset = true;
    gt_teams_on = Teams();
  }

  if (team_reset) {
    // move all to spectator first
    for (auto ec : active_clients()) {
      // SetIntermissionPoint();
      FindIntermissionPoint();

      ec->s.origin = level.intermission.origin;
      ec->client->ps.pmove.origin = level.intermission.origin;
      ec->client->ps.viewAngles = level.intermission.angles;

      ec->client->awaitingRespawn = true;
      ec->client->ps.pmove.pmType = PM_FREEZE;
      ec->client->ps.rdFlags = RDF_NONE;
      ec->deadFlag = false;
      ec->solid = SOLID_NOT;
      ec->moveType = MoveType::FreeCam;
      ec->s.modelIndex = 0;
      ec->svFlags |= SVF_NOCLIENT;
      gi.linkEntity(ec);
    }

    // set to team and reset match
    for (auto ec : active_clients()) {
      if (!ClientIsPlaying(ec->client))
        continue;
      SetTeam(ec, PickTeam(-1), false, false, true);
    }
  }

  if (gt != gt_check)
    Match_SetGameType(gt);
}

/*
============
PreInitGame

This will be called when the dll is first loaded, which
only happens when a new game is started or a save game
is loaded.
============
*/
extern void GT_Init();
static void PreInitGame() {
  maxclients = gi.cvar("maxclients", G_Fmt("{}", MAX_SPLIT_PLAYERS).data(),
                       CVAR_SERVERINFO | CVAR_LATCH);
  minplayers = gi.cvar("minplayers", "2", CVAR_NOFLAGS);
  maxplayers = gi.cvar("maxplayers", "16", CVAR_NOFLAGS);

  GT_Init();
}

/*
============
InitMapSystem
============
*/
static void InitMapSystem(gentity_t *ent) {
  if (game.mapSystem.mapPool.empty())
    LoadMapPool(ent);

  bool hasCycleable =
      std::any_of(game.mapSystem.mapPool.begin(), game.mapSystem.mapPool.end(),
                  [](const MapEntry &m) { return m.isCycleable; });

  if (!hasCycleable)
    LoadMapCycle(ent);
}

// ================================================

/*
================
ParseIDListFile
================
*/
static std::unordered_set<std::string> ParseIDListFile(const char *filename) {
  std::unordered_set<std::string> ids;

  std::ifstream file(filename);
  if (!file.is_open())
    return ids;

  std::string line;
  bool inCommentBlock = false;

  while (std::getline(file, line)) {
    // Remove leading/trailing whitespace
    line.erase(0, line.find_first_not_of(" \t\r\n"));
    line.erase(line.find_last_not_of(" \t\r\n") + 1);

    if (line.empty())
      continue;

    // Handle block comments
    if (inCommentBlock) {
      if (line.find("*/") != std::string::npos)
        inCommentBlock = false;
      continue;
    }
    if (line.find("/*") != std::string::npos) {
      inCommentBlock = true;
      continue;
    }

    // Skip single-line comments
    if (line.starts_with("#") || line.starts_with("//"))
      continue;

    // Replace commas with spaces
    for (char &ch : line) {
      if (ch == ',')
        ch = ' ';
    }

    std::istringstream iss(line);
    std::string id;
    while (iss >> id) {
      if (!id.empty())
        ids.insert(id);
    }
  }

  return ids;
}

/*
================
LoadBanList
================
*/
void LoadBanList() { game.bannedIDs = ParseIDListFile("ban.txt"); }

/*
================
LoadAdminList
================
*/
void LoadAdminList() { game.adminIDs = ParseIDListFile("admin.txt"); }

/*
================
AppendIDToFile
================
*/
bool AppendIDToFile(const char *filename, const std::string &id) {
  std::ofstream file(filename, std::ios::app);
  if (!file.is_open())
    return false;

  file << id << "\n";
  return true;
}

/*
================
RemoveIDFromFile
================
*/
bool RemoveIDFromFile(const char *filename, const std::string &id) {
  std::ifstream infile(filename);
  if (!infile.is_open())
    return false;

  std::vector<std::string> lines;
  std::string line;

  while (std::getline(infile, line)) {
    std::string trimmed = line;
    const std::size_t firstNonWhitespace = trimmed.find_first_not_of(" \t\r\n");
    if (firstNonWhitespace == std::string::npos) {
      trimmed.clear();
    } else {
      const std::size_t lastNonWhitespace = trimmed.find_last_not_of(" \t\r\n");
      trimmed = trimmed.substr(firstNonWhitespace,
                               lastNonWhitespace - firstNonWhitespace + 1);
    }

    if (trimmed == id)
      continue;

    lines.push_back(line); // preserve original line formatting
  }

  std::ofstream outfile(filename, std::ios::trunc);
  if (!outfile.is_open())
    return false;

  for (const auto &out : lines)
    outfile << out << "\n";

  return true;
}

// ================================================

/*
============
InitGame

Called after PreInitGame when the game has set up cvars.
============
*/
static void InitGame() {
  gi.Com_Print("==== InitGame ====\n");

  RegisterAllCommands();

  G_InitSave();

  game = {};

  std::random_device rd;
  game.mapRNG.seed(rd());

  std::mt19937 mapRNGPreview = game.mapRNG;
  std::array<uint32_t, 3> mapRNGPreviewValues = {};
  for (auto &value : mapRNGPreviewValues)
    value = mapRNGPreview();

  gi.Com_PrintFmt("InitGame: map RNG preview values: {}, {}, {}\n",
                  mapRNGPreviewValues[0], mapRNGPreviewValues[1],
                  mapRNGPreviewValues[2]);

  // seed RNG
  mt_rand.seed(
      (uint32_t)std::chrono::system_clock::now().time_since_epoch().count());

  hostname = gi.cvar("hostname", "Welcome to WORR!", CVAR_NOFLAGS);

  gun_x = gi.cvar("gun_x", "0", CVAR_NOFLAGS);
  gun_y = gi.cvar("gun_y", "0", CVAR_NOFLAGS);
  gun_z = gi.cvar("gun_z", "0", CVAR_NOFLAGS);

  g_rollSpeed = gi.cvar("g_roll_speed", "200", CVAR_NOFLAGS);
  g_rollAngle = gi.cvar("g_roll_angle", "2", CVAR_NOFLAGS);
  g_maxvelocity = gi.cvar("g_max_velocity", "2000", CVAR_NOFLAGS);
  g_gravity = gi.cvar("g_gravity", "800", CVAR_NOFLAGS);

  g_skip_view_modifiers = gi.cvar("g_skip_view_modifiers", "0", CVAR_NOSET);

  g_stopspeed = gi.cvar("g_stop_speed", "100", CVAR_NOFLAGS);

  g_horde_starting_wave =
      gi.cvar("g_horde_starting_wave", "1", CVAR_SERVERINFO | CVAR_LATCH);

  g_huntercam = gi.cvar("g_hunter_cam", "1", CVAR_SERVERINFO | CVAR_LATCH);
  g_dm_strong_mines = gi.cvar("g_dm_strong_mines", "0", CVAR_NOFLAGS);
  g_dm_random_items = gi.cvar("g_dm_random_items", "0", CVAR_NOFLAGS);
  g_domination_tick_interval =
      gi.cvar("g_domination_tick_interval", "1.0", CVAR_NOFLAGS);
  g_domination_points_per_tick =
      gi.cvar("g_domination_points_per_tick", "1", CVAR_NOFLAGS);
  g_domination_capture_bonus =
      gi.cvar("g_domination_capture_bonus", "5", CVAR_NOFLAGS);
  g_domination_capture_time =
      gi.cvar("g_domination_capture_time", "3.0", CVAR_NOFLAGS);
  g_domination_neutralize_time =
      gi.cvar("g_domination_neutralize_time", "2.0", CVAR_NOFLAGS);

  // freeze tag
  g_frozen_time = gi.cvar("g_frozen_time", "180", CVAR_NOFLAGS);

  // [Paril-KEX]
  g_coop_player_collision = gi.cvar("g_coop_player_collision", "0", CVAR_LATCH);
  g_coop_squad_respawn = gi.cvar("g_coop_squad_respawn", "1", CVAR_LATCH);
  g_coop_enable_lives = gi.cvar("g_coop_enable_lives", "0", CVAR_LATCH);
  g_coop_num_lives = gi.cvar("g_coop_num_lives", "2", CVAR_LATCH);
  g_coop_instanced_items = gi.cvar("g_coop_instanced_items", "1", CVAR_LATCH);
  g_lms_num_lives = gi.cvar("g_lms_num_lives", "4", CVAR_LATCH);
  g_allow_grapple = gi.cvar("g_allow_grapple", "auto", CVAR_NOFLAGS);
  g_allow_kill = gi.cvar("g_allow_kill", "1", CVAR_NOFLAGS);
  g_grapple_offhand = gi.cvar("g_grapple_offhand", "0", CVAR_NOFLAGS);
  g_grapple_fly_speed =
      gi.cvar("g_grapple_fly_speed", G_Fmt("{}", DEFAULT_GRAPPLE_SPEED).data(),
              CVAR_NOFLAGS);
  g_grapple_pull_speed =
      gi.cvar("g_grapple_pull_speed",
              G_Fmt("{}", DEFAULT_GRAPPLE_PULL_SPEED).data(), CVAR_NOFLAGS);
  g_grapple_damage = gi.cvar("g_grapple_damage", "10", CVAR_NOFLAGS);

  g_frag_messages = gi.cvar("g_frag_messages", "1", CVAR_NOFLAGS);
  g_ghost_min_play_time = gi.cvar("g_ghost_min_play_time", "60", CVAR_NOFLAGS);

  g_debug_monster_paths = gi.cvar("g_debug_monster_paths", "0", CVAR_NOFLAGS);
  g_debug_monster_kills = gi.cvar("g_debug_monster_kills", "0", CVAR_LATCH);

  bot_debug_follow_actor = gi.cvar("bot_debug_follow_actor", "0", CVAR_NOFLAGS);
  bot_debug_move_to_point =
      gi.cvar("bot_debug_move_to_point", "0", CVAR_NOFLAGS);

  // noset vars
  g_dedicated = gi.cvar("dedicated", "0", CVAR_NOSET);

  // latched vars
  g_cheats = gi.cvar("cheats",
#if defined(_DEBUG)
                     "1"
#else
                     "0"
#endif
                     ,
                     CVAR_SERVERINFO | CVAR_LATCH);
  gi.cvar("gamename", GAMEVERSION.c_str(), CVAR_SERVERINFO | CVAR_LATCH);

  skill = gi.cvar("skill", "3", CVAR_LATCH);
  maxentities =
      gi.cvar("maxentities", G_Fmt("{}", MAX_ENTITIES).data(), CVAR_LATCH);

  // change anytime vars
  fragLimit = gi.cvar("fraglimit", "0", CVAR_SERVERINFO);
  timeLimit = gi.cvar("timelimit", "0", CVAR_SERVERINFO);
  roundLimit = gi.cvar("roundlimit", "8", CVAR_SERVERINFO);
  roundTimeLimit = gi.cvar("roundtimelimit", "2", CVAR_SERVERINFO);
  captureLimit = gi.cvar("capturelimit", "8", CVAR_SERVERINFO);
  mercyLimit = gi.cvar("mercylimit", "0", CVAR_NOFLAGS);
  noPlayersTime = gi.cvar("noplayerstime", "10", CVAR_NOFLAGS);
  marathon = gi.cvar("marathon", "0", CVAR_SERVERINFO);
  g_marathon_timelimit = gi.cvar("g_marathon_timelimit", "0", CVAR_NOFLAGS);
  g_marathon_scorelimit = gi.cvar("g_marathon_scorelimit", "0", CVAR_NOFLAGS);

  g_ruleset = gi.cvar("g_ruleset", std::to_string(Ruleset::Quake2).c_str(),
                      CVAR_SERVERINFO);

  password = gi.cvar("password", "", CVAR_USERINFO);
  spectatorPassword = gi.cvar("spectator_password", "", CVAR_USERINFO);
  admin_password = gi.cvar("admin_password", "", CVAR_NOFLAGS);
  needPass = gi.cvar("needpass", "0", CVAR_SERVERINFO);
  filterBan = gi.cvar("filterban", "1", CVAR_NOFLAGS);
  G_LoadIPFilters();

  run_pitch = gi.cvar("run_pitch", "0.002", CVAR_NOFLAGS);
  run_roll = gi.cvar("run_roll", "0.005", CVAR_NOFLAGS);
  bob_up = gi.cvar("bob_up", "0.005", CVAR_NOFLAGS);
  bob_pitch = gi.cvar("bob_pitch", "0.002", CVAR_NOFLAGS);
  bob_roll = gi.cvar("bob_roll", "0.002", CVAR_NOFLAGS);

  flood_msgs = gi.cvar("flood_msgs", "4", CVAR_NOFLAGS);
  flood_persecond = gi.cvar("flood_persecond", "4", CVAR_NOFLAGS);
  flood_waitdelay = gi.cvar("flood_waitdelay", "10", CVAR_NOFLAGS);

  ai_allow_dm_spawn = gi.cvar("ai_allow_dm_spawn", "0", CVAR_NOFLAGS);
  ai_damage_scale = gi.cvar("ai_damage_scale", "1", CVAR_NOFLAGS);
  ai_model_scale = gi.cvar("ai_model_scale", "0", CVAR_NOFLAGS);
  ai_movement_disabled = gi.cvar("ai_movement_disabled", "0", CVAR_NOFLAGS);
  ai_widow_roof_spawn = gi.cvar("ai_widow_roof_spawn", "0", CVAR_NOFLAGS);

  bot_name_prefix = gi.cvar("bot_name_prefix", "B|", CVAR_NOFLAGS);
  g_allowAdmin = gi.cvar("g_allow_admin", "1", CVAR_NOFLAGS);
  g_allowCustomSkins = gi.cvar("g_allow_custom_skins", "1", CVAR_NOFLAGS);
  g_allowForfeit = gi.cvar("g_allow_forfeit", "1", CVAR_NOFLAGS);
  g_allow_duel_queue = gi.cvar("g_allow_duel_queue", "1", CVAR_NOFLAGS);
  g_allowMymap = gi.cvar("g_allow_mymap", "1", CVAR_NOFLAGS);
  g_allowSpecVote = gi.cvar("g_allow_spec_vote", "0", CVAR_NOFLAGS);
  g_allowTechs = gi.cvar("g_allow_techs", "auto", CVAR_NOFLAGS);
  g_allowVoteMidGame = gi.cvar("g_allow_vote_mid_game", "0", CVAR_NOFLAGS);
  g_allowVoting = gi.cvar("g_allow_voting", "1", CVAR_NOFLAGS);
  g_arenaSelfDmgArmor = gi.cvar("g_arena_self_dmg_armor", "0", CVAR_NOFLAGS);
  g_arenaStartingArmor = gi.cvar("g_arena_starting_armor", "200", CVAR_NOFLAGS);
  g_arenaStartingHealth =
      gi.cvar("g_arena_starting_health", "200", CVAR_NOFLAGS);
  g_autoScreenshotTool = gi.cvar("g_auto_screenshot_tool", "0", CVAR_NOFLAGS);
  g_coop_health_scaling = gi.cvar("g_coop_health_scaling", "0", CVAR_LATCH);
  g_damage_scale = gi.cvar("g_damage_scale", "1", CVAR_NOFLAGS);
  g_disable_player_collision =
      gi.cvar("g_disable_player_collision", "0", CVAR_NOFLAGS);
  match_startNoHumans = gi.cvar("match_start_no_humans", "1", CVAR_NOFLAGS);
  match_autoJoin = gi.cvar("match_auto_join", "1", CVAR_NOFLAGS);
  match_crosshairIDs = gi.cvar("match_crosshair_ids", "1", CVAR_NOFLAGS);
  warmup_doReadyUp = gi.cvar("warmup_do_ready_up", "0", CVAR_NOFLAGS);
  warmup_enabled = gi.cvar("warmup_enabled", "1", CVAR_NOFLAGS);
  g_dm_exec_level_cfg = gi.cvar("g_dm_exec_level_cfg", "0", CVAR_NOFLAGS);
  match_forceJoin = gi.cvar("match_force_join", "0", CVAR_NOFLAGS);
  match_doForceRespawn = gi.cvar("match_do_force_respawn", "1", CVAR_NOFLAGS);
  match_forceRespawnTime =
      gi.cvar("match_force_respawn_time", "2.4", CVAR_NOFLAGS);
  match_holdableAdrenaline =
      gi.cvar("match_holdable_adrenaline", "1", CVAR_NOFLAGS);
  match_instantItems = gi.cvar("match_instant_items", "1", CVAR_NOFLAGS);
  owner_intermissionShots =
      gi.cvar("owner_intermission_shots", "0", CVAR_NOFLAGS);
  match_itemsRespawnRate =
      gi.cvar("match_items_respawn_rate", "1.0", CVAR_NOFLAGS);
  g_fallingDamage = gi.cvar("g_falling_damage", "1", CVAR_NOFLAGS);
  g_selfDamage = gi.cvar("g_self_damage", "1", CVAR_NOFLAGS);
  match_doOvertime = gi.cvar("match_do_overtime", "120", CVAR_NOFLAGS);
  match_powerupDrops = gi.cvar("match_powerup_drops", "1", CVAR_NOFLAGS);
  match_powerupMinPlayerLock =
      gi.cvar("match_powerup_min_player_lock", "0", CVAR_NOFLAGS);
  match_playerRespawnMinDelay =
      gi.cvar("match_player_respawn_min_delay", "1", CVAR_NOFLAGS);
  match_playerRespawnMinDistance =
      gi.cvar("match_player_respawn_min_distance", "256", CVAR_NOFLAGS);
  match_playerRespawnMinDistanceDebug =
      gi.cvar("match_player_respawn_min_distance_debug", "0", CVAR_NOFLAGS);
  match_map_sameLevel = gi.cvar("match_map_same_level", "0", CVAR_NOFLAGS);
  match_allowSpawnPads = gi.cvar("match_allow_spawn_pads", "1", CVAR_NOFLAGS);
  match_allowTeleporterPads =
      gi.cvar("match_allow_teleporter_pads", "1", CVAR_NOFLAGS);
  match_timeoutLength = gi.cvar("match_timeout_length", "120", CVAR_NOFLAGS);
  match_weaponsStay = gi.cvar("match_weapons_stay", "0", CVAR_NOFLAGS);
  match_dropCmdFlags = gi.cvar("match_drop_cmd_flags", "7", CVAR_NOFLAGS);
  g_entityOverrideDir = gi.cvar("g_entity_override_dir", "maps", CVAR_NOFLAGS);
  g_entityOverrideLoad = gi.cvar("g_entity_override_load", "1", CVAR_NOFLAGS);
  g_entityOverrideSave = gi.cvar("g_entity_override_save", "0", CVAR_NOFLAGS);
  g_eyecam = gi.cvar("g_eyecam", "1", CVAR_NOFLAGS);
  g_fastDoors = gi.cvar("g_fast_doors", "1", CVAR_NOFLAGS);
  g_framesPerFrame = gi.cvar("g_frames_per_frame", "1", CVAR_NOFLAGS);
  g_friendlyFireScale = gi.cvar("g_friendly_fire_scale", "1.0", CVAR_NOFLAGS);
  g_inactivity = gi.cvar("g_inactivity", "120", CVAR_NOFLAGS);
  g_infiniteAmmo = gi.cvar("g_infinite_ammo", "0", CVAR_LATCH);
  g_instantWeaponSwitch = gi.cvar("g_instant_weapon_switch", "0", CVAR_LATCH);
  g_itemBobbing = gi.cvar("g_item_bobbing", "1", CVAR_NOFLAGS);
  g_knockbackScale = gi.cvar("g_knockback_scale", "0.8", CVAR_NOFLAGS);
  g_ladderSteps = gi.cvar("g_ladder_steps", "1", CVAR_NOFLAGS);
  g_lagCompensation = gi.cvar("g_lag_compensation", "1", CVAR_NOFLAGS);
  g_level_rulesets = gi.cvar("g_level_rulesets", "0", CVAR_NOFLAGS);
  match_maps_list = gi.cvar("match_maps_list", "", CVAR_NOFLAGS);
  match_maps_listShuffle =
      gi.cvar("match_maps_list_shuffle", "1", CVAR_NOFLAGS);
  g_mapspawn_no_bfg = gi.cvar("g_mapspawn_no_bfg", "0", CVAR_NOFLAGS);
  g_mapspawn_no_plasmabeam =
      gi.cvar("g_mapspawn_no_plasmabeam", "0", CVAR_NOFLAGS);
  match_lock = gi.cvar("match_lock", "0", CVAR_SERVERINFO);
  match_setup_active = gi.cvar("match_setup_active", "0", CVAR_NOFLAGS);
  match_setup_format = gi.cvar("match_setup_format", "regular", CVAR_NOFLAGS);
  match_setup_gametype = gi.cvar("match_setup_gametype", "ffa", CVAR_NOFLAGS);
  match_setup_modifier =
      gi.cvar("match_setup_modifier", "standard", CVAR_NOFLAGS);
  match_setup_maxplayers =
      gi.cvar("match_setup_maxplayers", "8", CVAR_NOFLAGS);
  match_setup_length = gi.cvar("match_setup_length", "standard", CVAR_NOFLAGS);
  match_setup_type = gi.cvar("match_setup_type", "standard", CVAR_NOFLAGS);
  match_setup_bestof = gi.cvar("match_setup_bestof", "bo1", CVAR_NOFLAGS);
  g_tourney_cfg = gi.cvar("g_tourney_cfg", "tourney.json", CVAR_NOFLAGS);
  g_matchstats = gi.cvar("g_matchstats", "0", CVAR_NOFLAGS);
  g_motd_filename = gi.cvar("g_motd_filename", "motd.txt", CVAR_NOFLAGS);
  g_mover_debug = gi.cvar("g_mover_debug", "0", CVAR_NOFLAGS);
  g_mover_speed_scale = gi.cvar("g_mover_speed_scale", "1.0f", CVAR_NOFLAGS);
  g_no_armor = gi.cvar("g_no_armor", "0", CVAR_NOFLAGS);
  g_no_health = gi.cvar("g_no_health", "0", CVAR_NOFLAGS);
  g_no_items = gi.cvar("g_no_items", "0", CVAR_NOFLAGS);
  g_no_mines = gi.cvar("g_no_mines", "0", CVAR_NOFLAGS);
  g_no_nukes = gi.cvar("g_no_nukes", "0", CVAR_NOFLAGS);
  g_no_powerups = gi.cvar("g_no_powerups", "0", CVAR_NOFLAGS);
  g_no_spheres = gi.cvar("g_no_spheres", "0", CVAR_NOFLAGS);
  g_quickWeaponSwitch = gi.cvar("g_quick_weapon_switch", "1", CVAR_LATCH);
  g_select_empty = gi.cvar("g_select_empty", "0", CVAR_ARCHIVE);
  g_showhelp = gi.cvar("g_showhelp", "1", CVAR_NOFLAGS);
  g_showmotd = gi.cvar("g_showmotd", "1", CVAR_NOFLAGS);
  g_start_items = gi.cvar("g_start_items", "", CVAR_NOFLAGS);
  g_starting_health = gi.cvar("g_starting_health", "100", CVAR_NOFLAGS);
  g_starting_health_bonus =
      gi.cvar("g_starting_health_bonus", "25", CVAR_NOFLAGS);
  g_starting_armor = gi.cvar("g_starting_armor", "0", CVAR_NOFLAGS);
  g_strict_saves = gi.cvar("g_strict_saves", "1", CVAR_NOFLAGS);
  g_teamplay_allow_team_pick =
      gi.cvar("g_teamplay_allow_team_pick", "0", CVAR_NOFLAGS);
  g_teamplay_armor_protect =
      gi.cvar("g_teamplay_armor_protect", "0", CVAR_NOFLAGS);
  g_teamplay_auto_balance =
      gi.cvar("g_teamplay_auto_balance", "1", CVAR_NOFLAGS);
  g_teamplay_force_balance =
      gi.cvar("g_teamplay_force_balance", "0", CVAR_NOFLAGS);
  g_teamplay_item_drop_notice =
      gi.cvar("g_teamplay_item_drop_notice", "1", CVAR_NOFLAGS);
  g_verbose = gi.cvar("g_verbose", "0", CVAR_NOFLAGS);
  static const std::string kDefaultVoteFlagsValue =
      std::to_string(Commands::kDefaultVoteFlags);
  g_vote_flags =
      gi.cvar("g_vote_flags", kDefaultVoteFlagsValue.c_str(), CVAR_NOFLAGS);
  g_vote_limit = gi.cvar("g_vote_limit", "3", CVAR_NOFLAGS);
  g_warmup_countdown = gi.cvar("g_warmup_countdown", "10", CVAR_NOFLAGS);
  g_warmup_ready_percentage =
      gi.cvar("g_warmup_ready_percentage", "0.51f", CVAR_NOFLAGS);
  g_weaponProjection = gi.cvar("g_weapon_projection", "0", CVAR_NOFLAGS);
  g_weapon_respawn_time = gi.cvar("g_weapon_respawn_time", "30", CVAR_NOFLAGS);

  MatchSetup_ApplyStartConfig();

  g_maps_pool_file = gi.cvar("g_maps_pool_file", "mapdb.json", CVAR_NOFLAGS);
  g_maps_cycle_file =
      gi.cvar("g_maps_cycle_file", "mapcycle.txt", CVAR_NOFLAGS);
  g_maps_selector = gi.cvar("g_maps_selector", "1", CVAR_NOFLAGS);
  g_maps_mymap = gi.cvar("g_maps_mymap", "1", CVAR_NOFLAGS);
  g_maps_mymap_queue_limit =
      gi.cvar("g_maps_mymap_queue_limit", "8", CVAR_NOFLAGS);
  g_maps_allow_custom_textures =
      gi.cvar("g_maps_allow_custom_textures", "1", CVAR_NOFLAGS);
  g_maps_allow_custom_sounds =
      gi.cvar("g_maps_allow_custom_sounds", "1", CVAR_NOFLAGS);

  g_statex_enabled = gi.cvar("g_statex_enabled", "1", CVAR_NOFLAGS);
  g_statex_humans_present =
      gi.cvar("g_statex_humans_present", "1", CVAR_NOFLAGS);
  g_statex_export_html = gi.cvar("g_statex_export_html", "1", CVAR_NOFLAGS);

  g_blueTeamName = gi.cvar("g_blue_team_name", "Team BLUE", CVAR_NOFLAGS);
  g_redTeamName = gi.cvar("g_red_team_name", "Team RED", CVAR_NOFLAGS);

  // items
  InitItems();

  // ruleset
  CheckRuleset();

  // initialize all entities for this game
  game.maxEntities = maxentities->integer;
  g_entities = (gentity_t *)gi.TagMalloc(
      game.maxEntities * sizeof(g_entities[0]), TAG_GAME);
  std::memset(static_cast<void *>(g_entities), 0,
              game.maxEntities * sizeof(g_entities[0]));
  globals.gentities = g_entities;
  globals.maxEntities = game.maxEntities;

  // initialize all clients for this game
  AllocateClientArray(maxclients->integer);

  level.levelStartTime = level.time;
  game.serverStartTime = time(nullptr);

  level.readyToExit = false;

  level.matchState = MatchState::Initial_Delay;
  level.matchStateTimer = 0_sec;
  level.matchStartRealTime = GetCurrentRealTimeMillis();
  level.warmupNoticeTime = level.time;

  level.locked.fill(false);

  level.weaponCount.fill(0);

  level.vote.cmd = nullptr;
  level.vote.arg = '\n';

  level.match.totalDeaths = 0;

  gt_teamplay = teamplay->modifiedCount;
  gt_ctf = ctf->modifiedCount;
  gt_g_gametype = g_gametype->modifiedCount;
  gt_teams_on = Teams();

  LoadMotd();

  InitMapSystem(host);

  LoadBanList();
  LoadAdminList();

  // initialise the heatmap system
  HM_Init();
}

//===================================================================

/*
==================
FindIntermissionPoint

This is also used for spectator spawns
==================
*/
void FindIntermissionPoint(void) {
  if (level.intermission.spot)
    return;

  gentity_t *ent = level.spawnSpots[SPAWN_SPOT_INTERMISSION];
  gentity_t *target = nullptr;
  Vector3 dir;
  bool is_landmark = false;

  if (!ent) {
    // fallback if no intermission spot set
    SelectSpawnPoint(nullptr, level.intermission.origin,
                     level.intermission.angles, false, is_landmark);
  } else {
    level.intermission.origin = ent->s.origin;

    // map-specific hacks
    if (!Q_strncasecmp(level.mapName.data(), "campgrounds", 11)) {
      const gvec3_t v = {-320, -96, 503};
      if (ent->s.origin == v)
        level.intermission.angles[PITCH] = -30;
    } else if (!Q_strncasecmp(level.mapName.data(), "rdm10", 5)) {
      const gvec3_t v = {-1256, -1672, -136};
      if (ent->s.origin == v)
        level.intermission.angles = {15, 135, 0};
    } else {
      level.intermission.angles = ent->s.angles;
      // gi.Com_PrintFmt("FindIntermissionPoint angles: {}\n",
      // level.intermission.angles);
    }

    // face toward target if angle is still unset
    if (ent->target && level.intermission.angles == gvec3_t{0, 0, 0}) {
      target = PickTarget(ent->target);
      if (target) {
        dir = (target->s.origin - ent->s.origin).normalized();
        AngleVectors(dir);
        level.intermission.angles = dir * 360.0f;
        // gi.Com_PrintFmt("FindIntermissionPoint angles: {}\n",
        // level.intermission.angles);
      }
    }
  }

  level.intermission.spot = true;
}

//===================================================================

static void ShutdownGame() {
  gi.Com_Print("==== ShutdownGame ====\n");

  FreeClientArray();

  gi.FreeTags(TAG_LEVEL);
  gi.FreeTags(TAG_GAME);
}

static void *G_GetExtension(const char *name) { return nullptr; }

const shadow_light_data_t *GetShadowLightData(int32_t entity_number);

GameTime FRAME_TIME_S;
GameTime FRAME_TIME_MS;

/*
=================
GetGameAPI

Returns a pointer to the structure with all entry points
and global variables
=================
*/
Q2GAME_API game_export_t *GetGameAPI(game_import_t *import) {
  gi = *import;

  InitServerLogging();

  FRAME_TIME_S = FRAME_TIME_MS = GameTime::from_ms(gi.frameTimeMs);

  globals.apiVersion = GAME_API_VERSION;
  globals.PreInit = PreInitGame;
  globals.Init = InitGame;
  globals.Shutdown = ShutdownGame;
  globals.SpawnEntities = SpawnEntities;

  globals.WriteGameJson = WriteGameJson;
  globals.ReadGameJson = ReadGameJson;
  globals.WriteLevelJson = WriteLevelJson;
  globals.ReadLevelJson = ReadLevelJson;
  globals.CanSave = CanSave;

  globals.Pmove = Pmove;

  globals.GetExtension = G_GetExtension;

  globals.ClientChooseSlot = ClientChooseSlot;
  globals.ClientThink = ClientThink;
  globals.ClientConnect = ClientConnect;
  globals.ClientUserinfoChanged = ClientUserinfoChanged;
  globals.ClientDisconnect = ClientDisconnect;
  globals.ClientBegin = ClientBegin;
  globals.ClientCommand = ClientCommand;

  globals.RunFrame = G_RunFrame;
  globals.PrepFrame = G_PrepFrame;

  globals.ServerCommand = ServerCommand;
  globals.Bot_SetWeapon = Bot_SetWeapon;
  globals.Bot_TriggerEntity = Bot_TriggerEntity;
  globals.Bot_GetItemID = Bot_GetItemID;
  globals.Bot_UseItem = Bot_UseItem;
  globals.Entity_ForceLookAtPoint = Entity_ForceLookAtPoint;
  globals.Bot_PickedUpItem = Bot_PickedUpItem;

  globals.Entity_IsVisibleToPlayer = Entity_IsVisibleToPlayer;
  globals.GetShadowLightData = GetShadowLightData;

  globals.gentitySize = sizeof(gentity_t);

  return &globals;
}

//======================================================================

/*
=================
ClientEndServerFrames
=================
*/
static void ClientEndServerFrames() {
  // calc the player views now that all pushing
  // and damage has been added
  for (auto ec : active_clients())
    ClientEndServerFrame(ec);
}

/*
===============
CreateTargetChangeLevel

Creates and returns a target_changelevel entity.
===============
*/
gentity_t *CreateTargetChangeLevel(std::string_view map) {
  if (map.empty())
    return nullptr;

  gentity_t *ent = Spawn();
  ent->className = "target_changelevel";

  // Write into the level buffer
  Q_strlcpy(level.nextMap.data(), map.data(), level.nextMap.size());

  // Copy into the entity's own buffer (avoids aliasing level.nextMap)
  Q_strlcpy(ent->map.data(), level.nextMap.data(), ent->map.size());

  return ent;
}

// =============================================================

/*
=================
CheckNeedPass
=================
*/
static void CheckNeedPass() {
  static uint32_t passwordModified = 0;
  static uint32_t spectatorPasswordModified = 0;

  // Only update if either password cvar was modified
  if (!Cvar_WasModified(password, passwordModified) &&
      !Cvar_WasModified(spectatorPassword, spectatorPasswordModified)) {
    return;
  }

  int need = 0;

  // Check main password
  if (*password->string && Q_strcasecmp(password->string, "none") != 0)
    need |= 1;

  // Check spectator password
  if (*spectatorPassword->string &&
      Q_strcasecmp(spectatorPassword->string, "none") != 0)
    need |= 2;

  gi.cvarSet("needPass", G_Fmt("{}", need).data());
}

/*
====================
QueueIntermission
====================
*/
void QueueIntermission(const char *msg, bool boo, bool reset) {
  const bool practice = g_practice && g_practice->integer;
  const bool marathonTransition =
      game.marathon.active && game.marathon.transitionPending;

  if (level.intermission.queued ||
      (!practice && level.matchState < MatchState::In_Progress))
    return;

  std::strncpy(level.intermission.victorMessage.data(), msg,
               level.intermission.victorMessage.size() - 1);
  level.intermission.victorMessage.back() = '\0'; // Ensure null-termination

  const char *reason = level.intermission.victorMessage[0]
                           ? level.intermission.victorMessage.data()
                           : "Unknown Reason";
  gi.Com_PrintFmt("MATCH END: {}\n", reason);

  if (!reset && !marathonTransition)
    Match_UpdateDuelRecords();

  const char *sound = boo ? "insane/insane4.wav" : "world/xian1.wav";
  gi.positionedSound(world->s.origin, world, CHAN_AUTO | CHAN_RELIABLE,
                     gi.soundIndex(sound), 1, ATTN_NONE, 0);

  if (reset) {
    Match_Reset();
    return;
  }

  int64_t now = GetCurrentRealTimeMillis();

  level.matchState = MatchState::Ended;
  level.matchStateTimer = 0_sec;
  level.matchEndRealTime = now;
  level.intermission.queued = level.time;

  for (auto ec : active_players())
    ec->client->sess.playEndRealTime = now;

  gi.configString(CS_CDTRACK, "0");
}

/*
============
Teams_CalcRankings

End game rankings
============
*/
void Teams_CalcRankings(std::array<uint32_t, MAX_CLIENTS> &playerRanks) {
  if (!Teams())
    return;

  // we're all winners.. or losers. whatever
  if (level.teamScores[static_cast<int>(Team::Red)] ==
      level.teamScores[static_cast<int>(Team::Blue)]) {
    playerRanks.fill(1);
    return;
  }

  Team winningTeam = (level.teamScores[static_cast<int>(Team::Red)] >
                      level.teamScores[static_cast<int>(Team::Blue)])
                         ? Team::Red
                         : Team::Blue;

  for (auto player : active_players())
    if (player->client->pers.spawned && ClientIsPlaying(player->client)) {
      int index = static_cast<int>(player->s.number) - 1;
      playerRanks[index] = player->client->sess.team == winningTeam ? 1 : 2;
    }
}

/*
=============
BeginIntermission

Initiates the intermission state and prepares for level transition.
=============
*/
extern void Gauntlet_MatchEnd_AdjustScores();
void BeginIntermission(gentity_t *targ) {
  if (level.intermission.time)
    return; // already triggered

  gentity_t *changeTarget = targ;
  if (!changeTarget || CharArrayIsBlank(changeTarget->map)) {
    if (!CharArrayHasText(level.mapName)) {
      gi.Com_ErrorFmt("{}: unable to resolve changelevel target because the "
                      "current map name is blank.",
                      __FUNCTION__);
      return;
    }

    const char *fallbackMap = level.mapName.data();
    if (changeTarget) {
      gi.Com_PrintFmt("{}: changelevel target missing map key. Falling back to "
                      "current map '{}'\n",
                      __FUNCTION__, fallbackMap);
      Q_strlcpy(changeTarget->map.data(), fallbackMap,
                changeTarget->map.size());
    } else {
      gi.Com_PrintFmt(
          "{}: missing changelevel target. Falling back to current map '{}'\n",
          __FUNCTION__, fallbackMap);
      changeTarget = CreateTargetChangeLevel(fallbackMap);
    }

    if (!changeTarget || CharArrayIsBlank(changeTarget->map)) {
      gi.Com_ErrorFmt(
          "{}: failed to establish a valid changelevel target for map '{}'.",
          __FUNCTION__, fallbackMap);
      return;
    }
  }

  targ = changeTarget;
  // Score adjustment (for duel, gauntlet, etc.)
  Gauntlet_MatchEnd_AdjustScores();

  game.autoSaved = false;
  level.intermission.time = level.time;
  ApplyQueuedTeamChanges(false);

  // Respawn any dead players (SP/Coop only)
  for (auto ec : active_players()) {
    if (ec->health <= 0 || ec->client->eliminated) {
      ec->health = 1;

      if (P_UseCoopInstancedItems()) {
        auto &cl = *ec->client;
        cl.pers.health = cl.pers.maxHealth = ec->maxHealth;
      }

      ClientRespawn(ec);
    }
  }

  level.intermission.serverFrame = gi.ServerFrame();

  level.changeMap.assign(CharArrayToStringView(targ->map));
  level.intermission.clear =
      targ->spawnFlags.has(SPAWNFLAG_CHANGELEVEL_CLEAR_INVENTORY);
  level.intermission.endOfUnit = false;
  level.intermission.fade =
      targ->spawnFlags.has(SPAWNFLAG_CHANGELEVEL_FADE_OUT);

  PlayerTrail_Destroy(nullptr);
  UpdateLevelEntry();

  const bool isEndOfUnit = std::strstr(level.changeMap.data(), "*") != nullptr;
  const bool isImmediateLeave =
      targ->spawnFlags.has(SPAWNFLAG_CHANGELEVEL_IMMEDIATE_LEAVE);

  if (isEndOfUnit) {
    level.intermission.endOfUnit = true;
    G_HudBlob_ClearEOUSection();

    // Coop: clear all keys across units
    if (coop->integer) {
      for (auto ec : active_clients()) {
        for (uint8_t i = 0; i < IT_TOTAL; ++i) {
          if (itemList[i].flags & IF_KEY)
            ec->client->pers.inventory[i] = 0;
        }
      }
    }

    // Broadcast achievement if defined
    if (!level.achievement.empty()) {
      gi.WriteByte(svc_achievement);
      gi.WriteString(level.achievement.data());
      gi.multicast(vec3_origin, MULTICAST_ALL, true);
    }

    // End-of-unit intermission message
    if (!targ->spawnFlags.has(SPAWNFLAG_CHANGELEVEL_NO_END_OF_UNIT)) {
      EndOfUnitMessage();
    }
  }

  // Immediate transition case (SP only)
  if (!deathmatch->integer && isImmediateLeave) {
    ReportMatchDetails(true);
    level.intermission.postIntermission = true;
    level.intermission.exit = true;
    return;
  }

  // SP with direct map change (non end-of-unit)
  if (!deathmatch->integer && !isEndOfUnit) {
    level.intermission.postIntermission = true;
    level.intermission.exit = true;
    return;
  }

  // Final match reporting before vote/menu/nextmap
  ReportMatchDetails(true);

  level.intermission.postIntermission = false;

  // Move all clients to intermission camera
  for (auto ec : active_clients()) {
    MoveClientToIntermission(ec);

    if (Teams()) {
      AnnouncerSound(ec, level.teamScores[static_cast<int>(Team::Red)] >
                                 level.teamScores[static_cast<int>(Team::Blue)]
                             ? "red_wins"
                             : "blue_wins");
    } else if (ClientIsPlaying(ec->client)) {
      AnnouncerSound(ec, ec->client->pers.currentRank == 0 ? "you_win"
                                                           : "you_lose");
    }
  }
}

/*
=============================
TakeIntermissionScreenshot
=============================
*/
static void TakeIntermissionScreenshot() {
  // Only valid in deathmatch with intermission shots enabled and human players
  // present
  if (!deathmatch->integer || !owner_intermissionShots->integer ||
      level.pop.num_playing_human_clients <= 0)
    return;

  // Build timestamp
  const std::tm lTime = LocalTimeNow();

  char timestamp[64];
  std::snprintf(timestamp, sizeof(timestamp), "%04d_%02d_%02d-%02d_%02d_%02d",
                1900 + lTime.tm_year, lTime.tm_mon + 1, lTime.tm_mday,
                lTime.tm_hour, lTime.tm_min, lTime.tm_sec);

  std::string filename;

  // Duel screenshots: show player vs player
  if (Game::Is(GameType::Duel)) {
    const bool hasP1 = level.sortedClients[0] >= 0;
    const bool hasP2 = level.sortedClients[1] >= 0;
    const gentity_t *e1 =
        hasP1 ? &g_entities[level.sortedClients[0] + 1] : nullptr;
    const gentity_t *e2 =
        hasP2 ? &g_entities[level.sortedClients[1] + 1] : nullptr;
    const char *n1 = (e1 && e1->client && e1->client->sess.netName[0])
                         ? e1->client->sess.netName
                         : "player1";
    const char *n2 = (e2 && e2->client && e2->client->sess.netName[0])
                         ? e2->client->sess.netName
                         : (hasP2 ? "player2" : "opponent");

    filename = G_Fmt("screenshot {}-vs-{}-{}-{}\n", n1, n2,
                     level.mapName.data(), timestamp);
  }
  // Other gametypes: gametype + POV name + map
  else {
    const gentity_t *ent = &g_entities[1];
    const gclient_t *followClient = ent->client->follow.target
                                        ? ent->client->follow.target->client
                                        : nullptr;
    const char *name = "player";
    GameType gameType = Game::GetCurrentType();

    if (followClient && followClient->sess.netName[0])
      name = followClient->sess.netName;
    else if (ent->client->sess.netName[0])
      name = ent->client->sess.netName;

    if (g_gametype)
      gameType = Game::NormalizeTypeValue(g_gametype->integer);

    filename =
        G_Fmt("screenshot {}-{}-{}-{}\n", GametypeIndexToString(gameType), name,
              level.mapName.data(), timestamp);
  }

  // Execute
  gi.Com_PrintFmt("[INTERMISSION]: Taking screenshot '{}'", filename.c_str());
  gi.AddCommandString(filename.c_str());
}

/*
=================
ExitLevel

Handles transitioning to the next map or endgame sequence,
depending on mode and configured changeMap.
=================
*/
extern void Gauntlet_RemoveLoser();
extern void Duel_RemoveLoser();
void ExitLevel(bool forceImmediate) {
  // Ensure a valid map transition is set
  if (level.changeMap.empty()) {
    gi.Com_Error("Got null changeMap when trying to exit level. Was a "
                 "trigger_changelevel configured correctly?");
    return;
  }

  // N64 fade delay before actual exit
  if (level.intermission.fade) {
    level.intermission.fadeTime = level.time + 1.3_sec;
    level.intermission.fading = true;
    return;
  }

  ClientEndServerFrames();
  TakeIntermissionScreenshot();

  // Cache intermission flags that need to persist through the struct reset
  const bool shouldClearInventory = level.intermission.clear;
  const bool shouldHandleEndOfUnit = level.intermission.endOfUnit;

  // Reset intermission state
  level.intermission = {};

  if (deathmatch->integer) {
    const bool tournamentMode =
        match_setup_type && match_setup_type->string &&
        Q_strcasecmp(match_setup_type->string, "tournament") == 0;
    const bool preserveDuelists =
        tournamentMode && game.tournament.active &&
        !game.tournament.seriesComplete && game.tournament.winTarget > 1;

    // In 1v1 modes, rotate out the loser so the queue advances.
    if (!preserveDuelists) {
      if (Game::Is(GameType::Gauntlet))
        Gauntlet_RemoveLoser();
      else if (Game::Is(GameType::Duel))
        Duel_RemoveLoser();
    }

    // In Red Rover, shuffle teams if only one team has players
    if (Game::Is(GameType::RedRover) && level.pop.num_playing_clients > 1 &&
        (!level.pop.num_playing_red || !level.pop.num_playing_blue))
      Commands::TeamSkillShuffle();

    // Do not proceed further in DM - map voting or shuffle controls transition
    if (!forceImmediate)
      return;
  }

  // Singleplayer or coop logic
  if (shouldClearInventory) {
    for (auto ec : active_clients()) {
      auto &cl = *ec->client;

      // Preserve userinfo across the wipe
      char userInfo[MAX_INFO_STRING];
      Q_strlcpy(userInfo, cl.pers.userInfo, sizeof(userInfo));

      cl.pers = {};
      cl.resp.coopRespawn = {};
      ec->health = 0;

      Q_strlcpy(cl.pers.userInfo, userInfo, sizeof(cl.pers.userInfo));
      Q_strlcpy(cl.resp.coopRespawn.userInfo, userInfo,
                sizeof(cl.resp.coopRespawn.userInfo));
    }
  }

  if (shouldHandleEndOfUnit) {
    game.levelEntries = {};

    // Restore lives to all players in coop
    if (g_coop_enable_lives->integer) {
      for (auto ec : active_clients()) {
        ec->client->pers.lives = g_coop_num_lives->integer + 1;
        ec->client->pers.limitedLivesStash = ec->client->pers.lives;
        ec->client->pers.limitedLivesPersist = false;
      }
    }
  }

  // Handle endgame condition
  auto IsEndGameMap = [](const char *map) -> bool {
    size_t offset = (map[0] == '*') ? 1 : 0;
    size_t len = strlen(map);
    return len > offset + 6 && !Q_strncasecmp(map + offset, "victor", 6) &&
           !Q_strncasecmp(map + len - 4, ".pcx", 4);
  };

  if (IsEndGameMap(level.changeMap.data())) {
    const char *map =
        level.changeMap.data() + (level.changeMap[0] == '*' ? 1 : 0);
    gi.AddCommandString(G_Fmt("endgame \"{}\"\n", map).data());
  } else {
    gi.AddCommandString(G_Fmt("gamemap \"{}\"\n", level.changeMap).data());
  }

  level.changeMap.clear();
}

/*
=================
PreExitLevel

Handles end-of-match vote and map transition sequence.
=================
*/
void MapSelectorBegin();
void MapSelectorFinalize();

static void PreExitLevel() {
  auto &ms = level.mapSelector;

  // Exit immediately in SP or coop
  if (!deathmatch->integer) {
    ExitLevel();
    return;
  }

  // Skip vote system if play queue is active
  if (!game.mapSystem.playQueue.empty()) {
    ExitLevel(true);
    return;
  }

  if (level.intermission.postIntermissionTime == 0_sec) {
    if (ms.forceExit) {
      level.intermission.postIntermissionTime = level.time;
      return;
    }

    // Run vote sequence once
    if (ms.voteStartTime == 0_sec) {
      MapSelectorBegin(); // sets voteStartTime internally
      return;
    }

    // Wait for voting period to complete
    if (level.time < ms.voteStartTime + MAP_SELECTOR_DURATION)
      return;

    // Finalize vote once after voting ends
    MapSelectorFinalize();
    level.intermission.postIntermissionTime = level.time;
    return;
  }

  // Delay briefly before actual level exit
  if (level.time < level.intermission.postIntermissionTime + 2_sec)
    return;

  ExitLevel(true);
}
/*
=============
CheckPowerupsDisabled
=============
*/
static int powerupMinplayersModificationCount = -1;
static int powerupNumPlayersCheck = -1;

static void CheckPowerupsDisabled() {
  bool docheck = false;

  if (powerupMinplayersModificationCount !=
      match_powerupMinPlayerLock->integer) {
    powerupMinplayersModificationCount = match_powerupMinPlayerLock->integer;
    docheck = true;
  }

  if (powerupNumPlayersCheck != level.pop.num_playing_clients) {
    powerupNumPlayersCheck = level.pop.num_playing_clients;
    docheck = true;
  }

  if (!docheck)
    return;

  bool disable =
      match_powerupMinPlayerLock->integer > 0 &&
      (level.pop.num_playing_clients < match_powerupMinPlayerLock->integer);
  gentity_t *ent = nullptr;
  size_t i;
  for (ent = g_entities + 1, i = 1; i < globals.numEntities; i++, ent++) {
    if (!ent->inUse || !ent->item)
      continue;

    if (!(ent->item->flags & IF_POWERUP))
      continue;
    if (g_quadhog->integer && ent->item->id == IT_POWERUP_QUAD)
      return;

    if (disable) {
      ent->s.renderFX |= (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE);
      ent->s.effects |= EF_COLOR_SHELL;
    } else {
      ent->s.renderFX &= ~(RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE);
      ent->s.effects &= ~EF_COLOR_SHELL;
    }
  }
}

/*
=============
CheckMinMaxPlayers
=============
*/
static int minplayers_mod_count = -1;
static int maxplayers_mod_count = -1;

static void CheckMinMaxPlayers() {

  if (!deathmatch->integer)
    return;

  if (minplayers_mod_count == minplayers->modifiedCount &&
      maxplayers_mod_count == maxplayers->modifiedCount)
    return;

  // set min/maxplayer limits
  if (minplayers->integer < 2)
    gi.cvarSet("minplayers", "2");
  else if (minplayers->integer > maxclients->integer)
    gi.cvarSet("minplayers", maxclients->string);
  if (maxplayers->integer < 0)
    gi.cvarSet("maxplayers", maxclients->string);
  if (maxplayers->integer > maxclients->integer)
    gi.cvarSet("maxplayers", maxclients->string);
  else if (maxplayers->integer < minplayers->integer)
    gi.cvarSet("maxplayers", minplayers->string);

  minplayers_mod_count = minplayers->modifiedCount;
  maxplayers_mod_count = maxplayers->modifiedCount;
}

static void CheckCvars() {
  if (Cvar_WasModified(g_gravity, game.gravity_modCount))
    level.gravity = g_gravity->value;

  CheckMinMaxPlayers();
}

static bool G_AnyDeadPlayersWithoutLives() {
  for (auto player : active_clients())
    if (player->health <= 0 &&
        (!player->client->pers.lives || player->client->eliminated))
      return true;

  return false;
}

static void HostAutoScreenshotsRun() {
  // gi.Com_Print("==== HostAutoScreenshotsRun ====\n");
  if (!g_autoScreenshotTool->integer)
    return;

  // gi.Com_Print("HostAutoScreenshotsRun: Auto screenshots enabled\n");

  if (!host || !host->client) // || !host->client->sess.initialised)
    return;

  // let everything initialize first
  if (level.time < 300_ms)
    return;

  // gi.Com_Print("HostAutoScreenshotsRun: Starting auto screenshots\n");

  if (!level.autoScreenshotTool_initialised) {
    host->client->initialMenu.shown = true;
    host->client->showScores = false;
    host->client->showInventory = false;
    CloseActiveMenu(host);
    level.autoScreenshotTool_initialised = true;
  }

  // time to take screenshot
  if (level.autoScreenshotTool_delayTime) {

    if (level.time >= level.autoScreenshotTool_delayTime) {
      host->client->initialMenu.shown = true;
      host->client->showScores = false;
      host->client->showInventory = false;
      CloseActiveMenu(host);
      // gi.Com_PrintFmt("HostAutoScreenshotsRun: Taking screenshots for {}\n",
      // host->client->sess.netName);

      std::string_view levelName(
          level.mapName.data(),
          strnlen(level.mapName.data(), sizeof(level.mapName)));

      // sanitize level name
      if (levelName.find_first_of("/\\") != std::string_view::npos) {
        gi.Com_Print("HostAutoScreenshotsRun: Invalid map name for screenshot, "
                     "skipping.\n");
        return;
      }
      gi.AddCommandString(G_Fmt("screenshotpng {}_{}\n", levelName.data(),
                                level.autoScreenshotTool_index)
                              .data());
      level.autoScreenshotTool_delayTime = 0_sec;
      level.autoScreenshotTool_index++;
    } else {
      // gi.Com_PrintFmt("HostAutoScreenshotsRun: Waiting for next screenshot in
      // {} seconds\n", 	(level.autoScreenshotTool_delayTime -
      // level.time).seconds<int>());
      return; // wait for next screenshot
    }
  }

  switch (level.autoScreenshotTool_index) {
  case 0:
    // gi.Com_Print("HostAutoScreenshotsRun: Moving host to intermission\n");
    MoveClientToIntermission(host);
    host->client->initialMenu.shown = true;
    host->client->showScores = false;
    host->client->showInventory = false;
    CloseActiveMenu(host);
    level.autoScreenshotTool_delayTime = level.time + 300_ms;
    break;
  case 1:
  case 2:
  case 3:
  case 4:
  case 5:
    host->client->initialMenu.shown = true;
    host->client->showScores = false;
    host->client->showInventory = false;
    CloseActiveMenu(host);
    if (level.spawnSpots[level.autoScreenshotTool_index]) {
      TeleportPlayer(
          host, level.spawnSpots[level.autoScreenshotTool_index]->s.origin,
          level.spawnSpots[level.autoScreenshotTool_index]->s.angles);
      level.autoScreenshotTool_delayTime = level.time + 300_ms;
    } else {
      Match_End();
      level.intermission.time = level.time + 30_sec;
      ExitLevel(true);
    }
    break;
  case 6:
    Match_End();
    level.intermission.time = level.time + 30_sec;
    ExitLevel(true);
    break;
  }
}

/*
================
G_RunFrame

Advances the world by 0.1 seconds
================
*/
extern void AnnounceCountdown(int t, GameTime &checkRef);
extern void CheckVote(void);
extern void CheckDMEndFrame();

/*
=================
TimeoutEnd

Clears the timeout state and notifies players that the match has resumed.
=================
*/
static void TimeoutEnd() {
  gentity_t *owner = level.timeoutOwner;

  level.timeoutActive = 0_ms;
  level.timeoutOwner = nullptr;
  game.tournament.autoTimeoutActive = false;

  if (owner && owner->client) {
    gi.LocBroadcast_Print(PRINT_HIGH, "$g_sgame_auto_6171c4a72be2",
                          G_ColorResetAfter(owner->client->sess.netName).c_str());
  } else {
    gi.LocBroadcast_Print(PRINT_HIGH, "$g_sgame_auto_5b8728eeddd3");
  }

  G_LogEvent("MATCH TIMEOUT ENDED");
}

/*
=================
G_RunFrame_

Main game frame logic - called every tick.
Handles timeouts, intermission, entity updates, and respawns.
=================
*/
static inline void G_RunFrame_(bool main_loop) {
  level.inFrame = true;

  // --- Timeout Handling ---
  if (level.timeoutActive > 0_ms && level.timeoutOwner) {
    int tick = level.timeoutActive.seconds<int>() + 1;
    AnnounceCountdown(tick, level.countdownTimerCheck);

    if (game.tournament.autoTimeoutActive && Tournament_IsActive() &&
        level.timeoutOwner == world && Tournament_AllParticipantsConnected()) {
      level.timeoutActive = std::min(level.timeoutActive, 1_sec);
      game.tournament.autoTimeoutActive = false;
    }

    level.timeoutActive -= FRAME_TIME_MS;
    if (level.timeoutActive <= 0_ms) {
      TimeoutEnd();
    }

    ClientEndServerFrames();
    level.inFrame = false;
    return;
  }

  // --- Global Updates ---
  GT_Changes();            // track gametype changes
  CheckVote();             // cancel vote if expired
  CheckCvars();            // check for updated cvars
  CheckPowerupsDisabled(); // disable unwanted powerups
  CheckRuleset();          // ruleset enforcement
  Bot_UpdateDebug();       // debug AI states
  {
    uint32_t hud_flags = 0;
    if ((g_instaGib && g_instaGib->integer) ||
        (g_nadeFest && g_nadeFest->integer)) {
      hud_flags |= HUD_FLAG_MINHUD;
    }
    G_HudBlob_SetFlags(hud_flags);
  }
  if (!level.intermission.endOfUnit)
    G_HudBlob_ClearEOUSection();

  // --- Gravity Lotto Timer ---
  if (g_gravity_lotto && g_gravity_lotto->integer &&
      level.matchState == MatchState::In_Progress) {
    if (level.gravityLottoNextChange == 0_ms) {
      // Initialize timer on first check
      level.gravityLottoNextChange = level.time + 2_min;
    } else if (level.time >= level.gravityLottoNextChange) {
      ApplyGravityLotto();
      level.gravityLottoNextChange = level.time + 2_min;
    }
  }

  level.time += FRAME_TIME_MS;

  // --- Intermission Fade ---
  if (!deathmatch->integer && level.intermission.fading) {
    if (level.intermission.fadeTime > level.time) {
      float alpha = std::clamp(
          1.0f - (level.intermission.fadeTime - level.time - 300_ms).seconds(),
          0.0f, 1.0f);
      for (auto player : active_clients())
        player->client->ps.screenBlend = {0, 0, 0, alpha};
    } else {
      level.intermission.fade = false;
      level.intermission.fading = false;
      ExitLevel();
    }
    level.inFrame = false;
    return;
  }

  // --- Intermission Transitions ---
  if (level.intermission.postIntermission) {
    PreExitLevel();
    ClientEndServerFrames();
    level.inFrame = false;
    return;
  }
  if (level.intermission.exit) {
    if (!level.intermission.postIntermission)
      PreExitLevel();

    level.inFrame = false;
    return;
  }

  // --- Campaign Restart ---
  if (!deathmatch->integer) {
    if (level.campaign.coopLevelRestartTime > 0_ms &&
        level.time > level.campaign.coopLevelRestartTime) {
      level.campaign.coopLevelRestartTime = 0_ms;
      ClientEndServerFrames();
      gi.AddCommandString("restart_level\n");
      level.inFrame = false;
      return;
    }

    // --- Coop Respawn State Updates ---
    if (CooperativeModeOn() &&
        (g_coop_enable_lives->integer || g_coop_squad_respawn->integer)) {
      bool anyDeadNoLives =
          g_coop_enable_lives->integer && G_AnyDeadPlayersWithoutLives();
      for (auto player : active_clients()) {
        auto &cl = *player->client;
        if (cl.respawnMaxTime >= level.time) {
          cl.coopRespawnState = CoopRespawn::Waiting;
        } else if (g_coop_enable_lives->integer && player->health <= 0 &&
                   cl.pers.lives == 0) {
          cl.coopRespawnState = CoopRespawn::NoLives;
        } else if (anyDeadNoLives) {
          cl.coopRespawnState = CoopRespawn::NoLives;
        } else {
          cl.coopRespawnState = CoopRespawn::None;
        }
      }
    }
  }

  // --- Entity Loop ---
  gentity_t *ent = world;
  for (size_t i = 0; i < globals.numEntities; ++i, ++ent) {
    if (!ent->inUse) {
      if (i >= 1 && i < 1 + static_cast<size_t>(game.maxClients) &&
          ent->timeStamp && level.time >= ent->timeStamp) {
        int32_t playernum = static_cast<int32_t>(i - 1);
        gi.configString(CS_PLAYERSKINS + playernum, "");
        ent->timeStamp = 0_ms;
      }
      continue;
    }

    level.currentEntity = ent;

    if (!(ent->s.renderFX & RF_BEAM))
      ent->s.oldOrigin = ent->s.origin;

    // Update ground entity if necessary
    if (ent->groundEntity &&
        ent->groundEntity->linkCount != ent->groundEntity_linkCount) {
      contents_t mask = G_GetClipMask(ent);

      if (!(ent->flags & (FL_SWIM | FL_FLY)) && (ent->svFlags & SVF_MONSTER)) {
        ent->groundEntity = nullptr;
        M_CheckGround(ent, mask);
      } else {
        trace_t tr = gi.trace(ent->s.origin, ent->mins, ent->maxs,
                              ent->s.origin + ent->gravityVector, ent, mask);

        if (tr.startSolid || tr.allSolid || tr.ent != ent->groundEntity)
          ent->groundEntity = nullptr;
        else
          ent->groundEntity_linkCount = ent->groundEntity->linkCount;
      }
    }

    // update projectile powerup shells
    if (ent->clipMask & MASK_PROJECTILE) {
      if (ent->owner && ent->owner->inUse && ent->owner->client &&
          ent->owner->client->pers.spawned) {
        ent->s.renderFX &= ~(RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE);
        ent->s.effects &= ~EF_COLOR_SHELL;

        if (ent->owner->client->PowerupTimer(PowerupTimer::QuadDamage) >
            level.time) {
          ent->s.renderFX |= RF_SHELL_BLUE;
          ent->s.effects |= EF_COLOR_SHELL;
        }
        if (ent->owner->client->PowerupTimer(PowerupTimer::DoubleDamage) >
            level.time) {
          ent->s.renderFX |= RF_SHELL_DOUBLE;
          ent->s.effects |= EF_COLOR_SHELL;
        }
        if (ent->owner->client->PowerupTimer(PowerupTimer::Invisibility) >
            level.time) {
          if (ent->owner->client->invisibility_fade_time <= level.time)
            ent->s.alpha = 0.05f;
          else {
            float x = (ent->owner->client->invisibility_fade_time - level.time)
                          .seconds() /
                      INVISIBILITY_TIME.seconds();
            ent->s.alpha = std::clamp(x, 0.0125f, 0.2f);
          }
        }
      } else {
        ent->s.renderFX &= ~(RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE);
        ent->s.effects &= ~EF_COLOR_SHELL;
      }
    }

    Entity_UpdateState(ent);

    if (i >= 1 && i < 1 + static_cast<size_t>(game.maxClients)) {
      ClientBeginServerFrame(ent);
      continue;
    }

    G_RunEntity(ent);
  }

  // --- Check for Match End / DM Logic ---
  CheckDMEndFrame();
  CheckNeedPass();

  // --- Reset coopRespawnState if all players are now alive ---
  if (CooperativeModeOn() &&
      (g_coop_enable_lives->integer || g_coop_squad_respawn->integer)) {
    bool allAlive = true;
    for (auto player : active_clients()) {
      if (player->health <= 0) {
        allAlive = false;
        break;
      }
    }
    if (allAlive) {
      for (auto player : active_clients())
        player->client->coopRespawnState = CoopRespawn::None;
    }
  }

  // --- Finalize Frame ---
  ClientEndServerFrames();
  HostAutoScreenshotsRun();

  // --- Heatmap thinking ---
  HM_Think();

  // --- Entry timer tracking ---
  if (level.entry && !level.intermission.time && g_entities[1].inUse &&
      g_entities[1].client->pers.connected) {
    level.entry->time += FRAME_TIME_S;
  }

  // --- Process monster pain ---
  size_t total = std::min(static_cast<size_t>(MAX_ENTITIES),
                          static_cast<size_t>(globals.numEntities));
  for (size_t i = 0; i < total; ++i) {
    gentity_t *e = &g_entities[i];
    if (!e->inUse || !(e->svFlags & SVF_MONSTER))
      continue;

    M_ProcessPain(e);
  }

  level.inFrame = false;
}

static inline bool G_AnyClientsSpawned() {
  for (auto player : active_clients())
    if (player->client && player->client->pers.spawned)
      return true;

  return false;
}

static inline bool G_AnyClientsConnected() {
  for (size_t i = 0; i < game.maxClients; ++i)
    if (game.clients[i].pers.connected)
      return true;

  return false;
}

void G_RunFrame(bool main_loop) {
  if (main_loop && !G_AnyClientsConnected())
    return;

  for (size_t i = 0; i < g_framesPerFrame->integer; i++)
    G_RunFrame_(main_loop);

  // match details.. only bother if there's at least 1 player in-game
  // and not already end of game
  if (G_AnyClientsSpawned() && !level.intermission.time) {
    constexpr GameTime report_time = 45_sec;

    if (level.time - level.campaign.next_match_report > report_time) {
      level.campaign.next_match_report = level.time + report_time;
      ReportMatchDetails(false);
    }
  }
}

/*
================
G_PrepFrame

This has to be done before the world logic, because
player processing happens outside RunFrame
================
*/
void G_PrepFrame() {
  for (size_t i = 0; i < globals.numEntities; i++)
    g_entities[i].s.event = EV_NONE;

  for (auto player : active_clients())
    player->client->ps.stats[STAT_HIT_MARKER] = 0;

  globals.serverFlags &= ~SERVER_FLAG_INTERMISSION;

  if (level.intermission.time) {
    globals.serverFlags |= SERVER_FLAG_INTERMISSION;
  }
}

