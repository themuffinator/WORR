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
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_8b14929e06d0");
			return;
		}

		if (AppendIDToFile("admin.txt", resolvedID)) {
			LoadAdminList();
			std::string playerName = GetClientConfigStore().PlayerNameForSocialID(resolvedID);
			if (!playerName.empty()) {
				gi.LocBroadcast_Print(PRINT_CHAT, "$g_sgame_auto_e96321e5d944", playerName.c_str());
			}
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_8890d5af0cc7", resolvedID);
		}
		else {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_489e4de99632");
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
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_8b14929e06d0");
			return;
		}

		if (game.adminIDs.contains(resolvedID)) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_a8f3d146c7ed");
			return;
		}

		if (host && host->client && Q_strcasecmp(resolvedID, host->client->sess.socialID) == 0) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_95bd35eafbce");
			return;
		}

		if (AppendIDToFile("ban.txt", resolvedID)) {
			LoadBanList();
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_3937f162b76a", resolvedID);
		}
		else {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_8f28d33f1942");
		}
	}

	void ForceArena(gentity_t* ent, const CommandArgs& args) {
		if (!level.arenaTotal) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_cfef3dc739d3");
			return;
		}

		if (args.count() < 2 || args.getString(1) == "?") {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_a1120f0db28e", level.arenaActive, level.arenaTotal);
			return;
		}

		auto arenaNum = args.getInt(1);
		if (!arenaNum) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_39309b7594c7", args.getString(1).data());
			return;
		}

		if (*arenaNum == level.arenaActive) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_9c6055a483ad", *arenaNum);
			return;
		}

		if (!CheckArenaValid(*arenaNum)) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_4e45602a2149", *arenaNum);
			return;
		}

		if (!ChangeArena(*arenaNum)) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_1acaa829ba8b");
			return;
		}

		gi.LocBroadcast_Print(PRINT_HIGH, "$g_sgame_auto_816ae28d87e7", level.arenaActive);
	}

	void BalanceTeams(gentity_t* ent, const CommandArgs& args) {
		gi.LocBroadcast_Print(PRINT_HIGH, "$g_sgame_auto_2d6b68bdde58");
		TeamBalance(true);
	}

	void Boot(gentity_t* ent, const CommandArgs& args) {
		if (args.count() < 2 || args.getString(1) == "?") {
			PrintUsage(ent, args, "<client name/number>", "", "Removes the specified client from the server.");
			return;
		}

		gentity_t* targ = ClientEntFromString(args.getString(1).data());
		if (!targ) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_616886d5b86e");
			return;
		}
		if (targ == host) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_5e51b556e29c");
			return;
		}
		if (targ->client && targ->client->sess.admin) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_48d937cd12ff");
			return;
		}

		std::string kickCmd = std::format("kick {}\n", targ->s.number - 1);
		gi.AddCommandString(kickCmd.c_str());
	}

	void EndMatch(gentity_t* ent, const CommandArgs& args) {
		if (level.matchState < MatchState::In_Progress) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_090fefe03462");
			return;
		}
		if (level.intermission.time) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_fe72b29f79ea");
			return;
		}
		QueueIntermission("[ADMIN]: Forced match end.", true, false);
	}

	void ForceVote(gentity_t* ent, const CommandArgs& args) {
		if (!level.vote.time) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_c8822d062f68");
			return;
		}

		if (args.count() < 2) {
			PrintUsage(ent, args, "<yes|no>", "", "Forces the outcome of a current vote.");
			return;
		}

		std::string_view arg = args.getString(1);
		if (arg.starts_with('y') || arg.starts_with('Y') || arg == "1") {
			gi.LocBroadcast_Print(PRINT_HIGH, "$g_sgame_auto_b415cfb02d6f");
			level.vote.executeTime = level.time + 3_sec;
			level.vote.client = nullptr;
		}
		else {
			gi.LocBroadcast_Print(PRINT_HIGH, "$g_sgame_auto_9ff89b8b7ce4");
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
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_112f94340b1b");
			return;
		}

		ChangeGametype(*gt);
	}

	void LoadAdmins(gentity_t* ent, const CommandArgs& args) {
		LoadAdminList();
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_96d5011cc198");
	}

	void LoadBans(gentity_t* ent, const CommandArgs& args) {
		LoadBanList();
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_cf57ab7223d8");
	}

	void LoadMotd(gentity_t* ent, const CommandArgs& args) {
		::LoadMotd();
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_3ba1ef32e50d");
	}

	void LoadMapPool(gentity_t* ent, const CommandArgs& args) {
		::LoadMapPool(ent);
		::LoadMapCycle(ent);
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_b00bc3af956d");
	}

	void LoadMapCycle(gentity_t* ent, const CommandArgs& args) {
		::LoadMapCycle(ent);
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_ea41db838261");
	}

	void LockTeam(gentity_t* ent, const CommandArgs& args) {
		if (args.count() < 2 || args.getString(1) == "?") {
			PrintUsage(ent, args, "<red|blue>", "", "Locks a team, preventing players from joining.");
			return;
		}

		Team team = StringToTeamNum(args.getString(1).data());
		if (team != Team::Red && team != Team::Blue) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_9b5b7bed9ce2");
			return;
		}

		auto team_idx = static_cast<size_t>(team);
		if (level.locked[team_idx]) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_7b5a5cf90df2", Teams_TeamName(team));
		}
		else {
			level.locked[team_idx] = true;
			gi.LocBroadcast_Print(PRINT_HIGH, "$g_sgame_auto_78c72af0ee0d", Teams_TeamName(team));
		}
	}

	void MapRestart(gentity_t* ent, const CommandArgs& args) {
		gi.LocBroadcast_Print(PRINT_HIGH, "$g_sgame_auto_19786f0b7339");
		std::string command = std::format("gamemap {}\n", level.mapName.data());
		gi.AddCommandString(command.c_str());
	}

	void NextMap(gentity_t* ent, const CommandArgs& args) {
		gi.LocBroadcast_Print(PRINT_HIGH, "$g_sgame_auto_d7103e1a0250");
		Match_End();
	}

	void ReadyAll(gentity_t* ent, const CommandArgs& args) {
		if (!ReadyConditions(ent, true)) {
			return;
		}
		::ReadyAll();
		gi.LocBroadcast_Print(PRINT_HIGH, "$g_sgame_auto_74061502354e");
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
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_8b14929e06d0");
			return;
		}

		if (host && host->client && Q_strcasecmp(resolvedID, host->client->sess.socialID) == 0) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_f9c062bd080b");
			return;
		}

		if (RemoveIDFromFile("admin.txt", resolvedID)) {
			LoadAdminList();
			std::string playerName = GetClientConfigStore().PlayerNameForSocialID(resolvedID);
			if (!playerName.empty()) {
				gi.LocBroadcast_Print(PRINT_CHAT, "$g_sgame_auto_8a604d586b47", playerName.c_str());
			}
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_95fd409fe486", resolvedID);
		}
		else {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_de59735dfea3");
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
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_d0090684e797", id_to_remove.data());
		}
		else {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_ce535a7769d1");
		}
	}

	void ResetMatch(gentity_t* ent, const CommandArgs& args) {
		if (level.matchState < MatchState::In_Progress) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_090fefe03462");
			return;
		}
		if (level.intermission.time) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_fe72b29f79ea");
			return;
		}
		gi.LocBroadcast_Print(PRINT_HIGH, "$g_sgame_auto_545849f8ced9");
		Match_Reset();
	}

	void ReplayGame(gentity_t* ent, const CommandArgs& args) {
		if (args.count() < 2 || args.getString(1) == "?") {
			PrintUsage(ent, args, "<game#> [confirm]", "", "Replays a specific tournament game.");
			return;
		}

		auto gameNumber = args.getInt(1);
		if (!gameNumber || *gameNumber < 1) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_3e0e3681a64e");
			return;
		}

		const bool confirmed = (args.count() >= 3 && (args.getString(2) == "confirm" || args.getString(2) == "yes"));
		if (!confirmed) {
			gi.LocClient_Print(ent, PRINT_HIGH,
				"$g_sgame_auto_c662416e196d",
				*gameNumber, *gameNumber);
			return;
		}

		std::string message;
		if (!Tournament_ReplayGame(*gameNumber, message)) {
			if (!message.empty()) {
				gi.LocClient_Print(ent, PRINT_HIGH, G_Fmt("{}\n", message.c_str()).data());
			}
			return;
		}

		gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_1f16fbced404", *gameNumber);
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
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_d6b7bd7d3da7");
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
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_356fb3f821c4", mapName.c_str());
			return;
		}

		if (map->longName.empty()) {
			gi.LocBroadcast_Print(PRINT_HIGH, "$g_sgame_auto_e695a4dbed07", map->filename.c_str());
		}
		else {
			gi.LocBroadcast_Print(PRINT_HIGH, "$g_sgame_auto_d29ad70496c5", map->filename.c_str(), map->longName.c_str());
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
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_616886d5b86e");
			return;
		}

		Team team = StringToTeamNum(args.getString(2).data());
		if (team == Team::None) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_0e6707b5d42f");
			return;
		}

		if (targ->client->sess.team == team) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_2fb1ead52da0", targ->client->sess.netName, Teams_TeamName(team));
			return;
		}

		if ((Teams() && team == Team::Free) || (!Teams() && team != Team::Spectator && team != Team::Free)) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_0f237b89f36f");
			return;
		}

		gi.LocBroadcast_Print(PRINT_HIGH, "$g_sgame_auto_03e82a05d734", targ->client->sess.netName, Teams_TeamName(team));
		::SetTeam(targ, team, false, true, false);
	}

	void Shuffle(gentity_t* ent, const CommandArgs& args) {
		gi.LocBroadcast_Print(PRINT_HIGH, "$g_sgame_auto_886f3dd92c2c");
		TeamSkillShuffle();
	}

	void StartMatch(gentity_t* ent, const CommandArgs& args) {
		if (level.matchState > MatchState::Warmup_ReadyUp) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_f8817b053646");
			return;
		}
		gi.LocBroadcast_Print(PRINT_HIGH, "$g_sgame_auto_71f4aa9e1d60");
		Match_Start();
	}

	void UnlockTeam(gentity_t* ent, const CommandArgs& args) {
		if (args.count() < 2 || args.getString(1) == "?") {
			PrintUsage(ent, args, "<red|blue>", "", "Unlocks a team, allowing players to join.");
			return;
		}

		Team team = StringToTeamNum(args.getString(1).data());
		if (team != Team::Red && team != Team::Blue) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_9b5b7bed9ce2");
			return;
		}

		auto team_idx = static_cast<size_t>(team);
		if (!level.locked[team_idx]) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_98251845a313", Teams_TeamName(team));
		}
		else {
			level.locked[team_idx] = false;
			gi.LocBroadcast_Print(PRINT_HIGH, "$g_sgame_auto_4637209e598c", Teams_TeamName(team));
		}
	}

	void UnReadyAll(gentity_t* ent, const CommandArgs& args) {
		if (!ReadyConditions(ent, false)) {
			return;
		}
		::UnReadyAll();
		gi.LocBroadcast_Print(PRINT_HIGH, "$g_sgame_auto_ceff6285cf36");
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

