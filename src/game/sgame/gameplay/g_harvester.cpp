/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

g_harvester.cpp implementation.*/

#include "../g_local.hpp"
#include "g_capture.hpp"
#include "g_teamplay.hpp"
#include "g_headhunters.hpp"
#include "g_harvester.hpp"

extern gentity_t* neutralObelisk;

namespace {
	constexpr GameTime HARVESTER_SKULL_LIFETIME = 30_sec;
	constexpr GameTime HARVESTER_REMINDER_COOLDOWN = 2_sec;
	constexpr float HARVESTER_SKULL_HORIZONTAL_TOSS = 60.0f;
	constexpr float HARVESTER_SKULL_VERTICAL_TOSS = 90.0f;
	constexpr int HARVESTER_MAX_SKULLS_PER_DROP = 16;
	constexpr Vector3 HARVESTER_BASE_MINS{ -24.0f, -24.0f, 0.0f };
	constexpr Vector3 HARVESTER_BASE_MAXS{ 24.0f, 24.0f, 64.0f };

	THINK(Harvester_SkullExpire)(gentity_t* ent)->void {
		if (!ent) {
			return;
		}
		FreeEntity(ent);
	}

	void Harvester_PositionOnFloor_Internal(gentity_t* ent) {
		if (!ent) {
			return;
		}

		Vector3 start = ent->s.origin;
		start.z += 1.0f;
		Vector3 end = start;
		end.z -= 4096.0f;

		const trace_t tr = gi.trace(start, HARVESTER_BASE_MINS, HARVESTER_BASE_MAXS, end, ent, MASK_SOLID);
		if (!tr.startSolid) {
			ent->s.origin = tr.endPos;
		}
	}


	/*
	=============
	Harvester_BaseTouch

	Handles scoring interactions for Harvester and OneFlag gametypes.
	=============
	*/
	TOUCH(Harvester_BaseTouch)(gentity_t* ent, gentity_t* other, const trace_t&, bool)->void {
		const bool harvester = Harvester_Active();
		const bool oneFlag = Game::Is(GameType::OneFlag);

		if (!harvester && !oneFlag) {
			return;
		}
		if (!ent || !other || !other->client) {
			return;
		}

		const Team baseTeam = ent->fteam;
		if (!Teamplay_IsPrimaryTeam(baseTeam)) {
			return;
		}

		if (other->client->sess.team != baseTeam) {
			return;
		}

		if (harvester) {
			const int tokens = other->client->ps.stats[STAT_GAMEPLAY_CARRIED];
			if (tokens <= 0) {
				Harvester_SendMissingObjectiveReminder(other, true, false);
				return;
			}

			other->client->ps.stats[STAT_GAMEPLAY_CARRIED] = 0;
			G_AdjustPlayerScore(other->client, tokens, true, tokens);

			level.ctf_last_flag_capture = level.time;
			level.ctf_last_capture_team = baseTeam;

			const auto msg = G_Fmt("{} delivered {} skull{}.",
				other->client->sess.netName,
				tokens,
				tokens == 1 ? "" : "s");
			gi.LocBroadcast_Print(PRINT_HIGH, msg.data());
			Team_CaptureFlagSound(baseTeam);
			return;
		}

		if (!oneFlag) {
			return;
		}

		if (!other->client->pers.inventory[IT_FLAG_NEUTRAL]) {
			Harvester_SendMissingObjectiveReminder(other, false, true);
			return;
		}

		GameTime pickupTime = other->client->pers.teamState.flag_pickup_time;
		if (!pickupTime) {
			pickupTime = other->client->resp.ctf_flagsince;
		}
		other->client->pers.inventory[IT_FLAG_NEUTRAL] = 0;
		other->client->pers.teamState.flag_pickup_time = 0_ms;
		other->client->resp.ctf_flagsince = 0_ms;

		AwardFlagCapture(ent, other, baseTeam, pickupTime);
		SetFlagStatus(Team::Free, FlagStatus::AtBase);
		CTF_ResetTeamFlag(Team::Free);
	}

	/*
	=============
	Harvester_GeneratorTouch

	Intentional no-op touch handler to keep the generator trigger non-blocking while
	documenting that overlaps are expected.
	=============
	*/
	TOUCH(Harvester_GeneratorTouch)(gentity_t*, gentity_t*, const trace_t&, bool)->void {}

	void Harvester_AssignRandomOrigin(gentity_t* skull, const Vector3& fallback, bool dropAtFallback) {
		const float horizontalRandomX = crandom();
		const float horizontalRandomY = crandom();
		const float verticalRandom = crandom();
		const Vector3 base = Harvester_GeneratorOrigin(fallback);
		Vector3 origin = Harvester_ComputeSkullOrigin(
			base,
			fallback,
			dropAtFallback,
			horizontalRandomX,
			horizontalRandomY,
			verticalRandom);
		skull->s.origin = origin;
	}
}

