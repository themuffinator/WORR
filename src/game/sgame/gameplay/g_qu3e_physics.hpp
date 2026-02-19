// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

struct gentity_t;

void SG_QU3EPhysics_Init();
void SG_QU3EPhysics_Shutdown();
void SG_QU3EPhysics_RunFrame();
bool SG_QU3EPhysics_HandleBarrelTouch(gentity_t *barrel, gentity_t *other);
