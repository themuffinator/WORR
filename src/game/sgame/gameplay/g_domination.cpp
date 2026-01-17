/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

g_domination.cpp implementation.*/

#include "../g_local.hpp"

#include <algorithm>
#include <cmath>
#include <string>

extern const spawn_temp_t& ED_GetSpawnTemp();

namespace {
	constexpr GameTime kDominationMinScoreInterval = 100_ms;
	constexpr GameTime kDominationMinCaptureTime = 250_ms;
	constexpr float kDominationDefaultTickIntervalSeconds = 1.0f;
	constexpr float kDominationMaxTickIntervalSeconds = 10.0f;
	constexpr int32_t kDominationDefaultPointsPerTick = 1;
	constexpr float kDominationDefaultCaptureSeconds = 3.0f;
	constexpr float kDominationDefaultNeutralizeSeconds = 2.0f;
	constexpr int32_t kDominationDefaultCaptureBonus = 5;
	constexpr int32_t kDominationOccupantGraceMinMs = 50;
	constexpr int32_t kDominationOccupantGraceMaxMs = 250;

	/*
	=============
	DominationTickInterval

	Returns the amount of time between passive domination score ticks.
	=============
	*/
	GameTime DominationTickInterval() {
		float seconds = kDominationDefaultTickIntervalSeconds;
		bool clamped = false;

		if (g_domination_tick_interval) {
			const float configured = g_domination_tick_interval->value;
			if (std::isfinite(configured))
				seconds = configured;
			else
				clamped = true;
		}

		GameTime interval = GameTime::from_sec(seconds);
		if (!(seconds > 0.0f)) {
			seconds = kDominationMinScoreInterval.seconds<float>();
			interval = kDominationMinScoreInterval;
			clamped = true;
		}

		const GameTime maxInterval = GameTime::from_sec(kDominationMaxTickIntervalSeconds);

		if (!interval || interval < kDominationMinScoreInterval) {
			interval = kDominationMinScoreInterval;
			seconds = std::max(seconds, kDominationMinScoreInterval.seconds<float>());
			clamped = true;
		} else if (maxInterval && interval > maxInterval) {
			interval = maxInterval;
			seconds = std::min(seconds, kDominationMaxTickIntervalSeconds);
			clamped = true;
		}

		if (clamped)
			gi.Com_PrintFmt("Domination: clamping g_domination_tick_interval to {:.2f} seconds\n", seconds);

		return interval;
	}

	/*
	=============
	DominationPointsPerTick

	Returns the number of score points earned each domination tick.
	=============
	*/
	int32_t DominationPointsPerTick() {
		int32_t points = kDominationDefaultPointsPerTick;

		if (g_domination_points_per_tick) {
			const int32_t configured = g_domination_points_per_tick->integer;
			if (configured > 0)
				points = configured;
		}

		return points;
	}

	/*
	=============
	DominationCaptureBonus

	Returns the team score bonus awarded for capturing a point.
	=============
	*/
	int32_t DominationCaptureBonus() {
		if (!g_domination_capture_bonus)
			return kDominationDefaultCaptureBonus;

		return std::max<int32_t>(0, g_domination_capture_bonus->integer);
	}

	/*
	=============
	DominationCaptureTime

	Returns how long a team must hold a point to capture it.
	=============
	*/
	GameTime DominationCaptureTime() {
		float seconds = kDominationDefaultCaptureSeconds;
		bool clamped = false;

		if (g_domination_capture_time) {
			const float configured = g_domination_capture_time->value;
			if (std::isfinite(configured))
				seconds = configured;
			else
				clamped = true;
		}

		if (!(seconds > 0.0f)) {
			seconds = kDominationMinCaptureTime.seconds<float>();
			clamped = true;
		}

		GameTime captureTime = GameTime::from_sec(seconds);
		if (!captureTime || captureTime < kDominationMinCaptureTime) {
			seconds = std::max(seconds, kDominationMinCaptureTime.seconds<float>());
			captureTime = kDominationMinCaptureTime;
			clamped = true;
		}

		if (clamped)
			gi.Com_PrintFmt("Domination: clamping g_domination_capture_time to {:.2f} seconds\n", seconds);

		return captureTime;
	}

