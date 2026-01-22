/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

g_match_logging.cpp (Game Match Logging) This file implements a detailed match statistics
logging system. At the end of each match, it gathers comprehensive data about players and teams
(kills, deaths, damage, accuracy, weapon usage, awards, etc.) and writes it out to structured
files for later analysis and display. Key Responsibilities: - Data Aggregation: The
`MatchStats_End` function iterates through all players at the end of a match and compiles their
performance data into `PlayerStats` and `TeamStats` structures. - JSON Output: Serializes the
collected match data into a well-structured JSON file. This format is ideal for data parsing by
external tools or websites. - HTML Report Generation: Creates a user-friendly HTML report of the
match results, including overall scores, team comparisons, top player lists, and detailed
individual performance breakdowns with progress bars and weapon stats. - Match Initialization:
`MatchStats_Init` is called at the start of a match to generate a unique match ID and reset all
statistical counters.*/

#include "../g_local.hpp"
#include "../gameplay/client_config.hpp"
#include "../../bgame/char_array_utils.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <fstream>
#include <iomanip>
#include <json/json.h>
#include <mutex>
#include <queue>
#include <sstream>
#include <string_view>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <cerrno>

using json = Json::Value;

/*
=============
JsonHasData

Returns true when the provided Json::Value is non-null and, for objects or
arrays, contains at least one element. Used to avoid emitting empty
gametype-specific blocks.
=============
*/
static bool JsonHasData(const json& value) {
	if (value.isNull())
		return false;
	if (value.isArray() || value.isObject())
		return value.size() > 0;
	return true;
}

/*
=============
HtmlEscape

Escapes special characters for safe HTML output.
=============
*/
static std::string HtmlEscape(std::string_view input) {
	std::string output;
	output.reserve(input.size());
	for (char c : input) {
		switch (c) {
		case '&':
			output.append("&amp;");
			break;
		case '<':
			output.append("&lt;");
			break;
		case '>':
			output.append("&gt;");
			break;
		case '\"':
			output.append("&quot;");
			break;
		case '\'':
			output.append("&apos;");
			break;
		default:
			output.push_back(c);
			break;
		}
	}
	return output;
}

const std::string MATCH_STATS_PATH = GAMEVERSION + "/matches";

/*
=============
WriteFileAtomically

Writes file contents to a temporary sibling (<path>.tmp) before renaming the
result into place. Ensures output streams are opened in binary mode with
exceptions enabled so callers can detect failures.
=============
*/
static void WriteFileAtomically(const std::filesystem::path& finalPath, const std::function<void(std::ofstream&)>& writer) {
	const std::string finalPathStr = finalPath.string();
	const std::filesystem::path tempPath(finalPathStr + ".tmp");

	std::ofstream file;
	file.exceptions(std::ios::failbit | std::ios::badbit);
	try {
		file.open(tempPath, std::ios::binary);
		writer(file);
		file.flush();
		file.close();

		std::error_code removeExistingError;
		std::filesystem::remove(finalPath, removeExistingError);
		if (removeExistingError && removeExistingError != std::errc::no_such_file_or_directory) {
			throw std::system_error(removeExistingError, "Failed to remove existing file prior to rename");
		}

		std::filesystem::rename(tempPath, finalPath);
	}
	catch (...) {
		if (file.is_open()) {
			try {
				file.close();
			}
			catch (...) {
			}
		}

		std::error_code cleanupError;
		std::filesystem::remove(tempPath, cleanupError);
		throw;
	}
}



/*
=============
Html_FormatMilliseconds

Converts a millisecond duration into a short, human-readable string for HTML output.
=============
*/
static inline std::string Html_FormatMilliseconds(int64_t milliseconds) {
	if (milliseconds <= 0) {
		return "0s";
	}

	const int seconds = static_cast<int>(milliseconds / 1000);
	if (seconds > 0) {
		return FormatDuration(seconds);
	}

	std::ostringstream stream;
	stream << std::fixed << std::setprecision(2) << (milliseconds / 1000.0) << 's';
	return stream.str();
}

[[maybe_unused]] static inline double GetAveragePickupDelay(uint32_t pickupCount, double totalDelay) {
	if (pickupCount == 0)
		return 0.0;
	return totalDelay / pickupCount;
}

static inline ModID getModIdByName(const std::string& modName) {
	for (const auto& m : modr) {
		if (m.name == modName)
			return m.mod;
	}
	return ModID::Unknown; // fallback
}

[[maybe_unused]] const std::array<std::string, 2> booleanStrings = { "false", "true" };
[[maybe_unused]] const std::array<std::string, 2> winLossStrings = { "loss", "win" };

struct PlayerStats {
	std::string socialID;
	std::string playerName;
	int totalKills = 0;
	int totalSpawnKills = 0;
	int totalTeamKills = 0;
	int totalDeaths = 0;
	int totalSuicides = 0;
	double totalKDR = 0.0;
	int totalScore = 0;
	int proBallGoals = 0;
	int proBallAssists = 0;
	int totalShots = 0;
	int totalHits = 0;
	double totalAccuracy = 0.0;
	int totalDmgDealt = 0;
	int totalDmgReceived = 0;
	int ratingChange = 0;

	double killsPerMinute = 0;
	int64_t playTimeMsec = 0;
	int skillRating = 0;
	int skillRatingChange = 0;

	std::array <uint32_t, static_cast<size_t>(HighValueItems::Total)> pickupCounts = {};
	std::array <double, static_cast<size_t>(HighValueItems::Total)> pickupDelays = {};

	int ctfFlagPickups = 0;
	int ctfFlagDrops = 0;
	int ctfFlagReturns = 0;
	int ctfFlagAssists = 0;
	int ctfFlagCaptures = 0;
	int64_t ctfFlagCarrierTimeTotalMsec = 0;
	int ctfFlagCarrierTimeShortestMsec = 0;
	int ctfFlagCarrierTimeLongestMsec = 0;

	// Weapon-based stats
	std::array<int, static_cast<size_t>(Weapon::Total)> totalShotsPerWeapon = {};
	std::array<int, static_cast<size_t>(Weapon::Total)> totalHitsPerWeapon = {};
	std::array<double, static_cast<size_t>(Weapon::Total)> accuracyPerWeapon = {};

	// MOD-based stats
	std::array<int, static_cast<size_t>(ModID::Total)> modTotalKills = {};
	std::array<int, static_cast<size_t>(ModID::Total)> modTotalDeaths = {};
	std::array<double, static_cast<size_t>(ModID::Total)> modTotalKDR = {};
	std::array<int, static_cast<size_t>(ModID::Total)> modTotalDmgD = {};
	std::array<int, static_cast<size_t>(ModID::Total)> modTotalDmgR = {};

	std::array<uint32_t, static_cast<size_t>(PlayerMedal::Total)> awards = {};
	json gametypeStats;

	PlayerStats() = default;

	/*
	=============
	PlayerStats::calculateWeaponAccuracy

	Calculates accuracy for each weapon slot based on hit and shot counts.
	=============
	*/
	void calculateWeaponAccuracy() {
		for (size_t i = 0; i < weaponAbbreviations.size(); ++i) {
			if (totalShotsPerWeapon[i] > 0) {
				accuracyPerWeapon[i] =
					(static_cast<double>(totalHitsPerWeapon[i]) / totalShotsPerWeapon[i]) * 100.0;
			}
			else {
				accuracyPerWeapon[i] = 0.0;
			}
		}
	}

	// Function to calculate the Kill-Death Ratio (KDR)
	void calculateKDR() {
		if (totalDeaths > 0) {
			totalKDR = static_cast<double>(totalKills) / totalDeaths;
		}
		else if (totalKills > 0) {
			totalKDR = static_cast<double>(totalKills);  // Infinite KDR represented as kills
		}
		else {
			totalKDR = 0.0;  // No kills, no deaths
		}
	}

	/*
	=============
	PlayerStats::toJson
	
	Serializes the per-player statistics into a JSON object for export.
	=============
	*/
	json toJson() const {
		json result;
		result["socialID"] = socialID;
		const std::string& identifier = !socialID.empty() ? socialID : playerName;
		result["playerIdentifier"] = identifier;
		result["playerName"] = playerName;
		result["totalScore"] = totalScore;
		if (proBallGoals > 0)    result["proBallGoals"] = proBallGoals;
		if (proBallAssists > 0) result["proBallAssists"] = proBallAssists;

		if (totalKills > 0)        result["totalKills"] = totalKills;
		if (totalSpawnKills > 0)   result["totalSpawnKills"] = totalSpawnKills;
		if (totalTeamKills > 0)    result["totalTeamKills"] = totalTeamKills; // fixed
		if (totalDeaths > 0)       result["totalDeaths"] = totalDeaths;
		if (totalSuicides > 0)     result["totalSuicides"] = totalSuicides;
		if (ctfFlagPickups > 0)     result["ctfFlagPickups"] = ctfFlagPickups;
		if (ctfFlagDrops > 0)     result["ctfFlagDrops"] = ctfFlagDrops;
		if (ctfFlagReturns > 0)     result["ctfFlagReturns"] = ctfFlagReturns;
		if (ctfFlagAssists > 0)     result["ctfFlagAssists"] = ctfFlagAssists;
		if (ctfFlagCaptures > 0)     result["ctfFlagCaptures"] = ctfFlagCaptures;
		if (ctfFlagCarrierTimeTotalMsec > 0)     result["ctfFlagCarrierTimeTotalMsec"] = Json::Int64(ctfFlagCarrierTimeTotalMsec);
		if (ctfFlagCarrierTimeShortestMsec > 0)     result["ctfFlagCarrierTimeShortestMsec"] = ctfFlagCarrierTimeShortestMsec;
		if (ctfFlagCarrierTimeLongestMsec > 0)     result["ctfFlagCarrierTimeLongestMsec"] = ctfFlagCarrierTimeLongestMsec;
		if (totalKDR > 0.0)        result["totalKDR"] = totalKDR;
		if (totalHits > 0)         result["totalHits"] = totalHits;
		if (totalShots > 0)        result["totalShots"] = totalShots;
		if (totalAccuracy > 0.0)   result["totalAccuracy"] = totalAccuracy;
		if (totalDmgDealt > 0)     result["totalDmgDealt"] = totalDmgDealt;
		if (totalDmgReceived > 0)  result["totalDmgReceived"] = totalDmgReceived;
		if (ratingChange != 0)     result["ratingChange"] = ratingChange;
		if (playTimeMsec > 0)      result["playTime"] = playTimeMsec;
		if (killsPerMinute > 0)    result["killsPerMinute"] = killsPerMinute;
		if (skillRating > 0)       result["skillRating"] = skillRating;
		if (skillRatingChange != 0) result["skillRatingChange"] = skillRatingChange;

		json shotsJson, hitsJson, accuracyJson;
		for (size_t i = 0; i < weaponAbbreviations.size(); ++i) {
			const auto& weaponName = weaponAbbreviations[i];
			if (totalShotsPerWeapon[i] > 0)
				shotsJson[weaponName] = totalShotsPerWeapon[i];
			if (totalHitsPerWeapon[i] > 0)
				hitsJson[weaponName] = totalHitsPerWeapon[i];
			if (accuracyPerWeapon[i] > 0.0)
				accuracyJson[weaponName] = accuracyPerWeapon[i];
		}
		if (!shotsJson.empty())    result["totalShotsPerWeapon"] = shotsJson;
		if (!hitsJson.empty())     result["totalHitsPerWeapon"] = hitsJson;
		if (!accuracyJson.empty()) result["accuracyPerWeapon"] = accuracyJson;

		json modKillsJson, modDeathsJson, modKDRJson, modDmgDJson, modDmgRJson;
		for (const auto& mod : modr) {
			const size_t idx = static_cast<size_t>(mod.mod);
			if (modTotalKills[idx] > 0) modKillsJson[mod.name] = modTotalKills[idx];
			if (modTotalDeaths[idx] > 0) modDeathsJson[mod.name] = modTotalDeaths[idx];
			if (modTotalKDR[idx] > 0.0) modKDRJson[mod.name] = modTotalKDR[idx];
			if (modTotalDmgD[idx] > 0)  modDmgDJson[mod.name] = modTotalDmgD[idx];
			if (modTotalDmgR[idx] > 0)  modDmgRJson[mod.name] = modTotalDmgR[idx];
		}
		if (!modKillsJson.empty()) result["totalKillsByMOD"] = modKillsJson;
		if (!modDeathsJson.empty()) result["totalDeathsByMOD"] = modDeathsJson;
		if (!modKDRJson.empty())    result["totalKDRByMOD"] = modKDRJson;
		if (!modDmgDJson.empty())   result["totalDmgDByMOD"] = modDmgDJson;
		if (!modDmgRJson.empty())   result["totalDmgRByMOD"] = modDmgRJson;

		json pickupsJson;
		json pickupDelayJson;
		for (int i = static_cast<size_t>(HighValueItems::None) + 1; i < static_cast<size_t>(HighValueItems::Total); ++i) {
			auto item = i;
			if (pickupCounts[item] > 0)
				pickupsJson[HighValueItemNames[item]] = pickupCounts[item];
			if (pickupDelays[item] > 0.0)
				pickupDelayJson[HighValueItemNames[item]] = pickupDelays[item];
		}
		if (!pickupsJson.empty())
			result["pickupCounts"] = std::move(pickupsJson);
		if (!pickupDelayJson.empty())
			result["pickupDelays"] = std::move(pickupDelayJson);

		if (JsonHasData(gametypeStats))
			result["gametype"] = gametypeStats;

		return result;
}
};

struct TeamStats {
	std::string teamName;          // Team name or identifier
	int score = 0;                 // Team score
	std::string outcome;           // "win", "loss", or "draw"
	std::vector<PlayerStats> players; // Players on the team

	// Generate JSON object for this team's stats
	json toJson() const {
		json teamJson;
		teamJson["teamName"] = teamName;
		teamJson["score"] = score;
		teamJson["outcome"] = outcome;
		teamJson["players"] = json(Json::arrayValue);
		for (const auto& player : players) {
			teamJson["players"].append(player.toJson());
		}
		return teamJson;
	}
};

