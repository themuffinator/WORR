/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

menu_page_mapselector.cpp (Menu Page - Map Selector) This file implements the end-of-match map
voting screen. This is a critical part of the multiplayer flow, allowing players to choose the
next map to be played from a list of randomly selected candidates. Key Responsibilities: - Map
Candidate Display: The `onUpdate` function populates the menu with the names of the three
candidate maps chosen by the server. - Vote Casting: The `onSelect` callbacks for each map
option call the `MapSelector_CastVote` function to register the player's choice. - Countdown
Timer: It renders a visual progress bar to show the time remaining for the vote. - Post-Vote
State: After a player has voted, the menu updates to show an acknowledgment message, preventing
them from voting again.*/

#include "../g_local.hpp"
#include "menu_ui_helpers.hpp"

namespace {

constexpr int kMapSelectorCandidates = 3;
constexpr int kMapSelectorBarSegments = 28;

std::string MapDisplayName(std::string_view mapName) {
  const MapEntry *entry = game.mapSystem.GetMapEntry(std::string(mapName));
  if (entry && !entry->longName.empty())
    return std::string(entry->longName);
  return std::string(mapName);
}

void UpdateMapSelectorMenu(gentity_t *ent, bool openMenu) {
  if (!ent || !ent->client)
    return;

  auto &ms = level.mapSelector;
  const int clientNum = ent->s.number - 1;
  if (clientNum < 0 || clientNum >= MAX_CLIENTS)
    return;

  const int vote = ms.votes[clientNum];
  const bool hasVoted =
      (vote >= 0 && vote < kMapSelectorCandidates && !ms.candidates[vote].empty());

  MenuUi::UiCommandBuilder cmd(ent);
  cmd.AppendCvar("ui_mapselector_title",
                 hasVoted ? "" : "Vote for the next arena:");

  for (int i = 0; i < kMapSelectorCandidates; ++i) {
    const auto &candidateId = ms.candidates[i];
    const bool showEntry = !hasVoted && !candidateId.empty();
    const std::string display = showEntry ? MapDisplayName(candidateId) : "";
    cmd.AppendCvar(fmt::format("ui_mapselector_option_{}", i).c_str(), display);
    cmd.AppendCvar(fmt::format("ui_mapselector_option_show_{}", i).c_str(),
                   showEntry ? "1" : "0");
  }

  if (hasVoted) {
    const auto &candidateId = ms.candidates[vote];
    cmd.AppendCvar("ui_mapselector_ack_show", "1");
    cmd.AppendCvar("ui_mapselector_ack_0", "Vote cast:");
    cmd.AppendCvar("ui_mapselector_ack_1", MapDisplayName(candidateId));
  } else {
    cmd.AppendCvar("ui_mapselector_ack_show", "0");
    cmd.AppendCvar("ui_mapselector_ack_0", "");
    cmd.AppendCvar("ui_mapselector_ack_1", "");
  }

  float elapsed = (level.time - ms.voteStartTime).seconds();
  elapsed = std::clamp(elapsed, 0.0f, MAP_SELECTOR_DURATION.seconds());

  const int filled = static_cast<int>(
      (elapsed / MAP_SELECTOR_DURATION.seconds()) * kMapSelectorBarSegments);
  const int empty = std::max(0, kMapSelectorBarSegments - filled);
  cmd.AppendCvar("ui_mapselector_bar",
                 std::string(filled, '=') + std::string(empty, ' '));

  if (openMenu)
    cmd.AppendCommand("pushmenu map_selector");
  cmd.Flush();
}

} // namespace

/*
========================
OpenMapSelectorMenu
========================
*/
void OpenMapSelectorMenu(gentity_t *ent) {
  if (!ent || !ent->client)
    return;
  ent->client->ui.mapSelectorActive = true;
  ent->client->ui.mapSelectorNextUpdate = level.time;
  UpdateMapSelectorMenu(ent, true);
}

void RefreshMapSelectorMenu(gentity_t *ent) {
  if (!ent || !ent->client)
    return;
  UpdateMapSelectorMenu(ent, false);
}
