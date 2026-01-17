#pragma once

#include "../g_local.hpp"

#include <functional>

bool Teamplay_IsEnabled();
bool Teamplay_IsTeamValid(Team team);
bool Teamplay_IsPrimaryTeam(Team team);
bool Teamplay_IsNeutralTeam(Team team);
bool Teamplay_ShouldForceBalance();
bool Teamplay_ShouldAutoBalance();
bool Teamplay_AllowsTeamPick();
bool Teamplay_ShouldAnnounceItemDrops();
void Teamplay_ForEachClient(const std::function<void(gentity_t*)>& fn);
void Teamplay_ForEachTeamMember(Team team, const std::function<void(gentity_t*)>& fn);
