/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

g_client_cfg.cpp implementation.*/

#include "client_config.hpp"

#include "../g_local.hpp"
#include "../../bgame/weapon_pref_utils.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <json/json.h>
#include <memory>
#include <limits>
#include <optional>
#include <sstream>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {

	constexpr int kDefaultSkillRating = 1500;
	const std::string kDefaultPlayerConfigDirectory = GAMEVERSION + "/pcfg";

	} // namespace

	/*
	=============
	ClientConfigStore::ClientConfigStore

	Captures the global interfaces needed to access cvars and filesystem state so
	the store can be exercised without implicit globals.
	=============
	*/
ClientConfigStore::ClientConfigStore(local_game_import_t& gi, std::string playerConfigDirectory)
	: gi_(gi)
	, playerConfigDirectory_(std::move(playerConfigDirectory)) {}

	/*
	=============
	ClientConfigStore::EnsurePlayerConfigDirectory

	Creates the player configuration directory if it does not yet exist.
	Returns true when the directory exists and can be written to.
	=============
	*/

	bool ClientConfigStore::EnsurePlayerConfigDirectory() const {
	std::error_code ec;
	std::filesystem::create_directories(playerConfigDirectory_, ec);
	if (ec) {
	gi_.Com_PrintFmt("WARNING: failed to create player config directory {}: {}\n",
	playerConfigDirectory_.c_str(), ec.message().c_str());
	}

	if (!std::filesystem::exists(playerConfigDirectory_)) {
	gi_.Com_PrintFmt("WARNING: player config directory missing: {}\n", playerConfigDirectory_.c_str());
	return false;
	}

	return true;
}

	/*
	=============
	ClientConfigStore::SaveInternal
	
	Persists statistics and optional player configuration updates to disk.
	=============
	*/
