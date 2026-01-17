/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

commands_voting.cpp - Implements the voting system commands. This module
contains all logic for calling votes, casting votes, and processing the results
for various game actions.*/

#include "command_voting.hpp"
#include "../g_local.hpp"
#include "command_registration.hpp"
#include "command_system.hpp"
#include "command_validation.hpp"
#include "command_voting_utils.hpp"
#include <algorithm>
#include <cctype>
#include <format>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Commands {

// --- Forward Declarations ---
void CallVote(gentity_t *ent, const CommandArgs &args);
void Vote(gentity_t *ent, const CommandArgs &args);

// --- Voting System Internals ---

// Use a map for efficient O(1) lookup of vote commands.
static std::unordered_map<std::string, VoteCommand, StringViewHash,
                          std::equal_to<>>
    s_voteCommands;
static std::vector<VoteDefinitionView> s_voteDefinitions;

bool IsVoteCommandEnabled(std::string_view name) {
  if (!g_allowVoting || !g_allowVoting->integer) {
    return false;
  }
  if (Tournament_IsActive()) {
    return false;
  }

  auto it = s_voteCommands.find(name);
  if (it == s_voteCommands.end()) {
    return false;
  }

  const VoteCommand &cmd = it->second;
  return (g_vote_flags->integer & cmd.flag) != 0;
}

/*
=============
RegisterVoteCommand

Helper to store vote command metadata and expose menu definitions with stable
ownership.
=============
*/
static void
RegisterVoteCommand(std::string_view name,
                    bool (*validateFn)(gentity_t *, const CommandArgs &),
                    void (*executeFn)(), int32_t flag, int8_t minArgs,
                    std::string_view argsUsage, std::string_view helpText,
                    bool visibleInMenu = true) {
  VoteCommand command{name,    validateFn, executeFn, flag,
                      minArgs, argsUsage,  helpText};
  auto [iter, inserted] = s_voteCommands.emplace(std::string(name), command);
  if (!inserted) {
    iter->second = std::move(command);
  }

  auto definitionIt = std::find_if(
      s_voteDefinitions.begin(), s_voteDefinitions.end(),
      [&](const VoteDefinitionView &view) { return view.name == iter->first; });

  if (definitionIt == s_voteDefinitions.end()) {
    s_voteDefinitions.push_back(
        {std::string(iter->first), flag, visibleInMenu});
  } else {
    definitionIt->name = iter->first;
    definitionIt->flag = flag;
    definitionIt->visibleInMenu = visibleInMenu;
  }
}

// --- Vote Execution Functions ("Pass_*") ---

static void Pass_Map() {
  const MapEntry *map = game.mapSystem.GetMapEntry(level.vote.arg);
  if (!map) {
    gi.Com_Print("Error: Map not found in pool at vote pass stage.\n");
    return;
  }
  level.changeMap = map->filename.c_str();
  game.map.overrideEnableFlags = level.vote_flags_enable;
  game.map.overrideDisableFlags = level.vote_flags_disable;
  ExitLevel(true);
}

/*
=============
Pass_NextMap

Advances to the next map and removes any consumed queue entries to keep
queue state consistent.
=============
*/
static void Pass_NextMap() {
  if (!game.mapSystem.playQueue.empty()) {
    const auto &queued = game.mapSystem.playQueue.front();
    level.changeMap = queued.filename.c_str();
    game.map.overrideEnableFlags = queued.enableFlags;
    game.map.overrideDisableFlags = queued.disableFlags;
    ExitLevel(true);
    game.mapSystem.ConsumeQueuedMap();
    return;
  }

  auto result = AutoSelectNextMap();
  if (result.has_value()) {
    level.changeMap = result->filename.c_str();
    game.map.overrideEnableFlags = 0;
    game.map.overrideDisableFlags = 0;
    ExitLevel(true);
    game.mapSystem.ConsumeQueuedMap();
  } else {
    game.map.overrideEnableFlags = 0;
    game.map.overrideDisableFlags = 0;
    gi.Broadcast_Print(PRINT_HIGH, "No eligible maps available.\n");
  }
}

