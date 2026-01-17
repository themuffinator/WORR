/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

p_hud_scoreboard.cpp (Player HUD Scoreboard) This file contains the server-side
logic for generating the layout strings for the multiplayer scoreboards. It
sorts players, gathers their scores and other relevant data, and constructs a
formatted string that the client-side game module can parse to render the
scoreboard. Key Responsibilities: - `MultiplayerScoreboard`: The main entry
point for generating the scoreboard layout. It dispatches to different functions
based on the current gametype. - Player Sorting: Implements the logic to sort
players based on score for Free-for-All modes and by team and score for
team-based modes. - Layout String Construction: Builds the complex string of HUD
commands that defines the position, content, and appearance of each element on
the scoreboard (player names, scores, pings, icons, etc.). - Gametype-Specific
Scoreboards: Contains specialized functions for rendering scoreboards for
different modes like FFA, Duel, and Team Deathmatch.*/

#include <algorithm>
#include <utility>
#include <vector>

#include "../g_local.hpp"

/*
===============
SortClientsByTeamAndScore
===============
*/
[[maybe_unused]] static void SortClientsByTeamAndScore(uint8_t sorted[2][MAX_CLIENTS],
                                      int8_t sortedScores[2][MAX_CLIENTS],
                                      uint8_t total[2], uint8_t totalLiving[2],
                                      int totalScore[2]) {
  memset(total, 0, sizeof(uint8_t) * 2);
  memset(totalLiving, 0, sizeof(uint8_t) * 2);
  memset(totalScore, 0, sizeof(int) * 2);

  for (uint32_t i = 0; i < game.maxClients; ++i) {
    gentity_t *cl_ent = g_entities + 1 + i;
    if (!cl_ent->inUse)
      continue;

    int team = -1;
    if (game.clients[i].sess.team == Team::Red)
      team = 0;
    else if (game.clients[i].sess.team == Team::Blue)
      team = 1;

    if (team < 0)
      continue;

    int score = ClientScoreForStandings(&game.clients[i]);
    int j;
    for (j = 0; j < total[team]; ++j) {
      if (score > sortedScores[team][j])
        break;
    }
    for (int k = total[team]; k > j; --k) {
      sorted[team][k] = sorted[team][k - 1];
      sortedScores[team][k] = sortedScores[team][k - 1];
    }
    sorted[team][j] = i;
    sortedScores[team][j] = score;
    totalScore[team] += score;
    total[team]++;

    if (!game.clients[i].eliminated)
      totalLiving[team]++;
  }
}

// ===========================================================================
// SCOREBOARD MESSAGE HANDLING
// ===========================================================================

enum class SpectatorListMode { QueuedOnly, PassiveOnly, Both };

enum class PlayerEntryMode { FFA, Duel, Team };

/*
===============
AppendFormat

Attempts to append a formatted string to the layout. Returns false when
the append would exceed MAX_STRING_CHARS, leaving the layout untouched.
===============
*/
template <typename... Args>
static bool AppendFormat(std::string &layout,
                         fmt::format_string<Args...> fmtStr, Args &&...args) {
  std::string buffer = fmt::format(fmtStr, std::forward<Args>(args)...);
  if (layout.size() + buffer.size() > MAX_STRING_CHARS)
    return false;

  layout += buffer;
  return true;
}

