// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

// g_local.h -- local definitions for game module
#pragma once

class Menu;
#include "../bgame/bg_local.hpp"
#include "../bgame/map_validation.hpp"
#include "../bgame/string_compat.hpp"
#include "../bgame/version.hpp"
#include <algorithm>
#include <array>
#include <bitset> // for bitset
#include <cctype>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional> // for AutoSelectNextMap()
#include <random>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

struct local_game_import_t;
extern local_game_import_t gi;

struct gclient_t;

// the "gameversion" client command will print this plus compile date
const std::string GAMEVERSION = "baseq2";

//==================================================================

#ifndef M_PI
constexpr float M_PI = 3.14159265358979323846f;
#endif
#ifndef M_TWOPI
constexpr float M_TWOPI = 6.28318530717958647692f;
#endif

constexpr unsigned MAX_REWARDSTACK = 10;

constexpr const int32_t GIB_HEALTH = -40;

constexpr const int32_t AMMO_INFINITE = 1000;

constexpr size_t MAX_CLIENTS_KEX = 32; // absolute limit

enum class Weapon : uint8_t {
  None,

  GrapplingHook,
  Blaster,
  Chainfist,
  Shotgun,
  SuperShotgun,
  Machinegun,
  ETFRifle,
  Chaingun,
  HandGrenades,
  Trap,
  TeslaMine,
  GrenadeLauncher,
  ProxLauncher,
  RocketLauncher,
  HyperBlaster,
  IonRipper,
  PlasmaGun,
  PlasmaBeam,
  Thunderbolt,
  Railgun,
  Phalanx,
  BFG10K,
  Disruptor,

  Total
};

const std::array<std::string, static_cast<uint8_t>(Weapon::Total)>
    weaponAbbreviations = {
        "NONE", // Weapon::None
        "GP",   // Weapon::GrapplingHook
        "BL",   // Weapon::Blaster
        "CF",   // Weapon::Chainfist
        "SG",   // Weapon::Shotgun
        "SSG",  // Weapon::SuperShotgun
        "MG",   // Weapon::Machinegun
        "ETF",  // Weapon::ETFRifle
        "CG",   // Weapon::Chaingun
        "HG",   // Weapon::HandGrenades
        "TP",   // Weapon::Trap
        "TM",   // Weapon::TeslaMine
        "GL",   // Weapon::GrenadeLauncher
        "PL",   // Weapon::ProxLauncher
        "RL",   // Weapon::RocketLauncher
        "HB",   // Weapon::HyperBlaster
        "IR",   // Weapon::IonRipper
        "PG",   // Weapon::PlasmaGun
        "PB",   // Weapon::PlasmaBeam
        "TB",   // Weapon::Thunderbolt
        "RG",   // Weapon::Railgun
        "PX",   // Weapon::Phalanx
        "BFG",  // Weapon::BFG10K
        "DTR"   // Weapon::Disruptor
};

// gameplay rulesets
class Ruleset {
public:
  enum Value : uint8_t { None, Quake1, Quake2, Quake3Arena, RS_NUM_RULESETS };

  constexpr Ruleset() : value(None) {}
  constexpr Ruleset(Value v) : value(v) {}
  constexpr explicit Ruleset(uint8_t v) : value(static_cast<Value>(v)) {}
  constexpr explicit Ruleset(size_t v) : value(static_cast<Value>(v)) {}
  constexpr explicit Ruleset(int v) : value(static_cast<Value>(v)) {}

  static constexpr size_t Count() {
    return static_cast<size_t>(RS_NUM_RULESETS);
  }

  constexpr Value get() const { return value; }

  constexpr operator Value() const { return value; }

private:
  Value value;
};
#define RS(x) game.ruleset == Ruleset::x
#define notRS(x) game.ruleset != Ruleset::x

// Quake 1 ruleset uses air acceleration; other rulesets keep legacy behavior.
constexpr int32_t kQuake1AirAccel = 10;
constexpr int32_t GetRulesetAirAccel(Ruleset ruleset) {
  return ruleset == Ruleset::Quake1 ? kQuake1AirAccel : 0;
}

constexpr size_t NUM_ALIASES = 4;
constexpr std::array<std::array<std::string_view, NUM_ALIASES>,
                     Ruleset::Count()>
    rs_short_name = {{
        {"", "", "", ""},                     // None
        {"q1", "quake1", "qw", "slipgate"},   // Quake1
        {"q2", "quake2", "q2re", "stroyent"}, // Quake2
        {"q3", "quake3", "q3a", "vadrigar"},  // Quake3Arena
    }};

constexpr std::array<const char *, Ruleset::Count()> rs_long_name = {
    "",
    "SLIPGATE",
    "STROYENT",
    "VADRIGAR",
};

const std::vector<std::string> stock_maps = {
    // constexpr const char *stock_maps[] = {
    "badlands",
    "base1",
    "base2",
    "base3",
    "base64",
    "biggun",
    "boss1",
    "boss2",
    "bunk1",
    "city1",
    "city2",
    "city3",
    "city64",
    "command",
    "cool1",
    "e3/bunk_e3",
    "e3/fact_e3",
    "e3/jail_e3",
    "e3/jail4_e3",
    "e3/lab_e3",
    "e3/mine_e3",
    "e3/space_e3",
    "e3/ware1a_e3",
    "e3/ware2_e3",
    "e3/waste_e3",
    "ec/base_ec",
    "ec/base3_ec",
    "ec/command_ec",
    "ec/factx_ec",
    "ec/jail_ec",
    "ec/kmdm3_ec",
    "ec/mine1_ec",
    "ec/power_ec",
    "ec/space_ec",
    "ec/waste_ec",
    "fact1",
    "fact2",
    "fact3",
    "hangar1",
    "hangar2",
    "industry",
    "jail1",
    "jail2",
    "jail3",
    "jail4",
    "jail5",
    "lab",
    "mgdm1",
    "mgu1m1",
    "mgu1m2",
    "mgu1m3",
    "mgu1m4",
    "mgu1m5",
    "mgu1trial",
    "mgu2m1",
    "mgu2m2",
    "mgu2m3",
    "mgu3m1",
    "mgu3m2",
    "mgu3m3",
    "mgu3m4",
    "mgu3secret",
    "mgu4m1",
    "mgu4m2",
    "mgu4m3",
    "mgu4trial",
    "mgu5m1",
    "mgu5m2",
    "mgu5m3",
    "mgu5trial",
    "mgu6m1",
    "mgu6m2",
    "mgu6m3",
    "mgu6trial",
    "mguboss",
    "mguhub",
    "mine1",
    "mine2",
    "mine3",
    "mine4",
    "mintro",
    "ndctf0",
    "old/baseold",
    "old/city2_4",
    "old/fact1",
    "old/fact2",
    "old/fact3",
    "old/facthub",
    "old/hangarold",
    "old/kmdm3",
    "old/pjtrain1",
    "old/ware1",
    "old/xcommand5",
    "outbase",
    "power1",
    "power2",
    "q2ctf1",
    "q2ctf2",
    "q2ctf3",
    "q2ctf4",
    "q2ctf5",
    "q2dm1",
    "q2dm2",
    "q2dm3",
    "q2dm4",
    "q2dm5",
    "q2dm6",
    "q2dm7",
    "q2dm8",
    "q2kctf1",
    "q2kctf2",
    "q64/bio",
    "q64/cargo",
    "q64/comm",
    "q64/command",
    "q64/complex",
    "q64/conduits",
    "q64/core",
    "q64/dm1",
    "q64/dm10",
    "q64/dm2",
    "q64/dm3",
    "q64/dm4",
    "q64/dm5",
    "q64/dm6",
    "q64/dm7",
    "q64/dm8",
    "q64/dm9",
    "q64/geo-stat",
    "q64/intel",
    "q64/jail",
    "q64/lab",
    "q64/mines",
    "q64/orbit",
    "q64/organic",
    "q64/outpost",
    "q64/process",
    "q64/rtest",
    "q64/ship",
    "q64/station",
    "q64/storage",
    "rammo1",
    "rammo2",
    "rbase1",
    "rbase2",
    "rboss",
    "rdm1",
    "rdm10",
    "rdm11",
    "rdm12",
    "rdm13",
    "rdm14",
    "rdm2",
    "rdm3",
    "rdm4",
    "rdm5",
    "rdm6",
    "rdm7",
    "rdm8",
    "rdm9",
    "refinery",
    "rhangar1",
    "rhangar2",
    "rlava1",
    "rlava2",
    "rmine1",
    "rmine2",
    "rsewer1",
    "rsewer2",
    "rware1",
    "rware2",
    "security",
    "sewer64",
    "space",
    "strike",
    "test/base1_flashlight",
    "test/gekk",
    "test/mals_barrier_test",
    "test/mals_box",
    "test/mals_ladder_test",
    "test/mals_locked_door_test",
    "test/paril_health_relay",
    "test/paril_ladder",
    "test/paril_poi",
    "test/paril_scaled_monsters",
    "test/paril_soundstage",
    "test/paril_steps",
    "test/paril_waterlight",
    "test/skysort",
    "test/spbox",
    "test/spbox2",
    "test/test_jerry",
    "test/test_kaiser",
    "test/tom_test_01",
    "train",
    "tutorial",
    "w_treat",
    "ware1",
    "ware2",
    "waste1",
    "waste2",
    "waste3",
    "xcompnd1",
    "xcompnd2",
    "xdm1",
    "xdm2",
    "xdm3",
    "xdm4",
    "xdm5",
    "xdm6",
    "xdm7",
    "xhangar1",
    "xhangar2",
    "xintell",
    "xmoon1",
    "xmoon2",
    "xreactor",
    "xsewer1",
    "xsewer2",
    "xship",
    "xswamp",
    "xware"};

#if 0
// Function to check if a map is a stock map
bool isStockMap(const std::string& mapName) {
	return std::find(stock_maps.begin(), stock_maps.end(), mapName) != stock_maps.end();
}
#endif

enum class Team : uint8_t { None, Spectator, Free, Red, Blue, Total };

enum class TournamentVetoAction : uint8_t { Ban, Pick };

struct TournamentVetoStep {
  TournamentVetoAction action = TournamentVetoAction::Ban;
  Team side = Team::None;
};

struct TournamentParticipant {
  std::string socialId;
  std::string name;
  Team lockedTeam = Team::Free;
  Team vetoSide = Team::None;
  bool captain = false;
};

enum class GameType : uint8_t {
  None,

  FreeForAll,
  Duel,
  TeamDeathmatch,
  Domination,
  CaptureTheFlag,
  ClanArena,
  OneFlag,
  Harvester,
  Overload,
  FreezeTag,
  CaptureStrike,
  RedRover,
  LastManStanding,
  LastTeamStanding,
  Horde,
  HeadHunters,
  ProBall,
  Gauntlet,

  Total
};

constexpr GameType GT_FIRST = GameType::None;
constexpr GameType GT_LAST = GameType::Gauntlet;

enum class GameFlags : uint32_t {
  None = 0,
  Teams = 1 << 0,       // 0x01
  CTF = 1 << 1,         // 0x02
  Arena = 1 << 2,       // 0x04
  Rounds = 1 << 3,      // 0x08
  Elimination = 1 << 4, // 0x10
  Frags = 1 << 5,       // 0x20
  OneVOne = 1 << 6      // 0x40
};

// Overload bitwise operators to allow combining flags in a type-safe way.
// Example: GameFlags::Teams | GameFlags::Frags
constexpr GameFlags operator|(GameFlags a, GameFlags b) {
  return static_cast<GameFlags>(static_cast<uint32_t>(a) |
                                static_cast<uint32_t>(b));
}
constexpr GameFlags operator&(GameFlags a, GameFlags b) {
  return static_cast<GameFlags>(static_cast<uint32_t>(a) &
                                static_cast<uint32_t>(b));
}
constexpr bool HasFlag(GameFlags allFlags, GameFlags flag) {
  return (allFlags & flag) != GameFlags::None;
}

// A central struct to hold all information for a single gametype.
// This replaces the multiple parallel arrays.
struct GameTypeInfo {
  GameType type;
  std::string_view short_name;
  std::string_view short_name_upper;
  std::string_view long_name;
  std::string_view spawn_name;
  GameFlags flags;
};

// A single, constant array of GameTypeInfo structs.
// All data is defined in one place, making it easy to add or modify gametypes.
constexpr std::array<GameTypeInfo, static_cast<size_t>(GameType::Total)>
    GAME_MODES = {
        {{GameType::None, "none", "NONE", "None", "none", GameFlags::None},
         {GameType::FreeForAll, "ffa", "FFA", "Free For All", "ffa",
          GameFlags::Frags},
         {GameType::Duel, "duel", "DUEL", "Duel", "tournament",
          GameFlags::OneVOne | GameFlags::Frags},
         {GameType::TeamDeathmatch, "tdm", "TDM", "Team Deathmatch", "team",
          GameFlags::Teams | GameFlags::Frags},
         {GameType::Domination, "dom", "DOM", "Domination", "domination",
          GameFlags::Teams | GameFlags::Frags},
         {GameType::CaptureTheFlag, "ctf", "CTF", "Capture The Flag", "ctf",
          GameFlags::Teams | GameFlags::CTF},
         {GameType::ClanArena, "ca", "CA", "Clan Arena", "ca",
          GameFlags::Teams | GameFlags::Arena | GameFlags::Rounds |
              GameFlags::Elimination},
         {GameType::OneFlag, "oneflag", "ONEFLAG", "One Flag", "1f",
          GameFlags::Teams | GameFlags::CTF},
         {GameType::Harvester, "har", "HAR", "Harvester", "har",
          GameFlags::Teams | GameFlags::CTF},
         {GameType::Overload, "overload", "OVLD", "Overload", "obelisk",
          GameFlags::Teams | GameFlags::CTF},
         {GameType::FreezeTag, "ft", "FT", "Freeze Tag", "freeze",
          GameFlags::Teams | GameFlags::Rounds | GameFlags::Elimination},
         {GameType::CaptureStrike, "strike", "STRIKE", "CaptureStrike",
          "strike",
          GameFlags::Teams | GameFlags::Arena | GameFlags::Rounds |
              GameFlags::CTF | GameFlags::Elimination},
         {GameType::RedRover, "rr", "REDROVER", "Red Rover", "rr",
          GameFlags::Teams | GameFlags::Rounds | GameFlags::Arena},
         {GameType::LastManStanding, "lms", "LMS", "Last Man Standing", "lms",
          GameFlags::Elimination},
         {GameType::LastTeamStanding, "lts", "LTS", "Last Team Standing", "lts",
          GameFlags::Teams | GameFlags::Elimination},
         {GameType::Horde, "horde", "HORDE", "Horde Mode", "horde",
          GameFlags::Rounds},
         {GameType::HeadHunters, "hh", "HH", "Head Hunters", "hh",
          GameFlags::None},
         {GameType::ProBall, "ball", "BALL", "ProBall", "ball",
          GameFlags::Teams},
         {GameType::Gauntlet, "gauntlet", "GAUNTLET", "Gauntlet", "gauntlet",
          GameFlags::OneVOne | GameFlags::Rounds | GameFlags::Frags}}};

extern cvar_t *g_gametype;

enum class MatchState : uint8_t {
  None,
  Initial_Delay,  // pre-warmup (delay is active)
  Warmup_Default, // 'waiting for players' / match short of players / imbalanced
                  // teams
  Warmup_ReadyUp, // time for players to get ready
  Countdown,      // all conditions met, counting down to match start, check
                  // conditions again at end of countdown before match start
  In_Progress,    // match is in progress, not used in round-based gametypes
  Ended           // match or final round has ended
};

enum class WarmupState : uint8_t {
  Default,
  Too_Few_Players,
  Teams_Imbalanced,
  Not_Ready
};

enum class RoundState : uint8_t {
  None,
  Countdown,   // round-based gametypes only: initial delay before round starts
  In_Progress, // round-based gametypes only: round is in progress
  Ended        // round-based gametypes only: round has ended
};

enum class PlayerMedal : uint8_t {
  None,

  Excellent,
  Humiliation,
  Impressive,
  Rampage,
  First_Frag,
  Defence,
  Assist,
  Captures,
  Holy_Shit,

  Total
};

// Medal names for mapping
const std::array<std::string, static_cast<uint8_t>(PlayerMedal::Total)>
    awardNames = {"None",

                  "Excellent",      "Humiliation",  "Impressive",
                  "Rampage",        "First Frag",   "Base Defense",
                  "Carrier Assist", "Flag Capture", "Holy Shit!"};

constexpr int RANK_TIED_FLAG = 0x4000;

enum class GrappleState : uint8_t { None, Fly, Pull, Hang };

struct VoteCommand;

extern int ii_highlight;
extern int ii_duel_header;
extern int ii_teams_red_default;
extern int ii_teams_blue_default;
extern int ii_ctf_red_dropped;
extern int ii_ctf_blue_dropped;
extern int ii_ctf_red_taken;
extern int ii_ctf_blue_taken;
extern int ii_teams_red_tiny;
extern int ii_teams_blue_tiny;
extern int ii_teams_header_red;
extern int ii_teams_header_blue;
extern int mi_ctf_red_flag, mi_ctf_blue_flag; // [Paril-KEX]

//==================================================================

constexpr Vector3 PLAYER_MINS = {-16, -16, -24};
constexpr Vector3 PLAYER_MAXS = {16, 16, 32};

#include <charconv>

template <typename T>
constexpr bool is_char_ptr_v = std::is_convertible_v<T, const char *>;

template <typename T>
constexpr bool is_string_like_v = std::is_convertible_v<T, std::string_view>;

template <typename T>
constexpr bool is_valid_loc_embed_v =
    std::is_floating_point_v<std::remove_reference_t<T>> ||
    std::is_integral_v<std::remove_reference_t<T>> || is_char_ptr_v<T> ||
    is_string_like_v<T>;

struct local_game_import_t : game_import_t {
  inline local_game_import_t() = default;
  inline local_game_import_t(const game_import_t &imports)
      : game_import_t(imports) {}

private:
  // shared buffer for wrappers below
  static char print_buffer[0x10000];

public:
  template <typename... Args>
  inline void Com_PrintFmt(std::format_string<Args...> format_str,
                           Args &&...args) {
    G_FmtTo_(print_buffer, format_str, std::forward<Args>(args)...);
    Com_Print(print_buffer);
  }

  template <typename... Args>
  inline void Com_ErrorFmt(std::format_string<Args...> format_str,
                           Args &&...args) {
    G_FmtTo_(print_buffer, format_str, std::forward<Args>(args)...);
    Com_Error(print_buffer);
  }

private:
  // localized print functions
  template <typename T>
  inline void loc_embed(T input, char *buffer, const char *&output) {
    using Decayed = std::remove_reference_t<T>;

    if constexpr (std::is_floating_point_v<Decayed> ||
                  std::is_integral_v<Decayed>) {
      auto result = std::to_chars(buffer, buffer + MAX_INFO_STRING - 1, input);
      *result.ptr = '\0';
      output = buffer;
    } else if constexpr (is_char_ptr_v<T>) {
      if (!input)
        Com_Error("null const char ptr passed to loc");

      output = input;
    } else if constexpr (is_string_like_v<Decayed>) {
      std::string_view view = std::string_view(input);
      const size_t copy_len =
          std::min(view.size(), size_t(MAX_INFO_STRING - 1));
      std::copy_n(view.data(), copy_len, buffer);
      buffer[copy_len] = '\0';
      output = buffer;
    } else
      Com_Error("invalid loc argument");
  }

  static std::array<char[MAX_INFO_STRING], MAX_LOCALIZATION_ARGS> buffers;
  static std::array<const char *, MAX_LOCALIZATION_ARGS> buffer_ptrs;

public:
  template <typename... Args>
  inline void LocClient_Print(gentity_t *e, print_type_t level,
                              const char *base, Args &&...args) {
    static_assert(sizeof...(args) < MAX_LOCALIZATION_ARGS,
                  "too many arguments to gi.LocClient_Print");
    static_assert(((is_valid_loc_embed_v<Args>) && ...),
                  "invalid argument passed to gi.LocClient_Print");

    size_t n = 0;
    ((loc_embed(args, buffers[n], buffer_ptrs[n]), ++n), ...);

    Loc_Print(e, level, base, &buffer_ptrs.front(), sizeof...(args));
  }

  template <typename... Args>
  inline void LocBroadcast_Print(print_type_t level, const char *base,
                                 Args &&...args) {
    static_assert(sizeof...(args) < MAX_LOCALIZATION_ARGS,
                  "too many arguments to gi.LocBroadcast_Print");
    static_assert(((is_valid_loc_embed_v<Args>) && ...),
                  "invalid argument passed to gi.LocBroadcast_Print");

    size_t n = 0;
    ((loc_embed(args, buffers[n], buffer_ptrs[n]), ++n), ...);

    Loc_Print(nullptr, (print_type_t)(level | print_type_t::PRINT_BROADCAST),
              base, &buffer_ptrs.front(), sizeof...(args));
  }

  template <typename... Args>
  inline void LocCenter_Print(gentity_t *e, const char *base, Args &&...args) {
    static_assert(sizeof...(args) < MAX_LOCALIZATION_ARGS,
                  "too many arguments to gi.LocCenter_Print");
    static_assert(((is_valid_loc_embed_v<Args>) && ...),
                  "invalid argument passed to gi.LocCenter_Print");

    size_t n = 0;
    ((loc_embed(args, buffers[n], buffer_ptrs[n]), ++n), ...);

    Loc_Print(e, PRINT_CENTER, base, &buffer_ptrs.front(), sizeof...(args));
  }

  // collision detection
  [[nodiscard]] inline trace_t trace(const Vector3 &start, const Vector3 &mins,
                                     const Vector3 &maxs, const Vector3 &end,
                                     const gentity_t *passent,
                                     contents_t contentmask) const {
    return game_import_t::trace(start, &mins, &maxs, end, passent, contentmask);
  }

  [[nodiscard]] inline trace_t traceLine(const Vector3 &start,
                                         const Vector3 &end,
                                         const gentity_t *passent,
                                         contents_t contentmask) const {
    return game_import_t::trace(start, nullptr, nullptr, end, passent,
                                contentmask);
  }

  // [Paril-KEX] clip the box against the specified entity
  [[nodiscard]] inline trace_t clip(gentity_t *entity, const Vector3 &start,
                                    const Vector3 &mins, const Vector3 &maxs,
                                    const Vector3 &end,
                                    contents_t contentmask) const {
    return game_import_t::clip(entity, start, &mins, &maxs, end, contentmask);
  }

  [[nodiscard]] inline trace_t clip(gentity_t *entity, const Vector3 &start,
                                    const Vector3 &end,
                                    contents_t contentmask) const {
    return game_import_t::clip(entity, start, nullptr, nullptr, end,
                               contentmask);
  }

  void unicast(gentity_t *ent, bool reliable, uint32_t dupe_key = 0) const {
    game_import_t::unicast(ent, reliable, dupe_key);
  }

  void localSound(gentity_t *target, const Vector3 &origin, gentity_t *ent,
                  soundchan_t channel, int soundIndex, float volume,
                  float attenuation, float timeofs,
                  uint32_t dupe_key = 0) const {
    game_import_t::localSound(target, &origin, ent, channel, soundIndex, volume,
                              attenuation, timeofs, dupe_key);
  }

  void localSound(gentity_t *target, gentity_t *ent, soundchan_t channel,
                  int soundIndex, float volume, float attenuation,
                  float timeofs, uint32_t dupe_key = 0) const {
    game_import_t::localSound(target, nullptr, ent, channel, soundIndex, volume,
                              attenuation, timeofs, dupe_key);
  }

  void localSound(const Vector3 &origin, gentity_t *ent, soundchan_t channel,
                  int soundIndex, float volume, float attenuation,
                  float timeofs, uint32_t dupe_key = 0) const {
    game_import_t::localSound(ent, &origin, ent, channel, soundIndex, volume,
                              attenuation, timeofs, dupe_key);
  }

  void localSound(gentity_t *ent, soundchan_t channel, int soundIndex,
                  float volume, float attenuation, float timeofs,
                  uint32_t dupe_key = 0) const {
    game_import_t::localSound(ent, nullptr, ent, channel, soundIndex, volume,
                              attenuation, timeofs, dupe_key);
  }
};

// This namespace encapsulates game state logic and provides safe, inline
// functions to replace the old macros.
namespace Game {
/*
=============
NormalizeTypeValue

Clamps a requested gametype into the valid range, reporting when adjustments
occur.
=============
*/
inline GameType NormalizeTypeValue(int type_value) {
  const int min_value = static_cast<int>(GT_FIRST);
  const int max_value = static_cast<int>(GT_LAST);
  const int clamped_value = std::clamp(type_value, min_value, max_value);

  if (clamped_value != type_value)
    gi.Com_PrintFmt("g_gametype {} out of range, clamped to {}\n", type_value,
                    clamped_value);

  return static_cast<GameType>(clamped_value);
}

// This function would get the current game type from your game's state,
// for example, a cvar like `g_gametype`.
inline GameType GetCurrentType() {
  if (!g_gametype)
    return GT_FIRST;

  return NormalizeTypeValue(g_gametype->integer);
}

inline const GameTypeInfo &GetInfo(GameType type) {
  const int raw_value = static_cast<int>(type);
  if (raw_value < static_cast<int>(GameType::None) ||
      raw_value >= static_cast<int>(GameType::Total)) {
    return GAME_MODES[static_cast<size_t>(GT_FIRST)];
  }
  return GAME_MODES[static_cast<size_t>(raw_value)];
}

inline const GameTypeInfo &GetInfo(int type_value) {
  return GetInfo(NormalizeTypeValue(type_value));
}

// Returns all information for the currently active gametype.
inline const GameTypeInfo &GetCurrentInfo() {
  if (!g_gametype)
    return GetInfo(GT_FIRST);

  return GetInfo(g_gametype->integer);
}

// Checks if the current gametype is a specific type.
// Replaces: Game::Is(GameType::TeamDeathmatch)
inline bool Is(GameType type) { return GetCurrentType() == type; }

// Checks if the current gametype is NOT a specific type.
// Replaces: Game::IsNot(GameType::TeamDeathmatch)
inline bool IsNot(GameType type) { return GetCurrentType() != type; }

// Checks if the current gametype has a specific property flag.
// Replaces: Game::Has(GTF_TEAMS)
inline bool Has(GameFlags flag) {
  return HasFlag(GetCurrentInfo().flags, flag);
}

// Checks if the current gametype does NOT have a specific property flag.
// Replaces: !Game::Has(GTF_TEAMS)
inline bool DoesNotHave(GameFlags flag) {
  return !HasFlag(GetCurrentInfo().flags, flag);
}

inline bool IsCurrentTypeValid() {
  if (!g_gametype)
    return false;

  const int type_value = g_gametype->integer;
  return type_value >= static_cast<int>(GT_FIRST) &&
         type_value <= static_cast<int>(GT_LAST);
}

inline char ToLowerASCII(char ch) {
  return static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
}

/*
=============
AreStringsEqualIgnoreCase

Helper for case-insensitive string_view comparison.
=============
*/
inline bool AreStringsEqualIgnoreCase(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) {
    return false;
  }

  for (size_t i = 0; i < a.size(); ++i) {
    if (ToLowerASCII(a[i]) != ToLowerASCII(b[i]))
      return false;
  }

  return true;
}

/*
=============
FromString

Finds a gametype by its short, long, or spawn name (case-insensitive).
=============
*/
inline std::optional<GameType> FromString(std::string_view name) {
  for (const auto &mode_info : GAME_MODES) {
    if (AreStringsEqualIgnoreCase(name, mode_info.short_name) ||
        AreStringsEqualIgnoreCase(name, mode_info.long_name) ||
        AreStringsEqualIgnoreCase(name, mode_info.spawn_name)) {
      return mode_info.type;
    }
  }
  return std::nullopt; // Return an empty optional if no match is found.
}
} // namespace Game

// =================================================================

// A template class to manage bitflags for any enum type.

// The Opt-in Trait: By default, no enum is a bitmask enum.
// You must specialize this for any enum you want to use with these operators.
template <typename E> struct is_bitmask_enum : std::false_type {};

// The generic, reusable BitFlags class
template <typename Enum>
  requires std::is_enum_v<Enum>
class BitFlags {
public:
  using T = std::underlying_type_t<Enum>;
  T value = 0;

  // Constructors
  constexpr BitFlags() = default;
  constexpr BitFlags(Enum flag) : value(static_cast<T>(flag)) {}
  constexpr explicit BitFlags(T v) : value(v) {}

  // Comparison (C++20 spaceship operator)
  auto operator<=>(const BitFlags<Enum> &) const = default;

  // Conversion
  explicit operator T() const { return value; }
  explicit operator bool() const { return !!value; }

  // Helper Methods
  constexpr bool any() const { return !!value; }
  constexpr bool none() const { return !value; }
  constexpr bool has(BitFlags<Enum> flags) const {
    return !!(value & flags.value);
  }
  constexpr bool has_all(BitFlags<Enum> flags) const {
    return (value & flags.value) == flags.value;
  }

  // Modification Methods
  constexpr BitFlags<Enum> &set(BitFlags<Enum> flags) {
    value |= flags.value;
    return *this;
  }
  constexpr BitFlags<Enum> &clear(BitFlags<Enum> flags) {
    value &= ~flags.value;
    return *this;
  }
  constexpr BitFlags<Enum> &toggle(BitFlags<Enum> flags) {
    value ^= flags.value;
    return *this;
  }

  // Operators
  constexpr bool operator!() const { return !value; }
  constexpr BitFlags<Enum> operator~() const { return BitFlags<Enum>(~value); }
  constexpr BitFlags<Enum> operator|(BitFlags<Enum> other) const {
    return BitFlags<Enum>(value | other.value);
  }
  constexpr BitFlags<Enum> operator&(BitFlags<Enum> other) const {
    return BitFlags<Enum>(value & other.value);
  }
  constexpr BitFlags<Enum> operator^(BitFlags<Enum> other) const {
    return BitFlags<Enum>(value ^ other.value);
  }

  constexpr BitFlags<Enum> &operator|=(BitFlags<Enum> other) {
    value |= other.value;
    return *this;
  }
  constexpr BitFlags<Enum> &operator&=(BitFlags<Enum> other) {
    value &= other.value;
    return *this;
  }
  constexpr BitFlags<Enum> &operator^=(BitFlags<Enum> other) {
    value ^= other.value;
    return *this;
  }
};

// Trait-constrained free-function operators for seamless enum usage.
// These will only be enabled for enums that specialize 'is_bitmask_enum'.
template <typename Enum>
  requires is_bitmask_enum<Enum>::value
/*
=============
EnumToValue

Converts the provided enum to its underlying integral representation without
relying on C++23's std::to_underlying so tests can compile under C++20.
=============
*/
constexpr std::underlying_type_t<Enum> EnumToValue(Enum value) noexcept {
  return static_cast<std::underlying_type_t<Enum>>(value);
}

template <typename Enum>
  requires is_bitmask_enum<Enum>::value
/*
=============
operator|

Computes the union of two bitmask-enabled enums.
=============
*/
constexpr BitFlags<Enum> operator|(Enum lhs, Enum rhs) {
  return BitFlags<Enum>(EnumToValue(lhs) | EnumToValue(rhs));
}