static void Pass_RestartMatch() { Match_Reset(); }
static void Pass_ShuffleTeams() { TeamSkillShuffle(); }
static void Pass_BalanceTeams() { TeamBalance(true); }

static void Pass_Unlagged() {
  const bool enable = CommandArgs::ParseInt(level.vote.arg).value_or(0) != 0;
  gi.cvarForceSet("g_lag_compensation", enable ? "1" : "0");
  gi.LocBroadcast_Print(PRINT_HIGH, "Lag compensation has been {}.\n",
                        enable ? "ENABLED" : "DISABLED");
}

static void Pass_Cointoss() {
  const bool heads = brandom();
  gi.LocBroadcast_Print(PRINT_HIGH, "Coin toss result: {}!\n",
                        heads ? "HEADS" : "TAILS");
}

static void Pass_Random() {
  const int maxValue = CommandArgs::ParseInt(level.vote.arg).value_or(0);
  if (maxValue <= 0) {
    gi.Com_Print("Random vote passed with invalid range.\n");
    return;
  }

  const int roll = irandom(1, maxValue + 1);
  gi.LocBroadcast_Print(PRINT_HIGH, "Random roll (1-{}): {}\n", maxValue, roll);
}

static void Pass_Arena() {
  auto desiredArena = CommandArgs::ParseInt(level.vote.arg);
  if (!desiredArena || !ChangeArena(*desiredArena)) {
    gi.Com_Print("Arena vote failed to change arenas.\n");
    return;
  }

  gi.LocBroadcast_Print(PRINT_HIGH, "Arena {} is now active.\n", *desiredArena);
}

static void Pass_Forfeit() {
  gi.Broadcast_Print(PRINT_HIGH, "Forfeit vote passed. Match ending.\n");
  ::Match_End();
}

static void Pass_Gametype() {
  auto gt = Game::FromString(level.vote.arg);
  if (gt) {
    ChangeGametype(*gt);
  }
}

static void Pass_Ruleset() {
  Ruleset rs = RS_IndexFromString(level.vote.arg.c_str());
  if (rs != Ruleset::None) {
    std::string cvar_val = std::format("{}", static_cast<int>(rs));
    gi.cvarForceSet("g_ruleset", cvar_val.c_str());
  }
}

static void Pass_Timelimit() {
  if (auto val = CommandArgs::ParseInt(level.vote.arg)) {
    if (*val == 0) {
      gi.Broadcast_Print(PRINT_HIGH, "Time limit has been DISABLED.\n");
    } else {
      gi.LocBroadcast_Print(PRINT_HIGH, "Time limit has been set to {}.\n",
                            TimeString(*val * 60000, false, false));
    }
    gi.cvarForceSet("timeLimit", level.vote.arg.c_str());
  }
}

static void Pass_Scorelimit() {
  if (auto val = CommandArgs::ParseInt(level.vote.arg)) {
    if (*val == 0) {
      gi.Broadcast_Print(PRINT_HIGH, "Score limit has been DISABLED.\n");
    } else {
      gi.LocBroadcast_Print(PRINT_HIGH, "Score limit has been set to {}.\n",
                            *val);
    }
    std::string limitCvar = std::format("{}limit", GT_ScoreLimitString());
    gi.cvarForceSet(limitCvar.c_str(), level.vote.arg.c_str());
  }
}

// --- Vote Validation Functions ("Validate_*") ---

static bool Validate_None(gentity_t *ent, const CommandArgs &args) {
  return true;
}

static bool Validate_Map(gentity_t *ent, const CommandArgs &args) {
  std::string_view mapName = args.getString(2);
  const MapEntry *map = game.mapSystem.GetMapEntry(std::string(mapName));

  if (!map) {
    gi.LocClient_Print(ent, PRINT_HIGH, "Map '{}' not found in map pool.\n",
                       mapName.data());
    return false;
  }
  if (map->lastPlayed) {
    const int64_t secondsSinceStart = std::max<int64_t>(
        0, static_cast<int64_t>(time(nullptr) - game.serverStartTime));
    const int64_t delta = secondsSinceStart - map->lastPlayed;
    const int cooldownSeconds = 1800;
    if (delta < 0 || delta < cooldownSeconds) {
      const int elapsed = delta > 0 ? static_cast<int>(delta) : 0;
      const int remaining = std::max(0, cooldownSeconds - elapsed);
      gi.LocClient_Print(ent, PRINT_HIGH,
                         "Map '{}' was played recently, please wait {}.\n",
                         mapName.data(), FormatDuration(remaining).c_str());
      return false;
    }
  }
  return true;
}