	/*
	=============
	DominationNeutralizeTime

	Returns how long a team must hold a point to neutralize it.
	=============
	*/
	GameTime DominationNeutralizeTime() {
		float seconds = kDominationDefaultNeutralizeSeconds;
		bool clamped = false;

		if (g_domination_neutralize_time) {
			const float configured = g_domination_neutralize_time->value;
			if (std::isfinite(configured))
				seconds = configured;
			else
				clamped = true;
		}

		if (!(seconds > 0.0f)) {
			seconds = kDominationMinCaptureTime.seconds<float>();
			clamped = true;
		}

		GameTime neutralizeTime = GameTime::from_sec(seconds);
		if (!neutralizeTime || neutralizeTime < kDominationMinCaptureTime) {
			seconds = std::max(seconds, kDominationMinCaptureTime.seconds<float>());
			neutralizeTime = kDominationMinCaptureTime;
			clamped = true;
		}

		if (clamped)
			gi.Com_PrintFmt("Domination: clamping g_domination_neutralize_time to {:.2f} seconds\n", seconds);

		return neutralizeTime;
	}
	/*
	=============
	DominationOccupantGrace

	Returns the grace period a player remains registered inside a point volume between touch events.
	=============
	*/
	GameTime DominationOccupantGrace() {
		const uint32_t frameMs = gi.frameTimeMs ? gi.frameTimeMs : 16;
		const uint32_t graceMs = std::clamp<uint32_t>(frameMs * 2, kDominationOccupantGraceMinMs, kDominationOccupantGraceMaxMs);
		return GameTime::from_ms(static_cast<int64_t>(graceMs));
	}

	constexpr float kDominationBeamTraceDistance = 8192.0f;

	constexpr int32_t PackColor(const rgba_t& color) {
		return static_cast<int32_t>(color.a)
			| (static_cast<int32_t>(color.b) << 8)
			| (static_cast<int32_t>(color.g) << 16)
			| (static_cast<int32_t>(color.r) << 24);
	}

	int32_t BeamColorForTeam(Team team) {
		switch (team) {
		case Team::Red:
			return PackColor(rgba_red);
		case Team::Blue:
			return PackColor(rgba_blue);
		default:
			return PackColor(rgba_white);
		}
	}

	/*
	=============
	FreePointBeam

	Releases the beam entity that visually marks a domination point.
	=============
	*/
	void FreePointBeam(LevelLocals::DominationState::Point& point) {
		if (point.beam) {
			FreeEntity(point.beam);
			point.beam = nullptr;
		}
	}

	/*
	=============
	EnsurePointBeam

	Creates or updates the beam entity for a domination point.
	=============
	*/
	void EnsurePointBeam(LevelLocals::DominationState::Point& point) {
		if (!point.ent || !point.ent->inUse || point.ent->spawn_count != point.spawnCount) {
			FreePointBeam(point);
			if (!point.ent || !point.ent->inUse || point.ent->spawn_count != point.spawnCount) {
				point.ent = nullptr;
				point.owner = Team::None;
				point.spawnCount = 0;
			}
			return;
		}

		if (point.owner == Team::None) {
			FreePointBeam(point);
			return;
		}

		gentity_t*& beam = point.beam;

		if (!beam) {
			beam = Spawn();
			if (!beam) {
				gi.Com_PrintFmt("WARNING: {} failed to spawn domination point beam for point {}\n", __FUNCTION__, point.index);
				return;
			}
			beam->className = "domination_point_beam";
			beam->moveType = MoveType::None;
			beam->solid = SOLID_NOT;
			beam->s.renderFX = RF_BEAM;
			beam->s.modelIndex = MODELINDEX_WORLD;
			beam->s.frame = 4;
		}


		beam->owner = point.ent;
		beam->count = static_cast<int32_t>(point.index);
		beam->moveType = MoveType::None;
		beam->solid = SOLID_NOT;
		beam->s.renderFX |= RF_BEAM;
		beam->s.modelIndex = MODELINDEX_WORLD;
		beam->s.frame = 4;
		beam->svFlags &= ~SVF_NOCLIENT;

		const Vector3 start = point.ent->s.origin;
		const Vector3 end = start + Vector3{ 0.0f, 0.0f, kDominationBeamTraceDistance };

		const trace_t tr = gi.trace(start, vec3_origin, vec3_origin, end, point.ent, MASK_SOLID);

		beam->s.origin = start;
		beam->s.oldOrigin = tr.endPos;
		beam->s.skinNum = BeamColorForTeam(point.owner);

		gi.linkEntity(beam);
	}


