#pragma once

enum class TeamJoinCapacityAction {
	Allow,
	QueueForDuel,
	Deny
};

/*
=============
EvaluateTeamJoinCapacity

Evaluates whether a player may join a team immediately, should queue for a duel, or must be denied based on player type, server capacity, and current match context.
=============
*/
inline TeamJoinCapacityAction EvaluateTeamJoinCapacity(
	bool joinPlaying,
	bool requestQueue,
	bool force,
	bool wasPlaying,
	bool duel,
	bool allowQueue,
	bool isHuman,
	int playingHumans,
	int maxPlayers) {
	if (!joinPlaying || requestQueue || force || wasPlaying || !isHuman)
		return TeamJoinCapacityAction::Allow;

	if (maxPlayers <= 0)
		return TeamJoinCapacityAction::Allow;

	if (playingHumans < maxPlayers)
		return TeamJoinCapacityAction::Allow;

	if (duel)
		return allowQueue ? TeamJoinCapacityAction::QueueForDuel
						  : TeamJoinCapacityAction::Deny;

	return TeamJoinCapacityAction::Deny;
}