/*
===============
AddScoreboardHeaderAndFooter

Displays standard header and footer for all scoreboard types.
Includes map name, gametype, score limit, match time, victor string, and
optional footer tip. Sections are omitted when insufficient room remains in the
layout buffer.
===============
*/
static void AddScoreboardHeaderAndFooter(std::string &layout, gentity_t *viewer,
                                         bool includeFooter = true) {
  const char *limitLabel = "Score Limit";
  if (Game::Has(GameFlags::Rounds) || Game::Has(GameFlags::Elimination)) {
    limitLabel = "Round Limit";
  } else if (Game::Has(GameFlags::CTF)) {
    limitLabel = "Capture Limit";
  }

  // Header: map and gametype
  if (!AppendFormat(layout,
                    "xv 0 yv -40 cstring2 \"{} on '{}'\" "
                    "xv 0 yv -30 cstring2 \"{}: {}\" ",
                    level.gametype_name.data(), level.longName.data(),
                    limitLabel, GT_ScoreLimit()))
    return;

  if (hostname->string[0] && *hostname->string)
    AppendFormat(layout, "xv 0 yv -50 cstring2 \"{}\" ", hostname->string);

  // During intermission
  if (level.intermission.time) {
    // Match duration
    if (level.levelStartTime &&
        (level.time - level.levelStartTime).seconds() > 0) {
      int duration = (level.intermission.time - level.levelStartTime - 1_sec)
                         .milliseconds();
      AppendFormat(layout, "xv 0 yv -50 cstring2 \"Total Match Time: {}\" ",
                   TimeString(duration, true, false));
    }

    // Victor message
    std::string_view msg(level.intermission.victorMessage.data());
    if (!msg.empty()) {
      AppendFormat(layout, "xv 0 yv -10 cstring2 \"{}\" ", msg);
    }

    // Press button prompt (5s gate)
    int frameGate = level.intermission.serverFrame + (5_sec).frames();
    AppendFormat(layout,
                 "ifgef {} yb -58 xv 0 cstring2 \"darkmatter-quake.com\" yb "
                 "-48 xv 0 loc_cstring2 0 \"$m_eou_press_button\" endif ",
                 frameGate);
  }
  // During live match
  else if (level.matchState == MatchState::In_Progress && viewer->client &&
           ClientIsPlaying(viewer->client)) {
    const int viewerScore = ClientScoreForStandings(viewer->client);
    if (viewerScore > 0 && level.pop.num_playing_clients > 1) {
      AppendFormat(layout,
                   "xv 0 yv -10 cstring2 \"{} place with a score of {}\" ",
                   PlaceString(viewer->client->pers.currentRank + 1),
                   viewerScore);
    }
    if (includeFooter) {
      AppendFormat(layout, "xv 0 yb -48 cstring2 \"{}\" ",
                   "Show inventory to toggle menu.");
    }
  }
}

/*
===============
AddSpectatorList

Draws queued players, passive spectators, or both.
Used by all scoreboard modes.
===============
*/
static void AddSpectatorList(std::string &layout, int startY,
                             SpectatorListMode mode) {
  uint32_t y = startY;
  bool wroteQueued = false;
  bool wroteSpecs = false;

  if (mode == SpectatorListMode::QueuedOnly || mode == SpectatorListMode::Both) {
    uint8_t lineIndex = 0;

    for (int clientIndex : level.sortedClients) {
      if (layout.size() >= MAX_STRING_CHARS - 50)
        return;
      if (clientIndex < 0 || clientIndex >= static_cast<int>(game.maxClients))
        continue;

      gentity_t *cl_ent = &g_entities[clientIndex + 1];
      gclient_t *cl = &game.clients[clientIndex];

      if (!cl_ent->inUse || !cl->pers.connected || cl_ent->solid != SOLID_NOT)
        continue;
      if (ClientIsPlaying(cl) || !cl->sess.matchQueued)
        continue;

      if (!wroteQueued) {
        if (!AppendFormat(layout,
                          "xv 0 yv {} loc_string2 0 \"Queued Contenders:\" "
                          "xv -40 yv {} loc_string2 0 \"w  l  name\" ",
                          y, y + 8))
          return; // Queued section omitted when the layout buffer is full
        y += 16;
        wroteQueued = true;
      }

      std::string_view entry =
          G_Fmt("ctf {} {} {} {} {} \"\" ", (lineIndex++ & 1) ? 200 : -40, y,
                clientIndex, cl->sess.matchWins, cl->sess.matchLosses)
              .data();

      if (layout.size() + entry.size() >= MAX_STRING_CHARS)
        return;

      layout += entry;
      if ((lineIndex & 1) == 0)
        y += 8;
    }

    if (lineIndex & 1)
      y += 8;
    if (wroteQueued)
      y += 8;
  }

  if (mode == SpectatorListMode::PassiveOnly || mode == SpectatorListMode::Both) {
    uint8_t lineIndex = 0;

    for (int clientIndex : level.sortedClients) {
      if (layout.size() >= MAX_STRING_CHARS - 50)
        return;
      if (clientIndex < 0 || clientIndex >= static_cast<int>(game.maxClients))
        continue;

      gentity_t *cl_ent = &g_entities[clientIndex + 1];
      gclient_t *cl = &game.clients[clientIndex];

      if (!cl_ent->inUse || !cl->pers.connected || cl_ent->solid != SOLID_NOT)
        continue;
      if (ClientIsPlaying(cl) || cl->sess.matchQueued)
        continue;

      if (!wroteSpecs) {
        if (!AppendFormat(layout, "xv 0 yv {} loc_string2 0 \"Spectators:\" ",
                          y))
          return; // Spectator section omitted when the layout buffer is full
        y += 8;
        wroteSpecs = true;
      }

      std::string_view entry =
          G_Fmt("ctf {} {} {} 0 0 \"\" ", (lineIndex++ & 1) ? 200 : -40, y,
                clientIndex)
              .data();

      if (layout.size() + entry.size() >= MAX_STRING_CHARS)
        return;

      layout += entry;
      if ((lineIndex & 1) == 0)
        y += 8;
    }
  }
}

