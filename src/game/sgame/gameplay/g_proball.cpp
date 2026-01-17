/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

============= g_proball.cpp Ball physics helpers and the ProBall gametype implementation. This
file merges all of the shared ball item behavior with the gametype-specific scoring and trigger
logic so the entire mode can be reasoned about in a single location. =============*/

#include "g_proball.hpp"

#include <algorithm>

namespace ProBall {
void OnBallReset(gentity_t* ball);
}

namespace {

void Ball_Think(gentity_t* ball);
void Ball_Touch(gentity_t* ball, gentity_t* other,
		const trace_t& tr, bool otherTouchingSelf);

constexpr Vector3 BALL_MINS{-12.f, -12.f, -12.f};
constexpr Vector3 BALL_MAXS{12.f, 12.f, 12.f};
constexpr GameTime BALL_THINK_INTERVAL = 50_ms;
constexpr GameTime BALL_IDLE_RESET_TIME = 10_sec;
constexpr GameTime BALL_PASS_COOLDOWN = 600_ms;
constexpr GameTime BALL_DROP_COOLDOWN = 400_ms;
constexpr GameTime BALL_OWNER_REGRAB_DELAY = 500_ms;
constexpr float BALL_PASS_SPEED = 650.f;
constexpr float BALL_DROP_OWNER_VEL_SCALE = 0.35f;
constexpr float BALL_DROP_UPWARD_SPEED = 90.f;
constexpr float BALL_THROW_UPWARD_SPEED = 180.f;
constexpr float BALL_ATTRACT_RADIUS = 256.f;
constexpr float BALL_ATTRACT_FORCE = 140.f;
constexpr float BALL_IDLE_SPEED_THRESHOLD_SQ = 64.f;
constexpr float BALL_MAX_SPEED = 1200.f;
constexpr float BALL_SIDE_JITTER = 30.f;
constexpr float BALL_OUT_OF_WORLD_DELTA = 2048.f;

/*
=============
Ball_GametypeActive
=============
*/
[[nodiscard]] inline bool Ball_GametypeActive() {
	return Game::Is(GameType::ProBall);
}

/*
=============
Ball_Item
=============
*/
[[nodiscard]] Item* Ball_Item() { return GetItemByIndex(IT_BALL); }

/*
=============
Ball_TeamForEntity
=============
*/
[[nodiscard]] Team Ball_TeamForEntity(const gentity_t* owner) {
	if (!owner || !owner->client)
		return Team::None;
	return owner->client->sess.team;
}

/*
=============
Ball_InitEntity
=============
*/
void Ball_InitEntity(gentity_t* ball) {
	if (!ball)
		return;

	Item* item = Ball_Item();
	if (item) {
		ball->item = item;
		ball->className = item->className;
		ball->s.modelIndex = gi.modelIndex(item->worldModel);
		ball->s.effects = item->worldModelFlags;
	}

	ball->mins = BALL_MINS;
	ball->maxs = BALL_MAXS;
	ball->clipMask = MASK_SOLID;
	ball->moveType = MoveType::NewToss;
	ball->solid = SOLID_TRIGGER;
	ball->svFlags &= ~SVF_NOCLIENT;
	ball->flags &= ~FL_TEAMSLAVE;
	ball->spawnFlags |= SPAWNFLAG_ITEM_DROPPED_PLAYER;

	if (Ball_GametypeActive()) {
		ball->s.effects |= EF_COLOR_SHELL;
		ball->s.renderFX |= RF_SHELL_RED | RF_SHELL_GREEN | RF_GLOW |
				    RF_NO_LOD | RF_IR_VISIBLE;
	}
}

/*
=============
Ball_EnsureEntity
=============
*/
[[nodiscard]] gentity_t* Ball_EnsureEntity() {
	gentity_t* ball = level.ball.entity;
	if (ball && ball->inUse)
		return ball;

	ball = Spawn();
	if (!ball)
		return nullptr;

	Ball_InitEntity(ball);
	level.ball.entity = ball;
	return ball;
}

/*
=============
Ball_AdjustOrigin
=============
*/
[[nodiscard]] Vector3 Ball_AdjustOrigin(gentity_t* owner,
					const Vector3& desired) {
	gentity_t* reference = Ball_EnsureEntity();
	if (!reference)
		return desired;

	const trace_t tr =
	    gi.trace(owner ? owner->s.origin : desired, BALL_MINS, BALL_MAXS,
		     desired, owner, MASK_SOLID);
	return tr.endPos;
}

/*
=============
Ball_DetachCarrier
=============
*/
void Ball_DetachCarrier(gentity_t* owner) {
	if (!owner || !owner->client)
		return;

	owner->client->pers.inventory[IT_BALL] = 0;
	if (owner->client->pers.selectedItem == IT_BALL) {
		owner->client->pers.selectedItem = IT_NULL;
		owner->client->ps.stats[STAT_SELECTED_ITEM] = 0;
	}
	ValidateSelectedItem(owner);

	if (level.ball.carrier == owner)
		level.ball.carrier = nullptr;
}

/*
=============
Ball_ApplyVelocityClamp
=============
*/
void Ball_ApplyVelocityClamp(gentity_t* ball) {
	const float speedSq = ball->velocity.lengthSquared();
	if (speedSq > BALL_MAX_SPEED * BALL_MAX_SPEED) {
		ball->velocity = ball->velocity.normalized() * BALL_MAX_SPEED;
	}
}

/*
=============
Ball_PlaySound
=============
*/
void Ball_PlaySound(gentity_t* source, const char* path) {
	if (!path)
		return;
	const int idx = gi.soundIndex(path);
	gi.sound(source, CHAN_ITEM, idx, 1.f, ATTN_NORM, 0.f);
}

/*
=============
Ball_StartWorldTravel
=============
*/
static void Ball_StartWorldTravel(gentity_t* ball, gentity_t* owner,
			Team team) {
	ball->svFlags &= ~SVF_NOCLIENT;
	ball->solid = SOLID_TRIGGER;
	ball->moveType = MoveType::NewToss;
	ball->clipMask = MASK_SOLID;
	ball->touch = Ball_Touch;
	ball->think = Ball_Think;
	ball->nextThink = level.time + BALL_THINK_INTERVAL;
	ball->owner = owner;
	ball->touch_debounce_time = level.time + BALL_OWNER_REGRAB_DELAY;
	ball->timeStamp = level.time;
	ball->fteam = team;
	level.ball.carrier = nullptr;
	level.ball.idleBegin = 0_ms;
}

/*
=============
Ball_Think
=============
*/
static THINK(Ball_Think)(gentity_t* ball) -> void {
	if (!ball)
		return;

	if (!Ball_GametypeActive())
		return;

	if (ball->owner && ball->touch_debounce_time <= level.time)
		ball->owner = nullptr;

	contents_t contents = gi.pointContents(ball->s.origin);
	if (contents & (CONTENTS_SLIME | CONTENTS_LAVA)) {
		Ball_Reset(false);
		return;
	}

	if (level.ball.homeValid &&
	    ball->s.origin[2] <
		level.ball.homeOrigin[2] - BALL_OUT_OF_WORLD_DELTA) {
		Ball_Reset(false);
		return;
	}

	if (ball->groundEntity &&
	    ball->velocity.lengthSquared() <= BALL_IDLE_SPEED_THRESHOLD_SQ) {
		if (level.ball.idleBegin == 0_ms)
			level.ball.idleBegin = level.time;
	} else {
		level.ball.idleBegin = 0_ms;
	}

	if (level.ball.idleBegin != 0_ms &&
	    level.time - level.ball.idleBegin >= BALL_IDLE_RESET_TIME) {
		Ball_Reset(false);
		return;
	}

	if (ball->fteam != Team::None && ball->fteam != Team::Spectator &&
	    ball->fteam != Team::Free) {
		gentity_t* best = nullptr;
		float bestDistSq = BALL_ATTRACT_RADIUS * BALL_ATTRACT_RADIUS;

		for (auto player : active_clients()) {
			if (!player->client)
				continue;
			if (player->health <= 0)
				continue;
			if (player == ball->owner)
				continue;
			if (player->client->sess.team != ball->fteam)
				continue;

			const Vector3 target =
			    player->s.origin +
			    Vector3{0.f, 0.f,
				    static_cast<float>(player->viewHeight) *
					0.5f};
			const float distSq =
			    (target - ball->s.origin).lengthSquared();
			if (distSq < bestDistSq) {
				bestDistSq = distSq;
				best = player;
			}
		}

		if (best) {
			Vector3 desired =
			    best->s.origin +
			    Vector3{0.f, 0.f,
				    static_cast<float>(best->viewHeight) *
					0.5f};
			Vector3 to = desired - ball->s.origin;
			const float dist = to.length();
			if (dist > 1.f) {
				const float strength = std::clamp(
				    1.f - (dist / BALL_ATTRACT_RADIUS), 0.f,
				    1.f);
				ball->velocity +=
				    to.normalized() *
				    (BALL_ATTRACT_FORCE * strength);
				Ball_ApplyVelocityClamp(ball);
			}
		}
	}

	ball->nextThink = level.time + BALL_THINK_INTERVAL;
}

/*
=============
Ball_Touch
=============
*/
static TOUCH(Ball_Touch)(gentity_t* ball, gentity_t* other,
						 const trace_t& tr, bool otherTouchingSelf) -> void {
	if (!ball)
		return;

	if (!other)
		return;

	if (tr.surface && (tr.surface->flags & SURF_SKY)) {
		Ball_Reset(false);
		return;
	}

	if (!other->client)
		return;

	if (ball->owner && other == ball->owner &&
	    ball->touch_debounce_time > level.time)
		return;

	trace_t safeTrace = tr;
	safeTrace.contents &= ~(CONTENTS_LAVA | CONTENTS_SLIME);

	Touch_Item(ball, other, safeTrace, otherTouchingSelf);
}

} // namespace

