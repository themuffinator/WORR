/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

commands_admin.cpp - Implementations for admin-level commands.*/

#include "../g_local.hpp"
#include "../gameplay/client_config.hpp"
#include "command_registration.hpp"
#include <string>
#include <format>
#include <vector>
#include <algorithm>
#include <ranges>

namespace Commands {

	// --- Forward Declarations for Admin Functions ---
	void AddAdmin(gentity_t* ent, const CommandArgs& args);
	void AddBan(gentity_t* ent, const CommandArgs& args);
	void BalanceTeams(gentity_t* ent, const CommandArgs& args);
	void Boot(gentity_t* ent, const CommandArgs& args);
	void EndMatch(gentity_t* ent, const CommandArgs& args);
	void ForceVote(gentity_t* ent, const CommandArgs& args);
	void Gametype(gentity_t* ent, const CommandArgs& args);
	void LoadAdmins(gentity_t* ent, const CommandArgs& args);
	void LoadBans(gentity_t* ent, const CommandArgs& args);
	void LoadMapCycle(gentity_t* ent, const CommandArgs& args);
	void LoadMapPool(gentity_t* ent, const CommandArgs& args);
	void LoadMotd(gentity_t* ent, const CommandArgs& args);
	void LockTeam(gentity_t* ent, const CommandArgs& args);
	void MapRestart(gentity_t* ent, const CommandArgs& args);
	void NextMap(gentity_t* ent, const CommandArgs& args);
	void ReadyAll(gentity_t* ent, const CommandArgs& args);
	void RemoveAdmin(gentity_t* ent, const CommandArgs& args);
	void RemoveBan(gentity_t* ent, const CommandArgs& args);
	void ReplayGame(gentity_t* ent, const CommandArgs& args);
	void ResetMatch(gentity_t* ent, const CommandArgs& args);
	void Ruleset(gentity_t* ent, const CommandArgs& args);
	void SetMap(gentity_t* ent, const CommandArgs& args);
	void SetTeam(gentity_t* ent, const CommandArgs& args);
	void Shuffle(gentity_t* ent, const CommandArgs& args);
	void StartMatch(gentity_t* ent, const CommandArgs& args);
	void UnlockTeam(gentity_t* ent, const CommandArgs& args);
	void UnReadyAll(gentity_t* ent, const CommandArgs& args);
	void ForceArena(gentity_t* ent, const CommandArgs& args);

	// --- Admin Command Implementations ---

	void AddAdmin(gentity_t* ent, const CommandArgs& args) {
		if (args.count() != 2) {
			PrintUsage(ent, args, "<client# | name | social_id>", "", "Adds a player to the admin list.");
			return;
		}

		std::string_view input = args.getString(1);
		gentity_t* target = nullptr;
		const char* resolvedID = ResolveSocialID(input.data(), target);

		if (!resolvedID || !*resolvedID) {
			gi.Client_Print(ent, PRINT_HIGH, "Invalid or unresolved social ID.\n");
			return;
		}

		if (AppendIDToFile("admin.txt", resolvedID)) {
			LoadAdminList();
			std::string playerName = GetClientConfigStore().PlayerNameForSocialID(resolvedID);
			if (!playerName.empty()) {
				gi.LocBroadcast_Print(PRINT_CHAT, "{} has been granted admin rights.\n", playerName.c_str());
			}
			gi.LocClient_Print(ent, PRINT_HIGH, "Admin added: {}\n", resolvedID);
		}
		else {
			gi.Client_Print(ent, PRINT_HIGH, "Failed to write to admin.txt\n");
		}
	}

