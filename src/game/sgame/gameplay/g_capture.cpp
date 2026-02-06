/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

Modernized capture gameplay implementation covering Capture the Flag, One Flag CTF and Harvester
logic.*/

#include "../g_local.hpp"
#include "g_capture.hpp"
#include "g_teamplay.hpp"

#include <array>
#include <optional>
#include <string>

namespace CTF {
	inline constexpr int CAPTURE_BONUS = 100;
	inline constexpr int TEAM_BONUS = 25;
	inline constexpr int RECOVERY_BONUS = 10;
	inline constexpr int FLAG_BONUS = 10;
	inline constexpr int FRAG_CARRIER_BONUS = 20;

	inline constexpr GameTime FLAG_RETURN_TIME = 40_sec;

	inline constexpr int CARRIER_DANGER_PROTECT_BONUS = 5;
	inline constexpr int CARRIER_PROTECT_BONUS = 2;
	inline constexpr int FLAG_DEFENSE_BONUS = 10;
	inline constexpr int RETURN_FLAG_ASSIST_BONUS = 10;
	inline constexpr int FRAG_CARRIER_ASSIST_BONUS = 10;

	inline constexpr float TARGET_PROTECT_RADIUS = 1000.0f;
	inline constexpr float ATTACKER_PROTECT_RADIUS = 1000.0f;

	inline constexpr GameTime CARRIER_DANGER_PROTECT_TIMEOUT = 8_sec;
	inline constexpr GameTime FRAG_CARRIER_ASSIST_TIMEOUT = 10_sec;
	inline constexpr GameTime RETURN_FLAG_ASSIST_TIMEOUT = 10_sec;

	inline constexpr GameTime AUTO_FLAG_RETURN_TIMEOUT = 30_sec;
}

gentity_t* neutralObelisk = nullptr;

namespace {

	class FlagStateManager {
	public:
		FlagStateManager();

		[[nodiscard]] static FlagStateManager& Instance();

		void Reset();
		[[nodiscard]] bool SetStatus(Team team, FlagStatus status);
		[[nodiscard]] FlagStatus GetStatus(Team team) const;
		void SetTakenTime(Team team, GameTime time);
		[[nodiscard]] GameTime GetTakenTime(Team team) const;
		void RecordCapture(GameTime time, Team team);
		[[nodiscard]] GameTime LastCaptureTime() const;
		[[nodiscard]] Team LastCaptureTeam() const;
		void UpdateConfigString() const;

	private:
		struct FlagData {
			FlagStatus status = FlagStatus::AtBase;
			GameTime lastTaken = 0_sec;
		};

		[[nodiscard]] static std::optional<size_t> IndexForTeam(Team team);
		[[nodiscard]] FlagData& DataFor(Team team);
		[[nodiscard]] const FlagData& DataFor(Team team) const;
		[[nodiscard]] std::string BuildConfigString() const;

		std::array<FlagData, 3> data_{};
		GameTime lastCaptureTime_ = 0_sec;
		Team lastCaptureTeam_ = Team::None;
		std::array<GameTime, 2> obeliskAttackTime_{ 0_sec, 0_sec };
	};

	/*
	=============
	FlagStateManager::FlagStateManager

	Constructs the flag state manager with default values.
	=============
	*/
	FlagStateManager::FlagStateManager() {
		Reset();
	}

	/*
	=============
	FlagStateManager::Instance

	Returns the singleton flag state manager.
	=============
	*/
	FlagStateManager& FlagStateManager::Instance() {
		static FlagStateManager instance;
		return instance;
	}

	/*
	=============
	FlagStateManager::Reset

	Restores all tracked flag state to defaults.
	=============
	*/
	void FlagStateManager::Reset() {
		for (auto& entry : data_) {
			entry.status = FlagStatus::AtBase;
			entry.lastTaken = 0_sec;
		}
		lastCaptureTime_ = 0_sec;
		lastCaptureTeam_ = Team::None;
		obeliskAttackTime_ = { 0_sec, 0_sec };
		UpdateConfigString();
	}

	/*
	=============
	FlagStateManager::SetStatus

	Updates the stored flag status for the provided team.
	=============
	*/
	bool FlagStateManager::SetStatus(Team team, FlagStatus status) {
		const auto index = IndexForTeam(team);
		if (!index) {
			return false;
		}

		FlagData& data = data_.at(*index);
		if (data.status == status) {
			return false;
		}

		data.status = status;
		UpdateConfigString();
		return true;
	}

	/*
	=============
	FlagStateManager::GetStatus

	Returns the stored flag status for the provided team.
	=============
	*/
	FlagStatus FlagStateManager::GetStatus(Team team) const {
		const auto index = IndexForTeam(team);
		if (!index) {
			return FlagStatus::Invalid;
		}

		return data_.at(*index).status;
	}

	/*
	=============
	FlagStateManager::SetTakenTime

	Records the time at which the specified team's flag was last taken.
	=============
	*/
	void FlagStateManager::SetTakenTime(Team team, GameTime time) {
		const auto index = IndexForTeam(team);
		if (!index) {
			return;
		}

		data_.at(*index).lastTaken = time;
	}

	/*
	=============
	FlagStateManager::GetTakenTime

	Fetches the last recorded time a team's flag was taken.
	=============
	*/
	GameTime FlagStateManager::GetTakenTime(Team team) const {
		const auto index = IndexForTeam(team);
		if (!index) {
			return 0_sec;
		}

		return data_.at(*index).lastTaken;
	}

	/*
	=============
	FlagStateManager::RecordCapture

	Stores the most recent flag capture information.
	=============
	*/
	void FlagStateManager::RecordCapture(GameTime time, Team team) {
		lastCaptureTime_ = time;
		lastCaptureTeam_ = team;
	}

	/*
	=============
	FlagStateManager::LastCaptureTime

	Returns the time at which the last capture occurred.
	=============
	*/
	GameTime FlagStateManager::LastCaptureTime() const {
		return lastCaptureTime_;
	}

	/*
	=============
	FlagStateManager::LastCaptureTeam

	Returns the team that last captured a flag.
	=============
	*/
	Team FlagStateManager::LastCaptureTeam() const {
		return lastCaptureTeam_;
	}