static void AddPlayerEntry(std::string &layout, gentity_t *cl_ent, int x, int y,
                           PlayerEntryMode mode, gentity_t *viewer,
                           gentity_t *killer, bool isReady,
                           const char *flagIcon);

static size_t CollectActiveDuelistsByJoinTime(
    std::array<int, 2> &duelists) {
  std::vector<int> playing;
  playing.reserve(2);

  for (uint32_t i = 0; i < game.maxClients; ++i) {
    gentity_t *cl_ent = &g_entities[i + 1];
    gclient_t *cl = &game.clients[i];

    if (!cl_ent->inUse || !cl->pers.connected)
      continue;
    if (!ClientIsPlaying(cl))
      continue;

    playing.push_back(static_cast<int>(i));
  }

  if (playing.empty())
    return 0;

  std::sort(playing.begin(), playing.end(), [](int a, int b) {
    const gclient_t *ca = &game.clients[a];
    const gclient_t *cb = &game.clients[b];

    if (ca->sess.teamJoinTime != cb->sess.teamJoinTime)
      return ca->sess.teamJoinTime < cb->sess.teamJoinTime;

    return a < b;
  });

  const size_t count = std::min(playing.size(), duelists.size());
  for (size_t i = 0; i < count; ++i)
    duelists[i] = playing[i];

  return count;
}

static int AddDuelistSummary(std::string &layout, int startY,
                             gentity_t *viewer, gentity_t *killer) {
  if (!Game::Has(GameFlags::OneVOne))
    return startY;

  std::array<int, 2> duelists{};
  const size_t found = CollectActiveDuelistsByJoinTime(duelists);

  if (!found)
    return startY;

  const int y = startY;
  for (size_t i = 0; i < found; ++i) {
    const int clientIndex = duelists[i];
    if (clientIndex < 0 || clientIndex >= static_cast<int>(game.maxClients))
      continue;

    gentity_t *cl_ent = &g_entities[clientIndex + 1];
    gclient_t *cl = &game.clients[clientIndex];

    if (!cl_ent->inUse || !cl->pers.connected)
      continue;

    const int x = (i == 0) ? -72 : 130;
    AddPlayerEntry(layout, cl_ent, x, y, PlayerEntryMode::Duel, viewer, killer,
                   cl->pers.readyStatus, nullptr);
  }

  return startY + 42;
}

/*
===============
AddPlayerEntry

Draws a single player entry row in the scoreboard.
Can be used by all scoreboard types.
===============
*/
static void AddPlayerEntry(std::string &layout, gentity_t *cl_ent, int x, int y,
                           PlayerEntryMode mode, gentity_t *viewer,
                           gentity_t *killer, bool isReady,
                           const char *flagIcon) {
  if (!cl_ent || !cl_ent->inUse || !cl_ent->client)
    return;

  gclient_t *cl = cl_ent->client;
  int clientNum = cl_ent->s.number - 1;

  std::string entry;

  // === Tag icon ===
  if (mode == PlayerEntryMode::FFA || mode == PlayerEntryMode::Duel) {
    if (cl_ent == viewer || Game::Is(GameType::RedRover)) {
      const char *tag = (cl->sess.team == Team::Red)    ? "/tags/ctf_red"
                        : (cl->sess.team == Team::Blue) ? "/tags/ctf_blue"
                                                        : "/tags/default";
      fmt::format_to(std::back_inserter(entry), "xv {} yv {} picn {} ", x, y,
                     tag);
    } else if (cl_ent == killer) {
      fmt::format_to(std::back_inserter(entry),
                     "xv {} yv {} picn /tags/bloody ", x, y);
    }
  } else if (mode == PlayerEntryMode::Team && flagIcon) {
    fmt::format_to(std::back_inserter(entry), "xv {} yv {} picn {} ", x, y,
                   flagIcon);
  }

  // === Skin icon ===
  if (cl->sess.skinIconIndex > 0) {
    std::string skinPath = G_Fmt("/players/{}_i", cl->sess.skinName).data();
    fmt::format_to(std::back_inserter(entry), "xv {} yv {} picn {} ", x, y,
                   skinPath);
  }

  // === Ready or eliminated marker ===
  if (isReady) {
    fmt::format_to(std::back_inserter(entry),
                   "xv {} yv {} picn wheel/p_compass_selected ", x + 16,
                   y + 16);
  } else if (Game::Has(GameFlags::Rounds) && mode == PlayerEntryMode::Team &&
             !cl->eliminated && level.matchState == MatchState::In_Progress) {
    const char *teamIcon = (cl->sess.team == Team::Red) ? "sbfctf1" : "sbfctf2";
    fmt::format_to(std::back_inserter(entry), "xv {} yv {} picn {} ", x + 16, y,
                   teamIcon);
  }

  // === Append entry if it fits ===
  if (layout.size() + entry.size() >= MAX_STRING_CHARS)
    return;
  layout += entry;
  entry.clear();

  // === Score/ping line ===
  fmt::format_to(std::back_inserter(entry), "client {} {} {} {} {} {} ", x, y,
                 clientNum, ClientScoreForStandings(cl), std::min(cl->ping, 999),
                 0);

  if (layout.size() + entry.size() >= MAX_STRING_CHARS)
    return;
  layout += entry;

  if (Game::Is(GameType::FreezeTag)) {
    std::string extra;

    if (cl->eliminated) {
      const bool thawing = cl->resp.thawer && cl->freeze.holdDeadline &&
                           cl->freeze.holdDeadline > level.time;
      const char *status = thawing ? "THAWING" : "FROZEN";

      fmt::format_to(std::back_inserter(extra), "xv {} yv {} string \"{}\" ",
                     x + 96, y, status);
    }

    if (cl->resp.thawed > 0) {
      fmt::format_to(std::back_inserter(extra), "xv {} yv {} string \"TH:{}\" ",
                     x + 96, y + 8, cl->resp.thawed);
    }

    if (!extra.empty()) {
      if (layout.size() + extra.size() >= MAX_STRING_CHARS)
        return;
      layout += extra;
    }
  }
}