	void AddBan(gentity_t* ent, const CommandArgs& args) {
		if (args.count() != 2) {
			PrintUsage(ent, args, "<client# | name | social_id>", "", "Adds a player to the ban list.");
			return;
		}

		std::string_view input = args.getString(1);
		gentity_t* target = nullptr;
		const char* resolvedID = ResolveSocialID(input.data(), target);

		if (!resolvedID || !*resolvedID) {
			gi.Client_Print(ent, PRINT_HIGH, "Invalid or unresolved social ID.\n");
			return;
		}

		if (game.adminIDs.contains(resolvedID)) {
			gi.Client_Print(ent, PRINT_HIGH, "Cannot ban: target is a listed admin.\n");
			return;
		}

		if (host && host->client && Q_strcasecmp(resolvedID, host->client->sess.socialID) == 0) {
			gi.Client_Print(ent, PRINT_HIGH, "Cannot ban the host.\n");
			return;
		}

		if (AppendIDToFile("ban.txt", resolvedID)) {
			LoadBanList();
			gi.LocClient_Print(ent, PRINT_HIGH, "Ban added: {}\n", resolvedID);
		}
		else {
			gi.Client_Print(ent, PRINT_HIGH, "Failed to write to ban.txt\n");
		}
	}

	void ForceArena(gentity_t* ent, const CommandArgs& args) {
		if (!level.arenaTotal) {
			gi.Client_Print(ent, PRINT_HIGH, "No arenas present in current map.\n");
			return;
		}

		if (args.count() < 2 || args.getString(1) == "?") {
			gi.LocClient_Print(ent, PRINT_HIGH, "Active arena is: {}\nTotal arenas: {}\n", level.arenaActive, level.arenaTotal);
			return;
		}

		auto arenaNum = args.getInt(1);
		if (!arenaNum) {
			gi.LocClient_Print(ent, PRINT_HIGH, "Invalid number: {}\n", args.getString(1).data());
			return;
		}

		if (*arenaNum == level.arenaActive) {
			gi.LocClient_Print(ent, PRINT_HIGH, "Arena {} is already active.\n", *arenaNum);
			return;
		}

		if (!CheckArenaValid(*arenaNum)) {
			gi.LocClient_Print(ent, PRINT_HIGH, "Invalid arena number: {}\n", *arenaNum);
			return;
		}

		if (!ChangeArena(*arenaNum)) {
			gi.Client_Print(ent, PRINT_HIGH, "Failed to change arena.\n");
			return;
		}

		gi.LocBroadcast_Print(PRINT_HIGH, "[ADMIN]: Forced active arena to {}.\n", level.arenaActive);
	}

	void BalanceTeams(gentity_t* ent, const CommandArgs& args) {
		gi.Broadcast_Print(PRINT_HIGH, "[ADMIN]: Forced team balancing.\n");
		TeamBalance(true);
	}

	void Boot(gentity_t* ent, const CommandArgs& args) {
		if (args.count() < 2 || args.getString(1) == "?") {
			PrintUsage(ent, args, "<client name/number>", "", "Removes the specified client from the server.");
			return;
		}

		gentity_t* targ = ClientEntFromString(args.getString(1).data());
		if (!targ) {
			gi.Client_Print(ent, PRINT_HIGH, "Invalid client specified.\n");
			return;
		}
		if (targ == host) {
			gi.Client_Print(ent, PRINT_HIGH, "You cannot kick the lobby owner.\n");
			return;
		}
		if (targ->client && targ->client->sess.admin) {
			gi.Client_Print(ent, PRINT_HIGH, "You cannot kick an admin.\n");
			return;
		}

		std::string kickCmd = std::format("kick {}\n", targ->s.number - 1);
		gi.AddCommandString(kickCmd.c_str());
	}

	void EndMatch(gentity_t* ent, const CommandArgs& args) {
		if (level.matchState < MatchState::In_Progress) {
			gi.Client_Print(ent, PRINT_HIGH, "Match has not yet begun.\n");
			return;
		}
		if (level.intermission.time) {
			gi.Client_Print(ent, PRINT_HIGH, "Match has already ended.\n");
			return;
		}
		QueueIntermission("[ADMIN]: Forced match end.", true, false);
	}

