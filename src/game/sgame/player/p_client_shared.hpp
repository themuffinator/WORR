#pragma once

#include "../g_local.hpp"

namespace worr::server::client {

	void P_AccumulateMatchPlayTime(gclient_t* cl, int64_t now);
	void InitClientResp(gclient_t* cl);
	void ClientCompleteSpawn(gentity_t* ent);
	void ClientBeginDeathmatch(gentity_t* ent);
	void G_SetLevelEntry();
	void PrintModifierIntro(gentity_t* ent);
	gentity_t* FreezeTag_FindFrozenTarget(gentity_t* thawer);
	bool FreezeTag_IsValidThawHelper(gentity_t* thawer, gentity_t* frozen);
	void FreezeTag_StartThawHold(gentity_t* thawer, gentity_t* frozen);
	void FreezeTag_ThawPlayer(gentity_t* thawer, gentity_t* frozen, bool awardScore, bool autoThaw);
	bool FreezeTag_UpdateThawHold(gentity_t* frozen);

} // namespace worr::server::client