void ClientConfigStore::SaveInternal(const std::string& playerID, int skillRating, int skillChange, int64_t timePlayedSeconds,
	bool won, bool isGhost, bool updateStats, const client_config_t* pc, const std::vector<Weapon>* weaponPrefs) const {
	if (playerID.empty())
		return;

	const auto pathOpt = PlayerConfigPathFromID(playerID, __FUNCTION__);
	if (!pathOpt)
		return;

	const std::string path = *pathOpt;
	Json::Value cfg(Json::objectValue);
	bool modified = false;

	{
		std::ifstream in(path);
		if (in.is_open()) {
			Json::CharReaderBuilder builder;
			std::string errs;
			if (!Json::parseFromStream(builder, in, &cfg, &errs)) {
				gi_.Com_PrintFmt("{}: parse error in {}: {}\n", __FUNCTION__, path.c_str(), errs.c_str());
				cfg = Json::Value(Json::objectValue);
			}
		}
		else {
			gi_.Com_PrintFmt("{}: creating new player config for missing file {}\n", __FUNCTION__, path.c_str());
		}
	}

	if (updateStats) {
		if (!cfg.isMember("stats") || !cfg["stats"].isObject()) {
			Json::Value stats(Json::objectValue);
			stats["totalMatches"] = 0;
			stats["totalWins"] = 0;
			stats["totalLosses"] = 0;
			stats["totalAbandons"] = 0;
			stats["totalTimePlayed"] = Json::Value::Int64(0);
			stats["bestSkillRating"] = 0;
			stats["lastSkillRating"] = kDefaultSkillRating;
			stats["lastSkillChange"] = 0;
			cfg["stats"] = stats;
			modified = true;
		}

		auto& stats = cfg["stats"];
		auto ensure_int = [&](const char* key, int def) {
			if (!stats.isMember(key) || !stats[key].isInt()) {
				stats[key] = def;
				modified = true;
			}
			return stats[key].asInt();
		};

		auto ensure_int64 = [&](const char* key, Json::Value::Int64 def) {
			if (!stats.isMember(key) || !stats[key].isInt64()) {
				stats[key] = def;
				modified = true;
			}
			return stats[key].asInt64();
		};

		const int totalMatches = ensure_int("totalMatches", 0);
		const int totalWins = ensure_int("totalWins", 0);
		const int totalLosses = ensure_int("totalLosses", 0);
		const int totalAbandons = ensure_int("totalAbandons", 0);
		const int bestSkillRating = ensure_int("bestSkillRating", 0);
		const Json::Value::Int64 totalTimePlayed = ensure_int64("totalTimePlayed", 0);
		const int lastSkillRating = ensure_int("lastSkillRating", kDefaultSkillRating);
		const int lastSkillChange = ensure_int("lastSkillChange", 0);

		const int newTotalMatches = totalMatches + 1;
		if (newTotalMatches != totalMatches) {
			stats["totalMatches"] = newTotalMatches;
			modified = true;
		}

		const int newTotalWins = won ? totalWins + 1 : totalWins;
		if (newTotalWins != totalWins) {
			stats["totalWins"] = newTotalWins;
			modified = true;
		}

		if (!won) {
			const int updatedAbandons = isGhost ? totalAbandons + 1 : totalAbandons;
			const int updatedLosses = isGhost ? totalLosses : totalLosses + 1;
			if (updatedAbandons != totalAbandons) {
				stats["totalAbandons"] = updatedAbandons;
				modified = true;
			}
			if (updatedLosses != totalLosses) {
				stats["totalLosses"] = updatedLosses;
				modified = true;
			}
		}

		const Json::Value::Int64 nonNegativeTime = std::max<Json::Value::Int64>(timePlayedSeconds, 0);
		const Json::Value::Int64 cappedBase = std::max<Json::Value::Int64>(totalTimePlayed, 0);
		const Json::Value::Int64 maxValue = std::numeric_limits<Json::Value::Int64>::max();
		Json::Value::Int64 newTotalTime = cappedBase;
		if (cappedBase > maxValue - nonNegativeTime)
			newTotalTime = maxValue;
		else
			newTotalTime = cappedBase + nonNegativeTime;

		if (newTotalTime != totalTimePlayed) {
			stats["totalTimePlayed"] = Json::Value::Int64(newTotalTime);
			modified = true;
		}

		const int updatedBest = std::max(bestSkillRating, skillRating);
		if (updatedBest != bestSkillRating) {
			stats["bestSkillRating"] = updatedBest;
			modified = true;
		}

		if (lastSkillRating != skillRating) {
			stats["lastSkillRating"] = skillRating;
			modified = true;
		}

		if (lastSkillChange != skillChange) {
			stats["lastSkillChange"] = skillChange;
			modified = true;
		}
	}

	if (pc) {
		if (!cfg.isMember("config") || !cfg["config"].isObject()) {
			cfg["config"] = Json::Value(Json::objectValue);
			modified = true;
		}

		auto& config = cfg["config"];
		if (!config.isMember("drawCrosshairID") || !config["drawCrosshairID"].isBool() || config["drawCrosshairID"].asBool() != pc->show_id) {
			config["drawCrosshairID"] = pc->show_id;
			modified = true;
		}
		if (!config.isMember("drawTimer") || !config["drawTimer"].isBool() || config["drawTimer"].asBool() != pc->show_timer) {
			config["drawTimer"] = pc->show_timer;
			modified = true;
		}
		if (!config.isMember("drawFragMessages") || !config["drawFragMessages"].isBool() || config["drawFragMessages"].asBool() != pc->show_fragmessages) {
			config["drawFragMessages"] = pc->show_fragmessages;
			modified = true;
		}
		if (!config.isMember("eyeCam") || !config["eyeCam"].isBool() || config["eyeCam"].asBool() != pc->use_eyecam) {
			config["eyeCam"] = pc->use_eyecam;
			modified = true;
		}
		if (!config.isMember("killBeep") || !config["killBeep"].isInt() || config["killBeep"].asInt() != pc->killbeep_num) {
			config["killBeep"] = pc->killbeep_num;
			modified = true;
		}
		if (!config.isMember("followKiller") || !config["followKiller"].isBool() || config["followKiller"].asBool() != pc->follow_killer) {
			config["followKiller"] = pc->follow_killer;
			modified = true;
		}
		if (!config.isMember("followLeader") || !config["followLeader"].isBool() || config["followLeader"].asBool() != pc->follow_leader) {
			config["followLeader"] = pc->follow_leader;
			modified = true;
		}
		if (!config.isMember("followPowerup") || !config["followPowerup"].isBool() || config["followPowerup"].asBool() != pc->follow_powerup) {
			config["followPowerup"] = pc->follow_powerup;
			modified = true;
		}
	}

	if (weaponPrefs) {
		if (!cfg.isMember("config") || !cfg["config"].isObject()) {
			cfg["config"] = Json::Value(Json::objectValue);
		}

		std::array<bool, static_cast<size_t>(Weapon::Total)> seen{};
		Json::Value prefs(Json::arrayValue);
		for (Weapon weapon : *weaponPrefs) {
			const auto index = static_cast<size_t>(weapon);
			if (weapon == Weapon::None || index >= seen.size() || seen[index])
				continue;

			seen[index] = true;
			std::string_view abbr = WeaponToAbbreviation(weapon);
			if (!abbr.empty())
				prefs.append(std::string(abbr));
		}

		if (cfg["config"]["weaponPrefs"] != prefs) {
			cfg["config"]["weaponPrefs"] = prefs;
			modified = true;
		}
	}

	if (!modified)
		return;

	cfg["lastUpdated"] = TimeStamp();

	try {
		if (!EnsurePlayerConfigDirectory()) {
			gi_.Com_PrintFmt("{}: failed to ensure player config directory\n", __FUNCTION__);
			return;
		}

		std::ofstream out(path);
		if (!out.is_open()) {
			gi_.Com_PrintFmt("{}: failed to write {}\n", __FUNCTION__, path.c_str());
			return;
		}

		Json::StreamWriterBuilder writerBuilder;
		writerBuilder["indentation"] = "\t";
		std::unique_ptr<Json::StreamWriter> writer(writerBuilder.newStreamWriter());
		writer->write(cfg, &out);
		out.close();

		gi_.Com_PrintFmt("{}: saved updates for {}\n", __FUNCTION__, playerID.c_str());
	}
	catch (const std::exception& e) {
		gi_.Com_PrintFmt("{}: exception: {}\n", __FUNCTION__, e.what());
	}
}
/*
=============
ClientConfigStore::PlayerConfigPathFromID

Sanitizes the supplied player ID and resolves it to an on-disk JSON path.
Returns std::nullopt when the ID cannot be mapped to a safe filename.
=============
*/
std::optional<std::string> ClientConfigStore::PlayerConfigPathFromID(const std::string& playerID, const char* functionName) const {
const std::string sanitized = SanitizeSocialID(playerID);
if (sanitized.empty()) {
if (!playerID.empty() && gi_.Com_Print) {
gi_.Com_PrintFmt("WARNING: {}: refusing to use invalid social ID '{}' for config filename\n", functionName, playerID.c_str());
}
return std::nullopt;
}

if (sanitized != playerID && gi_.Com_Print) {
gi_.Com_PrintFmt("WARNING: {}: sanitized social ID '{}' to '{}' for config filename\n", functionName, playerID.c_str(), sanitized.c_str());
}

return std::optional<std::string>(G_Fmt("{}/{}.json", playerConfigDirectory_, sanitized).data());
}

