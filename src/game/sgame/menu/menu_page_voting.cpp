/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

menu_page_voting.cpp (Menu Page - Voting) This file implements the UI for participating in a
vote that has been called by another player. It displays the vote question and provides the
"Yes" and "No" options. Key Responsibilities: - Vote Display: The `onUpdate` function populates
the menu with the details of the active vote, including who called it and what the proposal is.
- Vote Actions: The `onSelect` callbacks for the "Yes" and "No" options update the server-side
vote counts (`level.vote.countYes`, `level.vote.countNo`). - Countdown Timer: Displays the time
remaining for the vote. - Automatic Closing: The menu automatically closes itself if the vote
ends (either by passing, failing, or timing out) while it is open.*/

#include "../g_local.hpp"
#include "../commands/command_system.hpp"
#include "menu_ui_helpers.hpp"

namespace {

void UpdateVoteMenu(gentity_t *ent, bool openMenu) {
  if (!ent || !ent->client)
    return;
  if (!level.vote.time || !level.vote.client || !level.vote.cmd)
    return;

  const int elapsed = (level.time - level.vote.time).seconds<int>();
  const int timeout = std::max(0, 30 - elapsed);

  std::string cmdLine = std::string(level.vote.cmd->name);
  if (!level.vote.arg.empty())
    cmdLine = fmt::format("{} {}", level.vote.cmd->name, level.vote.arg.c_str());

  const bool canVote = (level.time - level.vote.time) >= 3_sec;
  std::string readyLabel;
  std::string readyCountdown;
  if (!canVote) {
    const int remaining = std::max(0, 3 - elapsed);
    readyLabel = "GET READY TO VOTE!";
    readyCountdown = fmt::format("{}...", remaining);
  }

  MenuUi::UiCommandBuilder cmd(ent);
  cmd.AppendCvar("ui_vote_line_0",
                 fmt::format("{} called a vote:", level.vote.client->sess.netName));
  cmd.AppendCvar("ui_vote_line_1", "");
  cmd.AppendCvar("ui_vote_line_2", cmdLine);
  cmd.AppendCvar("ui_vote_can_vote", canVote ? "1" : "0");
  cmd.AppendCvar("ui_vote_ready_label", readyLabel);
  cmd.AppendCvar("ui_vote_ready_countdown", readyCountdown);
  cmd.AppendCvar("ui_vote_time_left", fmt::format("{}", timeout));
  if (openMenu)
    cmd.AppendCommand("pushmenu vote_menu");
  cmd.Flush();
}

} // namespace

void OpenVoteMenu(gentity_t *ent) {
  if (!Vote_Menu_Active(ent))
    return;
  ent->client->ui.voteActive = true;
  ent->client->ui.voteNextUpdate = level.time;
  UpdateVoteMenu(ent, true);
}

void RefreshVoteMenu(gentity_t *ent) {
  if (!Vote_Menu_Active(ent))
    return;
  UpdateVoteMenu(ent, false);
}