/*
=============
Ball_RegisterSpawn
=============
*/
void Ball_RegisterSpawn(gentity_t* ent) {
	if (!ent)
		return;

	if (!Ball_GametypeActive()) {
		ent->svFlags |= SVF_NOCLIENT;
		ent->solid = SOLID_NOT;
		ent->think = nullptr;
		ent->nextThink = 0_ms;
		return;
	}

	Ball_InitEntity(ent);
	level.ball.entity = ent;
	level.ball.carrier = nullptr;
	level.ball.homeOrigin = ent->s.origin;
	level.ball.homeAngles = ent->s.angles;
	level.ball.homeValid = true;
	level.ball.idleBegin = 0_ms;

	Ball_Reset(true);
}

/*
=============
Ball_OnPickup
=============
*/
void Ball_OnPickup(gentity_t* ball, gentity_t* player) {
	if (!ball || !player || !player->client)
		return;

	Ball_InitEntity(ball);

	ball->svFlags |= SVF_NOCLIENT;
	ball->solid = SOLID_NOT;
	ball->moveType = MoveType::None;
	ball->velocity = {};
	ball->aVelocity = {};
	ball->owner = player;
	ball->think = nullptr;
	ball->nextThink = 0_ms;
	ball->touch = nullptr;
	ball->fteam = player->client->sess.team;
	level.ball.entity = ball;
	level.ball.carrier = player;
	level.ball.idleBegin = 0_ms;

	player->client->ball.nextPassTime = level.time;
	player->client->ball.nextDropTime = level.time;

	gi.linkEntity(ball);
}