/*
=============
ClientConfigStore::CreateProfile

Creates a new configuration JSON file on disk populated with the default
schema for the supplied player identity.
=============
*/
void ClientConfigStore::CreateProfile(gclient_t* client, const std::string& playerID, const std::string& playerName, const std::string& gameType) const {
	if (playerID.empty())
		return;

	Json::Value newFile(Json::objectValue);

	newFile["socialID"] = playerID;
	newFile["playerName"] = playerName;
	newFile["originalPlayerName"] = playerName;
	newFile["playerAliases"] = Json::Value(Json::arrayValue);

	Json::Value config(Json::objectValue);
	config["drawCrosshairID"] = 1;
	config["drawFragMessages"] = 1;
	config["drawTimer"] = 1;
	config["eyeCam"] = 1;
	config["killBeep"] = 1;
	config["followKiller"] = client ? client->sess.pc.follow_killer : false;
	config["followLeader"] = client ? client->sess.pc.follow_leader : false;
	config["followPowerup"] = client ? client->sess.pc.follow_powerup : false;
	config["weaponPrefs"] = Json::Value(Json::arrayValue);
	newFile["config"] = config;

	Json::Value ratings(Json::objectValue);
	ratings[gameType] = kDefaultSkillRating;
	newFile["ratings"] = ratings;

	Json::Value stats(Json::objectValue);
	stats["totalMatches"] = 0;
	stats["totalWins"] = 0;
	stats["totalLosses"] = 0;
	stats["totalTimePlayed"] = Json::Value::Int64(0);
	stats["bestSkillRating"] = 0;
	stats["lastSkillRating"] = kDefaultSkillRating;
	stats["lastSkillChange"] = 0;
	newFile["stats"] = stats;

	newFile["admin"] = false;
	newFile["banned"] = false;
	newFile["lastUpdated"] = TimeStamp();
	newFile["lastSeen"] = TimeStamp();
	newFile["firstSeen"] = TimeStamp();

	try {
		const auto pathOpt = PlayerConfigPathFromID(playerID, __FUNCTION__);
		if (!pathOpt)
			return;

		if (!EnsurePlayerConfigDirectory())
			return;

		const std::string path = *pathOpt;
		std::ofstream file(path);
if (file.is_open()) {
Json::StreamWriterBuilder builder;
builder["indentation"] = "\t";
std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
writer->write(newFile, &file);
file.close();
gi_.Com_PrintFmt("Created new client config file: {}\n", path);
}
else {
gi_.Com_PrintFmt("Failed to create client config file: {}\n", path);
}
}
catch (const std::exception& e) {
gi_.Com_PrintFmt("{}: exception while creating client config: {}\n", __FUNCTION__, e.what());
}
}