template <typename Enum>
  requires is_bitmask_enum<Enum>::value
/*
=============
operator&

Computes the intersection of two bitmask-enabled enums.
=============
*/
constexpr BitFlags<Enum> operator&(Enum lhs, Enum rhs) {
  return BitFlags<Enum>(EnumToValue(lhs) & EnumToValue(rhs));
}

template <typename Enum>
  requires is_bitmask_enum<Enum>::value
/*
=============
operator^

Computes the symmetric difference of two bitmask-enabled enums.
=============
*/
constexpr BitFlags<Enum> operator^(Enum lhs, Enum rhs) {
  return BitFlags<Enum>(EnumToValue(lhs) ^ EnumToValue(rhs));
}

template <typename Enum>
  requires is_bitmask_enum<Enum>::value
/*
=============
operator~

Flips all bits for a bitmask-enabled enum.
=============
*/
constexpr BitFlags<Enum> operator~(Enum val) {
  return BitFlags<Enum>(~EnumToValue(val));
}

// ==================================================================

// A simple iterator to iterate over enum values from 0 to _Count - 1.

template <typename E> class EnumIterator {
public:
  // Required iterator traits
  using iterator_category = std::forward_iterator_tag;
  using value_type = E;
  using difference_type = std::ptrdiff_t;
  using pointer = E *;
  using reference = E &;

  constexpr EnumIterator(std::underlying_type_t<E> value)
      : m_value(static_cast<E>(value)) {}

  constexpr E operator*() const { return m_value; }

  constexpr EnumIterator &operator++() {
    using T = std::underlying_type_t<E>;
    m_value = static_cast<E>(static_cast<T>(m_value) + 1);
    return *this;
  }

  constexpr bool operator!=(const EnumIterator &other) const {
    return m_value != other.m_value;
  }

private:
  E m_value;
};

template <typename E>
constexpr auto begin(E) -> std::enable_if_t<std::is_enum_v<E> &&requires {
  E::_Count;
}, EnumIterator<E>> {
  return EnumIterator<E>(0);
}

template <typename E>
constexpr auto end(E) -> std::enable_if_t<std::is_enum_v<E> &&requires {
  E::_Count;
}, EnumIterator<E>> {
  return EnumIterator<E>(static_cast<std::underlying_type_t<E>>(E::_Count));
}

// ==================================================================
struct SpawnFlags {
  uint32_t value{};

  constexpr SpawnFlags() = default;
  explicit constexpr SpawnFlags(uint32_t v) noexcept : value(v) {}

  constexpr SpawnFlags(const SpawnFlags &) = default;
  constexpr SpawnFlags(SpawnFlags &&) = default;
  constexpr SpawnFlags &operator=(const SpawnFlags &) = default;
  constexpr SpawnFlags &operator=(SpawnFlags &&) = default;
  ~SpawnFlags() = default;

  [[nodiscard]] explicit constexpr operator uint32_t() const noexcept {
    return value;
  }

  // has any flags at all (!!a)
  [[nodiscard]] constexpr bool any() const noexcept { return value != 0; }
  // has any of the given flags (!!(a & b))
  [[nodiscard]] constexpr bool has(const SpawnFlags &flags) const noexcept {
    return (value & flags.value) != 0;
  }
  // has all of the given flags ((a & b) == b)
  [[nodiscard]] constexpr bool has_all(const SpawnFlags &flags) const noexcept {
    return (value & flags.value) == flags.value;
  }
  constexpr bool operator!() const noexcept { return value == 0; }

  constexpr bool operator==(const SpawnFlags &flags) const noexcept {
    return value == flags.value;
  }

  constexpr bool operator!=(const SpawnFlags &flags) const noexcept {
    return value != flags.value;
  }

  [[nodiscard]] constexpr SpawnFlags operator~() const noexcept {
    return SpawnFlags(~value);
  }
  [[nodiscard]] constexpr SpawnFlags
  operator|(const SpawnFlags &v2) const noexcept {
    return SpawnFlags(value | v2.value);
  }
  [[nodiscard]] constexpr SpawnFlags
  operator&(const SpawnFlags &v2) const noexcept {
    return SpawnFlags(value & v2.value);
  }
  [[nodiscard]] constexpr SpawnFlags
  operator^(const SpawnFlags &v2) const noexcept {
    return SpawnFlags(value ^ v2.value);
  }
  constexpr SpawnFlags &operator|=(const SpawnFlags &v2) noexcept {
    value |= v2.value;
    return *this;
  }
  constexpr SpawnFlags &operator&=(const SpawnFlags &v2) noexcept {
    value &= v2.value;
    return *this;
  }
  constexpr SpawnFlags &operator^=(const SpawnFlags &v2) noexcept {
    value ^= v2.value;
    return *this;
  }
};

// ent->spawnFlags
// these are set with checkboxes on each entity in the map editor.
// the following 8 are reserved and should never be used by any entity.
// (power cubes in coop use these after spawning as well)
//
// these spawnFlags affect every entity. note that items are a bit special
// because these 8 bits are instead used for power cube bits.
constexpr SpawnFlags SPAWNFLAG_NONE = SpawnFlags(0);
constexpr SpawnFlags SPAWNFLAG_NOT_EASY = SpawnFlags(0x00000100),
                     SPAWNFLAG_NOT_MEDIUM = SpawnFlags(0x00000200),
                     SPAWNFLAG_NOT_HARD = SpawnFlags(0x00000400),
                     SPAWNFLAG_NOT_DEATHMATCH = SpawnFlags(0x00000800),
                     SPAWNFLAG_NOT_COOP = SpawnFlags(0x00001000),
                     SPAWNFLAG_RESERVED1 = SpawnFlags(0x00002000),
                     SPAWNFLAG_COOP_ONLY = SpawnFlags(0x00004000),
                     SPAWNFLAG_RESERVED2 = SpawnFlags(0x00008000);

constexpr SpawnFlags SPAWNFLAG_EDITOR_MASK =
    (SPAWNFLAG_NOT_EASY | SPAWNFLAG_NOT_MEDIUM | SPAWNFLAG_NOT_HARD |
     SPAWNFLAG_NOT_DEATHMATCH | SPAWNFLAG_NOT_COOP | SPAWNFLAG_RESERVED1 |
     SPAWNFLAG_COOP_ONLY);

// use this for global spawnFlags
constexpr SpawnFlags operator""_spawnflag(unsigned long long int v) {
  if (v & SPAWNFLAG_EDITOR_MASK.value)
    throw std::invalid_argument("attempting to use reserved spawnflag");

  return static_cast<SpawnFlags>(static_cast<uint32_t>(v));
}

// use this for global spawnFlags
constexpr SpawnFlags operator""_spawnflag_bit(unsigned long long int v) {
  v = 1ull << v;

  if (v & SPAWNFLAG_EDITOR_MASK.value)
    throw std::invalid_argument("attempting to use reserved spawnflag");

  return static_cast<SpawnFlags>(static_cast<uint32_t>(v));
}

constexpr SpawnFlags SPAWNFLAG_DOMINATION_START_RED = 0x00000001_spawnflag;
constexpr SpawnFlags SPAWNFLAG_DOMINATION_START_BLUE = 0x00000002_spawnflag;

constexpr SpawnFlags SPAWNFLAG_PROBALL_GOAL_RED = 0x00000001_spawnflag;
constexpr SpawnFlags SPAWNFLAG_PROBALL_GOAL_BLUE = 0x00000002_spawnflag;

// stores a level time; most newer engines use int64_t for
// time storage, but seconds are handy for compatibility
// with Quake and older mods.
struct GameTime {
private:
  // times always start at zero, just to prevent memory issues
  int64_t _ms = 0;

  // internal; use _sec/_ms/_min or
  // GameTime::from_sec(n)/GameTime::from_ms(n)/GameTime::from_min(n)
  constexpr explicit GameTime(const int64_t &ms) : _ms(ms) {}

public:
  constexpr GameTime() = default;
  constexpr GameTime(const GameTime &) = default;
  constexpr GameTime &operator=(const GameTime &) = default;

  // constructors are here, explicitly named, so you always
  // know what you're getting.

  // new time from ms
  static constexpr GameTime from_ms(const int64_t &ms) { return GameTime(ms); }

  // new time from seconds
  template <typename T,
            typename = std::enable_if_t<std::is_floating_point_v<T> ||
                                        std::is_integral_v<T>>>
  static constexpr GameTime from_sec(const T &s) {
    return GameTime(static_cast<int64_t>(s * 1000));
  }

  // new time from minutes
  template <typename T,
            typename = std::enable_if_t<std::is_floating_point_v<T> ||
                                        std::is_integral_v<T>>>
  static constexpr GameTime from_min(const T &s) {
    return GameTime(static_cast<int64_t>(s * 60000));
  }

  // new time from hz
  static constexpr GameTime from_hz(const uint64_t &hz) {
    return from_ms(static_cast<int64_t>((1.0 / hz) * 1000));
  }

  // get value in minutes (truncated for integers)
  template <typename T = float> constexpr T minutes() const {
    return static_cast<T>(
        _ms /
        static_cast<std::conditional_t<std::is_floating_point_v<T>, T, float>>(
            60000));
  }

  // get value in seconds (truncated for integers)
  template <typename T = float> constexpr T seconds() const {
    return static_cast<T>(
        _ms /
        static_cast<std::conditional_t<std::is_floating_point_v<T>, T, float>>(
            1000));
  }

  // get value in milliseconds
  constexpr const int64_t &milliseconds() const { return _ms; }

  int64_t frames() const { return _ms / gi.frameTimeMs; }

  // check if non-zero
  constexpr explicit operator bool() const { return !!_ms; }

  // invert time
  constexpr GameTime operator-() const { return GameTime(-_ms); }

  // operations with other times as input
  constexpr GameTime operator+(const GameTime &r) const {
    return GameTime(_ms + r._ms);
  }
  constexpr GameTime operator-(const GameTime &r) const {
    return GameTime(_ms - r._ms);
  }
  constexpr GameTime &operator+=(const GameTime &r) {
    return *this = *this + r;
  }
  constexpr GameTime &operator-=(const GameTime &r) {
    return *this = *this - r;
  }

  // operations with scalars as input
  template <typename T,
            typename = std::enable_if_t<std::is_floating_point_v<T> ||
                                        std::is_integral_v<T>>>
  constexpr GameTime operator*(const T &r) const {
    return GameTime::from_ms(static_cast<int64_t>(_ms * r));
  }
  template <typename T,
            typename = std::enable_if_t<std::is_floating_point_v<T> ||
                                        std::is_integral_v<T>>>
  constexpr GameTime operator/(const T &r) const {
    return GameTime::from_ms(static_cast<int64_t>(_ms / r));
  }
  template <typename T,
            typename = std::enable_if_t<std::is_floating_point_v<T> ||
                                        std::is_integral_v<T>>>
  constexpr GameTime &operator*=(const T &r) {
    return *this = *this * r;
  }
  template <typename T,
            typename = std::enable_if_t<std::is_floating_point_v<T> ||
                                        std::is_integral_v<T>>>
  constexpr GameTime &operator/=(const T &r) {
    return *this = *this / r;
  }

  // comparisons with gtime_ts
  constexpr bool operator==(const GameTime &time) const {
    return _ms == time._ms;
  }
  constexpr bool operator!=(const GameTime &time) const {
    return _ms != time._ms;
  }
  constexpr bool operator<(const GameTime &time) const {
    return _ms < time._ms;
  }
  constexpr bool operator>(const GameTime &time) const {
    return _ms > time._ms;
  }
  constexpr bool operator<=(const GameTime &time) const {
    return _ms <= time._ms;
  }
  constexpr bool operator>=(const GameTime &time) const {
    return _ms >= time._ms;
  }
};

// user literals, allowing you to specify times
// as 128_sec and 128_ms
constexpr GameTime operator""_min(long double s) {
  return GameTime::from_min(s);
}
constexpr GameTime operator""_min(unsigned long long int s) {
  return GameTime::from_min(s);
}
constexpr GameTime operator""_sec(long double s) {
  return GameTime::from_sec(s);
}
constexpr GameTime operator""_sec(unsigned long long int s) {
  return GameTime::from_sec(s);
}
constexpr GameTime operator""_ms(long double s) {
  return GameTime::from_ms(static_cast<int64_t>(s));
}
constexpr GameTime operator""_ms(unsigned long long int s) {
  return GameTime::from_ms(static_cast<int64_t>(s));
}
constexpr GameTime operator""_hz(unsigned long long int s) {
  return GameTime::from_ms(static_cast<int64_t>((1.0 / s) * 1000));
}

#define SERVER_TICK_RATE gi.tickRate // in hz
extern GameTime FRAME_TIME_S;
extern GameTime FRAME_TIME_MS;

// view pitching times
inline GameTime DAMAGE_TIME_SLACK() { return (100_ms - FRAME_TIME_MS); }

inline GameTime DAMAGE_TIME() { return 500_ms + DAMAGE_TIME_SLACK(); }

inline GameTime FALL_TIME() { return 300_ms + DAMAGE_TIME_SLACK(); }

constexpr GameTime CORPSE_SINK_TIME = 5_sec;

// every save_data_list_t has a tag
// which is used for integrity checks.
enum save_data_tag_t {
  SAVE_DATA_MMOVE,

  SAVE_FUNC_MONSTERINFO_STAND,
  SAVE_FUNC_MONSTERINFO_IDLE,
  SAVE_FUNC_MONSTERINFO_SEARCH,
  SAVE_FUNC_MONSTERINFO_WALK,
  SAVE_FUNC_MONSTERINFO_RUN,
  SAVE_FUNC_MONSTERINFO_DODGE,
  SAVE_FUNC_MONSTERINFO_ATTACK,
  SAVE_FUNC_MONSTERINFO_MELEE,
  SAVE_FUNC_MONSTERINFO_SIGHT,
  SAVE_FUNC_MONSTERINFO_CHECKATTACK,
  SAVE_FUNC_MONSTERINFO_SETSKIN,

  SAVE_FUNC_MONSTERINFO_BLOCKED,
  SAVE_FUNC_MONSTERINFO_DUCK,
  SAVE_FUNC_MONSTERINFO_UNDUCK,
  SAVE_FUNC_MONSTERINFO_SIDESTEP,
  SAVE_FUNC_MONSTERINFO_PHYSCHANGED,

  SAVE_FUNC_MOVEINFO_ENDFUNC,
  SAVE_FUNC_MOVEINFO_BLOCKED,

  SAVE_FUNC_PRETHINK,
  SAVE_FUNC_THINK,
  SAVE_FUNC_TOUCH,
  SAVE_FUNC_USE,
  SAVE_FUNC_PAIN,
  SAVE_FUNC_DIE
};

// forward-linked list, storing data for
// saving pointers. every save_data_ptr has an
// instance of this; there's one head instance of this
// in g_save.cpp.
struct save_data_list_t {
  const char *name; // name of savable object; persisted in the JSON file
  save_data_tag_t tag;
  const void *ptr;              // pointer to raw data
  const save_data_list_t *next; // next in list

  save_data_list_t(const char *name, save_data_tag_t tag, const void *ptr);

  static const save_data_list_t *fetch(const void *link_ptr,
                                       save_data_tag_t tag);
};

#include <functional>

// save data wrapper, which holds a pointer to a T
// and the tag value for integrity. this is how you
// store a savable pointer of data safely.
template <typename T, size_t Tag> struct save_data_t {
  using value_type = typename std::conditional<
      std::is_pointer<T>::value &&
          std::is_function<typename std::remove_pointer<T>::type>::value,
      T, const T *>::type;

private:
  value_type value;
  const save_data_list_t *list;

public:
  constexpr save_data_t() : value(nullptr), list(nullptr) {}

  constexpr save_data_t(nullptr_t) : save_data_t() {}

  constexpr save_data_t(const save_data_list_t *list_in)
      : value(list_in->ptr), list(list_in) {}

  inline save_data_t(value_type ptr_in)
      : value(ptr_in), list(ptr_in ? save_data_list_t::fetch(
                                         reinterpret_cast<const void *>(ptr_in),
                                         static_cast<save_data_tag_t>(Tag))
                                   : nullptr) {}

  inline save_data_t(const save_data_t<T, Tag> &ref_in)
      : save_data_t(ref_in.value) {}

  inline save_data_t &operator=(value_type ptr_in) {
    if (value != ptr_in) {
      value = ptr_in;
      list =
          value
              ? save_data_list_t::fetch(reinterpret_cast<const void *>(ptr_in),
                                        static_cast<save_data_tag_t>(Tag))
              : nullptr;
    }

    return *this;
  }

  constexpr const value_type pointer() const { return value; }
  constexpr const save_data_list_t *save_list() const { return list; }
  constexpr const char *name() const { return value ? list->name : "null"; }
  constexpr const value_type operator->() const { return value; }
  constexpr explicit operator bool() const { return value; }
  constexpr bool operator==(value_type ptr_in) const { return value == ptr_in; }
  constexpr bool operator!=(value_type ptr_in) const { return value != ptr_in; }
  constexpr bool operator==(const save_data_t<T, Tag> *ptr_in) const {
    return value == ptr_in->value;
  }
  constexpr bool operator==(const save_data_t<T, Tag> &ref_in) const {
    return value == ref_in.value;
  }
  constexpr bool operator!=(const save_data_t<T, Tag> *ptr_in) const {
    return value != ptr_in->value;
  }
  constexpr bool operator!=(const save_data_t<T, Tag> &ref_in) const {
    return value != ref_in.value;
  }

  // invoke wrapper, for function-likes
  template <typename... Args> inline auto operator()(Args &&...args) const {
    static_assert(std::is_invocable_v<std::remove_pointer_t<T>, Args...>,
                  "bad invoke args");
    return std::invoke(value, std::forward<Args>(args)...);
  }
};

// memory tags to allow dynamic memory to be cleaned up
enum {
  TAG_GAME = 765, // clear when unloading the dll
  TAG_LEVEL = 766 // clear when loading a new level
};

constexpr float MELEE_DISTANCE = 50;

constexpr size_t BODY_QUEUE_SIZE = 8;

// null trace used when touches don't need a trace
constexpr trace_t null_trace{};

enum class WeaponState : uint8_t { Ready, Activating, Dropping, Firing };

// gib flags
enum gib_type_t {
  GIB_NONE = 0,     // no flags (organic)
  GIB_METALLIC = 1, // bouncier
  GIB_ACID = 2,     // acidic (gekk)
  GIB_HEAD = 4,     // head gib; the input entity will transform into this
  GIB_DEBRIS = 8,   // explode outwards rather than in velocity, no blood
  GIB_SKINNED = 16, // use skinNum
  GIB_UPRIGHT = 32, // stay upright on ground
};
MAKE_ENUM_BITFLAGS(gib_type_t);

// monster ai flags
enum monster_ai_flags_t : uint64_t {
  AI_NONE = 0,
  AI_STAND_GROUND = bit_v<0>,
  AI_TEMP_STAND_GROUND = bit_v<1>,
  AI_SOUND_TARGET = bit_v<2>,
  AI_LOST_SIGHT = bit_v<3>,
  AI_PURSUIT_LAST_SEEN = bit_v<4>,
  AI_PURSUE_NEXT = bit_v<5>,
  AI_PURSUE_TEMP = bit_v<6>,
  AI_HOLD_FRAME = bit_v<7>,
  AI_GOOD_GUY = bit_v<8>,
  AI_BRUTAL = bit_v<9>,
  AI_NOSTEP = bit_v<10>,
  AI_DUCKED = bit_v<11>,
  AI_COMBAT_POINT = bit_v<12>,
  AI_MEDIC = bit_v<13>,
  AI_RESURRECTING = bit_v<14>,
  AI_MANUAL_STEERING = bit_v<15>,
  AI_TARGET_ANGER = bit_v<16>,
  AI_DODGING = bit_v<17>,
  AI_CHARGING = bit_v<18>,
  AI_HINT_PATH = bit_v<19>,
  AI_IGNORE_SHOTS = bit_v<20>,
  // PMM - FIXME - last second added for E3 .. there's probably a better way to
  // do this, but this works
  AI_DO_NOT_COUNT = bit_v<21>, // set for healed monsters
  AI_SPAWNED_CARRIER =
      bit_v<22>, // both do_not_count and spawned are set for spawned monsters
  AI_SPAWNED_MEDIC_C =
      bit_v<23>, // both do_not_count and spawned are set for spawned monsters
  AI_SPAWNED_WIDOW =
      bit_v<24>, // both do_not_count and spawned are set for spawned monsters
  AI_SPAWNED_OLDONE = bit_v<25>,
  AI_BLOCKED = bit_v<26>, // used by blocked_checkattack: set to say I'm
                          // attacking while blocked (prevents run-attacks)
  AI_SPAWNED_ALIVE = bit_v<27>, // [Paril-KEX] for spawning dead
  AI_SPAWNED_DEAD = bit_v<28>,
  AI_HIGH_TICK_RATE = bit_v<29>,  // not limited by 10hz actions
  AI_NO_PATH_FINDING = bit_v<30>, // don't try nav nodes for path finding
  AI_PATHING = bit_v<31>,         // using nav nodes currently
  AI_STINKY = bit_v<32>,          // spawn flies
  AI_STUNK = bit_v<33>,           // already spawned files

  AI_ALTERNATE_FLY =
      bit_v<34>, // use alternate flying mechanics; see monsterInfo.fly_xxx
  AI_TEMP_MELEE_COMBAT =
      bit_v<35>,                 // temporarily switch to the melee combat style
  AI_FORGET_ENEMY = bit_v<36>,   // forget the current enemy
  AI_DOUBLE_TROUBLE = bit_v<37>, // JORG only
  AI_REACHED_HOLD_COMBAT = bit_v<38>,
  AI_THIRD_EYE = bit_v<39>,

  AI_CHTHON_VULNERABLE = bit_v<40>, // can be damaged
  AI_OLDONE_VULNERABLE = bit_v<41>, // can be damaged
  AI_SPAWNED_OVERLORD = bit_v<42>,
  AI_ENFORCER_SECOND_VOLLEY = bit_v<43>,
};
MAKE_ENUM_BITFLAGS(monster_ai_flags_t);

// mask to catch all flavors of spawned minions
constexpr monster_ai_flags_t AI_SPAWNED_MASK =
    AI_SPAWNED_CARRIER | AI_SPAWNED_MEDIC_C | AI_SPAWNED_WIDOW |
    AI_SPAWNED_OLDONE |
    AI_SPAWNED_OVERLORD; // mask to catch all three flavors of spawned

// monster attack state

enum class MonsterFlags : unsigned {
  None = 0x00,
  Ground = 0x01,
  Air = 0x02,
  Water = 0x04,
  Medium = 0x08,
  Boss = 0x10
};
// Add this line right here
MAKE_ENUM_BITFLAGS(
    MonsterFlags); // This enables operators like | and & for MonsterFlags

enum class MonsterAttackState : uint8_t {
  None,
  Straight,
  Sliding,
  Melee,
  Missile,
  Blind
};

// handedness values
enum class Handedness : uint8_t { Right, Left, Center };

enum class WeaponAutoSwitch : uint8_t { Smart, Always, Always_No_Ammo, Never };

constexpr uint32_t SFL_CROSS_TRIGGER_MASK =
    (0xffffffffu & ~SPAWNFLAG_EDITOR_MASK.value);

// noise types for G_PlayerNoise
enum class PlayerNoise : uint8_t { Self, Weapon, Impact };

enum Armor : uint8_t { Shard, Jacket, Combat, Body, Total };

/*
===============
gitem_armor_t
===============
*/
struct gitem_armor_t {
  int32_t base_count;
  int32_t max_count;
  float normal_protection;
  float energy_protection;
};

constexpr int NUM_RULESETS = static_cast<int>(Ruleset::RS_NUM_RULESETS);
constexpr int NUM_ARMOR_TYPES = Armor::Total;

using ArmorArray = std::array<gitem_armor_t, NUM_ARMOR_TYPES>;
using ArmorStatsArray = std::array<ArmorArray, NUM_RULESETS>;

constexpr ArmorStatsArray armor_stats = {
    {// None
     ArmorArray{{{3, 999, 0.30f, 0.00f},
                 {25, 50, 0.30f, 0.00f},
                 {50, 100, 0.60f, 0.30f},
                 {100, 200, 0.80f, 0.60f}}},
     // Quake1
     ArmorArray{{{1, 999, 0.30f, 0.30f},
                 {100, 100, 0.30f, 0.30f},
                 {150, 150, 0.60f, 0.60f},
                 {200, 200, 0.80f, 0.80f}}},
     // Quake2
     ArmorArray{{{3, 999, 0.30f, 0.00f},
                 {25, 50, 0.30f, 0.00f},
                 {50, 100, 0.60f, 0.30f},
                 {100, 200, 0.80f, 0.60f}}},
     // Quake3Arena
     ArmorArray{{{5, 200, 0.66f, 0.66f},
                 {25, 200, 0.66f, 0.66f},
                 {50, 200, 0.66f, 0.66f},
                 {100, 200, 0.66f, 0.66f}}}}};

struct gitem_ammo_t {
  std::array<int32_t, 3> max{}; // 0 = normal, 1 = bando, 2 = ammopack
  int32_t ammo_pu;
  int32_t weapon_pu;
  int32_t bando_pu;
  int32_t ammopack_pu;
};

const gitem_ammo_t ammoStats[static_cast<int>(
    Ruleset::RS_NUM_RULESETS)][static_cast<int>(AmmoID::_Total)] = {
    // {max_normal, max_bandolier, max_ammopack}, ammo_pu,  weapon_pu, bando_pu,
    // ammopack_pu}

    // None
    {/*AmmoID::Bullets*/ {{0, 0, 0}, 0, 0, 0, 0},
     /*AmmoID::Shells*/ {{0, 0, 0}, 0, 0, 0, 0},
     /*AmmoID::Rockets*/ {{0, 0, 0}, 0, 0, 0, 0},
     /*AmmoID::Grenades*/ {{0, 0, 0}, 0, 0, 0, 0},
     /*AmmoID::Cells*/ {{0, 0, 0}, 0, 0, 0, 0},
     /*AmmoID::Slugs*/ {{0, 0, 0}, 0, 0, 0, 0},
     /*AmmoID::MagSlugs*/ {{0, 0, 0}, 0, 0, 0, 0},
     /*AmmoID::Traps*/ {{0, 0, 0}, 0, 0, 0, 0},
     /*AmmoID::Flechettes*/ {{0, 0, 0}, 0, 0, 0, 0},
     /*AmmoID::TeslaMines*/ {{0, 0, 0}, 0, 0, 0, 0},
     /*AmmoID::Rounds*/ {{0, 0, 0}, 0, 0, 0, 0},
     /*AmmoID::ProxMines*/ {{0, 0, 0}, 0, 0, 0, 0}},
    // Quake1
    {/*AmmoID::Bullets*/ {{200, 250, 300}, 50, 50, 50, 30},
     /*AmmoID::Shells*/ {{50, 100, 150}, 10, 10, 20, 10},
     /*AmmoID::Rockets*/ {{25, 25, 50}, 10, 5, 0, 5},
     /*AmmoID::Grenades*/ {{25, 25, 50}, 10, 5, 0, 5},
     /*AmmoID::Cells*/ {{200, 200, 300}, 50, 50, 0, 30},
     /*AmmoID::Slugs*/ {{25, 25, 50}, 10, 5, 0, 5},
     /*AmmoID::MagSlugs*/ {{25, 25, 50}, 10, 5, 0, 5},
     /*AmmoID::Traps*/ {{3, 3, 12}, 1, 1, 0, 0},
     /*AmmoID::Flechettes*/ {{100, 150, 200}, 50, 50, 50, 30},
     /*AmmoID::TeslaMines*/ {{3, 3, 12}, 1, 1, 0, 0},
     /*AmmoID::Rounds*/ {{6, 6, 12}, 3, 3, 0, 3},
     /*AmmoID::ProxMines*/ {{10, 10, 20}, 5, 5, 0, 5}},
    // Quake2
    {/*AmmoID::Bullets*/ {{200, 250, 300}, 50, 50, 50, 30},
     /*AmmoID::Shells*/ {{50, 100, 150}, 10, 10, 20, 10},
     /*AmmoID::Rockets*/ {{25, 25, 50}, 10, 5, 0, 5},
     /*AmmoID::Grenades*/ {{25, 25, 50}, 10, 5, 0, 5},
     /*AmmoID::Cells*/ {{200, 200, 300}, 50, 50, 0, 30},
     /*AmmoID::Slugs*/ {{25, 25, 50}, 10, 5, 0, 5},
     /*AmmoID::MagSlugs*/ {{25, 25, 50}, 10, 5, 0, 5},
     /*AmmoID::Traps*/ {{3, 3, 12}, 1, 1, 0, 0},
     /*AmmoID::Flechettes*/ {{100, 150, 200}, 50, 50, 50, 30},
     /*AmmoID::TeslaMines*/ {{3, 3, 12}, 1, 1, 0, 0},
     /*AmmoID::Rounds*/ {{6, 6, 12}, 3, 3, 0, 3},
     /*AmmoID::ProxMines*/ {{10, 10, 20}, 5, 5, 0, 5}},
    // Quake3Arena
    {/*AmmoID::Bullets*/ {{200, 250, 300}, 50, 50, 50, 30},
     /*AmmoID::Shells*/ {{50, 100, 150}, 10, 10, 20, 10},
     /*AmmoID::Rockets*/ {{25, 25, 50}, 10, 5, 0, 5},
     /*AmmoID::Grenades*/ {{25, 25, 50}, 10, 5, 0, 5},
     /*AmmoID::Cells*/ {{200, 200, 300}, 50, 50, 0, 30},
     /*AmmoID::Slugs*/ {{25, 25, 50}, 10, 5, 0, 5},
     /*AmmoID::MagSlugs*/ {{25, 25, 50}, 10, 5, 0, 5},
     /*AmmoID::Traps*/ {{3, 3, 12}, 1, 1, 0, 0},
     /*AmmoID::Flechettes*/ {{100, 150, 200}, 50, 50, 50, 30},
     /*AmmoID::TeslaMines*/ {{3, 3, 12}, 1, 1, 0, 0},
     /*AmmoID::Rounds*/ {{6, 6, 12}, 3, 3, 0, 3},
     /*AmmoID::ProxMines*/ {{10, 10, 20}, 5, 5, 0, 5}},
};

// entity->moveType values
enum class MoveType : uint8_t {
  None,   // never moves
  NoClip, // origin and angles change with no interaction
  Push,   // no clip to world, push on box contact
  Stop,   // no clip to world, stops on box contact
  Walk,   // gravity
  Step,   // gravity, special edge handling
  Fly,
  Toss,       // gravity
  FlyMissile, // extra size to monsters
  Bounce,
  WallBounce,
  NewToss, // PGM - for deathball
  FreeCam  // spectator free cam
};