/*
=============
Ball_PlayerHasBall
=============
*/
bool Ball_PlayerHasBall(const gentity_t* ent) {
	if (!ent || !ent->client)
		return false;
	return ent->client->pers.inventory[IT_BALL] > 0;
}

/*
=============
Ball_Launch
=============
*/
bool Ball_Launch(gentity_t* owner, const Vector3& start, const Vector3& dir,
		 float speed) {
	if (!Ball_GametypeActive())
		return false;

	if (!Ball_PlayerHasBall(owner))
		return false;

	gentity_t* ball = Ball_EnsureEntity();
	if (!ball)
		return false;

	Vector3 launchDir = dir;
	if (launchDir.lengthSquared() < 1e-4f)
		launchDir = owner && owner->client ? owner->client->vForward
						   : Vector3{1.f, 0.f, 0.f};
	launchDir = launchDir.normalized();

	Vector3 up{}, right{}, forward{};
	AngleVectors(VectorToAngles(launchDir), forward, right, up);

	Ball_DetachCarrier(owner);

	const Vector3 spawn = Ball_AdjustOrigin(owner, start);
	ball->s.origin = spawn;
	ball->velocity = launchDir * speed;
	ball->velocity += up * BALL_THROW_UPWARD_SPEED;
	ball->velocity += right * (crandom() * BALL_SIDE_JITTER);
	ball->aVelocity = {crandom() * 180.f, crandom() * 180.f,
			   crandom() * 180.f};
	Ball_ApplyVelocityClamp(ball);

	Ball_StartWorldTravel(ball, owner, Ball_TeamForEntity(owner));
	gi.linkEntity(ball);

	ProBall::OnBallLaunched(owner, ball, spawn, ball->velocity);

	if (owner && owner->client)
		G_PlayerNoise(owner, spawn, PlayerNoise::Weapon);

	Ball_PlaySound(owner, "weapons/hgrena1b.wav");
	return true;
}

