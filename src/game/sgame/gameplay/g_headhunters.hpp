#pragma once

#include <cstdint>

struct gclient_t;
struct gentity_t;
struct trace_t;
struct Vector3;
enum class Team : uint8_t;

namespace HeadHunters {
	constexpr uint8_t kMaxCarriedHeads = 20;

	void ClearState();
	void InitLevel();
	void RunFrame();
	void ResetPlayerState(gclient_t* client);
	void DropHeads(gentity_t* player, gentity_t* instigator);
	gentity_t* SpawnGroundHead(const Vector3& origin, const Vector3& velocity, Team team);
	void HandlePickup(gentity_t* ent, gentity_t* other, const trace_t& tr, bool otherTouchingSelf);
	void RegisterReceptacle(gentity_t* ent);
	void ApplyReceptacleVisuals(gentity_t* ent, Team team);
	void OnReceptacleTouch(gentity_t* ent, gentity_t* other, const trace_t& tr, bool otherTouchingSelf);
	void SP_headhunters_receptacle(gentity_t* ent);
}