	/*
	=============
	FlagStateManager::IndexForTeam

	Provides the array index backing a team's flag data.
	=============
	*/
	std::optional<size_t> FlagStateManager::IndexForTeam(Team team) {
		switch (team) {
		case Team::Red:
			return 0;
		case Team::Blue:
			return 1;
		case Team::Free:
			return 2;
		default:
			return std::nullopt;
		}
	}

	/*
	=============
	FlagStateManager::DataFor

	Fetches mutable flag data for the specified team.
	=============
	*/
	FlagStateManager::FlagData& FlagStateManager::DataFor(Team team) {
		const auto index = IndexForTeam(team);
		if (!index) {
			static FlagData dummy{};
			return dummy;
		}

		return data_.at(*index);
	}

	/*
	=============
	FlagStateManager::DataFor

	Fetches immutable flag data for the specified team.
	=============
	*/
	const FlagStateManager::FlagData& FlagStateManager::DataFor(Team team) const {
		const auto index = IndexForTeam(team);
		if (!index) {
			static FlagData dummy{};
			return dummy;
		}

		return data_.at(*index);
	}

	/*
	=============
	BuildFlagStatusPayload

	Constructs the configstring payload for the current flag states.
	=============
	*/
	std::string BuildFlagStatusPayload(bool captureTheFlagMode, const std::array<FlagStatus, 3>& statuses) {
		std::string flagStatusStr;
		flagStatusStr.reserve(captureTheFlagMode ? 2 : 3);

		static constexpr std::array<char, 5> ctfFlagStatusRemap{ '0', '1', '*', '*', '2' };
		static constexpr std::array<char, 5> oneFlagStatusRemap{ '0', '1', '2', '3', '4' };

		if (captureTheFlagMode) {
			flagStatusStr.push_back(ctfFlagStatusRemap.at(static_cast<int>(statuses.at(0))));
			flagStatusStr.push_back(ctfFlagStatusRemap.at(static_cast<int>(statuses.at(1))));
		}
		else {
			flagStatusStr.push_back(oneFlagStatusRemap.at(static_cast<int>(statuses.at(0))));
			flagStatusStr.push_back(oneFlagStatusRemap.at(static_cast<int>(statuses.at(1))));
			flagStatusStr.push_back(oneFlagStatusRemap.at(static_cast<int>(statuses.at(2))));
		}

		return flagStatusStr;
	}

	/*
	=============
	FlagStateManager::BuildConfigString

	Constructs the configstring payload for clients.
	=============
	*/
	std::string FlagStateManager::BuildConfigString() const {
		const std::array<FlagStatus, 3> statuses{
			DataFor(Team::Red).status,
			DataFor(Team::Blue).status,
			DataFor(Team::Free).status
		};
		return BuildFlagStatusPayload(Game::Is(GameType::CaptureTheFlag), statuses);
	}

	/*
	=============
	FlagStateManager::UpdateConfigString

	Sends updated flag state to connected clients.
	=============
	*/
	void FlagStateManager::UpdateConfigString() const {
		const std::string payload = BuildConfigString();
		gi.configString(CS_FLAGSTATUS, payload.c_str());
	}

	/*
	=============
	Flags

	Returns the shared flag state manager instance.
	=============
	*/
	FlagStateManager& Flags() {
		return FlagStateManager::Instance();
	}

	/*
	=============
	SupportsCTF

	Returns true when the current gametype supports CTF features.
	=============
	*/
	[[nodiscard]] bool SupportsCTF() {
		return Game::Has(GameFlags::CTF);
	}

	/*
	=============
	TeamFlagClassName

	Maps a team to its corresponding flag classname.
	=============
	*/
	[[nodiscard]] const char* TeamFlagClassName(Team team) {
		switch (team) {
		case Team::Red:
			return ITEM_CTF_FLAG_RED;
		case Team::Blue:
			return ITEM_CTF_FLAG_BLUE;
		case Team::Free:
			return ITEM_CTF_FLAG_NEUTRAL;
		default:
			return nullptr;
		}
	}

	/*
	=============
	TeamFlagItem

	Maps a team to its inventory identifier.
	=============
	*/
	[[nodiscard]] item_id_t TeamFlagItem(Team team) {
		switch (team) {
		case Team::Red:
			return IT_FLAG_RED;
		case Team::Blue:
			return IT_FLAG_BLUE;
		case Team::Free:
			return IT_FLAG_NEUTRAL;
		default:
			return IT_NULL;
		}
	}

	/*
	=============
	TeamFromFlagItem

	Maps a flag item identifier back to its owning team.
	=============
	*/
	[[nodiscard]] std::optional<Team> TeamFromFlagItem(item_id_t item) {
		switch (item) {
		case IT_FLAG_RED:
			return Team::Red;
		case IT_FLAG_BLUE:
			return Team::Blue;
		case IT_FLAG_NEUTRAL:
			return Team::Free;
		default:
			return std::nullopt;
		}
	}

	template <typename Flags>
	[[nodiscard]] bool HasSpawnFlagImpl(const Flags& flags, SpawnFlags flag) {
		if constexpr (requires(const Flags & f) { f.has(flag); }) {
			return flags.has(flag);
		}
		else {
			return (flags & flag) != 0;
		}
	}

	[[nodiscard]] bool HasSpawnFlag(const gentity_t* ent, SpawnFlags flag) {
		if (!ent) {
			return false;
		}
		return HasSpawnFlagImpl(ent->spawnFlags, flag);
	}

	[[nodiscard]] bool IsDroppedFlag(const gentity_t* ent) {
		return HasSpawnFlag(ent, SPAWNFLAG_ITEM_DROPPED);
	}

	[[nodiscard]] bool IsDroppedByPlayer(const gentity_t* ent) {
		return HasSpawnFlag(ent, SPAWNFLAG_ITEM_DROPPED_PLAYER);
	}

	[[nodiscard]] const char* TeamNameOrNeutral(Team team) {
		return team == Team::Free ? "NEUTRAL" : Teams_TeamName(team);
	}

	void ResetCarrierHurtTimers(Team team) {
		Teamplay_ForEachTeamMember(team, [](gentity_t* entity) {
			entity->client->resp.ctf_lasthurtcarrier = 0_ms;
			});
	}

