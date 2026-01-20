/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

menu_page_welcome.cpp (Menu Page - Welcome/Join) This file implements the main
menu that players see when they are spectators or have just joined the server.
It is the primary navigation hub for joining the game, spectating, or accessing
other informational menus. Key Responsibilities:
- Main Menu Hub: `OpenJoinMenu` is the function called to display the main menu.
- Dynamic Join Options: The `onUpdate` function (`AddJoinOptions`) dynamically
creates the "Join" options based on the current gametype (e.g., "Join Red",
"Join Blue" for TDM; "Join Match" or "Join Queue" for FFA/Duel). - Player
Counts: Displays the current number of players in the match or on each team.
- Navigation: Provides the entry points to all other major menus, such as "Host
Info", "Match Info", and "Call a Vote".*/

#include "../g_local.hpp"
#include "menu_ui_helpers.hpp"

extern bool SetTeam(gentity_t *ent, Team desired_team, bool inactive,
                    bool force, bool silent);
void GetFollowTarget(gentity_t *ent);
void FreeFollower(gentity_t *ent);
bool Vote_Menu_Active(gentity_t *ent);

extern void OpenHostInfoMenu(gentity_t *ent);
extern void OpenMatchInfoMenu(gentity_t *ent);
extern void OpenPlayerMatchStatsMenu(gentity_t *ent);
extern void OpenAdminSettingsMenu(gentity_t *ent);
extern void OpenVoteMenu(gentity_t *ent);
extern void OpenMyMapMenu(gentity_t *ent);
extern void OpenForfeitMenu(gentity_t *ent);
void OpenJoinMenu(gentity_t *ent);

namespace {

static void OpenJoinMenuInternal(gentity_t *ent, const std::string &title,
                                 const std::string &subtitle,
                                 const char *menuName) {
  if (!ent || !ent->client)
    return;

  gclient_t *cl = ent->client;

  int maxPlayers = maxplayers->integer;
  if (maxPlayers < 1)
    maxPlayers = 1;

  uint8_t redCount = 0, blueCount = 0, freeCount = 0, queueCount = 0;
  const bool duelQueueAllowed =
      Game::Has(GameFlags::OneVOne) && g_allow_duel_queue &&
      g_allow_duel_queue->integer && !Tournament_IsActive();

  for (auto ec : active_clients()) {
    if (duelQueueAllowed && ec->client->sess.team == Team::Spectator &&
        ec->client->sess.matchQueued) {
      queueCount++;
    } else {
      switch (ec->client->sess.team) {
      case Team::Free:
        freeCount++;
        break;
      case Team::Red:
        redCount++;
        break;
      case Team::Blue:
        blueCount++;
        break;
      case Team::None:
      case Team::Spectator:
      case Team::Total:
        break;
      }
    }
  }

  const bool teamplay = Teams();
  std::string joinRed =
      fmt::format("Join Red ({}/{})", redCount, maxPlayers / 2);
  std::string joinBlue =
      fmt::format("Join Blue ({}/{})", blueCount, maxPlayers / 2);

  std::string joinFree;
  if (duelQueueAllowed && level.pop.num_playing_clients == 2) {
    joinFree = fmt::format("Join Queue ({}/{})", queueCount, maxPlayers - 2);
  } else {
    const int targetMax = Game::Has(GameFlags::OneVOne) ? 2 : maxPlayers;
    joinFree = fmt::format("Join Match ({}/{})", freeCount, targetMax);
  }

  const bool isPlaying = ClientIsPlaying(cl);
  const bool isTournament = Tournament_IsActive();
  const bool showJoin = (cl->initialMenu.frozen || !isPlaying) && !isTournament;
  const bool showSpectate =
      (cl->initialMenu.frozen || isPlaying) && !isTournament;

  const bool showTourneyInfo = isTournament;
  const bool showTourneyMaps =
      isTournament && Tournament_VetoComplete() &&
      !game.tournament.mapOrder.empty();
  const bool showCallvote =
      !isTournament && g_allowVoting->integer &&
      (isPlaying || (!isPlaying && g_allowSpecVote->integer));
  const bool showMymap =
      !isTournament && g_maps_mymap && g_maps_mymap->integer &&
      (!g_allowMymap || g_allowMymap->integer);
  const bool showForfeit =
      !isTournament && g_allowVoting->integer && isPlaying &&
      (level.matchState == MatchState::In_Progress ||
       level.matchState == MatchState::Countdown);
  const bool showMatchStats = g_matchstats->integer != 0;
  const bool showAdmin = cl->sess.admin;

  MenuUi::UiCommandBuilder cmd(ent);
  cmd.AppendCvar("ui_dm_title", title);
  cmd.AppendCvar("ui_dm_subtitle", subtitle);
  cmd.AppendCvar("ui_dm_teamplay", teamplay ? "1" : "0");
  cmd.AppendCvar("ui_dm_show_join", showJoin ? "1" : "0");
  cmd.AppendCvar("ui_dm_show_spectate", showSpectate ? "1" : "0");
  cmd.AppendCvar("ui_dm_join_red", joinRed);
  cmd.AppendCvar("ui_dm_join_blue", joinBlue);
  cmd.AppendCvar("ui_dm_join_free", joinFree);
  cmd.AppendCvar("ui_dm_show_tourney_info", showTourneyInfo ? "1" : "0");
  cmd.AppendCvar("ui_dm_show_tourney_maps", showTourneyMaps ? "1" : "0");
  cmd.AppendCvar("ui_dm_show_callvote", showCallvote ? "1" : "0");
  cmd.AppendCvar("ui_dm_show_mymap", showMymap ? "1" : "0");
  cmd.AppendCvar("ui_dm_show_forfeit", showForfeit ? "1" : "0");
  cmd.AppendCvar("ui_dm_show_matchstats", showMatchStats ? "1" : "0");
  cmd.AppendCvar("ui_dm_show_admin", showAdmin ? "1" : "0");
  cmd.AppendCommand(std::string("pushmenu ") + menuName);
  cmd.Flush();

  cl->initialMenu.dmJoinActive = true;
  cl->initialMenu.dmWelcomeActive = false;
}

} // namespace