/*
=============
Harvester_SendMissingObjectiveReminder

Sends a short, localized reminder when a player tries to score without a required objective.
=============
*/
void Harvester_SendMissingObjectiveReminder(gentity_t* ent, bool harvesterMode, bool oneFlagMode) {
	if ((!harvesterMode && !oneFlagMode) || !ent || !ent->client) {
		return;
	}

	GameTime& nextReminderTime = ent->client->harvesterReminderTime;
	if (level.time < nextReminderTime) {
		return;
	}

	nextReminderTime = level.time + HARVESTER_REMINDER_COOLDOWN;
	const char* message = harvesterMode ? "$g_harvester_need_skulls" : "$g_oneflag_need_flag";
	gi.LocClient_Print(ent, PRINT_HIGH, message);
}

bool Harvester_Active() {
	return Game::Is(GameType::Harvester);
}

Vector3 Harvester_GeneratorOrigin(const Vector3& fallback) {
	if (level.harvester.generator && level.harvester.generator->inUse) {
		return level.harvester.generator->s.origin;
	}
	return fallback;
}

void Harvester_PositionOnFloor(gentity_t* ent) {
	Harvester_PositionOnFloor_Internal(ent);
}

static void Harvester_SetupSkullEntity(gentity_t* skull, Item* item, Team team) {
	skull->className = item->className;
	skull->item = item;
	skull->s.effects = item->worldModelFlags;
	skull->s.renderFX |= RF_GLOW | RF_NO_LOD | RF_IR_VISIBLE;
	if (team == Team::Red) {
		skull->s.renderFX |= RF_SHELL_RED;
	}
	else if (team == Team::Blue) {
		skull->s.renderFX |= RF_SHELL_BLUE;
	}

	skull->mins = { -12.0f, -12.0f, -12.0f };
	skull->maxs = { 12.0f, 12.0f, 12.0f };
	skull->solid = SOLID_TRIGGER;
	skull->clipMask = MASK_SOLID;
	skull->moveType = MoveType::Toss;
	skull->touch = Touch_Item;
	skull->think = Harvester_SkullExpire;
	skull->nextThink = level.time + HARVESTER_SKULL_LIFETIME;
	skull->spawnFlags |= SPAWNFLAG_ITEM_DROPPED_PLAYER;
	skull->fteam = team;
}

/*
=============
Harvester_SpawnSkull

Attempts to spawn a skull entity and returns the spawn result status.
=============
*/
HarvesterSpawnResult Harvester_SpawnSkull(Team team, const Vector3& fallback, bool dropAtFallback) {
	HarvesterSpawnResult result;

	if (!Harvester_Active()) {
		return result;
	}

	Item* item = GetItemByIndex(IT_HARVESTER_SKULL);
	if (!item) {
		++level.harvester.spawnFailureCount;
		gi.Com_PrintFmt("{}: missing harvester skull item for {} team.\n", __FUNCTION__, Teams_TeamName(team));
		result.status = HarvesterSpawnStatus::MissingItem;
		return result;
	}

	gentity_t* skull = Spawn();
	if (!skull) {
		++level.harvester.spawnFailureCount;
		gi.Com_PrintFmt("{}: failed to allocate skull entity for {} team.\n", __FUNCTION__, Teams_TeamName(team));
		result.status = HarvesterSpawnStatus::AllocationFailed;
		return result;
	}

	Harvester_SetupSkullEntity(skull, item, team);
	Harvester_AssignRandomOrigin(skull, fallback, dropAtFallback);

	skull->velocity = {
		crandom() * HARVESTER_SKULL_HORIZONTAL_TOSS,
		crandom() * HARVESTER_SKULL_HORIZONTAL_TOSS,
		HARVESTER_SKULL_VERTICAL_TOSS + frandom() * HARVESTER_SKULL_VERTICAL_TOSS };

	gi.setModel(skull, item->worldModel);
	gi.linkEntity(skull);

	result.status = HarvesterSpawnStatus::Success;
	result.entity = skull;
	return result;
}