/*
===============
AddTeamScoreOverlay
===============
*/
[[maybe_unused]] static void AddTeamScoreOverlay(std::string &layout, const uint8_t total[2],
                                const uint8_t totalLiving[2], int teamsize) {
  const bool domination = Game::Is(GameType::Domination);
  const bool proBall = Game::Is(GameType::ProBall);
  const char *scoreLabel = domination ? "PT/points" : (proBall ? "GO" : "SC");

  if (Game::Is(GameType::CaptureTheFlag)) {
    if (!AppendFormat(layout,
                      "if 25 xv -32 yv 8 pic 25 endif "
                      "xv 0 yv 28 string \"{}/{}\" "
                      "xv 58 yv 12 rnum 2 19 "
                      "xv -40 yv 42 string \"{}\" "
                      "xv -12 yv 42 picn ping "
                      "if 26 xv 208 yv 8 pic 26 endif "
                      "xv 240 yv 28 string \"{}/{}\" "
                      "xv 296 yv 12 rnum 2 21 "
                      "xv 200 yv 42 string \"{}\" "
                      "xv 228 yv 42 picn ping ",
                      total[0], teamsize, scoreLabel, total[1], teamsize,
                      scoreLabel))
      return; // Team overlay omitted when the layout buffer is full
  } else if (Game::Has(GameFlags::Rounds)) {
    if (!AppendFormat(layout,
                      "if 25 xv -32 yv 8 pic 25 endif "
                      "xv 0 yv 28 string \"{}/{}/{}\" "
                      "xv 58 yv 12 rnum 2 19 "
                      "xv -40 yv 42 string \"{}\" "
                      "xv -12 yv 42 picn ping "
                      "if 26 xv 208 yv 8 pic 26 endif "
                      "xv 240 yv 28 string \"{}/{}/{}\" "
                      "xv 296 yv 12 rnum 2 21 "
                      "xv 200 yv 42 string \"{}\" "
                      "xv 228 yv 42 picn ping ",
                      totalLiving[0], total[0], teamsize, scoreLabel,
                      totalLiving[1], total[1], teamsize, scoreLabel))
      return; // Team overlay omitted when the layout buffer is full
  } else {
    if (!AppendFormat(layout,
                      "if 25 xv -32 yv 8 pic 25 endif "
                      "xv -123 yv 28 cstring \"{}/{}\" "
                      "xv 41 yv 12 rnum 3 19 "
                      "xv -40 yv 42 string \"{}\" "
                      "xv -12 yv 42 picn ping "
                      "if 26 xv 208 yv 8 pic 26 endif "
                      "xv 117 yv 28 cstring \"{}/{}\" "
                      "xv 280 yv 12 rnum 3 21 "
                      "xv 200 yv 42 string \"{}\" "
                      "xv 228 yv 42 picn ping ",
                      total[0], teamsize, scoreLabel, total[1], teamsize,
                      scoreLabel))
      return; // Team overlay omitted when the layout buffer is full
  }
}

