#include "../src/server/server.hpp"

#include <cstdio>

cvar_t *sv_effect_cull_distance;
cvar_t *sv_effect_cull_mask;

static char distance_storage[16];
static char mask_storage[32];
static cvar_t distance_cvar{};
static cvar_t mask_cvar{};

/*
=============
ShouldCullAtDistance

Checks whether an effect-only entity will be skipped based on distance.
=============
*/
static bool ShouldCullAtDistance(const edict_t *ent, const vec3_t vieworg, float limit_sq)
{
	sv_effect_cull_distance = &distance_cvar;
	sv_effect_cull_mask = &mask_cvar;

	if (!SV_ShouldDistanceCullEffect(ent))
		return false;

	return DistanceSquared(vieworg, ent->s.origin) > limit_sq;
}

/*
=============
SetDistanceCvar

Updates the distance cvar backing the effect cull helper.
=============
*/
static void SetDistanceCvar(float value)
{
	std::snprintf(distance_storage, sizeof(distance_storage), "%.0f", value);
	distance_cvar.string = distance_storage;
	distance_cvar.value = value;
	distance_cvar.integer = static_cast<int>(value);
}

/*
=============
SetMaskCvar

Updates the mask cvar backing the effect cull helper.
=============
*/
static void SetMaskCvar(uint64_t value)
{
	std::snprintf(mask_storage, sizeof(mask_storage), "%llu", static_cast<unsigned long long>(value));
	mask_cvar.string = mask_storage;
	mask_cvar.value = static_cast<float>(value);
	mask_cvar.integer = static_cast<int>(value);
}

/*
=============
main
=============
*/
int main()
{
	vec3_t vieworg{};
	edict_t ent{};
	float limit_sq;

	VectorSet(ent.s.origin, 800.0f, 0.0f, 0.0f);
	ent.s.effects = EF_BLASTER;

	SetDistanceCvar(512.0f);
	SetMaskCvar(0);
	limit_sq = distance_cvar.value * distance_cvar.value;

	if (ShouldCullAtDistance(&ent, vieworg, limit_sq))
		return 1;

	ent.s.renderfx = RF_LOW_PRIORITY;
	if (!ShouldCullAtDistance(&ent, vieworg, limit_sq))
		return 2;

	ent.s.renderfx = 0;
	SetMaskCvar(ent.s.effects);

	if (!ShouldCullAtDistance(&ent, vieworg, limit_sq))
		return 3;

	SetDistanceCvar(0.0f);
	limit_sq = 0.0f;

	if (ShouldCullAtDistance(&ent, vieworg, limit_sq))
		return 4;

	return 0;
}