	/*
	=============
	FindPointForEntity

	Finds the domination point that owns the provided entity.
	=============
	*/
	LevelLocals::DominationState::Point* FindPointForEntity(gentity_t* ent) {
		auto& dom = level.domination;
		for (size_t i = 0; i < dom.count; ++i) {
			if (dom.points[i].ent == ent && dom.points[i].spawnCount == ent->spawn_count)
				return &dom.points[i];
		}
		return nullptr;
	}

	/*
	=============
	ApplyPointOwnerVisual

	Updates skin and beam colors to reflect the owning team.
	=============
	*/
	void ApplyPointOwnerVisual(LevelLocals::DominationState::Point& point) {
		if (!point.ent)
			return;

		switch (point.owner) {
		case Team::Red:
			point.ent->s.skinNum = 1;
			break;
		case Team::Blue:
			point.ent->s.skinNum = 2;
			break;
		default:
			point.ent->s.skinNum = 0;
			break;
		}

		EnsurePointBeam(point);
	}

	/*
	=============
	SpawnFlagOwner

	Determines which team initially owns the point based on spawn flags.
	=============
	*/
	Team SpawnFlagOwner(const gentity_t* ent) {
		const bool red = ent->spawnFlags.has(SPAWNFLAG_DOMINATION_START_RED);
		const bool blue = ent->spawnFlags.has(SPAWNFLAG_DOMINATION_START_BLUE);

		if (red == blue)
			return Team::None;

		return red ? Team::Red : Team::Blue;
	}

	/*
	=============
	RegisterPoint

	Registers a domination point entity with the level state.
	=============
	*/
	LevelLocals::DominationState::Point* RegisterPoint(gentity_t* ent) {
		auto& dom = level.domination;

		if (dom.count >= LevelLocals::DominationState::MAX_POINTS) {
			gi.Com_PrintFmt("Domination: ignoring {} because the maximum number of points ({}) has been reached.\n", *ent,
				LevelLocals::DominationState::MAX_POINTS);
			return nullptr;
		}

		auto& point = dom.points[dom.count];
		FreePointBeam(point);
		point = {};
		point.ent = ent;
		point.index = dom.count;
		point.owner = SpawnFlagOwner(ent);
		point.spawnCount = ent->spawn_count;
		++dom.count;

		return &point;
	}

	/*
	=============
	PointLabel

	Returns a friendly label for a domination point used in announcements.
	=============
	*/
	std::string PointLabel(const gentity_t* ent, size_t index) {
		if (ent->message && ent->message[0])
			return ent->message;
		if (ent->targetName && ent->targetName[0])
			return ent->targetName;
		return G_Fmt("Point {}", index + 1).data();
	}

	/*
	=============
	AnnounceCapture

	Broadcasts that a team has captured the specified point.
	=============
	*/
	void AnnounceCapture(gentity_t* ent, Team team, size_t index) {
		const std::string label = PointLabel(ent, index);
		gi.LocBroadcast_Print(PRINT_HIGH, "{} captured {}.\n", Teams_TeamName(team), label.c_str());
	}

	/*
	=============
	FinalizeCapture

	Applies the ownership change for a point capture and triggers visuals/announcements.
	=============
	*/
	void FinalizeCapture(LevelLocals::DominationState::Point& point, Team newOwner) {
		point.owner = newOwner;
		point.capturingTeam = Team::None;
		point.captureProgress = 0.0f;
		point.lastProgressTime = level.time;
		ApplyPointOwnerVisual(point);
		const int32_t bonus = DominationCaptureBonus();
		if (bonus > 0)
			G_AdjustTeamScore(newOwner, bonus);
		AnnounceCapture(point.ent, newOwner, point.index);
	}

	/*
	=============
	UpdatePointOccupants

	Refreshes the tracked player counts occupying a domination point.
	=============
	*/
	void UpdatePointOccupants(LevelLocals::DominationState::Point& point) {
		point.occupantCounts.fill(0);

		const bool hasClients = game.clients && g_entities && game.maxClients > 0;
		const GameTime now = level.time;

		for (size_t i = 0; i < point.occupantExpiry.size(); ++i) {
			GameTime& expiry = point.occupantExpiry[i];
			if (!expiry)
				continue;

			if (expiry <= now) {
				expiry = 0_ms;
				continue;
			}

			if (!hasClients || i >= static_cast<size_t>(game.maxClients)) {
				expiry = 0_ms;
				continue;
			}

			gclient_t* cl = &game.clients[i];
			gentity_t* ent = &g_entities[i + 1];
			if (!ent || !ent->inUse || ent->client != cl) {
				expiry = 0_ms;
				continue;
			}

			if (!ClientIsPlaying(cl) || cl->eliminated) {
				expiry = 0_ms;
				continue;
			}

			const Team team = cl->sess.team;
			if (team != Team::Red && team != Team::Blue) {
				expiry = 0_ms;
				continue;
			}

			++point.occupantCounts[static_cast<size_t>(team)];
		}
	}

