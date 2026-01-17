#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

struct gclient_t;
struct Ghosts;
struct client_config_t;
struct local_game_import_t;

enum class Weapon : uint8_t;

namespace Json {
class Value;
}

class ClientConfigStore {
	public:
ClientConfigStore(local_game_import_t& gi, std::string playerConfigDirectory);

		bool LoadProfile(gclient_t* client, const std::string& playerID, const std::string& playerName, const std::string& gameType);
		void SaveStats(gclient_t* client, bool wonMatch);
		void SaveStatsForGhost(const Ghosts& ghost, bool wonMatch);
		void SaveWeaponPreferences(gclient_t* client);
		int DefaultSkillRating() const;
		std::string PlayerNameForSocialID(const std::string& socialID) const;

	private:
local_game_import_t& gi_;
		std::string playerConfigDirectory_;

		bool EnsurePlayerConfigDirectory() const;
		std::optional<std::string> PlayerConfigPathFromID(const std::string& playerID, const char* functionName) const;
		void CreateProfile(gclient_t* client, const std::string& playerID, const std::string& playerName, const std::string& gameType) const;
		void SaveInternal(const std::string& playerID, int skillRating, int skillChange, int64_t timePlayedSeconds, bool won, bool isGhost,
		bool updateStats, const client_config_t* pc = nullptr, const std::vector<Weapon>* weaponPrefs = nullptr) const;
		bool UpdateConfig(const std::string& playerID, const std::function<void(Json::Value&)>& updater) const;
		void ApplyWeaponPreferencesFromJson(gclient_t* client, const Json::Value& playerData) const;
		void ApplyVisualConfigFromJson(gclient_t* client, const Json::Value& playerData) const;
};

void InitializeClientConfigStore(local_game_import_t& gi, std::string playerConfigDirectory);
ClientConfigStore& GetClientConfigStore();