// entity->flags
enum ent_flags_t : uint64_t {
  FL_NONE = 0, // no flags
  FL_FLY = bit_v<0>,
  FL_SWIM = bit_v<1>, // implied immunity to drowning
  FL_IMMUNE_LASER = bit_v<2>,
  FL_INWATER = bit_v<3>,
  FL_GODMODE = bit_v<4>,
  FL_NOTARGET = bit_v<5>,
  FL_IMMUNE_SLIME = bit_v<6>,
  FL_IMMUNE_LAVA = bit_v<7>,
  FL_PARTIALGROUND = bit_v<8>, // not all corners are valid
  FL_WATERJUMP = bit_v<9>,     // player jumping out of water
  FL_TEAMSLAVE = bit_v<10>,    // not the first on the team
  FL_NO_KNOCKBACK = bit_v<11>,
  FL_POWER_ARMOR = bit_v<12>, // power armor (if any) is active

  FL_MECHANICAL = bit_v<13>, // entity is mechanical, use sparks not blood
  FL_SAM_RAIMI = bit_v<14>,  // entity is in sam raimi cam mode
  FL_DISGUISED =
      bit_v<15>,        // entity is in disguise, monsters will not recognize.
  FL_NOGIB = bit_v<16>, // player has been vaporized by a nuke, drop no gibs
  FL_DAMAGEABLE = bit_v<17>,
  FL_STATIONARY = bit_v<18>,

  FL_ALIVE_KNOCKBACK_ONLY =
      bit_v<19>, // only apply knockback if alive or on same frame as death
  FL_NO_DAMAGE_EFFECTS = bit_v<20>,

  // [Paril-KEX] gets scaled by coop health scaling
  FL_COOP_HEALTH_SCALE = bit_v<21>,
  FL_FLASHLIGHT = bit_v<22>,    // enable flashlight
  FL_KILL_VELOCITY = bit_v<23>, // for berserker slam
  FL_NOVISIBLE = bit_v<24>,     // super invisibility
  FL_DODGE = bit_v<25>,         // monster should try to dodge this
  FL_TEAMMASTER =
      bit_v<26>,         // is a team master (only here so that entities abusing
                         // teamMaster/teamChain for stuff don't break)
  FL_LOCKED = bit_v<27>, // entity is locked for the purposes of navigation
  FL_ALWAYS_TOUCH = bit_v<28>, // always touch, even if we normally wouldn't
  FL_NO_STANDING = bit_v<29>,  // don't allow "standing" on non-brush entities
  FL_WANTS_POWER_ARMOR = bit_v<30>, // for players, auto-shield

  FL_RESPAWN = bit_v<31>, // used for item respawning
  FL_TRAP = bit_v<32>,    // entity is a trap of some kind
  FL_TRAP_LASER_FIELD =
      bit_v<33>,           // enough of a special case to get it's own flag...
  FL_IMMORTAL = bit_v<34>, // never go below 1hp

  FL_NO_BOTS = bit_v<35>,   // not to be used by bots
  FL_NO_HUMANS = bit_v<36>, // not to be used by humans
};
MAKE_ENUM_BITFLAGS(ent_flags_t);

// Item->flags
enum item_flags_t : uint32_t {
  IF_NONE = 0,

  IF_WEAPON = bit_v<0>, // use makes active weapon
  IF_AMMO = bit_v<1>,
  IF_ARMOR = bit_v<2>,
  IF_STAY_COOP = bit_v<3>,
  IF_KEY = bit_v<4>,
  IF_TIMED = bit_v<5>,        // minor powerup items
  IF_NOT_GIVEABLE = bit_v<6>, // item can not be given
  IF_HEALTH = bit_v<7>,
  IF_TECH = bit_v<8>,
  IF_NO_HASTE = bit_v<9>,
  IF_NO_INFINITE_AMMO =
      bit_v<10>, // [Paril-KEX] don't allow infinite ammo to affect
  IF_POWERUP_WHEEL = bit_v<11>, // [Paril-KEX] item should be in powerup wheel
  IF_POWERUP_ONOFF = bit_v<12>, // [Paril-KEX] for wheel; can't store more than
                                // one, show on/off state
  IF_NOT_RANDOM =
      bit_v<13>, // [Paril-KEX] item never shows up in randomizations
  IF_POWER_ARMOR = bit_v<14>,
  IF_POWERUP = bit_v<15>,
  IF_SPHERE = bit_v<16>,

  IF_ANY = 0xFFFFFFFF
};

MAKE_ENUM_BITFLAGS(item_flags_t);
constexpr item_flags_t IF_TYPE_MASK =
    (IF_WEAPON | IF_AMMO | IF_TIMED | IF_POWERUP | IF_SPHERE | IF_ARMOR |
     IF_POWER_ARMOR | IF_KEY);

// health gentity_t->style
enum { HEALTH_IGNORE_MAX = 1, HEALTH_TIMED = 2 };

// item IDs; must match itemList order
enum item_id_t : uint8_t {
  IT_NULL, // must always be zero

  IT_ARMOR_BODY,
  IT_ARMOR_COMBAT,
  IT_ARMOR_JACKET,
  IT_ARMOR_SHARD,

  IT_POWER_SCREEN,
  IT_POWER_SHIELD,

  IT_WEAPON_GRAPPLE,
  IT_WEAPON_BLASTER,
  IT_WEAPON_CHAINFIST,
  IT_WEAPON_SHOTGUN,
  IT_WEAPON_SSHOTGUN,
  IT_WEAPON_MACHINEGUN,
  IT_WEAPON_ETF_RIFLE,
  IT_WEAPON_CHAINGUN,
  IT_AMMO_GRENADES,
  IT_AMMO_TRAP,
  IT_AMMO_TESLA,
  IT_WEAPON_GLAUNCHER,
  IT_WEAPON_PROXLAUNCHER,
  IT_WEAPON_RLAUNCHER,
  IT_WEAPON_HYPERBLASTER,
  IT_WEAPON_IONRIPPER,
  IT_WEAPON_PLASMAGUN,
  IT_WEAPON_PLASMABEAM,
  IT_WEAPON_THUNDERBOLT,
  IT_WEAPON_RAILGUN,
  IT_WEAPON_PHALANX,
  IT_WEAPON_BFG,
  IT_WEAPON_DISRUPTOR,

  IT_AMMO_SHELLS,
  IT_AMMO_BULLETS,
  IT_AMMO_CELLS,
  IT_AMMO_ROCKETS,
  IT_AMMO_SLUGS,
  IT_AMMO_MAGSLUG,
  IT_AMMO_FLECHETTES,
  IT_AMMO_PROX,
  IT_AMMO_NUKE,
  IT_AMMO_ROUNDS,

  IT_POWERUP_QUAD,
  IT_POWERUP_HASTE,
  IT_POWERUP_BATTLESUIT,
  IT_POWERUP_INVISIBILITY,
  IT_POWERUP_SILENCER,
  IT_POWERUP_REBREATHER,
  IT_POWERUP_ENVIROSUIT,
  IT_POWERUP_EMPATHY_SHIELD,
  IT_POWERUP_ANTIGRAV_BELT,
  IT_ANCIENT_HEAD,
  IT_LEGACY_HEAD,
  IT_ADRENALINE,
  IT_BANDOLIER,
  IT_PACK,
  IT_IR_GOGGLES,
  IT_POWERUP_DOUBLE,
  IT_POWERUP_SPHERE_VENGEANCE,
  IT_POWERUP_SPHERE_HUNTER,
  IT_POWERUP_SPHERE_DEFENDER,
  IT_DOPPELGANGER,
  IT_TAG_TOKEN,

  IT_KEY_DATA_CD,
  IT_KEY_POWER_CUBE,
  IT_KEY_EXPLOSIVE_CHARGES,
  IT_KEY_YELLOW,
  IT_KEY_POWER_CORE,
  IT_KEY_PYRAMID,
  IT_KEY_DATA_SPINNER,
  IT_KEY_PASS,
  IT_KEY_BLUE_KEY,
  IT_KEY_RED_KEY,
  IT_KEY_GREEN_KEY,
  IT_KEY_COMMANDER_HEAD,
  IT_KEY_AIRSTRIKE,
  IT_KEY_NUKE_CONTAINER,
  IT_KEY_NUKE,

  IT_HEALTH_SMALL,
  IT_HEALTH_MEDIUM,
  IT_HEALTH_LARGE,
  IT_HEALTH_MEGA,

  IT_FLAG_RED,
  IT_FLAG_BLUE,
  IT_FLAG_NEUTRAL,

  IT_TECH_DISRUPTOR_SHIELD,
  IT_TECH_POWER_AMP,
  IT_TECH_TIME_ACCEL,
  IT_TECH_AUTODOC,

  IT_AMMO_SHELLS_LARGE,
  IT_AMMO_SHELLS_SMALL,
  IT_AMMO_BULLETS_LARGE,
  IT_AMMO_BULLETS_SMALL,
  IT_AMMO_CELLS_LARGE,
  IT_AMMO_CELLS_SMALL,
  IT_AMMO_ROCKETS_SMALL,
  IT_AMMO_SLUGS_LARGE,
  IT_AMMO_SLUGS_SMALL,

  IT_TELEPORTER,
  IT_POWERUP_REGEN,

  IT_FOODCUBE,

  IT_BALL,
  IT_POWERUP_SPAWN_PROTECTION,

  IT_FLASHLIGHT,
  IT_COMPASS,

  IT_HARVESTER_SKULL,

  IT_TOTAL
};

enum class PowerupTimer : uint8_t {
  QuadDamage,
  DoubleDamage,
  BattleSuit,
  Rebreather,
  Invisibility,
  Haste,
  Regeneration,
  EnviroSuit,
  EmpathyShield,
  AntiGravBelt,
  SpawnProtection,
  IrGoggles,

  Count
};

constexpr size_t PowerupTimerCount = static_cast<size_t>(PowerupTimer::Count);

enum class PowerupCount : uint8_t {
  SilencerShots,

  Count
};

constexpr size_t PowerupCountCount = static_cast<size_t>(PowerupCount::Count);

constexpr size_t ToIndex(PowerupTimer timer) {
  return static_cast<size_t>(timer);
}

constexpr size_t ToIndex(PowerupCount counter) {
  return static_cast<size_t>(counter);
}

constexpr std::optional<PowerupTimer> PowerupTimerForItem(item_id_t item) {
  switch (item) {
  case IT_POWERUP_QUAD:
    return PowerupTimer::QuadDamage;
  case IT_POWERUP_DOUBLE:
    return PowerupTimer::DoubleDamage;
  case IT_POWERUP_BATTLESUIT:
    return PowerupTimer::BattleSuit;
  case IT_POWERUP_REBREATHER:
    return PowerupTimer::Rebreather;
  case IT_POWERUP_INVISIBILITY:
    return PowerupTimer::Invisibility;
  case IT_POWERUP_HASTE:
    return PowerupTimer::Haste;
  case IT_POWERUP_REGEN:
    return PowerupTimer::Regeneration;
  case IT_POWERUP_ENVIROSUIT:
    return PowerupTimer::EnviroSuit;
  case IT_POWERUP_EMPATHY_SHIELD:
    return PowerupTimer::EmpathyShield;
  case IT_POWERUP_ANTIGRAV_BELT:
    return PowerupTimer::AntiGravBelt;
  case IT_POWERUP_SPAWN_PROTECTION:
    return PowerupTimer::SpawnProtection;
  case IT_IR_GOGGLES:
    return PowerupTimer::IrGoggles;
  default:
    break;
  }

  return std::nullopt;
}

constexpr std::optional<PowerupCount> PowerupCountForItem(item_id_t item) {
  switch (item) {
  case IT_POWERUP_SILENCER:
    return PowerupCount::SilencerShots;
  default:
    break;
  }

  return std::nullopt;
}

enum class HighValueItems : uint8_t {
  None,

  MegaHealth,
  BodyArmor,
  CombatArmor,
  PowerShield,
  PowerScreen,
  Adrenaline,
  QuadDamage,
  DoubleDamage,
  Invisibility,
  Haste,
  Regeneration,
  BattleSuit,
  EmpathyShield,
  AmmoPack,
  Bandolier,

  Total
};

static constexpr std::array<const char *,
                            static_cast<size_t>(HighValueItems::Total)>
    HighValueItemNames = {"",
                          "MegaHealth",
                          "BodyArmor",
                          "CombatArmor",
                          "PowerShield",
                          "PowerScreen",
                          "Adrenaline",
                          "QuadDamage",
                          "DoubleDamage",
                          "Invisibility",
                          "Haste",
                          "Regeneration",
                          "BattleSuit",
                          "EmpathyShield",
                          "AmmoPack",
                          "Bandolier"};

static_assert(HighValueItemNames.size() ==
                  static_cast<size_t>(HighValueItems::Total),
              "HighValueItemNames must match HighValueItems enum length");

constexpr int FIRST_WEAPON = IT_WEAPON_GRAPPLE;
constexpr int LAST_WEAPON = IT_WEAPON_DISRUPTOR;

constexpr item_id_t tech_ids[] = {IT_TECH_DISRUPTOR_SHIELD, IT_TECH_POWER_AMP,
                                  IT_TECH_TIME_ACCEL, IT_TECH_AUTODOC};

constexpr const char *ITEM_CTF_FLAG_RED = "item_flag_team_red";
constexpr const char *ITEM_CTF_FLAG_BLUE = "item_flag_team_blue";
constexpr const char *ITEM_CTF_FLAG_NEUTRAL = "item_flag_team_neutral";

struct Item {
  item_id_t id;                    // matches item list index
  const char *className = nullptr; // spawning name
  bool (*pickup)(gentity_t *ent, gentity_t *other);
  void (*use)(gentity_t *ent, Item *item);
  void (*drop)(gentity_t *ent, Item *item);
  void (*weaponThink)(gentity_t *ent);
  const char *pickupSound;
  const char *worldModel;
  Effect worldModelFlags;
  const char *viewModel;

  // client side info
  const char *icon;
  const char *useName;              // for use command, english only
  const char *pickupName;           // for printing on pickup
  const char *pickupNameDefinitive; // definite article version for languages
                                    // that need it

  int quantity = 0; // for ammo how much, for weapons how much is used per shot
  item_id_t ammo = IT_NULL;     // for weapons
  item_id_t chain = IT_NULL;    // weapon chain root
  item_flags_t flags = IF_NONE; // IT_* flags

  const char *viewWeaponModel = nullptr; // vwep model string (for weapons)

  const gitem_armor_t *armorInfo = nullptr;
  int tag = 0;

  HighValueItems highValue;

  std::string_view precaches{}; // was: const char* precaches

  int32_t sortID = 0;       // used by some items to control their sorting
  int32_t quantityWarn = 5; // when to warn on low ammo

  // set in InitItems, don't set by hand
  // circular list of chained weapons
  Item *chainNext = nullptr;

  // set in SP_worldspawn, don't set by hand
  // model index for vwep
  int32_t viewWeaponIndex = 0;

  // set in SetItemNames, don't set by hand
  // offset into CS_WHEEL_AMMO/CS_WHEEL_WEAPONS/CS_WHEEL_POWERUPS
  int32_t ammoWheelIndex = -1;
  int32_t weaponWheelIndex = -1;
  int32_t powerupWheelIndex = -1;

  bool precached = false; // new: prevents duplicate precaches
};

using gitem_t = Item;

// means of death
enum class ModID : uint8_t {
  Unknown,
  Blaster,
  Shotgun,
  SuperShotgun,
  Machinegun,
  Chaingun,
  GrenadeLauncher,
  GrenadeLauncher_Splash,
  RocketLauncher,
  RocketLauncher_Splash,
  HyperBlaster,
  Railgun,
  BFG10K_Laser,
  BFG10K_Blast,
  BFG10K_Effect,
  HandGrenade,
  HandGrenade_Splash,
  HandGrenade_Held,
  Drowning,
  Slime,
  Lava,
  Crushed,
  Telefragged,
  Telefrag_Spawn,
  FallDamage,
  Suicide,
  Explosives,
  Barrel,
  Bomb,
  ExitLevel,
  Splash,
  Laser,
  Hurt,
  Hit,
  ShooterBlaster,
  IonRipper,
  PlasmaGun,
  PlasmaGun_Splash,
  Phalanx,
  BrainTentacle,
  BlastOff,
  Gekk,
  Trap,
  Chainfist,
  Disruptor,
  ETFRifle,
  Blaster2,
  PlasmaBeam,
  Thunderbolt,
  Thunderbolt_Discharge,
  TeslaMine,
  ProxMine,
  Nuke,
  VengeanceSphere,
  HunterSphere,
  DefenderSphere,
  Tracker,
  Thaw,
  Doppelganger_Explode,
  Doppelganger_Vengeance,
  Doppelganger_Hunter,
  GrapplingHook,
  BlueBlaster,
  Railgun_Splash,
  Expiration,
  Silent,

  Total
};

struct MeansOfDeath {
  ModID id;
  bool friendly_fire = false;
  bool no_point_loss = false;

  MeansOfDeath() = default;
  constexpr MeansOfDeath(ModID id, bool no_point_loss = false)
      : id(id), no_point_loss(no_point_loss) {}
};

struct mod_remap {
  ModID mod;
  Weapon weapon;
  std::string name;
};

const std::array<mod_remap, static_cast<uint8_t>(ModID::Total)> modr = {{
    {ModID::Unknown, Weapon::None, "Unknown"},
    {ModID::Blaster, Weapon::Blaster, "Blaster"},
    {ModID::Shotgun, Weapon::Shotgun, "Shotgun"},
    {ModID::SuperShotgun, Weapon::SuperShotgun, "Super Shotgun"},
    {ModID::Machinegun, Weapon::Machinegun, "Machinegun"},
    {ModID::Chaingun, Weapon::Chaingun, "Chaingun"},
    {ModID::GrenadeLauncher, Weapon::GrenadeLauncher, "Grenade Impact"},
    {ModID::GrenadeLauncher_Splash, Weapon::GrenadeLauncher, "Grenade Splash"},
    {ModID::RocketLauncher, Weapon::RocketLauncher, "Rocket Impact"},
    {ModID::RocketLauncher_Splash, Weapon::RocketLauncher, "Rocket Splash"},
    {ModID::HyperBlaster, Weapon::HyperBlaster, "HyperBlaster"},
    {ModID::Railgun, Weapon::Railgun, "Railgun"},
    {ModID::BFG10K_Laser, Weapon::BFG10K, "BFG Laser"},
    {ModID::BFG10K_Blast, Weapon::BFG10K, "BFG Blast"},
    {ModID::BFG10K_Effect, Weapon::BFG10K, "BFG Core"},
    {ModID::HandGrenade, Weapon::HandGrenades, "Hand Grenade Impact"},
    {ModID::HandGrenade_Splash, Weapon::HandGrenades, "Hand Grenade Splash"},
    {ModID::HandGrenade_Held, Weapon::None, "Held Grenade Explosion"},
    {ModID::Drowning, Weapon::None, "Drowning"},
    {ModID::Slime, Weapon::None, "Slime"},
    {ModID::Lava, Weapon::None, "Lava"},
    {ModID::Crushed, Weapon::None, "Crushed"},
    {ModID::Telefragged, Weapon::None, "Telefrag"},
    {ModID::Telefrag_Spawn, Weapon::None, "Telefrag (Spawn)"},
    {ModID::FallDamage, Weapon::None, "Falling"},
    {ModID::Suicide, Weapon::None, "Suicide"},
    {ModID::Explosives, Weapon::None, "Explosion"},
    {ModID::Barrel, Weapon::None, "Barrel"},
    {ModID::Bomb, Weapon::None, "Bomb"},
    {ModID::ExitLevel, Weapon::None, "Exit"},
    {ModID::Splash, Weapon::None, "Splash Damage"},
    {ModID::Laser, Weapon::None, "Target Laser"},
    {ModID::Hurt, Weapon::None, "Trigger Hurt"},
    {ModID::Hit, Weapon::None, "Hit"},
    {ModID::ShooterBlaster, Weapon::None, "Target Blaster"},
    {ModID::IonRipper, Weapon::IonRipper, "Ion Ripper"},
    {ModID::PlasmaGun, Weapon::PlasmaGun, "Plasma Gun"},
    {ModID::PlasmaGun_Splash, Weapon::PlasmaGun, "Plasma Gun Splash"},
    {ModID::Phalanx, Weapon::Phalanx, "Phalanx"},
    {ModID::BrainTentacle, Weapon::None, "Brain Tentacle"},
    {ModID::BlastOff, Weapon::None, "Blast Off"},
    {ModID::Gekk, Weapon::None, "Gekk"},
    {ModID::Trap, Weapon::Trap, "Trap"},
    {ModID::Chainfist, Weapon::Chainfist, "Chainfist"},
    {ModID::Disruptor, Weapon::Disruptor, "Disruptor"},
    {ModID::ETFRifle, Weapon::ETFRifle, "ETF Rifle"},
    {ModID::Blaster2, Weapon::None, "Blaster 2"},
    {ModID::PlasmaBeam, Weapon::PlasmaBeam, "Plasma Beam"},
    {ModID::Thunderbolt, Weapon::Thunderbolt, "Thunderbolt"},
    {ModID::Thunderbolt_Discharge, Weapon::Thunderbolt, "Thunderbolt"},
    {ModID::TeslaMine, Weapon::TeslaMine, "Tesla"},
    {ModID::ProxMine, Weapon::ProxLauncher, "Proximity Mine"},
    {ModID::Nuke, Weapon::None, "Nuke"},
    {ModID::VengeanceSphere, Weapon::None, "Vengeance Sphere"},
    {ModID::HunterSphere, Weapon::None, "Hunter Sphere"},
    {ModID::DefenderSphere, Weapon::None, "Defender Sphere"},
    {ModID::Tracker, Weapon::None, "Tracker"},
    {ModID::Thaw, Weapon::None, "Thaw"},
    {ModID::Doppelganger_Explode, Weapon::None, "Doppelganger Explosion"},
    {ModID::Doppelganger_Vengeance, Weapon::None, "Doppelganger Vengeance"},
    {ModID::Doppelganger_Hunter, Weapon::None, "Doppelganger Hunter"},
    {ModID::GrapplingHook, Weapon::GrapplingHook, "Grapple"},
    {ModID::BlueBlaster, Weapon::None, "Blue Blaster"},
    {ModID::Railgun_Splash, Weapon::Railgun, "Railgun Splash"},
    {ModID::Expiration, Weapon::None, "Expire"},
    {ModID::Silent, Weapon::None, "Change Team"},
}};

// the total number of levels we'll track for the
// end of unit screen.
constexpr size_t MAX_LEVELS_PER_UNIT = 8;

struct LevelEntry {
  // bsp name
  std::array<char, MAX_QPATH> mapName{};
  // map name
  std::array<char, MAX_QPATH> longMapName{};
  // these are set when we leave the level
  int32_t totalSecrets;
  int32_t foundSecrets;
  int32_t totalMonsters;
  int32_t killedMonsters;
  // total time spent in the level, for end screen
  GameTime time;
  // the order we visited levels in
  int32_t visit_order;
};

struct game_mapqueue_t {
  std::string map;
  std::uint8_t item_inhibit_pu;
  std::uint8_t item_inhibit_pa;
  std::uint8_t item_inhibit_ht;
  std::uint8_t item_inhibit_ar;
  std::uint8_t item_inhibit_am;
  std::uint8_t item_inhibit_wp;
};

// === MAP SYSTEM ===

enum MapTypeFlags : uint8_t {
  MAP_DM = 1 << 0,  // deathmatch
  MAP_SP = 1 << 1,  // singleplayer
  MAP_COOP = 1 << 2 // coop
};

struct MapEntry {
  std::string filename;                        // Required
  std::string longName;                        // Optional
  int minPlayers = -1;                         // Optional
  int maxPlayers = -1;                         // Optional
  GameType suggestedGametype = GameType::None; // Optional
  Ruleset suggestedRuleset = Ruleset::None;    // Optional
  int scoreLimit = -1;                         // Optional
  int timeLimit = -1;                          // Optional
  bool isPopular = false;                      // Optional
  bool isCustom = false;                       // Optional
  bool isCycleable = false;                    // Assigned after cycle load
  bool hasCustomTextures = false;              // Optional
  bool hasCustomSounds = false;                // Optional
  uint8_t mapTypeFlags = 0;                    // MAP_DM | MAP_SP | MAP_COOP
  int64_t lastPlayed = 0; // Seconds since server start when last played
  bool preferredTDM = false;
  bool preferredCTF = false;
  bool preferredDuel = false;
};

inline void ApplyCustomResourceFlags(MapEntry &map, bool customFlag,
                                     bool hasCustomTextures,
                                     bool hasCustomSounds) {
  map.hasCustomTextures = hasCustomTextures;
  map.hasCustomSounds = hasCustomSounds;
  map.isCustom = customFlag || hasCustomTextures || hasCustomSounds;
}

inline bool ShouldAvoidCustomResources(const MapEntry &map, bool avoidCustom,
                                       bool avoidCustomTextures,
                                       bool avoidCustomSounds) {
  if (avoidCustom && map.isCustom)
    return true;
  if (avoidCustomTextures && map.hasCustomTextures)
    return true;
  if (avoidCustomSounds && map.hasCustomSounds)
    return true;
  return false;
}

enum MyMapOverride : uint16_t {
  MAPFLAG_NONE = 0,
  MAPFLAG_PU = 1 << 0,  // powerups
  MAPFLAG_PA = 1 << 1,  // power armor
  MAPFLAG_AR = 1 << 2,  // armor
  MAPFLAG_AM = 1 << 3,  // ammo
  MAPFLAG_HT = 1 << 4,  // health
  MAPFLAG_BFG = 1 << 5, // bfg
  MAPFLAG_PB = 1 << 6,  // plasma beam
  MAPFLAG_FD = 1 << 7,  // fall damage
  MAPFLAG_SD = 1 << 8,  // self damage
  MAPFLAG_WS = 1 << 9   // weapons stay
};

struct MyMapRequest {
  std::string mapName = {};
  std::string socialID = {};
  uint16_t enableFlags = 0;
  uint16_t disableFlags = 0;
  GameTime queuedTime = 0_sec;
};

struct QueuedMap {
  std::string filename;
  std::string socialID; // One-per-client rule
  uint16_t enableFlags = 0;
  uint16_t disableFlags = 0;
};

struct MapSystem {
  std::vector<MapEntry> mapPool;
  std::vector<QueuedMap> playQueue;
  std::vector<MyMapRequest> myMapQueue;

  static constexpr int DEFAULT_MYMAP_QUEUE_LIMIT = 8;

  struct MyMapEnqueueResult {
    bool accepted = false;
    bool evictedOldest = false;
  };

  bool MapExists(std::string_view mapName) const;

  bool IsMapInQueue(const std::string &mapName) const;
  bool IsClientInQueue(const std::string &socialID) const;

  void
  PruneQueuesToMapPool(std::vector<std::string> *removedRequests = nullptr);

  MyMapEnqueueResult EnqueueMyMapRequest(const MapEntry &map,
                                         std::string_view socialID,
                                         uint16_t enableFlags,
                                         uint16_t disableFlags,
                                         GameTime queuedTime);

  void ConsumeQueuedMap();

  const MapEntry *GetMapEntry(const std::string &mapName) const;
};

extern cvar_t *g_maps_pool_file;
extern cvar_t *g_maps_mymap_queue_limit;

struct MapPoolLocation {
  std::string path;
  bool loadedFromMod = false;
  bool exists = false;
};

extern cvar_t *g_maps_pool_file;
extern cvar_t *g_maps_mymap_queue_limit;

/*
=============
G_ResolveMapPoolPath

Selects the map pool JSON path, preferring the active gamedir when it
contains the configured file and falling back to GAMEVERSION otherwise.
=============
*/

inline MapPoolLocation G_ResolveMapPoolPath() {
  const char *defaultPoolFile = "mapdb.json";
  const char *poolFile = (g_maps_pool_file && g_maps_pool_file->string)
                             ? g_maps_pool_file->string
                             : "";

  std::string sanitizedPoolFile;
  std::string rejectReason;
  if (!G_SanitizeMapConfigFilename(poolFile, sanitizedPoolFile, rejectReason)) {
    gi.Com_PrintFmt(
        "{}: invalid g_maps_pool_file \"{}\" ({}) falling back to {}\n",
        __FUNCTION__, poolFile, rejectReason.c_str(), defaultPoolFile);
    sanitizedPoolFile = defaultPoolFile;
  }

  const std::string basePath =
      std::string(GAMEVERSION) + "/" + sanitizedPoolFile;

  if (gi.cvar) {
    cvar_t *gameCvar = gi.cvar("game", "", CVAR_NOFLAGS);
    if (gameCvar && gameCvar->string && gameCvar->string[0]) {
      const std::string modPath =
          std::string(gameCvar->string) + "/" + sanitizedPoolFile;
      std::ifstream file(modPath, std::ifstream::binary);
      if (file.is_open())
        return {modPath, true, true};
    }
  }

  std::ifstream baseFile(basePath, std::ifstream::binary);
  if (!baseFile.is_open()) {
    gi.Com_PrintFmt("{}: map pool file '{}' not found.\n", __FUNCTION__,
                    basePath.c_str());
    return {basePath, false, false};
  }

  return {basePath, false, true};
}

/*
=============
G_ResolveMapCyclePath

Selects the map cycle path, preferring the active gamedir when it contains
the configured file and falling back to GAMEVERSION otherwise.
=============
*/
inline MapPoolLocation G_ResolveMapCyclePath(const std::string &cycleFile) {
  const std::string basePath = std::string(GAMEVERSION) + "/" + cycleFile;

  if (gi.cvar) {
    cvar_t *gameCvar = gi.cvar("game", "", CVAR_NOFLAGS);
    if (gameCvar && gameCvar->string && gameCvar->string[0]) {
      const std::string modPath =
          std::string(gameCvar->string) + "/" + cycleFile;
      std::ifstream file(modPath, std::ifstream::binary);
      if (file.is_open())
        return {modPath, true};
    }
  }

  return {basePath, false};
}

/*
===============
MapSystem::MapExists

Checks whether a map BSP file exists within the active gamedir's maps
directory, falling back to the default GAMEVERSION path when no mod is
active. Returns true if the BSP can be opened from any applicable search
path.
===============
*/
inline bool MapSystem::MapExists(std::string_view mapName) const {
  if (!G_IsValidMapIdentifier(mapName)) {
    gi.Com_PrintFmt("{}: rejected invalid map identifier \"{}\"\n",
                    __FUNCTION__, std::string(mapName).c_str());
    return false;
  }

  const std::string bspName = std::string(mapName) + ".bsp";
  const auto mapExistsInDir = [&](std::string_view gamedir) -> bool {
    if (gamedir.empty())
      return false;

    std::filesystem::path candidate(gamedir);
    candidate /= "maps";
    candidate /= bspName;
    std::ifstream file(candidate, std::ifstream::binary);
    return file.is_open();
  };

  std::string activeGameDir;
  if (gi.cvar) {
    cvar_t *gameCvar = gi.cvar("game", "", CVAR_NOFLAGS);
    if (gameCvar && gameCvar->string && gameCvar->string[0])
      activeGameDir = gameCvar->string;
  }

  if (!activeGameDir.empty() && mapExistsInDir(activeGameDir))
    return true;

  if (mapExistsInDir(GAMEVERSION))
    return true;

  return false;
}

/*
=============
MapSystem::ConsumeQueuedMap

Removes the leading entries from both the play queue and the mymap queue if
they are present.
=============
*/
inline void MapSystem::ConsumeQueuedMap() {
  if (!playQueue.empty())
    playQueue.erase(playQueue.begin());

  if (!myMapQueue.empty())
    myMapQueue.erase(myMapQueue.begin());
}

