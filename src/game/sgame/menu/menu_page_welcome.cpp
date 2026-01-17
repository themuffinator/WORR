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
#include <sstream>

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

static void ReleaseWelcomeFreeze(gentity_t *ent) {
  if (!ent || !ent->client)
    return;

  if (!ent->client->initialMenu.frozen)
    return;

  ent->client->initialMenu.frozen = false;
  ent->client->initialMenu.shown = true;
  ent->client->initialMenu.delay = 0_sec;
  ent->client->initialMenu.hostSetupDone = true;
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

static void AddJoinOptions(MenuBuilder &builder, gentity_t *ent,
                           int maxPlayers) {
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

  if (Teams()) {
    builder.add(fmt::format("Join Red ({}/{})", redCount, maxPlayers / 2),
                MenuAlign::Left,
                [](gentity_t *e, Menu &) { TryJoinTeam(e, Team::Red); });
    builder.add(fmt::format("Join Blue ({}/{})", blueCount, maxPlayers / 2),
                MenuAlign::Left,
                [](gentity_t *e, Menu &) { TryJoinTeam(e, Team::Blue); });
  } else {
    std::string joinText;
    if (duelQueueAllowed && level.pop.num_playing_clients == 2)
      joinText = fmt::format("Join Queue ({}/{})", queueCount, maxPlayers - 2);
    else
      joinText = fmt::format("Join Match ({}/{})", freeCount,
                             Game::Has(GameFlags::OneVOne) ? 2 : maxPlayers);

    builder.add(joinText, MenuAlign::Left,
                [](gentity_t *e, Menu &) { TryJoinTeam(e, Team::Free); });
  }
}

void OpenJoinMenu(gentity_t *ent) {
  if (!ent || !ent->client)
    return;

  if (Vote_Menu_Active(ent)) {
    OpenVoteMenu(ent);
    return;
  }

  int maxPlayers = maxplayers->integer;
  if (maxPlayers < 1)
    maxPlayers = 1;

  MenuBuilder builder;
  builder
      .add(G_Fmt("{} v{}", worr::version::kGameTitle,
                 worr::version::kGameVersion)
               .data(),
           MenuAlign::Center)
      .spacer();
  builder.add("---", MenuAlign::Center).spacer().spacer();

  const bool welcomeFrozen = ent->client->initialMenu.frozen;
  const bool isPlaying = ClientIsPlaying(ent->client);
  const bool isTournament = Tournament_IsActive();
  const bool showJoinOptions = (welcomeFrozen || !isPlaying) && !isTournament;
  const bool showSpectate = (welcomeFrozen || isPlaying) && !isTournament;

  if (showJoinOptions)
    AddJoinOptions(builder, ent, maxPlayers);

  if (showSpectate) {
    builder.add("Spectate", MenuAlign::Left,
                [](gentity_t *e, Menu &) { SelectSpectate(e); });
  }

  if (isTournament) {
    builder.add("Information", MenuAlign::Left,
                [](gentity_t *e, Menu &) { OpenTournamentInfoMenu(e); });

    if (Tournament_VetoComplete() && !game.tournament.mapOrder.empty()) {
      builder.add("Map Choices", MenuAlign::Left,
                  [](gentity_t *e, Menu &) { OpenTournamentMapChoicesMenu(e); });
    }
  }

  if (!isTournament && g_allowVoting->integer &&
      (ClientIsPlaying(ent->client) ||
       (!ClientIsPlaying(ent->client) && g_allowSpecVote->integer))) {
    builder.add("Call a Vote", MenuAlign::Left,
                [](gentity_t *e, Menu &) { OpenCallvoteMenu(e); });
  }

  if (!isTournament && g_maps_mymap && g_maps_mymap->integer &&
      (!g_allowMymap || g_allowMymap->integer)) {
    builder.add("MyMap", MenuAlign::Left,
                [](gentity_t *e, Menu &) { OpenMyMapMenu(e); });
  }

  builder.add("Host Info", MenuAlign::Left,
              [](gentity_t *e, Menu &) { OpenHostInfoMenu(e); });

  // [Paril-KEX] Forfeit option
  if (!isTournament && g_allowVoting->integer && ClientIsPlaying(ent->client) &&
      (level.matchState == MatchState::In_Progress ||
       level.matchState == MatchState::Countdown)) {
    builder.add("Forfeit", MenuAlign::Left,
                [](gentity_t *e, Menu &) { OpenForfeitMenu(e); });
  }

  builder.add("Match Info", MenuAlign::Left,
              [](gentity_t *e, Menu &) { OpenMatchInfoMenu(e); });

  if (g_matchstats->integer) {
    builder.add("Player Stats", MenuAlign::Left,
                [](gentity_t *e, Menu &) { OpenPlayerMatchStatsMenu(e); });
  }

  if (ent->client->sess.admin) {
    builder.add("Admin", MenuAlign::Left,
                [](gentity_t *e, Menu &) { OpenAdminSettingsMenu(e); });
  }

  builder.spacer().spacer().spacer().spacer();
  builder.add("visit darkmatter-quake.com", MenuAlign::Center);
  builder.add(":: community :: matches ::", MenuAlign::Center);
  builder.add(":: content :: news ::", MenuAlign::Center);
  MenuSystem::Open(ent, builder.build());
}

/*
===============
WordWrapText

Wraps text to fit within a specified width, returning up to maxLines lines.
===============
*/
static std::vector<std::string> WordWrapText(const std::string &text,
                                             size_t maxWidth, size_t maxLines) {
  std::vector<std::string> lines;
  if (text.empty() || maxWidth == 0 || maxLines == 0)
    return lines;

  std::istringstream stream(text);
  std::string word;
  std::string currentLine;

  while (stream >> word) {
    if (lines.size() >= maxLines)
      break;

    if (currentLine.empty()) {
      currentLine = word;
    } else if (currentLine.size() + 1 + word.size() <= maxWidth) {
      currentLine += " " + word;
    } else {
      lines.push_back(currentLine);
      currentLine = word;
      if (lines.size() >= maxLines)
        break;
    }
  }

  if (!currentLine.empty() && lines.size() < maxLines)
    lines.push_back(currentLine);

  return lines;
}

/*
===============
OpenPlayerWelcomeMenu

Welcome menu for non-hosts. Shows welcome, hostname, MOTD, and Continue button.
===============
*/
void OpenPlayerWelcomeMenu(gentity_t *ent) {
  if (!ent || !ent->client)
    return;

  MenuBuilder b;

  // Line 1: "Welcome to"
  b.add("Welcome to", MenuAlign::Center);

  // Line 2: Product name and version
  b.add(G_Fmt("{} v{}", worr::version::kGameTitle, worr::version::kGameVersion)
            .data(),
        MenuAlign::Center);

  // Line 3: blank
  b.spacer();

  // Line 4: hostname (or skip if none set)
  bool hasHostname =
      hostname && hostname->string && hostname->string[0] != '\0';
  if (hasHostname) {
    b.add(hostname->string, MenuAlign::Center);
    // Line 5: blank (only if hostname was shown)
    b.spacer();
  }

  // Line 6+: MOTD word-wrapped for up to 5 lines
  if (!game.motd.empty()) {
    auto motdLines = WordWrapText(game.motd, MAX_MENU_WIDTH, 5);
    for (const auto &line : motdLines) {
      b.add(line, MenuAlign::Left);
    }
    if (!motdLines.empty())
      b.spacer();
  }

  // Last line: "Continue" (default selection)
  b.addDefault("Continue", MenuAlign::Left, [](gentity_t *e, Menu &) {
    ReleaseWelcomeFreeze(e);
    MenuSystem::Close(e);

    // Progress to join menu or auto-join based on server settings
    if (match_autoJoin && match_autoJoin->integer) {
      // Auto-join to appropriate team
      Team targetTeam = Teams() ? PickTeam(-1) : Team::Free;
      SetTeam(e, targetTeam, false, false, false);
    } else {
      OpenJoinMenu(e);
    }
  });

  MenuSystem::Open(ent, b.build());
}