static void ReleaseWelcomeFreeze(gentity_t *ent) {
  if (!ent || !ent->client)
    return;

  if (!ent->client->initialMenu.frozen)
    return;

  ent->client->initialMenu.frozen = false;
  ent->client->initialMenu.shown = true;
  ent->client->initialMenu.delay = 0_sec;
  ent->client->initialMenu.hostSetupDone = true;
  ent->client->initialMenu.dmWelcomeActive = false;
  ent->client->initialMenu.dmJoinActive = false;
}

static void TryJoinTeam(gentity_t *ent, Team team) {
  if (SetTeam(ent, team, false, false, false))
    ReleaseWelcomeFreeze(ent);
}

static void SelectSpectate(gentity_t *ent) {
  if (!ent || !ent->client)
    return;

  const bool wasFrozen = ent->client->initialMenu.frozen;
  const bool wasSpectator = !ClientIsPlaying(ent->client);
  const bool changed = SetTeam(ent, Team::Spectator, false, false, false);

  if (changed || wasSpectator) {
    ReleaseWelcomeFreeze(ent);
    if (!changed)
      CloseActiveMenu(ent);
    else if (!wasFrozen)
      OpenJoinMenu(ent);
  }
}

void OpenDmWelcomeMenu(gentity_t *ent) {
  if (!ent || !ent->client)
    return;

  const std::string title =
      fmt::format("{} v{}", worr::version::kGameTitle, worr::version::kGameVersion);
  const char *host_value =
      (hostname && hostname->string) ? hostname->string : "";

  MenuUi::UiCommandBuilder cmd(ent);
  cmd.AppendCvar("ui_welcome_title", title);
  cmd.AppendCvar("ui_welcome_hostname", host_value);
  cmd.AppendCvar("ui_welcome_motd", game.motd);
  cmd.AppendCommand("pushmenu dm_welcome");
  cmd.Flush();

  ent->client->initialMenu.dmWelcomeActive = true;
  ent->client->initialMenu.dmJoinActive = false;
}