/*
===============
AddTeamPlayerEntries

Returns the last shown index for the team.
===============
*/
[[maybe_unused]] static uint8_t AddTeamPlayerEntries(std::string &layout, int teamIndex,
                                    const uint8_t *sorted, uint8_t total,
                                    gentity_t * /* killer */) {
  uint8_t lastShown = 0;

  for (uint8_t i = 0; i < total; ++i) {
    uint32_t clientNum = sorted[i];
    if (clientNum < 0 || clientNum >= game.maxClients)
      continue;

    gentity_t *cl_ent = &g_entities[clientNum + 1];
    gclient_t *cl = &game.clients[clientNum];

    int y = 52 + i * 8;
    int x = (teamIndex == 0) ? -40 : 200;
    bool isReady = (level.matchState == MatchState::Warmup_ReadyUp &&
                    (cl->pers.readyStatus || cl->sess.is_a_bot));

    const char *flagIcon = nullptr;
    if (teamIndex == 0 && cl->pers.inventory[IT_FLAG_BLUE])
      flagIcon = "sbfctf2";
    if (teamIndex == 1 && cl->pers.inventory[IT_FLAG_RED])
      flagIcon = "sbfctf1";

    size_t preSize = layout.size();
    AddPlayerEntry(layout, cl_ent, x, y, PlayerEntryMode::Team, nullptr,
                   nullptr, isReady, flagIcon);

    if (layout.size() != preSize)
      lastShown = i;
  }

  return lastShown;
}

/*
===============
AddSpectatorEntries
===============
*/
[[maybe_unused]] static void AddSpectatorEntries(std::string &layout, uint8_t lastRed,
                                uint8_t lastBlue) {
  uint32_t j = ((std::max(lastRed, lastBlue) + 3) * 8) + 42;
  uint32_t lineIndex = 0;
  bool wroteQueued = false, wroteSpecs = false;

  for (uint32_t i = 0;
       i < game.maxClients && layout.size() < MAX_STRING_CHARS - 50; ++i) {
    gentity_t *cl_ent = &g_entities[i + 1];
    gclient_t *cl = &game.clients[i];

    if (!cl_ent->inUse || cl_ent->solid != SOLID_NOT || ClientIsPlaying(cl))
      continue;

    if (cl->sess.matchQueued) {
      if (!wroteQueued) {
        if (!AppendFormat(layout,
                          "xv 0 yv {} loc_string2 0 \"Queued Contenders:\" "
                          "xv -40 yv {} loc_string2 0 \"w  l  name\" ",
                          j, j + 8))
          return; // Queued spectators omitted when the layout buffer is full
        j += 16;
        wroteQueued = true;
      }

      std::string_view entry =
          G_Fmt("ctf {} {} {} {} {} \"\" ", (lineIndex++ & 1) ? 200 : -40, j, i,
                cl->sess.matchWins, cl->sess.matchLosses)
              .data();

      if (layout.size() + entry.size() < MAX_STRING_CHARS) {
        layout += entry;
        if ((lineIndex & 1) == 0)
          j += 8;
      }
    }
  }

  // Reset state for non-queued spectators
  lineIndex = 0;
  if (wroteQueued)
    j += 8;

  for (uint32_t i = 0;
       i < game.maxClients && layout.size() < MAX_STRING_CHARS - 50; ++i) {
    gentity_t *cl_ent = &g_entities[i + 1];
    gclient_t *cl = &game.clients[i];

    if (!cl_ent->inUse || cl_ent->solid != SOLID_NOT || ClientIsPlaying(cl) ||
        cl->sess.matchQueued)
      continue;

    if (!wroteSpecs) {
      if (!AppendFormat(layout, "xv 0 yv {} loc_string2 0 \"Spectators:\" ", j))
        return; // Passive spectator section omitted when the layout buffer is
                // full
      j += 8;
      wroteSpecs = true;
    }

    std::string_view entry =
        G_Fmt("ctf {} {} {} 0 0 \"\" ", (lineIndex++ & 1) ? 200 : -40, j, i)
            .data();

    if (layout.size() + entry.size() < MAX_STRING_CHARS) {
      layout += entry;
      if ((lineIndex & 1) == 0)
        j += 8;
    }
  }
}