static bool Validate_Forfeit(gentity_t *ent, const CommandArgs &args) {
  if (level.matchState != MatchState::In_Progress &&
      level.matchState != MatchState::Countdown) {
    gi.Client_Print(ent, PRINT_HIGH, "Can only forfeit during a match.\n");
    return false;
  }
  return true;
}

static bool Validate_Gametype(gentity_t *ent, const CommandArgs &args) {
  if (!Game::FromString(args.getString(2))) {
    gi.Client_Print(ent, PRINT_HIGH, "Invalid gametype.\n");
    return false;
  }
  return true;
}

static bool Validate_Ruleset(gentity_t *ent, const CommandArgs &args) {
  Ruleset desired_rs = RS_IndexFromString(args.getString(2).data());
  if (desired_rs == Ruleset::None) {
    gi.Client_Print(ent, PRINT_HIGH, "Invalid ruleset.\n");
    return false;
  }
  if (desired_rs == game.ruleset) {
    gi.Client_Print(ent, PRINT_HIGH, "That ruleset is already active.\n");
    return false;
  }
  return true;
}

static bool Validate_Timelimit(gentity_t *ent, const CommandArgs &args) {
  auto limit = args.getInt(2);
  if (!limit || *limit < 0 || *limit > 1440) {
    gi.Client_Print(ent, PRINT_HIGH, "Invalid time limit value.\n");
    return false;
  }
  if (*limit == timeLimit->integer) {
    gi.LocClient_Print(ent, PRINT_HIGH, "Time limit is already set to {}.\n",
                       *limit);
    return false;
  }
  return true;
}

static bool Validate_Scorelimit(gentity_t *ent, const CommandArgs &args) {
  auto limit = args.getInt(2);
  if (!limit || *limit < 0) {
    gi.Client_Print(ent, PRINT_HIGH, "Invalid score limit value.\n");
    return false;
  }
  if (*limit == GT_ScoreLimit()) {
    gi.LocClient_Print(ent, PRINT_HIGH, "Score limit is already set to {}.\n",
                       *limit);
    return false;
  }
  return true;
}

static bool Validate_TeamBased(gentity_t *ent, const CommandArgs &args) {
  if (!Teams()) {
    gi.Client_Print(ent, PRINT_HIGH,
                    "This vote is only available in team-based gametypes.\n");
    return false;
  }
  return true;
}

static bool Validate_Unlagged(gentity_t *ent, const CommandArgs &args) {
  auto value = args.getInt(2);
  if (!value || (*value != 0 && *value != 1)) {
    gi.Client_Print(ent, PRINT_HIGH, "Usage: callvote unlagged <0|1>.\n");
    return false;
  }

  const bool currentlyEnabled =
      g_lagCompensation && g_lagCompensation->integer != 0;
  if (currentlyEnabled == (*value != 0)) {
    gi.LocClient_Print(ent, PRINT_HIGH, "Lag compensation is already {}.\n",
                       currentlyEnabled ? "ENABLED" : "DISABLED");
    return false;
  }

  return true;
}

static bool Validate_Cointoss(gentity_t *ent, const CommandArgs &args) {
  if (args.count() > 2) {
    gi.Client_Print(ent, PRINT_HIGH,
                    "Cointoss does not take any parameters.\n");
    return false;
  }

  return true;
}

static bool Validate_Random(gentity_t *ent, const CommandArgs &args) {
  auto limit = args.getInt(2);
  if (!limit || *limit < 2 || *limit > 100) {
    gi.Client_Print(ent, PRINT_HIGH,
                    "Random vote range must be between 2 and 100.\n");
    return false;
  }

  return true;
}