	/*
	=============
	AdvanceCaptureProgress

	Advances or decays capture progress depending on the players present.
	=============
	*/
	void AdvanceCaptureProgress(LevelLocals::DominationState::Point& point) {
		const int redCount = point.occupantCounts[static_cast<size_t>(Team::Red)];
		const int blueCount = point.occupantCounts[static_cast<size_t>(Team::Blue)];
		const bool contested = redCount > 0 && blueCount > 0;
		Team activeTeam = Team::None;

		if (!contested) {
			if (redCount > 0 && blueCount == 0)
				activeTeam = Team::Red;
			else if (blueCount > 0 && redCount == 0)
				activeTeam = Team::Blue;
		}

		const GameTime now = level.time;
		GameTime delta = point.lastProgressTime ? (now - point.lastProgressTime) : 0_ms;
		if (delta.milliseconds() < 0)
			delta = 0_ms;
		point.lastProgressTime = now;

		const bool neutral = point.owner == Team::None;
		const GameTime phaseTime = neutral ? DominationCaptureTime() : DominationNeutralizeTime();
		const int64_t phaseMs = phaseTime.milliseconds();

		const auto decayProgress = [&](float amount) {
			point.captureProgress = std::max(0.0f, point.captureProgress - amount);
			if (point.captureProgress == 0.0f)
				point.capturingTeam = Team::None;
			};

		if (phaseMs <= 0) {
			if (activeTeam != Team::None && activeTeam != point.owner) {
				if (neutral) {
					FinalizeCapture(point, activeTeam);
				} else {
					point.owner = Team::None;
					point.captureProgress = 0.0f;
					ApplyPointOwnerVisual(point);
				}
			} else if (contested || activeTeam == Team::None) {
				point.capturingTeam = Team::None;
			}
			return;
		}

		const float deltaProgress = static_cast<float>(delta.milliseconds()) / static_cast<float>(phaseMs);

		if (contested) {
			if (point.capturingTeam != Team::None && deltaProgress > 0.0f)
				decayProgress(deltaProgress);
			return;
		}

		if (activeTeam == Team::None) {
			if (point.capturingTeam != Team::None && deltaProgress > 0.0f)
				decayProgress(deltaProgress);
			return;
		}

		if (point.owner == activeTeam) {
			point.capturingTeam = Team::None;
			point.captureProgress = 0.0f;
			return;
		}

		if (point.capturingTeam != activeTeam) {
			point.capturingTeam = activeTeam;
			point.captureProgress = 0.0f;
		}

		if (deltaProgress > 0.0f)
			point.captureProgress = std::min(1.0f, point.captureProgress + deltaProgress);

		if (point.captureProgress >= 1.0f) {
			if (!neutral) {
				point.owner = Team::None;
				point.captureProgress = 0.0f;
				ApplyPointOwnerVisual(point);
				return;
			}

			FinalizeCapture(point, activeTeam);
		}
	}

	/*
	=============
	Domination_PointTouch

	Registers a player touching a domination point so capture logic can track occupancy.
	=============
	*/
	TOUCH(Domination_PointTouch)(gentity_t* self, gentity_t* other, const trace_t&, bool) -> void {
		if (!other->client)
			return;
		if (!ClientIsPlaying(other->client) || other->client->eliminated)
			return;
		if (Game::IsNot(GameType::Domination))
			return;

		const Team team = other->client->sess.team;
		if (team != Team::Red && team != Team::Blue)
			return;

		auto* point = FindPointForEntity(self);
		if (!point)
			return;

		if (!game.clients)
			return;

		const ptrdiff_t clientIndex = other->client - game.clients;
		if (clientIndex < 0 || clientIndex >= game.maxClients)
			return;

		point->occupantExpiry[static_cast<size_t>(clientIndex)] = level.time + DominationOccupantGrace();
	}