/*
===============
AddTeamSummaryLine
===============
*/
[[maybe_unused]] static void AddTeamSummaryLine(std::string &layout, const uint8_t total[2],
                               const uint8_t lastShown[2]) {
  if (total[0] > lastShown[0] + 1) {
    int y = 42 + (lastShown[0] + 1) * 8;
    if (!AppendFormat(layout, "xv -32 yv {} loc_string 1 $g_ctf_and_more {} ",
                      y, total[0] - lastShown[0] - 1))
      return; // Remaining red team members omitted when the layout buffer is
              // full
  }
  if (total[1] > lastShown[1] + 1) {
    int y = 42 + (lastShown[1] + 1) * 8;
    if (!AppendFormat(layout, "xv 208 yv {} loc_string 1 $g_ctf_and_more {} ",
                      y, total[1] - lastShown[1] - 1))
      return; // Remaining blue team members omitted when the layout buffer is
              // full
  }
}

/*
===============
TeamsScoreboardMessage
===============
*/
void TeamsScoreboardMessage(gentity_t *ent, gentity_t *killer) {
  uint32_t i, j, k, n;
  uint8_t sorted[2][MAX_CLIENTS] = {};
  int8_t sortedScores[2][MAX_CLIENTS] = {};
  int score;
  uint8_t total[2] = {};
  uint8_t totalLiving[2] = {};
  int totalScore[2] = {};
  uint8_t last[2] = {};
  gclient_t *cl;
  gentity_t *cl_ent;
  int team;
  const int teamsize = floor(maxplayers->integer / 2);

  total[0] = total[1] = 0;
  totalLiving[0] = totalLiving[1] = 0;
  last[0] = last[1] = 0;
  totalScore[0] = totalScore[1] = 0;

  for (i = 0; i < game.maxClients; ++i) {
    cl_ent = g_entities + 1 + i;
    if (!cl_ent->inUse)
      continue;

    if (game.clients[i].sess.team == Team::Red)
      team = 0;
    else if (game.clients[i].sess.team == Team::Blue)
      team = 1;
    else
      continue;

    score = ClientScoreForStandings(&game.clients[i]);
    for (j = 0; j < total[team]; ++j)
      if (score > sortedScores[team][j])
        break;
    for (k = total[team]; k > j; --k) {
      sorted[team][k] = sorted[team][k - 1];
      sortedScores[team][k] = sortedScores[team][k - 1];
    }
    sorted[team][j] = i;
    sortedScores[team][j] = score;
    totalScore[team] += score;
    total[team]++;
    if (!game.clients[i].eliminated)
      totalLiving[team]++;
  }

  std::string layout;
  layout.clear();

  fmt::format_to(std::back_inserter(layout),
                 FMT_STRING("xv 0 yv -40 cstring2 \"{} on '{}'\" "),
                 level.gametype_name.data(), level.longName.data());
  fmt::format_to(std::back_inserter(layout),
                 FMT_STRING("xv 0 yv -30 cstring2 \"Score Limit: {}\" "),
                 GT_ScoreLimit());

  if (level.intermission.time) {
    if (level.levelStartTime &&
        (level.time - level.levelStartTime).seconds() > 0) {
      int duration = (level.intermission.time - level.levelStartTime - 1_sec)
                         .milliseconds();
      fmt::format_to(
          std::back_inserter(layout),
          FMT_STRING("xv 0 yv -50 cstring2 \"Total Match Time: {}\" "),
          TimeString(duration, false, false));
    }
    if (level.intermission.victorMessage[0]) {
      fmt::format_to(std::back_inserter(layout),
                     FMT_STRING("xv 0 yv -10 cstring2 \"{}\" "),
                     level.intermission.victorMessage.data());
    }

    fmt::format_to(
        std::back_inserter(layout),
        FMT_STRING("ifgef {} yb -58 xv 0 cstring2 \"darkmatter-quake.com\" yb "
                   "-48 xv 0 loc_cstring2 0 \"$m_eou_press_button\" endif "),
        (level.intermission.serverFrame + (5_sec).frames()));
  } else if (level.matchState == MatchState::In_Progress) {
    const int entScore = ClientScoreForStandings(ent->client);
    if (ent->client && ClientIsPlaying(ent->client) && entScore > 0 &&
        level.pop.num_playing_clients > 1) {
      fmt::format_to(
          std::back_inserter(layout),
          FMT_STRING("xv 0 yv -10 cstring2 \"{} place with a score of {}\" "),
          PlaceString(ent->client->pers.currentRank + 1), entScore);
    }
    fmt::format_to(std::back_inserter(layout),
                   FMT_STRING("xv 0 yb -48 cstring2 \"{}\" "),
                   "Use inventory bind to toggle menu.");
  }

  if (Game::Has(GameFlags::CTF)) {
    fmt::format_to(std::back_inserter(layout),
                   FMT_STRING("if 25 xv -32 yv 8 pic 25 endif "
                              "xv 0 yv 28 string \"{}/{}\" "
                              "xv 58 yv 12 num 3 19 "
                              "xv -40 yv 42 string \"SC\" "
                              "xv -12 yv 42 picn ping "
                              "if 26 xv 208 yv 8 pic 26 endif "
                              "xv 240 yv 28 string \"{}/{}\" "
                              "xv 296 yv 12 num 3 21 "
                              "xv 200 yv 42 string \"SC\" "
                              "xv 228 yv 42 picn ping "),
                   total[0], teamsize, total[1], teamsize);
  } else if (Game::Has(GameFlags::Rounds)) {
    fmt::format_to(std::back_inserter(layout),
                   FMT_STRING("if 25 xv -32 yv 8 pic 25 endif "
                              "xv 0 yv 28 string \"{}/{}/{}\" "
                              "xv 58 yv 12 num 3 19 "
                              "xv -40 yv 42 string \"SC\" "
                              "xv -12 yv 42 picn ping "
                              "if 26 xv 208 yv 8 pic 26 endif "
                              "xv 240 yv 28 string \"{}/{}/{}\" "
                              "xv 296 yv 12 num 3 21 "
                              "xv 200 yv 42 string \"SC\" "
                              "xv 228 yv 42 picn ping "),
                   totalLiving[0], total[0], teamsize, totalLiving[1], total[1],
                   teamsize);
  } else {
    fmt::format_to(std::back_inserter(layout),
                   FMT_STRING("if 25 xv -32 yv 8 pic 25 endif "
                              "xv -123 yv 28 cstring \"{}/{}\" "
                              "xv 41 yv 12 num 3 19 "
                              "xv -40 yv 42 string \"SC\" "
                              "xv -12 yv 42 picn ping "
                              "if 26 xv 208 yv 8 pic 26 endif "
                              "xv 117 yv 28 cstring \"{}/{}\" "
                              "xv 280 yv 12 num 3 21 "
                              "xv 200 yv 42 string \"SC\" "
                              "xv 228 yv 42 picn ping "),
                   total[0], teamsize, total[1], teamsize);
  }

  for (i = 0; i < 16; ++i) {
    if (i >= total[0] && i >= total[1])
      break;

    if (i < total[0]) {
      cl = &game.clients[sorted[0][i]];
      cl_ent = g_entities + 1 + sorted[0][i];

      int ty = 52 + i * 8;
      const int playerScore = ClientScoreForStandings(cl);
      std::string_view entry = G_Fmt(
          "ctf -40 {} {} {} {} {} ", ty, sorted[0][i], playerScore,
          cl->ping > 999 ? 999 : cl->ping,
          cl_ent->client->pers.inventory[IT_FLAG_BLUE] ? "sbfctf2" : "\"\"");

      if (level.matchState == MatchState::Warmup_ReadyUp &&
          (cl->pers.readyStatus || cl->sess.is_a_bot)) {
        fmt::format_to(std::back_inserter(layout),
                       FMT_STRING("xv -56 yv {} picn {} "), ty - 2,
                       "wheel/p_compass_selected");
      } else if (Game::Has(GameFlags::Rounds) &&
                 level.matchState == MatchState::In_Progress &&
                 !cl->eliminated) {
        fmt::format_to(std::back_inserter(layout),
                       FMT_STRING("xv -50 yv {} picn {} "), ty, "sbfctf1");
      }

      if (layout.size() + entry.size() < MAX_STRING_CHARS) {
        layout += entry;
        last[0] = static_cast<uint8_t>(i);
      }
    }

    if (i < total[1]) {
      cl = &game.clients[sorted[1][i]];
      cl_ent = g_entities + 1 + sorted[1][i];

      int ty = 52 + i * 8;
      const int playerScore = ClientScoreForStandings(cl);
      std::string_view entry = G_Fmt(
          "ctf 200 {} {} {} {} {} ", ty, sorted[1][i], playerScore,
          cl->ping > 999 ? 999 : cl->ping,
          cl_ent->client->pers.inventory[IT_FLAG_RED] ? "sbfctf1" : "\"\"");

      if (level.matchState == MatchState::Warmup_ReadyUp &&
          (cl->pers.readyStatus || cl->sess.is_a_bot)) {
        fmt::format_to(std::back_inserter(layout),
                       FMT_STRING("xv 182 yv {} picn {} "), ty - 2,
                       "wheel/p_compass_selected");
      } else if (Game::Has(GameFlags::Rounds) &&
                 level.matchState == MatchState::In_Progress &&
                 !cl->eliminated) {
        fmt::format_to(std::back_inserter(layout),
                       FMT_STRING("xv 190 yv {} picn {} "), ty, "sbfctf2");
      }

      if (layout.size() + entry.size() < MAX_STRING_CHARS) {
        layout += entry;
        last[1] = static_cast<uint8_t>(i);
      }
    }
  }

  if (last[0] > last[1])
    j = last[0];
  else
    j = last[1];
  j = (j + 3) * 8 + 42;

  k = n = 0;
  if (layout.size() < MAX_STRING_CHARS - 50) {
    for (i = 0; i < game.maxClients; ++i) {
      cl_ent = g_entities + 1 + i;
      cl = &game.clients[i];
      if (!cl_ent->inUse || cl_ent->solid != SOLID_NOT ||
          ClientIsPlaying(cl_ent->client))
        continue;

      if (!k) {
        k = 1;
        fmt::format_to(
            std::back_inserter(layout),
            FMT_STRING("xv 0 yv {} loc_string2 0 \"$g_pc_spectators\" "), j);
        j += 8;
      }

      std::string_view entry =
          G_Fmt("ctf {} {} {} {} {} \"\" ", (n & 1) ? 200 : -40, j, i,
                ClientScoreForStandings(cl), cl->ping > 999 ? 999 : cl->ping);

      if (layout.size() + entry.size() < MAX_STRING_CHARS)
        layout += entry;

      if (n & 1)
        j += 8;
      n++;
    }
  }

  if (total[0] - last[0] > 1) {
    fmt::format_to(std::back_inserter(layout),
                   FMT_STRING("xv -32 yv {} loc_string 1 $g_ctf_and_more {} "),
                   42 + (last[0] + 1) * 8, total[0] - last[0] - 1);
  }
  if (total[1] - last[1] > 1) {
    fmt::format_to(std::back_inserter(layout),
                   FMT_STRING("xv 208 yv {} loc_string 1 $g_ctf_and_more {} "),
                   42 + (last[1] + 1) * 8, total[1] - last[1] - 1);
  }

  gi.WriteByte(svc_layout);
  gi.WriteString(layout.c_str());
}

