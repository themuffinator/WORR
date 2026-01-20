/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

menu_page_callvote.cpp (Menu Page - Call Vote) migrated to cgame JSON menus.*/

#include "../g_local.hpp"
#include "../commands/commands.hpp"
#include "menu_ui_helpers.hpp"
#include "menu_ui_list.hpp"

#include <string_view>

namespace {

static bool VoteEnabled(std::string_view name)
{
  if (!g_allowVoting || !g_allowVoting->integer)
    return false;
  for (const auto &def : Commands::GetRegisteredVoteDefinitions()) {
    if (def.name == name) {
      if (!def.visibleInMenu)
        return false;
      return (g_vote_flags->integer & def.flag) != 0;
    }
  }
  return false;
}

} // namespace

void OpenCallvoteMenu(gentity_t *ent)
{
  if (!ent || !ent->client)
    return;

  ent->client->ui.callvoteMap.enableFlags = 0;
  ent->client->ui.callvoteMap.disableFlags = 0;

  MenuUi::UiCommandBuilder cmd(ent);
  cmd.AppendCvar("ui_callvote_show_map", VoteEnabled("map") ? "1" : "0");
  cmd.AppendCvar("ui_callvote_show_nextmap", VoteEnabled("nextmap") ? "1" : "0");
  cmd.AppendCvar("ui_callvote_show_restart", VoteEnabled("restart") ? "1" : "0");
  cmd.AppendCvar("ui_callvote_show_gametype", VoteEnabled("gametype") ? "1" : "0");
  cmd.AppendCvar("ui_callvote_show_ruleset", VoteEnabled("ruleset") ? "1" : "0");
  cmd.AppendCvar("ui_callvote_show_timelimit", VoteEnabled("timelimit") ? "1" : "0");
  cmd.AppendCvar("ui_callvote_show_scorelimit", VoteEnabled("scorelimit") ? "1" : "0");
  cmd.AppendCvar("ui_callvote_show_shuffle",
                 (Teams() && VoteEnabled("shuffle")) ? "1" : "0");
  cmd.AppendCvar("ui_callvote_show_balance",
                 (Teams() && VoteEnabled("balance")) ? "1" : "0");
  cmd.AppendCvar("ui_callvote_show_unlagged", VoteEnabled("unlagged") ? "1" : "0");
  cmd.AppendCvar("ui_callvote_show_cointoss", VoteEnabled("cointoss") ? "1" : "0");
  cmd.AppendCvar("ui_callvote_show_random", VoteEnabled("random") ? "1" : "0");
  cmd.AppendCvar("ui_callvote_show_arena",
                 (level.arenaTotal && VoteEnabled("arena")) ? "1" : "0");
  cmd.AppendCommand("pushmenu callvote_main");
  cmd.Flush();
}

void OpenCallvoteMapMenu(gentity_t *ent)
{
  UiList_Open(ent, UiListKind::CallvoteMap);
}

void OpenCallvoteGametypeMenu(gentity_t *ent)
{
  UiList_Open(ent, UiListKind::CallvoteGametype);
}

void OpenCallvoteArenaMenu(gentity_t *ent)
{
  UiList_Open(ent, UiListKind::CallvoteArena);
}

void OpenCallvoteRulesetMenu(gentity_t *ent)
{
  MenuUi::UiCommandBuilder cmd(ent);
  cmd.AppendCommand("pushmenu callvote_ruleset");
  cmd.Flush();
}

void OpenCallvoteTimelimitMenu(gentity_t *ent)
{
  if (!ent || !ent->client)
    return;

  const int cur = timeLimit ? timeLimit->integer : 0;
  const std::string currentLabel =
      fmt::format("Current: {}", cur ? TimeString(cur * 60000, false, false) : "Disabled");

  MenuUi::UiCommandBuilder cmd(ent);
  cmd.AppendCvar("ui_callvote_timelimit_current", currentLabel);
  cmd.AppendCommand("pushmenu callvote_timelimit");
  cmd.Flush();
}

void OpenCallvoteScorelimitMenu(gentity_t *ent)
{
  if (!ent || !ent->client)
    return;

  const char *metric = GT_ScoreLimitString();
  const int cur = GT_ScoreLimit();
  const std::string currentLabel =
      fmt::format("Current: {} {}", cur ? cur : 0, cur ? metric : "(Disabled)");

  MenuUi::UiCommandBuilder cmd(ent);
  cmd.AppendCvar("ui_callvote_scorelimit_current", currentLabel);
  cmd.AppendCvar("ui_callvote_scorelimit_set_5",
                 fmt::format("Set 5 {}", metric));
  cmd.AppendCvar("ui_callvote_scorelimit_set_10",
                 fmt::format("Set 10 {}", metric));
  cmd.AppendCvar("ui_callvote_scorelimit_set_15",
                 fmt::format("Set 15 {}", metric));
  cmd.AppendCvar("ui_callvote_scorelimit_set_20",
                 fmt::format("Set 20 {}", metric));
  cmd.AppendCvar("ui_callvote_scorelimit_set_25",
                 fmt::format("Set 25 {}", metric));
  cmd.AppendCvar("ui_callvote_scorelimit_set_30",
                 fmt::format("Set 30 {}", metric));
  cmd.AppendCvar("ui_callvote_scorelimit_set_50",
                 fmt::format("Set 50 {}", metric));
  cmd.AppendCvar("ui_callvote_scorelimit_set_100",
                 fmt::format("Set 100 {}", metric));
  cmd.AppendCommand("pushmenu callvote_scorelimit");
  cmd.Flush();
}

void OpenCallvoteUnlaggedMenu(gentity_t *ent)
{
  if (!ent || !ent->client)
    return;

  const bool cur = g_lagCompensation && g_lagCompensation->integer;
  const std::string currentLabel = fmt::format("Current: {}",
                                               cur ? "ENABLED" : "DISABLED");

  MenuUi::UiCommandBuilder cmd(ent);
  cmd.AppendCvar("ui_callvote_unlagged_current", currentLabel);
  cmd.AppendCommand("pushmenu callvote_unlagged");
  cmd.Flush();
}

void OpenCallvoteRandomMenu(gentity_t *ent)
{
  MenuUi::UiCommandBuilder cmd(ent);
  cmd.AppendCommand("pushmenu callvote_random");
  cmd.Flush();
}

namespace {

void UpdateCallvoteMapFlagsMenu(gentity_t *ent, bool openMenu)
{
  if (!ent || !ent->client)
    return;

  const auto &state = ent->client->ui.callvoteMap;
  MenuUi::UiCommandBuilder cmd(ent);
  for (const auto &flag : MapFlagEntries()) {
    cmd.AppendCvar(fmt::format("ui_callvote_flag_{}", flag.code).c_str(),
                   MapFlagStateLabel(state, flag));
  }
  if (openMenu)
    cmd.AppendCommand("pushmenu callvote_map_flags");
  cmd.Flush();
}

} // namespace

void OpenCallvoteMapFlagsMenu(gentity_t *ent)
{
  UpdateCallvoteMapFlagsMenu(ent, true);
}

void RefreshCallvoteMapFlagsMenu(gentity_t *ent)
{
  UpdateCallvoteMapFlagsMenu(ent, false);
}