	void AwardAssistBonuses(gentity_t* scorer) {
		Teamplay_ForEachClient([scorer](gentity_t* teammate) {
			if (teammate->client->sess.team != scorer->client->sess.team) {
				teammate->client->resp.ctf_lasthurtcarrier = -5_sec;
				return;
			}

			if (teammate == scorer) {
				return;
			}

			G_AdjustPlayerScore(teammate->client, CTF::TEAM_BONUS, false, 0);

			if (teammate->client->resp.ctf_lastreturnedflag &&
				teammate->client->resp.ctf_lastreturnedflag + CTF::RETURN_FLAG_ASSIST_TIMEOUT > level.time) {
				gi.LocBroadcast_Print(PRINT_HIGH, "$g_bonus_assist_return",
					G_ColorResetAfter(teammate->client->sess.netName).c_str());
				G_AdjustPlayerScore(teammate->client, CTF::RETURN_FLAG_ASSIST_BONUS, false, 0);
				teammate->client->pers.match.ctfFlagAssists++;
				PushAward(teammate, PlayerMedal::Assist);
			}

			if (teammate->client->resp.ctf_lastfraggedcarrier &&
				teammate->client->resp.ctf_lastfraggedcarrier + CTF::FRAG_CARRIER_ASSIST_TIMEOUT > level.time) {
				gi.LocBroadcast_Print(PRINT_HIGH, "$g_bonus_assist_frag_carrier",
					G_ColorResetAfter(teammate->client->sess.netName).c_str());
				G_AdjustPlayerScore(teammate->client, CTF::FRAG_CARRIER_ASSIST_BONUS, false, 0);
				teammate->client->pers.match.ctfFlagAssists++;
				PushAward(teammate, PlayerMedal::Assist);
			}
			});
	}

	/*
	=============
	ApplyCaptureRewards

	Handles post-capture scoring, assists, and medal awards.
	=============
	*/
	void ApplyCaptureRewards(gentity_t* flagEntity, gentity_t* scorer, Team scoringTeam) {
		if (!scorer || !scorer->client) {
			return;
		}

		if (flagEntity) {
			// Placeholder for any entity-dependent logic.
		}

		Flags().RecordCapture(level.time, scoringTeam);
		level.ctf_last_flag_capture = level.time;
		level.ctf_last_capture_team = scoringTeam;
		G_AdjustTeamScore(scoringTeam, Game::Is(GameType::CaptureStrike) ? 2 : 1);

		G_AdjustPlayerScore(scorer->client, CTF::CAPTURE_BONUS, false, 0);
		PushAward(scorer, PlayerMedal::Captures);

		AwardAssistBonuses(scorer);
	}

	/*
	=============
	BroadcastCaptureMessage

	Informs all players about a completed capture.
	=============
	*/
	void BroadcastCaptureMessage(Team scoringTeam, gentity_t* scorer, GameTime pickupTime) {
		if (!scorer || !scorer->client) {
			return;
		}

		if (pickupTime) {
			gi.LocBroadcast_Print(
				PRINT_HIGH,
				"$g_sgame_auto_4e27928143af",
				Teams_TeamName(scoringTeam),
				G_ColorResetAfter(scorer->client->sess.netName).c_str(),
				TimeString((level.time - pickupTime).milliseconds(), true, false));
		}
		else {
			gi.LocBroadcast_Print(
				PRINT_HIGH,
				"$g_sgame_auto_e9691a6f20ec",
				Teams_TeamName(scoringTeam),
				G_ColorResetAfter(scorer->client->sess.netName).c_str());
		}
	}

	/*
	=============
	FindTeamFlag

	Finds an in-world flag entity for the specified team.
	=============
	*/
	[[nodiscard]] std::optional<gentity_t*> FindTeamFlag(Team team) {
		const char* className = TeamFlagClassName(team);
		if (!className) {
			return std::nullopt;
		}

		gentity_t* flag = nullptr;
		while ((flag = G_FindByString<&gentity_t::className>(flag, className)) != nullptr) {
			if (!IsDroppedFlag(flag)) {
				return flag;
			}
		}

		return std::nullopt;
	}

	/*
	=============
	FindFlagCarrier

	Searches for the player currently holding a specific flag.
	=============
	*/
	[[nodiscard]] gentity_t* FindFlagCarrier(item_id_t flagItem) {
		gentity_t* carrier = nullptr;
		Teamplay_ForEachClient([&carrier, flagItem](gentity_t* entity) {
			if (!carrier && entity->client->pers.inventory[flagItem]) {
				carrier = entity;
			}
			});
		return carrier;
	}

	/*
	=============
	AwardBaseDefense

	Grants a defender bonus for protecting the base.
	=============
	*/
	void AwardBaseDefense(gentity_t* attacker) {
		if (!attacker || !attacker->client) {
			return;
		}
		G_AdjustPlayerScore(attacker->client, CTF::FLAG_DEFENSE_BONUS, false, 0);
		PushAward(attacker, PlayerMedal::Defence);
	}

	/*
	=============
	AwardCarrierDefense

	Grants a defender bonus for protecting the flag carrier.
	=============
	*/
	void AwardCarrierDefense(gentity_t* attacker) {
		if (!attacker || !attacker->client) {
			return;
		}
		G_AdjustPlayerScore(attacker->client, CTF::CARRIER_PROTECT_BONUS, false, 0);
	}

	/*
	=============
	AwardCarrierDangerDefense

	Grants a defender bonus for eliminating threats near the carrier.
	=============
	*/
	void AwardCarrierDangerDefense(gentity_t* attacker) {
		if (!attacker || !attacker->client) {
			return;
		}
		G_AdjustPlayerScore(attacker->client, CTF::CARRIER_DANGER_PROTECT_BONUS, false, 0);
		PushAward(attacker, PlayerMedal::Defence);
	}

	constexpr GameTime FLAG_RETURN_SOUND_COOLDOWN = 2_sec;

	/*
	=============
	ReturnSoundIndex

	Maps a team to its throttling slot for flag return audio.
	=============
	*/
	[[nodiscard]] std::optional<size_t> ReturnSoundIndex(Team team) {
		switch (team) {
		case Team::Red:
			return 0;
		case Team::Blue:
			return 1;
		case Team::Free:
			return 2;
		default:
			return std::nullopt;
		}
	}