struct MatchStats {
	std::string matchID;           // Unique match identifier
	std::string serverName;          // Server name
	std::string serverHostName;    // Name of the server host
	std::string gameType;          // Game type (e.g., "FFA", "TDM")
	std::string ruleSet;
	std::string mapName;           // Name of the map
	bool ranked = false;
	int64_t matchStartMS = 0;
	int64_t matchEndMS = 0;
	bool wasTeamMode = false;
	GameFlags recordedFlags = GameFlags::None;
	int totalKills = 0;            // Total kills in the match
	int totalSpawnKills = 0;        // Total spawn kills in the match
	int totalTeamKills = 0;        // Total team kills in the match
	int totalDeaths = 0;            // Total deaths in the match
	int totalSuicides = 0;            // Total suicides in the match
	int proBall_totalGoals = 0;
	int proBall_totalAssists = 0;
	double avKillsPerMinute = 0;
	int ctf_totalFlagsCaptured = 0;    // Flags captured (for CTF)
	int ctf_totalFlagAssists = 0;    // Flag assists (for CTF)
	int ctf_totalFlagDefends = 0;    // Flag defends (for CTF)
	std::map<std::string, int>    totalKillsByMOD;
	std::map<std::string, int>    totalDeathsByMOD;
	std::map<std::string, double> totalKDRByMOD;
	int64_t durationMS = 0;         // Match duration in msec
	std::vector<PlayerStats> players; // Individual player stats (for FFA/Duel)
	std::vector<TeamStats> teams;  // Team stats (for TDM/CTF)
	json gametypeStats;
	int timeLimitSeconds = 0;
	int scoreLimit = 0;
	std::vector<MatchEvent> eventLog;
	std::vector<MatchDeathEvent> deathLog;
#if 0
	// Convert time_t to ISO 8601 string
	std::string formatTime(std::time_t time) const {
		std::ostringstream oss;
		oss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
		return oss.str();
	}
#endif
	std::string formatTime(int64_t msec) const {
		time_t t = static_cast<time_t>(msec / 1000);
		if (!t || t <= 0) return "n/a";

		std::tm* tm = std::gmtime(&t);
		if (!tm) return "invalid";

		char buf[64];
		if (std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm)) {
			return buf;
		}
		return "error";
	}

		// Calculate duration based on start and end times
		void calculateDuration() {
			if (matchEndMS > matchStartMS && matchStartMS > 0) {
				durationMS = matchEndMS - matchStartMS;
			}
			else if (matchEndMS > 0 && matchStartMS == 0) {
				durationMS = matchEndMS;
			}
			else {
				durationMS = 0;
			}
		}

	/*
	=============
	MatchStats::toJson
	
	Serializes the collected match-wide statistics to JSON.
	=============
	*/
	json toJson() const {
		const bool hadTeams = wasTeamMode && teams.size() >= 2;
		json matchJson;
		matchJson["matchID"] = matchID;
		matchJson["serverName"] = serverName;
		if (!serverHostName.empty()) {
			matchJson["serverHostName"] = serverHostName;
		}
		matchJson["gameType"] = gameType;
		matchJson["ruleSet"] = ruleSet;
		matchJson["mapName"] = mapName;
		matchJson["matchRanked"] = ranked;
		matchJson["totalKills"] = totalKills;
		matchJson["totalSpawnKills"] = totalSpawnKills;
		matchJson["totalTeamKills"] = totalTeamKills;
		matchJson["totalDeaths"] = totalDeaths;
		matchJson["totalSuicides"] = totalSuicides;
		if (proBall_totalGoals > 0)
			matchJson["totalGoals"] = proBall_totalGoals;
		if (proBall_totalAssists > 0)
			matchJson["totalGoalAssists"] = proBall_totalAssists;
		matchJson["avKillsPerMinute"] = avKillsPerMinute;
		matchJson["totalFlagsCaptured"] = ctf_totalFlagsCaptured;
		matchJson["totalFlagAssists"] = ctf_totalFlagAssists;
		matchJson["totalFlagDefends"] = ctf_totalFlagDefends;
		// NOTE: Exporters intentionally rely on frozen timestamps captured at match end.
		matchJson["matchStartMS"] = matchStartMS;
		matchJson["matchEndMS"] = matchEndMS;
		matchJson["matchTimeDuration"] = durationMS;
		matchJson["timeLimitSeconds"] = timeLimitSeconds;
		matchJson["scoreLimit"] = scoreLimit;
		matchJson["players"] = json(Json::arrayValue);
		if (hadTeams) {
			matchJson["teams"] = json(Json::arrayValue);
		}

		// Add player stats for FFA or Duel
		for (const auto& player : players) {
			matchJson["players"].append(player.toJson());
		}

		if (hadTeams) {
			// Add team stats for team-based modes
			for (const auto& team : teams) {
				matchJson["teams"].append(team.toJson());
			}
		}

		if (JsonHasData(gametypeStats))
			matchJson["gametype"] = gametypeStats;

		if (!eventLog.empty()) {
			json eventArray = json(Json::arrayValue);
			for (const auto& entry : eventLog) {
				json eventJson;
				eventJson["time"] = Json::Int64(entry.time.seconds<int64_t>());
				eventJson["event"] = entry.eventStr;
				eventArray.append(eventJson);
			}
			matchJson["eventLog"] = std::move(eventArray);
		}

		if (!deathLog.empty()) {
			json dlog = json(Json::arrayValue);
			for (const auto& e : deathLog) {
				json entry;
				entry["time"] = Json::Int64(e.time.seconds<int64_t>());
				entry["victim"]["name"] = e.victim.name;
				entry["victim"]["id"] = e.victim.id;
				entry["attacker"]["name"] = e.attacker.name;
				entry["attacker"]["id"] = e.attacker.id;
				entry["mod"] = modr[static_cast<int>(e.mod.id)].name;
				dlog.append(entry);
			}
			matchJson["deathLog"] = std::move(dlog);
		}

		return matchJson;
}
};
MatchStats matchStats;

struct TournamentSeriesSnapshot {
	std::string seriesId;
	std::string name;
	int bestOf = 1;
	int winTarget = 1;
	bool teamBased = false;
	GameType gametype = GameType::None;
	std::vector<json> matches;
};

static std::unordered_map<std::string, TournamentSeriesSnapshot> g_tournamentSeries;

static std::atomic<uint64_t> g_matchStatsNextJobID{ 1 };
static std::atomic<uint32_t> g_matchStatsPendingJobs{ 0 };
static std::atomic<uint32_t> g_matchStatsCompletedJobs{ 0 };
static std::atomic<uint32_t> g_matchStatsFailedJobs{ 0 };

struct MatchStatsWorkerJob {
	uint64_t	jobId = 0;
	MatchStats	stats;
	std::string	baseFilePath;
};

static std::mutex			g_matchStatsWorkerMutex;
static std::condition_variable	g_matchStatsWorkerCondition;
static std::queue<MatchStatsWorkerJob>	g_matchStatsWorkerQueue;
static std::once_flag		g_matchStatsWorkerOnceFlag;

static void MatchStatsWorker_ThreadMain();
static void MatchStatsWorker_EnsureStarted();
static uint64_t MatchStatsWorker_Enqueue(MatchStats&& stats, std::string baseFilePath);

/*
=============
ValidateModTotals

Ensures aggregated MOD totals line up with the recorded match totals.
=============
*/
static void ValidateModTotals(const MatchStats& matchStats) {
	uint32_t aggregatedKillSum = 0;
	for (const auto& [modName, kills] : matchStats.totalKillsByMOD) {
		if (kills <= 0)
			continue;

		aggregatedKillSum += static_cast<uint32_t>(kills);
	}

	uint32_t aggregatedDeathSum = 0;
	for (const auto& [modName, deaths] : matchStats.totalDeathsByMOD) {
		if (deaths <= 0)
			continue;

		aggregatedDeathSum += static_cast<uint32_t>(deaths);
	}

	const uint32_t declaredKills = static_cast<uint32_t>(std::max(0, matchStats.totalKills));
	const uint32_t declaredDeaths = static_cast<uint32_t>(std::max(0, matchStats.totalDeaths));

	if (aggregatedKillSum != declaredKills) {
		gi.Com_PrintFmt("{}: totalKillsByMOD mismatch ({} != {})\n", __FUNCTION__, aggregatedKillSum, declaredKills);
	}

	if (aggregatedDeathSum != declaredDeaths) {
		gi.Com_PrintFmt("{}: totalDeathsByMOD mismatch ({} != {})\n", __FUNCTION__, aggregatedDeathSum, declaredDeaths);
	}
}

static bool MatchStats_WriteJson(const MatchStats& matchStats, const std::string& fileName) {
	try {
		WriteFileAtomically(fileName, [&](std::ofstream& file) {
			Json::StreamWriterBuilder writer;
			writer["indentation"] = "    "; // match original .dump(4)
			std::string output = Json::writeString(writer, matchStats.toJson());
			file << output;
		});
		gi.Com_PrintFmt("Match JSON written to {}\n", fileName.c_str());
		return true;
	}
	catch (const std::exception& e) {
		gi.Com_PrintFmt("Exception while writing JSON ({}): {}\n", fileName.c_str(), e.what());
	}
	catch (...) {
		gi.Com_PrintFmt("Unknown error while writing JSON ({})\n", fileName.c_str());
	}

	return false;
}

static std::string TournamentSeriesFileId(std::string_view seriesId) {
	std::string sanitized = SanitizeSocialID(seriesId);
	if (sanitized.empty())
		return "series";
	return sanitized;
}

static const char* TournamentTeamKey(Team team) {
	switch (team) {
	case Team::Red:
		return "red";
	case Team::Blue:
		return "blue";
	case Team::Free:
		return "free";
	case Team::Spectator:
		return "spectator";
	default:
		return "none";
	}
}

static int TournamentPlayerWinsForId(std::string_view id) {
	if (id.empty())
		return 0;

	for (size_t i = 0; i < game.tournament.playerIds.size(); ++i) {
		if (game.tournament.playerIds[i] == id)
			return game.tournament.playerWins[i];
	}

	return 0;
}

static json TournamentSeries_BuildJson(const TournamentSeriesSnapshot& series) {
	json seriesJson;
	seriesJson["seriesId"] = series.seriesId;
	if (!series.name.empty())
		seriesJson["name"] = series.name;
	seriesJson["bestOf"] = series.bestOf;
	seriesJson["winTarget"] = series.winTarget;
	seriesJson["gametype"] = std::string(Game::GetInfo(series.gametype).short_name_upper);

	seriesJson["matches"] = json(Json::arrayValue);
	for (const auto& matchJson : series.matches) {
		seriesJson["matches"].append(matchJson);
	}

	seriesJson["mapPool"] = json(Json::arrayValue);
	for (const auto& map : game.tournament.mapPool) {
		seriesJson["mapPool"].append(map);
	}

	seriesJson["mapBans"] = json(Json::arrayValue);
	for (const auto& map : game.tournament.mapBans) {
		seriesJson["mapBans"].append(map);
	}

	seriesJson["mapPicks"] = json(Json::arrayValue);
	for (const auto& map : game.tournament.mapPicks) {
		seriesJson["mapPicks"].append(map);
	}

	seriesJson["mapOrder"] = json(Json::arrayValue);
	for (const auto& map : game.tournament.mapOrder) {
		seriesJson["mapOrder"].append(map);
	}

	seriesJson["participants"] = json(Json::arrayValue);
	for (const auto& participant : game.tournament.participants) {
		json entry;
		entry["id"] = participant.socialId;
		if (!participant.name.empty())
			entry["name"] = participant.name;
		entry["team"] = TournamentTeamKey(participant.lockedTeam);
		entry["wins"] = TournamentPlayerWinsForId(participant.socialId);
		seriesJson["participants"].append(entry);
	}

	if (game.tournament.teamBased) {
		json teamsJson;
		json redJson;
		json blueJson;
		redJson["name"] = Teams_TeamName(Team::Red);
		blueJson["name"] = Teams_TeamName(Team::Blue);
		redJson["wins"] = game.tournament.teamWins[static_cast<size_t>(Team::Red)];
		blueJson["wins"] = game.tournament.teamWins[static_cast<size_t>(Team::Blue)];
		teamsJson["red"] = std::move(redJson);
		teamsJson["blue"] = std::move(blueJson);
		seriesJson["teams"] = std::move(teamsJson);
	}

	return seriesJson;
}

static bool TournamentSeries_WriteJson(const TournamentSeriesSnapshot& series, const std::string& fileName) {
	try {
		WriteFileAtomically(fileName, [&](std::ofstream& file) {
			Json::StreamWriterBuilder writer;
			writer["indentation"] = "    ";
			std::string output = Json::writeString(writer, TournamentSeries_BuildJson(series));
			file << output;
		});
		gi.Com_PrintFmt("Tournament series JSON written to {}\n", fileName.c_str());
		return true;
	}
	catch (const std::exception& e) {
		gi.Com_PrintFmt("Exception while writing series JSON ({}): {}\n", fileName.c_str(), e.what());
	}
	catch (...) {
		gi.Com_PrintFmt("Unknown error while writing series JSON ({})\n", fileName.c_str());
	}

	return false;
}

static bool TournamentSeries_WriteHtml(const TournamentSeriesSnapshot& series, const std::string& fileName) {
	try {
		WriteFileAtomically(fileName, [&](std::ofstream& html) {
			const std::string title = series.name.empty() ? series.seriesId : series.name;
			html << "<!DOCTYPE html>\n<html lang=\"en\"><head><meta charset=\"UTF-8\">\n";
			html << "<title>Tournament Series - " << HtmlEscape(title) << "</title>\n";
			html << "<style>body{font-family:Arial,sans-serif;background:#f4f4f4;margin:0;padding:20px;}";
			html << "h1,h2{margin:0 0 10px;}table{width:100%;border-collapse:collapse;background:#fff;}";
			html << "th,td{padding:8px 10px;border:1px solid #ddd;text-align:left;}</style>";
			html << "</head><body>\n";
			html << "<h1>Tournament Series</h1>\n";
			html << "<p><strong>Series ID:</strong> " << HtmlEscape(series.seriesId) << "</p>\n";
			if (!series.name.empty())
				html << "<p><strong>Name:</strong> " << HtmlEscape(series.name) << "</p>\n";
			html << "<p><strong>Best Of:</strong> " << series.bestOf << "</p>\n";
			html << "<p><strong>Gametype:</strong> "
				 << HtmlEscape(Game::GetInfo(series.gametype).long_name) << "</p>\n";

			if (series.teamBased) {
				html << "<h2>Teams</h2>\n<table>\n<tr><th>Team</th><th>Wins</th></tr>\n";
				html << "<tr><td>" << HtmlEscape(Teams_TeamName(Team::Red)) << "</td><td>"
					 << game.tournament.teamWins[static_cast<size_t>(Team::Red)] << "</td></tr>\n";
				html << "<tr><td>" << HtmlEscape(Teams_TeamName(Team::Blue)) << "</td><td>"
					 << game.tournament.teamWins[static_cast<size_t>(Team::Blue)] << "</td></tr>\n";
				html << "</table>\n";
			}
			else {
				html << "<h2>Players</h2>\n<table>\n<tr><th>Player</th><th>Wins</th></tr>\n";
				for (const auto& participant : game.tournament.participants) {
					html << "<tr><td>" << HtmlEscape(participant.name.empty() ? participant.socialId : participant.name)
						 << "</td><td>" << TournamentPlayerWinsForId(participant.socialId) << "</td></tr>\n";
				}
				html << "</table>\n";
			}

			if (!game.tournament.mapOrder.empty()) {
				html << "<h2>Map Order</h2>\n<ol>\n";
				for (const auto& map : game.tournament.mapOrder) {
					html << "<li>" << HtmlEscape(map) << "</li>\n";
				}
				html << "</ol>\n";
			}

			html << "<h2>Matches</h2>\n<ol>\n";
			for (const auto& matchJson : series.matches) {
				const std::string matchId = matchJson["matchID"].asString();
				const std::string mapName = matchJson["mapName"].asString();
				html << "<li>" << HtmlEscape(mapName) << " - <a href=\""
					 << HtmlEscape(matchId) << ".html\">" << HtmlEscape(matchId) << "</a></li>\n";
			}
			html << "</ol>\n</body></html>\n";
		});
		gi.Com_PrintFmt("Tournament series HTML written to {}\n", fileName.c_str());
		return true;
	}
	catch (const std::exception& e) {
		gi.Com_PrintFmt("Exception while writing series HTML ({}): {}\n", fileName.c_str(), e.what());
	}
	catch (...) {
		gi.Com_PrintFmt("Unknown error while writing series HTML ({})\n", fileName.c_str());
	}

	return false;
}