/*
========================
MapSystem::EnqueueMyMapRequest

Adds a MyMap request to both the play queue and the persistent
MyMap request log, preserving flag overrides and request metadata.
Respects g_maps_mymap_queue_limit to cap queue size, evicting the
oldest entry when full or rejecting requests when the limit is
disabled. Returns the operation outcome flags.
========================
*/
inline MapSystem::MyMapEnqueueResult
MapSystem::EnqueueMyMapRequest(const MapEntry &map, std::string_view socialID,
                               uint16_t enableFlags, uint16_t disableFlags,
                               GameTime queuedTime) {
  MyMapEnqueueResult result{};

  const int maxQueue = g_maps_mymap_queue_limit
                           ? g_maps_mymap_queue_limit->integer
                           : DEFAULT_MYMAP_QUEUE_LIMIT;
  if (maxQueue <= 0) {
    gi.Com_PrintFmt("{}: rejected MyMap request for '{}' because the queue "
                    "limit is disabled (<= 0)\n",
                    __FUNCTION__, map.filename.c_str());
    return result;
  }

  if (playQueue.size() >= static_cast<size_t>(maxQueue)) {
    if (!playQueue.empty()) {
      const auto &evicted = playQueue.front();
      gi.Com_PrintFmt("{}: MyMap queue full ({}). Evicting '{}'.\n",
                      __FUNCTION__, maxQueue, evicted.filename.c_str());
      playQueue.erase(playQueue.begin());
      if (!myMapQueue.empty())
        myMapQueue.erase(myMapQueue.begin());
      result.evictedOldest = true;
    }
  }

  QueuedMap queued{};
  queued.filename = map.filename;
  queued.socialID.assign(socialID.begin(), socialID.end());
  queued.enableFlags = enableFlags;
  queued.disableFlags = disableFlags;
  playQueue.push_back(std::move(queued));

  MyMapRequest request{};
  request.mapName = map.filename;
  request.socialID.assign(socialID.begin(), socialID.end());
  request.enableFlags = enableFlags;
  request.disableFlags = disableFlags;
  request.queuedTime = queuedTime;
  myMapQueue.push_back(std::move(request));

  result.accepted = true;
  return result;
}

struct HelpMessage {
  std::array<char, MAX_TOKEN_CHARS> message{};
  int32_t modificationCount = 0;

  [[nodiscard]] std::string_view view() const {
    return std::string_view(message.data());
  }

  [[nodiscard]] bool empty() const { return message[0] == '\0'; }
};

enum class GameCheatFlags : uint32_t {
  None = 0,
  Fly = bit_v<0>,
};

MAKE_ENUM_BITFLAGS(GameCheatFlags);

//
// this structure is left intact through an entire game
// it should be initialized at dll load time, and read/written to
// the server.ssv file for savegames
//
struct GameLocals {
  GameCheatFlags cheatsFlag = GameCheatFlags::None;
  HelpMessage help[2]{};

  gclient_t *clients; // [maxClients]

  // can't store spawnpoint in level, because
  // it would get overwritten by the savegame restore
  std::array<char, MAX_TOKEN_CHARS> spawnPoint; // needed for coop respawns

  // store latched cvars here that we want to get at often
  uint32_t maxClients = MAX_CLIENTS;
  uint32_t maxEntities = MAX_ENTITIES;

  // cross level triggers
  uint32_t crossLevelFlags = 0, crossUnitFlags = 0;

  bool autoSaved = false;

  // [Paril-KEX]
  uint32_t gravity_modCount = 0;
  std::array<LevelEntry, MAX_LEVELS_PER_UNIT> levelEntries{};
  int32_t maxLagOrigins = 0;
  Vector3 *lagOrigins{}; // maxClients * maxLagOrigins

  GameType gametype = GameType::None; // current gametype
  std::string motd = "";              // message of the day
  int motdModificationCount = 0;      // used to detect changes

  Ruleset ruleset = Ruleset::None; // current ruleset

  int8_t item_inhibit_pu;
  int8_t item_inhibit_pa;
  int8_t item_inhibit_ht;
  int8_t item_inhibit_ar;
  int8_t item_inhibit_am;
  int8_t item_inhibit_wp;

  // new map system stuff
  struct {
    bool spawnPowerups = true;
    bool spawnPowerArmor = true;
    bool spawnArmor = true;
    bool spawnHealth = true;
    bool spawnAmmo = true;
    bool spawnBFG = true;
    bool spawnPlasmaBeam = true;

    bool fallingDamage = true;
    bool selfDamage = true;
    bool weaponsStay = false;

    uint16_t overrideEnableFlags = 0;
    uint16_t overrideDisableFlags = 0;
  } map;

  MapSystem mapSystem = {};

  struct MarathonState {
    bool active = false;
    uint32_t legIndex = 0;
    bool transitionPending = false;
    GameTime totalElapsedBeforeCurrentMap = 0_ms;
    GameTime mapStartTime = 0_ms;
    std::array<int, MAX_CLIENTS> mapStartPlayerScores{};
    std::array<bool, MAX_CLIENTS> mapStartScoreValid{};
    std::array<int, static_cast<int>(Team::Total)> mapStartTeamScores{};
    std::array<int, static_cast<int>(Team::Total)> cumulativeTeamScores{};
    std::string matchID{};
  } marathon{};

  struct TournamentState {
    bool configLoaded = false;
    bool configValid = false;
    bool active = false;
    bool seriesComplete = false;
    int bestOf = 1;
    int winTarget = 1;
    bool teamBased = false;
    GameType gametype = GameType::None;
    int gamesPlayed = 0;
    std::array<int, static_cast<size_t>(Team::Total)> teamWins{};
    std::array<std::string, MAX_CLIENTS> playerIds{};
    std::array<std::string, MAX_CLIENTS> playerNames{};
    std::array<int, MAX_CLIENTS> playerWins{};
    std::string configFile{};
    std::string name{};
    std::string seriesId{};
    std::string homeId{};
    std::string awayId{};
    Team homeTeam = Team::None;
    Team awayTeam = Team::None;
    std::string matchLength{};
    std::string matchType{};
    std::string matchModifier{};
    std::string matchBestOfKey{};
    int maxPlayers = 0;
    std::array<std::string, static_cast<size_t>(Team::Total)> teamNames{};
    std::array<std::string, static_cast<size_t>(Team::Total)> teamCaptains{};
    std::vector<TournamentParticipant> participants{};
    std::vector<std::string> mapPool{};
    std::vector<std::string> mapBans{};
    std::vector<std::string> mapPicks{};
    std::vector<std::string> mapOrder{};
    std::vector<TournamentVetoStep> vetoSteps{};
    bool vetoStarted = false;
    bool vetoComplete = false;
    size_t vetoIndex = 0;
    bool vetoHomeTurn = true;
    bool pendingMapChange = false;
    bool autoTimeoutActive = false;
    std::vector<std::string> matchIds{};
    std::vector<std::string> matchMaps{};
    std::vector<std::string> matchWinners{};
  } tournament{};

  time_t serverStartTime = 0;

  std::mt19937 mapRNG;

  std::unordered_set<std::string> bannedIDs{};
  std::unordered_set<std::string> adminIDs{};
};

constexpr size_t MAX_HEALTH_BARS = 2;

enum class VotingType { None, Match, Admin, Map };

struct PlayerTeamState {
  GameTime returned_flag_time;
  GameTime flag_pickup_time;
  GameTime fragged_carrier_time;

  int location;

  int base_defense;
  int carrier_defense;
  int frag_recovery;
  int frag_carrier;

  int hurt_carrier_time;
};

struct PlayerRef {
  std::string name;
  std::string id;
};

struct MatchEvent {
  GameTime time;
  std::string eventStr;
};

struct MatchDeathEvent {
  GameTime time;
  PlayerRef victim;
  PlayerRef attacker;
  MeansOfDeath mod;
};

struct MatchOverallStats {
  uint32_t totalKills{0};
  uint32_t totalDeaths{0};
  uint32_t totalSuicides{0};
  uint32_t totalTeamKills{0};
  uint32_t totalSpawnKills{0};
  uint32_t proBallGoals{0};
  uint32_t proBallAssists{0};

  std::array<uint32_t, static_cast<size_t>(ModID::Total)> modKills{};
  std::array<uint32_t, static_cast<size_t>(ModID::Total)> modDeaths{};

  std::array<uint32_t, static_cast<size_t>(PlayerMedal::Total)> medalCount{};

  std::vector<MatchDeathEvent> deathLog{};
  std::vector<MatchEvent> eventLog{};

  uint32_t pickupCounts[static_cast<size_t>(HighValueItems::Total)]{};
  GameTime pickupDelay[static_cast<size_t>(HighValueItems::Total)]{};

  // CTF stuff
  int64_t ctfRedFlagTotalHoldTimeMsec = 0;
  int64_t ctfRedFlagShortestHoldTimeMsec = 0;
  int64_t ctfRedFlagLongestHoldTimeMsec = 0;
  int64_t ctfRedFlagPickupCount = 0;
  int64_t ctfRedFlagDropCount = 0;

  int64_t ctfBlueFlagTotalHoldTimeMsec = 0;
  int64_t ctfBlueFlagShortestHoldTimeMsec = 0;
  int64_t ctfBlueFlagLongestHoldTimeMsec = 0;
  int64_t ctfBlueFlagPickupCount = 0;
  int64_t ctfBlueFlagDropCount = 0;

  int64_t ctfRedTeamTotalCaptures = 0;
  int64_t ctfRedTeamTotalDefences = 0;
  int64_t ctfRedTeamTotalAssists = 0;

  int64_t ctfBlueTeamTotalCaptures = 0;
  int64_t ctfBlueTeamTotalDefences = 0;
  int64_t ctfBlueTeamTotalAssists = 0;
};

//
// this structure is cleared as each map is entered
// it is read/written to the level.sav file for savegames
//

struct ClientMatchStats {
  uint32_t lifeAverage;
  uint32_t lifeLongest;

  uint32_t totalDmgDealt;
  uint32_t totalDmgReceived;

  uint32_t totalShots;
  uint32_t totalHits;

  uint32_t proBallGoals;
  uint32_t proBallAssists;

  uint32_t totalKills;
  uint32_t totalTeamKills;
  uint32_t totalSpawnKills;
  uint32_t totalDeaths;
  uint32_t totalSpawnDeaths;
  uint32_t totalSuicides;

  std::array<uint32_t, static_cast<uint8_t>(ModID::Total)> modTotalKills;
  std::array<uint32_t, static_cast<uint8_t>(ModID::Total)> modTotalDeaths;
  std::array<uint32_t, static_cast<uint8_t>(ModID::Total)> modTotalDmgD;
  std::array<uint32_t, static_cast<uint8_t>(ModID::Total)> modTotalDmgR;
  std::array<uint32_t, static_cast<uint8_t>(Weapon::Total)> totalShotsPerWeapon;
  std::array<uint32_t, static_cast<uint8_t>(Weapon::Total)> totalHitsPerWeapon;

  // uint32_t	medalCount[static_cast<uint8_t>(PlayerMedal::Total)];
  std::array<uint32_t, static_cast<size_t>(PlayerMedal::Total)> medalCount{};

  std::array<uint32_t, static_cast<size_t>(HighValueItems::Total)> pickupCounts;
  std::array<GameTime, static_cast<size_t>(HighValueItems::Total)> pickupDelay;

  uint32_t ctfFlagPickups = 0;
  uint32_t ctfFlagDrops = 0;
  uint32_t ctfFlagReturns = 0;
  uint32_t ctfFlagAssists = 0;
  uint32_t ctfFlagCaptures = 0;
  uint64_t ctfFlagCarrierTimeTotalMsec = 0;
  uint32_t ctfFlagCarrierTimeShortestMsec = 0;
  uint32_t ctfFlagCarrierTimeLongestMsec = 0;
};

struct Ghosts {
  char netName[MAX_NETNAME]{};               // ent->client->pers.netName
  char socialID[MAX_INFO_VALUE]{};           // ent->client->sess.socialID
  std::array<int32_t, IT_TOTAL> inventory{}; // ent->client->inventory
  std::array<int16_t, static_cast<int>(AmmoID::_Total)> ammoMax =
      {};                     // ent->client->pers.ammoMax
  ClientMatchStats match{};   // ent->client->sess.match
  Item *weapon = nullptr;     // ent->client->pers.weapon
  Item *lastWeapon = nullptr; // ent->client->pers.lastWeapon
  Team team = Team::None;     // ent->client->sess.team
  int32_t score = 0;          // ent->client->resp.score

  uint16_t skillRating = 0;       // ent->client->sess.skillRating
  uint16_t skillRatingChange = 0; // ent->client->sess.skillRatingChange

  Vector3 origin = vec3_origin; // ent->s.origin
  Vector3 angles = vec3_origin; // ent->s.angles

  int64_t totalMatchPlayRealTime; // accumulated play time in milliseconds
};

struct ShadowLightInfo {
  int entity_number;
  shadow_light_data_t shadowlight;
};

constexpr size_t NUM_SPAWN_SPOTS = 1024; // keep your existing cap or adjust
constexpr size_t SPAWN_SPOT_INTERMISSION = NUM_SPAWN_SPOTS - 1;

// ----------------------------------------------------------------------------
// New spawn containers
// ----------------------------------------------------------------------------
struct SpawnLists {
  std::vector<gentity_t *> ffa;      // info_player_deathmatch
  std::vector<gentity_t *> red;      // info_player_team_red
  std::vector<gentity_t *> blue;     // info_player_team_blue
  gentity_t *intermission = nullptr; // info_player_intermission

  /*
  ===============
  Clear
  ===============
  */
  void Clear() {
    ffa.clear();
    red.clear();
    blue.clear();
    intermission = nullptr;
  }

  /*
  ===============
  Total
  ===============
  */
  size_t Total() const { return ffa.size() + red.size() + blue.size(); }
};

struct LevelLocals {
  bool inFrame = false; // true if G_RunFrame is running
  GameTime time = 0_ms; // time in the level, never reset
  GameTime levelStartTime =
      0_ms; // time the level was started, used for intermission
  int64_t matchStartRealTime =
      0; // real time the match started, used for match end
  int64_t matchEndRealTime = 0; // real time the match ended, used for match end
  GameTime exitTime = 0_ms; // time the level was exited, used for intermission
  GameTime entityReloadGraceUntil =
      0_ms; // grace period for spawn point availability during reloads
  bool readyToExit =
      false; // true if the level is ready to exit, used for intermission

  std::array<char, MAX_QPATH> longName{}; // descriptive name
  std::array<char, MAX_QPATH> mapName{};  // base1, etc
  std::array<char, MAX_QPATH> nextMap{};  // when score limit is hit
  std::array<char, MAX_QPATH> forceMap{}; // forced destination

  int arenaTotal = 0;
  int arenaActive = 0;

  std::string changeMap = "";   // map to change to, if any
  std::string achievement = ""; // achievement to show on intermission, if any
  std::string
      savedEntityString{}; // original map entity string for match resets
  bool matchReloadedFromEntities =
      false; // true when the last reset rebuilt from saved entities

  struct Intermission {
    // intermission state
    GameTime time;   // time the intermission was started
    GameTime queued; // intermission was qualified, but
    // wait INTERMISSION_DELAY_TIME before
    // actually going there so the last
    // frag can be watched.  Disable future
    // kills during this delay

    bool exit = false; // true if the intermission is over and we are exiting
    GameTime postIntermissionTime = 0_ms; // time to wait before exiting the
                                          // level after the score limit is hit
    bool postIntermission =
        false; // true if we are waiting for postIntermissionTime to expire
    bool endOfUnit = false; // true if this is the end of unit intermission
    bool clear = false;     // clear inventory on switch
    bool set =
        false; // for target_camera switches; don't find intermission point
    bool fade = false,
         fading = false;      // fade on exit instead of immediately leaving
    GameTime fadeTime = 0_ms; // time to fade out
    Vector3 origin;           // intermission point origin
    Vector3 angles;           // intermission point angles
    bool spot = false; // true if we are using a spawn spot for intermission
    int32_t
        serverFrame; // server frame when intermission was set, used to prevent
    // intermission from being set multiple times in the same frame
    std::array<char, 64>
        victorMessage; // winner message to show on intermission
    bool duelWinLossApplied =
        false; // prevents duplicate duel win/loss updates per match end
  };
  Intermission intermission{};

  // Modern spawn registry
  SpawnLists spawn;

  // ------------------------------------------------------------------------
  // Legacy compatibility (optional, keep while migrating old call sites)
  // ------------------------------------------------------------------------
  std::array<gentity_t *, NUM_SPAWN_SPOTS>
      spawnSpots{}; // flat view of all non-intermission spawns

  int32_t picHealth = 0;
  int32_t picPing = 0;

  gentity_t *currentEntity; // entity running from G_RunFrame
  int32_t bodyQue = 0;      // dead bodies

  int32_t powerCubes = 0; // ugly necessity for coop

  std::array<ShadowLightInfo, MAX_SHADOW_LIGHTS> shadowLightInfo = {};
  int32_t shadowLightCount = 0;

  bool isN64 = false;
  bool instantItems = false; // instantItems 1 set in worldspawn

  // offset for the first vwep model, for
  // skinNum encoding
  int32_t viewWeaponOffset = 0;

  // [Paril-KEX] current level entry
  LevelEntry *entry{};

  // point of interest
  struct {
    bool valid = false;
    Vector3 current = vec3_origin;
    int32_t currentImage = 0;
    int32_t currentStage = 0;
    gentity_t *currentDynamic;
    Vector3 *points[MAX_SPLIT_PLAYERS]{}; // temporary storage for POIs in coop
  } poi{};

  // start items
  const char *start_items;
  // disable grappling hook
  bool no_grapple = false;
  // disable DM spawn pads
  bool no_dm_spawnpads = false;
  // disable teleporter pads
  bool no_dm_telepads = false;

  // saved gravity
  float gravity = 800.0f;
  // gravity lotto - next time to change gravity (when enabled)
  GameTime gravityLottoNextChange = 0_ms;

  std::array<char, 64> gamemod_name{};
  std::array<char, 64> gametype_name{};

  struct Voting {
    gclient_t *client{};
    GameTime time = 0_ms;        // level.time vote was called
    GameTime executeTime = 0_ms; // time the vote is executed
    int8_t countYes = 0;
    int8_t countNo = 0;
    const VoteCommand *cmd{};
    std::string arg{};
  };
  Voting vote;

  struct Population {
    uint8_t num_connected_clients = 0;
    uint8_t num_console_clients = 0;
    uint8_t num_nonspectator_clients = 0;  // includes connecting clients
    uint8_t num_playing_clients = 0;       // connected, non-spectators
    uint8_t num_playing_human_clients = 0; // players, bots excluded
    uint8_t num_living_red = 0;
    uint8_t num_eliminated_red = 0;
    uint8_t num_living_blue = 0;
    uint8_t num_eliminated_blue = 0;
    uint8_t num_playing_red = 0;
    uint8_t num_playing_blue = 0;
    uint8_t num_voting_clients = 0; // set by CalculateRanks
  };
  Population pop;

  std::array<int, MAX_CLIENTS> sortedClients;
  std::array<int, MAX_CLIENTS> skillSortedClients{};
  uint8_t follow1 = 0, follow2 = 0; // clientNums for auto-follow spectators

  std::array<int, static_cast<int>(Team::Total)> teamScores{};
  std::array<int, static_cast<int>(Team::Total)> teamOldScores{};

  struct DominationState {
    static constexpr size_t MAX_POINTS = 8;

    struct Point {
      gentity_t *ent = nullptr;
      gentity_t *beam = nullptr;
      Team owner = Team::None;
      Team capturingTeam = Team::None;
      float captureProgress = 0.0f;
      GameTime lastProgressTime = 0_ms;
      std::array<int, static_cast<size_t>(Team::Total)> occupantCounts{};
      std::array<GameTime, MAX_CLIENTS_KEX> occupantExpiry{};
      size_t index = 0;
      int32_t spawnCount = 0;
    };

    std::array<Point, MAX_POINTS> points{};
    size_t count = 0;
    GameTime nextScoreTime = 0_ms;
  } domination{};

  struct HeadHuntersState {
    static constexpr size_t MAX_RECEPTACLES = 32;
    static constexpr size_t MAX_SPIKES = 80;
    static constexpr size_t MAX_LOOSE_HEADS = 64;

    struct Receptacle {
      gentity_t *ent = nullptr;
      Team team = Team::None;
    };

    struct SpikeEntry {
      gentity_t *ent = nullptr;
      gentity_t *base = nullptr;
      GameTime nextActivation = 0_ms;
    };

    std::array<Receptacle, MAX_RECEPTACLES> receptacles{};
    size_t receptacleCount = 0;

    std::array<SpikeEntry, MAX_SPIKES> spikeQueue{};
    size_t spikeCount = 0;

    std::array<gentity_t *, MAX_LOOSE_HEADS> looseHeads{};
    size_t looseHeadCount = 0;

    int headModelIndex = 0;
  } headHunters{};

  struct ProBallState {
    struct AssistInfo {
      gentity_t *player = nullptr;
      GameTime expires = 0_ms;
      Team team = Team::None;
    } assist{};

    struct GoalVolume {
      gentity_t *ent = nullptr;
      Team team = Team::None;
    };

    gentity_t *spawnEntity = nullptr;
    gentity_t *ballEntity = nullptr;
    gentity_t *carrier = nullptr;
    gentity_t *lastToucher = nullptr;
    GameTime lastTouchTime = 0_ms;
    std::array<GoalVolume, 4> goals{};
    std::array<gentity_t *, 4> outOfBounds{};
  } proBall{};

  struct BallState {
    gentity_t *entity = nullptr;
    gentity_t *carrier = nullptr;
    Vector3 homeOrigin = vec3_origin;
    Vector3 homeAngles = vec3_origin;
    GameTime idleBegin = 0_ms;
    bool homeValid = false;
  } ball{};

  struct HarvesterState {
    gentity_t *generator = nullptr;
    std::array<gentity_t *, static_cast<size_t>(Team::Total)> bases{};
    std::array<int, static_cast<size_t>(Team::Total)> pendingDrops{};
    int spawnFailureCount = 0;
  } harvester{};

  MatchState matchState = MatchState::None;
  WarmupState warmupState = WarmupState::Default;
  GameTime warmupNoticeTime = 0_ms;
  GameTime matchStateTimer = 0_ms; // change match state at this time
  int32_t
      warmupModificationCount; // for detecting if g_warmup_countdown is changed

  GameTime countdownTimerCheck = 0_ms;
  GameTime matchEndWarnTimerCheck = 0_ms;

  int roundNumber = 0;
  RoundState roundState = RoundState::None;
  int roundStateQueued = 0;
  GameTime roundStateTimer = 0_ms; // change match state at this time

  bool restarted = false;

  GameTime overtime = 0_ms;
  bool suddenDeath = false;

  std::array<int, static_cast<int>(Team::Total)> locked{};

  GameTime ctf_last_flag_capture = 0_ms;
  Team ctf_last_capture_team = Team::None;

  std::array<int, LAST_WEAPON - FIRST_WEAPON + 1> weaponCount;

  GameTime no_players_time = 0_ms;

  bool init = false;

  bool strike_red_attacks = false;
  bool strike_flag_touch = false;
  bool strike_turn_red = false;
  bool strike_turn_blue = false;

  GameTime horde_monster_spawn_time = 0_ms;
  int8_t horde_num_monsters_to_spawn = 0;
  bool horde_all_spawned = true;

  char author[MAX_QPATH];
  char author2[MAX_QPATH];

  GameTime timeoutActive = 0_ms;
  gentity_t *timeoutOwner{};

  std::string matchID{};

  std::array<bool, 3> fragWarning{};

  bool prepare_to_fight = false;

  GameTime endmatch_grace = 0_ms;

  // overall match stats
  MatchOverallStats match;

  // protects death/event logs while async jobs grab snapshots
  std::mutex matchLogMutex;

  // new map system stuff
  uint16_t vote_flags_enable = 0;
  uint16_t vote_flags_disable = 0;

  // map selector
  struct {
    std::array<std::string, 3> candidates{};
    std::array<int, MAX_CLIENTS> votes = [] {
      std::array<int, MAX_CLIENTS> init{};
      init.fill(-1);
      return init;
    }(); // -1 = no vote
    std::array<int, 3> voteCounts = {0, 0, 0};
    GameTime voteStartTime = 0_ms;
    bool forceExit = false;
  } mapSelector;
  std::array<Ghosts, MAX_CLIENTS> ghosts{};

  int autoScreenshotTool_index = 0;
  bool autoScreenshotTool_initialised = false;
  GameTime autoScreenshotTool_delayTime = 0_ms;

  // ===================

  struct Campaign {
    int32_t totalSecrets = 0;
    int32_t foundSecrets = 0;

    int32_t totalGoals = 0;
    int32_t foundGoals = 0;

    int32_t totalMonsters = 0;
    std::array<gentity_t *, MAX_ENTITIES> monstersRegistered =
        {}; // only for debug
    int32_t killedMonsters = 0;

    gentity_t *disguiseViolator = nullptr;
    GameTime disguiseViolationTime = 0_ms;
    int32_t disguiseIcon = 0;

    GameTime coopLevelRestartTime = 0_ms; // restart the level after this time

    // N64 goal stuff
    const char *goals = nullptr; // nullptr if no goals in world
    int32_t goalNum =
        0; // current relative goal number, increased with each target_goal

    // coop health scaling factor;
    // this percentage of health is added
    // to the monster's health per player.
    float coopHealthScaling = 0;
    // this isn't saved in the save file, but stores
    // the amount of players currently active in the
    // level, compared against monsters' individual
    // scale #
    int32_t coopScalePlayers = 0;

    // level is a hub map, and shouldn't be included in EOU stuff
    bool hub_map = false;
    // active health bar entities
    std::array<gentity_t *, MAX_HEALTH_BARS> health_bar_entities;
    bool deadly_kill_box = false;
    bool story_active = false;
    GameTime next_auto_save = 0_ms;
    GameTime next_match_report = 0_ms;
  } campaign{};
};

struct shadow_light_temp_t {
  shadow_light_data_t data;
  const char *lightStyleTarget = nullptr;
};

void G_LoadShadowLights();

// spawn_temp_t is only used to hold entity field values that
// can be set from the editor, but aren't actualy present
// in gentity_t during gameplay.
// defaults can/should be set in the struct.
struct spawn_temp_t {
  /* world vars */
  const char *sky = nullptr;
  float skyRotate = 0.0f;
  Vector3 skyAxis = vec3_origin;
  int32_t skyAutoRotate = 1;
  const char *nextMap = nullptr;

  int32_t lip = 0;
  int32_t distance = 0;
  int32_t height = 0;
  const char *noise = nullptr;
  float pauseTime = 0.0f;
  const char *item = nullptr;
  const char *gravity = "800";

  float minYaw = 0.0f;
  float maxYaw = 0.0f;
  float minPitch = 0.0f;
  float maxPitch = 0.0f;

  shadow_light_temp_t sl = {};
  const char *music = nullptr;
  int instantItems = 0;
  float radius = 0.0f;
  bool hub_map = false;
  const char *achievement = nullptr;

  const char *goals = nullptr;
  const char *image = nullptr;

  int fade_start_dist = 96;
  int fade_end_dist = 384;
  const char *start_items = nullptr;
  int no_grapple = 0;
  int no_dm_spawnpads = 0;
  int no_dm_telepads = 0;
  float health_multiplier = 1.0f;

  const char *reinforcements = nullptr;
  const char *noise_start = nullptr;
  const char *noise_middle = nullptr;
  const char *noise_end = nullptr;
  int32_t loop_count = 0;

  const char *cvar = nullptr;
  const char *cvarValue = nullptr;

  const char *author = nullptr;
  const char *author2 = nullptr;

  const char *ruleset = nullptr;

  bool noBots = false;
  bool noHumans = false;

  int arena = 0;

  Vector3 speeds = vec3_origin;     // oblivion
  Vector3 rotate = vec3_origin;     // oblivion
  const char *pathTarget = nullptr; // oblivion
  Vector3 mangle = vec3_origin;     // oblivion
  float duration = 0.0f;            // oblivion
  Vector3 durations = vec3_origin;  // oblivion
  float accel = 0.0f;               // oblivion
  float decel = 0.0f;               // oblivion

  std::unordered_set<const char *> keys_specified;

  inline bool was_key_specified(const char *key) const {
    return keys_specified.find(key) != keys_specified.end();
  }
};

enum class MoveState : uint8_t { Top, Bottom, Up, Down };

#define DEFINE_DATA_FUNC(ns_lower, ns_upper, returnType, ...)                  \
  using save_##ns_lower##_t =                                                  \
      save_data_t<returnType (*)(__VA_ARGS__), SAVE_FUNC_##ns_upper>

#define SAVE_DATA_FUNC(n, ns, returnType, ...)                                 \
  using save_##n##_t =                                                         \
      save_data_t<returnType (*)(__VA_ARGS__), SAVE_FUNC_##ns>;                \
  extern returnType n(__VA_ARGS__);                                            \
  static const save_data_list_t save__##n(#n, SAVE_FUNC_##ns,                  \
                                          reinterpret_cast<const void *>(n));  \
  auto n

DEFINE_DATA_FUNC(moveinfo_endfunc, MOVEINFO_ENDFUNC, void, gentity_t *self);
#define MOVEINFO_ENDFUNC(n)                                                    \
  SAVE_DATA_FUNC(n, MOVEINFO_ENDFUNC, void, gentity_t *self)

DEFINE_DATA_FUNC(moveinfo_blocked, MOVEINFO_BLOCKED, void, gentity_t *self,
                 gentity_t *other);
#define MOVEINFO_BLOCKED(n)                                                    \
  SAVE_DATA_FUNC(n, MOVEINFO_BLOCKED, void, gentity_t *self, gentity_t *other)

// a struct that can store type-safe allocations
// of a fixed amount of data. it self-destructs when
// re-assigned. TODO: because gentities are still kind of
// managed like C memory, the destructor may not be
// called for a freed entity if this is stored as a member.
template <typename T, int32_t tag> struct savable_allocated_memory_t {
  T *ptr;
  size_t count;

  constexpr savable_allocated_memory_t(T *ptr, size_t count)
      : ptr(ptr), count(count) {}

  inline ~savable_allocated_memory_t() { release(); }

  // no copy
  constexpr savable_allocated_memory_t(const savable_allocated_memory_t &) =
      delete;
  constexpr savable_allocated_memory_t &
  operator=(const savable_allocated_memory_t &) = delete;

  // free move
  constexpr savable_allocated_memory_t(savable_allocated_memory_t &&move) {
    ptr = move.ptr;
    count = move.count;

    move.ptr = nullptr;
    move.count = 0;
  }

  constexpr savable_allocated_memory_t &
  operator=(savable_allocated_memory_t &&move) {
    ptr = move.ptr;
    count = move.count;

    move.ptr = nullptr;
    move.count = 0;

    return *this;
  }

  inline void release() {
    if (ptr) {
      gi.TagFree(ptr);
      count = 0;
      ptr = nullptr;
    }
  }

  constexpr explicit operator T *() { return ptr; }
  constexpr explicit operator const T *() const { return ptr; }

  constexpr std::add_lvalue_reference_t<T> operator[](const size_t index) {
    return ptr[index];
  }
  constexpr const std::add_lvalue_reference_t<T>
  operator[](const size_t index) const {
    return ptr[index];
  }

  constexpr size_t size() const { return count * sizeof(T); }
  constexpr operator bool() const { return !!ptr; }
};

