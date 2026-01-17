#pragma once

#include "../g_local.hpp"

enum class FlagStatus : int {
	Invalid = -1,
	AtBase = 0,
	Taken = 1,
	TakenRed = 2,
	TakenBlue = 3,
	Dropped = 4
};

void Team_CaptureFlagSound(Team team);
bool SetFlagStatus(Team team, FlagStatus status);
FlagStatus GetFlagStatus(Team team);
void AwardFlagCapture(gentity_t* flagEntity, gentity_t* scorer, Team scoringTeam, GameTime pickupTime);
bool CTF_ResetTeamFlag(Team team);
void CTF_ResetFlags();
bool CTF_PickupFlag(gentity_t* ent, gentity_t* other);
void CTF_DropFlag(gentity_t* ent, Item* item);
void CTF_ClientEffects(gentity_t* player);
void CTF_DeadDropFlag(gentity_t* self);
void CTF_ScoreBonuses(gentity_t* targ, gentity_t* inflictor, gentity_t* attacker);
void CTF_CheckHurtCarrier(gentity_t* targ, gentity_t* attacker);
void CTF_FlagSetup(gentity_t* ent);
