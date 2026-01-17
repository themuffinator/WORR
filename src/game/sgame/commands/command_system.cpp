/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

command_system.cpp - Core implementation of the command dispatcher. This file contains the
command map, the main dispatcher logic, and the top-level registration function that calls into
each command module.*/

#include "command_system.hpp"
#include "command_registration.hpp"
#include <algorithm>
#include <numeric>
#include <ranges>
#include <string>
#include <unordered_map>
#include <vector>

// The central registry for all client commands.
static std::unordered_map<std::string, Command, StringViewHash, std::equal_to<>> s_clientCommands;

namespace {
	static inline char AsciiToLower(char c) {
		if (c >= 'A' && c <= 'Z') {
			return static_cast<char>(c - 'A' + 'a');
		}
		return c;
	}

	static std::string NormalizeCommandKey(std::string_view name) {
		std::string key(name);
		for (char& ch : key) {
			ch = AsciiToLower(ch);
		}
		return key;
	}
}

// Definition of the global registration function.
void RegisterCommand(std::string_view name, void (*function)(gentity_t*, const CommandArgs&), BitFlags<CommandFlag> flags, bool floodExempt) {
	s_clientCommands[NormalizeCommandKey(name)] = { function, flags, floodExempt };
}

// Forward declarations for the registration functions in other files.
namespace Commands {
	void RegisterAdminCommands();
	void RegisterClientCommands();
	void RegisterVotingCommands();
	void RegisterCheatCommands();

	void PrintUsage(gentity_t* ent, const CommandArgs& args, std::string_view required_params, std::string_view optional_params, std::string_view help_text) {
		std::string usage = std::format("Usage: {} {}", args.getString(0), required_params);
		if (!optional_params.empty()) {
			usage += std::format(" {}", optional_params);
		}
		if (!help_text.empty()) {
			usage += std::format("\n{}", help_text);
		}
		gi.Client_Print(ent, PRINT_HIGH, usage.c_str());
	}

	// --- Helper Function Definitions ---

		/*
		=============
		ValidateSocialIDFormat

		Validates the structure and characters of a social ID.
		=============
		*/
		static bool ValidateSocialIDFormat(std::string_view id) {
			size_t sep = id.find(':');
			if (sep == std::string_view::npos || sep == 0 || sep + 1 >= id.length())
					return false;

			std::string_view prefix = id.substr(0, sep);
			std::string_view value = id.substr(sep + 1);

			auto isDigit = [](char c) {
				return std::isdigit(static_cast<unsigned char>(c));
			};

			auto isHexDigit = [](char c) {
				return std::isxdigit(static_cast<unsigned char>(c));
			};

			if (prefix == "EOS") {
				return value.length() == 32 && std::ranges::all_of(value, isHexDigit);
			}
			if (prefix == "Galaxy" || prefix == "NX") {
				return (value.length() >= 17 && value.length() <= 20) && std::ranges::all_of(value, isDigit);
			}
			if (prefix == "GDK") {
				return (value.length() >= 15 && value.length() <= 17) && std::ranges::all_of(value, isDigit);
			}
			if (prefix == "PSN") {
				return !value.empty() && std::ranges::all_of(value, isDigit);
			}
			if (prefix == "Steamworks") {
				return value.starts_with("7656119") && std::ranges::all_of(value, isDigit);
			}
			return false;
		}

		/*
		=============
		ResolveSocialID

		Resolves a social ID argument to a client or returns the validated raw value.
		=============
		*/
		const char* ResolveSocialID(const char* rawArg, gentity_t*& foundClient) {
			std::string_view arg(rawArg);
			foundClient = ClientEntFromString(rawArg);
			if (foundClient && foundClient->client) {
					return foundClient->client->sess.socialID;
			}

			if (ValidateSocialIDFormat(arg)) {
					return rawArg;
			}

			foundClient = nullptr;
			return nullptr;
		}



	/*
	=============
	CollectEligibleClientIndices

	Collects client indices for active players within the valid client range.
	=============
	*/
	static std::vector<int> CollectEligibleClientIndices() {
		std::vector<int> clientIndices;
		for (auto p_ent : active_players()) {
			const int clientIndex = p_ent->s.number - 1;
			if (clientIndex < 0 || clientIndex >= static_cast<int>(game.maxClients)) {
				continue;
			}

			clientIndices.push_back(clientIndex);
		}
		return clientIndices;
	}

	/*
	=============
	ComputeSkillTotal

	Calculates the combined skill rating for the provided client indices.
	=============
	*/
	static int ComputeSkillTotal(const std::vector<int>& clientIndices) {
		return std::accumulate(clientIndices.begin(), clientIndices.end(), 0, [](int total, int clientNum) {
			return total + game.clients[clientNum].sess.skillRating;
		});
	}