static bool Validate_Arena(gentity_t *ent, const CommandArgs &args) {
  if (level.arenaTotal <= 0) {
    gi.Client_Print(ent, PRINT_HIGH,
                    "This vote is only available in arena-based modes.\n");
    return false;
  }

  auto arenaNum = args.getInt(2);
  if (!arenaNum) {
    gi.Client_Print(ent, PRINT_HIGH, "Invalid arena number.\n");
    return false;
  }

  if (!CheckArenaValid(*arenaNum)) {
    gi.LocClient_Print(ent, PRINT_HIGH, "Arena {} is not available.\n",
                       *arenaNum);
    return false;
  }

  if (*arenaNum == level.arenaActive) {
    gi.Client_Print(ent, PRINT_HIGH, "That arena is already active.\n");
    return false;
  }

  return true;
}

static void RegisterAllVoteCommands() {
  s_voteCommands.clear();
  s_voteDefinitions.clear();
  RegisterVoteCommand("map", &Validate_Map, &Pass_Map, kVoteFlag_Map, 2,
                      "<mapname> [flags]", "Changes to the specified map");
  RegisterVoteCommand("nextmap", &Validate_None, &Pass_NextMap,
                      kVoteFlag_NextMap, 1, "",
                      "Moves to the next map in the rotation");
  RegisterVoteCommand("restart", &Validate_None, &Pass_RestartMatch,
                      kVoteFlag_Restart, 1, "", "Restarts the current match");
  RegisterVoteCommand("forfeit", &Validate_Forfeit, &Pass_Forfeit,
                      kVoteFlag_Forfeit, 1, "",
                      "Votes to forfeit and end the match");
  RegisterVoteCommand("gametype", &Validate_Gametype, &Pass_Gametype,
                      kVoteFlag_Gametype, 2, "<gametype>",
                      "Changes the current gametype");
  RegisterVoteCommand("ruleset", &Validate_Ruleset, &Pass_Ruleset,
                      kVoteFlag_Ruleset, 2, "<q1|q2|q3a>",
                      "Changes the current ruleset", true);
  RegisterVoteCommand("timelimit", &Validate_Timelimit, &Pass_Timelimit,
                      kVoteFlag_Timelimit, 2, "<minutes>",
                      "Alters the match time limit (0 for none)");
  RegisterVoteCommand("scorelimit", &Validate_Scorelimit, &Pass_Scorelimit,
                      kVoteFlag_Scorelimit, 2, "<score>",
                      "Alters the match score limit (0 for none)");
  RegisterVoteCommand("shuffle", &Validate_TeamBased, &Pass_ShuffleTeams,
                      kVoteFlag_Shuffle, 1, "",
                      "Shuffles the teams based on skill");
  RegisterVoteCommand("balance", &Validate_TeamBased, &Pass_BalanceTeams,
                      kVoteFlag_Balance, 1, "",
                      "Balances teams without shuffling");
  RegisterVoteCommand("unlagged", &Validate_Unlagged, &Pass_Unlagged,
                      kVoteFlag_Unlagged, 2, "<0|1>",
                      "Toggles lag compensation", true);
  RegisterVoteCommand("cointoss", &Validate_Cointoss, &Pass_Cointoss,
                      kVoteFlag_Cointoss, 1, "",
                      "Flip a coin for a random decision", true);
  RegisterVoteCommand("random", &Validate_Random, &Pass_Random,
                      kVoteFlag_Random, 2, "<max>",
                      "Roll a random number between 1 and <max>", true);
  RegisterVoteCommand("arena", &Validate_Arena, &Pass_Arena, kVoteFlag_Arena, 2,
                      "<number>", "Switches to a different arena", true);
}