template <typename T, int32_t tag>
inline savable_allocated_memory_t<T, tag> make_savable_memory(size_t count) {
  if (!count)
    return {nullptr, 0};

  return {reinterpret_cast<T *>(gi.TagMalloc(sizeof(T) * count, tag)), count};
}

struct MoveInfo {
  // fixed data
  Vector3 startOrigin;
  Vector3 startAngles;
  Vector3 endOrigin;
  Vector3 endAngles, endAnglesReversed;

  int32_t sound_start;
  int32_t sound_middle;
  int32_t sound_end;

  float accel;
  float speed;
  float decel;
  float distance;

  float wait;

  // state data
  MoveState state;
  bool reversing;
  Vector3 dir;
  Vector3 dest;
  float currentSpeed;
  float moveSpeed;
  float nextSpeed;
  float remainingDistance;
  float decelDistance;
  save_moveinfo_endfunc_t endFunc;
  save_moveinfo_blocked_t blocked;

  // [Paril-KEX] new accel state
  Vector3 curveRef;
  savable_allocated_memory_t<float, TAG_LEVEL> curvePositions;
  size_t curveFrame;
  uint8_t subFrame, numSubFrames;
  size_t numFramesDone;

  /*
  =============
  MoveInfo::MoveInfo

  Initializes movement bookkeeping and curve buffers to known defaults so
  stack-allocated entities used in tests have predictable state.
  =============
  */
  MoveInfo()
      : startOrigin{}, startAngles{}, endOrigin{}, endAngles{},
        endAnglesReversed{}, sound_start(0), sound_middle(0), sound_end(0),
        accel(0.0f), speed(0.0f), decel(0.0f), distance(0.0f), wait(0.0f),
        state(MoveState::Top), reversing(false), dir{}, dest{},
        currentSpeed(0.0f), moveSpeed(0.0f), nextSpeed(0.0f),
        remainingDistance(0.0f), decelDistance(0.0f), endFunc(nullptr),
        blocked(nullptr), curveRef{}, curvePositions(nullptr, 0), curveFrame(0),
        subFrame(0), numSubFrames(0), numFramesDone(0) {}
};

struct MonsterFrame {
  void (*aiFunc)(gentity_t *self, float dist) = nullptr;
  float dist = 0;
  void (*thinkFunc)(gentity_t *self) = nullptr;
  int32_t lerp_frame = -1;
};

// this check only works on windows, and is only
// of importance to developers anyways
#if defined(_WIN32) && defined(_MSC_VER)
#if _MSC_VER >= 1934
#define COMPILE_TIME_MOVE_CHECK
#endif
#endif

struct MonsterMove {
  int32_t firstFrame;
  int32_t lastFrame;
  const MonsterFrame *frame;
  void (*endFunc)(gentity_t *self);
  float sidestep_scale;

#ifdef COMPILE_TIME_MOVE_CHECK
  template <size_t N>
  constexpr MonsterMove(int32_t firstFrame, int32_t lastFrame,
                        const MonsterFrame (&frames)[N],
                        void (*endFunc)(gentity_t *self) = nullptr,
                        float sidestep_scale = 0.0f)
      : firstFrame(firstFrame), lastFrame(lastFrame), frame(frames),
        endFunc(endFunc), sidestep_scale(sidestep_scale) {
    if ((lastFrame - firstFrame + 1) != N)
      throw std::exception("Bad animation frames; check your numbers!");
  }
#endif
};

using save_mmove_t = save_data_t<MonsterMove, SAVE_DATA_MMOVE>;
#ifdef COMPILE_TIME_MOVE_CHECK
#define MMOVE_T(n)                                                             \
  extern const MonsterMove n;                                                  \
  static const save_data_list_t save__##n(#n, SAVE_DATA_MMOVE, &n);            \
  constexpr MonsterMove n
#else
#define MMOVE_T(n)                                                             \
  extern const MonsterMove n;                                                  \
  static const save_data_list_t save__##n(#n, SAVE_DATA_MMOVE, &n);            \
  const MonsterMove n
#endif

DEFINE_DATA_FUNC(monsterinfo_stand, MONSTERINFO_STAND, void, gentity_t *self);
#define MONSTERINFO_STAND(n)                                                   \
  SAVE_DATA_FUNC(n, MONSTERINFO_STAND, void, gentity_t *self)

DEFINE_DATA_FUNC(monsterinfo_idle, MONSTERINFO_IDLE, void, gentity_t *self);
#define MONSTERINFO_IDLE(n)                                                    \
  SAVE_DATA_FUNC(n, MONSTERINFO_IDLE, void, gentity_t *self)

DEFINE_DATA_FUNC(monsterinfo_search, MONSTERINFO_SEARCH, void, gentity_t *self);
#define MONSTERINFO_SEARCH(n)                                                  \
  SAVE_DATA_FUNC(n, MONSTERINFO_SEARCH, void, gentity_t *self)

DEFINE_DATA_FUNC(monsterinfo_walk, MONSTERINFO_WALK, void, gentity_t *self);
#define MONSTERINFO_WALK(n)                                                    \
  SAVE_DATA_FUNC(n, MONSTERINFO_WALK, void, gentity_t *self)

DEFINE_DATA_FUNC(monsterinfo_run, MONSTERINFO_RUN, void, gentity_t *self);
#define MONSTERINFO_RUN(n)                                                     \
  SAVE_DATA_FUNC(n, MONSTERINFO_RUN, void, gentity_t *self)

DEFINE_DATA_FUNC(monsterinfo_dodge, MONSTERINFO_DODGE, void, gentity_t *self,
                 gentity_t *attacker, GameTime eta, trace_t *tr, bool gravity);
#define MONSTERINFO_DODGE(n)                                                   \
  SAVE_DATA_FUNC(n, MONSTERINFO_DODGE, void, gentity_t *self,                  \
                 gentity_t *attacker, GameTime eta, trace_t *tr, bool gravity)

DEFINE_DATA_FUNC(monsterinfo_attack, MONSTERINFO_ATTACK, void, gentity_t *self);
#define MONSTERINFO_ATTACK(n)                                                  \
  SAVE_DATA_FUNC(n, MONSTERINFO_ATTACK, void, gentity_t *self)

DEFINE_DATA_FUNC(monsterinfo_melee, MONSTERINFO_MELEE, void, gentity_t *self);
#define MONSTERINFO_MELEE(n)                                                   \
  SAVE_DATA_FUNC(n, MONSTERINFO_MELEE, void, gentity_t *self)

DEFINE_DATA_FUNC(monsterinfo_sight, MONSTERINFO_SIGHT, void, gentity_t *self,
                 gentity_t *other);
#define MONSTERINFO_SIGHT(n)                                                   \
  SAVE_DATA_FUNC(n, MONSTERINFO_SIGHT, void, gentity_t *self, gentity_t *other)

DEFINE_DATA_FUNC(monsterinfo_checkattack, MONSTERINFO_CHECKATTACK, bool,
                 gentity_t *self);
#define MONSTERINFO_CHECKATTACK(n)                                             \
  SAVE_DATA_FUNC(n, MONSTERINFO_CHECKATTACK, bool, gentity_t *self)

DEFINE_DATA_FUNC(monsterinfo_setskin, MONSTERINFO_SETSKIN, void,
                 gentity_t *self);
#define MONSTERINFO_SETSKIN(n)                                                 \
  SAVE_DATA_FUNC(n, MONSTERINFO_SETSKIN, void, gentity_t *self)

DEFINE_DATA_FUNC(monsterinfo_blocked, MONSTERINFO_BLOCKED, bool,
                 gentity_t *self, float dist);
#define MONSTERINFO_BLOCKED(n)                                                 \
  SAVE_DATA_FUNC(n, MONSTERINFO_BLOCKED, bool, gentity_t *self, float dist)

DEFINE_DATA_FUNC(monsterinfo_physicschange, MONSTERINFO_PHYSCHANGED, void,
                 gentity_t *self);
#define MONSTERINFO_PHYSCHANGED(n)                                             \
  SAVE_DATA_FUNC(n, MONSTERINFO_PHYSCHANGED, void, gentity_t *self)

DEFINE_DATA_FUNC(monsterinfo_duck, MONSTERINFO_DUCK, bool, gentity_t *self,
                 GameTime eta);
#define MONSTERINFO_DUCK(n)                                                    \
  SAVE_DATA_FUNC(n, MONSTERINFO_DUCK, bool, gentity_t *self, GameTime eta)

DEFINE_DATA_FUNC(monsterinfo_unduck, MONSTERINFO_UNDUCK, void, gentity_t *self);
#define MONSTERINFO_UNDUCK(n)                                                  \
  SAVE_DATA_FUNC(n, MONSTERINFO_UNDUCK, void, gentity_t *self)

DEFINE_DATA_FUNC(monsterinfo_sidestep, MONSTERINFO_SIDESTEP, bool,
                 gentity_t *self);
#define MONSTERINFO_SIDESTEP(n)                                                \
  SAVE_DATA_FUNC(n, MONSTERINFO_SIDESTEP, bool, gentity_t *self)

// combat styles, for navigation
enum class CombatStyle : uint8_t {
  Unknown, // automatically choose based on attack functions
  Melee,   // should attempt to get up close for melee
  Mixed,   // has mixed melee/ranged; runs to get up close if far enough away
  Ranged   // don't bother pathing if we can see the player
};

struct reinforcement_t {
  const char *className = nullptr;
  int32_t strength;
  Vector3 mins, maxs;
};

struct reinforcement_list_t {
  reinforcement_t *reinforcements;
  uint32_t num_reinforcements;
  uint32_t next_reinforcement;
  uint32_t *spawn_counts;
};

constexpr size_t MAX_REINFORCEMENTS =
    5; // max number of spawns we can do at once.

constexpr GameTime HOLD_FOREVER =
    GameTime::from_ms(std::numeric_limits<int64_t>::max());

struct MonsterInfo {
  // [Paril-KEX] allow some moves to be done instantaneously, but
  // others can wait the full frame.
  // NB: always use `M_SetAnimation` as it handles edge cases.
  save_mmove_t active_move, next_move;
  monster_ai_flags_t aiFlags; // PGM - unsigned, since we're close to the max
  int32_t
      nextFrame; // if next_move is set, this is ignored until a frame is ran
  float scale;

  save_monsterinfo_stand_t stand;
  save_monsterinfo_idle_t idle;
  save_monsterinfo_search_t search;
  save_monsterinfo_walk_t walk;
  save_monsterinfo_run_t run;
  save_monsterinfo_dodge_t dodge;
  save_monsterinfo_attack_t attack;
  save_monsterinfo_melee_t melee;
  save_monsterinfo_sight_t sight;
  save_monsterinfo_checkattack_t checkAttack;
  save_monsterinfo_setskin_t setSkin;
  save_monsterinfo_physicschange_t physicsChange;

  GameTime pauseTime;
  GameTime attackFinished;
  GameTime fireWait;

  Vector3 savedGoal;
  GameTime searchTime;
  GameTime trailTime;
  Vector3 lastSighting;
  MonsterAttackState attackState;
  bool lefty;
  GameTime idleTime;
  int32_t linkCount;

  item_id_t powerArmorType;
  int32_t powerArmorPower;

  // for monster revive
  item_id_t initialPowerArmorType;
  int32_t max_power_armor_power;
  int32_t weaponSound, engineSound;

  save_monsterinfo_blocked_t blocked;
  GameTime last_hint_time; // last time the monster checked for hintpaths.
  gentity_t *goal_hint;    // which hint_path we're trying to get to
  int32_t medicTries;
  gentity_t *badMedic1,
      *badMedic2;    // these medics have declared this monster "unhealable"
  gentity_t *healer; // this is who is healing this monster
  save_monsterinfo_duck_t duck;
  save_monsterinfo_unduck_t unDuck;
  save_monsterinfo_sidestep_t sideStep;
  float base_height;
  GameTime next_duck_time;
  GameTime duck_wait_time;
  gentity_t *last_player_enemy;
  // blindFire stuff .. the boolean says whether the monster will do it, and
  // blind_fire_time is the timing (set in the monster) of the next shot
  bool blindFire;      // will the monster blindFire?
  bool canJump;        // will the monster jump?
  bool had_visibility; // Paril: used for blindFire
  float dropHeight, jumpHeight;
  GameTime blind_fire_delay;
  Vector3 blind_fire_target;
  Vector3 teleportReturnOrigin;
  GameTime teleportReturnTime;
  bool teleportActive = false;
  // used by the spawners to not spawn too much and keep track of #s of monsters
  // spawned
  int32_t monster_slots; // nb: for spawned monsters, this is how many slots we
                         // took from our commander
  int32_t monster_used;
  gentity_t *commander;
  // powerup timers, used by widow, our friend
  GameTime quad_time;
  GameTime invincibility_time;
  GameTime double_time;

  // Paril
  GameTime surprise_time;
  item_id_t armorType;
  int32_t armor_power;
  bool close_sight_tripped;
  GameTime melee_debounce_time; // don't melee until this time has passed
  GameTime strafe_check_time;   // time until we should reconsider strafing
  int32_t
      base_health; // health that we had on spawn, before any co-op adjustments
  int32_t health_scaling;        // number of players we've been scaled up to
  GameTime next_move_time;       // high tick rate
  GameTime bad_move_time;        // don't try straight moves until this is over
  GameTime bump_time;            // don't slide against walls for a bit
  GameTime random_change_time;   // high tickrate
  GameTime path_blocked_counter; // break out of paths when > a certain time
  GameTime path_wait_time;       // don't try nav nodes until this is over
  PathInfo nav_path; // if AI_PATHING, this is where we are trying to reach
  GameTime nav_path_cache_time; // cache nav_path result for this much time
  CombatStyle combatStyle;      // pathing style

  struct dmg {
    gentity_t *attacker = nullptr;
    gentity_t *inflictor = nullptr;
    MeansOfDeath mod = ModID::Unknown;

    int32_t blood = 0;
    int32_t knockback = 0;
    Vector3 origin = vec3_origin;
  };
  dmg damage;

  // alternate flying mechanics
  float fly_max_distance, fly_min_distance; // how far we should try to stay
  float fly_acceleration;                   // accel/decel speed
  float fly_speed;                          // max speed from flying
  Vector3 fly_ideal_position; // ideally where we want to end up to hover,
                              // relative to our target if not pinned
  GameTime fly_position_time; // if <= level.time, we can try changing positions
  bool fly_buzzard,
      fly_above;   // orbit around all sides of their enemy, not just the sides
  bool fly_pinned; // whether we're currently pinned to ideal position (made
                   // absolute)
  bool fly_thrusters; // slightly different flight mechanics, for melee attacks
  GameTime fly_recovery_time; // time to try a new dir to get away from hazards
  Vector3 fly_recovery_dir;

  // teleport helpers
  Vector3 teleport_saved_origin;
  GameTime teleport_return_time;
  bool teleport_active;

  GameTime checkattack_time;
  int32_t startFrame;
  GameTime dodge_time;
  int32_t move_block_counter;
  GameTime move_block_change_time;
  GameTime react_to_damage_time;

  reinforcement_list_t reinforcements;
  std::array<uint8_t, MAX_REINFORCEMENTS>
      chosen_reinforcements; // readied for spawn; 255 is value for none

  GameTime jump_time;

  // NOTE: if adding new elements, make sure to add them
  // in g_save.cpp too!
};

// non-monsterInfo save stuff
using save_prethink_t =
    save_data_t<void (*)(gentity_t *self), SAVE_FUNC_PRETHINK>;
#define PRETHINK(n)                                                            \
  void n(gentity_t *self);                                                     \
  static const save_data_list_t save__##n(#n, SAVE_FUNC_PRETHINK,              \
                                          reinterpret_cast<const void *>(n));  \
  auto n

using save_think_t = save_data_t<void (*)(gentity_t *self), SAVE_FUNC_THINK>;
#define THINK(n)                                                               \
  void n(gentity_t *self);                                                     \
  static const save_data_list_t save__##n(#n, SAVE_FUNC_THINK,                 \
                                          reinterpret_cast<const void *>(n));  \
  auto n

using save_touch_t =
    save_data_t<void (*)(gentity_t *self, gentity_t *other, const trace_t &tr,
                         bool otherTouchingSelf),
                SAVE_FUNC_TOUCH>;
#define TOUCH(n)                                                               \
  void n(gentity_t *self, gentity_t *other, const trace_t &tr,                 \
         bool otherTouchingSelf);                                              \
  static const save_data_list_t save__##n(#n, SAVE_FUNC_TOUCH,                 \
                                          reinterpret_cast<const void *>(n));  \
  auto n

using save_use_t = save_data_t<void (*)(gentity_t *self, gentity_t *other,
                                        gentity_t *activator),
                               SAVE_FUNC_USE>;
#define USE(n)                                                                 \
  void n(gentity_t *self, gentity_t *other, gentity_t *activator);             \
  static const save_data_list_t save__##n(#n, SAVE_FUNC_USE,                   \
                                          reinterpret_cast<const void *>(n));  \
  auto n

using save_pain_t =
    save_data_t<void (*)(gentity_t *self, gentity_t *other, float kick,
                         int damage, const MeansOfDeath &mod),
                SAVE_FUNC_PAIN>;
#define PAIN(n)                                                                \
  void n(gentity_t *self, gentity_t *other, float kick, int damage,            \
         const MeansOfDeath &mod);                                             \
  static const save_data_list_t save__##n(#n, SAVE_FUNC_PAIN,                  \
                                          reinterpret_cast<const void *>(n));  \
  auto n

using save_die_t =
    save_data_t<void (*)(gentity_t *self, gentity_t *inflictor,
                         gentity_t *attacker, int damage, const Vector3 &point,
                         const MeansOfDeath &mod),
                SAVE_FUNC_DIE>;
#define DIE(n)                                                                 \
  void n(gentity_t *self, gentity_t *inflictor, gentity_t *attacker,           \
         int damage, const Vector3 &point, const MeansOfDeath &mod);           \
  static const save_data_list_t save__##n(#n, SAVE_FUNC_DIE,                   \
                                          reinterpret_cast<const void *>(n));  \
  auto n

// this determines how long to wait after a duck to duck again.
// if we finish a duck-up, this gets cut in half.
constexpr GameTime DUCK_INTERVAL = 5000_ms;

extern GameLocals game;
extern LevelLocals level;
extern game_export_t globals;
extern spawn_temp_t st;

extern gentity_t *g_entities;

extern std::mt19937 mt_rand;

// uniform float [0, 1)
[[nodiscard]] inline float frandom() {
  return std::uniform_real_distribution<float>()(mt_rand);
}

// uniform float [min_inclusive, max_exclusive)
[[nodiscard]] inline float frandom(float min_inclusive, float max_exclusive) {
  return std::uniform_real_distribution<float>(min_inclusive,
                                               max_exclusive)(mt_rand);
}

// uniform float [0, max_exclusive)
[[nodiscard]] inline float frandom(float max_exclusive) {
  return std::uniform_real_distribution<float>(0, max_exclusive)(mt_rand);
}

// uniform time [min_inclusive, max_exclusive)
[[nodiscard]] inline GameTime random_time(GameTime min_inclusive,
                                          GameTime max_exclusive) {
  return GameTime::from_ms(std::uniform_int_distribution<int64_t>(
      min_inclusive.milliseconds(), max_exclusive.milliseconds())(mt_rand));
}

// uniform time [0, max_exclusive)
[[nodiscard]] inline GameTime random_time(GameTime max_exclusive) {
  return GameTime::from_ms(std::uniform_int_distribution<int64_t>(
      0, max_exclusive.milliseconds())(mt_rand));
}

// uniform float [-1, 1)
// note: closed on min but not max
// to match vanilla behavior
[[nodiscard]] inline float crandom() {
  return std::uniform_real_distribution<float>(-1.f, 1.f)(mt_rand);
}

// uniform float (-1, 1)
[[nodiscard]] inline float crandom_open() {
  return std::uniform_real_distribution<float>(std::nextafterf(-1.f, 0.f),
                                               1.f)(mt_rand);
}

// raw unsigned int32 value from random
[[nodiscard]] inline uint32_t irandom() { return mt_rand(); }

// uniform int [min, max)
// always returns min if min == (max - 1)
// undefined behavior if min > (max - 1)
[[nodiscard]] inline int32_t irandom(int32_t min_inclusive,
                                     int32_t max_exclusive) {
  if (min_inclusive == max_exclusive - 1)
    return min_inclusive;

  return std::uniform_int_distribution<int32_t>(min_inclusive,
                                                max_exclusive - 1)(mt_rand);
}

// uniform int [0, max)
// always returns 0 if max <= 0
// note for Q2 code:
// - to fix rand()%x, do irandom(x)
// - to fix rand()&x, do irandom(x + 1)
[[nodiscard]] inline int32_t irandom(int32_t max_exclusive) {
  if (max_exclusive <= 0)
    return 0;

  return irandom(0, max_exclusive);
}

// uniform random index from given container
template <typename T>
[[nodiscard]] inline int32_t random_index(const T &container) {
  return irandom(std::size(container));
}

// uniform random element from given container
template <typename T>
[[nodiscard]] inline auto random_element(T &container)
    -> decltype(*std::begin(container)) {
  return *(std::begin(container) + random_index(container));
}

// flip a coin
[[nodiscard]] inline bool brandom() { return irandom(2) == 0; }

extern cvar_t *hostname;

extern cvar_t *deathmatch;
extern cvar_t *ctf;
extern cvar_t *teamplay;
extern cvar_t *g_gametype;
extern cvar_t *g_practice;

extern cvar_t *coop;

extern cvar_t *skill;
extern cvar_t *fragLimit;
extern cvar_t *captureLimit;
extern cvar_t *timeLimit;
extern cvar_t *roundLimit;
extern cvar_t *roundTimeLimit;
extern cvar_t *mercyLimit;
extern cvar_t *noPlayersTime;
extern cvar_t *marathon;
extern cvar_t *g_marathon_timelimit;
extern cvar_t *g_marathon_scorelimit;

extern cvar_t *g_ruleset;

extern cvar_t *password;
extern cvar_t *spectatorPassword;
extern cvar_t *admin_password;
extern cvar_t *needPass;
extern cvar_t *filterBan;

extern cvar_t *maxplayers;
extern cvar_t *minplayers;

extern cvar_t *ai_allow_dm_spawn;
extern cvar_t *ai_damage_scale;
extern cvar_t *ai_model_scale;
extern cvar_t *ai_movement_disabled;
extern cvar_t *ai_widow_roof_spawn;

extern cvar_t *bot_debug_follow_actor;
extern cvar_t *bot_debug_move_to_point;

extern cvar_t *flood_msgs;
extern cvar_t *flood_persecond;
extern cvar_t *flood_waitdelay;

extern cvar_t *bob_pitch;
extern cvar_t *bob_roll;
extern cvar_t *bob_up;
extern cvar_t *gun_x, *gun_y, *gun_z;
extern cvar_t *run_pitch;
extern cvar_t *run_roll;

extern cvar_t *g_allowAdmin;
extern cvar_t *g_allowCustomSkins;
extern cvar_t *g_allowForfeit;
extern cvar_t *g_allow_duel_queue;
extern cvar_t *g_allow_grapple;
extern cvar_t *g_allow_kill;
extern cvar_t *g_allowMymap;
extern cvar_t *g_allowSpecVote;
extern cvar_t *g_allowTechs;
extern cvar_t *g_allowVoteMidGame;
extern cvar_t *g_allowVoting;
extern cvar_t *g_arenaSelfDmgArmor;
extern cvar_t *g_arenaStartingArmor;
extern cvar_t *g_arenaStartingHealth;
extern cvar_t *g_cheats;
extern cvar_t *g_ghost_min_play_time;
extern cvar_t *g_coop_enable_lives;
extern cvar_t *g_coop_health_scaling;
extern cvar_t *g_coop_instanced_items;
extern cvar_t *g_coop_num_lives;
extern cvar_t *g_coop_player_collision;
extern cvar_t *g_coop_squad_respawn;
extern cvar_t *g_lms_num_lives;
extern cvar_t *g_damage_scale;
extern cvar_t *g_debug_monster_kills;
extern cvar_t *g_debug_monster_paths;
extern cvar_t *g_dedicated;
extern cvar_t *g_disable_player_collision;
extern cvar_t *match_startNoHumans;
extern cvar_t *match_autoJoin;
extern cvar_t *match_crosshairIDs;
extern cvar_t *warmup_doReadyUp;
extern cvar_t *warmup_enabled;
extern cvar_t *g_dm_exec_level_cfg;
extern cvar_t *match_forceJoin;
extern cvar_t *match_doForceRespawn;
extern cvar_t *match_forceRespawnTime;
extern cvar_t *match_holdableAdrenaline;
extern cvar_t *match_instantItems;
extern cvar_t *owner_intermissionShots;
extern cvar_t *match_itemsRespawnRate;
extern cvar_t *g_fallingDamage;
extern cvar_t *g_selfDamage;
extern cvar_t *match_doOvertime;
extern cvar_t *match_powerupDrops;
extern cvar_t *match_powerupMinPlayerLock;
extern cvar_t *g_dm_random_items;
extern cvar_t *g_domination_tick_interval;
extern cvar_t *g_domination_points_per_tick;
extern cvar_t *g_domination_capture_bonus;
extern cvar_t *g_domination_capture_time;
extern cvar_t *g_domination_neutralize_time;
extern cvar_t *match_playerRespawnMinDelay;
extern cvar_t *match_playerRespawnMinDistance;
extern cvar_t *match_playerRespawnMinDistanceDebug;
extern cvar_t *match_map_sameLevel;
extern cvar_t *match_allowSpawnPads;
extern cvar_t *g_dm_strong_mines;
extern cvar_t *match_allowTeleporterPads;
extern cvar_t *match_timeoutLength;
extern cvar_t *match_weaponsStay;
extern cvar_t *match_dropCmdFlags;
extern cvar_t *g_entityOverrideDir;
extern cvar_t *g_entityOverrideLoad;
extern cvar_t *g_entityOverrideSave;
extern cvar_t *g_eyecam;
extern cvar_t *g_fastDoors;
extern cvar_t *g_frag_messages;
extern cvar_t *g_frenzy;
extern cvar_t *g_friendlyFireScale;
extern cvar_t *g_frozen_time;
extern cvar_t *g_grapple_damage;
extern cvar_t *g_grapple_fly_speed;
extern cvar_t *g_grapple_offhand;
extern cvar_t *g_grapple_pull_speed;
extern cvar_t *g_gravity;
extern cvar_t *g_gravity_lotto;
extern cvar_t *g_horde_starting_wave;
extern cvar_t *g_huntercam;
extern cvar_t *g_inactivity;
extern cvar_t *g_infiniteAmmo;
extern cvar_t *g_instaGib;
extern cvar_t *g_instagib_splash;
extern cvar_t *g_instantWeaponSwitch;
extern cvar_t *g_itemBobbing;
extern cvar_t *g_knockbackScale;
extern cvar_t *g_ladderSteps;
extern cvar_t *g_lagCompensation;
extern cvar_t *g_level_rulesets;
extern cvar_t *match_maps_list;
extern cvar_t *match_maps_listShuffle;
extern cvar_t *match_lock;
extern cvar_t *match_setup_length;
extern cvar_t *match_setup_type;
extern cvar_t *match_setup_bestof;
extern cvar_t *g_tourney_cfg;
extern cvar_t *g_matchstats;
extern cvar_t *g_maxvelocity;
extern cvar_t *g_motd_filename;
extern cvar_t *g_mover_debug;
extern cvar_t *g_mover_speed_scale;
extern cvar_t *g_nadeFest;
extern cvar_t *g_no_armor;
extern cvar_t *g_mapspawn_no_bfg;
extern cvar_t *g_mapspawn_no_plasmabeam;
extern cvar_t *g_no_health;
extern cvar_t *g_no_items;
extern cvar_t *g_no_mines;
extern cvar_t *g_no_nukes;
extern cvar_t *g_no_powerups;
extern cvar_t *g_no_spheres;
extern cvar_t *g_owner_auto_join;
extern cvar_t *g_owner_push_scores;
extern cvar_t *g_quadhog;
extern cvar_t *g_quickWeaponSwitch;
extern cvar_t *g_rollAngle;
extern cvar_t *g_rollSpeed;
extern cvar_t *g_select_empty;
extern cvar_t *g_showhelp;
extern cvar_t *g_showmotd;
extern cvar_t *g_skip_view_modifiers;
extern cvar_t *g_start_items;
extern cvar_t *g_starting_health;
extern cvar_t *g_starting_health_bonus;
extern cvar_t *g_starting_armor;
extern cvar_t *g_stopspeed;
extern cvar_t *g_strict_saves;
extern cvar_t *g_teamplay_allow_team_pick;
extern cvar_t *g_teamplay_armor_protect;
extern cvar_t *g_teamplay_auto_balance;
extern cvar_t *g_teamplay_force_balance;
extern cvar_t *g_teamplay_item_drop_notice;
extern cvar_t *g_vampiric_damage;
extern cvar_t *g_vampiric_exp_min;
extern cvar_t *g_vampiric_health_max;
extern cvar_t *g_vampiric_percentile;
extern cvar_t *g_verbose;
extern cvar_t *g_vote_flags;
extern cvar_t *g_vote_limit;
extern cvar_t *g_warmup_countdown;
extern cvar_t *g_warmup_ready_percentage;
extern cvar_t *g_weaponProjection;
extern cvar_t *g_weapon_respawn_time;

extern cvar_t *g_maps_cycle_file;
extern cvar_t *g_maps_selector;
extern cvar_t *g_maps_mymap;
extern cvar_t *g_maps_allow_custom_textures;
extern cvar_t *g_maps_allow_custom_sounds;

extern cvar_t *g_statex_enabled;
extern cvar_t *g_statex_humans_present;
extern cvar_t *g_statex_export_html;

extern cvar_t *g_blueTeamName;
extern cvar_t *g_redTeamName;

extern cvar_t *bot_name_prefix;

extern cvar_t *g_autoScreenshotTool;

#define world (&g_entities[0])
#define host (&g_entities[1])

uint32_t GetUnicastKey();

// item spawnFlags
constexpr SpawnFlags SPAWNFLAG_ITEM_TRIGGER_SPAWN = 0x00000001_spawnflag;
constexpr SpawnFlags SPAWNFLAG_ITEM_NO_TOUCH = 0x00000002_spawnflag;
constexpr SpawnFlags SPAWNFLAG_ITEM_TOSS_SPAWN = 0x00000004_spawnflag;
constexpr SpawnFlags SPAWNFLAG_ITEM_SUSPENDED = 0x00000008_spawnflag;
constexpr SpawnFlags SPAWNFLAG_ITEM_MAX = 0x00000010_spawnflag;
// 8 bits reserved for editor flags & power cube bits
// (see SPAWNFLAG_NOT_EASY above)
constexpr SpawnFlags SPAWNFLAG_ITEM_DROPPED = 0x00010000_spawnflag;
constexpr SpawnFlags SPAWNFLAG_ITEM_DROPPED_PLAYER = 0x00020000_spawnflag;
constexpr SpawnFlags SPAWNFLAG_ITEM_TARGETS_USED = 0x00040000_spawnflag;

