/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "server.hpp"

/*
=============
SV_ParseEffectCullMask

Parses the configured mask of effect bits eligible for distance culling.
=============
*/
static uint64_t SV_ParseEffectCullMask()
{
	if (!sv_effect_cull_mask || !sv_effect_cull_mask->string)
		return 0;

	return strtoull(sv_effect_cull_mask->string, NULL, 0);
}

/*
=============
SV_ShouldDistanceCullEffect

Determines whether an entity without a model is eligible for distance-based culling.
=============
*/
bool SV_ShouldDistanceCullEffect(const edict_t *ent)
{
	float distance_limit = sv_effect_cull_distance ? sv_effect_cull_distance->value : 0.0f;

	if (distance_limit <= 0.0f)
		return false;

	if (ent->s.modelindex || ent->s.sound || ent->s.event || (ent->s.renderfx & RF_CASTSHADOW))
		return false;

	uint64_t effect_mask = SV_ParseEffectCullMask();

	if (!(ent->s.renderfx & RF_LOW_PRIORITY) && !(ent->s.effects & effect_mask))
		return false;

	return true;
}