	static constexpr GameTime kTeamShuffleGraceDuration = 3_sec;

	/*
	=============
	BroadcastShuffleAnnouncement

	Sends consistent broadcast and center notifications for skill-based team
	shuffles so admins, votes, and automated triggers share the same phrasing.
	=============
	*/
	static void BroadcastShuffleAnnouncement(bool matchInProgress, GameTime graceDuration) {
		const char* phaseLabel = matchInProgress ? "mid-match" : "pre-game";
		const char* actionLabel = matchInProgress ? "rebalance" : "shuffle";
		const int32_t graceSeconds = std::max(graceDuration.seconds<int32_t>(), 0);

		gi.LocBroadcast_Print(PRINT_HIGH, "[SHUFFLE]: Skill-based {} ({}) - respawning in {}s.\n", actionLabel, phaseLabel, graceSeconds);
		gi.LocBroadcast_Print(PRINT_CENTER, ".Skill-based {} ({})\n.Respawning in {} seconds...\n", actionLabel, phaseLabel, graceSeconds);
	}

	/*
	=============
	ApplyShuffleGrace

	Respawns active players onto their new teams and holds movement briefly so
	they can adjust loadouts before play resumes.
	=============
	*/
	static void ApplyShuffleGrace(GameTime graceDuration, bool matchInProgress) {
		const GameTime resumeTime = level.time + graceDuration;
		const uint16_t holdMs = static_cast<uint16_t>(std::clamp<int64_t>(graceDuration.milliseconds(), 0, std::numeric_limits<uint16_t>::max()));
		const bool shuffleRedRover = Game::Is(GameType::RedRover) && matchInProgress;

		for (auto ec : active_clients()) {
			if (!ec->client || !ClientIsPlaying(ec->client))
				continue;

			const Team originalTeam = ec->client->sess.team;
			ClientRespawn(ec);
			if (shuffleRedRover) {
				ec->client->sess.team = originalTeam;
				ec->client->ps.teamID = static_cast<int>(originalTeam);
				AssignPlayerSkin(ec, ec->client->sess.skinName);
			}

			ec->client->respawnMinTime = std::max(ec->client->respawnMinTime, resumeTime);
			ec->client->respawnMaxTime = std::max(ec->client->respawnMaxTime, resumeTime);
			ec->client->respawn_timeout = std::max(ec->client->respawn_timeout, resumeTime);

			ec->client->ps.pmove.pmFlags |= PMF_TIME_KNOCKBACK;
			ec->client->ps.pmove.pmTime = std::max<uint16_t>(ec->client->ps.pmove.pmTime, holdMs);
			ec->client->latchedButtons = BUTTON_NONE;
			ec->client->buttons = BUTTON_NONE;
			ec->client->oldButtons = BUTTON_NONE;
		}
	}

		/*
		=============
		ToggleTeam
		=============
		*/
		static Team ToggleTeam(Team currentTeam) {
			return (currentTeam == Team::Red) ? Team::Blue : Team::Red;
		}

	/*
	=============
	ApplyTeamShuffleAssignments

	Applies shuffled team targets for each client while respecting the current match context.
	=============
	*/
	static void ApplyTeamShuffleAssignments(const std::vector<std::pair<int, Team>>& assignments, bool matchInProgress) {
		for (const auto& [clientNum, targetTeam] : assignments) {
			if (clientNum < 0 || clientNum >= static_cast<int>(game.maxClients))
				continue;

			gentity_t* ent = &g_entities[clientNum + 1];
			if (!ent->inUse || !ent->client)
				continue;

			if (ent->client->sess.team == targetTeam)
				continue;

			SetTeam(ent, targetTeam, false, true, true);

			if (!matchInProgress)
				ent->client->pers.readyStatus = false;
		}
	}

	/*
	=============
	TeamSkillShuffle

	Shuffles active players between teams based on descending skill rating.
	=============
	*/
	bool TeamSkillShuffle() {
		if (!Teams()) return false;

		auto playerIndices = CollectEligibleClientIndices();
		const int totalSkill = ComputeSkillTotal(playerIndices);
		(void)totalSkill;

		if (playerIndices.size() < 2) return false;

		std::ranges::sort(playerIndices, std::greater{}, [](int clientNum) {
			return game.clients[clientNum].sess.skillRating;
		});

		std::vector<std::pair<int, Team>> assignments;
		assignments.reserve(playerIndices.size());

		Team currentTeam = Team::Red;
		for (const int clientNum : playerIndices) {
			assignments.emplace_back(clientNum, currentTeam);
			currentTeam = ToggleTeam(currentTeam);
		}

		const bool matchInProgress = level.matchState == MatchState::In_Progress;
		ApplyTeamShuffleAssignments(assignments, matchInProgress);

		CalculateRanks();
		ApplyShuffleGrace(kTeamShuffleGraceDuration, matchInProgress);
		BroadcastShuffleAnnouncement(matchInProgress, kTeamShuffleGraceDuration);
		return true;
	}

}