extern std::array<Item, IT_TOTAL> itemList;

//
// g_items.cpp
//
void SelectNextItem(gentity_t *ent, item_flags_t itflags);
void SelectPrevItem(gentity_t *ent, item_flags_t itflags);
void ValidateSelectedItem(gentity_t *ent);
void QuadHog_DoSpawn(gentity_t *ent);
void QuadHog_DoReset(gentity_t *ent);
void QuadHog_SetupSpawn(GameTime delay);
void QuadHog_Spawn(Item *item, gentity_t *spot, bool reset);
void Tech_DeadDrop(gentity_t *ent);
void Tech_Reset();
void Tech_SetupSpawn();
Item *Tech_Held(gentity_t *ent);
int Tech_ApplyDisruptorShield(gentity_t *ent, int dmg);
bool Tech_ApplyPowerAmpSound(gentity_t *ent);
bool Tech_ApplyTimeAccel(gentity_t *ent);
void Tech_ApplyTimeAccelSound(gentity_t *ent);
void Tech_ApplyAutoDoc(gentity_t *ent);
bool Tech_HasRegeneration(gentity_t *ent);
void PrecacheItem(Item *it);
bool CheckItemEnabled(Item *item);
Item *CheckItemReplacements(Item *item);
void InitItems();
void SetItemNames();
Item *FindItem(const char *pickupName);
Item *FindItemByClassname(const char *className);
gentity_t *Drop_Item(gentity_t *ent, Item *item);
void SetRespawn(gentity_t *ent, GameTime delay, bool hide_self = true);
void Change_Weapon(gentity_t *ent);
bool SpawnItem(gentity_t *ent, Item *item);
void Think_Weapon(gentity_t *ent);
item_id_t ArmorIndex(gentity_t *ent);
item_id_t PowerArmorType(gentity_t *ent);
Item *GetItemByIndex(item_id_t index);
Item *GetItemByAmmo(AmmoID ammo);
Item *GetItemByPowerup(powerup_t powerup);
bool Add_Ammo(gentity_t *ent, Item *item, int count);
void CheckPowerArmorState(gentity_t *ent);
void Touch_Item(gentity_t *ent, gentity_t *other, const trace_t &tr,
                bool otherTouchingSelf);
void P_ToggleFlashlight(gentity_t *ent, bool state);
bool Entity_IsVisibleToPlayer(gentity_t *ent, gentity_t *player);
void Compass_Update(gentity_t *ent, bool first);
item_id_t DoRandomRespawn(gentity_t *ent);
void RespawnItem(gentity_t *ent);
void fire_doppelganger(gentity_t *ent, const Vector3 &start,
                       const Vector3 &aimDir);
void Drop_Backpack(gentity_t *ent);
void G_CapAllAmmo(gentity_t *ent);
//
// g_utilities.cpp
//
bool CheckArenaValid(int arenaNum);
bool ChangeArena(int newArenaNum);
bool KillBox(gentity_t *ent, bool from_spawning, ModID mod = ModID::Telefragged,
             bool bsp_clipping = true);
gentity_t *FindEntity(gentity_t *from,
                      std::function<bool(gentity_t *e)> matcher);
bool ReadyConditions(gentity_t *ent, bool admin_cmd);
int TeamBalance(bool force);
void ApplyQueuedTeamChange(gentity_t *ent, bool silent);
void ApplyQueuedTeamChanges(bool silent);
Team PickTeam(int ignore_client_num);

// utility template for getting the type of a field
template <typename> struct member_object_type {};
template <typename T1, typename T2> struct member_object_type<T1 T2::*> {
  using type = T1;
};
template <typename T>
using member_object_type_t =
    typename member_object_type<std::remove_cv_t<T>>::type;

template <auto M>
gentity_t *G_FindByString(gentity_t *from, const std::string_view &value) {
  static_assert(std::is_same_v<member_object_type_t<decltype(M)>, const char *>,
                "can only use string member functions");

  return FindEntity(from, [&](gentity_t *e) {
    return e->*M && strlen(e->*M) == value.length() &&
           !Q_strncasecmp(e->*M, value.data(), value.length());
  });
}

gentity_t *FindRadius(gentity_t *from, const Vector3 &org, float rad);
gentity_t *PickTarget(const char *targetName);
void UseTargets(gentity_t *ent, gentity_t *activator);
void PrintActivationMessage(gentity_t *ent, gentity_t *activator,
                            bool coop_global);
void SetMoveDir(Vector3 &angles, Vector3 &moveDir);

void InitGEntity(gentity_t *e);
gentity_t *Spawn();
void FreeEntity(gentity_t *e);

void TouchTriggers(gentity_t *ent);
void G_TouchProjectiles(gentity_t *ent, Vector3 previous_origin);

char *CopyString(const char *in, int32_t tag);

void G_PlayerNotifyGoal(gentity_t *player);

const char *Teams_TeamName(Team team);
const char *Teams_OtherTeamName(Team team);
Team Teams_OtherTeam(Team team);
bool Teams();
void G_AdjustPlayerScore(gclient_t *cl, int32_t offset, bool adjust_team,
                         int32_t team_offset);
void Horde_AdjustPlayerScore(gclient_t *cl, int32_t offset);
void G_SetPlayerScore(gclient_t *cl, int32_t value);
void G_AdjustTeamScore(Team team, int32_t offset);
void G_SetTeamScore(Team team, int32_t value);
void Domination_ClearState();
void Domination_InitLevel();
void Domination_RunFrame();

const char *PlaceString(int rank);
bool ItemSpawnsEnabled();
bool LocCanSee(gentity_t *targetEnt, gentity_t *sourceEnt);
bool SetTeam(gentity_t *ent, Team desired_team, bool inactive, bool force,
             bool silent);
const char *TimeString(const int msec, bool showMilliseconds, bool state);
void BroadcastFriendlyMessage(Team team, const char *msg);
void BroadcastTeamMessage(Team team, print_type_t level, const char *msg);
Team StringToTeamNum(const char *in);
bool CombatIsDisabled();
bool ItemPickupsAreDisabled();
GameType GametypeStringToIndex(std::string_view input);
std::string_view GametypeIndexToString(GameType gametype);
std::string GametypeOptionList();
bool ScoringIsDisabled();
void BroadcastReadyReminderMessage();
bool CooperativeModeOn();
bool G_LimitedLivesActive();
bool G_LimitedLivesInCoop();
bool G_LimitedLivesInLMS();
int G_LimitedLivesMax();

gentity_t *ClientEntFromString(const char *in);
Ruleset RS_IndexFromString(const char *in);
void TeleporterVelocity(gentity_t *ent, gvec3_t angles);
void TeleportPlayer(gentity_t *player, Vector3 origin, Vector3 angles);
void TeleportPlayerToRandomSpawnPoint(gentity_t *ent, bool fx);
void AnnouncerSound(gentity_t *announcer, std::string_view soundKey);
void CreateSpawnPad(gentity_t *ent);
bool LogAccuracyHit(gentity_t *target, gentity_t *attacker);
void AssignPlayerSkin(gentity_t *ent, const std::string &skin);
void G_LogEvent(std::string str);
void GT_SetLongName(void);
void ApplyGravityLotto();
void CalculateRanks();
std::string TimeStamp();
std::string FileTimeStamp();
std::string DateStamp();
std::string FormatDuration(int seconds);
Weapon GetWeaponIndexByAbbrev(const std::string &abbr);
int64_t GetCurrentRealTimeMillis();
double GetRealTimeSeconds();
bool Vote_Menu_Active(gentity_t *ent);

//
// g_spawn.cpp
//
void ED_CallSpawn(gentity_t *ent);
char *ED_NewString(const char *string);
void GT_PrecacheAssets();
void SpawnEntities(const char *mapname, const char *entities,
                   const char *spawnPoint);
bool G_ResetWorldEntitiesFromSavedString();

//
// g_player_spawn.cpp
//
void G_LocateSpawnSpots();
void MoveClientToFreeCam(gentity_t *ent);

//
// g_target.cpp
//
void target_laser_think(gentity_t *self);
void target_laser_off(gentity_t *self);
void Locations_Reset();
void Locations_Finalize();
std::string Location_NameFor(const gentity_t *ent);

constexpr SpawnFlags SPAWNFLAG_LASER_ON = 0x0001_spawnflag;
constexpr SpawnFlags SPAWNFLAG_LASER_RED = 0x0002_spawnflag;
constexpr SpawnFlags SPAWNFLAG_LASER_GREEN = 0x0004_spawnflag;
constexpr SpawnFlags SPAWNFLAG_LASER_BLUE = 0x0008_spawnflag;
constexpr SpawnFlags SPAWNFLAG_LASER_YELLOW = 0x0010_spawnflag;
constexpr SpawnFlags SPAWNFLAG_LASER_ORANGE = 0x0020_spawnflag;
constexpr SpawnFlags SPAWNFLAG_LASER_FAT = 0x0040_spawnflag;
constexpr SpawnFlags SPAWNFLAG_LASER_ZAP = 0x80000000_spawnflag;
constexpr SpawnFlags SPAWNFLAG_LASER_LIGHTNING = 0x10000_spawnflag;

constexpr SpawnFlags SPAWNFLAG_HEALTHBAR_PVS_ONLY = 1_spawnflag;

// damage flags
enum class DamageFlags {
  Normal = 0x00000000,      // no damage flags
  Radius = 0x00000001,      // damage was indirect
  NoArmor = 0x00000002,     // armour does not protect from this damage
  Energy = 0x00000004,      // damage is from an energy based weapon
  NoKnockback = 0x00000008, // do not affect velocity, just view angles
  Bullet = 0x00000010,      // damage is from a bullet (used for ricochets)
  NoProtection =
      0x00000020, // armor, shields, protection, godmode etc. have no effect
  DestroyArmor = 0x00000040,   // damage is done to armor and health.
  NoRegularArmor = 0x00000080, // damage skips regular armor
  NoPowerArmor = 0x00000100,   // damage skips power armor
  NoIndicator = 0x00000200,    // for clients: no damage indicators
  StatOnce = 0x00000400        // only add as a hit to player stats once
};

MAKE_ENUM_BITFLAGS(DamageFlags);

struct DamageProtectionContext {
  bool hasClient = false;
  bool combatDisabled = false;
  bool proBall = false;
  bool selfDamageDisabled = false;
  bool isSelfDamage = false;
  bool hasBattleSuit = false;
  bool isRadiusDamage = false;
  bool hasGodMode = false;
  bool isMonster = false;
  GameTime monsterInvincibilityTime = 0_ms;
  GameTime painDebounceTime = 0_ms;
  GameTime levelTime = 0_ms;
};

struct DamageProtectionResult {
  bool prevented = false;
  bool playBattleSuitSound = false;
  bool playMonsterSound = false;
  GameTime newPainDebounceTime = 0_ms;
};

inline DamageProtectionResult
EvaluateDamageProtection(const DamageProtectionContext &ctx, DamageFlags dFlags,
                         const MeansOfDeath &mod) {
  DamageProtectionResult result{};

  if (static_cast<int>(dFlags & DamageFlags::NoProtection))
    return result;

  if (ctx.hasClient) {
    if (ctx.combatDisabled || ctx.proBall) {
      result.prevented = true;
      return result;
    }

    if (ctx.isSelfDamage && ctx.selfDamageDisabled) {
      result.prevented = true;
      return result;
    }
  }

  if (mod.id == ModID::Railgun_Splash) {
    result.prevented = true;
    return result;
  }

  if (ctx.hasClient && ctx.hasBattleSuit && ctx.isRadiusDamage) {
    result.prevented = true;
    result.playBattleSuitSound = true;
    return result;
  }

  if (ctx.hasGodMode) {
    result.prevented = true;
    return result;
  }

  if (ctx.isMonster && (ctx.monsterInvincibilityTime > ctx.levelTime)) {
    result.prevented = true;

    if (ctx.painDebounceTime < ctx.levelTime) {
      result.playMonsterSound = true;
      result.newPainDebounceTime = ctx.levelTime + 2_sec;
    }

    return result;
  }

  return result;
}

//
// g_combat.cpp
//
bool OnSameTeam(gentity_t *ent1, gentity_t *ent2);
bool CanDamage(gentity_t *targ, gentity_t *inflictor);
bool CheckTeamDamage(gentity_t *targ, gentity_t *attacker);
void Damage(gentity_t *targ, gentity_t *inflictor, gentity_t *attacker,
            const Vector3 &dir, const Vector3 &point, const Vector3 &normal,
            int damage, int knockback, DamageFlags damageFlags,
            MeansOfDeath mod);
bool RadiusDamage(gentity_t *inflictor, gentity_t *attacker, float damage,
                  gentity_t *ignore, float radius, DamageFlags damageFlags,
                  MeansOfDeath mod);
void Killed(gentity_t *targ, gentity_t *inflictor, gentity_t *attacker,
            int damage, const Vector3 &point, MeansOfDeath mod);

void RadiusNukeDamage(gentity_t *inflictor, gentity_t *attacker, float damage,
                      gentity_t *ignore, float radius, MeansOfDeath mod);

constexpr int32_t DEFAULT_BULLET_HSPREAD = 500; // 300;
constexpr int32_t DEFAULT_BULLET_VSPREAD = 500;
constexpr int32_t DEFAULT_SHOTGUN_HSPREAD = 1000;
constexpr int32_t DEFAULT_SHOTGUN_VSPREAD = 500;
constexpr int32_t DEFAULT_SHOTGUN_COUNT = 10;
constexpr int32_t DEFAULT_SSHOTGUN_COUNT = DEFAULT_SHOTGUN_COUNT * 2;

//
// g_func.cpp
//
void train_use(gentity_t *self, gentity_t *other, gentity_t *activator);
void func_train_find(gentity_t *self);
gentity_t *plat_spawn_inside_trigger(gentity_t *ent);
void Move_Calc(gentity_t *ent, const Vector3 &dest,
               void (*endFunc)(gentity_t *self));
void G_SetMoveinfoSounds(gentity_t *self, const char *default_start,
                         const char *default_mid, const char *default_end);

constexpr SpawnFlags SPAWNFLAG_TRAIN_START_ON = 1_spawnflag;
constexpr SpawnFlags SPAWNFLAG_WATER_SMART = 2_spawnflag;
constexpr SpawnFlags SPAWNFLAG_TRAIN_MOVE_TEAMCHAIN = 8_spawnflag;
constexpr SpawnFlags SPAWNFLAG_DOOR_REVERSE = 2_spawnflag;

//
// g_horde.cpp
//
void Horde_RunSpawning();

//
// g_monster.cpp
//
void monster_muzzleflash(gentity_t *self, const Vector3 &start,
                         MonsterMuzzleFlashID id);
void monster_fire_bullet(gentity_t *self, const Vector3 &start,
                         const Vector3 &dir, int damage, int kick, int hSpread,
                         int vSpread, MonsterMuzzleFlashID flashType);
void monster_fire_shotgun(gentity_t *self, const Vector3 &start,
                          const Vector3 &aimDir, int damage, int kick,
                          int hSpread, int vSpread, int count,
                          MonsterMuzzleFlashID flashType);
void monster_fire_blaster(gentity_t *self, const Vector3 &start,
                          const Vector3 &dir, int damage, int speed,
                          MonsterMuzzleFlashID flashType, Effect effect);
void monster_fire_flechette(gentity_t *self, const Vector3 &start,
                            const Vector3 &dir, int damage, int speed,
                            MonsterMuzzleFlashID flashType);
void monster_fire_grenade(gentity_t *self, const Vector3 &start,
                          const Vector3 &aimDir, int damage, int speed,
                          MonsterMuzzleFlashID flashType, float rightAdjust,
                          float upAdjust);
void monster_fire_flakcannon(gentity_t *self, const Vector3 &start,
                             const Vector3 &aimDir, int damage, int speed,
                             int hSpread, int vSpread, int count,
                             MonsterMuzzleFlashID flashType);
void monster_fire_multigrenade(gentity_t *self, const Vector3 &start,
                               const Vector3 &aimDir, int damage, int speed,
                               MonsterMuzzleFlashID flashType,
                               float rightAdjust, float upAdjust);
void monster_fire_rocket(gentity_t *self, const Vector3 &start,
                         const Vector3 &dir, int damage, int speed,
                         MonsterMuzzleFlashID flashType);
Vector3 M_ProjectFlashSource(gentity_t *self, const Vector3 &offset,
                             const Vector3 &forward, const Vector3 &right);
void monster_fire_homing_pod(gentity_t *self, const Vector3 &start,
                             const Vector3 &dir, int damage, int speed,
                             MonsterMuzzleFlashID flashType);
void monster_fire_railgun(gentity_t *self, const Vector3 &start,
                          const Vector3 &aimDir, int damage, int kick,
                          MonsterMuzzleFlashID flashType);
void monster_fire_bfg(gentity_t *self, const Vector3 &start,
                      const Vector3 &aimDir, int damage, int speed, int kick,
                      float splashRadius, MonsterMuzzleFlashID flashType);
void fire_acid(gentity_t *self, const Vector3 &start, const Vector3 &dir,
               int damage, int speed);
bool TryRandomTeleportPosition(gentity_t *self, float radius,
                               GameTime returnDelay);
bool M_CheckClearShot(gentity_t *self, const Vector3 &offset);
bool M_CheckClearShot(gentity_t *self, const Vector3 &offset, Vector3 &start);
bool M_droptofloor_generic(Vector3 &origin, const Vector3 &mins,
                           const Vector3 &maxs, const Vector3 &gravity,
                           bool ceiling, gentity_t *ignore, contents_t mask,
                           bool allow_partial);
bool M_droptofloor(gentity_t *ent);
void monster_think(gentity_t *self);
void monster_dead_think(gentity_t *self);
void monster_dead(gentity_t *self);
void walkmonster_start(gentity_t *self);
void swimmonster_start(gentity_t *self);
void flymonster_start(gentity_t *self);
void monster_death_use(gentity_t *self);
void M_CatagorizePosition(gentity_t *self, const Vector3 &in_point,
                          water_level_t &waterLevel, contents_t &waterType);
void M_WorldEffects(gentity_t *ent);
bool M_CheckAttack(gentity_t *self);
void M_CheckGround(gentity_t *ent, contents_t mask);
void monster_use(gentity_t *self, gentity_t *other, gentity_t *activator);
void M_ProcessPain(gentity_t *e);
bool M_ShouldReactToPain(gentity_t *self, const MeansOfDeath &mod);
void M_SetAnimation(gentity_t *self, const save_mmove_t &move,
                    bool instant = true);
bool M_AllowSpawn(gentity_t *self);
void M_CleanupHealTarget(gentity_t *ent);

// Paril: used in N64. causes them to be mad at the player
// regardless of circumstance.
constexpr size_t HACKFLAG_ATTACK_PLAYER = 1;
// used in N64, appears to change their behavior for the end scene.
constexpr size_t HACKFLAG_END_CUTSCENE = 4;

bool monster_start(gentity_t *self);
void monster_start_go(gentity_t *self);

void monster_fire_ionripper(gentity_t *self, const Vector3 &start,
                            const Vector3 &dir, int damage, int speed,
                            MonsterMuzzleFlashID flashType, Effect effect);
void monster_fire_heat(gentity_t *self, const Vector3 &start,
                       const Vector3 &dir, int damage, int speed,
                       MonsterMuzzleFlashID flashType, float lerp_factor);
void monster_fire_dabeam(gentity_t *self, int damage, bool secondary,
                         void (*update_func)(gentity_t *self));
void dabeam_update(gentity_t *self, bool damage);
void monster_fire_blueblaster(gentity_t *self, const Vector3 &start,
                              const Vector3 &dir, int damage, int speed,
                              MonsterMuzzleFlashID flashType, Effect effect);
void G_Monster_CheckCoopHealthScaling();

void monster_fire_blaster2(gentity_t *self, const Vector3 &start,
                           const Vector3 &dir, int damage, int speed,
                           MonsterMuzzleFlashID flashType, Effect effect);
void monster_fire_disruptor(gentity_t *self, const Vector3 &start,
                            const Vector3 &dir, int damage, int speed,
                            gentity_t *enemy, MonsterMuzzleFlashID flashType);
void monster_fire_heatbeam(gentity_t *self, const Vector3 &start,
                           const Vector3 &dir, const Vector3 &offset,
                           int damage, int kick,
                           MonsterMuzzleFlashID flashType);
void stationarymonster_start(gentity_t *self);
void monster_done_dodge(gentity_t *self);

StuckResult G_FixStuckObject(gentity_t *self, Vector3 check);

// this is for the count of monsters
int32_t M_SlotsLeft(gentity_t *self);

// shared with monsters
constexpr SpawnFlags SPAWNFLAG_MONSTER_AMBUSH = 1_spawnflag;
constexpr SpawnFlags SPAWNFLAG_MONSTER_TRIGGER_SPAWN = 2_spawnflag;
// constexpr SpawnFlags SPAWNFLAG_MONSTER_SIGHT			= 4_spawnflag;
// // not currently used?? FUBAR?
constexpr SpawnFlags SPAWNFLAG_MONSTER_CORPSE = 16_spawnflag_bit;
constexpr SpawnFlags SPAWNFLAG_MONSTER_SUPER_STEP = 17_spawnflag_bit;
constexpr SpawnFlags SPAWNFLAG_MONSTER_NO_DROP = 18_spawnflag_bit;
constexpr SpawnFlags SPAWNFLAG_MONSTER_SCENIC = 19_spawnflag_bit;

// fixbot spawnflags
constexpr SpawnFlags SPAWNFLAG_FIXBOT_FIXIT = 4_spawnflag;
constexpr SpawnFlags SPAWNFLAG_FIXBOT_TAKEOFF = 8_spawnflag;
constexpr SpawnFlags SPAWNFLAG_FIXBOT_LANDING = 16_spawnflag;
constexpr SpawnFlags SPAWNFLAG_FIXBOT_WORKING = 32_spawnflag;

//
// g_misc.cpp
//
void Respawn_Think(gentity_t *ent);
void ThrowClientHead(gentity_t *self, int damage);
void gib_die(gentity_t *self, gentity_t *inflictor, gentity_t *attacker,
             int damage, const Vector3 &point, const MeansOfDeath &mod);
gentity_t *ThrowGib(gentity_t *self, std::string gibname, int damage,
                    gib_type_t type, float scale);
void BecomeExplosion1(gentity_t *self);
void misc_viper_use(gentity_t *self, gentity_t *other, gentity_t *activator);
void misc_strogg_ship_use(gentity_t *self, gentity_t *other,
                          gentity_t *activator);
void VelocityForDamage(int damage, Vector3 &v);
void ClipGibVelocity(gentity_t *ent);

constexpr SpawnFlags SPAWNFLAG_PATH_CORNER_TELEPORT = 1_spawnflag;

constexpr SpawnFlags SPAWNFLAG_POINT_COMBAT_HOLD = 1_spawnflag;

// max chars for a clock string;
// " 0:00:00" is the longest string possible
// plus null terminator.
constexpr size_t CLOCK_MESSAGE_SIZE = 9;

//
// g_ai.cpp
//
gentity_t *AI_GetSightClient(gentity_t *self);

void ai_stand(gentity_t *self, float dist);
void ai_move(gentity_t *self, float dist);
void ai_walk(gentity_t *self, float dist);
void ai_turn(gentity_t *self, float dist);
void ai_run(gentity_t *self, float dist);
void ai_charge(gentity_t *self, float dist);

constexpr float RANGE_MELEE = 20; // bboxes basically touching
constexpr float RANGE_NEAR = 440;
constexpr float RANGE_MID = 940;

// [Paril-KEX] adjusted to return an actual distance, measured
// in a way that is consistent regardless of what is fighting what
float range_to(gentity_t *self, gentity_t *other);

void FoundTarget(gentity_t *self);
void HuntTarget(gentity_t *self, bool animate_state = true);
bool infront(gentity_t *self, gentity_t *other);
bool visible(gentity_t *self, gentity_t *other, bool through_glass = true);
bool FacingIdeal(gentity_t *self);
// [Paril-KEX] generic function
bool M_CheckAttack_Base(gentity_t *self, float stand_ground_chance,
                        float melee_chance, float near_chance, float mid_chance,
                        float far_chance, float strafe_scalar);

//
// g_weapon.cpp
//
bool fire_hit(gentity_t *self, Vector3 aim, int damage, int kick);
void fire_bullet(gentity_t *self, const Vector3 &start, const Vector3 &aimDir,
                 int damage, int kick, int hSpread, int vSpread,
                 MeansOfDeath mod);
void fire_shotgun(gentity_t *self, const Vector3 &start, const Vector3 &aimDir,
                  int damage, int kick, int hSpread, int vSpread, int count,
                  MeansOfDeath mod);
void blaster_touch(gentity_t *self, gentity_t *other, const trace_t &tr,
                   bool otherTouchingSelf);
void fire_blaster(gentity_t *self, const Vector3 &start, const Vector3 &aimDir,
                  int damage, int speed, Effect effect, MeansOfDeath mod,
                  bool altNoise);
void fire_blueblaster(gentity_t *self, const Vector3 &start,
                      const Vector3 &aimDir, int damage, int speed,
                      Effect effect);
void fire_greenblaster(gentity_t *self, const Vector3 &start,
                       const Vector3 &aimDir, int damage, int speed,
                       Effect effect, bool hyper);
void fire_grenade(gentity_t *self, const Vector3 &start, const Vector3 &aimDir,
                  int damage, int speed, GameTime timer, float splashRadius,
                  float rightAdjust, float upAdjust, bool monster);
void fire_handgrenade(gentity_t *self, const Vector3 &start,
                      const Vector3 &aimDir, int damage, int speed,
                      GameTime timer, float splashRadius, bool held);
gentity_t *fire_rocket(gentity_t *self, const Vector3 &start,
                       const Vector3 &dir, int damage, int speed,
                       float splashRadius, int splashDamage);
void fire_rail(gentity_t *self, const Vector3 &start, const Vector3 &aimDir,
               int damage, int kick);
void fire_bfg(gentity_t *self, const Vector3 &start, const Vector3 &dir,
              int damage, int speed, float splashRadius);
void fire_ionripper(gentity_t *self, const Vector3 &start,
                    const Vector3 &aimDir, int damage, int speed,
                    Effect effect);
void fire_plasmagun(gentity_t *self, const Vector3 &start, const Vector3 &dir,
                    int damage, int speed, float splashRadius,
                    int splashDamage);
void fire_heat(gentity_t *self, const Vector3 &start, const Vector3 &dir,
               int damage, int speed, float splashRadius, int splashDamage,
               float turnFraction);
void fire_phalanx(gentity_t *self, const Vector3 &start, const Vector3 &dir,
                  int damage, int speed, float splashRadius, int splashDamage);
void fire_trap(gentity_t *self, const Vector3 &start, const Vector3 &aimDir,
               int speed);
void fire_plasmabeam(gentity_t *self, const Vector3 &start,
                     const Vector3 &aimDir, const Vector3 &offset, int damage,
                     int kick, bool monster);
bool fire_thunderbolt(gentity_t *self, const Vector3 &start,
                      const Vector3 &aimDir, const Vector3 &offset, int damage,
                      int kick, MeansOfDeath mod, int damageMultiplier);
void fire_disruptor(gentity_t *self, const Vector3 &start, const Vector3 &dir,
                    int damage, int speed, gentity_t *enemy);
void fire_flechette(gentity_t *self, const Vector3 &start, const Vector3 &dir,
                    int damage, int speed, int kick);
void fire_prox(gentity_t *self, const Vector3 &start, const Vector3 &aimDir,
               int damage, int speed);
void fire_nuke(gentity_t *self, const Vector3 &start, const Vector3 &aimDir,
               int speed);
bool fire_player_melee(gentity_t *self, const Vector3 &start,
                       const Vector3 &aim, int reach, int damage, int kick,
                       MeansOfDeath mod);
void fire_tesla(gentity_t *self, const Vector3 &start, const Vector3 &aimDir,
                int damage, int speed);
void fire_disintegrator(gentity_t *self, const Vector3 &start,
                        const Vector3 &dir, int speed);
void fire_homing_pod(gentity_t *self, const Vector3 &start, const Vector3 &dir,
                     int damage, int speed, MonsterMuzzleFlashID flashType);
int G_ExplodeNearbyMinesSafe(const Vector3 &origin, float radius,
                             gentity_t *safe);

Vector3 P_CurrentKickAngles(gentity_t *ent);
Vector3 P_CurrentKickOrigin(gentity_t *ent);
void P_AddWeaponKick(gentity_t *ent, const Vector3 &origin,
                     const Vector3 &angles);

void Nuke_Explode(gentity_t *ent);

// we won't ever pierce more than this many entities for a single trace.
constexpr size_t MAX_PIERCE = 16;

// base class for pierce args; this stores
// the stuff we are piercing.
struct pierce_args_t {
  // stuff we pierced
  std::array<gentity_t *, MAX_PIERCE> pierced;
  std::array<solid_t, MAX_PIERCE> pierce_solidities;
  size_t num_pierced = 0;
  // the last trace that was done, when piercing stopped
  trace_t tr;

  // mark entity as pierced
  inline bool mark(gentity_t *ent);

  // restore entities' previous solidities
  inline void restore();

  // we hit an entity; return false to stop the piercing.
  // you can adjust the mask for the re-trace (for water, etc).
  virtual bool hit(contents_t &mask, Vector3 &end) = 0;

  virtual ~pierce_args_t() { restore(); }
};

void pierce_trace(const Vector3 &start, const Vector3 &end, gentity_t *ignore,
                  pierce_args_t &pierce, contents_t mask);

//
// g_ptrail.cpp
//
void PlayerTrail_Add(gentity_t *player);
void PlayerTrail_Destroy(gentity_t *player);
gentity_t *PlayerTrail_Pick(gentity_t *self, bool next);
void PlayerShellLight_Destroy(gentity_t *player);

//
// g_client.cpp
//
constexpr SpawnFlags SPAWNFLAG_CHANGELEVEL_CLEAR_INVENTORY = 8_spawnflag;
constexpr SpawnFlags SPAWNFLAG_CHANGELEVEL_NO_END_OF_UNIT = 16_spawnflag;
constexpr SpawnFlags SPAWNFLAG_CHANGELEVEL_FADE_OUT = 32_spawnflag;
constexpr SpawnFlags SPAWNFLAG_CHANGELEVEL_IMMEDIATE_LEAVE = 64_spawnflag;

void ClientRespawn(gentity_t *ent);
bool FreezeTag_IsActive();
bool FreezeTag_IsFrozen(const gentity_t *ent);
void FreezeTag_ForceRespawn(gentity_t *ent);
void BeginIntermission(gentity_t *targ);
void ClientSpawn(gentity_t *ent);
void InitClientPersistant(gentity_t *ent, gclient_t *client);
void InitBodyQue();
void CopyToBodyQue(gentity_t *ent);
void ClientBeginServerFrame(gentity_t *ent);
void ClientUserinfoChanged(gentity_t *ent, const char *userInfo);
void P_AssignClientSkinNum(gentity_t *ent);
void P_ForceFogTransition(gentity_t *ent, bool instant);
void P_SendLevelPOI(gentity_t *ent);
unsigned int P_GetLobbyUserNum(const gentity_t *player);
std::string G_EncodedPlayerName(gentity_t *player);
void TossClientItems(gentity_t *self);
bool G_LimitedLivesRespawn(gentity_t *ent);
void EndOfUnitMessage();
bool SelectSpawnPoint(gentity_t *ent, Vector3 &origin, Vector3 &angles,
                      bool force_spawn, bool &landmark);