	struct FlagReturnSoundState {
		std::array<GameTime, 3> lastPlayed{ 0_sec, 0_sec, 0_sec };
	};

	/*
	=============
	ReturnSoundState

	Provides access to the shared flag return sound throttle state.
	=============
	*/
	FlagReturnSoundState& ReturnSoundState() {
		static FlagReturnSoundState state;
		return state;
	}

	/*
	=============
	PlayTeamAnnouncer

	Dispatches a localized announcer cue to every member of a team.
	=============
	*/
	void PlayTeamAnnouncer(Team team, const char* soundKey) {
		if (!soundKey || !Teamplay_IsTeamValid(team)) {
			return;
		}

		Teamplay_ForEachTeamMember(team, [soundKey](gentity_t* entity) {
			AnnouncerSound(entity, soundKey);
			});
	}

	/*
	=============
	Team_ReturnFlagSound

	Triggers team-scoped flag return SFX and VO cues.
	=============
	*/
	void Team_ReturnFlagSound(Team team) {
		if (!SupportsCTF()) {
			return;
		}

		const auto index = ReturnSoundIndex(team);
		if (!index) {
			return;
		}

		auto& lastPlayed = ReturnSoundState().lastPlayed[*index];
		if (lastPlayed != 0_sec && level.time < lastPlayed + FLAG_RETURN_SOUND_COOLDOWN) {
			return;
		}

		lastPlayed = level.time;

		constexpr soundchan_t SOUND_FLAGS = static_cast<soundchan_t>(CHAN_RELIABLE | CHAN_NO_PHS_ADD | CHAN_AUX);
		constexpr float VOLUME = 1.0f;
		constexpr float ATTENUATION = ATTN_NONE;
		constexpr uint32_t DUPE_KEY = 0;

		if (world) {
			const int sfx = gi.soundIndex("ctf/flagret.wav");
			gi.sound(world, SOUND_FLAGS, sfx, VOLUME, ATTENUATION, DUPE_KEY);
		}

		if (Teamplay_IsPrimaryTeam(team)) {
			const char* worldCue = team == Team::Red ? "red_flag_returned" : "blue_flag_returned";
			AnnouncerSound(world, worldCue);
			PlayTeamAnnouncer(team, "your_flag_returned");
			const Team enemy = Teams_OtherTeam(team);
			if (Teamplay_IsTeamValid(enemy)) {
				PlayTeamAnnouncer(enemy, "enemy_flag_returned");
			}
		}
		else if (team == Team::Free) {
			AnnouncerSound(world, "enemy_flag_returned");
		}
	}

	/*
	=============
	Team_TakeFlagSound

	Handles audio triggers for flag pickups while throttling repeats.
	=============
	*/
	void Team_TakeFlagSound(Team team) {
		Team targetTeam = Team::None;
		const char* announcerKey = nullptr;

		switch (team) {
		case Team::Red:
			if (Flags().GetStatus(Team::Blue) != FlagStatus::AtBase &&
				Flags().GetTakenTime(Team::Blue) > level.time - 5_sec) {
				return;
			}
			Flags().SetTakenTime(Team::Blue, level.time);
			targetTeam = Team::Blue;
			announcerKey = "red_flag_taken";
			break;
		case Team::Blue:
			if (Flags().GetStatus(Team::Red) != FlagStatus::AtBase &&
				Flags().GetTakenTime(Team::Red) > level.time - 5_sec) {
				return;
			}
			Flags().SetTakenTime(Team::Red, level.time);
			targetTeam = Team::Red;
			announcerKey = "blue_flag_taken";
			break;
		case Team::Free:
			if (Flags().GetStatus(Team::Free) != FlagStatus::AtBase &&
				Flags().GetTakenTime(Team::Free) > level.time - 5_sec) {
				return;
			}
			Flags().SetTakenTime(Team::Free, level.time);
			targetTeam = Team::Free;
			announcerKey = "enemy_flag_taken";
			break;
		default:
			return;
		}

		if (world) {
			gi.sound(world, CHAN_RELIABLE | CHAN_NO_PHS_ADD | CHAN_AUX, gi.soundIndex("ctf/flagtk.wav"), 1, ATTN_NONE, 0);
		}

		if (announcerKey) {
			AnnouncerSound(world, announcerKey);
		}

		if (targetTeam != Team::None) {
			gi.LocBroadcast_Print(
				PRINT_HIGH,
				"$g_sgame_auto_855da5256f5a",
				Teams_TeamName(targetTeam),
				Teams_TeamName(team));
		}
	}


	/*
	=============
	Team_CaptureFlagSound_Internal

	Plays the capture stinger and announcer VO in a network-reliable way.
	=============
	*/
	void Team_CaptureFlagSound_Internal(Team team) {
		if (!SupportsCTF()) {
			return;
		}

		constexpr soundchan_t SOUND_FLAGS = static_cast<soundchan_t>(CHAN_RELIABLE | CHAN_NO_PHS_ADD | CHAN_AUX);
		constexpr float SOUND_VOLUME = 1.0f;
		constexpr float SOUND_ATTENUATION = ATTN_NONE;
		constexpr uint32_t SOUND_OFFSET = 0;

		const int captureSound = gi.soundIndex("ctf/flagcap.wav");
		bool playedCaptureSound = false;

		if (Teamplay_IsPrimaryTeam(team)) {
			if (auto flagEntity = FindTeamFlag(team); flagEntity && *flagEntity) {
				gi.sound(*flagEntity, SOUND_FLAGS, captureSound, SOUND_VOLUME, SOUND_ATTENUATION, SOUND_OFFSET);
				playedCaptureSound = true;
			}
		}

		if (!playedCaptureSound) {
			gi.positionedSound(world->s.origin, world, SOUND_FLAGS, captureSound, SOUND_VOLUME, SOUND_ATTENUATION, SOUND_OFFSET);
		}

		const char* announcerKey = nullptr;
		switch (team) {
		case Team::Red:
			announcerKey = "red_scores";
			break;
		case Team::Blue:
			announcerKey = "blue_scores";
			break;
		default:
			break;
		}

		static GameTime lastAnnounceTime = 0_ms;
		static Team lastAnnounceTeam = Team::None;
		const GameTime referenceTime = level.ctf_last_flag_capture ? level.ctf_last_flag_capture : level.time;
		const bool shouldAnnounce = announcerKey &&
			(referenceTime != lastAnnounceTime || team != lastAnnounceTeam);

		if (shouldAnnounce) {
			AnnouncerSound(world, announcerKey);
			lastAnnounceTime = referenceTime;
			lastAnnounceTeam = team;
		}
	}