	void ForceVote(gentity_t* ent, const CommandArgs& args) {
		if (!level.vote.time) {
			gi.Client_Print(ent, PRINT_HIGH, "No vote in progress.\n");
			return;
		}

		if (args.count() < 2) {
			PrintUsage(ent, args, "<yes|no>", "", "Forces the outcome of a current vote.");
			return;
		}

		std::string_view arg = args.getString(1);
		if (arg.starts_with('y') || arg.starts_with('Y') || arg == "1") {
			gi.Broadcast_Print(PRINT_HIGH, "[ADMIN]: Passed the vote.\n");
			level.vote.executeTime = level.time + 3_sec;
			level.vote.client = nullptr;
		}
		else {
			gi.Broadcast_Print(PRINT_HIGH, "[ADMIN]: Failed the vote.\n");
			level.vote.time = 0_sec;
			level.vote.client = nullptr;
		}
	}

	void Gametype(gentity_t* ent, const CommandArgs& args) {
		if (!deathmatch->integer) return;

		if (args.count() < 2 || args.getString(1) == "?") {
			std::string usage = std::format("Changes the current gametype. Current is {}.\nValid gametypes: {}",
				Game::GetCurrentInfo().long_name.data(),
				GametypeOptionList());
			PrintUsage(ent, args, "<gametype>", "", usage);
			return;
		}

		auto gt = Game::FromString(args.getString(1));
		if (!gt) {
			gi.Client_Print(ent, PRINT_HIGH, "Invalid gametype.\n");
			return;
		}

		ChangeGametype(*gt);
	}

	void LoadAdmins(gentity_t* ent, const CommandArgs& args) {
		LoadAdminList();
		gi.Client_Print(ent, PRINT_HIGH, "Admin list reloaded.\n");
	}

	void LoadBans(gentity_t* ent, const CommandArgs& args) {
		LoadBanList();
		gi.Client_Print(ent, PRINT_HIGH, "Ban list reloaded.\n");
	}

	void LoadMotd(gentity_t* ent, const CommandArgs& args) {
		::LoadMotd();
		gi.Client_Print(ent, PRINT_HIGH, "MOTD reloaded.\n");
	}

	void LoadMapPool(gentity_t* ent, const CommandArgs& args) {
		::LoadMapPool(ent);
		::LoadMapCycle(ent);
		gi.Client_Print(ent, PRINT_HIGH, "Map pool and cycle reloaded.\n");
	}

	void LoadMapCycle(gentity_t* ent, const CommandArgs& args) {
		::LoadMapCycle(ent);
		gi.Client_Print(ent, PRINT_HIGH, "Map cycle reloaded.\n");
	}

	void LockTeam(gentity_t* ent, const CommandArgs& args) {
		if (args.count() < 2 || args.getString(1) == "?") {
			PrintUsage(ent, args, "<red|blue>", "", "Locks a team, preventing players from joining.");
			return;
		}

		Team team = StringToTeamNum(args.getString(1).data());
		if (team != Team::Red && team != Team::Blue) {
			gi.Client_Print(ent, PRINT_HIGH, "Invalid team specified.\n");
			return;
		}

		auto team_idx = static_cast<size_t>(team);
		if (level.locked[team_idx]) {
			gi.LocClient_Print(ent, PRINT_HIGH, "{} is already locked.\n", Teams_TeamName(team));
		}
		else {
			level.locked[team_idx] = true;
			gi.LocBroadcast_Print(PRINT_HIGH, "[ADMIN]: {} has been locked.\n", Teams_TeamName(team));
		}
	}

	void MapRestart(gentity_t* ent, const CommandArgs& args) {
		gi.Broadcast_Print(PRINT_HIGH, "[ADMIN]: Session reset.\n");
		std::string command = std::format("gamemap {}\n", level.mapName.data());
		gi.AddCommandString(command.c_str());
	}

	void NextMap(gentity_t* ent, const CommandArgs& args) {
		gi.Broadcast_Print(PRINT_HIGH, "[ADMIN]: Changing to next map.\n");
		Match_End();
	}

	void ReadyAll(gentity_t* ent, const CommandArgs& args) {
		if (!ReadyConditions(ent, true)) {
			return;
		}
		::ReadyAll();
		gi.Broadcast_Print(PRINT_HIGH, "[ADMIN]: Forced all players to ready status.\n");
	}