static void VoteCommandStore(gentity_t *ent, const VoteCommand *vote_cmd,
                             std::string_view arg,
                             std::string_view displayArg = {}) {
  level.vote.client = ent->client;
  level.vote.time = level.time;
  level.vote.countYes = 1;
  level.vote.countNo = 0;
  level.vote.cmd = vote_cmd;
  level.vote.arg = arg;

  std::string_view effectiveArg = displayArg.empty() ? arg : displayArg;

  std::string argSuffix;
  if (!effectiveArg.empty()) {
    argSuffix = std::format(" {}", effectiveArg);
  }

  gi.LocBroadcast_Print(PRINT_CENTER, "{} called a vote:\n{}{}\n",
                        level.vote.client->sess.netName, vote_cmd->name.data(),
                        argSuffix.empty() ? "" : argSuffix.c_str());

  for (auto ec : active_clients()) {
    ec->client->pers.voted = (ec == ent) ? 1 : 0;
  }

  ent->client->pers.vote_count++;
  AnnouncerSound(world, "vote_now");

  for (auto ec : active_players()) {
    if (ec->svFlags & SVF_BOT)
      continue;
    if (ec == ent)
      continue;
    if (!ClientIsPlaying(ec->client) && !g_allowSpecVote->integer)
      continue;

    CloseActiveMenu(ec);
    OpenVoteMenu(ec);
  }
}

std::span<const VoteDefinitionView> GetRegisteredVoteDefinitions() {
  return s_voteDefinitions;
}

VoteLaunchResult TryLaunchVote(gentity_t *ent, std::string_view voteName,
                               std::string_view voteArg) {
  VoteLaunchResult result;
  std::string validationError;

  if (!g_allowVoting || !g_allowVoting->integer) {
    result.message = "Voting is disabled on this server.";
    return result;
  }
  if (level.vote.time) {
    result.message = "A vote is already in progress.";
    return result;
  }
  if (level.vote.executeTime || level.restarted) {
    result.message = "Cannot start a vote right now.";
    return result;
  }
  if (!g_allowVoteMidGame->integer &&
      level.matchState >= MatchState::Countdown) {
    result.message = "Voting is only allowed during warmup.";
    return result;
  }
  if (g_vote_limit->integer &&
      ent->client->pers.vote_count >= g_vote_limit->integer) {
    result.message =
        std::format("You have called the maximum number of votes ({}).",
                    g_vote_limit->integer);
    return result;
  }
  if (!ClientIsPlaying(ent->client) && !g_allowSpecVote->integer) {
    result.message = "Spectators cannot call a vote on this server.";
    return result;
  }

  if (!ValidatePrintableASCII(voteName, "Vote command", validationError)) {
    result.message = validationError;
    return result;
  }
  if (!voteArg.empty() &&
      !ValidatePrintableASCII(voteArg, "Vote argument", validationError)) {
    result.message = validationError;
    return result;
  }

  auto it = s_voteCommands.find(voteName);
  if (it == s_voteCommands.end()) {
    result.message = std::format("Invalid vote command: '{}'.", voteName);
    return result;
  }

  const VoteCommand &found_cmd = it->second;
  if ((g_vote_flags->integer & found_cmd.flag) == 0) {
    result.message = "That vote type is disabled on this server.";
    return result;
  }

  level.vote_flags_enable = 0;
  level.vote_flags_disable = 0;

  std::vector<std::string> args;
  args.emplace_back("callvote");
  args.emplace_back(voteName);

  std::vector<std::string> splitTokens;
  std::string voteArgStr;
  if (!voteArg.empty()) {
    voteArgStr = std::string(voteArg);

    std::istringstream splitter(voteArgStr);
    std::string token;
    while (splitter >> token) {
      splitTokens.emplace_back(token);
      args.emplace_back(splitTokens.back());
    }
  }

  CommandArgs manualArgs(std::move(args));
  if (manualArgs.count() < (1 + found_cmd.minArgs)) {
    result.message = "Not enough parameters supplied for that vote.";
    return result;
  }

  if (!found_cmd.validate || !found_cmd.validate(ent, manualArgs)) {
    return result;
  }

  std::string displayArg = voteArgStr;
  std::string_view storedArg;
  std::string mapStoredArg;

  if (found_cmd.name == "map") {
    std::string parseError;
    auto parsed = ParseMapVoteArguments(splitTokens, parseError);
    if (!parsed) {
      result.message = parseError.empty()
                           ? "Unable to parse map vote arguments."
                           : parseError;
      return result;
    }

    level.vote_flags_enable = parsed->enableFlags;
    level.vote_flags_disable = parsed->disableFlags;

    mapStoredArg = parsed->mapName;

    storedArg = mapStoredArg;
    displayArg = parsed->displayArg;
  } else if (manualArgs.count() >= 3) {
    storedArg = manualArgs.getString(2);
  }

  VoteCommandStore(ent, &found_cmd, storedArg, displayArg);
  result.success = true;
  return result;
}