	void EnsureBounds(gentity_t* ent, const spawn_temp_t& st) {
		if (ent->model && ent->model[0]) {
			gi.setModel(ent, ent->model);
			return;
		}

		if (ent->mins || ent->maxs)
			return;

		const float radius = (st.radius > 0.0f) ? st.radius : 64.0f;
		const float height = (st.height > 0) ? static_cast<float>(st.height) : 72.0f;

		ent->mins = { -radius, -radius, 0.0f };
		ent->maxs = { radius, radius, height };
	}

} // namespace

/*
=============
Domination_ClearState

Resets domination state and frees transient entities.
=============
*/
void Domination_ClearState() {
	for (auto& point : level.domination.points) {
		FreePointBeam(point);
		point.ent = nullptr;
		point.owner = Team::None;
		point.spawnCount = 0;
	}

	level.domination = {};
}

/*
=============
Domination_InitLevel

Initializes domination state when a level loads.
=============
*/
void Domination_InitLevel() {
	if (Game::IsNot(GameType::Domination)) {
		Domination_ClearState();
		return;
	}

	auto& dom = level.domination;
	if (dom.count > LevelLocals::DominationState::MAX_POINTS)
		dom.count = LevelLocals::DominationState::MAX_POINTS;

	dom.nextScoreTime = level.time + DominationTickInterval();

	for (size_t i = 0; i < dom.count; ++i) {
		auto& point = dom.points[i];
		point.index = i;
		point.capturingTeam = Team::None;
		point.captureProgress = 0.0f;
		point.lastProgressTime = level.time;
		point.occupantCounts.fill(0);
		point.occupantExpiry.fill(0_ms);
		ApplyPointOwnerVisual(point);
	}
}

/*
=============
Domination_RunFrame

Advances domination capture logic each frame and awards periodic scoring.
=============
*/
void Domination_RunFrame() {
	if (Game::IsNot(GameType::Domination))
		return;
	if (level.matchState != MatchState::In_Progress)
		return;
	if (ScoringIsDisabled())
		return;

	auto& dom = level.domination;
	if (!dom.count)
		return;

	const GameTime interval = DominationTickInterval();
	if (!dom.nextScoreTime)
		dom.nextScoreTime = level.time + interval;

	const bool readyToScore = level.time >= dom.nextScoreTime;
	if (readyToScore)
		dom.nextScoreTime = level.time + interval;

	int redOwned = 0;
	int blueOwned = 0;

	for (size_t i = 0; i < dom.count; ++i) {
		auto& point = dom.points[i];
		if (!point.ent || !point.ent->inUse || point.ent->spawn_count != point.spawnCount) {
			FreePointBeam(point);
			if (!point.ent || !point.ent->inUse || point.ent->spawn_count != point.spawnCount) {
				point.ent = nullptr;
				point.owner = Team::None;
				point.spawnCount = 0;
				point.capturingTeam = Team::None;
				point.captureProgress = 0.0f;
				point.lastProgressTime = level.time;
				point.occupantCounts.fill(0);
				point.occupantExpiry.fill(0_ms);
			}
			continue;
		}

		UpdatePointOccupants(point);
		AdvanceCaptureProgress(point);

		if (point.owner == Team::Red)
			++redOwned;
		else if (point.owner == Team::Blue)
			++blueOwned;
	}

	if (!readyToScore)
		return;

	if (!redOwned && !blueOwned)
		return;

	const int32_t pointsPerTick = DominationPointsPerTick();

	if (redOwned)
		G_AdjustTeamScore(Team::Red, redOwned * pointsPerTick);
	if (blueOwned)
		G_AdjustTeamScore(Team::Blue, blueOwned * pointsPerTick);
}

void SP_domination_point(gentity_t* ent) {
	const spawn_temp_t& st = ED_GetSpawnTemp();

	ent->solid = SOLID_TRIGGER;
	ent->moveType = MoveType::None;
	ent->svFlags |= SVF_NOCLIENT;
	ent->clipMask = CONTENTS_PLAYER;
	ent->touch = Domination_PointTouch;

	EnsureBounds(ent, st);

	auto* point = RegisterPoint(ent);
	if (!point) {
		ent->touch = nullptr;
		ent->solid = SOLID_NOT;
		ent->clipMask = CONTENTS_NONE;
	}
	else {
		ent->count = static_cast<int32_t>(point->index);
	}

	gi.linkEntity(ent);

	if (point)
		ApplyPointOwnerVisual(*point);
}
