/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.*/

#include "../g_local.hpp"

#include <array>
#include <string>
#include <string_view>

namespace {

constexpr std::array<std::string_view, 4> kFormatKeys = {
    {"regular", "practice", "marathon", "tournament"}};

constexpr std::array<std::string_view, 5> kModifierKeys = {
    {"standard", "instagib", "vampiric", "frenzy", "gravity_lotto"}};

constexpr std::array<std::string_view, 4> kLengthKeys = {
    {"short", "standard", "long", "endurance"}};

constexpr std::array<std::string_view, 4> kTypeKeys = {
    {"casual", "standard", "competitive", "tournament"}};

constexpr std::array<std::string_view, 5> kBestOfKeys = {
    {"bo1", "bo3", "bo5", "bo7", "bo9"}};

template <size_t N>
bool IsSelectionAllowed(std::string_view value,
                        const std::array<std::string_view, N> &allowed) {
  for (const auto &candidate : allowed) {
    if (candidate == value)
      return true;
  }
  return false;
}

template <size_t N>
std::string NormalizeSelection(std::string_view value,
                               std::string_view fallback,
                               const std::array<std::string_view, N> &allowed) {
  if (IsSelectionAllowed(value, allowed))
    return std::string(value);
  return std::string(fallback);
}

constexpr std::array<int, 4> kMatchLengthSmallMinutes = {5, 10, 15, 30};
constexpr std::array<int, 4> kMatchLengthLargeMinutes = {10, 20, 30, 40};

constexpr std::array<int, 4> kMatchTypeScoreFree = {30, 40, 40, 50};
constexpr std::array<int, 4> kMatchTypeMercyFree = {20, 30, 0, 0};
constexpr std::array<int, 4> kMatchTypeScoreTeamFrag = {50, 100, 0, 0};
constexpr std::array<int, 4> kMatchTypeMercyTeamFrag = {30, 50, 50, 0};
constexpr std::array<int, 4> kMatchTypeScoreTeamCapture = {5, 8, 8, 8};
constexpr std::array<int, 4> kMatchTypeRoundTeam = {5, 8, 8, 8};
constexpr std::array<int, 4> kMatchTypeMercyOneVOne = {10, 20, 20, 0};
constexpr std::array<int, 4> kMatchTypeWeaponTeam = {15, 25, 25, 25};
constexpr std::array<int, 4> kMatchTypeWeaponFree = {5, 8, 8, 8};

int MatchLengthIndex(std::string_view length) {
  if (length == "short")
    return 0;
  if (length == "long")
    return 2;
  if (length == "endurance")
    return 3;
  return 1;
}

int MatchTypeIndex(std::string_view type) {
  if (type == "casual")
    return 0;
  if (type == "competitive")
    return 2;
  if (type == "tournament")
    return 3;
  return 1;
}

bool UsesRoundLimit(std::string_view gt) {
  if (auto type = Game::FromString(gt))
    return HasFlag(Game::GetInfo(*type).flags, GameFlags::Rounds);
  return false;
}

bool UsesCaptureLimit(std::string_view gt) {
  if (auto type = Game::FromString(gt))
    return *type == GameType::CaptureTheFlag || *type == GameType::ProBall;
  return false;
}

bool IsTeamBasedGametype(std::string_view gt) {
  const auto type = Game::FromString(gt);
  if (!type)
    return false;
  return HasFlag(Game::GetInfo(*type).flags, GameFlags::Teams);
}

bool IsOneVOneGametype(std::string_view gt) {
  const auto type = Game::FromString(gt);
  if (!type)
    return false;
  return HasFlag(Game::GetInfo(*type).flags, GameFlags::OneVOne);
}

int MatchLengthMinutes(std::string_view length, std::string_view gametype,
                       int maxPlayers) {
  const bool oneVOne = IsOneVOneGametype(gametype);
  const bool teamBased = IsTeamBasedGametype(gametype);
  const bool smallTeams = teamBased && maxPlayers > 0 && maxPlayers <= 4;
  const bool useSmallTable = oneVOne || !teamBased || smallTeams;
  const size_t index = static_cast<size_t>(MatchLengthIndex(length));
  return useSmallTable ? kMatchLengthSmallMinutes[index]
                       : kMatchLengthLargeMinutes[index];
}

int GetDefaultPlayerCount(std::string_view gt) {
  if (IsOneVOneGametype(gt))
    return 2;
  if (IsTeamBasedGametype(gt))
    return 8;
  return 12;
}

std::string NormalizeGametypeSelection(std::string_view value) {
  if (auto gt = Game::FromString(value))
    return std::string(Game::GetInfo(*gt).short_name);
  if (Game::GetCurrentType() != GameType::None)
    return std::string(Game::GetCurrentInfo().short_name);
  return "ffa";
}

void ApplyMatchLength(std::string_view length, std::string_view gametype,
                      int maxPlayers) {
  const int minutes = MatchLengthMinutes(length, gametype, maxPlayers);
  gi.cvarSet("timelimit", G_Fmt("{}", minutes).data());
}

void ApplyMatchType(std::string_view type, std::string_view gametype) {
  const bool readyUp = (type == "competitive" || type == "tournament");
  const bool lock = (type == "tournament");
  gi.cvarSet("warmup_do_ready_up", readyUp ? "1" : "0");
  gi.cvarSet("match_lock", lock ? "1" : "0");

  const size_t typeIndex = static_cast<size_t>(MatchTypeIndex(type));
  const bool oneVOne = IsOneVOneGametype(gametype);
  const bool teamBased = IsTeamBasedGametype(gametype);
  const bool free = !oneVOne && !teamBased;

  const int weaponRespawn =
      (oneVOne || teamBased) ? kMatchTypeWeaponTeam[typeIndex]
                             : kMatchTypeWeaponFree[typeIndex];
  gi.cvarSet("g_weapon_respawn_time", G_Fmt("{}", weaponRespawn).data());

  const bool usesRounds = UsesRoundLimit(gametype);
  const bool usesCapture = UsesCaptureLimit(gametype);

  if (oneVOne) {
    if (usesRounds)
      gi.cvarSet("roundlimit", "0");
    else
      gi.cvarSet("fraglimit", "0");
    gi.cvarSet("mercylimit",
               G_Fmt("{}", kMatchTypeMercyOneVOne[typeIndex]).data());
    return;
  }

  if (free) {
    const int scoreLimit = kMatchTypeScoreFree[typeIndex];
    if (usesRounds)
      gi.cvarSet("roundlimit", G_Fmt("{}", scoreLimit).data());
    else
      gi.cvarSet("fraglimit", G_Fmt("{}", scoreLimit).data());
    gi.cvarSet("mercylimit",
               G_Fmt("{}", kMatchTypeMercyFree[typeIndex]).data());
    return;
  }

  if (teamBased && usesRounds) {
    gi.cvarSet("roundlimit",
               G_Fmt("{}", kMatchTypeRoundTeam[typeIndex]).data());
    gi.cvarSet("mercylimit", "0");
    return;
  }

  if (teamBased && usesCapture) {
    gi.cvarSet("capturelimit",
               G_Fmt("{}", kMatchTypeScoreTeamCapture[typeIndex]).data());
    gi.cvarSet("mercylimit", "0");
    return;
  }

  if (teamBased) {
    gi.cvarSet("fraglimit",
               G_Fmt("{}", kMatchTypeScoreTeamFrag[typeIndex]).data());
    gi.cvarSet("mercylimit",
               G_Fmt("{}", kMatchTypeMercyTeamFrag[typeIndex]).data());
  }
}

void ApplyMatchFormat(std::string_view format) {
  const bool practice = (format == "practice");
  const bool marathonEnabled = (format == "marathon");

  if (g_practice)
    gi.cvarSet("g_practice", practice ? "1" : "0");
  if (marathon)
    gi.cvarSet("marathon", marathonEnabled ? "1" : "0");
}

bool ApplyModifiers(std::string_view modifier) {
  const bool wantInsta = (modifier == "instagib");
  const bool wantVampiric = (modifier == "vampiric");
  const bool wantFrenzy = (modifier == "frenzy");
  const bool wantGravity = (modifier == "gravity_lotto");

  const int prevInsta = g_instaGib ? g_instaGib->integer : 0;
  const int prevFrenzy = g_frenzy ? g_frenzy->integer : 0;
  const int prevQuad = g_quadhog ? g_quadhog->integer : 0;
  const int prevNade = g_nadeFest ? g_nadeFest->integer : 0;
  const int prevGravity = g_gravity_lotto ? g_gravity_lotto->integer : 0;

  const int nextInsta = wantInsta ? 1 : 0;
  const int nextFrenzy = wantFrenzy ? 1 : 0;
  const int nextGravity = wantGravity ? 1 : 0;

  bool latchedChanged = (prevInsta != nextInsta) || (prevFrenzy != nextFrenzy);
  latchedChanged |= (prevQuad != 0) || (prevNade != 0);

  gi.cvarSet("g_instaGib", nextInsta ? "1" : "0");
  gi.cvarSet("g_vampiric_damage", wantVampiric ? "1" : "0");
  gi.cvarSet("g_frenzy", nextFrenzy ? "1" : "0");
  gi.cvarSet("g_quadhog", "0");
  gi.cvarSet("g_nadeFest", "0");
  gi.cvarSet("g_gravity_lotto", nextGravity ? "1" : "0");

  if (nextGravity && prevGravity != nextGravity)
    ApplyGravityLotto();

  return latchedChanged;
}

} // namespace

