/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

menu_page_tournament.cpp (Menu Page - Tournament) provides the tournament
information panel, map order list, veto UI, and replay confirmation flows.
*/

#include "../g_local.hpp"
#include "menu_ui_helpers.hpp"
#include "menu_ui_list.hpp"

#include <algorithm>

namespace {

std::string MapDisplayName(std::string_view mapName) {
  const MapEntry *entry = game.mapSystem.GetMapEntry(std::string(mapName));
  if (entry && !entry->longName.empty()) {
    return G_Fmt("{} ({})", entry->longName.c_str(), mapName.data()).data();
  }
  return std::string(mapName);
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

} // namespace

void OpenTournamentInfoMenu(gentity_t *ent) {
  if (!ent || !ent->client)
    return;

  MenuUi::UiCommandBuilder cmd(ent);
  cmd.AppendCommand("pushmenu tourney_info");
  cmd.Flush();
}

void OpenTournamentMapChoicesMenu(gentity_t *ent) {
  if (!ent || !ent->client)
    return;

  std::array<std::string, 10> lines{};
  size_t lineIndex = 0;

  if (!Tournament_IsActive() || !game.tournament.vetoComplete ||
      game.tournament.mapOrder.empty()) {
    lines[lineIndex++] = "Map order appears once";
    if (lineIndex < lines.size())
      lines[lineIndex++] = "picks and bans finish.";
  } else {
    int mapIndex = 1;
    for (const auto &map : game.tournament.mapOrder) {
      if (lineIndex >= lines.size())
        break;
      lines[lineIndex++] = fmt::format("{}: {}", mapIndex, MapDisplayName(map));
      ++mapIndex;
    }
  }

  MenuUi::UiCommandBuilder cmd(ent);
  for (size_t i = 0; i < lines.size(); ++i) {
    cmd.AppendCvar(fmt::format("ui_tourney_mapchoice_line_{}", i).c_str(),
                   lines[i]);
  }
  cmd.AppendCommand("pushmenu tourney_mapchoices");
  cmd.Flush();
}

void OpenTournamentVetoMenu(gentity_t *ent) {
  if (!ent || !ent->client)
    return;

  MenuUi::UiCommandBuilder cmd(ent);

  if (!Tournament_IsActive() || game.tournament.vetoComplete) {
    cmd.AppendCvar("ui_tourney_veto_show_inactive", "1");
    cmd.AppendCvar("ui_tourney_veto_inactive", "Veto is not active.");
    cmd.AppendCvar("ui_tourney_veto_turn", "");
    cmd.AppendCvar("ui_tourney_veto_show_wait", "0");
    cmd.AppendCvar("ui_tourney_veto_wait_0", "");
    cmd.AppendCvar("ui_tourney_veto_wait_1", "");
    cmd.AppendCvar("ui_tourney_veto_can_pick", "0");
    cmd.AppendCvar("ui_tourney_veto_can_ban", "0");
    cmd.AppendCvar("ui_tourney_veto_picks_needed", "");
    cmd.AppendCvar("ui_tourney_veto_maps_remaining", "");
    cmd.AppendCommand("pushmenu tourney_veto");
    cmd.Flush();
    return;
  }

  cmd.AppendCvar("ui_tourney_veto_show_inactive", "0");
  cmd.AppendCvar("ui_tourney_veto_inactive", "");
  cmd.AppendCvar("ui_tourney_veto_turn",
                 fmt::format("Turn: {}",
                             TournamentSideLabel(game.tournament.vetoHomeTurn)));

  if (!TournamentActorTurn(ent)) {
    cmd.AppendCvar("ui_tourney_veto_show_wait", "1");
    cmd.AppendCvar("ui_tourney_veto_wait_0", "Waiting for the active");
    cmd.AppendCvar("ui_tourney_veto_wait_1", "side to make a choice.");
    cmd.AppendCvar("ui_tourney_veto_can_pick", "0");
    cmd.AppendCvar("ui_tourney_veto_can_ban", "0");
    cmd.AppendCvar("ui_tourney_veto_picks_needed", "");
    cmd.AppendCvar("ui_tourney_veto_maps_remaining", "");
  } else {
    cmd.AppendCvar("ui_tourney_veto_show_wait", "0");
    cmd.AppendCvar("ui_tourney_veto_wait_0", "");
    cmd.AppendCvar("ui_tourney_veto_wait_1", "");
    cmd.AppendCvar("ui_tourney_veto_can_pick", "1");
    cmd.AppendCvar("ui_tourney_veto_can_ban",
                   TournamentBansAllowed() ? "1" : "0");
    cmd.AppendCvar("ui_tourney_veto_picks_needed",
                   fmt::format("Picks needed: {}", TournamentPicksRemaining()));
    cmd.AppendCvar(
        "ui_tourney_veto_maps_remaining",
        fmt::format("Maps remaining: {}", TournamentRemainingMaps()));
  }

  cmd.AppendCommand("pushmenu tourney_veto");
  cmd.Flush();
}

void OpenTournamentReplayMenu(gentity_t *ent) {
  UiList_Open(ent, UiListKind::TournamentReplay);
}

void OpenTournamentReplayConfirmMenu(gentity_t *ent, int gameNumber) {
  if (!ent || !ent->client)
    return;

  MenuUi::UiCommandBuilder cmd(ent);
  cmd.AppendCvar("ui_tourney_replay_prompt",
                 fmt::format("Replay game {}?", gameNumber));
  cmd.AppendCvar("ui_tourney_replay_yes_cmd",
                 fmt::format("worr_tourney_replay {}", gameNumber));
  cmd.AppendCommand("pushmenu tourney_replay_confirm");
  cmd.Flush();
}