/*
=============
Ball_Pass
=============
*/
bool Ball_Pass(gentity_t* owner, const Vector3& start, const Vector3& dir) {
	return Ball_Launch(owner, start, dir, BALL_PASS_SPEED);
}

/*
=============
Ball_Drop
=============
*/
bool Ball_Drop(gentity_t* owner, const Vector3& origin) {
	if (!Ball_GametypeActive())
		return false;

	if (!Ball_PlayerHasBall(owner))
		return false;

	gentity_t* ball = Ball_EnsureEntity();
	if (!ball)
		return false;

	Ball_DetachCarrier(owner);

	Vector3 dropOrigin = origin;
	if (owner)
		dropOrigin =
		    owner->s.origin +
		    Vector3{0.f, 0.f,
			    static_cast<float>(owner->viewHeight) * 0.4f};

	ball->s.origin = Ball_AdjustOrigin(owner, dropOrigin);
	ball->velocity =
	    owner ? owner->velocity * BALL_DROP_OWNER_VEL_SCALE : Vector3{};
	ball->velocity[_Z] += BALL_DROP_UPWARD_SPEED;
	ball->aVelocity = {crandom() * 60.f, crandom() * 60.f,
			   crandom() * 60.f};
	Ball_ApplyVelocityClamp(ball);

	Ball_StartWorldTravel(ball, owner, Ball_TeamForEntity(owner));
	level.ball.idleBegin = level.time;
	gi.linkEntity(ball);

	ProBall::OnBallDropped(owner, ball, ball->s.origin, ball->velocity);

	Ball_PlaySound(owner, "weapons/hgrenb1a.wav");
	if (owner && owner->client)
		G_PlayerNoise(owner, ball->s.origin, PlayerNoise::Weapon);
	return true;
}

/*
=============
Ball_Reset
=============
*/
void Ball_Reset(bool silent) {
	if (!Ball_GametypeActive())
		return;

	if (!level.ball.homeValid)
		return;

	gentity_t* ball = Ball_EnsureEntity();
	if (!ball)
		return;

	Ball_InitEntity(ball);
	ball->s.origin = level.ball.homeOrigin;
	ball->s.angles = level.ball.homeAngles;
	ball->velocity = {};
	ball->aVelocity = {};
	ball->groundEntity = nullptr;
	ball->owner = nullptr;
	ball->touch_debounce_time = 0_ms;
	ball->fteam = Team::None;
	level.ball.carrier = nullptr;
	level.ball.idleBegin = 0_ms;

	Ball_StartWorldTravel(ball, nullptr, Team::None);
	gi.linkEntity(ball);

	if (!silent)
		Ball_PlaySound(ball, "items/respawn1.wav");

	if (Game::Is(GameType::ProBall))
		ProBall::OnBallReset(ball);
}

/*
=============
Ball_GetPassCooldown
=============
*/
GameTime Ball_GetPassCooldown() { return BALL_PASS_COOLDOWN; }

/*
=============
Ball_GetDropCooldown
=============
*/
GameTime Ball_GetDropCooldown() { return BALL_DROP_COOLDOWN; }

