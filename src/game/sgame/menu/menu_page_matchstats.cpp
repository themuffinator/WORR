/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

menu_page_matchstats.cpp (Menu Page - Match Stats) This file implements the in-game menu page
for players to view their own performance statistics for the current match. Key
Responsibilities: - Stats Display: The `onUpdate` callback dynamically populates the menu with
the player's current stats. - Data Source: It reads data directly from the
`client_match_stats_t` struct associated with the player's client entity. - Real-Time Updates:
Because it uses the `onUpdate` callback, the stats displayed in the menu are updated live as the
match progresses whenever the menu is open.*/

#include "../g_local.hpp"
#include "menu_ui_helpers.hpp"

namespace {

void UpdateMatchStatsMenu(gentity_t *ent, bool openMenu) {
  if (!ent || !ent->client || !g_matchstats->integer)
    return;

  auto &st = ent->client->pers.match;
  std::array<std::string, 16> lines{};
  size_t i = 0;

  lines[i++] = "Player Stats for Match";
  lines[i++] = G_ColorResetAfter(ent->client->sess.netName);
  lines[i++] = "--------------------------";
  lines[i++] = fmt::format("kills: {}", st.totalKills);
  lines[i++] = fmt::format("deaths: {}", st.totalDeaths);
  if (st.totalDeaths > 0)
    lines[i++] =
        fmt::format("k/d ratio: {:.2f}",
                    static_cast<float>(st.totalKills) / st.totalDeaths);
  else
    lines[i++] = "";
  lines[i++] = fmt::format("dmg dealt: {}", st.totalDmgDealt);
  lines[i++] = fmt::format("dmg received: {}", st.totalDmgReceived);
  if (st.totalDmgReceived > 0)
    lines[i++] =
        fmt::format("dmg ratio: {:.2f}",
                    static_cast<float>(st.totalDmgDealt) / st.totalDmgReceived);
  else
    lines[i++] = "";
  lines[i++] = fmt::format("shots fired: {}", st.totalShots);
  lines[i++] = fmt::format("shots on target: {}", st.totalHits);
  if (st.totalShots > 0)
    lines[i++] = fmt::format(
        "total accuracy: {}%",
        static_cast<int>((static_cast<float>(st.totalHits) / st.totalShots) *
                         100));

  MenuUi::UiCommandBuilder cmd(ent);
  for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
    cmd.AppendCvar(fmt::format("ui_matchstats_line_{}", lineIndex).c_str(),
                   lines[lineIndex]);
  }
  if (openMenu)
    cmd.AppendCommand("pushmenu match_stats");
  cmd.Flush();
}

} // namespace

void OpenPlayerMatchStatsMenu(gentity_t *ent) {
  if (!ent || !ent->client || !g_matchstats->integer)
    return;
  ent->client->ui.matchStatsActive = true;
  ent->client->ui.matchStatsNextUpdate = level.time;
  UpdateMatchStatsMenu(ent, true);
}

void RefreshMatchStatsMenu(gentity_t *ent) {
  UpdateMatchStatsMenu(ent, false);
}
