#pragma once

#include "../g_local.hpp"

void		Ball_RegisterSpawn(gentity_t* ent);
void		Ball_OnPickup(gentity_t* ball, gentity_t* player);
bool		Ball_Launch(gentity_t* owner, const Vector3& start, const Vector3& dir, float speed);
bool		Ball_Pass(gentity_t* owner, const Vector3& start, const Vector3& dir);
bool		Ball_Drop(gentity_t* owner, const Vector3& origin);
void		Ball_Reset(bool silent);
GameTime	Ball_GetPassCooldown();
GameTime	Ball_GetDropCooldown();
bool		Ball_PlayerHasBall(const gentity_t* ent);

namespace ProBall {
	void ClearState();
	void InitLevel();
	void RunFrame();
	void RegisterBallSpawn(gentity_t* ent);
	void OnBallPickedUp(gentity_t* ballEnt, gentity_t* player);
	bool DropBall(gentity_t* carrier, gentity_t* instigator, bool forced);
	bool ThrowBall(gentity_t* carrier, const Vector3& origin, const Vector3& dir);
	void OnBallLaunched(gentity_t* owner, gentity_t* ballEnt, const Vector3& origin, const Vector3& velocity);
	void OnBallDropped(gentity_t* owner, gentity_t* ballEnt, const Vector3& origin, const Vector3& velocity);
	void HandleCarrierDeath(gentity_t* carrier);
	void HandleCarrierDisconnect(gentity_t* carrier);
	bool HandleCarrierHit(gentity_t* carrier, gentity_t* attacker,
		const MeansOfDeath& mod);
}

void SP_trigger_proball_goal(gentity_t* ent);
void SP_trigger_proball_oob(gentity_t* ent);