	void RemoveAdmin(gentity_t* ent, const CommandArgs& args) {
		if (args.count() != 2) {
			PrintUsage(ent, args, "<client# | name | social_id>", "", "Removes a player from the admin list.");
			return;
		}

		std::string_view input = args.getString(1);
		gentity_t* target = nullptr;
		const char* resolvedID = ResolveSocialID(input.data(), target);

		if (!resolvedID || !*resolvedID) {
			gi.Client_Print(ent, PRINT_HIGH, "Invalid or unresolved social ID.\n");
			return;
		}

		if (host && host->client && Q_strcasecmp(resolvedID, host->client->sess.socialID) == 0) {
			gi.Client_Print(ent, PRINT_HIGH, "Cannot remove admin rights from the host.\n");
			return;
		}

		if (RemoveIDFromFile("admin.txt", resolvedID)) {
			LoadAdminList();
			std::string playerName = GetClientConfigStore().PlayerNameForSocialID(resolvedID);
			if (!playerName.empty()) {
				gi.LocBroadcast_Print(PRINT_CHAT, "{} has lost admin rights.\n", playerName.c_str());
			}
			gi.LocClient_Print(ent, PRINT_HIGH, "Admin removed: {}\n", resolvedID);
		}
		else {
			gi.Client_Print(ent, PRINT_HIGH, "Failed to remove from admin.txt or admin not found.\n");
		}
	}

	void RemoveBan(gentity_t* ent, const CommandArgs& args) {
		if (args.count() != 2) {
			PrintUsage(ent, args, "<social_id>", "", "Removes a player from the ban list.");
			return;
		}

		std::string_view id_to_remove = args.getString(1);

		if (RemoveIDFromFile("ban.txt", id_to_remove.data())) {
			LoadBanList();
			gi.LocClient_Print(ent, PRINT_HIGH, "Ban removed: {}\n", id_to_remove.data());
		}
		else {
			gi.Client_Print(ent, PRINT_HIGH, "Failed to remove from ban.txt or ban not found.\n");
		}
	}

	void ResetMatch(gentity_t* ent, const CommandArgs& args) {
		if (level.matchState < MatchState::In_Progress) {
			gi.Client_Print(ent, PRINT_HIGH, "Match has not yet begun.\n");
			return;
		}
		if (level.intermission.time) {
			gi.Client_Print(ent, PRINT_HIGH, "Match has already ended.\n");
			return;
		}
		gi.Broadcast_Print(PRINT_HIGH, "[ADMIN]: Forced match reset.\n");
		Match_Reset();
	}

	void ReplayGame(gentity_t* ent, const CommandArgs& args) {
		if (args.count() < 2 || args.getString(1) == "?") {
			PrintUsage(ent, args, "<game#> [confirm]", "", "Replays a specific tournament game.");
			return;
		}

		auto gameNumber = args.getInt(1);
		if (!gameNumber || *gameNumber < 1) {
			gi.Client_Print(ent, PRINT_HIGH, "Invalid game number.\n");
			return;
		}

		const bool confirmed = (args.count() >= 3 && (args.getString(2) == "confirm" || args.getString(2) == "yes"));
		if (!confirmed) {
			gi.LocClient_Print(ent, PRINT_HIGH,
				"Replay will restart game {}. Run 'replay {} confirm' to proceed.\n",
				*gameNumber, *gameNumber);
			return;
		}

		std::string message;
		if (!Tournament_ReplayGame(*gameNumber, message)) {
			if (!message.empty()) {
				gi.Client_Print(ent, PRINT_HIGH, G_Fmt("{}\n", message.c_str()).data());
			}
			return;
		}

		gi.LocClient_Print(ent, PRINT_HIGH, "Replay queued for game {}.\n", *gameNumber);
	}