static bool TournamentSeries_WriteAll(const TournamentSeriesSnapshot& series, const std::string& baseFilePath) {
	const std::filesystem::path basePath(baseFilePath);
	const std::filesystem::path directory = basePath.parent_path();

	if (!directory.empty()) {
		std::error_code dirError;
		std::filesystem::create_directories(directory, dirError);
		if (dirError) {
			gi.Com_PrintFmt("{}: Failed to create directory '{}': {}\n", __FUNCTION__, directory.string().c_str(), dirError.message().c_str());
			return false;
		}
	}

	const bool jsonWritten = TournamentSeries_WriteJson(series, baseFilePath + ".json");
	bool htmlWritten = true;
	if (g_statex_export_html->integer) {
		htmlWritten = TournamentSeries_WriteHtml(series, baseFilePath + ".html");
	}
	else {
		gi.Com_PrintFmt("{}: HTML export disabled via g_statex_export_html.\n", __FUNCTION__);
	}

	if (!jsonWritten || !htmlWritten) {
		gi.Com_PrintFmt("{}: Series export completed with errors (JSON: {}, HTML: {})\n", __FUNCTION__, jsonWritten ? "ok" : "failed", htmlWritten ? "ok" : "failed");
	}

	return jsonWritten && htmlWritten;
}