void MatchSetup_ApplyStartConfig() {
  if (!deathmatch || !deathmatch->integer)
    return;
  if (!match_setup_active || match_setup_active->integer == 0)
    return;

  std::string format = NormalizeSelection(
      match_setup_format ? match_setup_format->string : "",
      "regular", kFormatKeys);
  std::string gametype = NormalizeGametypeSelection(
      match_setup_gametype ? match_setup_gametype->string : "");
  std::string modifier = NormalizeSelection(
      match_setup_modifier ? match_setup_modifier->string : "",
      "standard", kModifierKeys);
  std::string length = NormalizeSelection(
      match_setup_length ? match_setup_length->string : "",
      "standard", kLengthKeys);
  std::string type = NormalizeSelection(
      match_setup_type ? match_setup_type->string : "",
      "standard", kTypeKeys);
  std::string bestOf = NormalizeSelection(
      match_setup_bestof ? match_setup_bestof->string : "",
      "bo1", kBestOfKeys);

  int maxPlayers =
      match_setup_maxplayers ? match_setup_maxplayers->integer : 0;
  if (maxPlayers <= 0 && maxplayers)
    maxPlayers = maxplayers->integer;
  if (maxPlayers <= 0)
    maxPlayers = GetDefaultPlayerCount(gametype);

  if (IsOneVOneGametype(gametype))
    maxPlayers = 2;

  if (format == "tournament") {
    std::string error;
    if (Tournament_LoadConfig({}, &error))
      return;
    if (!error.empty())
      gi.Com_PrintFmt("Tournament config load failed: {}\n", error.c_str());
    format = "regular";
  }

  bool gametypeChanged = false;
  if (auto gt = Game::FromString(gametype)) {
    if (*gt != Game::GetCurrentType()) {
      ChangeGametype(*gt);
      gametypeChanged = true;
    }
  }

  ApplyMatchFormat(format);
  const bool latchedChanged = ApplyModifiers(modifier);
  ApplyMatchLength(length, gametype, maxPlayers);
  ApplyMatchType(type, gametype);

  gi.cvarSet("maxplayers", G_Fmt("{}", maxPlayers).data());

  if (match_setup_format)
    gi.cvarSet("match_setup_format", format.c_str());
  if (match_setup_gametype)
    gi.cvarSet("match_setup_gametype", gametype.c_str());
  if (match_setup_modifier)
    gi.cvarSet("match_setup_modifier", modifier.c_str());
  if (match_setup_length)
    gi.cvarSet("match_setup_length", length.c_str());
  if (match_setup_type)
    gi.cvarSet("match_setup_type", type.c_str());
  if (match_setup_bestof)
    gi.cvarSet("match_setup_bestof", bestOf.c_str());
  if (match_setup_maxplayers)
    gi.cvarSet("match_setup_maxplayers", G_Fmt("{}", maxPlayers).data());

  if (latchedChanged && !gametypeChanged && level.mapName[0]) {
    gi.AddCommandString(G_Fmt("gamemap {}\n", level.mapName.data()).data());
  }

  gi.Com_PrintFmt("Start server match setup: format={} gametype={} modifier={} "
                  "players={} length={} type={} bestof={}\n",
                  format.c_str(), gametype.c_str(), modifier.c_str(),
                  maxPlayers, length.c_str(), type.c_str(), bestOf.c_str());
}