	void Ruleset(gentity_t* ent, const CommandArgs& args) {
		if (args.count() < 2 || args.getString(1) == "?") {
			std::string usage = std::format("Current ruleset is {}.\nValid rulesets: q1, q2, q3a",
				rs_long_name[static_cast<int>(game.ruleset)]);
			PrintUsage(ent, args, "<ruleset>", "", usage);
			return;
		}

		::Ruleset rs = RS_IndexFromString(args.getString(1).data());
		if (rs == ::Ruleset::None) {
			gi.Client_Print(ent, PRINT_HIGH, "Invalid ruleset.\n");
			return;
		}

		std::string cvar_val = std::format("{}", static_cast<int>(rs));
		gi.cvarForceSet("g_ruleset", cvar_val.c_str());
	}

	void SetMap(gentity_t* ent, const CommandArgs& args) {
		if (args.count() < 2 || args.getString(1) == "?") {
			PrintUsage(ent, args, "<mapname>", "", "Changes to a map within the map pool.");
			PrintMapList(ent, false);
			return;
		}

		std::string_view mapName_sv = args.getString(1);
		std::string mapName(mapName_sv);
		const MapEntry* map = game.mapSystem.GetMapEntry(mapName);

		if (!map) {
			gi.LocClient_Print(ent, PRINT_HIGH, "Map '{}' not found in map pool.\n", mapName.c_str());
			return;
		}

		if (map->longName.empty()) {
			gi.LocBroadcast_Print(PRINT_HIGH, "[ADMIN]: Changing map to {}\n", map->filename.c_str());
		}
		else {
			gi.LocBroadcast_Print(PRINT_HIGH, "[ADMIN]: Changing map to {} ({})\n", map->filename.c_str(), map->longName.c_str());
		}

		level.changeMap = map->filename;
		ExitLevel(true);
	}

	void SetTeam(gentity_t* ent, const CommandArgs& args) {
		if (args.count() < 3) {
			PrintUsage(ent, args, "<client> <team>", "", "Forcibly moves a client to the specified team.");
			return;
		}

		gentity_t* targ = ClientEntFromString(args.getString(1).data());
		if (!targ || !targ->inUse || !targ->client) {
			gi.Client_Print(ent, PRINT_HIGH, "Invalid client specified.\n");
			return;
		}

		Team team = StringToTeamNum(args.getString(2).data());
		if (team == Team::None) {
			gi.Client_Print(ent, PRINT_HIGH, "Invalid team.\n");
			return;
		}

		if (targ->client->sess.team == team) {
			gi.LocClient_Print(ent, PRINT_HIGH, "{} is already on the {} team.\n", targ->client->sess.netName, Teams_TeamName(team));
			return;
		}

		if ((Teams() && team == Team::Free) || (!Teams() && team != Team::Spectator && team != Team::Free)) {
			gi.Client_Print(ent, PRINT_HIGH, "Cannot set this team in the current gametype.\n");
			return;
		}

		gi.LocBroadcast_Print(PRINT_HIGH, "[ADMIN]: Moved {} to the {} team.\n", targ->client->sess.netName, Teams_TeamName(team));
		::SetTeam(targ, team, false, true, false);
	}

	void Shuffle(gentity_t* ent, const CommandArgs& args) {
		gi.Broadcast_Print(PRINT_HIGH, "[ADMIN]: Forced team shuffle.\n");
		TeamSkillShuffle();
	}

	void StartMatch(gentity_t* ent, const CommandArgs& args) {
		if (level.matchState > MatchState::Warmup_ReadyUp) {
			gi.Client_Print(ent, PRINT_HIGH, "Match has already started.\n");
			return;
		}
		gi.Broadcast_Print(PRINT_HIGH, "[ADMIN]: Forced match start.\n");
		Match_Start();
	}