namespace ProBall {

namespace {

constexpr GameTime kAssistWindow = 8_sec;

/*
=============
State
=============
*/
LevelLocals::ProBallState& State() { return level.proBall; }

/*
=============
BallIsLoose
=============
*/
inline bool BallIsLoose(const LevelLocals::ProBallState& state) {
	return state.ballEntity && state.ballEntity->inUse && !state.carrier;
}

/*
=============
PlayerHasBall
=============
*/
inline bool PlayerHasBall(const gentity_t* player) {
	return player && player->client &&
	       player->client->pers.inventory[IT_BALL] > 0;
}

/*
=============
ValidPlayer
=============
*/
inline bool ValidPlayer(const gentity_t* ent) {
	return ent && ent->inUse && ent->client &&
	       ClientIsPlaying(ent->client) && !ent->client->eliminated;
}

/*
=============
ClearAssist
=============
*/
void ClearAssist(LevelLocals::ProBallState& state) { state.assist = {}; }

/*
=============
ResetBallToSpawn
=============
*/
void ResetBallToSpawn(LevelLocals::ProBallState& state, bool silent = true) {
	if (!state.spawnEntity || !state.spawnEntity->inUse)
		return;

	Ball_Reset(silent);
}

/*
=============
LogGoal
=============
*/
void LogGoal(const gentity_t* scorer, Team team) {
	if (!scorer || !scorer->client)
		return;

	G_LogEvent(G_Fmt("{} scored for the {} team",
			 scorer->client->sess.netName, Teams_TeamName(team))
		       .data());
}

/*
=============
AnnounceGoal
=============
*/
void AnnounceGoal(const gentity_t* scorer, Team team,
		  const gentity_t* goalEnt) {
	const char* teamName = Teams_TeamName(team);
	const char* goalLabel =
	    (goalEnt && goalEnt->message && goalEnt->message[0])
		? goalEnt->message
		: "goal";

	if (scorer && scorer->client) {
		gi.LocBroadcast_Print(
		    PRINT_HIGH, "{} scores for the {} at {}!\n",
		    scorer->client->sess.netName, teamName, goalLabel);
	} else {
		gi.LocBroadcast_Print(PRINT_HIGH, "{} scores!\n", teamName);
	}

	AnnouncerSound(world, team == Team::Red ? "red_scores" : "blue_scores");
}

/*
=============
AwardAssist
=============
*/
void AwardAssist(LevelLocals::ProBallState& state, gentity_t* scorer,
		 Team scoringTeam) {
	if (!state.assist.player || state.assist.team != scoringTeam)
		return;

	if (state.assist.expires && state.assist.expires <= level.time)
		return;

	gentity_t* assistPlayer = state.assist.player;
	if (!ValidPlayer(assistPlayer) || assistPlayer == scorer)
		return;

	if (!assistPlayer->client)
		return;

	G_AdjustPlayerScore(assistPlayer->client, 1, false, 0);
	assistPlayer->client->pers.match.proBallAssists++;
	level.match.proBallAssists++;

	gi.LocBroadcast_Print(PRINT_HIGH, "Assist: {}\n",
			      assistPlayer->client->sess.netName);
}

/*
=============
AwardGoal
=============
*/
void AwardGoal(gentity_t* scorer, Team team, gentity_t* goalEnt) {
	auto& state = State();

	if (team != Team::Red && team != Team::Blue)
		return;

	if (ScoringIsDisabled() ||
	    level.matchState != MatchState::In_Progress) {
		ResetBallToSpawn(state);
		return;
	}

	G_AdjustTeamScore(team, 1);
	level.match.proBallGoals++;

	if (ValidPlayer(scorer)) {
		G_AdjustPlayerScore(scorer->client, 1, false, 0);
		scorer->client->pers.match.proBallGoals++;
		scorer->client->pers.inventory[IT_BALL] = 0;
	}

	AwardAssist(state, scorer, team);

	AnnounceGoal(scorer, team, goalEnt);
	LogGoal(scorer, team);

	ResetBallToSpawn(state);
}

/*
=============
CarrierDropOrigin
=============
*/
Vector3 CarrierDropOrigin(const gentity_t* carrier) {
	Vector3 start = carrier->s.origin;
	start.z += carrier->viewHeight * 0.5f;
	return start;
}

/*
=============
ComputeDropVelocity
=============
*/
Vector3 ComputeDropVelocity(const gentity_t* carrier,
			    const gentity_t* instigator, bool forced) {
	Vector3 result = carrier ? carrier->velocity : vec3_origin;

	if (forced && instigator && instigator->client) {
		result += instigator->client->vForward * 450.0f;
		result += Vector3{0.0f, 0.0f, 150.0f};
	} else {
		result += Vector3{0.0f, 0.0f, 150.0f};
	}

	return result;
}

/*
=============
ResolveGoalTeam
=============
*/
Team ResolveGoalTeam(const LevelLocals::ProBallState::GoalVolume* volume,
		     const gentity_t* actor) {
	if (volume && volume->team != Team::None)
		return volume->team;

	if (actor && actor->client)
		return actor->client->sess.team;

	return Team::None;
}

/*
=============
FindGoalVolume
=============
*/
LevelLocals::ProBallState::GoalVolume* FindGoalVolume(gentity_t* ent) {
	auto& state = State();
	for (auto& goal : state.goals) {
		if (goal.ent == ent)
			return &goal;
	}
	return nullptr;
}

/*
=============
PointInVolume
=============
*/
bool PointInVolume(const Vector3& point, const gentity_t* volume) {
	if (!volume || !volume->inUse)
		return false;

	return point.x >= volume->absMin.x && point.x <= volume->absMax.x &&
	       point.y >= volume->absMin.y && point.y <= volume->absMax.y &&
	       point.z >= volume->absMin.z && point.z <= volume->absMax.z;
}

/*
=============
ResetCarrierInventory
=============
*/
void ResetCarrierInventory(gentity_t* carrier) {
	if (carrier && carrier->client)
		carrier->client->pers.inventory[IT_BALL] = 0;
}

} // namespace

/*
=============
ClearState
=============
*/
void ClearState() { level.proBall = {}; }

/*
=============
InitLevel
=============
*/
void InitLevel() {
	auto& state = State();

	if (Game::IsNot(GameType::ProBall)) {
		ClearState();
		return;
	}

	if (state.spawnEntity && state.spawnEntity->inUse) {
		ResetBallToSpawn(state);
	}

	state.assist = {};
}

/*
=============
RunFrame
=============
*/
void RunFrame() {
	if (Game::IsNot(GameType::ProBall))
		return;

	auto& state = State();

	if (state.assist.player) {
		if (!ValidPlayer(state.assist.player) ||
		    (state.assist.expires &&
		     state.assist.expires <= level.time)) {
			state.assist = {};
		}
	}

	if (state.carrier) {
		if (!ValidPlayer(state.carrier) ||
		    !PlayerHasBall(state.carrier)) {
			state.carrier = nullptr;
		}
	}

	if (BallIsLoose(state)) {
		gentity_t* ball = state.ballEntity;
		const Vector3& origin = ball->s.origin;

		const contents_t contents = gi.pointContents(origin);
		if (contents & (CONTENTS_LAVA | CONTENTS_SLIME)) {
			gi.LocBroadcast_Print(PRINT_HIGH,
					      "Ball reset (hazard).\n");
			ResetBallToSpawn(state);
			return;
		}

		if (origin.z < world->absMin.z - 64.0f) {
			gi.LocBroadcast_Print(PRINT_HIGH,
					      "Ball reset (fell out).\n");
			ResetBallToSpawn(state);
			return;
		}

		for (const auto& oob : state.outOfBounds) {
			if (PointInVolume(origin, oob)) {
				gi.LocBroadcast_Print(
				    PRINT_HIGH,
				    "Ball reset (out of bounds).\n");
				ResetBallToSpawn(state);
				return;
			}
		}

		for (const auto& goal : state.goals) {
			if (!goal.ent || !goal.ent->inUse)
				continue;

			if (PointInVolume(origin, goal.ent)) {
				Team scoringTeam =
				    ResolveGoalTeam(&goal, state.lastToucher);
				AwardGoal(state.lastToucher, scoringTeam,
					  goal.ent);
				return;
			}
		}
	}
}

/*
=============
RegisterBallSpawn
=============
*/
void RegisterBallSpawn(gentity_t* ent) {
	if (!ent)
		return;

	auto& state = State();

	state.spawnEntity = ent;
	state.ballEntity = ent;
}

/*
=============
OnBallPickedUp
=============
*/
void OnBallPickedUp(gentity_t* ballEnt, gentity_t* player) {
	if (Game::IsNot(GameType::ProBall) || !player || !player->client)
		return;

	auto& state = State();

	if (!state.spawnEntity)
		state.spawnEntity = ballEnt;

	state.carrier = player;
	state.lastToucher = player;
	state.lastTouchTime = level.time;
	state.ballEntity = nullptr;

	if (state.spawnEntity && state.spawnEntity->inUse) {
		state.spawnEntity->svFlags |= SVF_NOCLIENT;
		state.spawnEntity->solid = SOLID_NOT;
		state.spawnEntity->moveType = MoveType::None;
		gi.linkEntity(state.spawnEntity);
	}

	if (state.assist.player &&
	    state.assist.team != player->client->sess.team)
		state.assist = {};
}

/*
=============
OnBallLaunched
=============
*/
void OnBallLaunched(gentity_t* owner, gentity_t* ballEnt, const Vector3&,
		    const Vector3&) {
	if (Game::IsNot(GameType::ProBall))
		return;

	auto& state = State();

	if (ballEnt && ballEnt->inUse)
		state.ballEntity = ballEnt;

	state.carrier = nullptr;
	if (ValidPlayer(owner))
		state.lastToucher = owner;
	else if (!owner)
		state.lastToucher = nullptr;
	state.lastTouchTime = level.time;
	state.assist = {};
}

/*
=============
OnBallDropped
=============
*/
void OnBallDropped(gentity_t* owner, gentity_t* ballEnt, const Vector3&,
		   const Vector3&) {
	if (Game::IsNot(GameType::ProBall))
		return;

	auto& state = State();

	if (ballEnt && ballEnt->inUse)
		state.ballEntity = ballEnt;

	state.carrier = nullptr;
	if (ValidPlayer(owner))
		state.lastToucher = owner;
	else if (!owner)
		state.lastToucher = nullptr;
	state.lastTouchTime = level.time;
	state.assist = {};
}

/*
=============
OnBallReset
=============
*/
void OnBallReset(gentity_t* ball) {
	if (!ball || !ball->inUse)
		return;

	auto& state = State();
	state.ballEntity = ball;
	state.spawnEntity = ball;
	state.carrier = nullptr;
	state.lastToucher = nullptr;
	state.lastTouchTime = 0_ms;
	ClearAssist(state);
}

/*
=============
DropBall
=============
*/
bool DropBall(gentity_t* carrier, gentity_t* instigator, bool forced) {
	if (Game::IsNot(GameType::ProBall) || !carrier || !carrier->client)
		return false;

	if (!PlayerHasBall(carrier))
		return false;

	Vector3 origin = CarrierDropOrigin(carrier);
	if (!Ball_Drop(carrier, origin))
		return false;

	auto& state = State();

	if (forced && instigator && instigator->client) {
		state.assist.player = instigator;
		state.assist.team = instigator->client->sess.team;
		state.assist.expires = level.time + kAssistWindow;
		state.lastToucher = instigator;
		state.lastTouchTime = level.time;
	}

	if (forced && state.ballEntity) {
		state.ballEntity->velocity =
		    ComputeDropVelocity(carrier, instigator, true);
		gi.linkEntity(state.ballEntity);
	}

	gi.LocBroadcast_Print(PRINT_HIGH, "{} drops the ball!\n",
			      carrier->client->sess.netName);
	return true;
}

/*
=============
ThrowBall
=============
*/
bool ThrowBall(gentity_t* carrier, const Vector3& origin, const Vector3& dir) {
	if (Game::IsNot(GameType::ProBall) || !carrier || !carrier->client)
		return false;

	if (!PlayerHasBall(carrier))
		return false;

	if (!Ball_Pass(carrier, origin, dir))
		return false;

	gi.LocBroadcast_Print(PRINT_HIGH, "{} throws the ball!\n",
			      carrier->client->sess.netName);
	return true;
}

/*
=============
HandleCarrierDeath
=============
*/
void HandleCarrierDeath(gentity_t* carrier) {
	if (Game::IsNot(GameType::ProBall))
		return;

	if (!PlayerHasBall(carrier))
		return;

	DropBall(carrier, nullptr, false);
}

/*
=============
HandleCarrierDisconnect
=============
*/
void HandleCarrierDisconnect(gentity_t* carrier) {
	if (Game::IsNot(GameType::ProBall))
		return;

	if (!PlayerHasBall(carrier))
		return;

	DropBall(carrier, nullptr, false);
}

/*
=============
HandleCarrierHit
=============
*/
bool HandleCarrierHit(gentity_t* carrier, gentity_t* attacker,
		const MeansOfDeath& mod) {
	if (Game::IsNot(GameType::ProBall))
		return false;

	if (!PlayerHasBall(carrier))
		return false;

	if (!attacker || !attacker->client)
		return false;

	if (OnSameTeam(carrier, attacker))
		return false;

	if (mod.id != ModID::Chainfist)
		return false;

	DropBall(carrier, attacker, true);
	return true;
}

/*
=============
RegisterGoalVolume
=============
*/
static void RegisterGoalVolume(gentity_t* ent) {
	if (!ent)
		return;

	auto& state = State();

	for (auto& slot : state.goals) {
		if (!slot.ent) {
			slot.ent = ent;
			slot.team = Team::None;
			if (ent->spawnFlags.has(SPAWNFLAG_PROBALL_GOAL_RED) &&
			    !ent->spawnFlags.has(SPAWNFLAG_PROBALL_GOAL_BLUE))
				slot.team = Team::Red;
			else if (ent->spawnFlags.has(
				     SPAWNFLAG_PROBALL_GOAL_BLUE) &&
				 !ent->spawnFlags.has(
				     SPAWNFLAG_PROBALL_GOAL_RED))
				slot.team = Team::Blue;
			return;
		}
	}

	gi.Com_PrintFmt("ProBall: ignoring goal {} (too many volumes).\n",
			*ent);
}

/*
=============
RegisterOutOfBoundsVolume
=============
*/
static void RegisterOutOfBoundsVolume(gentity_t* ent) {
	if (!ent)
		return;

	auto& state = State();

	for (auto& slot : state.outOfBounds) {
		if (!slot) {
			slot = ent;
			return;
		}
	}

	gi.Com_PrintFmt(
	    "ProBall: ignoring out-of-bounds volume {} (too many volumes).\n",
	    *ent);
}

/*
=============
GoalTouch
=============
*/
static TOUCH(GoalTouch)(gentity_t* self, gentity_t* other, const trace_t&, bool) -> void {
	if (Game::IsNot(GameType::ProBall) || !other || !other->client)
		return;

	if (!PlayerHasBall(other))
		return;

	auto* goal = FindGoalVolume(self);
	Team scoringTeam = ResolveGoalTeam(goal, other);

	AwardGoal(other, scoringTeam, self);
}

/*
=============
OutOfBoundsTouch
=============
*/
static TOUCH(OutOfBoundsTouch)(gentity_t* self, gentity_t* other,
			    const trace_t&, bool) -> void {
	if (Game::IsNot(GameType::ProBall))
		return;

	auto& state = State();

	if (other == state.ballEntity) {
		gi.LocBroadcast_Print(PRINT_HIGH,
				      "Ball reset (out of bounds).\n");
		ResetBallToSpawn(state);
		return;
	}

	if (other && other->client && PlayerHasBall(other)) {
		gi.LocBroadcast_Print(PRINT_HIGH,
				      "Ball reset (out of bounds).\n");
		ResetCarrierInventory(other);
		ResetBallToSpawn(state);
	}
}

/*
=============
SpawnGoalTrigger
=============
*/
static void SpawnGoalTrigger(gentity_t* ent) {
	if (!ent)
		return;

	ent->solid = SOLID_TRIGGER;
	ent->moveType = MoveType::None;
	ent->svFlags |= SVF_NOCLIENT;
	ent->clipMask = CONTENTS_PLAYER | CONTENTS_MONSTER;
	ent->touch = GoalTouch;

	if (ent->model && ent->model[0])
		gi.setModel(ent, ent->model);

	gi.linkEntity(ent);
	RegisterGoalVolume(ent);
}

/*
=============
SpawnOutOfBoundsTrigger
=============
*/
static void SpawnOutOfBoundsTrigger(gentity_t* ent) {
	if (!ent)
		return;

	ent->solid = SOLID_TRIGGER;
	ent->moveType = MoveType::None;
	ent->svFlags |= SVF_NOCLIENT;
	ent->clipMask = CONTENTS_PLAYER | CONTENTS_MONSTER;
	ent->touch = OutOfBoundsTouch;

	if (ent->model && ent->model[0])
		gi.setModel(ent, ent->model);

	gi.linkEntity(ent);
	RegisterOutOfBoundsVolume(ent);
}

} // namespace ProBall

/*
=============
SP_trigger_proball_goal
=============
*/
void SP_trigger_proball_goal(gentity_t* ent) { ProBall::SpawnGoalTrigger(ent); }

/*
=============
SP_trigger_proball_oob
=============
*/
void SP_trigger_proball_oob(gentity_t* ent) {
	ProBall::SpawnOutOfBoundsTrigger(ent);
}