void OpenDmJoinMenu(gentity_t *ent) {
  if (!ent || !ent->client)
    return;

  std::string title = "Match Lobby";
  if (hostname && hostname->string && hostname->string[0])
    title = hostname->string;

  std::string subtitle;
  const char *gametype = level.gametype_name.data();
  const char *mapName = level.mapName.data();
  if (gametype && *gametype && mapName && *mapName) {
    subtitle = fmt::format("{} | {}", gametype, mapName);
  } else if (gametype && *gametype) {
    subtitle = gametype;
  }
  OpenJoinMenuInternal(ent, title, subtitle, "dm_join");
}

void CloseDmJoinMenu(gentity_t *ent) {
  if (!ent || !ent->client)
    return;

  ent->client->initialMenu.dmJoinActive = false;
  if (ent->client->initialMenu.frozen) {
    ent->client->initialMenu.frozen = false;
    ent->client->initialMenu.shown = true;
    ent->client->initialMenu.delay = 0_sec;
    ent->client->initialMenu.hostSetupDone = true;
    ent->client->initialMenu.dmWelcomeActive = false;
  }
}

void ForceCloseDmJoinMenu(gentity_t *ent) {
  CloseDmJoinMenu(ent);
  MenuUi::SendUiCommand(ent, "forcemenuoff\n");
}

void OpenDmHostInfoMenu(gentity_t *ent) {
  if (!ent || !ent->client)
    return;

  std::string hostName;
  if (g_entities[1].client) {
    char value[MAX_INFO_VALUE] = {0};
    gi.Info_ValueForKey(g_entities[1].client->pers.userInfo, "name", value,
                        sizeof(value));
    if (value[0])
      hostName = value;
  }

  const char *serverName =
      (hostname && hostname->string) ? hostname->string : "";

  MenuUi::UiCommandBuilder cmd(ent);
  cmd.AppendCvar("ui_hostinfo_server", serverName);
  cmd.AppendCvar("ui_hostinfo_host", hostName);
  cmd.AppendCvar("ui_hostinfo_motd", game.motd);
  cmd.AppendCommand("pushmenu dm_hostinfo");
  cmd.Flush();
}

void OpenDmMatchInfoMenu(gentity_t *ent) {
  if (!ent || !ent->client)
    return;

  std::string author1;
  std::string author2;
  if (level.author[0])
    author1 = fmt::format("author: {}", level.author);
  if (level.author2[0])
    author2 = fmt::format("      {}", level.author2);

  std::string scorelimit;
  if (GT_ScoreLimit())
    scorelimit = fmt::format("{} limit: {}", GT_ScoreLimitString(),
                             GT_ScoreLimit());

  std::string timelimit;
  if (timeLimit->value > 0) {
    timelimit = fmt::format(
        "time limit: {}",
        TimeString(timeLimit->value * 60000, false, false));
  }

  MenuUi::UiCommandBuilder cmd(ent);
  cmd.AppendCvar("ui_matchinfo_gametype", level.gametype_name.data());
  cmd.AppendCvar("ui_matchinfo_map", fmt::format("map: {}", level.longName.data()));
  cmd.AppendCvar("ui_matchinfo_mapname", fmt::format("mapname: {}", level.mapName.data()));
  cmd.AppendCvar("ui_matchinfo_author1", author1);
  cmd.AppendCvar("ui_matchinfo_author2", author2);
  cmd.AppendCvar("ui_matchinfo_ruleset",
                 fmt::format("ruleset: {}", rs_long_name[(int)game.ruleset]));
  cmd.AppendCvar("ui_matchinfo_scorelimit", scorelimit);
  cmd.AppendCvar("ui_matchinfo_timelimit", timelimit);
  cmd.AppendCommand("pushmenu dm_matchinfo");
  cmd.Flush();
}

void OpenJoinMenu(gentity_t *ent) {
  if (!ent || !ent->client)
    return;

  if (deathmatch->integer) {
    OpenDmJoinMenu(ent);
    return;
  }

  if (Vote_Menu_Active(ent)) {
    OpenVoteMenu(ent);
    return;
  }

  const std::string title = fmt::format("{} v{}", worr::version::kGameTitle,
                                        worr::version::kGameVersion);
  OpenJoinMenuInternal(ent, title, "", "join");
}

/*
===============
OpenPlayerWelcomeMenu

Welcome menu for non-hosts. Shows welcome, hostname, MOTD, and Continue button.
===============
*/
void OpenPlayerWelcomeMenu(gentity_t *ent) {
  OpenDmWelcomeMenu(ent);
}