/*
=============
Html_WriteHeader
=============
*/
static inline void Html_WriteHeader(std::ofstream& html, const MatchStats& matchStats) {
	const std::string escapedMatchId = HtmlEscape(matchStats.matchID);
	html << R"(<!DOCTYPE html>
<html lang="en"><head><meta charset="UTF-8">
<title>Match Summary - )" << escapedMatchId << R"(</title>
<style>
  body { font-family:Arial,sans-serif; background:#f4f4f4; margin:0; padding:20px; }
  .top-info {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
    gap: 10px;
    background:#fff;
    padding:20px;
    border-radius:8px;
    box-shadow:0 2px 4px rgba(0,0,0,0.1);
    margin-bottom:20px;
  }
  .top-info h1 {
    grid-column:1 / -1;
    font-size:1.8em;
    margin:0 0 10px;
  }
  .top-info p {
    margin:0;
    font-size:0.9em;
    color:#555;
  }
.progress-cell {
  position: relative;
  background: #eee;
//  border-radius: 4px;
  overflow: hidden;
}
.progress-cell .bar {
  position: absolute;
  top: 0; left: 0; bottom: 0;
  background: rgba(0,100,0,0.2);
}
.progress-cell.red .bar { background: rgba(200,0,0,0.3); }
.progress-cell.blue .bar { background: rgba(0,0,200,0.3); }
.progress-cell.green .bar { background: rgba(0,100,0,0.3); }
.player-cell {
  border-left: 6px solid transparent;
  padding-left: 6px;
}
.player-cell.red { border-color: #c00; }
.player-cell.blue { border-color: #00c; }
.player-cell.green { border-color: #060; }

.section.team-red {
  border: 2px solid #c00;
}
.section.team-blue {
  border: 2px solid #00c;
}
.team-score-header {
  font-size: 1.8em;
  font-weight: bold;
  text-align: center;
  margin: 20px 0;
}
.team-score-header span {
  padding: 4px 10px;
  border-radius: 8px;
  color: #fff;
}
.team-score-header .red {
  background: #c00;
}
.team-score-header .blue {
  background: #00c;
}
.player-name.red { color: #c00; font-weight: bold; }
.player-name.blue { color: #00c; font-weight: bold; }
.player-name.green { color: #060; font-weight: bold; }
.player-cell.red { border-left: 6px solid #c00; padding-left: 6px; }
.player-cell.blue { border-left: 6px solid #00c; padding-left: 6px; }
.player-cell.green { border-left: 6px solid #060; padding-left: 6px; }
.player-section.red { border-left: 6px solid #c00; padding-left: 8px; margin-bottom: 16px; }
.player-section.blue { border-left: 6px solid #00c; padding-left: 8px; margin-bottom: 16px; }
.player-section.green { border-left: 6px solid #060; padding-left: 8px; margin-bottom: 16px; }

.winner.red {
  color: #c00;
}
.winner.blue {
  color: #00c;
}
.progress-cell span {
  position: relative;
  padding: 0 4px;
  z-index: 1;
}
.flex-container {
	display: flex;
	flex-wrap: wrap;
	gap: 24px;
	margin-top: 12px;
}

.flex-item {
	flex: 1;
	min-width: 320px;
}
  .section { background:#fff; padding:15px; margin-bottom:20px; border-radius:5px; box-shadow:0 1px 3px rgba(0,0,0,0.1); }
  .overall { border:2px solid #006400; }
  table { width:100%; border-collapse:collapse; margin-top:10px; }
  th,td { border:1px solid #ccc; padding:8px; text-align:left; }
  th { background:#eee; }
  .winner { font-size:1.5em; font-weight:bold; color:#006400; text-align:center; margin-bottom:10px; }
  .footer { font-size:0.8em; color:#666; text-align:right; }
</style>
</head><body>
)";
}

/*
=============
Html_WriteTopInfo
=============
*/
static inline void Html_WriteTopInfo(std::ofstream& html, const MatchStats& matchStats) {
	const bool proBall = Q_strcasecmp(matchStats.gameType.c_str(), "PROBALL") == 0;
	const std::string escapedMatchId = HtmlEscape(matchStats.matchID);
	const std::string escapedServerName = HtmlEscape(matchStats.serverName);
	const std::string escapedGameType = HtmlEscape(matchStats.gameType);
	const std::string escapedMapName = HtmlEscape(matchStats.mapName);
	// NOTE: HTML exports intentionally render frozen timestamps captured when the match ended.
	html << "<div class=\"top-info\">\n"
		<< "  <h1>Match Summary - " << escapedMatchId << "</h1>\n"
		<< "  <p><strong>Server:</strong> " << escapedServerName << "</p>\n"
		<< "  <p><strong>Type:</strong> " << escapedGameType << "</p>\n"
		<< "  <p><strong>Start:</strong> " << matchStats.formatTime(matchStats.matchStartMS) << " UTC</p>\n"
		<< "  <p><strong>End:</strong>   " << matchStats.formatTime(matchStats.matchEndMS) << " UTC</p>\n"
		<< "  <p><strong>Map:</strong>  " << escapedMapName << "</p>\n"
		<< "  <p><strong>Score Limit:</strong> " << matchStats.scoreLimit << "</p>\n";
	// Time Limit
	{
		int t_secs = matchStats.timeLimitSeconds;
		int h = t_secs / 3600;
		int m = (t_secs % 3600) / 60;
		int s = t_secs % 60;
		html << "  <p><strong>Time Limit:</strong> ";
		if (h > 0)      html << h << "h " << m << "m " << s << "s";
		else if (m > 0) html << m << "m " << s << "s";
		else            html << s << "s";
		html << "</p>\n";
	}
	// Duration
	html << "  <p><strong>Duration:</strong> ";
	{
		int secs = matchStats.durationMS / 1000;
		int h = secs / 3600;
		int m = (secs % 3600) / 60;
		int s = secs % 60;
		if (h > 0)      html << h << "h " << m << "m " << s << "s";
		else if (m > 0) html << m << "m " << s << "s";
		else            html << s << "s";
	}
	html << "</p>\n";
	if (proBall) {
		html << "  <p><strong>Total Goals:</strong> " << matchStats.proBall_totalGoals << "</p>\n"
			<< "  <p><strong>Total Assists:</strong> " << matchStats.proBall_totalAssists << "</p>\n";
	}
	html << "</div>\n";
}


/*
=============
Html_WriteWinnerSummary
=============
*/
static inline void Html_WriteWinnerSummary(std::ofstream& html, const MatchStats& matchStats) {
	std::string winner;
	std::string winnerClass;

	if (!matchStats.teams.empty()) {
		auto& t0 = matchStats.teams[0];
		auto& t1 = matchStats.teams[1];
		if (t0.score > t1.score) {
			winner = t0.teamName;
			winnerClass = (winner == "Red") ? "red" : "blue";
		}
		else {
			winner = t1.teamName;
			winnerClass = (winner == "Red") ? "red" : "blue";
		}
	}
	else if (!matchStats.players.empty()) {
		const auto* best = &matchStats.players.front();
		for (auto& p : matchStats.players)
			if (p.totalScore > best->totalScore) best = &p;
		winner = best->playerName;
	}

	const std::string escapedWinner = HtmlEscape(winner);
	html << "<div class=\"winner";
	if (!winnerClass.empty()) {
		html << ' ' << winnerClass;
	}
	html << "\">Winner: " << escapedWinner << "</div>\\n";
}


/*
=============
Html_WriteOverallScores
=============
*/
static inline void Html_WriteOverallScores(std::ofstream& html, const MatchStats& matchStats, std::vector<const PlayerStats*> allPlayers) {
	const bool proBall = Q_strcasecmp(matchStats.gameType.c_str(), "PROBALL") == 0;
	html << "<div class=\"section overall\">\n"
		<< "  <h2>Overall Scores</h2>\n"
		<< "  <table>\n"
		<< "    <tr>"
		"<th title=\"Player's in-game name (click to jump)\">Player</th>"
		"<th title=\"Percentage of match time played\">%TIME</th>"
		"<th title=\"Skill Rating (and change from match)\">SR</th>"
		"<th title=\"Kill-Death Ratio (Kills / Deaths)\">KDR</th>"
		"<th title=\"Kills Per Minute (Kills / Minutes Played)\">KPM</th>"
		"<th title=\"Damage Ratio (Damage Dealt / Damage Received)\">DMR</th>"
		"<th>Score</th>";
	if (proBall) {
		html << "<th title=\"Goals scored\">GO</th>"
			<< "<th title=\"Goal assists credited\">AS</th>";
	}
	html << "</tr>\n";

	int maxSR = 0, maxScore = 0;
	double maxKDR = 0.0, maxKPM = 0.0, maxDMR = 0.0;
	int maxGoals = 0, maxAssists = 0;

	for (auto* p : allPlayers) {
		maxSR = std::max(maxSR, p->skillRating);
		maxScore = std::max(maxScore, p->totalScore);

		const double kdr = p->totalDeaths ? double(p->totalKills) / p->totalDeaths : double(p->totalKills);
		const double kpm = (p->playTimeMsec > 0) ? (p->totalKills * 60.0) / (p->playTimeMsec / 1000.0) : 0.0;
		const double dmr = p->totalDmgReceived ? double(p->totalDmgDealt) / p->totalDmgReceived : double(p->totalDmgDealt);

		maxKDR = std::max(maxKDR, kdr);
		maxKPM = std::max(maxKPM, kpm);
		maxDMR = std::max(maxDMR, dmr);
		if (proBall) {
			maxGoals = std::max(maxGoals, p->proBallGoals);
			maxAssists = std::max(maxAssists, p->proBallAssists);
		}
	}

	const double durationMin = matchStats.durationMS / 60000.0;

	for (auto* p : allPlayers) {
		const double kdr = p->totalDeaths ? double(p->totalKills) / p->totalDeaths
			: (p->totalKills ? double(p->totalKills) : 0.0);
		const double kpm = (durationMin > 0.0) ? p->totalKills / durationMin : 0.0;
		const double dmr = p->totalDmgReceived ? double(p->totalDmgDealt) / p->totalDmgReceived
			: (p->totalDmgDealt ? double(p->totalDmgDealt) : 0.0);
		const int tp = (p->playTimeMsec > 0) ? p->playTimeMsec : matchStats.durationMS;

		const std::string escapedSocialId = HtmlEscape(p->socialID);
		const std::string escapedPlayerName = HtmlEscape(p->playerName);
		html << "    <tr><td title=\"" << escapedSocialId << "\">"
			<< "<a href=\"#player-" << escapedSocialId << "\">" << escapedPlayerName << "</a></td>";

		const double pctTime = (tp > 0) ? (tp / matchStats.durationMS) * 100.0 : 0.0;
		html << "<td class=\"progress-cell\" title=\"% of match time\">"
			<< "<div class=\"bar\" style=\"width:" << pctTime << "%\"></div>"
			<< "<span>" << std::fixed << std::setprecision(1) << pctTime << "%</span></td>";

		// Skill Rating
		const double pctSR = (maxSR > 0) ? (double(p->skillRating) / maxSR) * 100.0 : 0.0;
		html << "<td class=\"progress-cell\" title=\"Skill Rating change from match: "
			<< (p->skillRatingChange >= 0 ? "+" : "") << p->skillRatingChange << "\">"
			<< "<div class=\"bar\" style=\"width:" << pctSR << "%\"></div><span>"
			<< p->skillRating;
		if (p->skillRatingChange != 0) {
			html << " (" << (p->skillRatingChange >= 0 ? "+" : "") << p->skillRatingChange << ")";
		}
		html << "</span></td>";

		const double pctKDR = (maxKDR > 0) ? (kdr / maxKDR) * 100.0 : 0.0;
		html << "<td class=\"progress-cell\" title=\"Kills: " << p->totalKills << ", Deaths: " << p->totalDeaths << "\">"
			<< "<div class=\"bar\" style=\"width:" << pctKDR << "%\"></div>"
			<< "<span>" << std::fixed << std::setprecision(2) << kdr << "</span></td>";

		const double pctKPM = (maxKPM > 0) ? (kpm / maxKPM) * 100.0 : 0.0;
		html << "<td class=\"progress-cell\" title=\"Kills: " << p->totalKills << ", Min: " << int(tp / 60) << "\">"
			<< "<div class=\"bar\" style=\"width:" << pctKPM << "%\"></div>"
			<< "<span>" << std::fixed << std::setprecision(2) << kpm << "</span></td>";

		const double pctDMR = (maxDMR > 0) ? (dmr / maxDMR) * 100.0 : 0.0;
		html << "<td class=\"progress-cell\" title=\"DmgD: " << p->totalDmgDealt << ", DmgR: " << p->totalDmgReceived << "\">"
			<< "<div class=\"bar\" style=\"width:" << pctDMR << "%\"></div>"
			<< "<span>" << std::fixed << std::setprecision(2) << dmr << "</span></td>";

		const double pctScore = (maxScore > 0) ? (double(p->totalScore) / maxScore) * 100.0 : 0.0;
		html << "<td class=\"progress-cell\" title=\"Score relative to top (" << maxScore << ")\">"
			<< "<div class=\"bar\" style=\"width:" << pctScore << "%\"></div>"
			<< "<span>" << p->totalScore << "</span></td>";

		if (proBall) {
			const double pctGoals = (maxGoals > 0) ? (double(p->proBallGoals) / maxGoals) * 100.0 : 0.0;
			html << "<td class=\"progress-cell\" title=\"Goals scored\">"
				<< "<div class=\"bar\" style=\"width:" << pctGoals << "%\"></div>"
				<< "<span>" << p->proBallGoals << "</span></td>";

			const double pctAssists = (maxAssists > 0) ? (double(p->proBallAssists) / maxAssists) * 100.0 : 0.0;
			html << "<td class=\"progress-cell\" title=\"Goal assists credited\">"
				<< "<div class=\"bar\" style=\"width:" << pctAssists << "%\"></div>"
				<< "<span>" << p->proBallAssists << "</span></td>";
		}

		html << "</tr>\n";
	}

	html << "  </table>\n</div>\n";
}

/*
=============
Html_WriteTeamScores
=============
*/
static inline void Html_WriteTeamScores(std::ofstream& html,
	const std::vector<const PlayerStats*>& redPlayersOrig,
	const std::vector<const PlayerStats*>& bluePlayersOrig,
	int redScore, int blueScore,
	double matchDuration,
	int maxGlobalScore) {

	// Big header
	html << "<div class=\"team-score-header\">\n"
		<< "<span class=\"red\">" << redScore << "</span> | "
		<< "<span class=\"blue\">" << blueScore << "</span>\n"
		<< "</div>\n";

	std::vector<const PlayerStats*> redPlayers = redPlayersOrig;
	std::vector<const PlayerStats*> bluePlayers = bluePlayersOrig;

	std::sort(redPlayers.begin(), redPlayers.end(), [](const PlayerStats* a, const PlayerStats* b) {
		return a->totalScore > b->totalScore;
		});
	std::sort(bluePlayers.begin(), bluePlayers.end(), [](const PlayerStats* a, const PlayerStats* b) {
		return a->totalScore > b->totalScore;
		});

	auto writeOneTeam = [&](const std::vector<const PlayerStats*>& teamPlayers, const std::string& color, const std::string& teamName, bool isWinner) {
		const std::string escapedTeamName = HtmlEscape(teamName);
		html << "<div class=\"section team-" << color << "\">\n"
			<< "<h2>" << escapedTeamName;
		if (isWinner) html << " (Winner)";
		html << "</h2>\n";

		html << "<table>\n<tr>"
			<< "<th class=\"" << color << "\">Player</th>"
			<< "<th class=\"" << color << "\">%TIME</th>"
			<< "<th class=\"" << color << "\">SR</th>"
			<< "<th class=\"" << color << "\">KDR</th>"
			<< "<th class=\"" << color << "\">KPM</th>"
			<< "<th class=\"" << color << "\">DMR</th>"
			<< "<th class=\"" << color << "\">Score</th>"
			<< "</tr>\n";

		for (auto* p : teamPlayers) {
			const std::string escapedPlayerName = HtmlEscape(p->playerName);
			html << "<tr><td class=\"player-cell " << color << "\">" << escapedPlayerName << "</td>";

			double pctTime = (matchDuration > 0.0) ? (double(p->playTimeMsec) / matchDuration) * 100.0 : 0.0;
			if (pctTime < 1.0) pctTime = 1.0;

			double kdr = (p->totalDeaths > 0) ? (double(p->totalKills) / p->totalDeaths) : double(p->totalKills);
			double kpm = (matchDuration > 0.0) ? (p->totalKills / (matchDuration / 60000.0)) : 0.0;
			double dmr = (p->totalDmgReceived > 0) ? (double(p->totalDmgDealt) / p->totalDmgReceived) : double(p->totalDmgDealt);

			double pctScore = (maxGlobalScore > 0) ? (double(p->totalScore) / maxGlobalScore) * 100.0 : 0.0;
			if (pctScore < 1.0) pctScore = 1.0;

			html << "<td class=\"progress-cell " << color << "\"><div class=\"bar\" style=\"width:" << pctTime << "%\"></div><span>" << std::fixed << std::setprecision(1) << pctTime << "%</span></td>";

			// SR cell with delta
			html << "<td class=\"progress-cell " << color << "\" title=\"Skill Rating change from match: "
				<< (p->skillRatingChange >= 0 ? "+" : "") << p->skillRatingChange << "\">"
				<< "<div class=\"bar\" style=\"width:100%\"></div><span>"
				<< p->skillRating;
			if (p->skillRatingChange != 0) {
				html << " (" << (p->skillRatingChange >= 0 ? "+" : "") << p->skillRatingChange << ")";
			}
			html << "</span></td>";

			html << "<td class=\"progress-cell " << color << "\"><div class=\"bar\" style=\"width:" << std::max(kdr * 10.0, 1.0) << "%\"></div><span>" << std::fixed << std::setprecision(2) << kdr << "</span></td>"
				<< "<td class=\"progress-cell " << color << "\"><div class=\"bar\" style=\"width:" << std::max(kpm * 10.0, 1.0) << "%\"></div><span>" << std::fixed << std::setprecision(2) << kpm << "</span></td>"
				<< "<td class=\"progress-cell " << color << "\"><div class=\"bar\" style=\"width:" << std::max(dmr * 10.0, 1.0) << "%\"></div><span>" << std::fixed << std::setprecision(2) << dmr << "</span></td>"
				<< "<td class=\"progress-cell " << color << "\"><div class=\"bar\" style=\"width:" << pctScore << "%\"></div><span>" << p->totalScore << "</span></td>"
				<< "</tr>\n";
		}

		html << "</table>\n</div>\n";
		};

	bool redWins = redScore > blueScore;
	writeOneTeam(redPlayers, "red", "Red", redWins);
	writeOneTeam(bluePlayers, "blue", "Blue", !redWins);
}

/*
=============
Html_WriteTeamsComparison
=============
*/
static inline void Html_WriteTeamsComparison(std::ofstream& html,
	const std::vector<const PlayerStats*>& redPlayers,
	const std::vector<const PlayerStats*>& bluePlayers,
	double matchDurationMs) {
	html << "<div class=\"section\">\n<h2>Team Comparison</h2>\n<table>\n";

	html << "<tr>"
		<< "<th title=\"Comparison metric\">Metric</th>"
		<< "<th title=\"Red Team\">Red</th>"
		<< "<th title=\"Blue Team\">Blue</th>"
		<< "</tr>\n";

	auto calcTeamStats = [](const std::vector<const PlayerStats*>& players, double matchMinutes) -> std::tuple<double, double, double> {
		int kills = 0, deaths = 0, dmgDealt = 0, dmgTaken = 0;
		for (auto* p : players) {
			kills += p->totalKills;
			deaths += p->totalDeaths;
			dmgDealt += p->totalDmgDealt;
			dmgTaken += p->totalDmgReceived;
		}
		double kdr = deaths > 0 ? double(kills) / deaths : (kills > 0 ? double(kills) : 0.0);
		double kpm = matchMinutes > 0.0 ? double(kills) / matchMinutes : 0.0;
		double dmr = dmgTaken > 0 ? double(dmgDealt) / dmgTaken : (dmgDealt > 0 ? double(dmgDealt) : 0.0);
		return { kdr, kpm, dmr };
		};

	const double matchMinutes = matchDurationMs / 60000.0;

	auto [redKDR, redKPM, redDMR] = calcTeamStats(redPlayers, matchMinutes);
	auto [blueKDR, blueKPM, blueDMR] = calcTeamStats(bluePlayers, matchMinutes);

	auto writeRow = [&](const char* name, const char* tip, double redVal, double blueVal, const char* redTip, const char* blueTip) {
		html << "<tr><td title=\"" << tip << "\">" << name << "</td>"
			<< "<td title=\"" << redTip << "\">" << std::fixed << std::setprecision(2) << redVal << "</td>"
			<< "<td title=\"" << blueTip << "\">" << std::fixed << std::setprecision(2) << blueVal << "</td></tr>\n";
		};

	writeRow("KDR", "Kills divided by Deaths", redKDR, blueKDR, "Red Team KDR", "Blue Team KDR");
	writeRow("KPM", "Kills per Minute played", redKPM, blueKPM, "Red Team KPM", "Blue Team KPM");
	writeRow("DMR", "Damage dealt divided by Damage received", redDMR, blueDMR, "Red Team DMR", "Blue Team DMR");

	// Totals row
	double redAvg = (redKDR + redKPM + redDMR) / 3.0;
	double blueAvg = (blueKDR + blueKPM + blueDMR) / 3.0;

	html << "<tr><td><b>Average</b></td>"
		<< "<td>" << std::fixed << std::setprecision(2) << redAvg << "</td>"
		<< "<td>" << std::fixed << std::setprecision(2) << blueAvg << "</td></tr>\n";

	html << "</table>\n</div>\n";
}

/*
=============
Html_WriteTopPlayers
=============
*/
static inline void Html_WriteTopPlayers(std::ofstream& html, const MatchStats& matchStats, std::vector<const PlayerStats*> allPlayers) {
	html << "<div class=\"section\">\n<h2>Top Players</h2>\n";
	const bool hadTeams = matchStats.wasTeamMode && matchStats.teams.size() >= 2;

	auto getPlayerColor = [&](const PlayerStats* p) -> const char* {
		if (!hadTeams)
			return "green";
		for (const auto& tp : matchStats.teams[0].players)
			if (&tp == p) return "red";
		for (const auto& tp : matchStats.teams[1].players)
			if (&tp == p) return "blue";
		return "green"; // fallback
		};

	auto writeList = [&](const char* title, auto valueFn) {
		std::vector<std::pair<const PlayerStats*, double>> list;
		double maxVal = 0.0;

		for (auto* p : allPlayers) {
			double val = valueFn(p);
			if (val > 0.0) {
				list.push_back({ p, val });
				maxVal = std::max(maxVal, val);
			}
		}

		if (list.empty())
			return;

		std::sort(list.begin(), list.end(), [](auto& a, auto& b) { return a.second > b.second; });

		html << "<h3>" << title << "</h3>\n<table>\n<tr><th>Player</th><th>" << title << "</th></tr>\n";

		for (size_t i = 0; i < std::min<size_t>(10, list.size()); ++i) {
			const auto* p = list[i].first;
			double val = list[i].second;
			const char* color = getPlayerColor(p);

			const std::string escapedPlayerName = HtmlEscape(p->playerName);
			double pct = (maxVal > 0.0) ? (val / maxVal) * 100.0 : 0.0;
			if (pct < 1.0) pct = 1.0; // enforce minimum

			html << "<tr><td class=\"player-cell " << color << "\">" << escapedPlayerName << "</td>"
				<< "<td class=\"progress-cell " << color << "\">"
				<< "<div class=\"bar\" style=\"width:" << pct << "%\"></div>"
				<< "<span>" << std::fixed << std::setprecision(2) << val << "</span></td></tr>\n";
		}

		html << "</table>\n";
		};

	// Write 3 separate lists
	writeList("KDR", [](const PlayerStats* p) -> double {
		if (p->totalKills == 0 && p->totalDeaths == 0) return 0.0;
		return (p->totalDeaths > 0) ? (double(p->totalKills) / p->totalDeaths) : double(p->totalKills);
		});

	writeList("KPM", [](const PlayerStats* p) -> double {
		if (p->playTimeMsec <= 0) return 0.0;
		return (p->totalKills * 60.0) / (p->playTimeMsec / 1000.0f);
		});

	writeList("DMR", [](const PlayerStats* p) -> double {
		if (p->totalDmgDealt == 0 && p->totalDmgReceived == 0) return 0.0;
		return (p->totalDmgReceived > 0) ? (double(p->totalDmgDealt) / p->totalDmgReceived) : double(p->totalDmgDealt);
		});

	html << "</div>\n";
}

/*
=============
Html_WriteItemPickups
=============
*/
static inline void Html_WriteItemPickups(std::ofstream& html, const MatchStats& matchStats, const std::vector<const PlayerStats*>& allPlayers) {
	if (allPlayers.empty())
		return;

	const bool hadTeams = matchStats.wasTeamMode && matchStats.teams.size() >= 2;

	std::unordered_map<std::string, uint32_t> itemTotals;
	std::unordered_map<std::string, double> itemDelays;

	auto getPickup = [](const PlayerStats* p, HighValueItems item) -> uint32_t {
		return (item > HighValueItems::None && item < HighValueItems::Total) ? p->pickupCounts[static_cast<int>(item)] : 0u;
		};

	auto getDelay = [](const PlayerStats* p, HighValueItems item) -> double {
		return (item > HighValueItems::None && item < HighValueItems::Total) ? p->pickupDelays[static_cast<int>(item)] : 0.0;
		};

	// Aggregate totals
	for (auto* p : allPlayers) {
		for (int i = static_cast<size_t>(HighValueItems::None) + 1; i < static_cast<size_t>(HighValueItems::Total); ++i) {
			HighValueItems item = static_cast<HighValueItems>(i);
			itemTotals[HighValueItemNames[i]] += getPickup(p, item);
			itemDelays[HighValueItemNames[i]] += getDelay(p, item);
		}
	}

	// Only items that were actually picked up
	std::vector<std::string> sortedItems;
	for (const auto& it : itemTotals) {
		if (it.second > 0)
			sortedItems.push_back(it.first);
	}
	std::sort(sortedItems.begin(), sortedItems.end(), [&](const std::string& a, const std::string& b) {
		return itemTotals[a] > itemTotals[b];
		});

	if (sortedItems.empty())
		return;

	html << "<div class=\"section\">\n<h2>Global High Value Item Pickups</h2>\n";

	html << "<div class=\"flex-container\">\n";

	// --- Players Table ---
	html << "<div class=\"flex-item\">\n";
	html << "<table>\n<tr><th>Player</th>";
	for (const auto& name : sortedItems) {
		html << "<th>" << name << "</th>";
	}
	html << "</tr>\n";

	bool wrotePlayerRow = false;

	for (auto* p : allPlayers) {
		bool hasPickup = false;
		for (const auto& name : sortedItems) {
			for (int i = static_cast<int>(HighValueItems::None) + 1; i < static_cast<int>(HighValueItems::Total); ++i) {
				if (HighValueItemNames[i] == name && getPickup(p, static_cast<HighValueItems>(i)) > 0) {
					hasPickup = true;
					break;
				}
			}
			if (hasPickup) break;
		}
		if (!hasPickup)
			continue;

		wrotePlayerRow = true;

		std::string color = "green";
		if (hadTeams) {
			for (const auto& rp : matchStats.teams[0].players)
				if (&rp == p) { color = "red"; break; }
			for (const auto& bp : matchStats.teams[1].players)
				if (&bp == p) { color = "blue"; break; }
		}

		const std::string escapedPlayerName = HtmlEscape(p->playerName);
		html << "<tr><td class=\"player-cell " << color << "\">" << escapedPlayerName << "</td>";

		for (const auto& name : sortedItems) {
			int idx = -1;
			for (int i = static_cast<int>(HighValueItems::None) + 1; i < static_cast<int>(HighValueItems::Total); ++i) {
				if (HighValueItemNames[i] == name) {
					idx = i;
					break;
				}
			}
			if (idx == -1) {
				html << "<td>-</td>";
				continue;
			}

			uint32_t pickups = getPickup(p, static_cast<HighValueItems>(idx));
			double delay = getDelay(p, static_cast<HighValueItems>(idx));

			if (pickups > 0) {
				int avgSecs = static_cast<int>((delay / pickups) + 0.5);
				html << "<td>" << pickups << " (" << FormatDuration(avgSecs) << ")</td>";
			}
			else {
				html << "<td>-</td>";
			}
		}

		html << "</tr>\n";
	}

	if (wrotePlayerRow) {
		html << "<tr><td><b>Totals</b></td>";

		for (const auto& name : sortedItems) {
			auto total = itemTotals[name];
			auto totalDelay = itemDelays[name];
			if (total > 0) {
				int avgSecs = static_cast<int>((totalDelay / total) + 0.5);
				html << "<td>" << total << " (" << FormatDuration(avgSecs) << ")</td>";
			}
			else {
				html << "<td>-</td>";
			}
		}

		html << "</tr>\n";
	}

	html << "</table>\n</div>\n"; // flex-item (players)

	// --- Team Totals Table ---
	if (hadTeams) {
		uint32_t redTotal = 0, blueTotal = 0;
		double redDelay = 0.0, blueDelay = 0.0;

		for (auto* p : allPlayers) {
			bool isRed = false;
			for (const auto& rp : matchStats.teams[0].players)
				if (&rp == p) { isRed = true; break; }
			for (const auto& bp : matchStats.teams[1].players)
				if (&bp == p) { isRed = false; break; }
			for (int i = static_cast<int>(HighValueItems::None) + 1; i < static_cast<int>(HighValueItems::Total); ++i) {
				if (isRed) {
					redTotal += getPickup(p, static_cast<HighValueItems>(i));
					redDelay += getDelay(p, static_cast<HighValueItems>(i));
				}
				else {
					blueTotal += getPickup(p, static_cast<HighValueItems>(i));
					blueDelay += getDelay(p, static_cast<HighValueItems>(i));
				}
			}
		}

		html << "<div class=\"flex-item\">\n";
		html << "<h3>Team Item Pickup Summary</h3>\n<table>\n<tr><th>Team</th><th>Total Pickups</th><th>Avg Delay</th></tr>\n";

		int redAvgSecs = (redTotal > 0) ? static_cast<int>((redDelay / redTotal) + 0.5) : 0;
		int blueAvgSecs = (blueTotal > 0) ? static_cast<int>((blueDelay / blueTotal) + 0.5) : 0;

		html << "<tr><td class=\"player-cell red\">Red</td><td>" << redTotal << "</td><td>" << FormatDuration(redAvgSecs) << "</td></tr>\n";
		html << "<tr><td class=\"player-cell blue\">Blue</td><td>" << blueTotal << "</td><td>" << FormatDuration(blueAvgSecs) << "</td></tr>\n";

		html << "</table>\n</div>\n"; // flex-item (teams)
	}

	html << "</div>\n"; // flex-container
	html << "</div>\n"; // section
}

/*
=============
Html_WriteTopMeansOfDeath
=============
*/
static inline void Html_WriteTopMeansOfDeath(std::ofstream& html, const MatchStats& matchStats, const std::vector<const PlayerStats*>& redPlayers, const std::vector<const PlayerStats*>& bluePlayers) {
	html << "<div class=\"section\">\n<h2>Deaths by Type</h2>\n<table>\n";
	const bool hadTeams = matchStats.wasTeamMode && matchStats.teams.size() >= 2;

	if (hadTeams) {
		html << "<tr><th>MOD</th><th>Red</th><th>Blue</th><th>Total</th></tr>\n";
	}
	else {
		html << "<tr><th>MOD</th><th>Total</th></tr>\n";
	}

	// Build the MOD list
	std::vector<std::string> mods;
	for (auto& kv : matchStats.totalDeathsByMOD) {
		if (kv.second > 0)
			mods.push_back(kv.first);
	}

	std::sort(mods.begin(), mods.end(), [&](const std::string& a, const std::string& b) {
		return matchStats.totalDeathsByMOD.at(a) > matchStats.totalDeathsByMOD.at(b);
		});

	for (auto& modName : mods) {
		int total = matchStats.totalDeathsByMOD.at(modName);
		const std::string escapedModName = HtmlEscape(modName);

		if (!hadTeams) {
			// Solo mode
			html << "<tr><td>" << escapedModName << "</td><td>" << total << "</td></tr>\n";
		}
		else {
			// Team mode: split
			int redDeaths = 0, blueDeaths = 0;

			const size_t modIdx = static_cast<size_t>(getModIdByName(modName));
			for (auto* p : redPlayers) {
				redDeaths += p->modTotalDeaths[modIdx];
			}
			for (auto* p : bluePlayers) {
				blueDeaths += p->modTotalDeaths[modIdx];
			}

			html << "<tr><td>" << escapedModName << "</td><td>" << redDeaths << "</td><td>" << blueDeaths << "</td><td>" << (redDeaths + blueDeaths) << "</td></tr>\n";
		}
	}

	html << "</table>\n</div>\n";
}

/*
=============
Html_WriteGametypeStats

Outputs gametype-specific highlights (currently CTF) including team totals,
standout performers, and player-level breakdowns derived from the aggregated
gametype statistics.
=============
*/
static inline void Html_WriteGametypeStats(std::ofstream& html, const MatchStats& matchStats, const std::vector<const PlayerStats*>& allPlayers) {
	if (!HasFlag(matchStats.recordedFlags, GameFlags::CTF)) {
		return;
	}

	if (!matchStats.gametypeStats.isObject()) {
		return;
	}

	if (!matchStats.gametypeStats.isMember("ctf") || !JsonHasData(matchStats.gametypeStats["ctf"])) {
		return;
	}

	const json& ctfJson = matchStats.gametypeStats["ctf"];
	const json& totalsJson = ctfJson["totals"];
	const json& teamsJson = ctfJson["teams"];
	const json& playersJson = ctfJson["players"];
	const bool hadTeams = matchStats.wasTeamMode && matchStats.teams.size() >= 2;

	auto readInt64 = [](const json& node, const char* key) -> int64_t {
		if (!node.isObject() || !node.isMember(key)) {
			return 0;
		}
		const json& value = node[key];
		if (value.isInt64()) {
			return value.asInt64();
		}
		if (value.isUInt64()) {
			return static_cast<int64_t>(value.asUInt64());
		}
		if (value.isInt()) {
			return value.asInt64();
		}
		if (value.isUInt()) {
			return static_cast<int64_t>(value.asUInt());
		}
		return 0;
	};

	html << "<div class=\"section gametype\">\n"
		<< "  <h2>CTF Highlights</h2>\n";

	if (totalsJson.isObject() && JsonHasData(totalsJson)) {
		const int64_t totalCaptures = readInt64(totalsJson, "flagsCaptured");
		const int64_t totalAssists = readInt64(totalsJson, "flagAssists");
		const int64_t totalDefends = readInt64(totalsJson, "flagDefends");
		const std::string totalCarry = Html_FormatMilliseconds(readInt64(totalsJson, "flagHoldTimeTotalMsec"));
		html << "  <p><strong>Total Captures:</strong> " << totalCaptures
			<< " | <strong>Assists:</strong> " << totalAssists
			<< " | <strong>Defends:</strong> " << totalDefends
			<< " | <strong>Combined Carry Time:</strong> " << totalCarry << "</p>\n";
	}

	if (hadTeams && teamsJson.isObject()) {
		html << "  <h3>Team Totals</h3>\n"
			<< "  <table><tr><th>Team</th><th>Captures</th><th>Assists</th><th>Defends</th><th>Pickups</th><th>Drops</th><th>Carry Time</th></tr>\n";
		bool wroteTeamRow = false;
		auto writeTeamRow = [&](const char* teamLabel, const char* cssClass, const json& teamJson) {
			if (!teamJson.isObject() || !JsonHasData(teamJson)) {
				return;
			}
			html << "    <tr><td class=\"player-cell " << cssClass << "\">" << teamLabel << "</td>"
				<< "<td>" << readInt64(teamJson, "flagsCaptured") << "</td>"
				<< "<td>" << readInt64(teamJson, "flagAssists") << "</td>"
				<< "<td>" << readInt64(teamJson, "flagDefends") << "</td>"
				<< "<td>" << readInt64(teamJson, "flagPickups") << "</td>"
				<< "<td>" << readInt64(teamJson, "flagDrops") << "</td>"
				<< "<td>" << Html_FormatMilliseconds(readInt64(teamJson, "flagHoldTimeTotalMsec")) << "</td></tr>\n";
			wroteTeamRow = true;
		};

		if (teamsJson.isMember("red")) {
			writeTeamRow("Red", "red", teamsJson["red"]);
		}
		if (teamsJson.isMember("blue")) {
			writeTeamRow("Blue", "blue", teamsJson["blue"]);
		}

		if (!wroteTeamRow) {
			html << "    <tr><td colspan=\"7\">No team data recorded.</td></tr>\n";
		}
		html << "  </table>\n";
	}

	std::unordered_map<std::string, const json*> playerLookup;
	const json* topCaptureEntry = nullptr;
	const json* topCarryEntry = nullptr;
	int64_t maxCaptures = 0;
	int64_t maxCarryTime = 0;

	if (playersJson.isArray()) {
		for (Json::ArrayIndex i = 0; i < playersJson.size(); ++i) {
			const json& entry = playersJson[i];
			if (!entry.isObject()) {
				continue;
			}
			if (!entry.isMember("stats") || !entry["stats"].isObject()) {
				continue;
			}
			const json& stats = entry["stats"];
			if (!JsonHasData(stats)) {
				continue;
			}
			if (entry.isMember("socialID")) {
				playerLookup[entry["socialID"].asString()] = &entry;
			}
			const int64_t captures = readInt64(stats, "flagCaptures");
			if (captures > maxCaptures) {
				maxCaptures = captures;
				topCaptureEntry = &entry;
			}
			const int64_t carryTime = readInt64(stats, "flagCarrierTimeTotalMsec");
			if (carryTime > maxCarryTime) {
				maxCarryTime = carryTime;
				topCarryEntry = &entry;
			}
		}
	}

	if ((topCaptureEntry && maxCaptures > 0) || (topCarryEntry && maxCarryTime > 0)) {
		html << "  <h3>Standouts</h3>\n"
			<< "  <ul>\n";
		if (topCaptureEntry && maxCaptures > 0) {
			const std::string bestCaptureName = HtmlEscape(topCaptureEntry->isMember("playerName") ? (*topCaptureEntry)["playerName"].asString() : std::string("Unknown"));
			html << "    <li>Top Flag Captures: " << bestCaptureName << " (" << maxCaptures << ")</li>\n";
		}
		if (topCarryEntry && maxCarryTime > 0) {
			const std::string bestCarrierName = HtmlEscape(topCarryEntry->isMember("playerName") ? (*topCarryEntry)["playerName"].asString() : std::string("Unknown"));
			html << "    <li>Longest Carrier: " << bestCarrierName << " (" << Html_FormatMilliseconds(maxCarryTime) << ")</li>\n";
		}
		html << "  </ul>\n";
	}

	html << "  <h3>Player CTF Stats</h3>\n"
		<< "  <table><tr><th>Player</th><th>Team</th><th>Captures</th><th>Assists</th><th>Returns</th><th>Pickups</th><th>Drops</th><th>Carry Time</th></tr>\n";
	bool wrotePlayerRow = false;
	for (const PlayerStats* player : allPlayers) {
		if (!player) {
			continue;
		}
		auto it = playerLookup.find(player->socialID);
		if (it == playerLookup.end()) {
			continue;
		}
		const json& entry = *it->second;
		const json& stats = entry["stats"];
		const int captures = static_cast<int>(readInt64(stats, "flagCaptures"));
		const int assists = static_cast<int>(readInt64(stats, "flagAssists"));
		const int returns = static_cast<int>(readInt64(stats, "flagReturns"));
		const int pickups = static_cast<int>(readInt64(stats, "flagPickups"));
		const int drops = static_cast<int>(readInt64(stats, "flagDrops"));
		const int64_t carryTime = readInt64(stats, "flagCarrierTimeTotalMsec");
		if (!(captures || assists || returns || pickups || drops || carryTime)) {
			continue;
		}
		wrotePlayerRow = true;
		std::string teamLabel = "-";
		std::string teamClass = "player-cell";
		if (entry.isMember("team")) {
			teamLabel = entry["team"].asString();
			if (teamLabel == "Red") {
				teamClass += " red";
			}
			else if (teamLabel == "Blue") {
				teamClass += " blue";
			}
		}
		const std::string escapedName = HtmlEscape(player->playerName);
		html << "    <tr><td class=\"player-cell\">" << escapedName << "</td>"
			<< "<td class=\"" << teamClass << "\">" << HtmlEscape(teamLabel) << "</td>"
			<< "<td>" << captures << "</td>"
			<< "<td>" << assists << "</td>"
			<< "<td>" << returns << "</td>"
			<< "<td>" << pickups << "</td>"
			<< "<td>" << drops << "</td>"
			<< "<td>" << Html_FormatMilliseconds(carryTime) << "</td></tr>\n";
	}
	if (!wrotePlayerRow) {
		html << "    <tr><td colspan=\"8\">No CTF player activity recorded.</td></tr>\n";
	}
	html << "  </table>\n";

	html << "</div>\n";
}

/*
=============
Html_WriteEventLog
=============
*/
static inline void Html_WriteEventLog(std::ofstream& html, const MatchStats& matchStats, const std::vector<const PlayerStats*>& allPlayers) {
	if (matchStats.eventLog.empty())
		return;

	const bool hadTeams = matchStats.wasTeamMode && matchStats.teams.size() >= 2;

	int matchDuration = matchStats.durationMS;

	// === Precompute name replacements ===
	std::unordered_map<std::string, std::string> nameToHtml;

	for (const auto* p : allPlayers) {
		std::string name = p->playerName;
		std::string color = "green";

		if (hadTeams) {
			for (const auto& tp : matchStats.teams[0].players)
				if (&tp == p) { color = "red"; break; }
			for (const auto& tp : matchStats.teams[1].players)
				if (&tp == p) { color = "blue"; break; }
		}

		const std::string escapedName = HtmlEscape(name);
		if (hadTeams) {
			nameToHtml[escapedName] = "<span class=\"player-name " + color + "\"><b>" + escapedName + "</b></span>";
		}
		else {
			nameToHtml[escapedName] = "<b>" + escapedName + "</b>";
		}
	}

	// === Render event log ===
	html << "<div class=\"section\">\n<h2>Event Log</h2>\n<table>\n<tr><th>Time</th><th>Event</th></tr>\n";

	for (auto& e : matchStats.eventLog) {
		int secs = static_cast<int>(e.time.seconds());
		double pctTime = (matchDuration > 0.0) ? (double(secs) / matchDuration) * 100.0 : 0.0;
		if (pctTime < 1.0) pctTime = 1.0;

		// Start with original string
		std::string evStr = HtmlEscape(e.eventStr);

		// Replace player names
		for (auto& kv : nameToHtml) {
			size_t pos = evStr.find(kv.first);
			if (pos != std::string::npos) {
				evStr.replace(pos, kv.first.length(), kv.second);
			}
		}

		// --- Write the event row ---
		html << "<tr><td class=\"progress-cell green\" title=\"" << secs << " seconds\">"
			<< "<div class=\"bar\" style=\"width:" << pctTime << "%\"></div>"
			<< "<span>";

		int h = secs / 3600;
		int m = (secs % 3600) / 60;
		int s = secs % 60;
		if (h > 0)
			html << h << "h " << m << "m " << s << "s";
		else if (m > 0)
			html << m << "m " << s << "s";
		else
			html << s << "s";

		html << "</span></td><td>" << evStr << "</td></tr>\n";
	}

	html << "</table>\n</div>\n";
}

/*
=============
Html_WriteIndividualPlayerSections
=============
*/
static inline void Html_WriteIndividualPlayerSections(std::ofstream& html, const MatchStats& matchStats, std::vector<const PlayerStats*> allPlayers) {
	const bool hadTeams = matchStats.wasTeamMode && matchStats.teams.size() >= 2;
	const bool hadCtf = HasFlag(matchStats.recordedFlags, GameFlags::CTF);
	for (const PlayerStats* p : allPlayers) {
		html << "<div class=\"section\">";
		const std::string fullID = p->socialID;
		const std::string escapedFullId = HtmlEscape(fullID);
		const std::string escapedPlayerName = HtmlEscape(p->playerName);
		const std::string steamPref = "Steamworks-";
		const std::string gogPref = "Galaxy-";
		std::string profileURL;

		// Steam branch
		if (fullID.rfind(steamPref, 0) == 0) {
			// strip "Steamworks-"
			auto id = fullID.substr(steamPref.size());
			profileURL = "https://steamcommunity.com/profiles/" + id;

			// GOG branch
		}
		else if (fullID.rfind(gogPref, 0) == 0) {
			// strip "GOG-" -> leftover is the user's GOG slug
			auto slug = fullID.substr(gogPref.size());
			profileURL = "https://www.gog.com/u/" + slug;
		}

		// emit the header
		const std::string escapedProfileURL = HtmlEscape(profileURL);
		html << "  <h2 id=\"player-" << escapedFullId << "\">Player: " << escapedPlayerName << " (";
		if (!profileURL.empty()) {
			html << "<a href=\"" << escapedProfileURL << "\">" << escapedFullId << "</a>";
		}
		else {
			html << escapedFullId;
		}
		html << ")</h2>";

		// Top-line summary
		if (hadTeams) {
			html << "  <p>"
				<< "Kills: " << p->totalKills
				<< " | SpawnKills: " << p->totalSpawnKills
				<< " | TeamKills: " << p->totalTeamKills
				<< " | Deaths: " << p->totalDeaths
				<< " | Suicides: " << p->totalSuicides
				<< " | Score: " << p->totalScore
				<< "</p>";
		}
		else {
			html << "  <p>"
				<< "Kills: " << p->totalKills
				<< " | SpawnKills: " << p->totalSpawnKills
				<< " | Deaths: " << p->totalDeaths
				<< " | Suicides: " << p->totalSuicides
				<< " | Score: " << p->totalScore
				<< "</p>";
		}

		const bool hasPlayerCtfStats = p->gametypeStats.isObject()
			&& p->gametypeStats.isMember("ctf")
			&& JsonHasData(p->gametypeStats["ctf"]);
		const bool hasCtfValues = (p->ctfFlagPickups > 0 || p->ctfFlagDrops > 0 || p->ctfFlagReturns > 0
			|| p->ctfFlagAssists > 0 || p->ctfFlagCaptures > 0
			|| p->ctfFlagCarrierTimeTotalMsec > 0 || p->ctfFlagCarrierTimeShortestMsec > 0
			|| p->ctfFlagCarrierTimeLongestMsec > 0);
		if (hadCtf && hasPlayerCtfStats && hasCtfValues) {
			html << "  <h3>CTF Performance</h3>\n"
				<< "  <table><tr><th>Metric</th><th>Value</th></tr>\n";
			bool wroteMetric = false;
			auto writeCountMetric = [&](const char* label, int value) {
				if (value <= 0) {
					return;
				}
				html << "    <tr><td>" << label << "</td><td>" << value << "</td></tr>\n";
				wroteMetric = true;
			};
			auto writeTimeMetric = [&](const char* label, int64_t value) {
				if (value <= 0) {
					return;
				}
				html << "    <tr><td>" << label << "</td><td>" << Html_FormatMilliseconds(value) << "</td></tr>\n";
				wroteMetric = true;
			};

			writeCountMetric("Flag Pickups", p->ctfFlagPickups);
			writeCountMetric("Flag Drops", p->ctfFlagDrops);
			writeCountMetric("Flag Returns", p->ctfFlagReturns);
			writeCountMetric("Flag Assists", p->ctfFlagAssists);
			writeCountMetric("Flag Captures", p->ctfFlagCaptures);
			writeTimeMetric("Total Carry Time", p->ctfFlagCarrierTimeTotalMsec);
			writeTimeMetric("Shortest Carry", p->ctfFlagCarrierTimeShortestMsec);
			writeTimeMetric("Longest Carry", p->ctfFlagCarrierTimeLongestMsec);

			if (!wroteMetric) {
				html << "    <tr><td colspan=\"2\">No gametype metrics recorded.</td></tr>\n";
			}
			html << "  </table>";
		}

		const auto& matchDeathLog = matchStats.deathLog;
		if (!matchDeathLog.empty()) {
			// Top Victims by this player
			{
				std::unordered_map<std::string, int> victimCounts;
				for (const auto& e : matchDeathLog) {
					if (e.attacker.id == p->socialID) {
						victimCounts[e.victim.name]++;
					}
				}
				std::vector<std::pair<std::string, int>> victims(victimCounts.begin(), victimCounts.end());
				std::sort(victims.begin(), victims.end(), [](auto& a, auto& b) { return a.second > b.second; });
				html << "  <h3>Top Victims by " << escapedPlayerName << "</h3>"
					<< "  <table><tr><th>Player</th><th>Kills</th></tr>";
				for (size_t i = 0; i < std::min<size_t>(10, victims.size()); ++i) {
					const std::string escapedVictim = HtmlEscape(victims[i].first);
					html << "    <tr><td>" << escapedVictim
						<< "</td><td>" << victims[i].second << "</td></tr>";
				}
				html << "  </table>";
			}

			// Top Killers of this player
			{
				std::unordered_map<std::string, int> killerCounts;
				for (const auto& e : matchDeathLog) {
					if (e.victim.id == p->socialID) {
						killerCounts[e.attacker.name]++;
					}
				}
				std::vector<std::pair<std::string, int>> killers(killerCounts.begin(), killerCounts.end());
				std::sort(killers.begin(), killers.end(), [](auto& a, auto& b) { return a.second > b.second; });
				html << "  <h3>Top Killers of " << escapedPlayerName << "</h3>"
					<< "  <table><tr><th>Player</th><th>Deaths</th></tr>";
				for (size_t i = 0; i < std::min<size_t>(10, killers.size()); ++i) {
					const std::string escapedKiller = HtmlEscape(killers[i].first);
					html << "    <tr><td>" << escapedKiller
						<< "</td><td>" << killers[i].second << "</td></tr>";
				}
				html << "  </table>";
			}
		}

		// Weapon Stats (only used weapons, sorted by accuracy desc)
		html << "  <h3>Weapon Stats</h3>"
			<< "  <table><tr><th>Weapon</th><th>Shots</th><th>Hits</th><th>Acc (%)</th></tr>";
		{
			std::vector<size_t> used;
			for (size_t i = 0; i < weaponAbbreviations.size(); ++i) {
				if (p->totalShotsPerWeapon[i] > 0 || p->totalHitsPerWeapon[i] > 0) {
					used.push_back(i);
				}
			}
			std::sort(used.begin(), used.end(), [&](size_t a, size_t b) {
				return p->accuracyPerWeapon[a] > p->accuracyPerWeapon[b];
			});
			for (size_t idx : used) {
				const auto& weaponName = weaponAbbreviations[idx];
				html << "    <tr>"
					<< "<td>" << weaponName << "</td>"
					<< "<td>" << p->totalShotsPerWeapon[idx] << "</td>"
					<< "<td>" << p->totalHitsPerWeapon[idx] << "</td>"
					<< std::fixed << std::setprecision(1)
					<< "<td>" << p->accuracyPerWeapon[idx] << "</td></tr>";
			}
		}
		html << "  </table>";

		// Means-of-Death Stats (MOD rows sorted by KDR)
		html << "  <h3>Means-of-Death Stats</h3>"
			<< "  <table><tr><th>MOD</th><th>Kills</th><th>Deaths</th><th>KDR</th><th>DmgD</th><th>DmgR</th></tr>";
		{
			struct Row { std::string mod; int k, d; double kdr; int dd, dr; };
			std::vector<Row> rows;
			for (auto& mr : modr) {
				const size_t idx = static_cast<size_t>(mr.mod);
				int kills = p->modTotalKills[idx];
				int deaths = p->modTotalDeaths[idx];
				if (!kills && !deaths) continue;
				double ratio = deaths > 0 ? double(kills) / deaths : (kills > 0 ? double(kills) : 0.0);
				rows.push_back({ mr.name,kills,deaths,ratio,p->modTotalDmgD[idx],p->modTotalDmgR[idx] });
			}
			std::sort(rows.begin(), rows.end(), [](auto& a, auto& b) { return a.kdr > b.kdr; });
			for (auto& r : rows) {
				html << "    <tr><td>" << r.mod
					<< "</td><td>" << r.k
					<< "</td><td>" << r.d
					<< std::fixed << std::setprecision(2)
					<< "</td><td>" << r.kdr
					<< std::setprecision(0)
					<< "</td><td>" << r.dd
					<< "</td><td>" << r.dr
					<< "</td></tr>";
			}
		}
		html << "  </table>";

		// Awards (only if earned)
		{
			std::vector<std::pair<std::string, int>> aw;
			for (size_t i = 0; i < static_cast<uint8_t>(PlayerMedal::Total); i++) {
				if (p->awards[i] > 0) aw.emplace_back(awardNames[i], p->awards[i]);
			}
			if (!aw.empty()) {
				std::sort(aw.begin(), aw.end(), [](auto& a, auto& b) {return a.second > b.second; });
				html << "  <h3>Awards</h3>"
					<< "  <table><tr><th>Award</th><th>Count</th></tr>";
				for (auto& e : aw) {
					html << "    <tr><td>" << e.first
						<< "</td><td>" << e.second << "</td></tr>";
				}
				html << "  </table>";
			}
		}

		html << "</div>";
	}
}

/*
=============
Html_WriteFooter
=============
*/
static inline void Html_WriteFooter(std::ofstream& html, const std::string& htmlPath) {
	html << "<div class=\"footer\">Compiled by " << worr::version::kGameTitle << " "
		<< worr::version::kGameVersion << "</div>\n";
	html << "</body></html>\n";
}

/*
=============
MatchStats_WriteHtml
=============
*/
static bool MatchStats_WriteHtml(const MatchStats& matchStats, const std::string& htmlPath) {
	try {
		WriteFileAtomically(htmlPath, [&](std::ofstream& html) {
			// Gather players
			std::vector<const PlayerStats*> allPlayers;
			std::vector<const PlayerStats*> redPlayers;
			std::vector<const PlayerStats*> bluePlayers;

			int redScore = 0, blueScore = 0;
			int maxGlobalScore = 0;

			// solo players
			for (const auto& p : matchStats.players) {
				allPlayers.push_back(&p);
				maxGlobalScore = std::max(maxGlobalScore, p.totalScore);
			}

			// team players
			for (size_t i = 0; i < matchStats.teams.size(); ++i) {
				const auto& team = matchStats.teams[i];
				if (i == 0) redScore = team.score;
				if (i == 1) blueScore = team.score;

				for (const auto& p : team.players) {
					allPlayers.push_back(&p);
					maxGlobalScore = std::max(maxGlobalScore, p.totalScore);
					if (i == 0)
						redPlayers.push_back(&p);
					else if (i == 1)
						bluePlayers.push_back(&p);
				}
			}

			const bool hadTeams = matchStats.wasTeamMode && matchStats.teams.size() >= 2;
			const bool hadCtf = HasFlag(matchStats.recordedFlags, GameFlags::CTF);
			// Sort by totalScore descending
			std::sort(allPlayers.begin(), allPlayers.end(), [](auto a, auto b) {
				return a->totalScore > b->totalScore;
			});

			Html_WriteHeader(html, matchStats);
			Html_WriteTopInfo(html, matchStats);
			Html_WriteWinnerSummary(html, matchStats);

			if (hadTeams) {
				Html_WriteTeamScores(html, redPlayers, bluePlayers, redScore, blueScore, matchStats.durationMS, maxGlobalScore);
				const double matchDurationMs = static_cast<double>(matchStats.durationMS);
				Html_WriteTeamsComparison(html, redPlayers, bluePlayers, matchDurationMs);
			}
			else {
				Html_WriteOverallScores(html, matchStats, allPlayers);
			}

			Html_WriteTopPlayers(html, matchStats, allPlayers);
			Html_WriteItemPickups(html, matchStats, allPlayers);
			Html_WriteTopMeansOfDeath(html, matchStats, redPlayers, bluePlayers);
			if (hadCtf) {
				Html_WriteGametypeStats(html, matchStats, allPlayers);
			}
			Html_WriteEventLog(html, matchStats, allPlayers);
			Html_WriteIndividualPlayerSections(html, matchStats, allPlayers);
			Html_WriteFooter(html, htmlPath);
		});
		gi.Com_PrintFmt("Match HTML report written to {}\n", htmlPath.c_str());
		return true;
	}
	catch (const std::exception& e) {
		gi.Com_PrintFmt("Exception while writing HTML ({}): {}\n", htmlPath.c_str(), e.what());
	}
	catch (...) {
		gi.Com_PrintFmt("Unknown error while writing HTML ({})\n", htmlPath.c_str());
	}

	return false;
}



/*
=============
SendIndividualMiniStats

Matches aggregated player statistics to active clients via a stable identifier
and prints a short personal summary for each client.
=============
*/
static void SendIndividualMiniStats(const MatchStats& matchStats) {
	auto findStatsForClient = [&](std::string_view identifier, std::string_view fallbackName) -> const PlayerStats* {
		auto matches = [&](const PlayerStats& stats) {
			if (!identifier.empty() && !stats.socialID.empty()) {
				return _stricmp(stats.socialID.c_str(), identifier.data()) == 0;
			}

			if (!fallbackName.empty() && !stats.playerName.empty()) {
				return _stricmp(stats.playerName.c_str(), fallbackName.data()) == 0;
			}

			return false;
		};

		for (const PlayerStats& p : matchStats.players) {
			if (matches(p)) {
				return &p;
			}
		}

		for (const TeamStats& team : matchStats.teams) {
			for (const PlayerStats& teamPlayer : team.players) {
				if (matches(teamPlayer)) {
					return &teamPlayer;
				}
			}
		}

		return nullptr;
	};

	for (auto ec : active_players()) {
		if (!ec || !ec->client)
			continue;

		const char* rawName = ec->client->sess.netName;
		const char* rawIdentifier = ec->client->sess.socialID;

		const bool hasName = rawName && rawName[0];
		const bool hasIdentifier = rawIdentifier && rawIdentifier[0];

		if (!hasName && !hasIdentifier) {
			gi.Com_PrintFmt("SendIndividualMiniStats: skipping client {} due to missing identifier and name\n", ec->s.number);
			continue;
		}

		const std::string_view identifierView = hasIdentifier ? std::string_view(rawIdentifier) : std::string_view();
		const std::string_view nameView = hasName ? std::string_view(rawName) : std::string_view();

		const PlayerStats* matchedStats = findStatsForClient(identifierView, nameView);
		if (!matchedStats)
			continue;

		const PlayerStats& p = *matchedStats;
		const char* displayName = hasName ? rawName : (hasIdentifier ? rawIdentifier : "Unknown");

		std::string msg;
		msg += ":: Match Summary ::\n";
		msg += G_Fmt("{} - ", displayName);
		msg += G_Fmt("Kills: {} | Deaths: {}", p.totalKills, p.totalDeaths);
		msg += G_Fmt(" | K/D Ratio: {:.2f}", p.totalKDR);

		/*
		double total = p.totalKills + p.totalAssists + p.totalDeaths;
		double eff = total > 0 ? (double)p.totalKills / total * 100.0 : 0.0;
		msg += G_Fmt(" | Eff: {:.1f}%%\n", eff);
		*/
		gi.LocClient_Print(ec, PRINT_HIGH, "$g_sgame_auto_5f36b2ea2906", msg.c_str());
	}
}
/*
=============
MatchStats_WriteAll

Ensures the destination directory exists, then writes JSON and HTML exports for the
provided match data while reporting any errors that occur during the process.
Returns true when both exports succeed.
=============
*/
static bool MatchStats_WriteAll(const MatchStats& matchStats, const std::string& baseFilePath) {
	const std::filesystem::path basePath(baseFilePath);
	const std::filesystem::path directory = basePath.parent_path();

	if (!directory.empty()) {
		std::error_code dirError;
		std::filesystem::create_directories(directory, dirError);
		if (dirError) {
			gi.Com_PrintFmt("{}: Failed to create directory '{}': {}\n", __FUNCTION__, directory.string().c_str(), dirError.message().c_str());
			return false;
		}
	}

	const bool jsonWritten = MatchStats_WriteJson(matchStats, baseFilePath + ".json");
	bool htmlWritten = true;
	if (g_statex_export_html->integer) {
		htmlWritten = MatchStats_WriteHtml(matchStats, baseFilePath + ".html");
	}
	else {
		gi.Com_PrintFmt("{}: HTML export disabled via g_statex_export_html.\n", __FUNCTION__);
	}
	if (!jsonWritten || !htmlWritten) {
		gi.Com_PrintFmt("{}: Export completed with errors (JSON: {}, HTML: {})\n", __FUNCTION__, jsonWritten ? "ok" : "failed", htmlWritten ? "ok" : "failed");
	}

	return jsonWritten && htmlWritten;
}

/*
=============
MatchStatsWorker_ThreadMain

Processes queued match stats export jobs on a detached worker thread.
=============
*/
static void MatchStatsWorker_ThreadMain() {
	while (true) {
		MatchStatsWorkerJob job;
		{
			std::unique_lock<std::mutex> lock(g_matchStatsWorkerMutex);
			g_matchStatsWorkerCondition.wait(lock, [] {
				return !g_matchStatsWorkerQueue.empty();
			});
			job = std::move(g_matchStatsWorkerQueue.front());
			g_matchStatsWorkerQueue.pop();
		}

		const auto startTime = std::chrono::steady_clock::now();
		bool success = true;
		try {
			success = MatchStats_WriteAll(job.stats, job.baseFilePath);
		}
		catch (const std::exception& e) {
			success = false;
			gi.Com_PrintFmt("Match stats job {} threw exception: {}\n", job.jobId, e.what());
		}
		catch (...) {
			success = false;
			gi.Com_PrintFmt("Match stats job {} threw unknown exception.\n", job.jobId);
		}

		const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - startTime).count();
		const uint32_t pending = g_matchStatsPendingJobs.fetch_sub(1) - 1;

		if (success) {
			const uint32_t completed = ++g_matchStatsCompletedJobs;
			gi.Com_PrintFmt("Match stats job {} succeeded in {} ms (pending: {}, completed: {}, failed: {})\n",
				job.jobId,
				elapsedMs,
				pending,
				completed,
				g_matchStatsFailedJobs.load());
		}
		else {
			const uint32_t failed = ++g_matchStatsFailedJobs;
			gi.Com_PrintFmt("Match stats job {} failed in {} ms (pending: {}, completed: {}, failed: {})\n",
				job.jobId,
				elapsedMs,
				pending,
				g_matchStatsCompletedJobs.load(),
				failed);
		}
	}
}

/*
=============
MatchStatsWorker_EnsureStarted

Creates the detached worker thread on first use.
=============
*/
static void MatchStatsWorker_EnsureStarted() {
	std::call_once(g_matchStatsWorkerOnceFlag, []() {
		std::thread worker(MatchStatsWorker_ThreadMain);
		worker.detach();
	});
}

/*
=============
MatchStatsWorker_Enqueue

Enqueues a finalized MatchStats snapshot for asynchronous export and returns the job ID.
=============
*/
static uint64_t MatchStatsWorker_Enqueue(MatchStats&& stats, std::string baseFilePath) {
	MatchStatsWorker_EnsureStarted();

	const uint64_t jobId = g_matchStatsNextJobID.fetch_add(1);
	{
		std::lock_guard<std::mutex> lock(g_matchStatsWorkerMutex);
		g_matchStatsWorkerQueue.push(MatchStatsWorkerJob{
			jobId,
			std::move(stats),
			std::move(baseFilePath)
		});
	}

	g_matchStatsPendingJobs.fetch_add(1);
	g_matchStatsWorkerCondition.notify_one();
	return jobId;
}
/*
=============
MatchStats_End
=============
*/
void MatchStats_End() {
	if (!deathmatch->integer)
		return;

	G_LogEvent("MATCH END");

	if (!g_statex_enabled->integer) {
		gi.Com_PrintFmt("{}: Reporting disabled.\n", __FUNCTION__);
		return;
	}

	if (g_statex_humans_present->integer && !level.pop.num_playing_human_clients) {
		gi.Com_PrintFmt("{}: No reporting without human players.\n", __FUNCTION__);
		return;
	}

		try {
			const auto& currentGameInfo = Game::GetCurrentInfo();
			matchStats.matchStartMS = level.matchStartRealTime;
			matchStats.matchEndMS = level.matchEndRealTime;
			matchStats.recordedFlags = currentGameInfo.flags;
			bool wasTeamMode = HasFlag(matchStats.recordedFlags, GameFlags::Teams);
			if (!wasTeamMode && Teams()) {
				matchStats.recordedFlags = matchStats.recordedFlags | GameFlags::Teams;
				wasTeamMode = true;
			}
			matchStats.wasTeamMode = wasTeamMode;
		matchStats.matchID = level.matchID;
		matchStats.gameType = std::string(currentGameInfo.short_name_upper);
		matchStats.ruleSet = rs_long_name[game.ruleset];
		matchStats.serverName = hostname->string ? hostname->string : "";
		matchStats.serverHostName.clear();
		if (host && host->client) {
			char hostNameValue[MAX_INFO_VALUE] = { 0 };
			gi.Info_ValueForKey(host->client->pers.userInfo, "name", hostNameValue, sizeof(hostNameValue));
			if (hostNameValue[0] != '\0') {
				matchStats.serverHostName = hostNameValue;
			}
		}
		matchStats.mapName.assign(CharArrayToStringView(level.mapName));
		matchStats.ranked = false;
		matchStats.totalKills = level.match.totalKills;
		matchStats.totalSpawnKills = level.match.totalSpawnKills;
		matchStats.totalTeamKills = level.match.totalTeamKills;
		matchStats.totalDeaths = level.match.totalDeaths;
		matchStats.totalSuicides = level.match.totalSuicides;
		matchStats.proBall_totalGoals = level.match.proBallGoals;
		matchStats.proBall_totalAssists = level.match.proBallAssists;
		matchStats.timeLimitSeconds = timeLimit ? timeLimit->integer * 60 : 0;
		matchStats.scoreLimit = GT_ScoreLimit();
		{
			std::lock_guard<std::mutex> logGuard(level.matchLogMutex);
			matchStats.eventLog.swap(level.match.eventLog);
			matchStats.deathLog.swap(level.match.deathLog);
			level.match.eventLog.clear();
			level.match.deathLog.clear();
		}

		if (HasFlag(matchStats.recordedFlags, GameFlags::CTF)) {
			const int64_t ctfTotalCaptures = level.match.ctfRedTeamTotalCaptures + level.match.ctfBlueTeamTotalCaptures;
			const int64_t ctfTotalAssists = level.match.ctfRedTeamTotalAssists + level.match.ctfBlueTeamTotalAssists;
			const int64_t ctfTotalDefends = level.match.ctfRedTeamTotalDefences + level.match.ctfBlueTeamTotalDefences;

			matchStats.ctf_totalFlagsCaptured = static_cast<int>(ctfTotalCaptures);
			matchStats.ctf_totalFlagAssists = static_cast<int>(ctfTotalAssists);
			matchStats.ctf_totalFlagDefends = static_cast<int>(ctfTotalDefends);

			json& ctfJson = matchStats.gametypeStats["ctf"];
			json& totalsJson = ctfJson["totals"];
			totalsJson["flagsCaptured"] = Json::Int64(ctfTotalCaptures);
			totalsJson["flagAssists"] = Json::Int64(ctfTotalAssists);
			totalsJson["flagDefends"] = Json::Int64(ctfTotalDefends);
			totalsJson["flagPickups"] = Json::Int64(level.match.ctfRedFlagPickupCount + level.match.ctfBlueFlagPickupCount);
			totalsJson["flagDrops"] = Json::Int64(level.match.ctfRedFlagDropCount + level.match.ctfBlueFlagDropCount);
			totalsJson["flagHoldTimeTotalMsec"] = Json::Int64(level.match.ctfRedFlagTotalHoldTimeMsec + level.match.ctfBlueFlagTotalHoldTimeMsec);
			totalsJson["flagHoldTimeShortestMsec"] = Json::Int64(level.match.ctfRedFlagShortestHoldTimeMsec + level.match.ctfBlueFlagShortestHoldTimeMsec);
			totalsJson["flagHoldTimeLongestMsec"] = Json::Int64(level.match.ctfRedFlagLongestHoldTimeMsec + level.match.ctfBlueFlagLongestHoldTimeMsec);

			json& teamsJson = ctfJson["teams"];
			json& redJson = teamsJson["red"];
			redJson["flagsCaptured"] = Json::Int64(level.match.ctfRedTeamTotalCaptures);
			redJson["flagAssists"] = Json::Int64(level.match.ctfRedTeamTotalAssists);
			redJson["flagDefends"] = Json::Int64(level.match.ctfRedTeamTotalDefences);
			redJson["flagPickups"] = Json::Int64(level.match.ctfRedFlagPickupCount);
			redJson["flagDrops"] = Json::Int64(level.match.ctfRedFlagDropCount);
			redJson["flagHoldTimeTotalMsec"] = Json::Int64(level.match.ctfRedFlagTotalHoldTimeMsec);
			redJson["flagHoldTimeShortestMsec"] = Json::Int64(level.match.ctfRedFlagShortestHoldTimeMsec);
			redJson["flagHoldTimeLongestMsec"] = Json::Int64(level.match.ctfRedFlagLongestHoldTimeMsec);

			json& blueJson = teamsJson["blue"];
			blueJson["flagsCaptured"] = Json::Int64(level.match.ctfBlueTeamTotalCaptures);
			blueJson["flagAssists"] = Json::Int64(level.match.ctfBlueTeamTotalAssists);
			blueJson["flagDefends"] = Json::Int64(level.match.ctfBlueTeamTotalDefences);
			blueJson["flagPickups"] = Json::Int64(level.match.ctfBlueFlagPickupCount);
			blueJson["flagDrops"] = Json::Int64(level.match.ctfBlueFlagDropCount);
			blueJson["flagHoldTimeTotalMsec"] = Json::Int64(level.match.ctfBlueFlagTotalHoldTimeMsec);
			blueJson["flagHoldTimeShortestMsec"] = Json::Int64(level.match.ctfBlueFlagShortestHoldTimeMsec);
			blueJson["flagHoldTimeLongestMsec"] = Json::Int64(level.match.ctfBlueFlagLongestHoldTimeMsec);
		}

		const bool hadTeams = matchStats.wasTeamMode;

		auto process_player = [&](gentity_t* ec) {
			auto* cl = ec->client;
			PlayerStats p;

			p.socialID = cl->sess.socialID;
			p.playerName = cl->sess.netName;
			p.skillRating = cl->sess.skillRating;
			p.skillRatingChange = cl->sess.skillRatingChange;
			p.totalKills = cl->pers.match.totalKills;
			p.totalSpawnKills = cl->pers.match.totalSpawnKills;
			p.totalTeamKills = cl->pers.match.totalTeamKills;
			p.totalDeaths = cl->pers.match.totalDeaths;
			p.totalSuicides = cl->pers.match.totalSuicides;
			p.calculateKDR();
			p.totalScore = cl->resp.score;
			p.proBallGoals = cl->pers.match.proBallGoals;
			p.proBallAssists = cl->pers.match.proBallAssists;
			p.totalShots = cl->pers.match.totalShots;
			p.totalHits = cl->pers.match.totalHits;
			p.totalDmgDealt = cl->pers.match.totalDmgDealt;
			p.totalDmgReceived = cl->pers.match.totalDmgReceived;
			p.ctfFlagPickups = cl->pers.match.ctfFlagPickups;
			p.ctfFlagDrops = cl->pers.match.ctfFlagDrops;
			p.ctfFlagReturns = cl->pers.match.ctfFlagReturns;
			p.ctfFlagAssists = cl->pers.match.ctfFlagAssists;
			p.ctfFlagCaptures = cl->pers.match.ctfFlagCaptures;
			p.ctfFlagCarrierTimeTotalMsec = static_cast<int64_t>(cl->pers.match.ctfFlagCarrierTimeTotalMsec);
			p.ctfFlagCarrierTimeShortestMsec = static_cast<int>(cl->pers.match.ctfFlagCarrierTimeShortestMsec);
			p.ctfFlagCarrierTimeLongestMsec = static_cast<int>(cl->pers.match.ctfFlagCarrierTimeLongestMsec);

		if (HasFlag(matchStats.recordedFlags, GameFlags::CTF)) {
				json& playerCtfJson = p.gametypeStats["ctf"];
				if (p.ctfFlagPickups > 0)
				playerCtfJson["flagPickups"] = p.ctfFlagPickups;
				if (p.ctfFlagDrops > 0)
				playerCtfJson["flagDrops"] = p.ctfFlagDrops;
				if (p.ctfFlagReturns > 0)
				playerCtfJson["flagReturns"] = p.ctfFlagReturns;
				if (p.ctfFlagAssists > 0)
				playerCtfJson["flagAssists"] = p.ctfFlagAssists;
				if (p.ctfFlagCaptures > 0)
				playerCtfJson["flagCaptures"] = p.ctfFlagCaptures;
				if (p.ctfFlagCarrierTimeTotalMsec > 0)
				playerCtfJson["flagCarrierTimeTotalMsec"] = Json::Int64(p.ctfFlagCarrierTimeTotalMsec);
				if (p.ctfFlagCarrierTimeShortestMsec > 0)
				playerCtfJson["flagCarrierTimeShortestMsec"] = p.ctfFlagCarrierTimeShortestMsec;
				if (p.ctfFlagCarrierTimeLongestMsec > 0)
				playerCtfJson["flagCarrierTimeLongestMsec"] = p.ctfFlagCarrierTimeLongestMsec;
			}

			p.playTimeMsec = cl->sess.playEndRealTime - cl->sess.playStartRealTime;
			if (p.playTimeMsec > 0)
				p.killsPerMinute = (p.totalKills * 60.0) / (p.playTimeMsec / 1000.0f);

			// Weapon stats
			for (size_t i = 0; i < weaponAbbreviations.size(); ++i) {
				int shots = cl->pers.match.totalShotsPerWeapon[i];
				int hits = cl->pers.match.totalHitsPerWeapon[i];
				if (shots > 0) {
					p.totalShotsPerWeapon[i] = shots;
					p.totalHitsPerWeapon[i] = hits;
					p.accuracyPerWeapon[i] = (double)hits / shots * 100.0;
				}
			}

			// Accuracy
			p.totalAccuracy = (p.totalShots > 0)
				? (double)p.totalHits / p.totalShots * 100.0
				: 0.0;

			// Pickup stats
			for (int i = static_cast<int>(HighValueItems::None) + 1; i < static_cast<int>(HighValueItems::Total); ++i) {
				p.pickupCounts[i] = cl->pers.match.pickupCounts[i];
				p.pickupDelays[i] = cl->pers.match.pickupDelay[i].seconds<double>();
			}

			// MOD stats
			for (const auto& mod : modr) {
				const size_t idx = static_cast<size_t>(mod.mod);
				int kills = cl->pers.match.modTotalKills[idx];
				int deaths = cl->pers.match.modTotalDeaths[idx];
				int dmgDealt = cl->pers.match.modTotalDmgD[idx];
				int dmgReceived = cl->pers.match.modTotalDmgR[idx];

				p.modTotalKills[idx] = kills;
				p.modTotalDeaths[idx] = deaths;
				p.modTotalDmgD[idx] = dmgDealt;
				p.modTotalDmgR[idx] = dmgReceived;

				if (kills > 0 || deaths > 0) {
					double kdr = (deaths > 0)
					? (double)kills / deaths
					: (kills > 0 ? (double)kills : 0.0);
					p.modTotalKDR[idx] = kdr;
				}
				else {
					p.modTotalKDR[idx] = 0.0;
				}
			}

			// Medals
			p.awards = cl->pers.match.medalCount;

			// Bot sanitization
			if (cl->sess.is_a_bot) {
				p.skillRating = 0;
				p.skillRatingChange = 0;
			}

			bool won = false;
			switch (cl->sess.team) {
			case Team::Red:
				if (level.teamScores[static_cast<int>(Team::Red)] > level.teamScores[static_cast<int>(Team::Blue)]) {
					won = true;
				}
				else {
					won = false;
				}
				break;
			case Team::Blue:
				if (level.teamScores[static_cast<int>(Team::Blue)] > level.teamScores[static_cast<int>(Team::Red)]) {
					won = true;
				}
				else {
					won = false;
				}
				break;
			default:
				won = (cl == &game.clients[level.sortedClients[0]]);
				break;
			}

			// Save persistent stats
			GetClientConfigStore().SaveStats(cl, won);

			return p;
			};

		if (hadTeams) {
			TeamStats redTeam = { "Red",  level.teamScores[static_cast<int>(Team::Red)],  level.teamScores[static_cast<int>(Team::Red)] > level.teamScores[static_cast<int>(Team::Blue)] ? "win" : "loss" };
			TeamStats blueTeam = { "Blue", level.teamScores[static_cast<int>(Team::Blue)], level.teamScores[static_cast<int>(Team::Blue)] > level.teamScores[static_cast<int>(Team::Red)] ? "win" : "loss" };

			for (auto ec : active_players()) {
				PlayerStats ps = process_player(ec);
				switch (ec->client->sess.team) {
				case Team::Red:  redTeam.players.push_back(ps); break;
				case Team::Blue: blueTeam.players.push_back(ps); break;
				default: break;
				}
			}

			matchStats.teams.push_back(redTeam);
			matchStats.teams.push_back(blueTeam);
		}
		else {
			for (auto ec : active_players()) {
				matchStats.players.push_back(process_player(ec));
			}
		}

		matchStats.calculateDuration();
		matchStats.avKillsPerMinute = matchStats.durationMS > 0
			? level.match.totalKills / (matchStats.durationMS / 60000.0f)
			: 0.0f;

		if (HasFlag(matchStats.recordedFlags, GameFlags::CTF)) {
			json& ctfPlayersJson = matchStats.gametypeStats["ctf"]["players"];
			if (!ctfPlayersJson.isArray())
				ctfPlayersJson = json(Json::arrayValue);

			auto appendPlayerGametypeStats = [&](const PlayerStats& player, const std::string& teamLabel) {
				if (!player.gametypeStats.isObject() || !player.gametypeStats.isMember("ctf"))
					return;
				const json& playerCtfJson = player.gametypeStats["ctf"];
				if (!JsonHasData(playerCtfJson))
					return;

				json entry;
				entry["socialID"] = player.socialID;
				const std::string& gametypeIdentifier = !player.socialID.empty() ? player.socialID : player.playerName;
				entry["playerIdentifier"] = gametypeIdentifier;
				entry["playerName"] = player.playerName;
				if (!teamLabel.empty())
					entry["team"] = teamLabel;
				entry["stats"] = playerCtfJson;
				ctfPlayersJson.append(entry);
			};

			for (const auto& player : matchStats.players) {
				appendPlayerGametypeStats(player, std::string());
			}

			for (const auto& team : matchStats.teams) {
				for (const auto& player : team.players) {
					appendPlayerGametypeStats(player, team.teamName);
				}
			}
		}

		std::unordered_set<std::string> accountedPlayerIDs;
		auto accumulateModTotals = [&](const std::vector<PlayerStats>& playersVec) {
			for (const auto& p : playersVec) {
				accountedPlayerIDs.insert(p.socialID);

				for (size_t idx = 0; idx < modr.size(); ++idx) {
					int kills = p.modTotalKills[idx];
					if (kills > 0) {
						const auto& modName = modr[idx].name;
						matchStats.totalKillsByMOD[modName] += kills;
					}

					int deaths = p.modTotalDeaths[idx];
					if (deaths > 0) {
						const auto& modName = modr[idx].name;
						matchStats.totalDeathsByMOD[modName] += deaths;
					}
				}
			}
			};

		accumulateModTotals(matchStats.players);
		for (const auto& team : matchStats.teams) {
			accumulateModTotals(team.players);
		}

		auto isAccounted = [&](const std::string& id) {
			return !id.empty() && accountedPlayerIDs.find(id) != accountedPlayerIDs.end();
			};

		for (const auto& e : matchStats.deathLog) {
			const auto& modName = modr[static_cast<int>(e.mod.id)].name;
			const bool attackerAccounted = isAccounted(e.attacker.id);
			const bool victimAccounted = isAccounted(e.victim.id);
			const bool environmentKill = e.attacker.id.empty() || e.attacker.id == "0";
			const bool suicide = !environmentKill && !e.attacker.id.empty() && e.attacker.id == e.victim.id;

			if (!victimAccounted) {
				matchStats.totalDeathsByMOD[modName]++;
			}

			if (!attackerAccounted && !environmentKill && !suicide) {
				matchStats.totalKillsByMOD[modName]++;
			}
		}

		for (auto& [modName, kills] : matchStats.totalKillsByMOD) {
			int deaths = matchStats.totalDeathsByMOD[modName];
			matchStats.totalKDRByMOD[modName] = deaths > 0
				? (double)kills / deaths
				: (double)kills;
		}

		ValidateModTotals(matchStats);
		SendIndividualMiniStats(matchStats);

		if (Tournament_IsActive() && game.tournament.configLoaded &&
			!game.tournament.seriesId.empty()) {
			auto& series = g_tournamentSeries[game.tournament.seriesId];
			if (series.matches.empty()) {
				series.seriesId = game.tournament.seriesId;
				series.name = game.tournament.name;
				series.bestOf = game.tournament.bestOf;
				series.winTarget = game.tournament.winTarget;
				series.teamBased = game.tournament.teamBased;
				series.gametype = game.tournament.gametype;
			}

			series.matches.push_back(matchStats.toJson());
			game.tournament.matchIds.push_back(matchStats.matchID);
			game.tournament.matchMaps.push_back(matchStats.mapName);

			if (game.tournament.seriesComplete) {
				const std::string seriesFileId = TournamentSeriesFileId(series.seriesId);
				const std::string seriesBasePath = MATCH_STATS_PATH + "/series_" + seriesFileId;
				TournamentSeries_WriteAll(series, seriesBasePath);
				g_tournamentSeries.erase(series.seriesId);
			}
		}

		MatchStats jobSnapshot = std::move(matchStats);
		std::string jobBasePath = MATCH_STATS_PATH + "/" + jobSnapshot.matchID;
		const uint64_t jobId = MatchStatsWorker_Enqueue(std::move(jobSnapshot), std::move(jobBasePath));
		const uint32_t pendingJobs = g_matchStatsPendingJobs.load();
		gi.Com_PrintFmt("{}: queued match stats job {} (pending: {}, completed: {}, failed: {})\n",
			__FUNCTION__,
			jobId,
			pendingJobs,
			g_matchStatsCompletedJobs.load(),
			g_matchStatsFailedJobs.load());
		matchStats = MatchStats{};
	}
	catch (const std::exception& e) {
		gi.Com_PrintFmt("{}: exception: {}\n", __FUNCTION__, e.what());
	}
}

/*
=============
MatchStats_Init
=============
*/
void MatchStats_Init() {
	if (!deathmatch->integer)
		return;

	// Clear any previous data
	matchStats = MatchStats{};
	level.match.deathLog.clear();
	level.match.eventLog.clear();

	level.matchID = std::format("{}_{}",
		GametypeIndexToString(static_cast<GameType>(g_gametype->integer)),
		FileTimeStamp());

	matchStats.matchID = level.matchID;
	//matchStats.startTime = level.matchStartRealTime.seconds();	// std::time(nullptr);

	gi.LocBroadcast_Print(PRINT_TTS, "$g_sgame_auto_4665bf2f3bac", level.matchID.c_str());

	G_LogEvent("MATCH START");
}