void PCfg_WriteConfig(gentity_t *ent);
void PCfg_ClientInitPConfig(gentity_t *ent);

enum class SelectSpawnFlags {
  None = 0,
  Normal = 1 << 0,
  Intermission = 1 << 1,
  Initial = 1 << 2,
  Fallback = 1 << 3
};
MAKE_ENUM_BITFLAGS(SelectSpawnFlags);

struct select_spawn_result_t {
  gentity_t *spot;
  SelectSpawnFlags flags;
};

select_spawn_result_t
SelectDeathmatchSpawnPoint(gentity_t *ent, Vector3 avoid_point,
                           bool force_spawn, bool fallback_to_ctf_or_start,
                           bool intermission, bool initial);
void G_PostRespawn(gentity_t *self);

//
// g_client_cfg.cpp
//
inline std::string SanitizeSocialID(std::string_view socialID) {
  std::string sanitized;
  sanitized.reserve(socialID.size());

  for (char ch : socialID) {
    const bool is_digit = ch >= '0' && ch <= '9';
    const bool is_upper = ch >= 'A' && ch <= 'Z';
    const bool is_lower = ch >= 'a' && ch <= 'z';
    if (is_digit || is_upper || is_lower || ch == '-' || ch == '_') {
      sanitized.push_back(ch);
    }
  }

  return sanitized;
}

//
// g_capture.cpp
//
bool CTF_PickupFlag(gentity_t *ent, gentity_t *other);
void CTF_DropFlag(gentity_t *ent, Item *item);
void CTF_ClientEffects(gentity_t *player);
void CTF_DeadDropFlag(gentity_t *self);
void CTF_FlagSetup(gentity_t *ent);
bool CTF_ResetTeamFlag(Team team);
void CTF_ResetFlags();
void CTF_ScoreBonuses(gentity_t *targ, gentity_t *inflictor,
                      gentity_t *attacker);
void CTF_CheckHurtCarrier(gentity_t *targ, gentity_t *attacker);

bool Harvester_PickupSkull(gentity_t *ent, gentity_t *other);
void Harvester_Reset();
void Harvester_HandlePlayerDeath(gentity_t *victim);
void Harvester_HandlePlayerDisconnect(gentity_t *ent);
void Harvester_HandleTeamChange(gentity_t *ent);
void Harvester_OnClientSpawn(gentity_t *ent);
void SP_team_redobelisk(gentity_t *ent);
void SP_team_blueobelisk(gentity_t *ent);
void SP_team_neutralobelisk(gentity_t *ent);

//
// g_player.cpp
//
void player_die(gentity_t *self, gentity_t *inflictor, gentity_t *attacker,
                int damage, const Vector3 &point, const MeansOfDeath &mod);

//
// g_svcmds.cpp
//
void ServerCommand();
bool G_FilterPacket(const char *from);
void G_LoadIPFilters();
void G_SaveIPFilters();

//
// p_view.cpp
//
void ClientEndServerFrame(gentity_t *ent);
void LagCompensate(gentity_t *from_player, const Vector3 &start,
                   const Vector3 &dir);
void UnLagCompensate();

//
// p_hud_main.cpp
//
void MoveClientToIntermission(gentity_t *ent);
void SetStats(gentity_t *ent);
void SetCoopStats(gentity_t *ent);
void SetSpectatorStats(gentity_t *ent);
void CheckFollowStats(gentity_t *ent);
void ValidateSelectedItem(gentity_t *ent);
void DeathmatchScoreboardMessage(gentity_t *ent, gentity_t *killer);
void TeamsScoreboardMessage(gentity_t *ent, gentity_t *killer);
void ReportMatchDetails(bool is_end);
void UpdateLevelEntry();
void DrawHelpComputer(gentity_t *ent);

//
// p_hud_scoreboard.cpp
//
void MultiplayerScoreboard(gentity_t *ent);

//
// p_weapon.cpp
//
void G_PlayerNoise(gentity_t *who, const Vector3 &where, PlayerNoise type);
void P_ProjectSource(gentity_t *ent, const Vector3 &angles, Vector3 distance,
                     Vector3 &result_start, Vector3 &result_dir);
void NoAmmoWeaponChange(gentity_t *ent, bool sound);
void Weapon_Generic(gentity_t *ent, int FRAME_ACTIVATE_LAST,
                    int FRAME_FIRE_LAST, int FRAME_IDLE_LAST,
                    int FRAME_DEACTIVATE_LAST, const int *pause_frames,
                    const int *fire_frames, void (*fire)(gentity_t *ent));
void Weapon_Repeating(gentity_t *ent, int FRAME_ACTIVATE_LAST,
                      int FRAME_FIRE_LAST, int FRAME_IDLE_LAST,
                      int FRAME_DEACTIVATE_LAST, const int *pause_frames,
                      void (*fire)(gentity_t *ent));
void Throw_Generic(gentity_t *ent, int FRAME_FIRE_LAST, int FRAME_IDLE_LAST,
                   int FRAME_PRIME_SOUND, const char *prime_sound,
                   int FRAME_THROW_HOLD, int FRAME_THROW_FIRE,
                   const int *pause_frames, int EXPLODE,
                   const char *primed_sound,
                   void (*fire)(gentity_t *ent, bool held),
                   bool extra_idle_frame, item_id_t ammoOverride = IT_TOTAL);
uint8_t PlayerDamageModifier(gentity_t *ent);
bool InfiniteAmmoOn(Item *item);
void Weapon_PowerupSound(gentity_t *ent);
void Client_RebuildWeaponPreferenceOrder(gclient_t &cl);
std::vector<std::string> GetSanitizedWeaponPrefStrings(const gclient_t &cl);

// GRAPPLE
void Weapon_Grapple(gentity_t *ent);
void Weapon_Grapple_DoReset(gclient_t *cl);
void Weapon_Grapple_Pull(gentity_t *self);
void Weapon_ForceIdle(gentity_t *ent);

// HOOK
void Weapon_Hook(gentity_t *ent);

constexpr GameTime GRENADE_TIMER = 3_sec;
constexpr float GRENADE_MINSPEED = 400.f;
constexpr float GRENADE_MAXSPEED = 800.f;

extern player_muzzle_t is_silenced;
extern byte damage_multiplier;

//
// m_move.cpp
//
bool M_CheckBottom_Fast_Generic(const Vector3 &absmins, const Vector3 &absmaxs,
                                const Vector3 &gravityDir);
bool M_CheckBottom_Slow_Generic(const Vector3 &origin, const Vector3 &absmins,
                                const Vector3 &absmaxs, gentity_t *ignore,
                                contents_t mask, const Vector3 &gravityDir,
                                bool allow_any_step_height);
bool M_CheckBottom(gentity_t *ent);
bool G_CloseEnough(gentity_t *ent, gentity_t *goal, float dist);
bool M_walkmove(gentity_t *ent, float yaw, float dist);
void M_MoveToGoal(gentity_t *ent, float dist);
void M_ChangeYaw(gentity_t *ent);
bool ai_check_move(gentity_t *self, float dist);

//
// g_phys.cpp
//
constexpr float g_friction = 6;
constexpr float g_water_friction = 1;

void G_RunEntity(gentity_t *ent);
bool G_RunThink(gentity_t *ent);
void G_AddRotationalFriction(gentity_t *ent);
void G_AddGravity(gentity_t *ent);
void G_CheckVelocity(gentity_t *ent);
void G_FlyMove(gentity_t *ent, float time, contents_t mask);
contents_t G_GetClipMask(gentity_t *ent);
void G_Impact(gentity_t *e1, const trace_t &trace);

//
// g_main.cpp
//
void SaveClientData();
void FetchClientEntData(gentity_t *ent);
void FindIntermissionPoint(void);
void G_RevertVote(gclient_t *client);
void Vote_Passed();
void ExitLevel(bool forceImmediate = false);
void Teams_CalcRankings(
    std::array<uint32_t, MAX_CLIENTS> &playerRanks); // [Paril-KEX]
void ReadyAll();
void UnReadyAll();
void QueueIntermission(const char *msg, bool boo, bool reset);
void Match_Reset();
gentity_t *CreateTargetChangeLevel(std::string_view map);
bool InAMatch();
void LoadMotd();
void LoadAdminList();
void LoadBanList();
bool AppendIDToFile(const char *filename, const std::string &id);
bool RemoveIDFromFile(const char *filename, const std::string &id);

//
// match_state.cpp
//
void Match_Start();
void Match_Reset();
void Round_End();

//
// tournament.cpp
//
bool Tournament_IsActive();
bool Tournament_ConfigIsValid(std::string_view configName = {});
bool Tournament_LoadConfig(std::string_view configName = {},
                           std::string *error = nullptr);
bool Tournament_AllParticipantsConnected();
bool Tournament_AllParticipantsReady();
bool Tournament_IsParticipant(const gclient_t *cl);
Team Tournament_AssignedTeam(const gclient_t *cl);
bool Tournament_StartVetoIfReady();
bool Tournament_VetoComplete();
bool Tournament_HandleVetoAction(gentity_t *ent, TournamentVetoAction action,
                                 std::string_view mapName,
                                 std::string &message);
std::string Tournament_GetVetoStatus();
bool Tournament_GetNextMap(std::string &mapName);
bool Tournament_ReplayGame(int gameNumber, std::string &message);

//
// g_map_manager.cpp
//
constexpr GameTime MAP_SELECTOR_DURATION = 5_sec;
int PrintMapList(gentity_t *ent, bool cycleOnly);
void LoadMapPool(gentity_t *ent);
void LoadMapCycle(gentity_t *ent);
std::optional<MapEntry> AutoSelectNextMap();
std::vector<const MapEntry *> MapSelectorVoteCandidates(int maxCandidates = 3);
void MapSelector_ClearVote(LevelLocals &levelState, int clientIndex);
int MapSelector_SyncVotes(LevelLocals &levelState);
int PrintMapListFiltered(gentity_t *ent, bool cycleOnly,
                         const std::string &filterQuery);

//
// match_state.cpp
//
void Marathon_RegisterClientBaseline(gclient_t *cl);
void CheckDMExitRules();
int GT_ScoreLimit();
const char *GT_ScoreLimitString();
void ChangeGametype(GameType gt);
void Match_End();
void Match_UpdateDuelRecords();

//
// match_logging.cpp
//

//
// g_chase.cpp
//
void FreeFollower(gentity_t *ent);
void FreeClientFollowers(gentity_t *ent);
void ClientUpdateFollowers(gentity_t *ent);
void FollowNext(gentity_t *ent);
void FollowPrev(gentity_t *ent);
void GetFollowTarget(gentity_t *ent);

//====================
// ROGUE PROTOTYPES

//
// g_ai_new.cpp
//
bool blocked_checkplat(gentity_t *self, float dist);

enum class BlockedJumpResult : uint8_t {
  No_Jump,
  Jump_Turn,
  Jump_Turn_Up,
  Jump_Turn_Down
};

BlockedJumpResult blocked_checkjump(gentity_t *self, float dist);
bool monsterlost_checkhint(gentity_t *self);
bool inback(gentity_t *self, gentity_t *other);
float realrange(gentity_t *self, gentity_t *other);
gentity_t *SpawnBadArea(const Vector3 &mins, const Vector3 &maxs,
                        GameTime lifespan, gentity_t *owner);
gentity_t *CheckForBadArea(gentity_t *ent);
bool MarkTeslaArea(gentity_t *self, gentity_t *tesla);
void InitHintPaths();
void PredictAim(gentity_t *self, gentity_t *target, const Vector3 &start,
                float bolt_speed, bool eye_height, float offset,
                Vector3 *aimDir, Vector3 *aimPoint);
bool M_CalculatePitchToFire(gentity_t *self, const Vector3 &target,
                            const Vector3 &start, Vector3 &aim, float speed,
                            float time_remaining, bool mortar,
                            bool destroy_on_touch = false);
bool below(gentity_t *self, gentity_t *other);
void drawbbox(gentity_t *self);
void M_MonsterDodge(gentity_t *self, gentity_t *attacker, GameTime eta,
                    trace_t *tr, bool gravity);
void monster_duck_down(gentity_t *self);
void monster_duck_hold(gentity_t *self);
void monster_duck_up(gentity_t *self);
bool has_valid_enemy(gentity_t *self);
void TargetTesla(gentity_t *self, gentity_t *tesla);
void hintpath_stop(gentity_t *self);
gentity_t *PickCoopTarget(gentity_t *self);
int CountPlayers();
bool monster_jump_finished(gentity_t *self);
void BossExplode(gentity_t *self);
void Q1BossExplode(gentity_t *self);

// g_rogue_func
void plat2_spawn_danger_area(gentity_t *ent);
void plat2_kill_danger_area(gentity_t *ent);

// g_rogue_spawn
gentity_t *CreateMonster(const Vector3 &origin, const Vector3 &angles,
                         const char *className);
gentity_t *CreateFlyMonster(const Vector3 &origin, const Vector3 &angles,
                            const Vector3 &mins, const Vector3 &maxs,
                            const char *className);
gentity_t *CreateGroundMonster(const Vector3 &origin, const Vector3 &angles,
                               const Vector3 &mins, const Vector3 &maxs,
                               const char *className, float height,
                               bool ceiling = false);
bool FindSpawnPoint(const Vector3 &startpoint, const Vector3 &mins,
                    const Vector3 &maxs, Vector3 &spawnpoint, float maxMoveUp,
                    bool drop = true, bool ceiling = false);
bool CheckSpawnPoint(const Vector3 &origin, const Vector3 &mins,
                     const Vector3 &maxs);
bool CheckGroundSpawnPoint(const Vector3 &origin, const Vector3 &entMins,
                           const Vector3 &entMaxs, float height,
                           bool ceiling = false);
void SpawnGrow_Spawn(const Vector3 &startpos, float start_size, float end_size);
void Widowlegs_Spawn(const Vector3 &startpos, const Vector3 &angles);

//
// g_rogue_sphere.cpp
//
void Defender_Launch(gentity_t *self);
void Vengeance_Launch(gentity_t *self);
void Hunter_Launch(gentity_t *self);

//
// p_client.cpp
//
void RemoveAttackingPainDaemons(gentity_t *self);
bool G_ShouldPlayersCollide(bool weaponry);
bool P_UseCoopInstancedItems();
void PushAward(gentity_t *ent, PlayerMedal medal);
void P_SaveGhostSlot(gentity_t *ent);
void P_RestoreFromGhostSlot(gentity_t *ent);
bool InitPlayerTeam(gentity_t *ent);
void ClientSetReadyStatus(gentity_t &ent, bool state, bool toggle);
void ClientSetReadyStatus(gentity_t *ent, bool state, bool toggle);

constexpr SpawnFlags SPAWNFLAG_LANDMARK_KEEP_Z = 1_spawnflag;

// [Paril-KEX] convenience functions that returns true
// if the powerup should be 'active' (false to disable,
// will flash at 500ms intervals after 3 sec)
[[nodiscard]] constexpr bool G_PowerUpExpiringRelative(GameTime left) {
  return left.milliseconds() > 3000 || (left.milliseconds() % 1000) < 500;
}

[[nodiscard]] constexpr bool G_PowerUpExpiring(GameTime time) {
  return G_PowerUpExpiringRelative(time - level.time);
}

bool ClientIsPlaying(gclient_t *cl);

//============================================================================

// client_t->anim.priority
enum anim_priority_t {
  ANIM_BASIC, // stand / run
  ANIM_WAVE,
  ANIM_JUMP,
  ANIM_PAIN,
  ANIM_ATTACK,
  ANIM_DEATH,

  // flags
  ANIM_REVERSED = bit_v<8>
};

MAKE_ENUM_BITFLAGS(anim_priority_t);

// height fog data values
struct height_fog_t {
  // r g b dist
  std::array<float, 4> start;
  std::array<float, 4> end;
  float falloff;
  float density;

  inline bool operator==(const height_fog_t &o) const {
    return start == o.start && end == o.end && falloff == o.falloff &&
           density == o.density;
  }
};

constexpr GameTime SELECTED_ITEM_TIME = 3_sec;

enum bmodel_animstyle_t : int32_t {
  BMODEL_ANIM_FORWARDS,
  BMODEL_ANIM_BACKWARDS,
  BMODEL_ANIM_RANDOM
};

struct bmodel_anim_t {
  // range, inclusive
  int32_t start, end;
  bmodel_animstyle_t style;
  int32_t speed; // in milliseconds
  bool nowrap;

  int32_t alt_start, alt_end;
  bmodel_animstyle_t alt_style;
  int32_t alt_speed; // in milliseconds
  bool alt_nowrap;

  // game-only
  bool enabled;
  bool alternate, currently_alternate;
  GameTime next_tick;
};

// never turn back shield on automatically; this is
// the legacy behavior.
constexpr int32_t AUTO_SHIELD_MANUAL = -1;
// when it is >= 0, the shield will turn back on
// when we have that many cells in our inventory
// if possible.
constexpr int32_t AUTO_SHIELD_AUTO = 0;

static constexpr int MAX_AWARD_QUEUE = 8;

// client data that stays across multiple level loads in SP, cleared on level
// loads in MP
struct client_persistant_t {
  char userInfo[MAX_INFO_STRING]{};
  char netName[MAX_NETNAME]{};
  Handedness hand = Handedness::Right;
  WeaponAutoSwitch autoswitch = WeaponAutoSwitch::Never;
  int32_t autoshield = 0; // see AUTO_SHIELD_*

  bool connected = false,
       spawned = false; // a loadgame will leave valid entities that
  // just don't have a connection yet

  // values saved and restored from gentities when changing levels
  int32_t health = 100;
  int32_t maxHealth = 100;
  ent_flags_t saved_flags;

  item_id_t selectedItem = IT_NULL;
  GameTime selected_item_time = 0_ms;
  std::array<int32_t, IT_TOTAL> inventory{};

  // ammo capacities
  std::array<int16_t, static_cast<int>(AmmoID::_Total)> ammoMax;

  Item *weapon = nullptr;
  Item *lastWeapon = nullptr;

  int32_t powerCubes = 0; // used for tracking the cubes in coop games
  int32_t score = 0;      // for calculating total unit score in coop games

  int32_t game_help1changed, game_help2changed;
  int32_t helpChanged; // flash F1 icon if non 0, play sound
  // and increment only if 1, 2, or 3
  GameTime help_time = 0_ms;

  bool bob_skip = false; // [Paril-KEX] client wants no movement bob

  // [Paril-KEX] fog that we want to achieve; density rgb skyfogfactor
  std::array<float, 5> wanted_fog;
  height_fog_t wanted_heightfog;
  // relative time value, copied from last touched trigger
  GameTime fog_transition_time = 0_ms;
  GameTime megaTime = 0_ms; // relative megahealth time value
  int32_t lives = 0;        // player lives left (1 = no respawns remaining)
  bool limitedLivesPersist =
      false; // whether the stored lives should survive the next respawn
  int32_t limitedLivesStash = 0; // cached lives value for respawn restoration
  uint8_t n64_crouch_warn_times = 0;
  GameTime n64_crouch_warning = 0_ms;

  // q3:

  int32_t dmg_scorer = 0; // for clan arena scoring from damage dealt
  int32_t dmg_team = 0;   // for team damage checks and warnings

  int skinIconIndex = 0;
  std::string skin;

  int32_t vote_count = 0; // to prevent people from constantly calling votes

  int32_t healthBonus = 0;

  bool timeout_used = false;

  bool holdable_item_msg_adren = false;
  bool holdable_item_msg_tele = false;
  bool holdable_item_msg_doppel = false;

  bool rail_hit = false;
  GameTime lastFragTime = 0_ms;

  GameTime last_spawn_time = 0_ms;

  GameTime introTime = 0_ms;

  // reward medals
  uint32_t medalStack = 0;
  GameTime medalTime = 0_ms;
  PlayerMedal medalType = PlayerMedal::None;
  int32_t medalDisplayCount = 0;
  GameTime announcerNextTime = 0_ms;

  PlayerTeamState teamState{};

  int currentRank = -1, previousRank = -1;

  int voted = false;
  bool readyStatus = false;

  ClientMatchStats match{};

  struct {
    int count[MAX_AWARD_QUEUE]{}; // how many times this award was earned (e.g.,
                                  // 1 or 2)
    int soundIndex[MAX_AWARD_QUEUE]{}; // announcer sound index
    PlayerMedal medal[MAX_AWARD_QUEUE]{};
    int queueSize = 0;
    int playIndex = 0;
    GameTime nextPlayTime = 0_ms;
  } awardQueue;
};

// player config vars:
struct client_config_t {
  bool show_id = true;
  bool show_timer = true;
  bool show_fragmessages = true;
  bool use_eyecam = true;
  int killbeep_num = 1;

  bool follow_killer = false;
  bool follow_leader = false;
  bool follow_powerup = false;
};

// client data that stays across deathmatch level changes, handled differently
// to client_persistent_t
struct client_session_t {
  client_config_t pc;

  char netName[MAX_NETNAME]{};
  char socialID[MAX_INFO_VALUE];
  uint16_t skillRating = 0;
  uint16_t skillRatingChange = 0;

  std::string skinName;
  int skinIconIndex = 0;

  Team team = Team::None;
  Team queuedTeam = Team::None;
  bool inGame = false;
  bool initialised = false;

  // client flags
  bool admin = false;
  bool banned = false;
  bool is_888 = false;
  bool is_a_bot = false;
  bool consolePlayer = false;

  // inactivity timer
  bool inactiveStatus = false;
  GameTime inactivityTime = 0_ms;
  bool inactivityWarning = false;

  // duel stats
  bool matchQueued = false;
  uint64_t duelQueueTicket = 0;
  int matchWins = 0, matchLosses = 0;

  // real time of team joining
  GameTime teamJoinTime = 0_ms;
  int64_t playStartRealTime = 0;
  int64_t playEndRealTime = 0;

  int motdModificationCount = -1;
  bool showed_help = false;

  int command_flood_count = 0;
  GameTime command_flood_time = 0_ms;

  std::vector<Weapon> weaponPrefs;
  std::vector<item_id_t> weaponPrefOrder;
};

// client data that stay across a match
// to change to clearing on respawn
struct client_respawn_t {
  client_persistant_t coopRespawn{}; // what to set client->pers to on a respawn
  GameTime enterTime = 0_ms;         // level.time the client entered the game
  int32_t score = 0;                 // frags, etc
  int32_t oldScore = 0;              // track changes in score
  Vector3 cmdAngles = vec3_origin;   // angles sent over in the last command
  bool hasPendingGhostSpawn =
      false; // awaiting placement using a restored ghost slot
  Vector3 pendingGhostOrigin = vec3_origin; // ghost-slot origin to resume from
  Vector3 pendingGhostAngles = vec3_origin; // ghost-slot angles to resume from

  int32_t ctf_state = 0;
  GameTime ctf_lasthurtcarrier = 0_ms;
  GameTime ctf_lastreturnedflag = 0_ms;
  GameTime ctf_flagsince = 0_ms;
  GameTime ctf_lastfraggedcarrier = 0_ms;

  GameTime lastIDTime = 0_ms; // crosshair ID time

  /*freeze*/
  gentity_t *thawer = nullptr;
  int help = 0;
  int thawed = 0;
  /*freeze*/

  GameTime teamDelayTime = 0_ms;

  int64_t totalMatchPlayRealTime = 0;
};

// [Paril-KEX] seconds until we are fully invisible after
// making a racket
constexpr GameTime INVISIBILITY_TIME = 2_sec;

// max number of individual damage indicators we'll track
constexpr size_t MAX_DAMAGE_INDICATORS = 4;

struct damage_indicator_t {
  Vector3 from;
  int32_t health, armor, power;
};

// time between ladder sounds
constexpr GameTime LADDER_SOUND_TIME = 300_ms;

// time after damage that we can't respawn on a player for
constexpr GameTime COOP_DAMAGE_RESPAWN_TIME = 2000_ms;

// time after firing that we can't respawn on a player for
constexpr GameTime COOP_DAMAGE_FIRING_TIME = 2500_ms;

struct MatchSetupState {
  std::string format = "regular";
  std::string gametype = "ffa";
  std::string modifier = "standard";
  int maxPlayers = 8;
  std::string length = "standard";
  std::string type = "standard";
  std::string bestOf = "bo1";
};

struct UiMapFlagState {
  uint16_t enableFlags = 0;
  uint16_t disableFlags = 0;
};

enum class UiListKind : uint8_t {
  None,
  CallvoteMap,
  CallvoteGametype,
  CallvoteArena,
  MyMap,
  SetupGametype,
  TournamentPick,
  TournamentBan,
  TournamentReplay
};

struct UiListState {
  UiListKind kind = UiListKind::None;
  int page = 0;
};

struct UiMenuState {
  UiMapFlagState callvoteMap{};
  UiMapFlagState mymap{};
  UiListState list{};
  bool voteActive = false;
  GameTime voteNextUpdate = 0_ms;
  bool mapSelectorActive = false;
  GameTime mapSelectorNextUpdate = 0_ms;
  bool matchStatsActive = false;
  GameTime matchStatsNextUpdate = 0_ms;
  MatchSetupState setup{};
  bool setupActive = false;
};

// this structure is cleared on each ClientSpawn(),
// except for 'client->pers'
struct gclient_t {
  // shared with server; do not touch members until the "private" section
  player_state_t ps; // communicated by server to clients
  int32_t ping;

  // private to game
  client_persistant_t pers{};
  client_respawn_t resp{};
  client_session_t sess{};
  pmove_state_t old_pmove{}; // for detecting out-of-pmove changes

  bool showScores = false;    // set layout stat
  bool showEOU = false;       // end of unit screen
  bool showInventory = false; // set layout stat
  bool showHelp = false;
  bool deadFlag = false;

  button_t buttons = BUTTON_NONE;
  button_t oldButtons = BUTTON_NONE;
  button_t latchedButtons = BUTTON_NONE;
  usercmd_t cmd; // last CMD send

  struct {
    // weapon cannot fire until this time is up
    GameTime fireFinished = 0_ms;
    // time between processing individual animation frames
    GameTime thinkTime = 0_ms;
    // if we latched fire between server frames but before
    // the weapon fire finish has elapsed, we'll "press" it
    // automatically when we have a chance
    bool fireBuffered = false;
    bool thunk = false;

    Item *pending = nullptr;
  } weapon;

  // sum up damage over an entire frame, so
  // shotgun blasts give a single big kick
  struct {
    int32_t armor = 0;            // damage absorbed by armor
    int32_t powerArmor = 0;       // damage absorbed by power armor
    int32_t blood = 0;            // damage taken out of health
    int32_t knockback = 0;        // impact damage
    Vector3 origin = vec3_origin; // origin for vector calculation
  } damage;

  damage_indicator_t damageIndicators[MAX_DAMAGE_INDICATORS];
  uint8_t numDamageIndicators = 0;

  float killerYaw; // when dead, look at killer

  WeaponState weaponState = WeaponState::Ready;

  struct {
    Vector3 angles = vec3_origin, origin = vec3_origin;
    GameTime time = 0_sec, total = 0_ms;
  } kick;

  struct {
    GameTime quakeTime = 0_ms;
    Vector3 kickOrigin = vec3_origin;
    float vDamageRoll = 0.0f, vDamagePitch = 0.0f;
    GameTime vDamageTime = 0_ms; // damage kicks
    GameTime fallTime = 0_ms;
    float fallValue = 0.0f; // for view drop on fall
    float damageAlpha = 0.0f;
    float bonusAlpha = 0.0f;
    Vector3 damageBlend = vec3_origin;
    float bobTime = 0.0f;      // so off-ground doesn't change it
    GameTime flashTime = 0_ms; // [Paril-KEX] for high tickrate
  } feedback;

  Vector3 vAngle = vec3_origin;
  Vector3 vForward = vec3_origin; // aiming direction
  Vector3 oldViewAngles = vec3_origin;
  Vector3 oldVelocity = vec3_origin;
  gentity_t *oldGroundEntity; // [Paril-KEX]

  struct {
    bool active = false;
    GameTime startTime = 0_ms;
    Vector3 startOffset = vec3_origin;
  } deathView;

  GameTime nextDrownTime = 0_ms;
  water_level_t oldWaterLevel = WATER_NONE;
  int32_t breatherSound = 0;

  int32_t machinegunShots = 0; // for weapon raising

  // animation vars
  struct {
    int32_t end = 0;
    anim_priority_t priority = ANIM_BASIC;
    bool duck = false;
    bool run = false;
    GameTime time = 0_sec;
  } anim;

  // powerup timers
  std::array<GameTime, PowerupTimerCount> powerupTimers{};
  std::array<uint32_t, PowerupCountCount> powerupCounts{};

  GameTime &PowerupTimer(::PowerupTimer timer) {
    return powerupTimers[ToIndex(timer)];
  }
  const GameTime &PowerupTimer(::PowerupTimer timer) const {
    return powerupTimers[ToIndex(timer)];
  }

  uint32_t &PowerupCount(::PowerupCount counter) {
    return powerupCounts[ToIndex(counter)];
  }
  const uint32_t &PowerupCount(::PowerupCount counter) const {
    return powerupCounts[ToIndex(counter)];
  }

  void ResetPowerups() {
    powerupTimers.fill(0_ms);
    powerupCounts.fill(0);
  }

  GameTime pu_regen_time_blip = 0_ms;
  GameTime pu_time_spawn_protection_blip = 0_ms;

  bool grenadeBlewUp = false;
  GameTime grenadeTime, grenadeFinishedTime;
  int32_t weaponSound = 0;

  GameTime pickupMessageTime = 0_ms;

  GameTime harvesterReminderTime = 0_ms;
  GameTime respawnMinTime = 0_ms; // minimum time delay before we can respawn
  GameTime respawnMaxTime =
      0_ms; // maximum time before we are forced to respawn

  // flood stuff is dm only
  struct {
    GameTime lockUntil = 0_ms;                  // locked from talking
    std::array<GameTime, 10> messageTimes = {}; // when messages were sent
    int32_t time = 0;                           // head pointer for when said
  } flood;

  // follow cam not required to persist
  struct {
    gentity_t *queuedTarget = nullptr;
    GameTime queuedTime = 0_ms;
    gentity_t *target = nullptr; // player we are following
    bool update = false;         // need to update follow info?
  } follow;

  GameTime nukeTime = 0_ms;
  GameTime trackerPainTime = 0_ms;

  gentity_t *ownedSphere; // this points to the player's sphere
  gentity_t *shellLight = nullptr; // dynamic light entity for colored shells

  struct HeadHunterData {
    static constexpr size_t MAX_ATTACHMENTS = 3;

    uint8_t carried = 0;
    std::array<gentity_t *, MAX_ATTACHMENTS> attachments{};
    GameTime pickupCooldown = 0_ms;
    GameTime dropCooldown = 0_ms;
  } headhunter{};

