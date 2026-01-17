#pragma once

struct FreezeTagDamageQuery {
	bool freezeTagActive = false;
	bool targetEliminated = false;
	bool targetThawing = false;
	bool attackerHasClient = false;
	bool modIsThaw = false;
};

/*
=============
FreezeTag_ShouldSuppressDamage

Determines whether incoming damage against a Freeze Tag target should be suppressed.
=============
*/
inline bool FreezeTag_ShouldSuppressDamage(const FreezeTagDamageQuery& query) {
	if (!query.freezeTagActive)
		return false;
	if (!query.targetEliminated)
		return false;
	if (query.targetThawing)
		return false;
	if (query.modIsThaw)
		return false;
	if (!query.attackerHasClient)
		return false;
	return true;
}

/*
=============
FreezeTag_ClampDamage

Returns the clamped damage value for a Freeze Tag interaction.
=============
*/
inline int FreezeTag_ClampDamage(const FreezeTagDamageQuery& query, int take) {
	if (take <= 0)
		return 0;
	return FreezeTag_ShouldSuppressDamage(query) ? 0 : take;
}