	/*
	=============
	GiveFlagToPlayer

	Assigns the picked-up flag to the player and updates state.
	=============
	*/
	void GiveFlagToPlayer(gentity_t* flagEntity, gentity_t* player, Team flagTeam, item_id_t flagItem) {
		if (!flagEntity || !player || !player->client) {
			return;
		}

		player->client->pers.inventory[flagItem] = 1;
		player->client->resp.ctf_flagsince = level.time;
		player->client->pers.match.ctfFlagPickups++;

		if (flagItem == IT_FLAG_NEUTRAL) {
			FlagStatus status = FlagStatus::Taken;
			switch (player->client->sess.team) {
			case Team::Red:
				status = FlagStatus::TakenRed;
				break;
			case Team::Blue:
				status = FlagStatus::TakenBlue;
				break;
			default:
				break;
			}
			if (!Flags().SetStatus(Team::Free, status)) {
				// Status already matched; nothing to update.
			}
			flagEntity->fteam = player->client->sess.team;
		}
		else {
			if (!Flags().SetStatus(flagTeam, FlagStatus::Taken)) {
				// Status already matched; nothing to update.
			}
		}

		Team_TakeFlagSound(player->client->sess.team);
	}

	/*
	=============
	RemoveDroppedFlag

	Removes a dropped flag entity from the world.
	=============
	*/
	void RemoveDroppedFlag(gentity_t* ent) {
		if (!ent) {
			return;
		}

		FreeEntity(ent);
	}

	/*
	=============
	RespawnFlag

	Respawns a flag entity at its original location.
	=============
	*/
	void RespawnFlag(gentity_t* ent, Team team) {
		if (!ent) {
			return;
		}

		ent->svFlags &= ~SVF_NOCLIENT;
		ent->solid = SOLID_TRIGGER;
		gi.linkEntity(ent);
		ent->s.event = EV_ITEM_RESPAWN;
		if (team == Team::Free) {
			ent->fteam = Team::Free;
		}
	}

}

/*
=============
CTF_RecordCarrierTime

Aggregates carrier duration statistics for the provided client.
=============
*/
static void CTF_RecordCarrierTime(gclient_t* client, GameTime pickupTime) {
	if (!client) {
		return;
	}

	GameTime startTime = pickupTime;
	if (!startTime) {
		startTime = client->resp.ctf_flagsince;
	}
	if (!startTime) {
		startTime = client->pers.teamState.flag_pickup_time;
	}
	if (!startTime) {
		return;
	}

	const GameTime elapsed = level.time - startTime;
	const int64_t elapsedMs = elapsed.milliseconds();
	if (elapsedMs <= 0) {
		return;
	}

	auto& match = client->pers.match;
	match.ctfFlagCarrierTimeTotalMsec += static_cast<uint64_t>(elapsedMs);
	const uint32_t duration = static_cast<uint32_t>(elapsedMs);
	if (match.ctfFlagCarrierTimeShortestMsec == 0 || duration < match.ctfFlagCarrierTimeShortestMsec) {
		match.ctfFlagCarrierTimeShortestMsec = duration;
	}
	if (duration > match.ctfFlagCarrierTimeLongestMsec) {
		match.ctfFlagCarrierTimeLongestMsec = duration;
	}
}

/*
=============
Team_CaptureFlagSound

External entry point for capture VO triggers.
=============
*/
void Team_CaptureFlagSound(Team team) {
	Team_CaptureFlagSound_Internal(team);
}

/*
=============
SetFlagStatus

External entry point to modify flag status.
=============
*/
bool SetFlagStatus(Team team, FlagStatus status) {
	return Flags().SetStatus(team, status);
}

/*
=============
GetFlagStatus

External entry point to read flag status for a team.
=============
*/
FlagStatus GetFlagStatus(Team team) {
	return Flags().GetStatus(team);
}

/*
=============
AwardFlagCapture

Awards a player and team for capturing a flag.
=============
*/
void AwardFlagCapture(gentity_t* flagEntity, gentity_t* scorer, Team scoringTeam, GameTime pickupTime) {
	BroadcastCaptureMessage(scoringTeam, scorer, pickupTime);
	ApplyCaptureRewards(flagEntity, scorer, scoringTeam);
	Team_CaptureFlagSound_Internal(scoringTeam);
	if (scorer && scorer->client) {
		CTF_RecordCarrierTime(scorer->client, pickupTime);
		scorer->client->pers.match.ctfFlagCaptures++;
	}
}

/*
=============
Team_ReturnFlag

Handles logic for returning a flag to its base.
=============
*/
static void Team_ReturnFlag(Team team) {
	if (!CTF_ResetTeamFlag(team)) {
		return;
	}

	Team_ReturnFlagSound(team);

	if (team == Team::Free) {
		gi.LocBroadcast_Print(PRINT_HIGH, "$g_sgame_auto_b10cd82591da");
	}
	else {
		gi.LocBroadcast_Print(PRINT_HIGH, "$g_flag_returned", Teams_TeamName(team));
	}
}

/*
=============
Team_CheckDroppedItem

Updates flag status for dropped flag entities.
=============
*/
[[maybe_unused]] static void Team_CheckDroppedItem(gentity_t* dropped) {
	if (!SupportsCTF() || !dropped || !dropped->item) {
		return;
	}

	switch (dropped->item->id) {
	case IT_FLAG_RED:
		SetFlagStatus(Team::Red, FlagStatus::Dropped);
		break;
	case IT_FLAG_BLUE:
		SetFlagStatus(Team::Blue, FlagStatus::Dropped);
		break;
	case IT_FLAG_NEUTRAL:
		SetFlagStatus(Team::Free, FlagStatus::Dropped);
		break;
	default:
		break;
	}
}

