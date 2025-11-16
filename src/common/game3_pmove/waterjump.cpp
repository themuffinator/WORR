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

#include "shared/shared.hpp"
#include "common/game3_pmove/waterjump.hpp"

/*
=============
PM_CalcWaterJumpImpulse

Returns the upward impulse to apply while swimming based on
frame time and medium contents.
=============
*/
float PM_CalcWaterJumpImpulse(float frametime, int contents)
{
	float	impulse;

	if (frametime <= 0.0f)
	return 0.0f;

	if (contents == CONTENTS_WATER)
	impulse = 100.0f;
	else if (contents == CONTENTS_SLIME)
	impulse = 80.0f;
	else
	impulse = 50.0f;

	return impulse * frametime;
}