bool CheckFlood(gentity_t* ent) {
	if (!flood_msgs->integer)
		return false;
	gclient_t* cl = ent->client;
	if (level.time < cl->flood.lockUntil) {
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_flood_cant_talk",
			(cl->flood.lockUntil - level.time).seconds<int32_t>());
		return true;
	}
	const size_t maxMsgs = static_cast<size_t>(flood_msgs->integer);
	const size_t bufferSize = std::size(cl->flood.messageTimes);
	size_t i = (static_cast<size_t>(cl->flood.time) + bufferSize - maxMsgs + 1) % bufferSize;
	if (cl->flood.messageTimes[i] && (level.time - cl->flood.messageTimes[i] < GameTime::from_sec(flood_persecond->value))) {
		cl->flood.lockUntil = level.time + GameTime::from_sec(flood_waitdelay->value);
		gi.LocClient_Print(ent, PRINT_CHAT, "$g_flood_cant_talk", flood_waitdelay->integer);
		return true;
	}
	cl->flood.time = static_cast<int>(
		(static_cast<size_t>(cl->flood.time) + 1) % bufferSize
		);
	cl->flood.messageTimes[cl->flood.time] = level.time;
	return false;
}

// Main registration function that orchestrates all others.
void RegisterAllCommands() {
	s_clientCommands.clear();
	Commands::RegisterAdminCommands();
	Commands::RegisterClientCommands();
	Commands::RegisterVotingCommands();
	Commands::RegisterCheatCommands();
}

// --- Permission Check Helpers ---
bool CheatsOk(gentity_t* ent) {
	if (!deathmatch->integer && !coop->integer) return true;
	if (!g_cheats->integer) {
		gi.Client_Print(ent, PRINT_HIGH, "Cheats must be enabled to use this command.\n");
		return false;
	}
	return true;
}

static inline bool AdminOk(gentity_t* ent) {
	if (!g_allowAdmin->integer || !ent->client->sess.admin) {
		gi.Client_Print(ent, PRINT_HIGH, "Only admins can use this command.\n");
		return false;
	}
	return true;
}


/*
=================
HasCommandPermission

Verifies that the client is allowed to execute the command based on
the current game state and command flags.
=================
*/
static inline bool HasCommandPermission(gentity_t* ent, const Command& cmd) {
	using enum CommandFlag;
	if (cmd.flags.has(AdminOnly) && !AdminOk(ent)) return false;
	if (cmd.flags.has(CheatProtect) && !CheatsOk(ent)) return false;
	if (!cmd.flags.has(AllowDead) && (ent->health <= 0 || ent->deadFlag)) return false;
	if (!cmd.flags.has(AllowSpectator) && !ClientIsPlaying(ent->client)) return false;
	if (cmd.flags.has(MatchOnly) && !InAMatch()) return false;
	if (!cmd.flags.has(AllowIntermission) && (level.intermission.time || level.intermission.postIntermissionTime)) return false;
	return true;
}

/*
=================
ClientCommand

Central command entrypoint. Looks up the command in the
registry, applies flood/permission checks, and executes it.
Falls back to dynamic cvar force set for replace_* / disable_*.
=================
*/
void ClientCommand(gentity_t* ent) {
	if (!ent || !ent->client) {
		return; // not fully in game yet
	}

	CommandArgs args;
	std::string_view commandName = args.getString(0);
	if (commandName.empty()) {
		return;
	}

	// Fast lookup in the heterogeneous s_clientCommands map
	auto it = s_clientCommands.find(NormalizeCommandKey(commandName));
	if (it == s_clientCommands.end()) {
		// Dynamic cvar fallback (parity with legacy behavior)
		// Example: "replace_gun 0" or "disable_powerups 1"
		if (args.count() > 1) {
			const char* raw = gi.argv(0);
			if (raw && (strstr(raw, "replace_") || strstr(raw, "disable_"))) {
				gi.cvarForceSet(raw, gi.argv(1));
				return;
			}
		}
		gi.LocClient_Print(ent, PRINT_HIGH, "Unknown command: '{}'\n", commandName.data());
		return;
	}

	const Command& cmd = it->second;

	// Optional per-command flood check (keeps your existing behavior)
	// Uses your existing CheckFlood(ent) gate and the command's floodExempt flag.
	// If you later decide to use a separate command-flood window, wire it here.
	if (!cmd.floodExempt && CheckFlood(ent)) {
		return;
	}

	// Permission gates: admin/cheat/intermission/spectator/dead/match-only
	if (!HasCommandPermission(ent, cmd)) {
		return;
	}

	// Execute
	cmd.function(ent, args);
}