/*
=============
CTF_ScoreBonuses

Awards context-sensitive bonuses related to flag interactions.
=============
*/
void CTF_ScoreBonuses(gentity_t* targ, gentity_t*, gentity_t* attacker) {
	if (!SupportsCTF()) {
		return;
	}
	if (!targ || !attacker || !targ->client || !attacker->client || targ == attacker) {
		return;
	}

	const Team targetTeam = targ->client->sess.team;
	const Team otherTeam = Teams_OtherTeam(targetTeam);
	if (otherTeam < Team::None) {
		return;
	}

	const item_id_t flagItem = targetTeam == Team::Red ? IT_FLAG_RED : IT_FLAG_BLUE;
	const item_id_t enemyFlagItem = targetTeam == Team::Red ? IT_FLAG_BLUE : IT_FLAG_RED;

	if (targ->client->pers.inventory[enemyFlagItem]) {
		attacker->client->resp.ctf_lastfraggedcarrier = level.time;
		G_AdjustPlayerScore(attacker->client, CTF::FRAG_CARRIER_BONUS, false, 0);
	gi.LocBroadcast_Print(PRINT_MEDIUM, "$g_sgame_auto_ebb9672c8db9",
		G_ColorResetAfter(attacker->client->sess.netName).c_str(),
		Teams_TeamName(targetTeam));
		ResetCarrierHurtTimers(otherTeam);
		return;
	}

	if (targ->client->resp.ctf_lasthurtcarrier &&
		level.time - targ->client->resp.ctf_lasthurtcarrier < CTF::CARRIER_DANGER_PROTECT_TIMEOUT &&
		!attacker->client->pers.inventory[flagItem]) {
		AwardCarrierDangerDefense(attacker);
		return;
	}

	auto flagEntity = FindTeamFlag(attacker->client->sess.team);
	if (!flagEntity) {
		return;
	}

	Vector3 v1 = targ->s.origin - (*flagEntity)->s.origin;
	Vector3 v2 = attacker->s.origin - (*flagEntity)->s.origin;
	const float targetDistance = v1.length();
	const float attackerDistance = v2.length();
	const bool flagHasLineOfSight = LocCanSee(*flagEntity, targ) || LocCanSee(*flagEntity, attacker);
	const bool bothNearFlag = targetDistance < CTF::TARGET_PROTECT_RADIUS && attackerDistance < CTF::TARGET_PROTECT_RADIUS;

	if ((flagHasLineOfSight || bothNearFlag) && attacker->client->sess.team != targetTeam) {
		AwardBaseDefense(attacker);
		return;
	}

	gentity_t* carrier = FindFlagCarrier(flagItem);
	if (!carrier || carrier == attacker) {
		return;
	}

	v1 = targ->s.origin - carrier->s.origin;
	v2 = attacker->s.origin - carrier->s.origin;

	if (v1.length() < CTF::ATTACKER_PROTECT_RADIUS ||
		v2.length() < CTF::ATTACKER_PROTECT_RADIUS ||
		LocCanSee(carrier, targ) || LocCanSee(carrier, attacker)) {
		AwardCarrierDefense(attacker);
	}
}

/*
=============
CTF_CheckHurtCarrier

Tracks when a player damages the opposing flag carrier.
=============
*/
void CTF_CheckHurtCarrier(gentity_t* targ, gentity_t* attacker) {
	if (!SupportsCTF()) {
		return;
	}
	if (!targ || !attacker || !targ->client || !attacker->client) {
		return;
	}

	const item_id_t flagItem = targ->client->sess.team == Team::Red ? IT_FLAG_BLUE : IT_FLAG_RED;
	const Team targetTeam = targ->client->sess.team;
	const Team attackerTeam = attacker->client->sess.team;
	if (!Teamplay_IsPrimaryTeam(targetTeam) || !Teamplay_IsPrimaryTeam(attackerTeam)) {
		return;
	}

	if (targ->client->pers.inventory[flagItem] &&
		targetTeam != attackerTeam) {
		attacker->client->resp.ctf_lasthurtcarrier = level.time;
	}
}

/*
=============
CTF_ResetTeamFlag

Resets a team's flag to its spawn state.
=============
*/
bool CTF_ResetTeamFlag(Team team) {
	if (!SupportsCTF()) {
		return false;
	}

	const char* className = TeamFlagClassName(team);
	if (!className) {
		return false;
	}

	bool found = false;
	bool restored = false;
	gentity_t* ent = nullptr;
	while ((ent = G_FindByString<&gentity_t::className>(ent, className)) != nullptr) {
		if (IsDroppedFlag(ent) || IsDroppedByPlayer(ent)) {
			RemoveDroppedFlag(ent);
			found = true;
			restored = true;
		}
		else {
			RespawnFlag(ent, team);
			found = true;
			restored = true;
		}
	}

	if (found) {
		SetFlagStatus(team, FlagStatus::AtBase);
		if (restored && Game::Is(GameType::CaptureStrike)) {
			const Team defendingTeam = level.strike_red_attacks ? Team::Blue : Team::Red;
			if (team == defendingTeam) {
				level.strike_flag_touch = false;
			}
		}
	}

	return found;
}

/*
=============
CTF_ResetFlags

Resets all flags for the current gametype.
=============
*/
void CTF_ResetFlags() {
	if (!SupportsCTF()) {
		return;
	}

	CTF_ResetTeamFlag(Team::Red);
	CTF_ResetTeamFlag(Team::Blue);
	if (Game::Is(GameType::OneFlag)) {
		CTF_ResetTeamFlag(Team::Free);
	}
}