// --- Main Command Functions ---

void CallVote(gentity_t *ent, const CommandArgs &args) {
  if (!g_allowVoting->integer) {
    gi.Client_Print(ent, PRINT_HIGH, "Voting is disabled on this server.\n");
    return;
  }
  if (Tournament_IsActive()) {
    gi.Client_Print(ent, PRINT_HIGH,
                    "Voting is disabled during tournaments.\n");
    return;
  }
  if (level.vote.time) {
    gi.Client_Print(ent, PRINT_HIGH, "A vote is already in progress.\n");
    return;
  }
  if (!g_allowVoteMidGame->integer &&
      level.matchState >= MatchState::Countdown) {
    gi.Client_Print(ent, PRINT_HIGH, "Voting is only allowed during warmup.\n");
    return;
  }
  if (g_vote_limit->integer &&
      ent->client->pers.vote_count >= g_vote_limit->integer) {
    gi.LocClient_Print(ent, PRINT_HIGH,
                       "You have called the maximum number of votes ({}).\n",
                       g_vote_limit->integer);
    return;
  }
  if (!ClientIsPlaying(ent->client) && !g_allowSpecVote->integer) {
    gi.Client_Print(ent, PRINT_HIGH,
                    "Spectators cannot call a vote on this server.\n");
    return;
  }
  if (level.vote.time) {
    gi.Client_Print(ent, PRINT_HIGH, "A vote is already in progress.\n");
    return;
  }
  if (level.vote.executeTime || level.restarted) {
    gi.Client_Print(ent, PRINT_HIGH, "Cannot start a vote right now.\n");
    return;
  }

  if (args.count() < 2) {
    PrintUsage(ent, args, "<command>", "[params]",
               "Call a vote to change a server setting.");

    std::vector<const VoteCommand *> enabledVotes;
    enabledVotes.reserve(s_voteDefinitions.size());
    const int32_t voteFlags = g_vote_flags->integer;

    for (const auto &definition : s_voteDefinitions) {
      if ((voteFlags & definition.flag) == 0) {
        continue;
      }

      auto itCommand = s_voteCommands.find(definition.name);
      if (itCommand == s_voteCommands.end()) {
        continue;
      }

      enabledVotes.push_back(&itCommand->second);
    }

    if (!enabledVotes.empty()) {
      std::ostringstream oss;
      oss << "Available votes:\n";

      for (const VoteCommand *command : enabledVotes) {
        oss << "  " << command->name;
        if (!command->argsUsage.empty()) {
          oss << ' ' << command->argsUsage;
        }

        if (!command->helpText.empty()) {
          oss << " - " << command->helpText;
        }

        oss << '\n';
      }

      const std::string message = oss.str();
      gi.Client_Print(ent, PRINT_HIGH, message.c_str());
    } else {
      gi.Client_Print(ent, PRINT_HIGH, "No votes are currently enabled.\n");
    }

    return;
  }

  std::string_view voteName = args.getString(1);
  std::string validationError;
  if (!ValidatePrintableASCII(voteName, "Vote command", validationError)) {
    std::string message = validationError;
    message.push_back('\n');
    gi.Client_Print(ent, PRINT_HIGH, message.c_str());
    return;
  }

  for (int i = 2; i < args.count(); ++i) {
    if (!ValidatePrintableASCII(args.getString(i), "Vote argument",
                                validationError)) {
      std::string message = validationError;
      message.push_back('\n');
      gi.Client_Print(ent, PRINT_HIGH, message.c_str());
      return;
    }
  }

  auto it = s_voteCommands.find(voteName);

  if (it == s_voteCommands.end()) {
    gi.LocClient_Print(ent, PRINT_HIGH, "Invalid vote command: '{}'.\n",
                       voteName.data());
    return;
  }
  const VoteCommand &found_cmd = it->second;

  if (args.count() < (1 + found_cmd.minArgs)) {
    PrintUsage(ent, args, found_cmd.name, found_cmd.argsUsage,
               found_cmd.helpText);
    return;
  }

  if (found_cmd.validate(ent, args)) {

    if ((g_vote_flags->integer & found_cmd.flag) == 0) {
      gi.Client_Print(ent, PRINT_HIGH,
                      "That vote type is disabled on this server.\n");
      return;
    }

    level.vote_flags_enable = 0;
    level.vote_flags_disable = 0;

    std::string vote_arg_str;
    std::string vote_display_str;

    if (found_cmd.name == "map") {
      std::vector<std::string> mapArgs;
      for (int i = 2; i < args.count(); ++i) {
        mapArgs.emplace_back(args.getString(i));
      }

      std::string parseError;
      auto parsed = ParseMapVoteArguments(mapArgs, parseError);
      if (!parsed) {
        gi.LocClient_Print(ent, PRINT_HIGH, "{}\n", parseError.c_str());
        return;
      }

      vote_arg_str = parsed->mapName;
      vote_display_str = parsed->displayArg;
      level.vote_flags_enable = parsed->enableFlags;
      level.vote_flags_disable = parsed->disableFlags;
    } else {
      if (args.count() >= 3) {
        vote_arg_str = args.getString(2);
      }
    }

    VoteCommandStore(ent, &found_cmd, vote_arg_str, vote_display_str);
  }
}

