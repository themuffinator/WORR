/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

Generic teamplay helpers shared across all team game modes.*/

#include "../g_local.hpp"
#include "g_teamplay.hpp"

/*
=============
Teamplay_IsEnabled

Returns true if the current match supports team-based logic.
=============
*/
bool Teamplay_IsEnabled() {
	return Teams();
}

/*
=============
Teamplay_IsTeamValid

Checks whether the supplied team enum maps to a gameplay team.
=============
*/
bool Teamplay_IsTeamValid(Team team) {
	switch (team) {
	case Team::Red:
	case Team::Blue:
	case Team::Free:
		return true;
	default:
		return false;
	}
}

/*
=============
Teamplay_IsPrimaryTeam

Returns true when the specified team is a primary (red or blue) team.
=============
*/
bool Teamplay_IsPrimaryTeam(Team team) {
	return team == Team::Red || team == Team::Blue;
}

/*
=============
Teamplay_IsNeutralTeam

Returns true when the specified team represents the neutral faction.
=============
*/
bool Teamplay_IsNeutralTeam(Team team) {
	return team == Team::Free;
}

/*
=============
Teamplay_ShouldForceBalance

Indicates if the server should force team balance immediately.
=============
*/
bool Teamplay_ShouldForceBalance() {
	return Teamplay_IsEnabled() && g_teamplay_force_balance && g_teamplay_force_balance->integer != 0;
}

/*
=============
Teamplay_ShouldAutoBalance

Indicates if the server should queue automatic balancing after deaths.
=============
*/
bool Teamplay_ShouldAutoBalance() {
	return Teamplay_IsEnabled() && g_teamplay_auto_balance && g_teamplay_auto_balance->integer != 0;
}

/*
=============
Teamplay_AllowsTeamPick

Returns true when players are allowed to choose their teams.
=============
*/
bool Teamplay_AllowsTeamPick() {
	return g_teamplay_allow_team_pick && g_teamplay_allow_team_pick->integer != 0;
}

/*
=============
Teamplay_ShouldAnnounceItemDrops

Returns true when the server should broadcast teammate item drops.
=============
*/
bool Teamplay_ShouldAnnounceItemDrops() {
	return Teamplay_IsEnabled() && g_teamplay_item_drop_notice && g_teamplay_item_drop_notice->integer != 0;
}

/*
=============
Teamplay_ForEachClient

Invokes the provided functor for every connected client entity.
=============
*/
void Teamplay_ForEachClient(const std::function<void(gentity_t*)>& fn) {
	if (!fn) {
		return;
	}

	for (auto entity : active_clients()) {
		if (!entity || !entity->client) {
			continue;
		}
		fn(entity);
	}
}

/*
=============
Teamplay_ForEachTeamMember

Executes the provided functor for each client belonging to the specified team.
=============
*/
void Teamplay_ForEachTeamMember(Team team, const std::function<void(gentity_t*)>& fn) {
	if (!Teamplay_IsTeamValid(team) || !fn) {
		return;
	}

	Teamplay_ForEachClient([&fn, team](gentity_t* entity) {
		if (entity->client->sess.team != team) {
			return;
		}

		fn(entity);
		});
}
