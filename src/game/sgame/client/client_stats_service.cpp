/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

client_stats_service.cpp implementation.*/

#include "client_stats_service.hpp"

#include "../g_local.hpp"
#include "../gameplay/client_config.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

namespace worr::server::client {
namespace {

constexpr float SKILL_K = 32.0f;

class DefaultClientStatsService : public ClientStatsService {
public:
	void PersistMatchResults(const MatchStatsContext& context) override;
	void SaveStatsForDisconnect(const MatchStatsContext& context, gentity_t* ent) override;
};

} // namespace

/*
=============
EloExpected

Calculates the expected score for a duel between two Elo ratings.
=============
*/
static float EloExpected(float ra, float rb) {
	return 1.0f / (1.0f + std::pow(10.0f, (rb - ra) / 400.0f));
}

/*
=============
DefaultClientStatsService::PersistMatchResults

Executes the Elo adjustments for all game modes and persists the resulting
stats via the client config store.
=============
*/
void DefaultClientStatsService::PersistMatchResults(const MatchStatsContext& context) {
	std::optional<int> defaultSkillRating;
	auto defaultSkillRatingForClient = [&]() {
		if (!defaultSkillRating.has_value())
			defaultSkillRating = GetClientConfigStore().DefaultSkillRating();

		return *defaultSkillRating;
	};

	for (auto* ent : context.participants) {
		if (!ent || !ent->client)
			continue;

		if (std::isnan(ent->client->sess.skillRating))
			ent->client->sess.skillRating = static_cast<float>(defaultSkillRatingForClient());
	}

	if (!context.allowSkillAdjustments) {
		if (g_verbose->integer) {
			gi.Com_Print("AdjustSkillRatings: Not all players are human, skipping skill rating adjustment.\n");
		}

		for (auto* ent : context.participants) {
			if (!ent || !ent->client)
				continue;

			GetClientConfigStore().SaveStats(ent->client, false);
		}
		return;
	}

	if (context.mode == GameType::Duel && context.participants.size() == 2) {
		auto* a = context.participants[0];
		auto* b = context.participants[1];
		float Ra = a->client->sess.skillRating;
		float Rb = b->client->sess.skillRating;
		bool aWon = a->client->resp.score > b->client->resp.score;
		bool draw = a->client->resp.score == b->client->resp.score;
		float Sa = draw ? 0.5f : (aWon ? 1.0f : 0.0f);
		float Sb = draw ? 0.5f : (aWon ? 0.0f : 1.0f);
		float Ea = EloExpected(Ra, Rb);
		float Eb = 1.0f - Ea;

		float dA = SKILL_K * (Sa - Ea);
		float dB = SKILL_K * (Sb - Eb);

		a->client->sess.skillRating += dA;
		b->client->sess.skillRating += dB;

		a->client->sess.skillRatingChange = static_cast<int>(dA);
		b->client->sess.skillRatingChange = static_cast<int>(dB);

		GetClientConfigStore().SaveStats(a->client, aWon && !draw);
		GetClientConfigStore().SaveStats(b->client, (!aWon && !draw));

		for (auto* ghost : context.ghosts) {
			if (!ghost || !*ghost->socialID)
				continue;

			if (Q_strcasecmp(ghost->socialID, a->client->sess.socialID) == 0) {
				ghost->skillRating += dA;
				ghost->skillRatingChange = static_cast<int>(dA);
				GetClientConfigStore().SaveStatsForGhost(*ghost, aWon && !draw);
			}
			else if (Q_strcasecmp(ghost->socialID, b->client->sess.socialID) == 0) {
				ghost->skillRating += dB;
				ghost->skillRatingChange = static_cast<int>(dB);
				GetClientConfigStore().SaveStatsForGhost(*ghost, (!aWon && !draw));
			}
		}
		return;
	}
	if (context.isTeamMode && context.participants.size() >= 2) {
		std::vector<gentity_t*> red;
		std::vector<gentity_t*> blue;

		for (auto* ent : context.participants) {
			if (!ent || !ent->client)
				continue;

			if (ent->client->sess.team == Team::Red)
				red.push_back(ent);
			else if (ent->client->sess.team == Team::Blue)
				blue.push_back(ent);
		}

		if (red.empty() || blue.empty())
			return;

		auto avg = [](const std::vector<gentity_t*>& v) {
			float sum = 0.0f;
			for (auto* e : v)
				sum += e->client->sess.skillRating;
			return sum / static_cast<float>(v.size());
		};

		float Rr = avg(red);
		float Rb = avg(blue);
		float Er = EloExpected(Rr, Rb);
		float Eb = 1.0f - Er;
		bool redWin = context.redScore > context.blueScore;
		bool draw = context.redScore == context.blueScore;

		for (auto* e : red) {
			float S = draw ? 0.5f : (redWin ? 1.0f : 0.0f);
			float d = SKILL_K * (S - Er);
			e->client->sess.skillRating += d;
			e->client->sess.skillRatingChange = static_cast<int>(d);
			GetClientConfigStore().SaveStats(e->client, redWin && !draw);
		}

		for (auto* e : blue) {
			float S = draw ? 0.5f : (redWin ? 0.0f : 1.0f);
			float d = SKILL_K * (S - Eb);
			e->client->sess.skillRating += d;
			e->client->sess.skillRatingChange = static_cast<int>(d);
			GetClientConfigStore().SaveStats(e->client, (!redWin && !draw));
		}

		for (auto* ghost : context.ghosts) {
			if (!ghost || !*ghost->socialID)
				continue;

			float S = (ghost->team == Team::Red)
					? (draw ? 0.5f : (redWin ? 1.0f : 0.0f))
					: (ghost->team == Team::Blue ? (draw ? 0.5f : (redWin ? 0.0f : 1.0f)) : 0.5f);
			float E = (ghost->team == Team::Red)
					? Er
					: (ghost->team == Team::Blue ? Eb : 0.5f);
			float d = SKILL_K * (S - E);

			ghost->skillRating += d;
			ghost->skillRatingChange = static_cast<int>(d);
			GetClientConfigStore().SaveStatsForGhost(*ghost,
					(draw ? false : ((ghost->team == Team::Red) ? redWin : (ghost->team == Team::Blue ? !redWin : false))));
		}
		return;
	}
	std::vector<gentity_t*> players = context.participants;
	if (players.size() == 1) {
		if (players[0] && players[0]->client)
			GetClientConfigStore().SaveStats(players[0]->client, true);
	}

	if (players.size() > 1) {
		std::sort(players.begin(), players.end(), [](gentity_t* a, gentity_t* b) {
			return a->client->resp.score > b->client->resp.score;
		});

		const int n = static_cast<int>(players.size());
		std::vector<float> R(n);
		std::vector<float> S(n);
		std::vector<float> E(n, 0.0f);

		for (int i = 0; i < n; ++i) {
			R[i] = players[i]->client->sess.skillRating;
			S[i] = 1.0f - static_cast<float>(i) / static_cast<float>(n - 1);
		}

		for (int i = 0; i < n; ++i) {
			for (int j = 0; j < n; ++j) {
				if (i == j)
					continue;
				E[i] += EloExpected(R[i], R[j]);
			}
			E[i] /= static_cast<float>(n - 1);
		}

		for (int i = 0; i < n; ++i) {
			float delta = SKILL_K * (S[i] - E[i]);
			gclient_t* cl = players[i]->client;
			cl->sess.skillRating += delta;
			cl->sess.skillRatingChange = static_cast<int>(delta);
			GetClientConfigStore().SaveStats(cl, i == 0);
		}
	}

	std::vector<Ghosts*> sortedGhosts = context.ghosts;
	if (sortedGhosts.empty())
		return;

	if (sortedGhosts.size() == 1) {
		GetClientConfigStore().SaveStatsForGhost(*sortedGhosts[0], true);
		return;
	}

	std::sort(sortedGhosts.begin(), sortedGhosts.end(), [](Ghosts* a, Ghosts* b) {
		return a->score > b->score;
	});

	const int gn = static_cast<int>(sortedGhosts.size());

	for (int i = 0; i < gn; ++i) {
		float Ri = sortedGhosts[i]->skillRating;
		float Si = 1.0f - static_cast<float>(i) / static_cast<float>(gn - 1);
		float Ei = 0.0f;

		for (int j = 0; j < gn; ++j) {
			if (i == j)
				continue;
			Ei += EloExpected(Ri, sortedGhosts[j]->skillRating);
		}
		Ei /= static_cast<float>(gn - 1);

		float delta = SKILL_K * (Si - Ei);
		sortedGhosts[i]->skillRating += delta;
		sortedGhosts[i]->skillRatingChange = static_cast<int>(delta);
		GetClientConfigStore().SaveStatsForGhost(*sortedGhosts[i], i == 0);
	}
}
/*
=============
DefaultClientStatsService::SaveStatsForDisconnect

Persists the player's current match stats when they disconnect.
=============
*/
void DefaultClientStatsService::SaveStatsForDisconnect(const MatchStatsContext& context, gentity_t* ent) {
	if (!ent || !ent->client)
		return;

	std::optional<int> defaultSkillRating;
	auto defaultSkillRatingForClient = [&]() {
		if (!defaultSkillRating.has_value())
			defaultSkillRating = GetClientConfigStore().DefaultSkillRating();

		return *defaultSkillRating;
	};

	auto ensureSkillRating = [&](gentity_t* player) {
		if (!player || !player->client)
			return;

		if (std::isnan(player->client->sess.skillRating))
			player->client->sess.skillRating = static_cast<float>(defaultSkillRatingForClient());
	};

	for (auto* participant : context.participants)
		ensureSkillRating(participant);

	if (std::isnan(ent->client->sess.skillRating))
		ent->client->sess.skillRating = static_cast<float>(defaultSkillRatingForClient());

	if (!context.allowSkillAdjustments) {
		GetClientConfigStore().SaveStats(ent->client, false);
		return;
	}

	if (context.mode == GameType::Duel && context.participants.size() == 2) {
		gentity_t* opponent = nullptr;

		if (context.participants[0] == ent)
			opponent = context.participants[1];
		else if (context.participants[1] == ent)
			opponent = context.participants[0];

		if (opponent && opponent->client) {
			float quitterRating = ent->client->sess.skillRating;
			float opponentRating = opponent->client->sess.skillRating;
			float expectedQuitter = EloExpected(quitterRating, opponentRating);
			float expectedOpponent = 1.0f - expectedQuitter;
			float quitterDelta = SKILL_K * (0.0f - expectedQuitter);
			float opponentDelta = SKILL_K * (1.0f - expectedOpponent);

			ent->client->sess.skillRating += quitterDelta;
			opponent->client->sess.skillRating += opponentDelta;
			ent->client->sess.skillRatingChange = static_cast<int>(quitterDelta);
			opponent->client->sess.skillRatingChange = static_cast<int>(opponentDelta);

			GetClientConfigStore().SaveStats(ent->client, false);
			GetClientConfigStore().SaveStats(opponent->client, true);

			for (auto* ghost : context.ghosts) {
				if (!ghost || !*ghost->socialID)
					continue;

				if (Q_strcasecmp(ghost->socialID, ent->client->sess.socialID) == 0) {
					ghost->skillRating += quitterDelta;
					ghost->skillRatingChange = static_cast<int>(quitterDelta);
					GetClientConfigStore().SaveStatsForGhost(*ghost, false);
				}
				else if (Q_strcasecmp(ghost->socialID, opponent->client->sess.socialID) == 0) {
					ghost->skillRating += opponentDelta;
					ghost->skillRatingChange = static_cast<int>(opponentDelta);
					GetClientConfigStore().SaveStatsForGhost(*ghost, true);
				}
			}
		}

		return;
	}

	if (context.isTeamMode && context.participants.size() >= 2) {
		std::vector<gentity_t*> red;
		std::vector<gentity_t*> blue;

		for (auto* participant : context.participants) {
			if (!participant || !participant->client)
				continue;

			if (participant->client->sess.team == Team::Red)
				red.push_back(participant);
			else if (participant->client->sess.team == Team::Blue)
				blue.push_back(participant);
		}

		if (red.empty() || blue.empty()) {
			GetClientConfigStore().SaveStats(ent->client, false);
			return;
		}

		const auto avg = [](const std::vector<gentity_t*>& players) {
			float sum = 0.0f;
			for (auto* player : players)
				sum += player->client->sess.skillRating;
			return sum / static_cast<float>(players.size());
		};

		float redAverage = avg(red);
		float blueAverage = avg(blue);
		float expectedRed = EloExpected(redAverage, blueAverage);
		float expectedBlue = 1.0f - expectedRed;
		bool redIsAhead = context.redScore > context.blueScore;
		bool isDraw = context.redScore == context.blueScore;
		Team quitterTeam = ent->client->sess.team;
		float score = isDraw ? 0.5f : ((quitterTeam == Team::Red) ? (redIsAhead ? 1.0f : 0.0f) : (redIsAhead ? 0.0f : 1.0f));
		float expected = (quitterTeam == Team::Red) ? expectedRed : expectedBlue;
		float quitterDelta = SKILL_K * (score - expected);

		ent->client->sess.skillRating += quitterDelta;
		ent->client->sess.skillRatingChange = static_cast<int>(quitterDelta);

		GetClientConfigStore().SaveStats(ent->client, score > 0.5f);
		return;
	}

	std::vector<gentity_t*> players = context.participants;

	if (players.size() <= 1) {
		GetClientConfigStore().SaveStats(ent->client, true);
		return;
	}

	std::sort(players.begin(), players.end(), [](gentity_t* a, gentity_t* b) {
		return a->client->resp.score > b->client->resp.score;
	});

	int quitterIndex = -1;
	for (int i = 0; i < static_cast<int>(players.size()); ++i) {
		if (players[i] == ent) {
			quitterIndex = i;
			break;
		}
	}

	if (quitterIndex == -1) {
		GetClientConfigStore().SaveStats(ent->client, false);
		return;
	}

	const int n = static_cast<int>(players.size());
	std::vector<float> R(n);
	std::vector<float> S(n);
	std::vector<float> E(n, 0.0f);

	for (int i = 0; i < n; ++i) {
		R[i] = players[i]->client->sess.skillRating;
		S[i] = 1.0f - static_cast<float>(i) / static_cast<float>(n - 1);
	}

	for (int i = 0; i < n; ++i) {
		for (int j = 0; j < n; ++j) {
			if (i == j)
				continue;
			E[i] += EloExpected(R[i], R[j]);
		}
		E[i] /= static_cast<float>(n - 1);
	}

	float quitterDelta = SKILL_K * (S[quitterIndex] - E[quitterIndex]);

	ent->client->sess.skillRating += quitterDelta;
	ent->client->sess.skillRatingChange = static_cast<int>(quitterDelta);

	GetClientConfigStore().SaveStats(ent->client, quitterIndex == 0);
}

/*
=============
BuildMatchStatsContext

Collects the current match state into a structure that the client stats service
can consume.
=============
*/
MatchStatsContext BuildMatchStatsContext(LevelLocals& level) {
	MatchStatsContext context{};
	context.mode = Game::GetCurrentType();
	context.isTeamMode = Teams() && Game::IsNot(GameType::RedRover);
	context.allowSkillAdjustments = (level.pop.num_playing_clients == level.pop.num_playing_human_clients);
	context.redScore = level.teamScores[static_cast<int>(Team::Red)];
	context.blueScore = level.teamScores[static_cast<int>(Team::Blue)];

	for (auto* ent : active_clients()) {
		if (!ent || !ent->client)
			continue;

		if (!ClientIsPlaying(ent->client))
			continue;

		context.participants.push_back(ent);
	}

	for (auto& ghost : level.ghosts) {
		if (!*ghost.socialID)
			continue;

		context.ghosts.push_back(&ghost);
	}

	return context;
}


/*
=============
GetClientStatsService

Provides access to the shared stats service implementation.
=============
*/
ClientStatsService& GetClientStatsService() {
	static DefaultClientStatsService service;
	return service;
}

} // namespace worr::server::client