#include "command_voting_vote.inl"

// --- Registration Function ---
void RegisterVotingCommands() {
  RegisterAllVoteCommands();
  using enum CommandFlag;
  RegisterCommand("callvote", &CallVote, AllowDead | AllowSpectator);
  RegisterCommand("cv", &CallVote, AllowDead | AllowSpectator); // Alias
  RegisterCommand("vote", &Vote, AllowDead);
}

} // namespace Commands

void G_RevertVote(gclient_t *client) {
  if (!client) {
    return;
  }

  if (!level.vote.time || !level.vote.client) {
    client->pers.voted = 0;
    return;
  }

  if (client->pers.voted > 0) {
    int yesVotes = std::max(0, static_cast<int>(level.vote.countYes) - 1);
    level.vote.countYes = static_cast<int8_t>(yesVotes);
  } else if (client->pers.voted < 0) {
    int noVotes = std::max(0, static_cast<int>(level.vote.countNo) - 1);
    level.vote.countNo = static_cast<int8_t>(noVotes);
  }

  client->pers.voted = 0;

  if (level.vote.client != client) {
    return;
  }

  gi.Broadcast_Print(PRINT_HIGH, "Vote cancelled (caller disconnected).\n");

  level.vote.client = nullptr;
  level.vote.cmd = nullptr;
  level.vote.arg.clear();
  level.vote.time = 0_sec;
  level.vote.executeTime = 0_sec;
  level.vote.countYes = 0;
  level.vote.countNo = 0;
  level.vote_flags_enable = 0;
  level.vote_flags_disable = 0;

  for (auto ec : active_clients()) {
    if (ec->client) {
      ec->client->pers.voted = 0;
    }
  }
}

void Vote_Passed() {
  const VoteCommand *command = level.vote.cmd;
  if (!command) {
    gi.Com_Print("Vote_Passed called without an active command.\n");
  } else if (command->execute) {
    command->execute();
  }

  for (auto ent : active_clients()) {
    if (ent->client) {
      ent->client->pers.voted = 0;
    }
  }

  level.vote.cmd = nullptr;
  level.vote.client = nullptr;
  level.vote.arg.clear();
  level.vote.countYes = 0;
  level.vote.countNo = 0;
  level.vote.time = 0_sec;
  level.vote.executeTime = 0_sec;
  level.vote_flags_enable = 0;
  level.vote_flags_disable = 0;
}