/*
=============
CTF_PickupFlag

Handles logic when a player touches a flag.
=============
*/
bool CTF_PickupFlag(gentity_t* ent, gentity_t* other) {
	if (!SupportsCTF() || !ent || !ent->item || !other || !other->client) {
		return false;
	}

	const auto flagTeamOpt = TeamFromFlagItem(ent->item->id);
	if (!flagTeamOpt) {
		gi.LocClient_Print(other, PRINT_HIGH, "$g_sgame_auto_1ad317f44749");
		RemoveDroppedFlag(ent);
		return false;
	}

	const Team flagTeam = *flagTeamOpt;
	const Team playerTeam = other->client->sess.team;
	const item_id_t flagItem = TeamFlagItem(flagTeam);
	const item_id_t enemyFlagItem = Teamplay_IsPrimaryTeam(flagTeam) ? TeamFlagItem(Teams_OtherTeam(flagTeam)) : IT_NULL;
	const bool droppedFlag = IsDroppedFlag(ent);
	const bool oneFlag = Game::Is(GameType::OneFlag);

	if (flagTeam == playerTeam && Teamplay_IsPrimaryTeam(playerTeam)) {
		if (!droppedFlag) {
			if (enemyFlagItem != IT_NULL && other->client->pers.inventory[enemyFlagItem]) {
				GameTime pickupTime = other->client->pers.teamState.flag_pickup_time;
				if (!pickupTime) {
					pickupTime = other->client->resp.ctf_flagsince;
				}
				other->client->pers.inventory[enemyFlagItem] = 0;
				other->client->resp.ctf_flagsince = 0_ms;

				AwardFlagCapture(ent, other, flagTeam, pickupTime);
				CTF_ResetFlags();

				if (Game::Is(GameType::CaptureStrike)) {
					gi.LocBroadcast_Print(PRINT_CENTER, "$g_sgame_auto_a448dc5ba7c1", Teams_TeamName(flagTeam));
					Round_End();
				}

				return false;
			}
			return false;
		}

	gi.LocBroadcast_Print(PRINT_HIGH, "$g_returned_flag",
		G_ColorResetAfter(other->client->sess.netName).c_str(),
		Teams_TeamName(flagTeam));
		G_AdjustPlayerScore(other->client, CTF::RECOVERY_BONUS, false, 0);
		other->client->resp.ctf_lastreturnedflag = level.time;
		other->client->pers.match.ctfFlagReturns++;
		gi.sound(ent, CHAN_RELIABLE | CHAN_NO_PHS_ADD | CHAN_AUX, gi.soundIndex("ctf/flagret.wav"), 1, ATTN_NONE, 0);
		SetFlagStatus(flagTeam, FlagStatus::AtBase);
		CTF_ResetTeamFlag(flagTeam);
		return false;
	}

	if (oneFlag && !droppedFlag && flagTeam != Team::Free && Teamplay_IsPrimaryTeam(playerTeam) &&
		other->client->pers.inventory[IT_FLAG_NEUTRAL]) {
		const Team scoringTeam = playerTeam;
		GameTime pickupTime = other->client->pers.teamState.flag_pickup_time;
		if (!pickupTime) {
			pickupTime = other->client->resp.ctf_flagsince;
		}
		other->client->pers.inventory[IT_FLAG_NEUTRAL] = 0;
		other->client->resp.ctf_flagsince = 0_ms;
		other->client->pers.teamState.flag_pickup_time = 0_ms;

		AwardFlagCapture(ent, other, scoringTeam, pickupTime);
		CTF_ResetTeamFlag(Team::Free);
		return false;
	}

	if (Game::Is(GameType::CaptureStrike)) {
		if ((level.strike_red_attacks && playerTeam != Team::Red) ||
			(!level.strike_red_attacks && playerTeam != Team::Blue)) {
			return false;
		}
	}

	if (!droppedFlag) {
		other->client->pers.teamState.flag_pickup_time = level.time;
	}

	gi.LocBroadcast_Print(PRINT_HIGH, "$g_got_flag",
		G_ColorResetAfter(other->client->sess.netName).c_str(),
		TeamNameOrNeutral(flagTeam));
	G_AdjustPlayerScore(other->client, CTF::FLAG_BONUS, false, 0);
	if (Game::Is(GameType::CaptureStrike) && !level.strike_flag_touch) {
		level.strike_flag_touch = true;
	}

	GiveFlagToPlayer(ent, other, flagTeam, flagItem);
	return true;
}

/*
=============
CTF_DropFlagTouch

Touch handler for dropped flag entities.
=============
*/
static TOUCH(CTF_DropFlagTouch)(gentity_t* ent, gentity_t* other, const trace_t& tr, bool otherTouchingSelf)->void {
	if (!SupportsCTF()) {
		return;
	}

	if (other == ent->owner && ent->nextThink - level.time > CTF::AUTO_FLAG_RETURN_TIMEOUT - 2_sec) {
		return;
	}

	Touch_Item(ent, other, tr, otherTouchingSelf);
}

/*
=============
CTF_DropFlagThink

Think function for handling automatic flag returns.
=============
*/
static THINK(CTF_DropFlagThink)(gentity_t* ent)->void {
	if (!SupportsCTF() || !ent || !ent->item) {
		return;
	}

	switch (ent->item->id) {
	case IT_FLAG_RED:
		CTF_ResetTeamFlag(Team::Red);
		gi.LocBroadcast_Print(PRINT_HIGH, "$g_flag_returned", Teams_TeamName(Team::Red));
		break;
	case IT_FLAG_BLUE:
		CTF_ResetTeamFlag(Team::Blue);
		gi.LocBroadcast_Print(PRINT_HIGH, "$g_flag_returned", Teams_TeamName(Team::Blue));
		break;
	case IT_FLAG_NEUTRAL:
		Team_ReturnFlag(Team::Free);
		break;
	default:
		break;
	}

	gi.sound(ent, CHAN_RELIABLE | CHAN_NO_PHS_ADD | CHAN_AUX, gi.soundIndex("ctf/flagret.wav"), 1, ATTN_NONE, 0);
}