/*
=============
Harvester_DropSkulls

Spawns requested skulls while tracking pending failures to keep totals consistent.
=============
*/
int Harvester_DropSkulls(Team team, int count, const Vector3& fallback, bool dropAtFallback) {
	const size_t teamIndex = static_cast<size_t>(team);
	const int requested = level.harvester.pendingDrops.at(teamIndex) + count;

	if (!Teamplay_IsPrimaryTeam(team) || requested <= 0) {
		return 0;
	}

	const int toSpawn = std::min(requested, HARVESTER_MAX_SKULLS_PER_DROP);
	const int deferred = requested - toSpawn;
	if (deferred > 0) {
		gi.Com_PrintFmt("{}: clamping {} {} skull drop(s) to {} per tick ({} deferred). Pending total: {}\n",
			__FUNCTION__,
			requested,
			Teams_TeamName(team),
			HARVESTER_MAX_SKULLS_PER_DROP,
			deferred,
			deferred);
	}

	level.harvester.pendingDrops.at(teamIndex) = deferred;
	int spawned = 0;

	for (int i = 0; i < toSpawn; ++i) {
		const HarvesterSpawnResult result = Harvester_SpawnSkull(team, fallback, dropAtFallback);
		if (result.status == HarvesterSpawnStatus::Success && result.entity) {
			++spawned;
			continue;
		}

		const int remaining = toSpawn - i;
		level.harvester.pendingDrops.at(teamIndex) = deferred + remaining;

		if (result.status != HarvesterSpawnStatus::Inactive) {
			const char* reason = result.status == HarvesterSpawnStatus::MissingItem ? "missing skull item" : "skull allocation failed";
			gi.Com_PrintFmt("{}: deferring {} {} skull drop(s) due to {}. Pending total: {}\n",
				__FUNCTION__,
				remaining,
				Teams_TeamName(team),
				reason,
				level.harvester.pendingDrops.at(teamIndex));
		}
		break;
	}

	return spawned;
}

void Harvester_RegisterBase(gentity_t* ent, Team team) {
	if (!ent) {
		return;
	}

	Harvester_PositionOnFloor_Internal(ent);
	ent->mins = HARVESTER_BASE_MINS;
	ent->maxs = HARVESTER_BASE_MAXS;
	ent->solid = SOLID_TRIGGER;
	ent->clipMask = MASK_PLAYERSOLID;
	ent->moveType = MoveType::None;
	ent->touch = Harvester_BaseTouch;
	ent->fteam = team;
	gi.linkEntity(ent);

	if (Harvester_Active()) {
		const size_t index = team == Team::Red ? 0 : 1;
		level.harvester.bases.at(index) = ent;
	}
}

void Harvester_RegisterGenerator(gentity_t* ent) {
	if (!ent) {
		return;
	}

	Harvester_PositionOnFloor_Internal(ent);
	ent->mins = HARVESTER_BASE_MINS;
	ent->maxs = HARVESTER_BASE_MAXS;
	ent->solid = SOLID_TRIGGER;
	ent->clipMask = MASK_PLAYERSOLID;
	ent->moveType = MoveType::None;
	ent->touch = Harvester_GeneratorTouch;
	gi.linkEntity(ent);

	level.harvester.generator = ent;
}

bool Harvester_TakeSkull(gentity_t* ent, gentity_t* other) {
	if (!ent || !other || !other->client) {
		return false;
	}

	const item_id_t skullItem = ent->item ? ent->item->id : IT_NULL;
	if (skullItem != IT_HARVESTER_SKULL) {
		return false;
	}

	const Team skullTeam = ent->fteam;
	const Team playerTeam = other->client->sess.team;
	const Team enemyTeam = Teams_OtherTeam(playerTeam);

	if (enemyTeam != skullTeam) {
		return false;
	}

	constexpr int MAX_SKULLS = 99;
	auto& carried = other->client->ps.stats[STAT_GAMEPLAY_CARRIED];
	if (carried < MAX_SKULLS) {
		const int next = static_cast<int>(carried) + 1;
		carried = static_cast<int16_t>(std::min(MAX_SKULLS, next));
	}

	return true;
}

/*
=============
Harvester_Reset

Clears harvester structures, active skulls, and pending drop bookkeeping.
=============
*/
void Harvester_Reset() {
	level.harvester.generator = nullptr;
	level.harvester.bases.fill(nullptr);
	level.harvester.pendingDrops.fill(0);
	level.harvester.spawnFailureCount = 0;

	for (size_t i = 0; i < globals.numEntities; ++i) {
		gentity_t* ent = &g_entities[i];
		if (!ent->inUse) {
			continue;
		}
		if (ent->item && ent->item->id == IT_HARVESTER_SKULL) {
			FreeEntity(ent);
		}
	}

	for (auto entity : active_clients()) {
		if (entity && entity->client) {
			entity->client->ps.stats[STAT_GAMEPLAY_CARRIED] = 0;
		}
	}
}

