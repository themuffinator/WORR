/*Copyright (c) 2026 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

tournament.cpp - Tournament configuration parsing and runtime helpers.*/

#include "../g_local.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <json/json.h>
#include <optional>
#include <random>
#include <unordered_set>

using json = Json::Value;

namespace {

struct TournamentConfigData {
  std::string name;
  std::string seriesId;
  std::string homeId;
  std::string awayId;
  Team homeTeam = Team::None;
  Team awayTeam = Team::None;
  GameType gametype = GameType::None;
  std::string gametypeKey;
  std::string matchLength = "standard";
  std::string matchType = "tournament";
  std::string matchModifier = "standard";
  std::string bestOfKey = "bo3";
  int bestOf = 3;
  int maxPlayers = 0;
  std::array<std::string, static_cast<size_t>(Team::Total)> teamNames{};
  std::array<std::string, static_cast<size_t>(Team::Total)> teamCaptains{};
  std::vector<TournamentParticipant> participants;
  std::vector<std::string> mapPool;
};

struct TournamentConfigLocation {
  std::string path;
  bool loadedFromMod = false;
  bool exists = false;
};

constexpr std::array<int, 4> kMatchLengthSmallMinutes = {5, 10, 15, 30};
constexpr std::array<int, 4> kMatchLengthLargeMinutes = {10, 20, 30, 40};

constexpr std::array<std::string_view, 4> kLengthKeys = {
    {"short", "standard", "long", "endurance"}};
constexpr std::array<std::string_view, 4> kTypeKeys = {
    {"casual", "standard", "competitive", "tournament"}};
constexpr std::array<std::string_view, 5> kModifierKeys = {
    {"standard", "instagib", "vampiric", "frenzy", "gravity_lotto"}};

constexpr std::array<int, 4> kMatchTypeScoreFree = {30, 40, 40, 50};
constexpr std::array<int, 4> kMatchTypeMercyFree = {20, 30, 0, 0};
constexpr std::array<int, 4> kMatchTypeScoreTeamFrag = {50, 100, 0, 0};
constexpr std::array<int, 4> kMatchTypeMercyTeamFrag = {30, 50, 50, 0};
constexpr std::array<int, 4> kMatchTypeScoreTeamCapture = {5, 8, 8, 8};
constexpr std::array<int, 4> kMatchTypeRoundTeam = {5, 8, 8, 8};
constexpr std::array<int, 4> kMatchTypeMercyOneVOne = {10, 20, 20, 0};
constexpr std::array<int, 4> kMatchTypeWeaponTeam = {15, 25, 25, 25};
constexpr std::array<int, 4> kMatchTypeWeaponFree = {5, 8, 8, 8};

template <size_t N>
bool IsSelectionAllowed(std::string_view value,
                        const std::array<std::string_view, N> &allowed) {
  for (const auto &candidate : allowed) {
    if (candidate == value)
      return true;
  }
  return false;
}

constexpr bool IsBestOfAllowed(int bestOf) {
  return bestOf == 3 || bestOf == 5 || bestOf == 7 || bestOf == 9;
}

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

[[maybe_unused]] bool UsesRoundLimit(std::string_view gt) {
  if (auto type = Game::FromString(gt))
    return HasFlag(Game::GetInfo(*type).flags, GameFlags::Rounds);
  return false;
}

bool UsesCaptureLimit(std::string_view gt) {
  if (auto type = Game::FromString(gt))
    return *type == GameType::CaptureTheFlag || *type == GameType::ProBall;
  return false;
}

bool IsTeamBasedGametype(GameType gt) {
  return HasFlag(Game::GetInfo(gt).flags, GameFlags::Teams) &&
         gt != GameType::RedRover;
}

bool IsOneVOneGametype(GameType gt) {
  return HasFlag(Game::GetInfo(gt).flags, GameFlags::OneVOne);
}

int MatchLengthMinutes(std::string_view length, GameType gametype,
                       int maxPlayers) {
  const bool oneVOne = IsOneVOneGametype(gametype);
  const bool teamBased = IsTeamBasedGametype(gametype);
  const bool smallTeams = teamBased && maxPlayers > 0 && maxPlayers <= 4;
  const bool useSmallTable = oneVOne || !teamBased || smallTeams;
  const size_t index = static_cast<size_t>(MatchLengthIndex(length));
  return useSmallTable ? kMatchLengthSmallMinutes[index]
                       : kMatchLengthLargeMinutes[index];
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

void ApplyMatchLength(std::string_view length, GameType gametype,
                      int maxPlayers) {
  const int minutes = MatchLengthMinutes(length, gametype, maxPlayers);
  gi.cvarSet("timelimit", G_Fmt("{}", minutes).data());
}

void ApplyMatchType(std::string_view type, GameType gametype) {
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

  const bool usesRounds = HasFlag(Game::GetInfo(gametype).flags, GameFlags::Rounds);
  const bool usesCapture = UsesCaptureLimit(Game::GetInfo(gametype).short_name);

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

Team ParseTeamToken(std::string_view token, bool allowFree) {
  if (token == "red")
    return Team::Red;
  if (token == "blue")
    return Team::Blue;
  if (allowFree && (token == "free" || token == "ffa"))
    return Team::Free;
  if (token == "spectator" || token == "spec")
    return Team::Spectator;
  return Team::None;
}

Team ParseVetoSide(std::string_view token) {
  if (token == "red")
    return Team::Red;
  if (token == "blue")
    return Team::Blue;
  return Team::None;
}

bool ParseBestOfKey(const json &value, int &bestOf, std::string &bestKey,
                    std::string &error) {
  if (value.isString()) {
    const std::string raw = value.asString();
    if (raw == "bo3") {
      bestOf = 3;
      bestKey = raw;
      return true;
    }
    if (raw == "bo5") {
      bestOf = 5;
      bestKey = raw;
      return true;
    }
    if (raw == "bo7") {
      bestOf = 7;
      bestKey = raw;
      return true;
    }
    if (raw == "bo9") {
      bestOf = 9;
      bestKey = raw;
      return true;
    }
    error = "bestOf must be one of: bo3, bo5, bo7, bo9";
    return false;
  }
  if (value.isInt()) {
    const int count = value.asInt();
    if (!IsBestOfAllowed(count)) {
      error = "bestOf must be 3, 5, 7, or 9";
      return false;
    }
    bestOf = count;
    bestKey = G_Fmt("bo{}", count).data();
    return true;
  }
  error = "bestOf must be a string or integer";
  return false;
}

TournamentConfigLocation ResolveTournamentConfigPath(std::string_view fileName) {
  std::string activeGameDir;
  if (gi.cvar) {
    cvar_t *gameCvar = gi.cvar("game", "", CVAR_NOFLAGS);
    if (gameCvar && gameCvar->string && gameCvar->string[0]) {
      activeGameDir = gameCvar->string;
    }
  }

  if (!activeGameDir.empty()) {
    std::string modPath =
        G_Fmt("{}/{}", activeGameDir, std::string(fileName)).data();
    std::ifstream file(modPath, std::ifstream::binary);
    if (file.is_open())
      return {modPath, true, true};
  }

  const std::string basePath =
      G_Fmt("{}/{}", GAMEVERSION, std::string(fileName)).data();
  std::ifstream baseFile(basePath, std::ifstream::binary);
  if (baseFile.is_open())
    return {basePath, false, true};

  return {basePath, false, false};
}

bool ParseTournamentConfig(const json &root, TournamentConfigData &out,
                           std::string &error) {
  if (!root.isObject()) {
    error = "tourney config root must be an object";
    return false;
  }

  if (root.isMember("name") && root["name"].isString())
    out.name = root["name"].asString();

  if (root.isMember("seriesId") && root["seriesId"].isString())
    out.seriesId = root["seriesId"].asString();

  if (root.isMember("home") && root["home"].isString()) {
    const std::string rawHome = root["home"].asString();
    const Team homeTeam = ParseVetoSide(rawHome);
    if (homeTeam != Team::None) {
      out.homeTeam = homeTeam;
    } else {
      out.homeId = SanitizeSocialID(rawHome);
      if (out.homeId.empty()) {
        error = "home id is invalid";
        return false;
      }
      if (out.homeId != rawHome) {
        gi.Com_PrintFmt("{}: sanitized home id '{}' to '{}'\n", __FUNCTION__,
                        rawHome.c_str(), out.homeId.c_str());
      }
    }
  }

  if (root.isMember("away") && root["away"].isString()) {
    const std::string rawAway = root["away"].asString();
    const Team awayTeam = ParseVetoSide(rawAway);
    if (awayTeam != Team::None) {
      out.awayTeam = awayTeam;
    } else {
      out.awayId = SanitizeSocialID(rawAway);
      if (out.awayId.empty()) {
        error = "away id is invalid";
        return false;
      }
      if (out.awayId != rawAway) {
        gi.Com_PrintFmt("{}: sanitized away id '{}' to '{}'\n", __FUNCTION__,
                        rawAway.c_str(), out.awayId.c_str());
      }
    }
  }

  json match = root["match"];
  if (!match.isObject()) {
    error = "match object is required";
    return false;
  }

  if (!match.isMember("gametype") || !match["gametype"].isString()) {
    error = "match.gametype is required";
    return false;
  }

  out.gametypeKey = match["gametype"].asString();
  if (auto gt = Game::FromString(out.gametypeKey)) {
    out.gametype = *gt;
  } else {
    error = "match.gametype is invalid";
    return false;
  }

  if (match.isMember("length") && match["length"].isString())
    out.matchLength = match["length"].asString();
  if (match.isMember("type") && match["type"].isString())
    out.matchType = match["type"].asString();
  if (match.isMember("modifier") && match["modifier"].isString())
    out.matchModifier = match["modifier"].asString();

  if (!IsSelectionAllowed(out.matchLength, kLengthKeys)) {
    error = "match.length must be short, standard, long, or endurance";
    return false;
  }
  if (!IsSelectionAllowed(out.matchType, kTypeKeys)) {
    error = "match.type must be casual, standard, competitive, or tournament";
    return false;
  }
  if (!IsSelectionAllowed(out.matchModifier, kModifierKeys)) {
    error = "match.modifier must be standard, instagib, vampiric, frenzy, or gravity_lotto";
    return false;
  }

  if (match.isMember("bestOf")) {
    if (!ParseBestOfKey(match["bestOf"], out.bestOf, out.bestOfKey, error))
      return false;
  }

  if (!IsBestOfAllowed(out.bestOf)) {
    error = "match.bestOf must be 3, 5, 7, or 9 in tournament mode";
    return false;
  }

  if (match.isMember("maxPlayers") && match["maxPlayers"].isInt())
    out.maxPlayers = match["maxPlayers"].asInt();

  json teams = root["teams"];
  if (teams.isObject()) {
    if (teams.isMember("red") && teams["red"].isObject()) {
      const json &red = teams["red"];
      if (red.isMember("name") && red["name"].isString())
        out.teamNames[static_cast<size_t>(Team::Red)] = red["name"].asString();
      if (red.isMember("captain") && red["captain"].isString())
        out.teamCaptains[static_cast<size_t>(Team::Red)] =
            SanitizeSocialID(red["captain"].asString());
    }
    if (teams.isMember("blue") && teams["blue"].isObject()) {
      const json &blue = teams["blue"];
      if (blue.isMember("name") && blue["name"].isString())
        out.teamNames[static_cast<size_t>(Team::Blue)] =
            blue["name"].asString();
      if (blue.isMember("captain") && blue["captain"].isString())
        out.teamCaptains[static_cast<size_t>(Team::Blue)] =
            SanitizeSocialID(blue["captain"].asString());
    }
  }

  json participants = root["participants"];
  if (!participants.isArray() || participants.empty()) {
    error = "participants array is required";
    return false;
  }

  std::unordered_set<std::string> seenIds;
  for (const json &entry : participants) {
    if (!entry.isObject()) {
      error = "participants entries must be objects";
      return false;
    }
    if (!entry.isMember("id") || !entry["id"].isString()) {
      error = "participants entries must include id";
      return false;
    }

    TournamentParticipant participant{};
    const std::string rawId = entry["id"].asString();
    participant.socialId = SanitizeSocialID(rawId);
    if (participant.socialId.empty()) {
      error = "participant id is invalid";
      return false;
    }

    if (participant.socialId != rawId) {
      gi.Com_PrintFmt("{}: sanitized participant id '{}' to '{}'\n",
                      __FUNCTION__, rawId.c_str(),
                      participant.socialId.c_str());
    }

    if (seenIds.contains(participant.socialId)) {
      error = "duplicate participant id detected";
      return false;
    }
    seenIds.insert(participant.socialId);

    if (entry.isMember("name") && entry["name"].isString())
      participant.name = entry["name"].asString();

    Team team = Team::None;
    if (entry.isMember("team") && entry["team"].isString()) {
      team = ParseTeamToken(entry["team"].asString(), true);
    }
    participant.lockedTeam =
        (team == Team::None) ? Team::Free : team;

    if (entry.isMember("side") && entry["side"].isString()) {
      participant.vetoSide = ParseVetoSide(entry["side"].asString());
    }

    if (entry.isMember("captain") && entry["captain"].isBool()) {
      participant.captain = entry["captain"].asBool();
    }

    out.participants.push_back(std::move(participant));
  }

  const bool teamBased = IsTeamBasedGametype(out.gametype);
  if (teamBased) {
    for (const auto &p : out.participants) {
      if (p.lockedTeam != Team::Red && p.lockedTeam != Team::Blue) {
        error = "team-based tournaments require participants to be on red/blue";
        return false;
      }
    }

    bool hasRed = false;
    bool hasBlue = false;
    for (const auto &p : out.participants) {
      hasRed |= (p.lockedTeam == Team::Red);
      hasBlue |= (p.lockedTeam == Team::Blue);
    }

    if (!hasRed || !hasBlue) {
      error = "team-based tournaments require both red and blue participants";
      return false;
    }

    for (auto &p : out.participants) {
      if (p.vetoSide == Team::None)
        p.vetoSide = p.lockedTeam;
      if (!p.captain)
        continue;
      if (p.lockedTeam == Team::Red &&
          out.teamCaptains[static_cast<size_t>(Team::Red)].empty()) {
        out.teamCaptains[static_cast<size_t>(Team::Red)] = p.socialId;
      }
      if (p.lockedTeam == Team::Blue &&
          out.teamCaptains[static_cast<size_t>(Team::Blue)].empty()) {
        out.teamCaptains[static_cast<size_t>(Team::Blue)] = p.socialId;
      }
    }

    if (out.teamCaptains[static_cast<size_t>(Team::Red)].empty() ||
        out.teamCaptains[static_cast<size_t>(Team::Blue)].empty()) {
      error = "team-based tournaments require captains for red and blue";
      return false;
    }
  }

  auto findParticipantById = [&](const std::string &id)
      -> const TournamentParticipant * {
    if (id.empty())
      return nullptr;
    for (const auto &participant : out.participants) {
      if (participant.socialId == id)
        return &participant;
    }
    return nullptr;
  };

  if (teamBased) {
    if (!out.homeId.empty()) {
      const auto *participant = findParticipantById(out.homeId);
      if (!participant) {
        error = "home id must match a participant";
        return false;
      }
      out.homeTeam = participant->lockedTeam;
    }

    if (out.homeTeam == Team::None) {
      out.homeTeam = out.participants.front().lockedTeam;
      out.homeId = out.participants.front().socialId;
    }

    if (out.homeTeam != Team::Red && out.homeTeam != Team::Blue) {
      error = "home must be red or blue for team-based tournaments";
      return false;
    }

    if (!out.awayId.empty()) {
      const auto *participant = findParticipantById(out.awayId);
      if (!participant) {
        error = "away id must match a participant";
        return false;
      }
      out.awayTeam = participant->lockedTeam;
    }

    if (out.awayTeam == Team::None) {
      out.awayTeam = (out.homeTeam == Team::Red) ? Team::Blue : Team::Red;
    }

    if (out.awayTeam != Team::Red && out.awayTeam != Team::Blue) {
      error = "away must be red or blue for team-based tournaments";
      return false;
    }

    if (out.awayTeam == out.homeTeam) {
      error = "home and away teams must differ";
      return false;
    }
  } else {
    if (out.homeTeam != Team::None || out.awayTeam != Team::None) {
      error = "home and away must be participant ids in free-for-all tournaments";
      return false;
    }

    if (!out.homeId.empty() && !findParticipantById(out.homeId)) {
      error = "home id must match a participant";
      return false;
    }

    if (!out.awayId.empty() && !findParticipantById(out.awayId)) {
      error = "away id must match a participant";
      return false;
    }

    if (out.homeId.empty())
      out.homeId = out.participants.front().socialId;

    if (out.awayId.empty()) {
      for (const auto &participant : out.participants) {
        if (participant.socialId != out.homeId) {
          out.awayId = participant.socialId;
          break;
        }
      }
    }

    if (out.homeId.empty() || out.awayId.empty()) {
      error = "home and away participants are required";
      return false;
    }

    if (out.homeId == out.awayId) {
      error = "home and away participants must differ";
      return false;
    }
  }

  json mapPool = root.isMember("mapPool") ? root["mapPool"] : root["map_pool"];
  if (!mapPool.isArray() || mapPool.empty()) {
    error = "mapPool array is required";
    return false;
  }

  std::unordered_set<std::string> seenMaps;
  for (const json &entry : mapPool) {
    if (!entry.isString()) {
      error = "mapPool entries must be strings";
      return false;
    }

    std::string mapName = entry.asString();
    if (!G_IsValidMapIdentifier(mapName)) {
      error = "mapPool contains invalid map names";
      return false;
    }

    if (seenMaps.contains(mapName))
      continue;

    seenMaps.insert(mapName);
    out.mapPool.push_back(std::move(mapName));
  }

  if (static_cast<int>(out.mapPool.size()) < out.bestOf) {
    error = "mapPool must contain at least bestOf maps";
    return false;
  }

  return true;
}

void ApplyTournamentConfig(const TournamentConfigData &cfg) {
  const GameType current = Game::GetCurrentType();
  const bool gametypeChanged = (current != cfg.gametype);

  if (gametypeChanged)
    ChangeGametype(cfg.gametype);

  if (cfg.maxPlayers > 0)
    gi.cvarSet("maxplayers", G_Fmt("{}", cfg.maxPlayers).data());

  gi.cvarSet("g_practice", "0");
  gi.cvarSet("marathon", "0");

  if (match_setup_length)
    gi.cvarSet("match_setup_length", cfg.matchLength.c_str());
  if (match_setup_type)
    gi.cvarSet("match_setup_type", "tournament");
  if (match_setup_bestof)
    gi.cvarSet("match_setup_bestof", cfg.bestOfKey.c_str());

  const bool latchedChanged = ApplyModifiers(cfg.matchModifier);
  ApplyMatchLength(cfg.matchLength, cfg.gametype, cfg.maxPlayers);
  ApplyMatchType(cfg.matchType, cfg.gametype);

  gi.cvarSet("match_lock", "1");
  gi.cvarSet("warmup_do_ready_up", "1");

  if (!cfg.teamNames[static_cast<size_t>(Team::Red)].empty())
    gi.cvarSet("g_red_team_name",
               cfg.teamNames[static_cast<size_t>(Team::Red)].c_str());
  if (!cfg.teamNames[static_cast<size_t>(Team::Blue)].empty())
    gi.cvarSet("g_blue_team_name",
               cfg.teamNames[static_cast<size_t>(Team::Blue)].c_str());

  if (latchedChanged && !gametypeChanged && level.mapName[0]) {
    gi.AddCommandString(G_Fmt("gamemap {}\n", level.mapName.data()).data());
  }
}

bool LoadTournamentConfigData(std::string_view configName,
                              TournamentConfigData &out,
                              std::string &error) {
  const std::string_view rawName =
      (configName.empty() && g_tourney_cfg && g_tourney_cfg->string)
          ? std::string_view(g_tourney_cfg->string)
          : configName;

  std::string sanitizedName;
  std::string rejectReason;
  if (!G_SanitizeMapConfigFilename(rawName, sanitizedName, rejectReason)) {
    error = G_Fmt("invalid g_tourney_cfg '{}': {}", std::string(rawName).c_str(),
                  rejectReason)
                .data();
    return false;
  }

  TournamentConfigLocation location = ResolveTournamentConfigPath(sanitizedName);
  if (!location.exists) {
    error = G_Fmt("tourney config file not found: {}", location.path).data();
    return false;
  }

  std::ifstream file(location.path, std::ifstream::binary);
  if (!file.is_open()) {
    error = G_Fmt("unable to open config file: {}", location.path).data();
    return false;
  }

  Json::CharReaderBuilder builder;
  std::string errs;
  json root;
  if (!Json::parseFromStream(builder, file, &root, &errs)) {
    error = G_Fmt("parse error in {}: {}", location.path, errs).data();
    return false;
  }

  if (!ParseTournamentConfig(root, out, error))
    return false;

  out.seriesId =
      out.seriesId.empty()
          ? G_Fmt("{}_{}", out.gametypeKey.c_str(), FileTimeStamp()).data()
          : out.seriesId;

  return true;
}

const gclient_t *FindClientById(std::string_view id) {
  if (id.empty())
    return nullptr;

  for (auto ec : active_clients()) {
    if (!ec || !ec->client)
      continue;
    const char *social = ec->client->sess.socialID;
    if (social && social[0] && id == social)
      return ec->client;
  }

  return nullptr;
}

gentity_t *FindEntityById(std::string_view id) {
  if (id.empty())
    return nullptr;

  for (auto ec : active_clients()) {
    if (!ec || !ec->client)
      continue;
    const char *social = ec->client->sess.socialID;
    if (social && social[0] && id == social)
      return ec;
  }

  return nullptr;
}

const TournamentParticipant *FindParticipantById(std::string_view id) {
  if (id.empty())
    return nullptr;

  for (const auto &participant : game.tournament.participants) {
    if (participant.socialId == id)
      return &participant;
  }

  return nullptr;
}

bool IsMapInPool(std::string_view mapName) {
  for (const auto &map : game.tournament.mapPool) {
    if (_stricmp(map.c_str(), mapName.data()) == 0)
      return true;
  }
  return false;
}

bool IsMapSelected(std::string_view mapName) {
  for (const auto &map : game.tournament.mapBans) {
    if (_stricmp(map.c_str(), mapName.data()) == 0)
      return true;
  }
  for (const auto &map : game.tournament.mapPicks) {
    if (_stricmp(map.c_str(), mapName.data()) == 0)
      return true;
  }
  return false;
}

int TournamentPicksNeeded() {
  return std::max(0, game.tournament.bestOf - 1);
}

int TournamentPicksRemaining() {
  return TournamentPicksNeeded() -
         static_cast<int>(game.tournament.mapPicks.size());
}

int TournamentRemainingMaps() {
  return static_cast<int>(game.tournament.mapPool.size()) -
         static_cast<int>(game.tournament.mapPicks.size()) -
         static_cast<int>(game.tournament.mapBans.size());
}

bool Tournament_BansAllowed() {
  const int picksRemaining = TournamentPicksRemaining();
  if (picksRemaining <= 0)
    return false;
  return TournamentRemainingMaps() - 1 >= picksRemaining;
}

std::string VetoSideLabel(bool homeSide) {
  const char *sideName = homeSide ? "Home" : "Away";
  if (game.tournament.teamBased) {
    const Team team = homeSide ? game.tournament.homeTeam
                               : game.tournament.awayTeam;
    if (team == Team::Red || team == Team::Blue) {
      return G_Fmt("{} ({})", sideName, Teams_TeamName(team)).data();
    }
    return sideName;
  }

  const std::string &id = homeSide ? game.tournament.homeId
                                   : game.tournament.awayId;
  if (const auto *participant = FindParticipantById(id)) {
    if (!participant->name.empty()) {
      return G_Fmt("{} ({})", sideName, participant->name.c_str()).data();
    }
  }

  return sideName;
}

std::string CurrentVetoPrompt() {
  if (!game.tournament.vetoStarted || game.tournament.vetoComplete)
    return {};

  const std::string sideLabel = VetoSideLabel(game.tournament.vetoHomeTurn);
  const char *action = Tournament_BansAllowed() ? "pick or ban" : "pick";
  return G_Fmt("Veto: {} to {} next.", sideLabel.c_str(), action).data();
}

bool CanActorVeto(gentity_t *ent) {
  if (!ent || !ent->client)
    return false;

  const char *id = ent->client->sess.socialID;
  if (!id || !id[0])
    return false;

  if (game.tournament.teamBased) {
    const Team side =
        game.tournament.vetoHomeTurn ? game.tournament.homeTeam
                                     : game.tournament.awayTeam;
    if (side != Team::Red && side != Team::Blue)
      return false;

    const std::string &captainId =
        game.tournament.teamCaptains[static_cast<size_t>(side)];
    return !captainId.empty() && captainId == id;
  }

  const std::string &allowedId =
      game.tournament.vetoHomeTurn ? game.tournament.homeId
                                   : game.tournament.awayId;
  return !allowedId.empty() && allowedId == id;
}

void OpenTournamentVetoMenuForCurrent() {
  if (!Tournament_IsActive())
    return;
  if (!game.tournament.vetoStarted || game.tournament.vetoComplete)
    return;

  if (game.tournament.teamBased) {
    const Team side =
        game.tournament.vetoHomeTurn ? game.tournament.homeTeam
                                     : game.tournament.awayTeam;
    if (side != Team::Red && side != Team::Blue)
      return;

    const std::string &captainId =
        game.tournament.teamCaptains[static_cast<size_t>(side)];
    if (captainId.empty())
      return;

    if (gentity_t *actor = FindEntityById(captainId)) {
      CloseActiveMenu(actor);
      OpenTournamentVetoMenu(actor);
    }
    return;
  }

  const std::string &id =
      game.tournament.vetoHomeTurn ? game.tournament.homeId
                                   : game.tournament.awayId;
  if (id.empty())
    return;

  if (gentity_t *actor = FindEntityById(id)) {
    CloseActiveMenu(actor);
    OpenTournamentVetoMenu(actor);
  }
}

void FinalizeVetoOrder() {
  game.tournament.mapOrder.clear();
  game.tournament.mapOrder.reserve(static_cast<size_t>(game.tournament.bestOf));

  for (const auto &pick : game.tournament.mapPicks) {
    if (game.tournament.mapOrder.size() >=
        static_cast<size_t>(game.tournament.bestOf))
      break;
    game.tournament.mapOrder.push_back(pick);
  }

  if (game.tournament.mapOrder.size() <
      static_cast<size_t>(game.tournament.bestOf)) {
    std::vector<std::string> remaining;
    remaining.reserve(game.tournament.mapPool.size());
    for (const auto &map : game.tournament.mapPool) {
      if (IsMapSelected(map))
        continue;
      remaining.push_back(map);
    }

    const std::vector<std::string> &selection =
        remaining.empty() ? game.tournament.mapPool : remaining;
    if (!selection.empty()) {
      std::uniform_int_distribution<size_t> pick(
          0, selection.size() - 1);
      game.tournament.mapOrder.push_back(selection[pick(game.mapRNG)]);
    }
  }
}

} // namespace

bool Tournament_ConfigIsValid(std::string_view configName) {
  TournamentConfigData parsed{};
  std::string error;
  if (!LoadTournamentConfigData(configName, parsed, error)) {
    if (!error.empty())
      gi.Com_PrintFmt("{}: {}\n", __FUNCTION__, error.c_str());
    return false;
  }
  return true;
}

bool Tournament_LoadConfig(std::string_view configName, std::string *error) {
  TournamentConfigData parsed{};
  std::string localError;
  if (!LoadTournamentConfigData(configName, parsed, localError)) {
    if (error)
      *error = localError;
    if (!localError.empty())
      gi.Com_PrintFmt("{}: {}\n", __FUNCTION__, localError.c_str());
    return false;
  }

  game.tournament = {};
  game.tournament.configLoaded = true;
  game.tournament.configValid = true;
  game.tournament.active = true;
  game.tournament.seriesComplete = false;
  game.tournament.bestOf = parsed.bestOf;
  game.tournament.winTarget = (parsed.bestOf / 2) + 1;
  game.tournament.teamBased = IsTeamBasedGametype(parsed.gametype);
  game.tournament.gametype = parsed.gametype;
  game.tournament.gamesPlayed = 0;
  game.tournament.configFile =
      (configName.empty() && g_tourney_cfg && g_tourney_cfg->string)
          ? g_tourney_cfg->string
          : std::string(configName);
  game.tournament.name = parsed.name;
  game.tournament.seriesId = parsed.seriesId;
  game.tournament.homeId = parsed.homeId;
  game.tournament.awayId = parsed.awayId;
  game.tournament.homeTeam = parsed.homeTeam;
  game.tournament.awayTeam = parsed.awayTeam;
  game.tournament.matchLength = parsed.matchLength;
  game.tournament.matchType = parsed.matchType;
  game.tournament.matchModifier = parsed.matchModifier;
  game.tournament.matchBestOfKey = parsed.bestOfKey;
  game.tournament.maxPlayers = parsed.maxPlayers;
  game.tournament.teamNames = parsed.teamNames;
  game.tournament.teamCaptains = parsed.teamCaptains;
  game.tournament.participants = parsed.participants;
  game.tournament.mapPool = parsed.mapPool;
  game.tournament.vetoStarted = false;
  game.tournament.vetoComplete = false;
  game.tournament.vetoIndex = 0;
  game.tournament.vetoHomeTurn = true;

  ApplyTournamentConfig(parsed);

  gi.Com_PrintFmt(
      "Tournament config loaded: {} (series {}, bestOf {})\n",
      game.tournament.configFile.c_str(), game.tournament.seriesId.c_str(),
      game.tournament.bestOf);

  return true;
}

bool Tournament_IsActive() {
  return game.tournament.configLoaded && game.tournament.configValid &&
         match_setup_type && match_setup_type->string &&
         Q_strcasecmp(match_setup_type->string, "tournament") == 0;
}

bool Tournament_AllParticipantsConnected() {
  if (!Tournament_IsActive())
    return false;

  for (const auto &participant : game.tournament.participants) {
    if (!FindClientById(participant.socialId))
      return false;
  }

  return true;
}

bool Tournament_AllParticipantsReady() {
  if (!Tournament_IsActive())
    return false;

  for (const auto &participant : game.tournament.participants) {
    const gclient_t *cl = FindClientById(participant.socialId);
    if (!cl || !cl->pers.readyStatus)
      return false;
  }

  return true;
}

bool Tournament_IsParticipant(const gclient_t *cl) {
  if (!cl || !cl->sess.socialID[0])
    return false;
  return FindParticipantById(cl->sess.socialID) != nullptr;
}

Team Tournament_AssignedTeam(const gclient_t *cl) {
  if (!cl || !cl->sess.socialID[0])
    return Team::Spectator;
  if (const auto *participant = FindParticipantById(cl->sess.socialID))
    return participant->lockedTeam;
  return Team::Spectator;
}

bool Tournament_StartVetoIfReady() {
  if (!Tournament_IsActive())
    return false;
  if (game.tournament.vetoStarted || game.tournament.vetoComplete)
    return false;
  if (!Tournament_AllParticipantsReady())
    return false;

  game.tournament.vetoStarted = true;
  game.tournament.vetoIndex = 0;
  game.tournament.vetoHomeTurn = true;
  gi.LocBroadcast_Print(
      PRINT_CENTER,
      ".Tournament veto is ready. Home side picks or bans first.");
  OpenTournamentVetoMenuForCurrent();
  return true;
}

bool Tournament_VetoComplete() {
  return Tournament_IsActive() && game.tournament.vetoComplete;
}

bool Tournament_HandleVetoAction(gentity_t *ent, TournamentVetoAction action,
                                 std::string_view mapName,
                                 std::string &message) {
  if (!Tournament_IsActive()) {
    message = "Tournament mode is not active.";
    return false;
  }

  if (level.matchState >= MatchState::Countdown) {
    message = "Tournament veto is only available before match start.";
    return false;
  }

  if (!Tournament_StartVetoIfReady()) {
    if (!game.tournament.vetoStarted) {
      message =
          "All participants must be connected and ready before veto starts.";
      return false;
    }
  }

  if (game.tournament.vetoComplete) {
    message = "Veto is already complete.";
    return false;
  }

  if (!CanActorVeto(ent)) {
    message = "Only the active side may perform this veto.";
    return false;
  }

  if (mapName.empty()) {
    message = "You must specify a map name.";
    return false;
  }

  if (!IsMapInPool(mapName)) {
    message = "That map is not in the tournament pool.";
    return false;
  }

  if (IsMapSelected(mapName)) {
    message = "That map has already been picked or banned.";
    return false;
  }

  const int picksNeeded = TournamentPicksNeeded();
  if (action == TournamentVetoAction::Pick) {
    if (static_cast<int>(game.tournament.mapPicks.size()) >= picksNeeded) {
      message = "All required picks are already locked.";
      return false;
    }
  } else {
    if (!Tournament_BansAllowed()) {
      message = "Bans are no longer available.";
      return false;
    }
  }

  if (action == TournamentVetoAction::Ban) {
    game.tournament.mapBans.emplace_back(mapName);
    const std::string sideLabel =
        VetoSideLabel(game.tournament.vetoHomeTurn);
    gi.LocBroadcast_Print(PRINT_HIGH, "{} banned {}.", sideLabel.c_str(),
                          mapName.data());
  } else {
    game.tournament.mapPicks.emplace_back(mapName);
    const std::string sideLabel =
        VetoSideLabel(game.tournament.vetoHomeTurn);
    gi.LocBroadcast_Print(PRINT_HIGH, "{} picked {}.", sideLabel.c_str(),
                          mapName.data());
  }

  if (ent)
    CloseActiveMenu(ent);

  game.tournament.vetoIndex++;
  game.tournament.vetoHomeTurn = !game.tournament.vetoHomeTurn;

  if (static_cast<int>(game.tournament.mapPicks.size()) >= picksNeeded) {
    game.tournament.vetoComplete = true;
    FinalizeVetoOrder();
    message = "Tournament veto complete.";
    gi.LocBroadcast_Print(PRINT_CENTER, ".Tournament veto complete.");

    if (!game.tournament.mapOrder.empty()) {
      const std::string &firstMap = game.tournament.mapOrder.front();
      if (!firstMap.empty() &&
          _stricmp(firstMap.c_str(), level.mapName.data()) != 0) {
        gi.AddCommandString(G_Fmt("gamemap {}\n", firstMap).data());
      }
    }
    return true;
  }

  message = CurrentVetoPrompt();
  if (!message.empty())
    gi.LocBroadcast_Print(PRINT_HIGH, "{}", message.c_str());
  OpenTournamentVetoMenuForCurrent();
  return true;
}

std::string Tournament_GetVetoStatus() {
  if (!Tournament_IsActive())
    return "Tournament mode is not active.";

  std::string status;
  status += "Tournament veto status:\n";
  status += G_Fmt("  Pool: {} map(s)\n", game.tournament.mapPool.size()).data();
  status += G_Fmt("  Picks: {}/{}\n", game.tournament.mapPicks.size(),
                  TournamentPicksNeeded())
                .data();
  status += G_Fmt("  Bans: {} map(s)\n", game.tournament.mapBans.size()).data();
  status +=
      G_Fmt("  Remaining: {} map(s)\n", TournamentRemainingMaps()).data();

  const std::string prompt = CurrentVetoPrompt();
  if (!prompt.empty()) {
    status += "  ";
    status += prompt;
    status.push_back('\n');
  }

  return status;
}

bool Tournament_GetNextMap(std::string &mapName) {
  if (!Tournament_IsActive())
    return false;

  if (game.tournament.mapOrder.empty())
    return false;

  const size_t index = static_cast<size_t>(game.tournament.gamesPlayed);
  if (index >= game.tournament.mapOrder.size())
    return false;

  mapName = game.tournament.mapOrder[index];
  return !mapName.empty();
}