/*
=============
ClientConfigStore::ApplyWeaponPreferencesFromJson

Hydrates the in-memory weapon preference order from the JSON configuration.
=============
*/
void ClientConfigStore::ApplyWeaponPreferencesFromJson(gclient_t* client, const Json::Value& playerData) const {
	if (!client)
		return;

	client->sess.weaponPrefs.clear();

	if (!playerData.isMember("config"))
		return;

	const auto& cfg = playerData["config"];
	if (!cfg.isMember("weaponPrefs") || !cfg["weaponPrefs"].isArray())
		return;

	const auto& prefs = cfg["weaponPrefs"];
	std::array<bool, static_cast<size_t>(Weapon::Total)> seen{};
	std::vector<Weapon> parsed;
	parsed.reserve(static_cast<size_t>(prefs.size()));
	std::vector<std::string> invalidTokens;
	bool capacityExceeded = false;

	for (const auto& pref : prefs) {
		if (!pref.isString())
			continue;

		std::string normalized;
		switch (TryAppendWeaponPreference(pref.asString(), parsed, seen, &normalized)) {
		case WeaponPrefAppendResult::Added:
			break;
		case WeaponPrefAppendResult::Duplicate:
			break;
		case WeaponPrefAppendResult::Invalid:
			if (!normalized.empty())
				invalidTokens.emplace_back(std::move(normalized));
			break;
		case WeaponPrefAppendResult::CapacityExceeded:
			capacityExceeded = true;
			break;
		}
	}

	client->sess.weaponPrefs.swap(parsed);

if (!invalidTokens.empty()) {
std::ostringstream joined;
for (size_t i = 0; i < invalidTokens.size(); ++i) {
if (i)
joined << ", ";
joined << invalidTokens[i];
}
gi_.Com_PrintFmt("{}: ignored invalid weapon preference tokens for {}: {}\n",
__FUNCTION__, client->sess.netName, joined.str().c_str());
}

if (capacityExceeded) {
gi_.Com_PrintFmt("{}: weapon preferences for {} truncated to {} entries\n",
__FUNCTION__, client->sess.netName, client->sess.weaponPrefs.size());
}

	Client_RebuildWeaponPreferenceOrder(*client);
}

/*
=============
ClientConfigStore::ApplyVisualConfigFromJson

Applies persisted HUD/audio settings to the client's persistant config block.
=============
*/
void ClientConfigStore::ApplyVisualConfigFromJson(gclient_t* client, const Json::Value& playerData) const {
	if (!client)
		return;

	if (!playerData.isMember("config"))
		return;

	const auto& cfg = playerData["config"];
	auto get_bool = [&](const std::string& key, bool def) {
		return cfg.isMember(key) && cfg[key].isBool() ? cfg[key].asBool() : def;
	};
	auto get_int = [&](const std::string& key, int def) {
		return cfg.isMember(key) && cfg[key].isInt() ? cfg[key].asInt() : def;
	};

	client->sess.pc.show_id = get_bool("drawCrosshairID", true);
	client->sess.pc.show_timer = get_bool("drawTimer", true);
	client->sess.pc.show_fragmessages = get_bool("drawFragMessages", true);
	client->sess.pc.use_eyecam = get_bool("eyeCam", true);
	client->sess.pc.killbeep_num = get_int("killBeep", 1);
	client->sess.pc.follow_killer = get_bool("followKiller", client->sess.pc.follow_killer);
	client->sess.pc.follow_leader = get_bool("followLeader", client->sess.pc.follow_leader);
	client->sess.pc.follow_powerup = get_bool("followPowerup", client->sess.pc.follow_powerup);
}