  GameTime emptyClickSound = 0_ms;

  struct {
    GameTime updateTime = 0_ms; // scoreboard refresh timing
  } menu;

  UiMenuState ui{};

  struct {
    gentity_t *entity = nullptr;             // entity of grapple
    GrappleState state = GrappleState::None; // true if pulling
    GameTime releaseTime = 0_ms;             // time of grapple release
  } grapple;

  struct {
    GameTime regenTime = 0_ms;
    GameTime soundTime = 0_ms;
    GameTime lastMessageTime = 0_ms;
  } tech;

  GameTime frenzyAmmoRegenTime = 0_ms;

  GameTime vampiricExpireTime = 0_ms;

  // used for player trails.
  gentity_t *trail_head = nullptr, *trail_tail = nullptr;
  // whether to use weapon chains
  bool noWeaponChains = false;

  // seamless level transitions
  bool landmark_free_fall = false;
  const char *landmark_name;
  Vector3 landmark_rel_pos; // position relative to landmark, un-rotated from
                            // landmark angle
  GameTime landmark_noise_time = 0_ms;

  GameTime invisibility_fade_time =
      0_ms; // [Paril-KEX] at this time, the player will be mostly fully cloaked
  Vector3 last_ladder_pos; // for ladder step sounds
  GameTime last_ladder_sound = 0_ms;
  CoopRespawn coopRespawnState = CoopRespawn::None;
  GameTime last_damage_time = 0_ms;

  // [Paril-KEX] these are now per-player, to work better in coop
  gentity_t *sight_entity = nullptr;
  GameTime sight_entity_time = 0_ms;
  gentity_t *sound_entity = nullptr;
  GameTime sound_entity_time = 0_ms;
  gentity_t *sound2_entity = nullptr;
  GameTime sound2_entity_time = 0_ms;

  GameTime thunderbolt_sound_time = 0_ms;

  // saved positions for lag compensation
  struct {
    uint8_t numOrigins = 0; // 0 to MAX_LAG_ORIGINS, how many we can go back
    uint8_t nextOrigin = 0; // the next one to write to
    bool isCompensated = false;
    Vector3 restoreOrigin = vec3_origin;
  } lag;

  // for high tickrate weapon angles
  Vector3 slow_view_angles = vec3_origin;
  GameTime slow_view_angle_time = 0_ms;

  struct {
    bool spawnBegin = false;          // waiting to spawn at begining of level
    bool useSquad = false;            // use squad for spawning
    Vector3 squadOrigin, squadAngles; // origin and angles of squad
  } coopRespawn;

  // not saved
  struct {
    bool drawPoints = false;
    size_t drawIndex = 0, drawCount = 0;
    GameTime drawTime = 0_ms;
    int32_t poiImage = 0;
    Vector3 poiLocation = vec3_origin;
  } compass;

  uint32_t stepFrame = 0;

  // only set temporarily
  bool awaitingRespawn = false;
  GameTime respawn_timeout = 0_ms; // after this time, force a respawn

  Vector3 lastDeathLocation =
      vec3_origin; // avoid this spawn location next time

  // [Paril-KEX] current active fog values; density rgb skyfogfactor
  std::array<float, 5> fog;
  height_fog_t heightfog;

  GameTime last_attacker_time = 0_ms;
  // saved - for coop; last time we were in a firing state
  GameTime lastFiringTime = 0_ms;

  bool eliminated = false;

  /*freeze*/
  struct {
    GameTime thawTime = 0_ms;
    GameTime frozenTime = 0_ms;
    GameTime holdDeadline = 0_ms;
  } freeze;
  /*freeze*/

  struct {
    GameTime nextPassTime = 0_ms;
    GameTime nextDropTime = 0_ms;
  } ball;

  bool readyToExit = false;

  int last_match_timer_update = 0;

  struct {
      GameTime delay = 0_ms;
      bool shown = false;
      bool frozen = false;
      bool hostSetupDone = false;
      bool dmWelcomeActive = false;
      bool dmJoinActive = false;
    } initialMenu;

  GameTime lastPowerupMessageTime = 0_ms;
  GameTime lastBannedMessageTime = 0_ms;

  GameTime timeResidual = 0_ms;

  int32_t killStreakCount =
      0; // for rampage award, reset on death or team change
};

/*
=============
ClientIsEliminatedFromLimitedLives

Evaluates whether a client with no remaining lives and zero-or-less persistent
health should be treated as eliminated for limited-lives modes such as Horde.
=============
*/
static inline bool ClientIsEliminatedFromLimitedLives(const gclient_t *client) {
  if (!client)
    return false;

  return client->pers.health <= 0 && client->pers.lives <= 0;
}

static inline int ClientScoreForStandings(const gclient_t *client) {
  if (!client)
    return 0;

  if (G_LimitedLivesInLMS()) {
    return client->pers.lives > 0 ? client->pers.lives : 0;
  }

  return client->resp.score;
}

// ==========================================
// PLAT 2
// ==========================================
enum plat2flags_t {
  PLAT2_NONE = 0,
  PLAT2_CALLED = 1,
  PLAT2_MOVING = 2,
  PLAT2_WAITING = 4
};

MAKE_ENUM_BITFLAGS(plat2flags_t);

// for respawning ents from sp in mp
struct saved_spawn_t {
  Vector3 origin;
  Vector3 angles;
  int32_t health;
  int32_t dmg{};
  float scale;
  const char *target = nullptr;
  const char *targetName = nullptr;
  SpawnFlags spawnFlags;
  int32_t mass{};
  const char *className = nullptr;
  gvec3_t mins, maxs; // bounding box size
  const char *model = nullptr;
  void (*spawnFunc)(gentity_t *);
};

struct gentity_t {
  /*
  =============
  gentity_t::gentity_t

  Value-initializes entity state so tests can instantiate lightweight entities
  without relying on external allocation helpers.
  =============
  */
  gentity_t() = default;
  gentity_t(const gentity_t &) = delete;
  gentity_t &operator=(const gentity_t &) = delete;
  gentity_t(gentity_t &&) = default;
  gentity_t &operator=(gentity_t &&) = default;

  // shared with server; do not touch members until the "private" section
  entity_state_t s{};
  gclient_t *client = nullptr; // nullptr if not a player
  // the server expects the first part
  // of gclient_t to be a player_state_t
  // but the rest of it is opaque

  sv_entity_t sv{}; // read only info about this entity for the server

  bool inUse{};

  // world linkage data
  bool linked{};
  int32_t linkCount{};
  int32_t areaNum{};
  int32_t areaNum2{};

  svflags_t svFlags{};
  Vector3 mins{};
  Vector3 maxs{};
  Vector3 absMin{};
  Vector3 absMax{};
  Vector3 size{};
  solid_t solid{};
  contents_t clipMask{};
  gentity_t *owner = nullptr;

  //================================

  // private to game
  int32_t spawn_count{}; // [Paril-KEX] used to differentiate different entities
                         // that may be in the same slot
  MoveType moveType;
  ent_flags_t flags{};

  const char *model = nullptr;
  GameTime freeTime; // sv.time when the object was freed

  //
  // only used locally in game, not by server
  //
  const char *message = nullptr;
  const char *className = nullptr;
  SpawnFlags spawnFlags;
  bool turretFireRequested{};

  GameTime timeStamp{};

  float angle; // set in qe3, -1 = up, -2 = down
  const char *target = nullptr;
  const char *targetName = nullptr;
  const char *killTarget = nullptr;
  const char *team = nullptr;
  const char *pathTarget = nullptr;
  const char *deathTarget = nullptr;
  const char *healthTarget = nullptr;
  const char *itemTarget = nullptr; // [Paril-KEX]
  const char *combatTarget = nullptr;
  gentity_t *targetEnt = nullptr;

  float speed, accel, decel;
  Vector3 moveDir{};
  Vector3 pos1{};
  Vector3 pos2{};
  Vector3 pos3{};

  Vector3 velocity;
  Vector3 aVelocity;
  int32_t mass{};
  GameTime airFinished{};
  float gravity; // per entity gravity multiplier (1.0 is normal)
  uint32_t lastGravityModCount;
  // use for lowgrav artifact, flares

  gentity_t *goalEntity = nullptr;
  gentity_t *moveTarget = nullptr;
  float yawSpeed{};
  float ideal_yaw{};

  GameTime nextThink{};
  save_prethink_t preThink{};
  save_prethink_t postThink{};
  save_think_t think{};
  save_touch_t touch{};
  save_use_t use{};
  save_pain_t pain{};
  save_die_t die{};

  GameTime touch_debounce_time{}; // are all these legit?  do we need more/less
                                  // of them?
  GameTime pain_debounce_time{};
  GameTime damage_debounce_time{};
  GameTime fly_sound_debounce_time{}; // move to clientinfo
  GameTime last_move_time{};

  int32_t health;
  int32_t maxHealth;
  int32_t gibHealth;
  GameTime show_hostile;

  GameTime powerArmorTime{};

  std::array<char, MAX_QPATH> map; // target_changelevel

  int32_t viewHeight{}; // height above origin where eyesight is determined
  bool deadFlag{};
  bool takeDamage{};
  int32_t dmg{};
  int32_t splashDamage{};
  float splashRadius;
  int32_t sounds{}; // make this a spawntemp var?
  int32_t count{};

  gentity_t *chain = nullptr;
  gentity_t *enemy = nullptr;
  gentity_t *oldEnemy = nullptr;
  gentity_t *activator = nullptr;
  gentity_t *groundEntity = nullptr;
  int32_t groundEntity_linkCount;
  gentity_t *teamChain = nullptr;
  gentity_t *teamMaster = nullptr;

  gentity_t *myNoise = nullptr; // can go in client only
  gentity_t *myNoise2 = nullptr;

  int32_t noiseIndex{};
  int32_t noiseIndex2{};
  float volume;
  float attenuation;

  // timing variables
  float wait{};
  float delay{}; // before firing targets
  float random{};

  GameTime teleportTime{};

  contents_t waterType{};
  water_level_t waterLevel{};

  Vector3 moveOrigin{};
  Vector3 moveAngles{};

  int32_t style{}; // also used as areaportal number

  Item *item = nullptr; // for bonus items

  // common data blocks
  MoveInfo moveInfo;
  MonsterInfo monsterInfo{};

  plat2flags_t plat2flags{};
  Vector3 offset{};
  Vector3 gravityVector{};
  gentity_t *bad_area = nullptr;
  gentity_t *hint_chain = nullptr;
  gentity_t *monster_hint_chain = nullptr;
  gentity_t *target_hint_chain = nullptr;
  int32_t hint_chain_id{};

  char clock_message[CLOCK_MESSAGE_SIZE]{};

  // Paril: we died on this frame, apply knockback even if we're dead
  GameTime dead_time{};
  // used for dabeam monsters
  gentity_t *beam = nullptr;
  gentity_t *beam2 = nullptr;
  // proboscus for Parasite
  gentity_t *proboscus = nullptr;
  // for vooping things
  gentity_t *disintegrator = nullptr;
  GameTime disintegrator_time{};
  int32_t hackFlags{}; // n64

  // fog stuff
  struct {
    Vector3 color{};
    float density{};
    float sky_factor{};

    Vector3 color_off{};
    float density_off{};
    float sky_factor_off{};
  } fog;

  struct {
    float falloff{};
    float density{};
    Vector3 start_color{};
    float start_dist{};
    Vector3 end_color{};
    float end_dist{};

    float falloff_off{};
    float density_off{};
    Vector3 start_color_off{};
    float start_dist_off{};
    Vector3 end_color_off{};
    float end_dist_off{};
  } heightfog;

  // instanced coop items
  std::bitset<MAX_CLIENTS> itemPickedUpBy{};
  GameTime slime_debounce_time{};

  // [Paril-KEX]
  bmodel_anim_t bmodel_anim{};

  MeansOfDeath lastMOD{};
  const char *style_on = nullptr;
  const char *style_off = nullptr;
  uint32_t crosslevel_flags{};
  // NOTE: if adding new elements, make sure to add them
  // in g_save.cpp too!

  // muff
  const char *gametype = nullptr;
  const char *not_gametype = nullptr;
  const char *notteam = nullptr;
  const char *notfree = nullptr;
  const char *notq2 = nullptr;
  const char *notq3a = nullptr;
  const char *notarena = nullptr;
  const char *ruleset = nullptr;
  const char *not_ruleset = nullptr;
  const char *powerups_on = nullptr;
  const char *powerups_off = nullptr;
  const char *bfg_on = nullptr;
  const char *bfg_off = nullptr;
  const char *plasmabeam_on = nullptr;
  const char *plasmabeam_off = nullptr;

  const char *spawnpad = nullptr;

  gvec3_t origin2{};

  bool skip;
  //-muff

  // q3 bobbing
  float height = 0.0f;
  float phase = 0.0f;

  // lazarus bobbingwater
  float bob;
  float duration;
  int bobFrame;

  // team for spawn spot
  Team fteam;

  // for q1 backpacks
  int pack_ammo_count[static_cast<int>(AmmoID::_Total)];
  Item *pack_weapon = nullptr;

  int arena; // for RA2 support

  Vector3 rotate = vec3_origin;    // oblivion
  Vector3 durations = vec3_origin; // oblivion
  Vector3 mangle = vec3_origin;    // oblivion

  saved_spawn_t *saved = {};
};

constexpr SpawnFlags SF_SPHERE_DEFENDER = 0x0001_spawnflag;
constexpr SpawnFlags SF_SPHERE_HUNTER = 0x0002_spawnflag;
constexpr SpawnFlags SF_SPHERE_VENGEANCE = 0x0004_spawnflag;
constexpr SpawnFlags SF_DOPPELGANGER = 0x10000_spawnflag;

constexpr SpawnFlags SF_SPHERE_TYPE =
    SF_SPHERE_DEFENDER | SF_SPHERE_HUNTER | SF_SPHERE_VENGEANCE;
constexpr SpawnFlags SF_SPHERE_FLAGS = SF_DOPPELGANGER;

struct dm_game_rt {
  void (*GameInit)();
  void (*PostInitSetup)();
  void (*ClientBegin)(gentity_t *ent);
  bool (*SelectSpawnPoint)(gentity_t *ent, Vector3 &origin, Vector3 &angles,
                           bool force_spawn);
  void (*PlayerDeath)(gentity_t *targ, gentity_t *inflictor,
                      gentity_t *attacker);
  void (*Score)(gentity_t *attacker, gentity_t *victim, int scoreChange,
                const MeansOfDeath &mod);
  void (*PlayerEffects)(gentity_t *ent);
  void (*DogTag)(gentity_t *ent, gentity_t *killer, const char **pic);
  void (*PlayerDisconnect)(gentity_t *ent);
  int (*ChangeDamage)(gentity_t *targ, gentity_t *attacker, int damage,
                      MeansOfDeath mod);
  int (*ChangeKnockback)(gentity_t *targ, gentity_t *attacker, int knockback,
                         MeansOfDeath mod);
  int (*CheckDMExitRules)();
};

// [Paril-KEX]
inline void monster_footstep(gentity_t *self) {
  if (self->groundEntity)
    self->s.event = EV_OTHER_FOOTSTEP;
}

// [Kex] helpers
// TFilter must be a type that is invokable with the
// signature bool(gentity_t *); it must return true if
// the entity given is valid for the given filter
template <typename TFilter> struct entity_iterator_t {
  using iterator_category = std::random_access_iterator_tag;
  using value_type = gentity_t *;
  using reference = gentity_t *;
  using pointer = gentity_t *;
  using difference_type = ptrdiff_t;

private:
  uint32_t index;
  uint32_t end_index; // where the end index is located for this iterator
  // index < globals.numEntities are valid
  TFilter filter;

  // this doubles as the "end" iterator
  inline bool is_out_of_range(uint32_t i) const { return i >= end_index; }

  inline bool is_out_of_range() const { return is_out_of_range(index); }

  inline void throw_if_out_of_range() const {
    if (is_out_of_range())
      throw std::out_of_range("index");
  }

  inline difference_type clamped_index() const {
    if (is_out_of_range())
      return end_index;

    return index;
  }

public:
  // note: index is not affected by filter. it is up to
  // the caller to ensure this index is filtered.
  constexpr entity_iterator_t(uint32_t i, uint32_t end_index = -1)
      : index(i),
        end_index((end_index >= globals.numEntities) ? globals.numEntities
                                                     : end_index) {}

  inline reference operator*() {
    throw_if_out_of_range();
    return &g_entities[index];
  }
  inline pointer operator->() {
    throw_if_out_of_range();
    return &g_entities[index];
  }

  inline entity_iterator_t &operator++() {
    throw_if_out_of_range();
    return *this = *this + 1;
  }

  inline entity_iterator_t &operator--() {
    throw_if_out_of_range();
    return *this = *this - 1;
  }

  inline difference_type operator-(const entity_iterator_t &it) const {
    return clamped_index() - it.clamped_index();
  }

  inline entity_iterator_t operator+(const difference_type &offset) const {
    entity_iterator_t it(index + offset, end_index);

    // move in the specified direction, only stopping if we
    // run out of range or find a filtered entity
    while (!is_out_of_range(it.index) && !filter(*it))
      it.index += offset > 0 ? 1 : -1;

    return it;
  }

  // + -1 and - 1 are the same (and - -1 & + 1)
  inline entity_iterator_t operator-(const difference_type &offset) const {
    return *this + (-offset);
  }

  // comparison. hopefully this won't break anything, but == and != use the
  // clamped index (so -1 and numEntities will be equal technically since they
  // are the same "invalid" entity) but <= and >= will affect them properly.
  inline bool operator==(const entity_iterator_t &it) const {
    return clamped_index() == it.clamped_index();
  }
  inline bool operator!=(const entity_iterator_t &it) const {
    return clamped_index() != it.clamped_index();
  }
  inline bool operator<(const entity_iterator_t &it) const {
    return index < it.index;
  }
  inline bool operator>(const entity_iterator_t &it) const {
    return index > it.index;
  }
  inline bool operator<=(const entity_iterator_t &it) const {
    return index <= it.index;
  }
  inline bool operator>=(const entity_iterator_t &it) const {
    return index >= it.index;
  }

  inline gentity_t *operator[](const difference_type &offset) const {
    return *(*this + offset);
  }
};

// iterate over range of entities, with the specified filter.
// can be "open-ended" (automatically expand with numEntities)
// by leaving the max unset.
template <typename TFilter> struct entity_iterable_t {
private:
  uint32_t begin_index, end_index;
  TFilter filter;

  // find the first entity that matches the filter, from the specified index,
  // in the specified direction
  inline uint32_t find_matched_index(uint32_t index, int32_t direction) {
    while (index < globals.numEntities && !filter(&g_entities[index]))
      index += direction;

    return index;
  }

public:
  // iterate all allocated entities that match the filter,
  // including ones allocated after this iterator is constructed
  inline entity_iterable_t<TFilter>()
      : begin_index(find_matched_index(0, 1)), end_index(game.maxEntities) {}
  // iterate all allocated entities that match the filter from the specified
  // begin offset including ones allocated after this iterator is constructed
  inline entity_iterable_t<TFilter>(uint32_t start)
      : begin_index(find_matched_index(start, 1)), end_index(game.maxEntities) {
  }
  // iterate all allocated entities that match the filter from the specified
  // begin offset to the specified INCLUSIVE end offset (or the first entity
  // that matches before it), including end itself but not ones that may appear
  // after this iterator is done
  inline entity_iterable_t<TFilter>(uint32_t start, uint32_t end)
      : begin_index(find_matched_index(start, 1)),
        end_index(find_matched_index(end, -1) + 1) {}

  inline entity_iterator_t<TFilter> begin() const {
    return entity_iterator_t<TFilter>(begin_index, end_index);
  }
  inline entity_iterator_t<TFilter> end() const { return end_index; }
};

// inUse clients that are connected; may not be spawned yet, however
struct active_clients_filter_t {
  inline bool operator()(gentity_t *ent) const {
    return (ent->inUse && ent->client && ent->client->pers.connected);
  }
};

inline entity_iterable_t<active_clients_filter_t> active_clients() {
  return entity_iterable_t<active_clients_filter_t>{1u, game.maxClients};
}

// inUse players that are connected; may not be spawned yet, however
struct active_players_filter_t {
  inline bool operator()(gentity_t *ent) const {
    return (ent->inUse && ent->client && ent->client->pers.connected &&
            ClientIsPlaying(ent->client));
  }
};

inline entity_iterable_t<active_players_filter_t> active_players() {
  return entity_iterable_t<active_players_filter_t>{1u, game.maxClients};
}

struct gib_def_t {
  size_t count;
  const char *gibname;
  float scale;
  gib_type_t type;

  constexpr gib_def_t(size_t count, const char *gibname)
      : count(count), gibname(gibname), scale(1.0f), type(GIB_NONE) {}

  constexpr gib_def_t(size_t count, const char *gibname, gib_type_t type)
      : count(count), gibname(gibname), scale(1.0f), type(type) {}

  constexpr gib_def_t(size_t count, const char *gibname, float scale)
      : count(count), gibname(gibname), scale(scale), type(GIB_NONE) {}

  constexpr gib_def_t(size_t count, const char *gibname, float scale,
                      gib_type_t type)
      : count(count), gibname(gibname), scale(scale), type(type) {}

  constexpr gib_def_t(const char *gibname, float scale, gib_type_t type)
      : count(1), gibname(gibname), scale(scale), type(type) {}

  constexpr gib_def_t(const char *gibname, float scale)
      : count(1), gibname(gibname), scale(scale), type(GIB_NONE) {}

  constexpr gib_def_t(const char *gibname, gib_type_t type)
      : count(1), gibname(gibname), scale(1.0f), type(type) {}

  constexpr gib_def_t(const char *gibname)
      : count(1), gibname(gibname), scale(1.0f), type(GIB_NONE) {}
};

// convenience function to throw different gib types
// NOTE: always throw the head gib *last* since self's size is used
// to position the gibs!
inline void ThrowGibs(gentity_t *self, int32_t damage,
                      std::initializer_list<gib_def_t> gibs) {
  for (auto &gib : gibs)
    for (size_t i = 0; i < gib.count; i++)
      ThrowGib(self, gib.gibname, damage, gib.type,
               gib.scale * (self->s.scale ? self->s.scale : 1));
}

inline bool M_CheckGib(gentity_t *self, const MeansOfDeath &mod) {
  if (self->deadFlag && mod.id == ModID::Crushed)
    return true;

  return self->health <= self->gibHealth;
}

// Fmt support for entities
template <> struct fmt::formatter<gentity_t> {
  template <typename ParseContext> constexpr auto parse(ParseContext &ctx) {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const gentity_t &p, FormatContext &ctx) const
      -> decltype(ctx.out()) {
    if (p.linked)
      return fmt::format_to(ctx.out(), FMT_STRING("{} @ {}"), p.className,
                            (p.absMax + p.absMin) * 0.5f);
    return fmt::format_to(ctx.out(), FMT_STRING("{} @ {}"), p.className,
                          p.s.origin);
  }
};

// POI tags used by this mod
enum pois_t : uint16_t {
  POI_OBJECTIVE = MAX_ENTITIES, // current objective
  POI_RED_FLAG,                 // red flag/carrier
  POI_BLUE_FLAG,                // blue flag/carrier
  POI_PING,
  POI_PING_END = POI_PING + MAX_CLIENTS - 1,
};

// implementation of pierce stuff
inline bool pierce_args_t::mark(gentity_t *ent) {
  // ran out of pierces
  if (num_pierced == MAX_PIERCE)
    return false;

  pierced[num_pierced] = ent;
  pierce_solidities[num_pierced] = ent->solid;
  num_pierced++;

  ent->solid = SOLID_NOT;
  gi.linkEntity(ent);

  return true;
}

// implementation of pierce stuff
inline void pierce_args_t::restore() {
  for (size_t i = 0; i < num_pierced; i++) {
    auto &ent = pierced[i];
    ent->solid = pierce_solidities[i];
    gi.linkEntity(ent);
  }

  num_pierced = 0;
}

// [Paril-KEX] these are to fix a legacy bug with cached indices
// in save games. these can *only* be static/globals!
template <auto T> struct cached_assetindex {
  static cached_assetindex<T> *head;

  const char *name = "";
  int32_t index = 0;
  cached_assetindex *next = nullptr;

  inline cached_assetindex() {
    next = head;
    cached_assetindex<T>::head = this;
  }
  constexpr operator int32_t() const { return index; }

  // assigned from spawn functions
  inline void assign(const char *name) {
    this->name = name;
    index = (gi.*T)(name);
  }
  // cleared before SpawnEntities
  constexpr void clear() { index = 0; }
  // re-find the index for the given cached entry, if we were cached
  // by the regular map load
  inline void reset() {
    if (index)
      index = (gi.*T)(this->name);
  }

  static void reset_all() {
    auto asset = head;

    while (asset) {
      asset->reset();
      asset = asset->next;
    }
  }

  static void clear_all() {
    auto asset = head;

    while (asset) {
      asset->clear();
      asset = asset->next;
    }
  }
};

using cached_soundIndex = cached_assetindex<&local_game_import_t::soundIndex>;
using cached_modelIndex = cached_assetindex<&local_game_import_t::modelIndex>;
using cached_imageIndex = cached_assetindex<&local_game_import_t::imageIndex>;

template <> cached_soundIndex *cached_soundIndex::head;
template <> cached_modelIndex *cached_modelIndex::head;
template <> cached_imageIndex *cached_imageIndex::head;

extern cached_modelIndex sm_meat_index;
extern cached_soundIndex snd_fry;

// ===========================================================
// UI MENU HELPERS
// ===========================================================

inline bool IsUiMenuOpen(const gclient_t *cl) {
  if (!cl)
    return false;
  return cl->initialMenu.dmWelcomeActive || cl->initialMenu.dmJoinActive ||
         cl->ui.voteActive || cl->ui.mapSelectorActive ||
         cl->ui.matchStatsActive || cl->ui.setupActive ||
         cl->ui.list.kind != UiListKind::None;
}

inline bool IsUiMenuOpen(const gentity_t *ent) {
  return ent && ent->client && IsUiMenuOpen(ent->client);
}

inline bool IsBlockingUiMenuOpen(const gclient_t *cl) {
  if (!cl)
    return false;
  if (cl->initialMenu.dmWelcomeActive || cl->initialMenu.dmJoinActive ||
      cl->ui.setupActive)
    return true;
  return cl->ui.list.kind == UiListKind::SetupGametype ||
         cl->ui.list.kind == UiListKind::TournamentPick ||
         cl->ui.list.kind == UiListKind::TournamentBan ||
         cl->ui.list.kind == UiListKind::TournamentReplay;
}

inline bool IsBlockingUiMenuOpen(const gentity_t *ent) {
  return ent && ent->client && IsBlockingUiMenuOpen(ent->client);
}

inline void CloseActiveMenu(gentity_t *ent) {
  if (!ent || !ent->client)
    return;

  ent->client->ui.voteActive = false;
  ent->client->ui.mapSelectorActive = false;
  ent->client->ui.matchStatsActive = false;
  ent->client->ui.setupActive = false;
  ent->client->ui.list.kind = UiListKind::None;
  ent->client->ui.list.page = 0;
  ent->client->initialMenu.dmWelcomeActive = false;
  ent->client->initialMenu.dmJoinActive = false;

  gi.WriteByte(svc_stufftext);
  gi.WriteString("forcemenuoff\n");
  gi.unicast(ent, true);
}

/*
===============
Menu Entry Points
===============
*/
void OpenJoinMenu(gentity_t *ent);
void OpenAdminSettingsMenu(gentity_t *ent);
void OpenAdminCommandsMenu(gentity_t *ent);
void OpenMyMapMenu(gentity_t *ent);
void OpenMyMapFlagsMenu(gentity_t *ent);
void RefreshMyMapFlagsMenu(gentity_t *ent);
void OpenVoteMenu(gentity_t *ent);
void RefreshVoteMenu(gentity_t *ent);
void OpenCallvoteMenu(gentity_t *ent);
void OpenCallvoteMapMenu(gentity_t *ent);
void OpenCallvoteGametypeMenu(gentity_t *ent);
void OpenCallvoteArenaMenu(gentity_t *ent);
void OpenCallvoteRulesetMenu(gentity_t *ent);
void OpenCallvoteTimelimitMenu(gentity_t *ent);
void OpenCallvoteScorelimitMenu(gentity_t *ent);
void OpenCallvoteUnlaggedMenu(gentity_t *ent);
void OpenCallvoteRandomMenu(gentity_t *ent);
void OpenCallvoteMapFlagsMenu(gentity_t *ent);
void RefreshCallvoteMapFlagsMenu(gentity_t *ent);
void OpenHostInfoMenu(gentity_t *ent);
void OpenMatchInfoMenu(gentity_t *ent);
void OpenPlayerMatchStatsMenu(gentity_t *ent);
void RefreshMatchStatsMenu(gentity_t *ent);
void OpenMapSelectorMenu(gentity_t *ent);
void RefreshMapSelectorMenu(gentity_t *ent);
void OpenForfeitMenu(gentity_t *ent);
void OpenSetupWelcomeMenu(gentity_t *ent);
void OpenSetupMatchFormatMenu(gentity_t *ent);
void OpenSetupMaxPlayersMenu(gentity_t *ent);
void OpenSetupGametypeMenu(gentity_t *ent);
void OpenSetupModifierMenu(gentity_t *ent);
void OpenSetupMatchLengthMenu(gentity_t *ent);
void OpenSetupMatchTypeMenu(gentity_t *ent);
void OpenSetupBestOfMenu(gentity_t *ent);
void FinishSetupWizard(gentity_t *ent);
void OpenPlayerWelcomeMenu(gentity_t *ent);
void OpenDmWelcomeMenu(gentity_t *ent);
void OpenDmJoinMenu(gentity_t *ent);
void CloseDmJoinMenu(gentity_t *ent);
void ForceCloseDmJoinMenu(gentity_t *ent);
void OpenDmHostInfoMenu(gentity_t *ent);
void OpenDmMatchInfoMenu(gentity_t *ent);
void OpenTournamentInfoMenu(gentity_t *ent);
void OpenTournamentMapChoicesMenu(gentity_t *ent);
void OpenTournamentVetoMenu(gentity_t *ent);
void OpenTournamentReplayMenu(gentity_t *ent);
void OpenTournamentReplayConfirmMenu(gentity_t *ent, int gameNumber);

// ===========================================================

/*
===============
HM_Init
===============
*/
void HM_Init();

/*
===============
HM_ResetForNewLevel
===============
*/
void HM_ResetForNewLevel();

/*
===============
HM_Think
Call once per frame to do light maintenance.
===============
*/
void HM_Think();

/*
===============
HM_AddEvent
Adds heat centered at 'pos'. Magnitude is scaled by 'amount' and a
radial falloff. Typical callers: Damage(), player_die().
===============
*/
void HM_AddEvent(const Vector3 &pos, float amount);

/*
===============
HM_Query
Returns an aggregated heat score around 'pos' within 'radius'.
Used by spawn logic to evaluate safety.
===============
*/
float HM_Query(const Vector3 &pos, float radius);

/*
===============
HM_Debug_Draw
Optional: emit temp entities to visualize a cell (for tuning).
===============
*/
void HM_Debug_Draw();

float HM_DangerAt(const Vector3 &pos);
