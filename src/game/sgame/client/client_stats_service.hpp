#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct gentity_t;
struct gclient_t;
struct LevelLocals;
struct Ghosts;

enum class GameType : uint8_t;

namespace worr::server::client {

struct MatchStatsContext {
GameType mode{};
	bool isTeamMode{};
	bool allowSkillAdjustments{};
	int redScore{};
	int blueScore{};
	std::vector<gentity_t*> participants;
	std::vector<Ghosts*> ghosts;
};

/*
=================
ClientStatsService

Coordinates skill rating adjustments and stat persistence for the current
match. The service consumes a snapshot of the match context so both
match-ending flows and disconnect flows share a single orchestration point.
=================
*/
class ClientStatsService {
public:
virtual ~ClientStatsService() = default;
virtual void PersistMatchResults(const MatchStatsContext& context) = 0;
virtual void SaveStatsForDisconnect(const MatchStatsContext& context, gentity_t* ent) = 0;
};

MatchStatsContext BuildMatchStatsContext(LevelLocals& level);
ClientStatsService& GetClientStatsService();

} // namespace worr::server::client