/*
=============
ClientConfigStore::LoadProfile

Initializes the gclient_t session data from the player's persisted profile.
Returns true when the profile was loaded from disk, false when defaults were used.
=============
*/
bool ClientConfigStore::LoadProfile(gclient_t* client, const std::string& playerID, const std::string& playerName, const std::string& gameType) {
	if (!client)
		return false;

	Json::Value playerData;
	bool modified = false;

	client->sess.skillRating = 0;
	client->sess.skillRatingChange = 0;

	if (playerID.empty()) {
		client->sess.skillRating = kDefaultSkillRating;
		return false;
	}

	const auto pathOpt = PlayerConfigPathFromID(playerID, __FUNCTION__);
	if (!pathOpt) {
		client->sess.skillRating = kDefaultSkillRating;
		return false;
	}

	const std::string path = *pathOpt;
	std::ifstream file(path);

	if (!file.is_open()) {
		CreateProfile(client, playerID, playerName, gameType);
		client->sess.skillRating = kDefaultSkillRating;
		return false;
	}

	Json::CharReaderBuilder builder;
	std::string errs;
if (!Json::parseFromStream(builder, file, &playerData, &errs)) {
file.close();
gi_.Com_PrintFmt("Failed to parse client config for {}: {} ({})\n",
playerName.c_str(), path.c_str(), errs.c_str());
gi_.Com_PrintFmt("Resetting {} to default configuration and recreating the client config.\n",
playerName.c_str());
		client->sess.skillRating = kDefaultSkillRating;
		client->sess.skillRatingChange = 0;
		client->sess.admin = false;
		client->sess.banned = false;
		client->sess.weaponPrefs.clear();
		CreateProfile(client, playerID, playerName, gameType);
		return false;
	}
	file.close();

	if (playerData.isMember("playerName") && playerData["playerName"].asString() != playerName) {
		if (!playerData.isMember("originalPlayerName"))
			playerData["originalPlayerName"] = playerData["playerName"];

		if (!playerData.isMember("playerAliases") || !playerData["playerAliases"].isArray())
			playerData["playerAliases"] = Json::Value(Json::arrayValue);

		bool alreadyPresent = false;
		for (const auto& alias : playerData["playerAliases"]) {
			if (alias.asString() == playerName) {
				alreadyPresent = true;
				break;
			}
		}
		if (!alreadyPresent)
			playerData["playerAliases"].append(playerName);

		playerData["playerName"] = playerName;
		modified = true;
	}

	if (!playerData.isMember("config") || !playerData["config"].isObject()) {
		Json::Value config(Json::objectValue);
		config["drawCrosshairID"] = 1;
		config["drawFragMessages"] = 1;
		config["drawTimer"] = 1;
		config["eyeCam"] = 1;
		config["killBeep"] = 1;
		config["followKiller"] = client->sess.pc.follow_killer;
		config["followLeader"] = client->sess.pc.follow_leader;
		config["followPowerup"] = client->sess.pc.follow_powerup;
		config["weaponPrefs"] = Json::Value(Json::arrayValue);
		playerData["config"] = config;
		modified = true;
	}
	else {
		auto& cfg = playerData["config"];
		auto ensure_config_bool = [&](const char* key, bool value) {
			if (!cfg.isMember(key) || !cfg[key].isBool()) {
				cfg[key] = value;
				modified = true;
			}
		};

		ensure_config_bool("followKiller", client->sess.pc.follow_killer);
		ensure_config_bool("followLeader", client->sess.pc.follow_leader);
		ensure_config_bool("followPowerup", client->sess.pc.follow_powerup);

		if (!cfg.isMember("weaponPrefs") || !cfg["weaponPrefs"].isArray()) {
			cfg["weaponPrefs"] = Json::Value(Json::arrayValue);
			modified = true;
		}
	}

	if (!playerData.isMember("stats") || !playerData["stats"].isObject()) {
		Json::Value stats(Json::objectValue);
		stats["totalMatches"] = 0;
		stats["totalWins"] = 0;
		stats["totalLosses"] = 0;
		stats["totalTimePlayed"] = Json::Value::Int64(0);
		stats["bestSkillRating"] = 0;
		stats["lastSkillRating"] = kDefaultSkillRating;
		stats["lastSkillChange"] = 0;
		playerData["stats"] = stats;
		modified = true;
	}

	if (!playerData.isMember("ratings") || !playerData["ratings"].isObject()) {
		Json::Value ratings(Json::objectValue);
		ratings[gameType] = kDefaultSkillRating;
		playerData["ratings"] = ratings;
		playerData["stats"]["lastSkillRating"] = kDefaultSkillRating;
		modified = true;
	}
	else if (!playerData["ratings"].isMember(gameType)) {
		int maxRating = kDefaultSkillRating;
		const Json::Value& ratings = playerData["ratings"];
		for (const auto& key : ratings.getMemberNames())
			maxRating = std::max(maxRating, ratings[key].asInt());

		playerData["ratings"][gameType] = maxRating;
		playerData["stats"]["lastSkillRating"] = maxRating;
		modified = true;
	}

	const std::string now = TimeStamp();
	if (!playerData.isMember("firstSeen")) {
		playerData["firstSeen"] = now;
		modified = true;
	}
	playerData["lastSeen"] = now;
	playerData["lastUpdated"] = now;

if (modified) {
try {
std::ofstream outFile(path);
if (outFile.is_open()) {
				Json::StreamWriterBuilder writerBuilder;
				writerBuilder["indentation"] = "\t";
				std::unique_ptr<Json::StreamWriter> writer(writerBuilder.newStreamWriter());
				writer->write(playerData, &outFile);
				outFile.close();
			}
			else {
gi_.Com_PrintFmt("Failed to write updated config for {}: {}\n", playerName.c_str(), path.c_str());
}
}
catch (const std::exception& e) {
gi_.Com_PrintFmt("{}: exception writing {}: {}\n", __FUNCTION__, playerName.c_str(), e.what());
}
}

	ApplyWeaponPreferencesFromJson(client, playerData);
	ApplyVisualConfigFromJson(client, playerData);

	client->sess.skillRating = playerData["ratings"][gameType].asInt();
	client->sess.skillRatingChange = playerData["stats"].get("lastSkillChange", 0).asInt();
	client->sess.admin = playerData.isMember("admin") && playerData["admin"].isBool() ? playerData["admin"].asBool() : false;
	client->sess.banned = playerData.isMember("banned") && playerData["banned"].isBool() ? playerData["banned"].asBool() : false;

	return true;
}

