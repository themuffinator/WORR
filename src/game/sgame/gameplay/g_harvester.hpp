#pragma once

#include "../../bgame/q_std.hpp"

struct gentity_t;
enum class Team : uint8_t;

enum class HarvesterSpawnStatus : uint8_t {
	Inactive,
	MissingItem,
	AllocationFailed,
	Success
};

struct HarvesterSpawnResult {
	HarvesterSpawnStatus status = HarvesterSpawnStatus::Inactive;
	gentity_t* entity = nullptr;
};

[[nodiscard]] inline Vector3 Harvester_ComputeSkullOrigin(
const Vector3& generatorOrigin,
const Vector3& fallback,
bool dropAtFallback,
	float horizontalRandomX,
	float horizontalRandomY,
	float verticalRandom) {
	Vector3 origin = dropAtFallback ? fallback : generatorOrigin;
	if (!dropAtFallback) {
		origin.x += horizontalRandomX * 24.0f;
		origin.y += horizontalRandomY * 24.0f;
	}
	origin.z += 16.0f + std::fabs(verticalRandom * 12.0f);
return origin;
}

bool Harvester_Active();
Vector3 Harvester_GeneratorOrigin(const Vector3& fallback);
void Harvester_PositionOnFloor(gentity_t* ent);
HarvesterSpawnResult Harvester_SpawnSkull(Team team, const Vector3& fallback, bool dropAtFallback);
int Harvester_DropSkulls(Team team, int count, const Vector3& fallback, bool dropAtFallback);
void Harvester_RegisterBase(gentity_t* ent, Team team);
void Harvester_RegisterGenerator(gentity_t* ent);
void Harvester_SendMissingObjectiveReminder(gentity_t* ent, bool harvesterMode, bool oneFlagMode);
bool Harvester_TakeSkull(gentity_t* ent, gentity_t* other);
void Harvester_Reset();
void Harvester_HandlePlayerDeath(gentity_t* victim);
void Harvester_HandlePlayerDisconnect(gentity_t* ent);
void Harvester_HandleTeamChange(gentity_t* ent);
void Harvester_OnClientSpawn(gentity_t* ent);
bool Harvester_PickupSkull(gentity_t* ent, gentity_t* other);
void Harvester_FlagSetup(gentity_t* ent);
void SP_team_redobelisk(gentity_t* ent);
void SP_team_blueobelisk(gentity_t* ent);
void SP_team_neutralobelisk(gentity_t* ent);