/*
===============
DuelScoreboardMessage
===============
*/
static void DuelScoreboardMessage(gentity_t *ent, gentity_t *killer) {
  std::string layout;
  AddScoreboardHeaderAndFooter(layout, ent);
  int spectatorStart = AddDuelistSummary(layout, 0, ent, killer);
  if (spectatorStart == 0)
    spectatorStart = 42;
  AddSpectatorList(layout, spectatorStart, SpectatorListMode::Both);

  gi.WriteByte(svc_layout);
  gi.WriteString(layout.c_str());
}

/*
===============
DeathmatchScoreboardMessage
===============
*/
void DeathmatchScoreboardMessage(gentity_t *ent, gentity_t *killer) {
  if (Teams() && Game::IsNot(GameType::RedRover)) {
    TeamsScoreboardMessage(ent, ent->enemy);
    return;
  }
  if (Game::Has(GameFlags::OneVOne)) {
    DuelScoreboardMessage(ent, ent->enemy);
    return;
  }

  uint8_t total = std::min<uint8_t>(level.pop.num_playing_clients, 16);
  std::string layout;

  for (size_t i = 0; i < total; ++i) {
    uint32_t clientNum = level.sortedClients[i];
    if (clientNum < 0 || clientNum >= game.maxClients)
      continue;

    gclient_t *cl = &game.clients[clientNum];
    gentity_t *cl_ent = &g_entities[clientNum + 1];

    if (!ClientIsPlaying(cl))
      continue;

    int x = (i >= 8) ? 130 : -72;
    int y = 32 * (i % 8);
    AddPlayerEntry(layout, cl_ent, x, y, PlayerEntryMode::FFA, ent, killer,
                   cl->pers.readyStatus, nullptr);
  }
  AddScoreboardHeaderAndFooter(layout, ent);

  gi.WriteByte(svc_layout);
  gi.WriteString(layout.c_str());
}

/*
===============
MultiplayerScoreboard

Displays the scoreboard instead of the help screen.
Note that it isn't that hard to overflow the 1400 byte message limit!
===============
*/
void MultiplayerScoreboard(gentity_t *ent) {
  gentity_t *target =
      ent->client->follow.target ? ent->client->follow.target : ent;

  DeathmatchScoreboardMessage(target, target->enemy);

  gi.unicast(ent, true);
  ent->client->menu.updateTime = level.time + 3_sec;
}