/*
=============
ClientConfigStore::SaveStats

Persists the real player's session statistics and HUD settings.
=============
*/
void ClientConfigStore::SaveStats(gclient_t* client, bool wonMatch) {
	if (!client || client->sess.is_a_bot || !client->sess.socialID[0])
		return;

	const int64_t rawTimePlayed = client->sess.playEndRealTime - client->sess.playStartRealTime;
	const int64_t timePlayed = rawTimePlayed > 0 ? rawTimePlayed : 0;
	SaveInternal(client->sess.socialID, client->sess.skillRating, client->sess.skillRatingChange, timePlayed, wonMatch, false, true, &client->sess.pc, &client->sess.weaponPrefs);
}

/*
=============
ClientConfigStore::SaveStatsForGhost

Persists statistics for ghost (disconnected) players.
=============
*/
void ClientConfigStore::SaveStatsForGhost(const Ghosts& ghost, bool wonMatch) {
	if (!*ghost.socialID)
		return;

	const int64_t timePlayed = ghost.totalMatchPlayRealTime > 0 ? ghost.totalMatchPlayRealTime : 0;
	SaveInternal(ghost.socialID, ghost.skillRating, ghost.skillRatingChange, timePlayed, wonMatch, true, true, nullptr, nullptr);
}

/*
=============
ClientConfigStore::UpdateConfig

Loads a player's JSON config, applies the provided updater, and writes it back
if any changes occurred.
=============
*/
bool ClientConfigStore::UpdateConfig(const std::string& playerID, const std::function<void(Json::Value&)>& updater) const {
	if (playerID.empty())
		return false;

	const auto pathOpt = PlayerConfigPathFromID(playerID, __FUNCTION__);
	if (!pathOpt)
		return false;

	const std::string path = *pathOpt;
	std::ifstream in(path);
if (!in.is_open()) {
gi_.Com_PrintFmt("{}: failed to open {}\n", __FUNCTION__, path.c_str());
return false;
}

	Json::Value cfg;
	Json::CharReaderBuilder builder;
	std::string errs;
if (!Json::parseFromStream(builder, in, &cfg, &errs)) {
gi_.Com_PrintFmt("{}: parse error in {}: {}\n", __FUNCTION__, path.c_str(), errs.c_str());
return false;
}
	in.close();

	Json::Value before = cfg;
	updater(cfg);

	if (cfg == before)
		return false;

	cfg["lastUpdated"] = TimeStamp();

	try {
if (!EnsurePlayerConfigDirectory())
return false;

std::ofstream out(path);
if (!out.is_open()) {
gi_.Com_PrintFmt("{}: failed to write {}\n", __FUNCTION__, path.c_str());
return false;
}

		Json::StreamWriterBuilder writerBuilder;
		writerBuilder["indentation"] = "\t";
		std::unique_ptr<Json::StreamWriter> writer(writerBuilder.newStreamWriter());
		writer->write(cfg, &out);
		out.close();

gi_.Com_PrintFmt("{}: saved updates for {}\n", __FUNCTION__, playerID.c_str());
return true;
}
catch (const std::exception& e) {
gi_.Com_PrintFmt("{}: exception: {}\n", __FUNCTION__, e.what());
return false;
}
}

