/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

menu_page_tournament.cpp (Menu Page - Tournament) provides the tournament
information panel, map order list, veto UI, and replay confirmation flows.
*/

#include "../g_local.hpp"

#include <algorithm>
#include <vector>

namespace {

std::string MapDisplayName(std::string_view mapName) {
  const MapEntry *entry = game.mapSystem.GetMapEntry(std::string(mapName));
  if (entry && !entry->longName.empty()) {
    return G_Fmt("{} ({})", entry->longName.c_str(), mapName.data()).data();
  }
  return std::string(mapName);
}

bool MapIsSelected(std::string_view mapName) {
  for (const auto &map : game.tournament.mapPicks) {
    if (_stricmp(map.c_str(), mapName.data()) == 0)
      return true;
  }
  for (const auto &map : game.tournament.mapBans) {
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

bool TournamentBansAllowed() {
  const int picksRemaining = TournamentPicksRemaining();
  if (picksRemaining <= 0)
    return false;
  return TournamentRemainingMaps() - 1 >= picksRemaining;
}

std::string TournamentSideLabel(bool homeSide) {
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
  for (const auto &participant : game.tournament.participants) {
    if (participant.socialId == id && !participant.name.empty()) {
      return G_Fmt("{} ({})", sideName, participant.name.c_str()).data();
    }
  }

  return sideName;
}

bool TournamentActorTurn(gentity_t *ent) {
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

std::vector<std::string> TournamentAvailableMaps() {
  std::vector<std::string> maps;
  maps.reserve(game.tournament.mapPool.size());
  for (const auto &map : game.tournament.mapPool) {
    if (MapIsSelected(map))
      continue;
    maps.push_back(map);
  }
  return maps;
}

void OpenTournamentVetoMapMenu(gentity_t *ent, TournamentVetoAction action);
void OpenTournamentReplayConfirmMenu(gentity_t *ent, int gameNumber);

} // namespace

void OpenTournamentInfoMenu(gentity_t *ent) {
  MenuBuilder builder;
  builder.add("*Tournament Info*", MenuAlign::Center);
  builder.spacer();
  builder.add("Tournament format runs a match", MenuAlign::Left);
  builder.add("as a best-of set of games.", MenuAlign::Left);
  builder.add("Rosters are locked to the", MenuAlign::Left);
  builder.add("listed participants.", MenuAlign::Left);
  builder.spacer();
  builder.add("When everyone is ready,", MenuAlign::Left);
  builder.add("home picks or bans first.", MenuAlign::Left);
  builder.add("The decider game is picked", MenuAlign::Left);
  builder.add("at random from what remains.", MenuAlign::Left);
  builder.spacer();
  builder.add("Use tourney_status in the", MenuAlign::Left);
  builder.add("console for live updates.", MenuAlign::Left);
  builder.spacer();
  builder.add("Back", MenuAlign::Left, [](gentity_t *e, Menu &) {
    OpenJoinMenu(e);
  });
  MenuSystem::Open(ent, builder.build());
}

void OpenTournamentMapChoicesMenu(gentity_t *ent) {
  MenuBuilder builder;
  builder.add("*Tournament Map Choices*", MenuAlign::Center);
  builder.spacer();

  if (!Tournament_IsActive() || !game.tournament.vetoComplete ||
      game.tournament.mapOrder.empty()) {
    builder.add("Map order appears once", MenuAlign::Left);
    builder.add("picks and bans finish.", MenuAlign::Left);
  } else {
    int index = 1;
    for (const auto &map : game.tournament.mapOrder) {
      const std::string display = MapDisplayName(map);
      builder.add(G_Fmt("{}: {}", index, display.c_str()).data(),
                  MenuAlign::Left);
      index++;
    }
  }

  builder.spacer();
  builder.add("Back", MenuAlign::Left, [](gentity_t *e, Menu &) {
    OpenJoinMenu(e);
  });
  MenuSystem::Open(ent, builder.build());
}

void OpenTournamentVetoMenu(gentity_t *ent) {
  if (!ent || !ent->client)
    return;

  MenuBuilder builder;
  builder.add("*Tournament Veto*", MenuAlign::Center);
  builder.spacer();

  if (!Tournament_IsActive() || game.tournament.vetoComplete) {
    builder.add("Veto is not active.", MenuAlign::Left);
    builder.spacer();
    builder.add("Back", MenuAlign::Left, [](gentity_t *e, Menu &) {
      OpenJoinMenu(e);
    });
    MenuSystem::Open(ent, builder.build());
    return;
  }

  const std::string sideLabel =
      TournamentSideLabel(game.tournament.vetoHomeTurn);
  builder.add(G_Fmt("Turn: {}", sideLabel.c_str()).data(),
              MenuAlign::Left);
  builder.spacer();

  if (!TournamentActorTurn(ent)) {
    builder.add("Waiting for the active", MenuAlign::Left);
    builder.add("side to make a choice.", MenuAlign::Left);
    builder.spacer();
    builder.add("Back", MenuAlign::Left, [](gentity_t *e, Menu &) {
      OpenJoinMenu(e);
    });
    MenuSystem::Open(ent, builder.build());
    return;
  }

  builder.add("Pick", MenuAlign::Left,
              [](gentity_t *e, Menu &) {
                OpenTournamentVetoMapMenu(e, TournamentVetoAction::Pick);
              });

  if (TournamentBansAllowed()) {
    builder.add("Ban", MenuAlign::Left,
                [](gentity_t *e, Menu &) {
                  OpenTournamentVetoMapMenu(e, TournamentVetoAction::Ban);
                });
  } else {
    builder.add("Ban (locked)", MenuAlign::Left);
  }

  builder.spacer();
  builder.add(G_Fmt("Picks needed: {}", TournamentPicksRemaining()).data(),
              MenuAlign::Left);
  builder.add(G_Fmt("Maps remaining: {}", TournamentRemainingMaps()).data(),
              MenuAlign::Left);
  builder.spacer();
  builder.add("Back", MenuAlign::Left, [](gentity_t *e, Menu &) {
    OpenJoinMenu(e);
  });

  MenuSystem::Open(ent, builder.build());
}

void OpenTournamentReplayMenu(gentity_t *ent) {
  MenuBuilder builder;
  builder.add("*Replay Tournament Game*", MenuAlign::Center);
  builder.spacer();

  if (!Tournament_IsActive() || game.tournament.mapOrder.empty()) {
    builder.add("Replay is available once", MenuAlign::Left);
    builder.add("the map order is locked.", MenuAlign::Left);
    builder.spacer();
    builder.add("Back", MenuAlign::Left, [](gentity_t *e, Menu &) {
      OpenAdminSettingsMenu(e);
    });
    MenuSystem::Open(ent, builder.build());
    return;
  }

  int gameNum = 1;
  for (const auto &map : game.tournament.mapOrder) {
    const std::string display = MapDisplayName(map);
    builder.add(
        G_Fmt("Replay game {}: {}", gameNum, display.c_str())
            .data(),
        MenuAlign::Left, [gameNum](gentity_t *e, Menu &) {
          OpenTournamentReplayConfirmMenu(e, gameNum);
        });
    gameNum++;
  }

  builder.spacer();
  builder.add("Back", MenuAlign::Left, [](gentity_t *e, Menu &) {
    OpenAdminSettingsMenu(e);
  });
  MenuSystem::Open(ent, builder.build());
}

namespace {

void OpenTournamentVetoMapMenu(gentity_t *ent, TournamentVetoAction action) {
  if (!ent || !ent->client)
    return;

  MenuBuilder builder;
  builder.add(action == TournamentVetoAction::Pick
                  ? "*Pick a Map*"
                  : "*Ban a Map*",
              MenuAlign::Center);
  builder.spacer();

  if (!Tournament_IsActive() || game.tournament.vetoComplete) {
    builder.add("Veto is not active.", MenuAlign::Left);
  } else {
    const auto maps = TournamentAvailableMaps();
    if (maps.empty()) {
      builder.add("No maps remain to", MenuAlign::Left);
      builder.add("pick or ban.", MenuAlign::Left);
    } else {
      for (const auto &map : maps) {
        builder.add(MapDisplayName(map), MenuAlign::Left,
                    [action, map](gentity_t *e, Menu &) {
                      std::string message;
                      if (!Tournament_HandleVetoAction(e, action, map,
                                                       message)) {
                        if (!message.empty()) {
                          gi.Client_Print(
                              e, PRINT_HIGH,
                              G_Fmt("{}\n", message.c_str()).data());
                        }
                        return;
                      }

                      if (!message.empty()) {
                        gi.Client_Print(
                            e, PRINT_HIGH,
                            G_Fmt("{}\n", message.c_str()).data());
                      }
                      MenuSystem::Close(e);
                    });
      }
    }
  }

  builder.spacer();
  builder.add("Back", MenuAlign::Left, [](gentity_t *e, Menu &) {
    OpenTournamentVetoMenu(e);
  });

  MenuSystem::Open(ent, builder.build());
}

void OpenTournamentReplayConfirmMenu(gentity_t *ent, int gameNumber) {
  auto menu = std::make_unique<Menu>();
  menu->entries.emplace_back("CONFIRM REPLAY", MenuAlign::Center);
  menu->entries.emplace_back("", MenuAlign::Center);
  menu->entries.emplace_back(
      G_Fmt("Replay game {}?", gameNumber).data(), MenuAlign::Center);
  menu->entries.emplace_back("", MenuAlign::Center);

  MenuEntry yesOpt("YES", MenuAlign::Center);
  yesOpt.onSelect = [gameNumber](gentity_t *e, Menu &) {
    std::string message;
    if (!Tournament_ReplayGame(gameNumber, message)) {
      if (!message.empty()) {
        gi.Client_Print(e, PRINT_HIGH,
                        G_Fmt("{}\n", message.c_str()).data());
      }
    }
    MenuSystem::Close(e);
  };
  menu->entries.push_back(std::move(yesOpt));

  MenuEntry noOpt("NO", MenuAlign::Center);
  noOpt.onSelect = [](gentity_t *e, Menu &) { OpenTournamentReplayMenu(e); };
  menu->entries.push_back(std::move(noOpt));

  MenuSystem::Open(ent, std::move(menu));
}

} // namespace