/*
=============
CTF_DeadDropFlag

Drops any carried flags when a player dies.
=============
*/
void CTF_DeadDropFlag(gentity_t* self) {
	if (!SupportsCTF() || !self || !self->client) {
		return;
	}

	gentity_t* dropped = nullptr;
	Team droppedTeam = Team::None;

	if (self->client->pers.inventory[IT_FLAG_RED]) {
		dropped = Drop_Item(self, GetItemByIndex(IT_FLAG_RED));
		self->client->pers.inventory[IT_FLAG_RED] = 0;
		gi.LocBroadcast_Print(PRINT_HIGH, "$g_lost_flag",
			G_ColorResetAfter(self->client->sess.netName).c_str(),
			Teams_TeamName(Team::Red));
		droppedTeam = Team::Red;
	}
	else if (self->client->pers.inventory[IT_FLAG_BLUE]) {
		dropped = Drop_Item(self, GetItemByIndex(IT_FLAG_BLUE));
		self->client->pers.inventory[IT_FLAG_BLUE] = 0;
		gi.LocBroadcast_Print(PRINT_HIGH, "$g_lost_flag",
			G_ColorResetAfter(self->client->sess.netName).c_str(),
			Teams_TeamName(Team::Blue));
		droppedTeam = Team::Blue;
	}
	else if (self->client->pers.inventory[IT_FLAG_NEUTRAL]) {
		dropped = Drop_Item(self, GetItemByIndex(IT_FLAG_NEUTRAL));
		self->client->pers.inventory[IT_FLAG_NEUTRAL] = 0;
		gi.LocBroadcast_Print(PRINT_HIGH, "$g_lost_flag",
			G_ColorResetAfter(self->client->sess.netName).c_str(),
			Teams_TeamName(Team::Free));
		droppedTeam = Team::Free;
	}

	if (droppedTeam != Team::None) {
		GameTime carryStart = self->client->resp.ctf_flagsince;
		if (!carryStart) {
			carryStart = self->client->pers.teamState.flag_pickup_time;
		}
		CTF_RecordCarrierTime(self->client, carryStart);
		self->client->pers.match.ctfFlagDrops++;
		self->client->resp.ctf_flagsince = 0_ms;
	}

	self->client->pers.teamState.flag_pickup_time = 0_ms;

	if (!dropped) {
		return;
	}

	dropped->think = CTF_DropFlagThink;
	dropped->nextThink = level.time + CTF::AUTO_FLAG_RETURN_TIMEOUT;
	dropped->touch = CTF_DropFlagTouch;
	dropped->fteam = self->client->sess.team;

	switch (droppedTeam) {
	case Team::Red:
		SetFlagStatus(Team::Red, FlagStatus::Dropped);
		break;
	case Team::Blue:
		SetFlagStatus(Team::Blue, FlagStatus::Dropped);
		break;
	case Team::Free:
		SetFlagStatus(Team::Free, FlagStatus::Dropped);
		break;
	default:
		break;
	}
}

/*
=============
CTF_DropFlag

Handles manual flag drops triggered by a player.
=============
*/
void CTF_DropFlag(gentity_t* ent, Item*) {
	if (!SupportsCTF() || !ent || !ent->client) {
		return;
	}

	ent->client->pers.teamState.flag_pickup_time = 0_ms;

	if (brandom()) {
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_lusers_drop_flags");
	}
	else {
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_winners_drop_flags");
	}
}

/*
=============
CTF_FlagThink

Animates flag models while active in the world.
=============
*/
static THINK(CTF_FlagThink)(gentity_t* ent)->void {
	if (!SupportsCTF() || !ent) {
		return;
	}

	if (ent->solid != SOLID_NOT) {
		ent->s.frame = 173 + (((ent->s.frame - 173) + 1) % 16);
	}
	ent->nextThink = level.time + 10_hz;
}

/*
=============
CTF_FlagSetup

Initialises static flag entities when they spawn.
=============
*/
THINK(CTF_FlagSetup)(gentity_t* ent)->void {
	if (!SupportsCTF() || !ent) {
		return;
	}

	ent->mins = { -15, -15, -15 };
	ent->maxs = { 15, 15, 15 };

	if (ent->model) {
		gi.setModel(ent, ent->model);
	}
	else if (ent->item) {
		gi.setModel(ent, ent->item->worldModel);
	}

	ent->solid = SOLID_TRIGGER;
	ent->moveType = MoveType::Toss;
	ent->touch = Touch_Item;
	ent->s.frame = 173;

	const Vector3 dest = ent->s.origin + Vector3{ 0, 0, -128 };
	const trace_t tr = gi.trace(ent->s.origin, ent->mins, ent->maxs, dest, ent, MASK_SOLID);
	if (tr.startSolid) {
		gi.Com_PrintFmt("{}: {} startSolid\n", __FUNCTION__, *ent);
		FreeEntity(ent);
		return;
	}

	ent->s.origin = tr.endPos;
	gi.linkEntity(ent);

	ent->nextThink = level.time + 10_hz;
	ent->think = CTF_FlagThink;
}

/*
=============
CTF_ClientEffects

Applies client-side flag visuals and effects.
=============
*/
void CTF_ClientEffects(gentity_t* player) {
	if (!SupportsCTF() || !player || !player->client) {
		return;
	}

	player->s.effects &= ~(EF_FLAG_RED | EF_FLAG_BLUE);
	if (player->health > 0) {
		if (player->client->pers.inventory[IT_FLAG_NEUTRAL]) {
			switch (player->client->sess.team) {
			case Team::Red:
				player->s.effects |= EF_FLAG_RED;
				break;
			case Team::Blue:
				player->s.effects |= EF_FLAG_BLUE;
				break;
			default:
				player->s.effects |= EF_FLAG_RED | EF_FLAG_BLUE;
				break;
			}
		}
		else {
			if (player->client->pers.inventory[IT_FLAG_RED]) {
				player->s.effects |= EF_FLAG_RED;
			}
			if (player->client->pers.inventory[IT_FLAG_BLUE]) {
				player->s.effects |= EF_FLAG_BLUE;
			}
		}
	}

	if (player->client->pers.inventory[IT_FLAG_NEUTRAL]) {
		switch (player->client->sess.team) {
		case Team::Red:
			player->s.modelIndex3 = mi_ctf_red_flag;
			break;
		case Team::Blue:
			player->s.modelIndex3 = mi_ctf_blue_flag;
			break;
		default:
			player->s.modelIndex3 = 0;
			break;
		}
	}
	else if (player->client->pers.inventory[IT_FLAG_RED]) {
		player->s.modelIndex3 = mi_ctf_red_flag;
	}
	else if (player->client->pers.inventory[IT_FLAG_BLUE]) {
		player->s.modelIndex3 = mi_ctf_blue_flag;
	}
	else {
		player->s.modelIndex3 = 0;
	}
}