	void UnlockTeam(gentity_t* ent, const CommandArgs& args) {
		if (args.count() < 2 || args.getString(1) == "?") {
			PrintUsage(ent, args, "<red|blue>", "", "Unlocks a team, allowing players to join.");
			return;
		}

		Team team = StringToTeamNum(args.getString(1).data());
		if (team != Team::Red && team != Team::Blue) {
			gi.Client_Print(ent, PRINT_HIGH, "Invalid team specified.\n");
			return;
		}

		auto team_idx = static_cast<size_t>(team);
		if (!level.locked[team_idx]) {
			gi.LocClient_Print(ent, PRINT_HIGH, "{} is already unlocked.\n", Teams_TeamName(team));
		}
		else {
			level.locked[team_idx] = false;
			gi.LocBroadcast_Print(PRINT_HIGH, "[ADMIN]: {} has been unlocked.\n", Teams_TeamName(team));
		}
	}

	void UnReadyAll(gentity_t* ent, const CommandArgs& args) {
		if (!ReadyConditions(ent, false)) {
			return;
		}
		::UnReadyAll();
		gi.Broadcast_Print(PRINT_HIGH, "[ADMIN]: Forced all players to NOT ready status.\n");
	}


	// --- Registration Function ---
	void RegisterAdminCommands() {
		using enum CommandFlag;
		RegisterCommand("add_admin", &AddAdmin, AdminOnly | AllowIntermission | AllowSpectator);
		RegisterCommand("add_ban", &AddBan, AdminOnly | AllowIntermission | AllowSpectator);
		RegisterCommand("arena", &ForceArena, AdminOnly | AllowSpectator);
		RegisterCommand("balance", &BalanceTeams, AdminOnly | AllowSpectator);
		RegisterCommand("boot", &Boot, AdminOnly | AllowIntermission | AllowSpectator);
		RegisterCommand("end_match", &EndMatch, AdminOnly | AllowSpectator);
		RegisterCommand("force_vote", &ForceVote, AdminOnly | AllowIntermission | AllowSpectator);
		RegisterCommand("gametype", &Gametype, AdminOnly | AllowIntermission | AllowSpectator);
		RegisterCommand("load_admins", &LoadAdmins, AdminOnly | AllowIntermission | AllowSpectator);
		RegisterCommand("load_bans", &LoadBans, AdminOnly | AllowIntermission | AllowSpectator);
		RegisterCommand("load_motd", &LoadMotd, AdminOnly | AllowIntermission | AllowSpectator);
		RegisterCommand("load_mappool", &LoadMapPool, AdminOnly | AllowIntermission | AllowSpectator);
		RegisterCommand("load_mapcycle", &LoadMapCycle, AdminOnly | AllowIntermission | AllowSpectator);
		RegisterCommand("lock_team", &LockTeam, AdminOnly | AllowIntermission | AllowSpectator);
		RegisterCommand("map_restart", &MapRestart, AdminOnly | AllowIntermission | AllowSpectator);
		RegisterCommand("next_map", &NextMap, AdminOnly | AllowIntermission | AllowSpectator);
		RegisterCommand("ready_all", &ReadyAll, AdminOnly | AllowIntermission | AllowSpectator);
		RegisterCommand("remove_admin", &RemoveAdmin, AdminOnly | AllowIntermission | AllowSpectator);
		RegisterCommand("remove_ban", &RemoveBan, AdminOnly | AllowIntermission | AllowSpectator);
		RegisterCommand("reset_match", &ResetMatch, AdminOnly | AllowIntermission | AllowSpectator);
		RegisterCommand("replay", &ReplayGame, AdminOnly | AllowIntermission | AllowSpectator);
		RegisterCommand("ruleset", &Ruleset, AdminOnly | AllowIntermission | AllowSpectator);
		RegisterCommand("set_map", &SetMap, AdminOnly | AllowIntermission | AllowSpectator);
		RegisterCommand("set_team", &SetTeam, AdminOnly | AllowIntermission | AllowSpectator);
		RegisterCommand("shuffle", &Shuffle, AdminOnly | AllowIntermission | AllowSpectator);
		RegisterCommand("start_match", &StartMatch, AdminOnly | AllowIntermission | AllowSpectator);
		RegisterCommand("unlock_team", &UnlockTeam, AdminOnly | AllowIntermission | AllowSpectator);
		RegisterCommand("unready_all", &UnReadyAll, AdminOnly | AllowIntermission | AllowSpectator);
	}

} // namespace Commands