/*
=============
ClientConfigStore::SaveWeaponPreferences

Serializes the client's sanitized weapon preference ordering to disk.
=============
*/
void ClientConfigStore::SaveWeaponPreferences(gclient_t* client) {
	if (!client || client->sess.is_a_bot || !client->sess.socialID[0])
		return;

	Client_RebuildWeaponPreferenceOrder(*client);
	std::vector<std::string> sanitized = GetSanitizedWeaponPrefStrings(*client);

	UpdateConfig(client->sess.socialID, [sanitized](Json::Value& cfg) {
		if (!cfg.isMember("config") || !cfg["config"].isObject())
			cfg["config"] = Json::Value(Json::objectValue);

		Json::Value prefs(Json::arrayValue);
		for (const auto& pref : sanitized)
			prefs.append(pref);

		cfg["config"]["weaponPrefs"] = prefs;
	});
}

/*
=============
ClientConfigStore::DefaultSkillRating

Provides the default rating assigned to new or untracked players.
=============
*/
int ClientConfigStore::DefaultSkillRating() const {
	return kDefaultSkillRating;
}

/*
=============
ClientConfigStore::PlayerNameForSocialID

Returns the persisted player name for the provided social ID, if available.
=============
*/
std::string ClientConfigStore::PlayerNameForSocialID(const std::string& socialID) const {
	if (socialID.empty())
		return {};

	const auto pathOpt = PlayerConfigPathFromID(socialID, __FUNCTION__);
	if (!pathOpt)
		return {};

	std::ifstream file(*pathOpt);
	if (!file.is_open())
	return {};

	Json::Value root;
	Json::CharReaderBuilder builder;
	std::string errs;

	if (!Json::parseFromStream(builder, file, &root, &errs))
		return {};

	if (!root.isMember("playerName") || !root["playerName"].isString())
		return {};

	return root["playerName"].asString();
}

/*
=============
InitializeClientConfigStore

Configures the dependencies used when lazily instantiating the client config
store singleton.
=============
*/
namespace {
struct ClientConfigStoreDependencies {
local_game_import_t* gi;
	std::string playerConfigDirectory;
	};

	ClientConfigStoreDependencies g_clientConfigStoreDependencies{ &gi, kDefaultPlayerConfigDirectory };
	std::unique_ptr<ClientConfigStore> g_clientConfigStoreInstance;
}

void InitializeClientConfigStore(local_game_import_t& giRef, std::string playerConfigDirectory) {
	g_clientConfigStoreDependencies.gi = &giRef;
	g_clientConfigStoreDependencies.playerConfigDirectory = std::move(playerConfigDirectory);
	g_clientConfigStoreInstance.reset();
}

/*
=============
GetClientConfigStore

Provides access to the global ClientConfigStore instance.
=============
*/
ClientConfigStore& GetClientConfigStore() {
	if (!g_clientConfigStoreInstance) {
	g_clientConfigStoreInstance = std::make_unique<ClientConfigStore>(
	*g_clientConfigStoreDependencies.gi, g_clientConfigStoreDependencies.playerConfigDirectory);
}
	return *g_clientConfigStoreInstance;
}