void Harvester_HandlePlayerDeath(gentity_t* victim) {
	if (!Harvester_Active() || !victim || !victim->client) {
		return;
	}

	const Team team = victim->client->sess.team;
	if (!Teamplay_IsPrimaryTeam(team)) {
		return;
	}

	const Team enemy = Teams_OtherTeam(team);
	const int carried = victim->client->ps.stats[STAT_GAMEPLAY_CARRIED];
	if (carried > 0 && Teamplay_IsPrimaryTeam(enemy)) {
		Harvester_DropSkulls(enemy, carried, victim->s.origin, true);
	}

	Harvester_DropSkulls(team, 1, victim->s.origin, false);
	victim->client->ps.stats[STAT_GAMEPLAY_CARRIED] = 0;
}

void Harvester_HandlePlayerDisconnect(gentity_t* ent) {
	if (!Harvester_Active() || !ent || !ent->client) {
		return;
	}

	const Team team = ent->client->sess.team;
	if (!Teamplay_IsPrimaryTeam(team)) {
		return;
	}

	const Team enemy = Teams_OtherTeam(team);
	const int carried = ent->client->ps.stats[STAT_GAMEPLAY_CARRIED];
	if (carried > 0 && Teamplay_IsPrimaryTeam(enemy)) {
		Harvester_DropSkulls(enemy, carried, ent->s.origin, true);
	}

	ent->client->ps.stats[STAT_GAMEPLAY_CARRIED] = 0;
	Harvester_DropSkulls(team, 1, ent->s.origin, false);
}

void Harvester_HandleTeamChange(gentity_t* ent) {
	if (!Harvester_Active() || !ent || !ent->client) {
		return;
	}

	const Team team = ent->client->sess.team;
	if (!Teamplay_IsPrimaryTeam(team)) {
		return;
	}

	const Team enemy = Teams_OtherTeam(team);
	const int carried = ent->client->ps.stats[STAT_GAMEPLAY_CARRIED];
	if (carried > 0 && Teamplay_IsPrimaryTeam(enemy)) {
		Harvester_DropSkulls(enemy, carried, ent->s.origin, true);
	}

	ent->client->ps.stats[STAT_GAMEPLAY_CARRIED] = 0;
}

void Harvester_OnClientSpawn(gentity_t* ent) {
	if (!ent || !ent->client) {
		return;
	}

	ent->client->ps.stats[STAT_GAMEPLAY_CARRIED] = 0;
}

bool Harvester_PickupSkull(gentity_t* ent, gentity_t* other) {
	if (!Harvester_Active()) {
		return false;
	}

	return Harvester_TakeSkull(ent, other);
}

void Harvester_FlagSetup(gentity_t* ent) {
	if (!ent) {
		return;
	}

	Harvester_PositionOnFloor_Internal(ent);
	ent->mins = HARVESTER_BASE_MINS;
	ent->maxs = HARVESTER_BASE_MAXS;
	ent->solid = SOLID_TRIGGER;
	ent->clipMask = MASK_SOLID;
	ent->moveType = MoveType::None;
	ent->touch = nullptr;
	gi.linkEntity(ent);
}


/*
=============
SP_team_redobelisk

Registers the red obelisk spawn if the gametype requires it.
=============
*/
void SP_team_redobelisk(gentity_t* ent) {
	if (Game::Is(GameType::Harvester) || Game::Is(GameType::OneFlag)) {
		Harvester_RegisterBase(ent, Team::Red);
		if (Game::Is(GameType::OneFlag)) {
			HeadHunters::ApplyReceptacleVisuals(ent, Team::Red);
		}
		return;
	}

	FreeEntity(ent);
}

/*
=============
SP_team_blueobelisk

Registers the blue obelisk spawn if the gametype requires it.
=============
*/
void SP_team_blueobelisk(gentity_t* ent) {
	if (Game::Is(GameType::Harvester) || Game::Is(GameType::OneFlag)) {
		Harvester_RegisterBase(ent, Team::Blue);
		if (Game::Is(GameType::OneFlag)) {
			HeadHunters::ApplyReceptacleVisuals(ent, Team::Blue);
		}
		return;
	}

	FreeEntity(ent);
}

/*
=============
SP_team_neutralobelisk

Handles the neutral obelisk spawn for Harvester and One Flag.
=============
*/
void SP_team_neutralobelisk(gentity_t* ent) {
	const bool harvester = Game::Is(GameType::Harvester);
	const bool oneFlag = Game::Is(GameType::OneFlag);

	if (!oneFlag) {
		neutralObelisk = nullptr;
	}

	if (harvester) {
		Harvester_RegisterGenerator(ent);
		return;
	}

	if (oneFlag) {
		neutralObelisk = ent;
		return;
	}

	FreeEntity(ent);
}
